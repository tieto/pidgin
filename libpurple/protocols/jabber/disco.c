/*
 * purple - Jabber Service Discovery
 *
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
 *
 */

#include "internal.h"
#include "network.h"
#include "prefs.h"
#include "debug.h"
#include "request.h"

#include "adhoccommands.h"
#include "buddy.h"
#include "disco.h"
#include "google/google.h"
#include "google/gmail.h"
#include "google/jingleinfo.h"
#include "iq.h"
#include "jabber.h"
#include "jingle/jingle.h"
#include "pep.h"
#include "presence.h"
#include "roster.h"
#include "useravatar.h"

struct _jabber_disco_info_cb_data {
	gpointer data;
	JabberDiscoInfoCallback *callback;
};

struct _jabber_disco_items_cb_data {
	gpointer data;
	JabberDiscoItemsCallback *callback;
};

#define SUPPORT_FEATURE(x) { \
	feature = purple_xmlnode_new_child(query, "feature"); \
	purple_xmlnode_set_attrib(feature, "var", x); \
}

static void
jabber_disco_bytestream_server_cb(JabberStream *js, const char *from,
                                  JabberIqType type, const char *id,
                                  PurpleXmlNode *packet, gpointer data)
{
	JabberBytestreamsStreamhost *sh = data;
	PurpleXmlNode *query = purple_xmlnode_get_child_with_namespace(packet, "query",
		NS_BYTESTREAMS);

	if (from && purple_strequal(from, sh->jid) && query != NULL) {
		PurpleXmlNode *sh_node = purple_xmlnode_get_child(query, "streamhost");
		if (sh_node) {
			const char *jid = purple_xmlnode_get_attrib(sh_node, "jid");
			const char *port = purple_xmlnode_get_attrib(sh_node, "port");


			if (jid == NULL || !purple_strequal(jid, from))
				purple_debug_error("jabber", "Invalid jid(%s) for bytestream.\n",
						   jid ? jid : "(null)");

			sh->host = g_strdup(purple_xmlnode_get_attrib(sh_node, "host"));
			sh->zeroconf = g_strdup(purple_xmlnode_get_attrib(sh_node, "zeroconf"));
			if (port != NULL)
				sh->port = atoi(port);
		}
	}

	purple_debug_info("jabber", "Discovered bytestream proxy server: "
			  "jid='%s' host='%s' port='%d' zeroconf='%s'\n",
			   from ? from : "", sh->host ? sh->host : "",
			   sh->port, sh->zeroconf ? sh->zeroconf : "");

	/* TODO: When we support zeroconf proxies, fix this to handle them */
	if (!(sh->jid && sh->host && sh->port > 0)) {
		js->bs_proxies = g_list_remove(js->bs_proxies, sh);
		g_free(sh->jid);
		g_free(sh->host);
		g_free(sh->zeroconf);
		g_free(sh);
	}
}


void jabber_disco_info_parse(JabberStream *js, const char *from,
                             JabberIqType type, const char *id,
                             PurpleXmlNode *in_query)
{
	if(type == JABBER_IQ_GET) {
		PurpleXmlNode *query, *identity, *feature;
		JabberIq *iq;
		const char *node = purple_xmlnode_get_attrib(in_query, "node");
		char *node_uri = NULL;

		/* create custom caps node URI */
		node_uri = g_strconcat(CAPS0115_NODE, "#", jabber_caps_get_own_hash(js), NULL);

		iq = jabber_iq_new_query(js, JABBER_IQ_RESULT, NS_DISCO_INFO);

		jabber_iq_set_id(iq, id);

		if (from)
			purple_xmlnode_set_attrib(iq->node, "to", from);
		query = purple_xmlnode_get_child(iq->node, "query");

		if(node)
			purple_xmlnode_set_attrib(query, "node", node);

		if(!node || purple_strequal(node, node_uri)) {
			GList *features, *identities;
			for(identities = jabber_identities; identities; identities = identities->next) {
				JabberIdentity *ident = (JabberIdentity*)identities->data;
				identity = purple_xmlnode_new_child(query, "identity");
				purple_xmlnode_set_attrib(identity, "category", ident->category);
				purple_xmlnode_set_attrib(identity, "type", ident->type);
				if (ident->lang)
					purple_xmlnode_set_attrib(identity, "xml:lang", ident->lang);
				if (ident->name)
					purple_xmlnode_set_attrib(identity, "name", ident->name);
			}
			for(features = jabber_features; features; features = features->next) {
				JabberFeature *feat = (JabberFeature*)features->data;
				if (!feat->is_enabled || feat->is_enabled(js, feat->namespace)) {
					feature = purple_xmlnode_new_child(query, "feature");
					purple_xmlnode_set_attrib(feature, "var", feat->namespace);
				}
			}
#ifdef USE_VV
		} else if (purple_strequal(node, CAPS0115_NODE "#" "voice-v1")) {
			/*
			 * HUGE HACK! We advertise this ext (see jabber_presence_create_js
			 * where we add <c/> to the <presence/>) for the Google Talk
			 * clients that don't actually check disco#info features.
			 *
			 * This specific feature is redundant but is what
			 * node='http://mail.google.com/xmpp/client/caps', ver='1.1'
			 * advertises as 'voice-v1'.
			 */
			PurpleXmlNode *feature = purple_xmlnode_new_child(query, "feature");
			purple_xmlnode_set_attrib(feature, "var", NS_GOOGLE_VOICE);
		} else if (purple_strequal(node, CAPS0115_NODE "#" "video-v1")) {
			/*
			 * HUGE HACK! We advertise this ext (see jabber_presence_create_js
			 * where we add <c/> to the <presence/>) for the Google Talk
			 * clients that don't actually check disco#info features.
			 *
			 * This specific feature is redundant but is what
			 * node='http://mail.google.com/xmpp/client/caps', ver='1.1'
			 * advertises as 'video-v1'.
			 */
			PurpleXmlNode *feature = purple_xmlnode_new_child(query, "feature");
			purple_xmlnode_set_attrib(feature, "var", NS_GOOGLE_VIDEO);
		} else if (purple_strequal(node, CAPS0115_NODE "#" "camera-v1")) {
			/*
			 * HUGE HACK! We advertise this ext (see jabber_presence_create_js
			 * where we add <c/> to the <presence/>) for the Google Talk
			 * clients that don't actually check disco#info features.
			 *
			 * This specific feature is redundant but is what
			 * node='http://mail.google.com/xmpp/client/caps', ver='1.1'
			 * advertises as 'camera-v1'.
			 */
			PurpleXmlNode *feature = purple_xmlnode_new_child(query, "feature");
			purple_xmlnode_set_attrib(feature, "var", NS_GOOGLE_CAMERA);
#endif
		} else {
			PurpleXmlNode *error, *inf;

			/* XXX: gross hack, implement jabber_iq_set_type or something */
			purple_xmlnode_set_attrib(iq->node, "type", "error");
			iq->type = JABBER_IQ_ERROR;

			error = purple_xmlnode_new_child(query, "error");
			purple_xmlnode_set_attrib(error, "code", "404");
			purple_xmlnode_set_attrib(error, "type", "cancel");
			inf = purple_xmlnode_new_child(error, "item-not-found");
			purple_xmlnode_set_namespace(inf, NS_XMPP_STANZAS);
		}
		g_free(node_uri);
		jabber_iq_send(iq);
	} else if (type == JABBER_IQ_SET) {
		/* wtf? seriously. wtf‽ */
		JabberIq *iq = jabber_iq_new(js, JABBER_IQ_ERROR);
		PurpleXmlNode *error, *bad_request;

		/* Free the <query/> */
		purple_xmlnode_free(purple_xmlnode_get_child(iq->node, "query"));
		/* Add an error */
		error = purple_xmlnode_new_child(iq->node, "error");
		purple_xmlnode_set_attrib(error, "type", "modify");
		bad_request = purple_xmlnode_new_child(error, "bad-request");
		purple_xmlnode_set_namespace(bad_request, NS_XMPP_STANZAS);

		jabber_iq_set_id(iq, id);
		if (from)
			purple_xmlnode_set_attrib(iq->node, "to", from);

		jabber_iq_send(iq);
	}
}

static void jabber_disco_info_cb(JabberStream *js, const char *from,
                                 JabberIqType type, const char *id,
                                 PurpleXmlNode *packet, gpointer data)
{
	struct _jabber_disco_info_cb_data *jdicd = data;
	PurpleXmlNode *query;

	query = purple_xmlnode_get_child_with_namespace(packet, "query", NS_DISCO_INFO);

	if (type == JABBER_IQ_RESULT && query) {
		PurpleXmlNode *child;
		JabberID *jid;
		JabberBuddy *jb;
		JabberBuddyResource *jbr = NULL;
		JabberCapabilities capabilities = JABBER_CAP_NONE;

		if((jid = jabber_id_new(from))) {
			if(jid->resource && (jb = jabber_buddy_find(js, from, TRUE)))
				jbr = jabber_buddy_find_resource(jb, jid->resource);
			jabber_id_free(jid);
		}

		if(jbr)
			capabilities = jbr->capabilities;

		for(child = query->child; child; child = child->next) {
			if(child->type != PURPLE_XMLNODE_TYPE_TAG)
				continue;

			if(purple_strequal(child->name, "identity")) {
				const char *category = purple_xmlnode_get_attrib(child, "category");
				const char *type = purple_xmlnode_get_attrib(child, "type");
				if(!category || !type)
					continue;

				if(purple_strequal(category, "conference") && purple_strequal(type, "text")) {
					/* we found a groupchat or MUC server, add it to the list */
					/* XXX: actually check for protocol/muc or gc-1.0 support */
					js->chat_servers = g_list_prepend(js->chat_servers, g_strdup(from));
				} else if(purple_strequal(category, "directory") && purple_strequal(type, "user")) {
					/* we found a JUD */
					js->user_directories = g_list_prepend(js->user_directories, g_strdup(from));
				} else if(purple_strequal(category, "proxy") && purple_strequal(type, "bytestreams")) {
					/* This is a bytestream proxy */
					JabberIq *iq;
					JabberBytestreamsStreamhost *sh;

					purple_debug_info("jabber", "Found bytestream proxy server: %s\n", from);

					sh = g_new0(JabberBytestreamsStreamhost, 1);
					sh->jid = g_strdup(from);
					js->bs_proxies = g_list_prepend(js->bs_proxies, sh);

					iq = jabber_iq_new_query(js, JABBER_IQ_GET,
							NS_BYTESTREAMS);
					purple_xmlnode_set_attrib(iq->node, "to", sh->jid);
					jabber_iq_set_callback(iq, jabber_disco_bytestream_server_cb, sh);
					jabber_iq_send(iq);
				}

			} else if(purple_strequal(child->name, "feature")) {
				const char *var = purple_xmlnode_get_attrib(child, "var");
				if(!var)
					continue;

				if(purple_strequal(var, "http://jabber.org/protocol/si"))
					capabilities |= JABBER_CAP_SI;
				else if(purple_strequal(var, "http://jabber.org/protocol/si/profile/file-transfer"))
					capabilities |= JABBER_CAP_SI_FILE_XFER;
				else if(purple_strequal(var, NS_BYTESTREAMS))
					capabilities |= JABBER_CAP_BYTESTREAMS;
				else if(purple_strequal(var, "jabber:iq:search"))
					capabilities |= JABBER_CAP_IQ_SEARCH;
				else if(purple_strequal(var, "jabber:iq:register"))
					capabilities |= JABBER_CAP_IQ_REGISTER;
				else if(purple_strequal(var, NS_PING))
					capabilities |= JABBER_CAP_PING;
				else if(purple_strequal(var, NS_DISCO_ITEMS))
					capabilities |= JABBER_CAP_ITEMS;
				else if(purple_strequal(var, "http://jabber.org/protocol/commands")) {
					capabilities |= JABBER_CAP_ADHOC;
				}
				else if(purple_strequal(var, NS_IBB)) {
					purple_debug_info("jabber", "remote supports IBB\n");
					capabilities |= JABBER_CAP_IBB;
				}
			}
		}

		js->chat_servers = g_list_reverse(js->chat_servers);

		capabilities |= JABBER_CAP_RETRIEVED;

		if(jbr)
			jbr->capabilities = capabilities;

		if (jdicd && jdicd->callback)
			jdicd->callback(js, from, capabilities, jdicd->data);
	} else { /* type == JABBER_IQ_ERROR or query == NULL */
		JabberID *jid;
		JabberBuddy *jb;
		JabberBuddyResource *jbr = NULL;
		JabberCapabilities capabilities = JABBER_CAP_NONE;

		if((jid = jabber_id_new(from))) {
			if(jid->resource && (jb = jabber_buddy_find(js, from, TRUE)))
				jbr = jabber_buddy_find_resource(jb, jid->resource);
			jabber_id_free(jid);
		}

		if(jbr)
			capabilities = jbr->capabilities;

		if (jdicd && jdicd->callback)
			jdicd->callback(js, from, capabilities, jdicd->data);
	}

	g_free(jdicd);
}

void jabber_disco_items_parse(JabberStream *js, const char *from,
                              JabberIqType type, const char *id,
                              PurpleXmlNode *query)
{
	if(type == JABBER_IQ_GET) {
		JabberIq *iq = jabber_iq_new_query(js, JABBER_IQ_RESULT,
				NS_DISCO_ITEMS);

		/* preserve node */
		PurpleXmlNode *iq_query = purple_xmlnode_get_child(iq->node, "query");
		const char *node = purple_xmlnode_get_attrib(query, "node");
		if(node)
			purple_xmlnode_set_attrib(iq_query,"node",node);

		jabber_iq_set_id(iq, id);

		if (from)
			purple_xmlnode_set_attrib(iq->node, "to", from);
		jabber_iq_send(iq);
	}
}

static void
jabber_disco_finish_server_info_result_cb(JabberStream *js)
{
	const char *ft_proxies;

	/*
	 * This *should* happen only if the server supports vcard-temp, but there
	 * are apparently some servers that don't advertise it even though they
	 * support it.
	 */
	jabber_vcard_fetch_mine(js);

	if (js->pep)
		jabber_avatar_fetch_mine(js);

	/* Yes, please! */
	jabber_roster_request(js);

	if (js->server_caps & JABBER_CAP_ADHOC) {
		/* The server supports ad-hoc commands, so let's request the list */
		jabber_adhoc_server_get_list(js);
	}

	/* If the server supports blocking, request the block list */
	if (js->server_caps & JABBER_CAP_BLOCKING) {
		jabber_request_block_list(js);
	}

	/* If there are manually specified bytestream proxies, query them */
	ft_proxies = purple_account_get_string(purple_connection_get_account(js->gc), "ft_proxies", NULL);
	if (ft_proxies) {
		JabberIq *iq;
		JabberBytestreamsStreamhost *sh;
		int i;
		char *tmp;
		gchar **ft_proxy_list = g_strsplit(ft_proxies, ",", 0);

		for(i = 0; ft_proxy_list[i]; i++) {
			g_strstrip(ft_proxy_list[i]);
			if(!(*ft_proxy_list[i]))
				continue;

			/* We used to allow specifying a port directly here; get rid of it */
			if((tmp = strchr(ft_proxy_list[i], ':')))
				*tmp = '\0';

			sh = g_new0(JabberBytestreamsStreamhost, 1);
			sh->jid = g_strdup(ft_proxy_list[i]);
			js->bs_proxies = g_list_prepend(js->bs_proxies, sh);

			iq = jabber_iq_new_query(js, JABBER_IQ_GET, NS_BYTESTREAMS);
			purple_xmlnode_set_attrib(iq->node, "to", sh->jid);
			jabber_iq_set_callback(iq, jabber_disco_bytestream_server_cb, sh);
			jabber_iq_send(iq);
		}

		g_strfreev(ft_proxy_list);
	}

}

static void
jabber_disco_stun_srv_resolve_cb(GObject *sender, GAsyncResult *result, gpointer data) {
	GError *error = NULL;
	GList *services = NULL;
	JabberStream *js = (JabberStream *) data;
	gint results = 0;

	services = g_resolver_lookup_service_finish(G_RESOLVER(sender),
			result, &error);

	if(error != NULL) {
		purple_debug_info("jabber", "Failed to look up a STUN record : %s\n", error->message);

		g_error_free(error);

		return;
	}

	results = g_list_length(services);

	purple_debug_info("jabber", "got %d SRV responses for STUN.\n", results);

	if (results > 0) {
		GSrvTarget *target = (GSrvTarget *)services->data;
		const gchar *hostname = g_srv_target_get_hostname(target);

		js->stun_ip = g_strdup(hostname);
		js->stun_port = g_srv_target_get_port(target);

		purple_debug_info("jabber", "set stun address to %s:%d\n",
			hostname, js->stun_port);
	}

	g_resolver_free_targets(services);
}


static void
jabber_disco_server_info_result_cb(JabberStream *js, const char *from,
                                   JabberIqType type, const char *id,
                                   PurpleXmlNode *packet, gpointer data)
{
	PurpleXmlNode *query, *child;

	if (!from || !purple_strequal(from, js->user->domain)) {
		jabber_disco_finish_server_info_result_cb(js);
		return;
	}

	if (type == JABBER_IQ_ERROR) {
		/* A common way to get here is for the server not to support xmlns http://jabber.org/protocol/disco#info */
		jabber_disco_finish_server_info_result_cb(js);
		return;
	}

	query = purple_xmlnode_get_child(packet, "query");

	if (!query) {
		jabber_disco_finish_server_info_result_cb(js);
		return;
	}

	for (child = purple_xmlnode_get_child(query, "identity"); child;
	     child = purple_xmlnode_get_next_twin(child)) {
		const char *category, *type, *name, *stun_ip;
		category = purple_xmlnode_get_attrib(child, "category");
		type = purple_xmlnode_get_attrib(child, "type");
		if(purple_strequal(category, "pubsub") && purple_strequal(type, "pep")) {
			PurpleConnection *gc = js->gc;
			js->pep = TRUE;
			purple_connection_set_flags(gc,
					  purple_connection_get_flags(gc)
					| PURPLE_CONNECTION_FLAG_SUPPORT_MOODS
					| PURPLE_CONNECTION_FLAG_SUPPORT_MOOD_MESSAGES);
		}
		if (!purple_strequal(category, "server"))
			continue;
		if (!purple_strequal(type, "im"))
			continue;

		name = purple_xmlnode_get_attrib(child, "name");
		if (!name)
			continue;

		g_free(js->server_name);
		js->server_name = g_strdup(name);
		stun_ip = purple_network_get_stun_ip();
		if (purple_strequal(name, "Google Talk")) {
			purple_debug_info("jabber", "Google Talk!\n");
			js->googletalk = TRUE;

			/* autodiscover stun and relays */
			if (!stun_ip || !*stun_ip) {
				jabber_google_send_jingle_info(js);
			}
		} else if (!stun_ip || !*stun_ip) {

			GResolver *resolver = g_resolver_get_default();
			g_resolver_lookup_service_async(resolver,
			                                "stun",
			                                "udp",
			                                js->user->domain,
			                                NULL,
			                                jabber_disco_stun_srv_resolve_cb,
			                                js);
			g_object_unref(resolver);
			/* TODO: add TURN support later... */
		}
	}

	for (child = purple_xmlnode_get_child(query, "feature"); child;
	     child = purple_xmlnode_get_next_twin(child)) {
		const char *var;
		var = purple_xmlnode_get_attrib(child, "var");
		if (!var)
			continue;

		if (purple_strequal(NS_GOOGLE_MAIL_NOTIFY, var)) {
			js->server_caps |= JABBER_CAP_GMAIL_NOTIFY;
			jabber_gmail_init(js);
		} else if (purple_strequal(NS_GOOGLE_ROSTER, var)) {
			js->server_caps |= JABBER_CAP_GOOGLE_ROSTER;
		} else if (purple_strequal("http://jabber.org/protocol/commands", var)) {
			js->server_caps |= JABBER_CAP_ADHOC;
		} else if (purple_strequal(NS_SIMPLE_BLOCKING, var)) {
			js->server_caps |= JABBER_CAP_BLOCKING;
		}
	}

	jabber_disco_finish_server_info_result_cb(js);
}

static void
jabber_disco_server_items_result_cb(JabberStream *js, const char *from,
                                    JabberIqType type, const char *id,
                                    PurpleXmlNode *packet, gpointer data)
{
	PurpleXmlNode *query, *child;

	if (!from || !purple_strequal(from, js->user->domain))
		return;

	if (type == JABBER_IQ_ERROR)
		return;

	while(js->chat_servers) {
		g_free(js->chat_servers->data);
		js->chat_servers = g_list_delete_link(js->chat_servers, js->chat_servers);
	}

	query = purple_xmlnode_get_child(packet, "query");

	for(child = purple_xmlnode_get_child(query, "item"); child;
			child = purple_xmlnode_get_next_twin(child)) {
		JabberIq *iq;
		const char *jid;

		if(!(jid = purple_xmlnode_get_attrib(child, "jid")))
			continue;

		/* we don't actually care about the specific nodes,
		 * so we won't query them */
		if(purple_xmlnode_get_attrib(child, "node") != NULL)
			continue;

		iq = jabber_iq_new_query(js, JABBER_IQ_GET, NS_DISCO_INFO);
		purple_xmlnode_set_attrib(iq->node, "to", jid);
		jabber_iq_set_callback(iq, jabber_disco_info_cb, NULL);
		jabber_iq_send(iq);
	}
}

void jabber_disco_items_server(JabberStream *js)
{
	JabberIq *iq = jabber_iq_new_query(js, JABBER_IQ_GET, NS_DISCO_ITEMS);

	purple_xmlnode_set_attrib(iq->node, "to", js->user->domain);

	jabber_iq_set_callback(iq, jabber_disco_server_items_result_cb, NULL);
	jabber_iq_send(iq);

	iq = jabber_iq_new_query(js, JABBER_IQ_GET, NS_DISCO_INFO);
	purple_xmlnode_set_attrib(iq->node, "to", js->user->domain);
	jabber_iq_set_callback(iq, jabber_disco_server_info_result_cb, NULL);
	jabber_iq_send(iq);
}

void jabber_disco_info_do(JabberStream *js, const char *who, JabberDiscoInfoCallback *callback, gpointer data)
{
	JabberID *jid;
	JabberBuddy *jb;
	JabberBuddyResource *jbr = NULL;
	struct _jabber_disco_info_cb_data *jdicd;
	JabberIq *iq;

	if((jid = jabber_id_new(who))) {
		if(jid->resource && (jb = jabber_buddy_find(js, who, TRUE)))
			jbr = jabber_buddy_find_resource(jb, jid->resource);
		jabber_id_free(jid);
	}

	if(jbr && jbr->capabilities & JABBER_CAP_RETRIEVED) {
		callback(js, who, jbr->capabilities, data);
		return;
	}

	jdicd = g_new0(struct _jabber_disco_info_cb_data, 1);
	jdicd->data = data;
	jdicd->callback = callback;

	iq = jabber_iq_new_query(js, JABBER_IQ_GET, NS_DISCO_INFO);
	purple_xmlnode_set_attrib(iq->node, "to", who);

	jabber_iq_set_callback(iq, jabber_disco_info_cb, jdicd);
	jabber_iq_send(iq);
}

