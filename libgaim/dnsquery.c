/**
 * @file dnsquery.c DNS query API
 * @ingroup core
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
 *
 */

#include "internal.h"
#include "debug.h"
#include "dnsquery.h"
#include "notify.h"
#include "prefs.h"
#include "util.h"

/**************************************************************************
 * DNS query API
 **************************************************************************/

typedef struct _GaimDnsQueryResolverProcess GaimDnsQueryResolverProcess;

struct _GaimDnsQueryData {
	char *hostname;
	int port;
	GaimDnsQueryConnectFunction callback;
	gpointer data;
	guint timeout;

	GThread *resolver;
	GSList *hosts;
	gchar *error_message;
};

static void
gaim_dnsquery_resolved(GaimDnsQueryData *query_data, GSList *hosts)
{
	gaim_debug_info("dnsquery", "IP resolved for %s\n", query_data->hostname);
	if (query_data->callback != NULL)
		query_data->callback(hosts, query_data->data, NULL);
	gaim_dnsquery_destroy(query_data);
}

static void
gaim_dnsquery_failed(GaimDnsQueryData *query_data, const gchar *error_message)
{
	gaim_debug_info("dnsquery", "%s\n", error_message);
	if (query_data->callback != NULL)
		query_data->callback(NULL, query_data->data, error_message);
	gaim_dnsquery_destroy(query_data);
}

static gboolean
dns_main_thread_cb(gpointer data)
{
	GaimDnsQueryData *query_data;

	query_data = data;

	if (query_data->error_message != NULL)
		gaim_dnsquery_failed(query_data, query_data->error_message);
	else
	{
		GSList *hosts;

		/* We don't want gaim_dns_query_resolved() to free hosts */
		hosts = query_data->hosts;
		query_data->hosts = NULL;
		gaim_dnsquery_resolved(query_data, hosts);
	}

	return FALSE;
}

#ifdef HAVE_GETADDRINFO
static gpointer
dns_thread(gpointer data)
{
	GaimDnsQueryData *query_data;
	int rc;
	struct addrinfo hints, *res, *tmp;
	char servname[20];

	query_data = data;

	g_snprintf(servname, sizeof(servname), "%d", query_data->port);
	memset(&hints,0,sizeof(hints));

	/*
	 * This is only used to convert a service
	 * name to a port number. As we know we are
	 * passing a number already, we know this
	 * value will not be really used by the C
	 * library.
	 */
	hints.ai_socktype = SOCK_STREAM;
	if ((rc = getaddrinfo(query_data->hostname, servname, &hints, &res)) == 0) {
		tmp = res;
		while(res) {
			query_data->hosts = g_slist_append(query_data->hosts,
				GSIZE_TO_POINTER(res->ai_addrlen));
			query_data->hosts = g_slist_append(query_data->hosts,
				g_memdup(res->ai_addr, res->ai_addrlen));
			res = res->ai_next;
		}
		freeaddrinfo(tmp);
	} else {
		query_data->error_message = g_strdup_printf(_("Error resolving %s: %s"), query_data->hostname, gai_strerror(rc));
	}

	/* We're done, tell the main thread to look at our results */
	g_idle_add(dns_main_thread_cb, query_data);

	return 0;
}
#endif

static gboolean
resolve_host(gpointer data)
{
	GaimDnsQueryData *query_data;
	struct sockaddr_in sin;
	GError *err = NULL;

	query_data = data;
	query_data->timeout = 0;

	if (inet_aton(query_data->hostname, &sin.sin_addr))
	{
		/*
		 * The given "hostname" is actually an IP address, so we
		 * don't need to do anything.
		 */
		GSList *hosts = NULL;
		sin.sin_family = AF_INET;
		sin.sin_port = htons(query_data->port);
		hosts = g_slist_append(hosts, GINT_TO_POINTER(sizeof(sin)));
		hosts = g_slist_append(hosts, g_memdup(&sin, sizeof(sin)));
		gaim_dnsquery_resolved(query_data, hosts);
	}
	else
	{
#ifdef HAVE_GETADDRINFO
		/*
		 * Spin off a separate thread to perform the DNS lookup so
		 * that we don't block the UI.
		 */
		query_data->resolver = g_thread_create(dns_thread,
				query_data, FALSE, &err);
		if (query_data->resolver == NULL)
		{
			char message[1024];
			g_snprintf(message, sizeof(message), _("Thread creation failure: %s"),
					err ? err->message : _("Unknown reason"));
			g_error_free(err);
			gaim_dnsquery_failed(query_data, message);
		}
#else
		struct sockaddr_in sin;
		struct hostent *hp;

		/*
		 * gethostbyname() is not threadsafe, but gethostbyname_r() is a GNU
		 * extension.  Unfortunately this means that we'll have to do a
		 * blocking DNS query for systems without GETADDRINFO.  Fortunately
		 * this should be a very small number of systems.
		 */

		if ((hp = gethostbyname(query_data->hostname))) {
			memset(&sin, 0, sizeof(struct sockaddr_in));
			memcpy(&sin.sin_addr.s_addr, hp->h_addr, hp->h_length);
			sin.sin_family = hp->h_addrtype;
			sin.sin_port = htons(query_data->port);

			query_data->hosts = g_slist_append(query_data->hosts,
					GSIZE_TO_POINTER(sizeof(sin)));
			query_data->hosts = g_slist_append(query_data->hosts,
					g_memdup(&sin, sizeof(sin)));
		} else {
			query_data->error_message = g_strdup_printf(_("Error resolving %s: %d"), query_data->hostname, h_errno);
		}

		/* We're done! */
		dns_main_thread(query_data);
#endif
	}

	return FALSE;
}

GaimDnsQueryData *
gaim_dnsquery_a(const char *hostname, int port,
				GaimDnsQueryConnectFunction callback, gpointer data)
{
	GaimDnsQueryData *query_data;

	g_return_val_if_fail(hostname != NULL, NULL);
	g_return_val_if_fail(port     != 0, NULL);

	gaim_debug_info("dnsquery", "Performing DNS lookup for %s\n", hostname);

	query_data = g_new(GaimDnsQueryData, 1);
	query_data->hostname = g_strdup(hostname);
	g_strstrip(query_data->hostname);
	query_data->port = port;
	query_data->callback = callback;
	query_data->data = data;
	query_data->error_message = NULL;
	query_data->hosts = NULL;

	/* Don't call the callback before returning */
	query_data->timeout = gaim_timeout_add(0, resolve_host, query_data);

	return query_data;
}

void
gaim_dnsquery_destroy(GaimDnsQueryData *query_data)
{
	if (query_data->resolver != NULL)
	{
		/*
		 * It's not really possible to kill a thread.  So instead we
		 * just set the callback to NULL and let the DNS lookup
		 * finish.
		 */
		query_data->callback = NULL;
		return;
	}

	while (query_data->hosts != NULL)
	{
		/* Discard the length... */
		query_data->hosts = g_slist_remove(query_data->hosts, query_data->hosts->data);
		/* Free the address... */
		g_free(query_data->hosts->data);
		query_data->hosts = g_slist_remove(query_data->hosts, query_data->hosts->data);
	}
	g_free(query_data->error_message);

	if (query_data->timeout > 0)
		gaim_timeout_remove(query_data->timeout);

	g_free(query_data->hostname);
	g_free(query_data);
}

void
gaim_dnsquery_init(void)
{
	if (!g_thread_supported())
		g_thread_init(NULL);
}

void
gaim_dnsquery_uninit(void)
{
}
