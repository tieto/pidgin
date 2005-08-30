/**
 * @file srvresolve.c
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
#include <glib.h>
#include <resolv.h>
#include <stdlib.h>
#include <arpa/nameser_compat.h>
#ifndef T_SRV
#define T_SRV	33
#endif

#include "dnssrv.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "eventloop.h"
#include "debug.h"

typedef union {
	HEADER hdr;
	u_char buf[1024];
} queryans;

struct resdata {
	SRVCallback cb;
	guint handle;
};

static gint responsecompare(gconstpointer ar, gconstpointer br) {
	struct srv_response *a = (struct srv_response*)ar;
	struct srv_response *b = (struct srv_response*)br;

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

static void resolve(int in, int out) {
	GList *ret = NULL;
	struct srv_response *srvres;
	queryans answer;
	int size;
	int qdcount;
	int ancount;
	gchar *end;
	gchar *cp;
	gchar name[256];
	int type, dlen, pref, weight, port;
	gchar query[256];

	if(read(in, query, 256) <= 0) {
		_exit(0);
	}
	size = res_query( query, C_IN, T_SRV, (u_char*)&answer, sizeof( answer));

	qdcount = ntohs(answer.hdr.qdcount);
	ancount = ntohs(answer.hdr.ancount);

	
	cp = (char*)&answer + sizeof(HEADER);
	end = (char*)&answer + size;

	/* skip over unwanted stuff */
	while (qdcount-- > 0 && cp < end) {
		size = dn_expand( (char*)&answer, end, cp, name, 256);
		if(size < 0) goto end;
		cp += size + QFIXEDSZ;
	}

	while (ancount-- > 0 && cp < end) {
		size = dn_expand((char*)&answer, end, cp, name, 256);
		if(size < 0)
			goto end;

		cp += size;
	
		NS_GET16(type,cp);
		cp += 6; /* skip ttl and class since we already know it */

		NS_GET16(dlen,cp);

		if (type == T_SRV) {
			NS_GET16(pref,cp);

			NS_GET16(weight, cp);

			NS_GET16(port, cp);

			size = dn_expand( (char*)&answer, end, cp, name, 256);
			if(size < 0 )
				goto end;

			cp += size;

			srvres = g_new0(struct srv_response,1);
			strcpy(srvres->hostname, name);
			srvres->pref = pref;
			srvres->port = port;
			srvres->weight = weight;
			
			ret = g_list_insert_sorted(ret, srvres, responsecompare);
		} else {
			cp += dlen;
		}
	}
end:	size = g_list_length(ret);
	write(out, &size, 4);
	while(g_list_first(ret)) {
		write(out, g_list_first(ret)->data, sizeof(struct srv_response));
		g_free(g_list_first(ret)->data);
		ret = g_list_remove(ret, g_list_first(ret)->data);
	}

	/* Should the resolver be reused?
	 * There is most likely only 1 SRV queries per prpl...
	 */
	_exit(0);	
}

static void resolved(gpointer data, gint source, GaimInputCondition cond) {
	int size;
	struct resdata *rdata = (struct resdata*)data;
	struct srv_response *res;
	struct srv_response *tmp;
	SRVCallback cb = rdata->cb;

	read(source, &size, 4);
	gaim_debug_info("srv","found %d SRV entries\n", size);
	tmp = res = g_malloc0(sizeof(struct srv_response)*size);
	while(size) {
		read(source, tmp++, sizeof(struct srv_response));
		size--;
	}
	cb(res, size);
	gaim_input_remove(rdata->handle);
	g_free(rdata);
}

void gaim_srv_resolve(char *protocol, char *transport, char *domain, SRVCallback cb) {
	char *query = g_strdup_printf("_%s._%s.%s",protocol, transport, domain);
	int in[2], out[2];
	int pid;
	struct resdata *rdata;
	gaim_debug_info("dnssrv","querying SRV record for %s\n",query);
	if(pipe(in) || pipe(out)) {
		gaim_debug_error("srv", "Could not create pipe\n");
		g_free(query);
		cb(NULL, 0);
		return;
	}

	pid = fork();

	if(pid == -1) {
		gaim_debug_error("srv","Could not create process!\n");
		cb(NULL, 0);
		g_free(query);
		return;
	}
	/* Child */
	if( pid == 0 ) {
		close(out[0]);
		close(in[1]);
		resolve(in[0], out[1]);
	}

	close(out[1]);
	close(in[0]);
	 
	if(write(in[1], query, strlen(query)+1)<0) {
		gaim_debug_error("srv", "Could not write to SRV resolver\n");
	}
	rdata = g_new0(struct resdata,1);
	rdata->cb = cb;
	rdata->handle = gaim_input_add(out[0], GAIM_INPUT_READ, resolved, rdata);
	g_free(query);
}
