/**
 * Purple is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include "internal.h"
#include "debug.h"
#include "jingleinfo.h"

#include <gio/gio.h>

static void
jabber_google_stun_lookup_cb(GObject *sender, GAsyncResult *result, gpointer data) {
	GError *error = NULL;
	GList *addresses = NULL;
	JabberStream *js = (JabberStream *) data;

	addresses = g_resolver_lookup_by_name_finish(G_RESOLVER(sender),
			result, &error);

	if(error) {
		purple_debug_error("jabber", "Google STUN lookup failed: %s\n",
			error->message);

		g_error_free(error);

		return;
	}

	if(g_list_length(addresses) > 0) {
		GInetAddress *inet_address = G_INET_ADDRESS(addresses->data);

		g_free(js->stun_ip);
		js->stun_ip = g_inet_address_to_string(inet_address);

		purple_debug_info("jabber", "set Google STUN IP/port address: "
		                  "%s:%d\n", js->stun_ip, js->stun_port);
	}

	g_resolver_free_addresses(addresses);
}

static void
jabber_google_jingle_info_common(JabberStream *js, const char *from,
                                 JabberIqType type, PurpleXmlNode *query)
{
	const PurpleXmlNode *stun = purple_xmlnode_get_child(query, "stun");
	const PurpleXmlNode *relay = purple_xmlnode_get_child(query, "relay");
	gchar *my_bare_jid;

	/*
	 * Make sure that random people aren't sending us STUN servers. Per
	 * https://developers.google.com/talk/jep_extensions/jingleinfo, these
	 * stanzas are stamped from our bare JID.
	 */
	if (from) {
		my_bare_jid = g_strdup_printf("%s@%s", js->user->node, js->user->domain);
		if (!purple_strequal(from, my_bare_jid)) {
			purple_debug_warning("jabber", "got google:jingleinfo with invalid from (%s)\n",
			                  from);
			g_free(my_bare_jid);
			return;
		}

		g_free(my_bare_jid);
	}

	if (type == JABBER_IQ_ERROR || type == JABBER_IQ_GET)
		return;

	purple_debug_info("jabber", "got google:jingleinfo\n");

	if (stun) {
		PurpleXmlNode *server = purple_xmlnode_get_child(stun, "server");

		if (server) {
			const gchar *host = purple_xmlnode_get_attrib(server, "host");
			const gchar *udp = purple_xmlnode_get_attrib(server, "udp");

			if (host && udp) {
				js->stun_port = atoi(udp);

				g_resolver_lookup_by_name_async(g_resolver_get_default(),
				                                host,
				                                NULL,
				                                jabber_google_stun_lookup_cb,
				                                js);
			}
		}
	}

	if (relay) {
		PurpleXmlNode *token = purple_xmlnode_get_child(relay, "token");
		PurpleXmlNode *server = purple_xmlnode_get_child(relay, "server");

		if (token) {
			gchar *relay_token = purple_xmlnode_get_data(token);

			/* we let js own the string returned from purple_xmlnode_get_data */
			js->google_relay_token = relay_token;
		}

		if (server) {
			js->google_relay_host =
				g_strdup(purple_xmlnode_get_attrib(server, "host"));
		}
	}
}

static void
jabber_google_jingle_info_cb(JabberStream *js, const char *from,
                             JabberIqType type, const char *id,
                             PurpleXmlNode *packet, gpointer data)
{
	PurpleXmlNode *query = purple_xmlnode_get_child_with_namespace(packet, "query",
			NS_GOOGLE_JINGLE_INFO);

	if (query)
		jabber_google_jingle_info_common(js, from, type, query);
	else
		purple_debug_warning("jabber", "Got invalid google:jingleinfo\n");
}

void
jabber_google_handle_jingle_info(JabberStream *js, const char *from,
                                 JabberIqType type, const char *id,
                                 PurpleXmlNode *child)
{
	jabber_google_jingle_info_common(js, from, type, child);
}

void
jabber_google_send_jingle_info(JabberStream *js)
{
	JabberIq *jingle_info =
		jabber_iq_new_query(js, JABBER_IQ_GET, NS_GOOGLE_JINGLE_INFO);

	jabber_iq_set_callback(jingle_info, jabber_google_jingle_info_cb,
		NULL);
	purple_debug_info("jabber", "sending google:jingleinfo query\n");
	jabber_iq_send(jingle_info);
}
