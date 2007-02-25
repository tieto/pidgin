/**
 * @file dnssrv.c
 *
 * gaim
 *
 * Copyright (C) 2005 Thomas Butter <butter@uni-mannheim.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "internal.h"
#include "util.h"

#ifndef _WIN32
#include <resolv.h>
#include <arpa/nameser.h>
#ifdef HAVE_ARPA_NAMESER_COMPAT_H
#include <arpa/nameser_compat.h>
#endif
#ifndef T_SRV
#define T_SRV	33
#endif
#else
#include <windns.h>
/* Missing from the mingw headers */
#ifndef DNS_TYPE_SRV
# define DNS_TYPE_SRV 33
#endif
#endif

#include "dnssrv.h"
#include "eventloop.h"
#include "debug.h"

#ifndef _WIN32
typedef union {
	HEADER hdr;
	u_char buf[1024];
} queryans;
#else
static DNS_STATUS WINAPI (*MyDnsQuery_UTF8) (
	PCSTR lpstrName, WORD wType, DWORD fOptions,
	PIP4_ARRAY aipServers, PDNS_RECORD* ppQueryResultsSet,
	PVOID* pReserved) = NULL;
static void WINAPI (*MyDnsRecordListFree) (PDNS_RECORD pRecordList,
	DNS_FREE_TYPE FreeType) = NULL;
#endif

struct _GaimSrvQueryData {
	GaimSrvCallback cb;
	gpointer extradata;
	guint handle;
#ifdef _WIN32
	GThread *resolver;
	char *query;
	char *error_message;
	GSList *results;
#else
	pid_t pid;
#endif
};

static gint
responsecompare(gconstpointer ar, gconstpointer br)
{
	GaimSrvResponse *a = (GaimSrvResponse*)ar;
	GaimSrvResponse *b = (GaimSrvResponse*)br;

	if(a->pref == b->pref) {
		if(a->weight == b->weight)
			return 0;
		if(a->weight < b->weight)
			return -1;
		return 1;
	}
	if(a->pref < b->pref)
		return -1;
	return 1;
}

#ifndef _WIN32

static void
resolve(int in, int out)
{
	GList *ret = NULL;
	GaimSrvResponse *srvres;
	queryans answer;
	int size;
	int qdcount;
	int ancount;
	guchar *end;
	guchar *cp;
	gchar name[256];
	guint16 type, dlen, pref, weight, port;
	gchar query[256];

#ifdef HAVE_SIGNAL_H
	gaim_restore_default_signal_handlers();
#endif
	
	if (read(in, query, 256) <= 0)
		_exit(0);

	size = res_query( query, C_IN, T_SRV, (u_char*)&answer, sizeof( answer));

	qdcount = ntohs(answer.hdr.qdcount);
	ancount = ntohs(answer.hdr.ancount);

	cp = (guchar*)&answer + sizeof(HEADER);
	end = (guchar*)&answer + size;

	/* skip over unwanted stuff */
	while (qdcount-- > 0 && cp < end) {
		size = dn_expand( (unsigned char*)&answer, end, cp, name, 256);
		if(size < 0) goto end;
		cp += size + QFIXEDSZ;
	}

	while (ancount-- > 0 && cp < end) {
		size = dn_expand((unsigned char*)&answer, end, cp, name, 256);
		if(size < 0)
			goto end;

		cp += size;

		GETSHORT(type,cp);

		/* skip ttl and class since we already know it */
		cp += 6;

		GETSHORT(dlen,cp);

		if (type == T_SRV) {
			GETSHORT(pref,cp);

			GETSHORT(weight,cp);

			GETSHORT(port,cp);

			size = dn_expand( (unsigned char*)&answer, end, cp, name, 256);
			if(size < 0 )
				goto end;

			cp += size;

			srvres = g_new0(GaimSrvResponse, 1);
			strcpy(srvres->hostname, name);
			srvres->pref = pref;
			srvres->port = port;
			srvres->weight = weight;

			ret = g_list_insert_sorted(ret, srvres, responsecompare);
		} else {
			cp += dlen;
		}
	}

end:
	size = g_list_length(ret);
	write(out, &size, sizeof(int));
	while (ret != NULL)
	{
		write(out, ret->data, sizeof(GaimSrvResponse));
		g_free(ret->data);
		ret = g_list_remove(ret, ret->data);
	}

	_exit(0);
}

static void
resolved(gpointer data, gint source, GaimInputCondition cond)
{
	int size;
	GaimSrvQueryData *query_data = (GaimSrvQueryData*)data;
	GaimSrvResponse *res;
	GaimSrvResponse *tmp;
	int i;
	GaimSrvCallback cb = query_data->cb;
	int status;

	read(source, &size, sizeof(int));
	gaim_debug_info("dnssrv","found %d SRV entries\n", size);
	tmp = res = g_new0(GaimSrvResponse, size);
	for (i = 0; i < size; i++) {
		read(source, tmp++, sizeof(GaimSrvResponse));
	}

	cb(res, size, query_data->extradata);
	waitpid(query_data->pid, &status, 0);

	gaim_srv_cancel(query_data);
}

#else /* _WIN32 */

/** The Jabber Server code was inspiration for parts of this. */

static gboolean
res_main_thread_cb(gpointer data)
{
	GaimSrvResponse *srvres = NULL;
	int size = 0;
	GaimSrvQueryData *query_data = data;

	if(query_data->error_message != NULL)
		gaim_debug_error("dnssrv", query_data->error_message);
	else {
		GaimSrvResponse *srvres_tmp = NULL;
		GSList *lst = query_data->results;

		size = g_slist_length(query_data->results);

		if(query_data->cb)
			srvres_tmp = srvres = g_new0(GaimSrvResponse, size);
		while (lst) {
			if(query_data->cb)
				memcpy(srvres_tmp++, lst->data, sizeof(GaimSrvResponse));
			g_free(lst->data);
			lst = g_slist_remove(lst, lst->data);
		}

		query_data->results = NULL;
	}

	gaim_debug_info("dnssrv", "found %d SRV entries\n", size);

	if(query_data->cb)
		query_data->cb(srvres, size, query_data->extradata);

	query_data->resolver = NULL;
	query_data->handle = 0;

	gaim_srv_cancel(query_data);

	return FALSE;
}

static gpointer
res_thread(gpointer data)
{
	PDNS_RECORD dr = NULL;
	int type = DNS_TYPE_SRV;
	DNS_STATUS ds;
	GaimSrvQueryData *query_data = data;

	ds = MyDnsQuery_UTF8(query_data->query, type, DNS_QUERY_STANDARD, NULL, &dr, NULL);
	if (ds != ERROR_SUCCESS) {
		gchar *msg = g_win32_error_message(ds);
		query_data->error_message = g_strdup_printf("Couldn't look up SRV record. %s (%lu).\n", msg, ds);
		g_free(msg);
	} else {
		PDNS_RECORD dr_tmp;
		GSList *lst = NULL;
		DNS_SRV_DATA *srv_data;
		GaimSrvResponse *srvres;

		for (dr_tmp = dr; dr_tmp != NULL; dr_tmp = dr_tmp->pNext) {
			/* Discard any incorrect entries. I'm not sure if this is necessary */
			if (dr_tmp->wType != type || strcmp(dr_tmp->pName, query_data->query) != 0) {
				continue;
			}

			srv_data = &dr_tmp->Data.SRV;
			srvres = g_new0(GaimSrvResponse, 1);
			strncpy(srvres->hostname, srv_data->pNameTarget, 255);
			srvres->hostname[255] = '\0';
			srvres->pref = srv_data->wPriority;
			srvres->port = srv_data->wPort;
			srvres->weight = srv_data->wWeight;

			lst = g_slist_insert_sorted(lst, srvres, responsecompare);
		}

		MyDnsRecordListFree(dr, DnsFreeRecordList);
		query_data->results = lst;
	}

	/* back to main thread */
	/* Note: this should *not* be attached to query_data->handle - it will cause leakage */
	g_idle_add(res_main_thread_cb, query_data);

	g_thread_exit(NULL);
	return NULL;
}

#endif

GaimSrvQueryData *
gaim_srv_resolve(const char *protocol, const char *transport, const char *domain, GaimSrvCallback cb, gpointer extradata)
{
	char *query;
	GaimSrvQueryData *query_data;
#ifndef _WIN32
	int in[2], out[2];
	int pid;
#else
	GError* err = NULL;
	static gboolean initialized = FALSE;
#endif

	query = g_strdup_printf("_%s._%s.%s", protocol, transport, domain);
	gaim_debug_info("dnssrv","querying SRV record for %s\n", query);

#ifndef _WIN32
	if(pipe(in) || pipe(out)) {
		gaim_debug_error("dnssrv", "Could not create pipe\n");
		g_free(query);
		cb(NULL, 0, extradata);
		return NULL;
	}

	pid = fork();
	if (pid == -1) {
		gaim_debug_error("dnssrv", "Could not create process!\n");
		cb(NULL, 0, extradata);
		g_free(query);
		return NULL;
	}

	/* Child */
	if (pid == 0)
	{
		close(out[0]);
		close(in[1]);
		resolve(in[0], out[1]);
	}

	close(out[1]);
	close(in[0]);

	if (write(in[1], query, strlen(query)+1) < 0)
		gaim_debug_error("dnssrv", "Could not write to SRV resolver\n");

	query_data = g_new0(GaimSrvQueryData, 1);
	query_data->cb = cb;
	query_data->extradata = extradata;
	query_data->pid = pid;
	query_data->handle = gaim_input_add(out[0], GAIM_INPUT_READ, resolved, query_data);

	g_free(query);

	return query_data;
#else
	if (!initialized) {
		MyDnsQuery_UTF8 = (void*) wgaim_find_and_loadproc("dnsapi.dll", "DnsQuery_UTF8");
		MyDnsRecordListFree = (void*) wgaim_find_and_loadproc(
			"dnsapi.dll", "DnsRecordListFree");
		initialized = TRUE;
	}

	query_data = g_new0(GaimSrvQueryData, 1);
	query_data->cb = cb;
	query_data->query = query;
	query_data->extradata = extradata;

	if (!MyDnsQuery_UTF8 || !MyDnsRecordListFree) {
		query_data->error_message = g_strdup_printf("System missing DNS API (Requires W2K+)\n");

		/* Asynchronously call the callback since stuff may not expect
		 * the callback to be called before this returns */
		query_data->handle = g_idle_add(res_main_thread_cb, query_data);

		return query_data;
	}

	query_data->resolver = g_thread_create(res_thread, query_data, FALSE, &err);
	if (query_data->resolver == NULL)
	{
		query_data->error_message = g_strdup_printf("SRV thread create failure: %s\n", err ? err->message : "");
		g_error_free(err);

		/* Asynchronously call the callback since stuff may not expect
		 * the callback to be called before this returns */
		query_data->handle = g_idle_add(res_main_thread_cb, query_data);

		return query_data;
	}

	return query_data;
#endif
}

void
gaim_srv_cancel(GaimSrvQueryData *query_data)
{
	if (query_data->handle > 0)
		gaim_input_remove(query_data->handle);
#ifdef _WIN32
	if (query_data->resolver != NULL)
	{
		/*
		 * It's not really possible to kill a thread.  So instead we
		 * just set the callback to NULL and let the DNS lookup
		 * finish.
		 */
		query_data->cb = NULL;
		return;
	}
	g_free(query_data->query);
	g_free(query_data->error_message);
#endif
	g_free(query_data);
}
