/*
 * purple - Jabber Service Discovery
 *
 * Copyright (C) 2003, Nathan Walp <faceprint@faceprint.com>
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
#include "prefs.h"
#include "debug.h"
#include "request.h"
#include "notify.h"
#include "libpurple/disco.h"

#include "buddy.h"
#include "google.h"
#include "iq.h"
#include "disco.h"
#include "jabber.h"
#include "jingle/jingle.h"
#include "presence.h"
#include "roster.h"
#include "pep.h"
#include "adhoccommands.h"
#include "xdata.h"

struct _jabber_disco_info_cb_data {
	gpointer data;
	JabberDiscoInfoCallback *callback;
};

struct _jabber_disco_items_cb_data {
	gpointer data;
	JabberDiscoItemsCallback *callback;
};

#define SUPPORT_FEATURE(x) { \
	feature = xmlnode_new_child(query, "feature"); \
	xmlnode_set_attrib(feature, "var", x); \
}

struct jabber_disco_list_data {
	JabberStream *js;
	PurpleDiscoList *list;
	char *server;
	int fetch_count;
};

static void
jabber_disco_bytestream_server_cb(JabberStream *js, const char *from,
                                  JabberIqType type, const char *id,
                                  xmlnode *packet, gpointer data)
{
	JabberBytestreamsStreamhost *sh = data;
	xmlnode *query = xmlnode_get_child_with_namespace(packet, "query",
		"http://jabber.org/protocol/bytestreams");

	if (from && !strcmp(from, sh->jid) && query != NULL) {
		xmlnode *sh_node = xmlnode_get_child(query, "streamhost");
		if (sh_node) {
			const char *jid = xmlnode_get_attrib(sh_node, "jid");
			const char *port = xmlnode_get_attrib(sh_node, "port");


			if (jid == NULL || strcmp(jid, from) != 0)
				purple_debug_error("jabber", "Invalid jid(%s) for bytestream.\n",
						   jid ? jid : "(null)");

			sh->host = g_strdup(xmlnode_get_attrib(sh_node, "host"));
			sh->zeroconf = g_strdup(xmlnode_get_attrib(sh_node, "zeroconf"));
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
		g_free(sh->jid);
		g_free(sh->host);
		g_free(sh->zeroconf);
		g_free(sh);
		js->bs_proxies = g_list_remove(js->bs_proxies, sh);
	}
}


void jabber_disco_info_parse(JabberStream *js, const char *from,
                             JabberIqType type, const char *id,
                             xmlnode *in_query) {

	if(!from)
		return;

	if(type == JABBER_IQ_GET) {
		xmlnode *query, *identity, *feature;
		JabberIq *iq;
		const char *node = xmlnode_get_attrib(in_query, "node");

		iq = jabber_iq_new_query(js, JABBER_IQ_RESULT,
				"http://jabber.org/protocol/disco#info");

		jabber_iq_set_id(iq, id);

		xmlnode_set_attrib(iq->node, "to", from);
		query = xmlnode_get_child(iq->node, "query");

		if(node)
			xmlnode_set_attrib(query, "node", node);

		if(!node || !strcmp(node, CAPS0115_NODE "#" VERSION)) {
			identity = xmlnode_new_child(query, "identity");
			xmlnode_set_attrib(identity, "category", "client");
			xmlnode_set_attrib(identity, "type", "pc"); /* XXX: bot, console,
														 * handheld, pc, phone,
														 * web */
			xmlnode_set_attrib(identity, "name", PACKAGE);

			SUPPORT_FEATURE("jabber:iq:last")
			SUPPORT_FEATURE("jabber:iq:oob")
			SUPPORT_FEATURE("jabber:iq:time")
			SUPPORT_FEATURE("urn:xmpp:time")
			SUPPORT_FEATURE("jabber:iq:version")
			SUPPORT_FEATURE("jabber:x:conference")
			SUPPORT_FEATURE("http://jabber.org/protocol/bytestreams")
			SUPPORT_FEATURE("http://jabber.org/protocol/disco#info")
			SUPPORT_FEATURE("http://jabber.org/protocol/disco#items")
			SUPPORT_FEATURE("http://jabber.org/protocol/ibb");
			SUPPORT_FEATURE("http://jabber.org/protocol/muc")
			SUPPORT_FEATURE("http://jabber.org/protocol/muc#user")
			SUPPORT_FEATURE("http://jabber.org/protocol/si")
			SUPPORT_FEATURE("http://jabber.org/protocol/si/profile/file-transfer")
			SUPPORT_FEATURE("http://jabber.org/protocol/xhtml-im")
			SUPPORT_FEATURE("urn:xmpp:ping")

			if(!node) { /* non-caps disco#info, add all enabled extensions */
				GList *features;
				for(features = jabber_features; features; features = features->next) {
					JabberFeature *feat = (JabberFeature*)features->data;
					if(feat->is_enabled == NULL || feat->is_enabled(js, feat->shortname, feat->namespace) == TRUE)
						SUPPORT_FEATURE(feat->namespace);
				}
			}
#ifdef USE_VV
		} else if (node && !strcmp(node, CAPS0115_NODE "#voice-v1")) {
			SUPPORT_FEATURE("http://www.google.com/xmpp/protocol/session");
			SUPPORT_FEATURE("http://www.google.com/xmpp/protocol/voice/v1");
			SUPPORT_FEATURE(JINGLE);
			SUPPORT_FEATURE(JINGLE_APP_RTP_SUPPORT_AUDIO);
			SUPPORT_FEATURE(JINGLE_APP_RTP_SUPPORT_VIDEO);
			SUPPORT_FEATURE(JINGLE_TRANSPORT_RAWUDP);
			SUPPORT_FEATURE(JINGLE_TRANSPORT_ICEUDP);
#endif
		} else {
			const char *ext = NULL;
			unsigned pos;
			unsigned nodelen = strlen(node);
			unsigned capslen = strlen(CAPS0115_NODE);
			/* do a basic plausability check */
			if(nodelen > capslen+1) {
				/* verify that the string is CAPS0115#<ext> and get the pointer to the ext part */
				for(pos = 0; pos < capslen+1; ++pos) {
					if(pos == capslen) {
						if(node[pos] == '#')
							ext = &node[pos+1];
						else
							break;
					} else if(node[pos] != CAPS0115_NODE[pos])
					break;
				}

				if(ext != NULL) {
					/* look for that ext */
					GList *features;
					for(features = jabber_features; features; features = features->next) {
						JabberFeature *feat = (JabberFeature*)features->data;
						if(!strcmp(feat->shortname, ext)) {
							SUPPORT_FEATURE(feat->namespace);
							break;
						}
					}
					if(features == NULL)
						ext = NULL;
				}
			}

			if(ext == NULL) {
				xmlnode *error, *inf;

				/* XXX: gross hack, implement jabber_iq_set_type or something */
				xmlnode_set_attrib(iq->node, "type", "error");
				iq->type = JABBER_IQ_ERROR;

				error = xmlnode_new_child(query, "error");
				xmlnode_set_attrib(error, "code", "404");
				xmlnode_set_attrib(error, "type", "cancel");
				inf = xmlnode_new_child(error, "item-not-found");
				xmlnode_set_namespace(inf, "urn:ietf:params:xml:ns:xmpp-stanzas");
			}
		}

		jabber_iq_send(iq);
	} else if(type == JABBER_IQ_RESULT) {
		xmlnode *child;
		JabberID *jid;
		JabberBuddy *jb;
		JabberBuddyResource *jbr = NULL;
		JabberCapabilities capabilities = JABBER_CAP_NONE;
		struct _jabber_disco_info_cb_data *jdicd;

		if((jid = jabber_id_new(from))) {
			if(jid->resource && (jb = jabber_buddy_find(js, from, TRUE)))
				jbr = jabber_buddy_find_resource(jb, jid->resource);
			jabber_id_free(jid);
		}

		if(jbr)
			capabilities = jbr->capabilities;

		for(child = in_query->child; child; child = child->next) {
			if(child->type != XMLNODE_TYPE_TAG)
				continue;

			if(!strcmp(child->name, "identity")) {
				const char *category = xmlnode_get_attrib(child, "category");
				const char *type = xmlnode_get_attrib(child, "type");
				if(!category || !type)
					continue;

				if(!strcmp(category, "conference") && !strcmp(type, "text")) {
					/* we found a groupchat or MUC server, add it to the list */
					/* XXX: actually check for protocol/muc or gc-1.0 support */
					js->chat_servers = g_list_prepend(js->chat_servers, g_strdup(from));
				} else if(!strcmp(category, "directory") && !strcmp(type, "user")) {
					/* we found a JUD */
					js->user_directories = g_list_prepend(js->user_directories, g_strdup(from));
				} else if(!strcmp(category, "proxy") && !strcmp(type, "bytestreams")) {
					/* This is a bytestream proxy */
					JabberIq *iq;
					JabberBytestreamsStreamhost *sh;

					purple_debug_info("jabber", "Found bytestream proxy server: %s\n", from);

					sh = g_new0(JabberBytestreamsStreamhost, 1);
					sh->jid = g_strdup(from);
					js->bs_proxies = g_list_prepend(js->bs_proxies, sh);

					iq = jabber_iq_new_query(js, JABBER_IQ_GET,
								 "http://jabber.org/protocol/bytestreams");
					xmlnode_set_attrib(iq->node, "to", sh->jid);
					jabber_iq_set_callback(iq, jabber_disco_bytestream_server_cb, sh);
					jabber_iq_send(iq);
				}

			} else if(!strcmp(child->name, "feature")) {
				const char *var = xmlnode_get_attrib(child, "var");
				if(!var)
					continue;

				if(!strcmp(var, "http://jabber.org/protocol/si"))
					capabilities |= JABBER_CAP_SI;
				else if(!strcmp(var, "http://jabber.org/protocol/si/profile/file-transfer"))
					capabilities |= JABBER_CAP_SI_FILE_XFER;
				else if(!strcmp(var, "http://jabber.org/protocol/bytestreams"))
					capabilities |= JABBER_CAP_BYTESTREAMS;
				else if(!strcmp(var, "jabber:iq:search"))
					capabilities |= JABBER_CAP_IQ_SEARCH;
				else if(!strcmp(var, "jabber:iq:register"))
					capabilities |= JABBER_CAP_IQ_REGISTER;
				else if(!strcmp(var, "urn:xmpp:ping"))
					capabilities |= JABBER_CAP_PING;
				else if(!strcmp(var, "http://jabber.org/protocol/disco#items"))
					capabilities |= JABBER_CAP_ITEMS;
				else if(!strcmp(var, "http://jabber.org/protocol/commands")) {
					capabilities |= JABBER_CAP_ADHOC;
				}
				else if(!strcmp(var, "http://jabber.org/protocol/ibb")) {
					purple_debug_info("jabber", "remote supports IBB\n");
					capabilities |= JABBER_CAP_IBB;
				}
			}
		}

		capabilities |= JABBER_CAP_RETRIEVED;

		if(jbr)
			jbr->capabilities = capabilities;

		if((jdicd = g_hash_table_lookup(js->disco_callbacks, id))) {
			jdicd->callback(js, from, capabilities, jdicd->data);
			g_hash_table_remove(js->disco_callbacks, id);
		}
	} else if(type == JABBER_IQ_ERROR) {
		JabberID *jid;
		JabberBuddy *jb;
		JabberBuddyResource *jbr = NULL;
		JabberCapabilities capabilities = JABBER_CAP_NONE;
		struct _jabber_disco_info_cb_data *jdicd;

		if(!(jdicd = g_hash_table_lookup(js->disco_callbacks, id)))
			return;

		if((jid = jabber_id_new(from))) {
			if(jid->resource && (jb = jabber_buddy_find(js, from, TRUE)))
				jbr = jabber_buddy_find_resource(jb, jid->resource);
			jabber_id_free(jid);
		}

		if(jbr)
			capabilities = jbr->capabilities;

		jdicd->callback(js, from, capabilities, jdicd->data);
		g_hash_table_remove(js->disco_callbacks, id);
	}
}

void jabber_disco_items_parse(JabberStream *js, const char *from,
                              JabberIqType type, const char *id,
                              xmlnode *query) {
	if(type == JABBER_IQ_GET) {
		JabberIq *iq = jabber_iq_new_query(js, JABBER_IQ_RESULT,
				"http://jabber.org/protocol/disco#items");

		/* preserve node */
		xmlnode *iq_query = xmlnode_get_child(iq->node, "query");
		const char *node = xmlnode_get_attrib(query, "node");
		if(node)
			xmlnode_set_attrib(iq_query,"node",node);

		jabber_iq_set_id(iq, id);

		if (from)
			xmlnode_set_attrib(iq->node, "to", from);
		jabber_iq_send(iq);
	}
}

static void
jabber_disco_finish_server_info_result_cb(JabberStream *js)
{
	const char *ft_proxies;

	jabber_vcard_fetch_mine(js);

	if (!(js->server_caps & JABBER_CAP_GOOGLE_ROSTER)) {
		/* If the server supports JABBER_CAP_GOOGLE_ROSTER; we will have already requested it */
		jabber_roster_request(js);
	}

	/* Send initial presence; this will trigger receipt of presence for contacts on the roster */
	jabber_presence_send(js->gc->account, NULL);

	if (js->server_caps & JABBER_CAP_ADHOC) {
		/* The server supports ad-hoc commands, so let's request the list */
		jabber_adhoc_server_get_list(js);
	}

	/* If the server supports blocking, request the block list */
	if (js->server_caps & JABBER_CAP_BLOCKING) {
		jabber_request_block_list(js);
	}

	/* If there are manually specified bytestream proxies, query them */
	ft_proxies = purple_account_get_string(js->gc->account, "ft_proxies", NULL);
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

			iq = jabber_iq_new_query(js, JABBER_IQ_GET,
						 "http://jabber.org/protocol/bytestreams");
			xmlnode_set_attrib(iq->node, "to", sh->jid);
			jabber_iq_set_callback(iq, jabber_disco_bytestream_server_cb, sh);
			jabber_iq_send(iq);
		}

		g_strfreev(ft_proxy_list);
	}

}

struct _disco_data {
	struct jabber_disco_list_data *list_data;
	PurpleDiscoService *parent;
	char *node;
};

static void
jabber_disco_server_info_result_cb(JabberStream *js, const char *from,
                                   JabberIqType type, const char *id,
                                   xmlnode *packet, gpointer data)
{
	xmlnode *query, *child;

	if (!from || strcmp(from, js->user->domain)) {
		jabber_disco_finish_server_info_result_cb(js);
		return;
	}

	if (type == JABBER_IQ_ERROR) {
		/* A common way to get here is for the server not to support xmlns http://jabber.org/protocol/disco#info */
		jabber_disco_finish_server_info_result_cb(js);
		return;
	}

	query = xmlnode_get_child(packet, "query");

	if (!query) {
		jabber_disco_finish_server_info_result_cb(js);
		return;
	}

	for (child = xmlnode_get_child(query, "identity"); child;
	     child = xmlnode_get_next_twin(child)) {
		const char *category, *type, *name;
		category = xmlnode_get_attrib(child, "category");
		type = xmlnode_get_attrib(child, "type");
		if(category && type && !strcmp(category, "pubsub") && !strcmp(type,"pep"))
			js->pep = TRUE;
		if (!category || strcmp(category, "server"))
			continue;
		if (!type || strcmp(type, "im"))
			continue;

		name = xmlnode_get_attrib(child, "name");
		if (!name)
			continue;

		g_free(js->server_name);
		js->server_name = g_strdup(name);
		if (!strcmp(name, "Google Talk")) {
			purple_debug_info("jabber", "Google Talk!\n");
			js->googletalk = TRUE;

			/* autodiscover stun and relays */
			jabber_google_send_jingle_info(js);
		} else {
			/* TODO: add external service discovery here... */
		}
	}

	for (child = xmlnode_get_child(query, "feature"); child;
	     child = xmlnode_get_next_twin(child)) {
		const char *var;
		var = xmlnode_get_attrib(child, "var");
		if (!var)
			continue;

		if (!strcmp("google:mail:notify", var)) {
			js->server_caps |= JABBER_CAP_GMAIL_NOTIFY;
			jabber_gmail_init(js);
		} else if (!strcmp("google:roster", var)) {
			js->server_caps |= JABBER_CAP_GOOGLE_ROSTER;
			jabber_google_roster_init(js);
		} else if (!strcmp("http://jabber.org/protocol/commands", var)) {
			js->server_caps |= JABBER_CAP_ADHOC;
		} else if (!strcmp("urn:xmpp:blocking", var)) {
			js->server_caps |= JABBER_CAP_BLOCKING;
		}
	}

	jabber_disco_finish_server_info_result_cb(js);
}

static void
jabber_disco_server_items_result_cb(JabberStream *js, const char *from,
                                    JabberIqType type, const char *id,
                                    xmlnode *packet, gpointer data)
{
	xmlnode *query, *child;

	if (!from || strcmp(from, js->user->domain) != 0)
		return;

	if (type == JABBER_IQ_ERROR)
		return;

	while(js->chat_servers) {
		g_free(js->chat_servers->data);
		js->chat_servers = g_list_delete_link(js->chat_servers, js->chat_servers);
	}

	query = xmlnode_get_child(packet, "query");

	for(child = xmlnode_get_child(query, "item"); child;
			child = xmlnode_get_next_twin(child)) {
		JabberIq *iq;
		const char *jid, *node;

		if(!(jid = xmlnode_get_attrib(child, "jid")))
			continue;

		/* we don't actually care about the specific nodes,
		 * so we won't query them */
		if((node = xmlnode_get_attrib(child, "node")))
			continue;

		iq = jabber_iq_new_query(js, JABBER_IQ_GET, "http://jabber.org/protocol/disco#info");
		xmlnode_set_attrib(iq->node, "to", jid);
		jabber_iq_send(iq);
	}
}

void jabber_disco_items_server(JabberStream *js)
{
	JabberIq *iq = jabber_iq_new_query(js, JABBER_IQ_GET,
			"http://jabber.org/protocol/disco#items");

	xmlnode_set_attrib(iq->node, "to", js->user->domain);

	jabber_iq_set_callback(iq, jabber_disco_server_items_result_cb, NULL);
	jabber_iq_send(iq);

	iq = jabber_iq_new_query(js, JABBER_IQ_GET, "http://jabber.org/protocol/disco#info");
	xmlnode_set_attrib(iq->node, "to", js->user->domain);
	jabber_iq_set_callback(iq, jabber_disco_server_info_result_cb, NULL);
	jabber_iq_send(iq);
}

void jabber_disco_info_do(JabberStream *js, const char *who, JabberDiscoInfoCallback *callback, gpointer data)
{
	JabberID *jid;
	JabberBuddy *jb;
	JabberBuddyResource *jbr = NULL;
	struct _jabber_disco_info_cb_data *jdicd;
	char *id;
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

	iq = jabber_iq_new_query(js, JABBER_IQ_GET, "http://jabber.org/protocol/disco#info");
	xmlnode_set_attrib(iq->node, "to", who);

	id = jabber_get_next_id(js);
	jabber_iq_set_id(iq, id);
	g_hash_table_insert(js->disco_callbacks, id, jdicd);

	jabber_iq_send(iq);
}

static void
jabber_disco_list_data_destroy(struct jabber_disco_list_data *data)
{
	g_free(data->server);
	g_free(data);
}

static void
disco_proto_data_destroy_cb(PurpleDiscoList *list)
{
	struct jabber_disco_list_data *data;
	
	data = purple_disco_list_get_protocol_data(list);
	jabber_disco_list_data_destroy(data);
}

static PurpleDiscoServiceType
jabber_disco_category_from_string(const gchar *str)
{
	if (!g_ascii_strcasecmp(str, "gateway"))
		return PURPLE_DISCO_SERVICE_TYPE_GATEWAY;
	else if (!g_ascii_strcasecmp(str, "directory"))
		return PURPLE_DISCO_SERVICE_TYPE_DIRECTORY;
	else if (!g_ascii_strcasecmp(str, "conference"))
		return PURPLE_DISCO_SERVICE_TYPE_CHAT;

	return PURPLE_DISCO_SERVICE_TYPE_OTHER;
}

static const gchar *
jabber_disco_type_from_string(const gchar *str)
{
	if (!strcasecmp(str, "xmpp"))
		return "jabber";
	else if (!strcasecmp(str, "icq"))
		return "icq";
	else if (!strcasecmp(str, "smtp"))
		return "smtp";
	else if (!strcasecmp(str, "yahoo"))
		return "yahoo";
	else if (!strcasecmp(str, "irc"))
		return "irc";
	else if (!strcasecmp(str, "gadu-gadu"))
		return "gg";
	else if (!strcasecmp(str, "aim"))
		return "aim";
	else if (!strcasecmp(str, "qq"))
		return "qq";
	else if (!strcasecmp(str, "msn"))
		return "msn";

	/* fallback to the string itself */
	return str;
}

static void
jabber_disco_service_info_cb(JabberStream *js, const char *from,
                             JabberIqType type, const char *id,
                             xmlnode *packet, gpointer data);

static void
jabber_disco_service_items_cb(JabberStream *js, const char *who, const char *node,
                              GSList *items, gpointer data)
{
	GSList *l;
	struct _disco_data *disco_data;
	struct jabber_disco_list_data *list_data;
	PurpleDiscoList *list;	
	PurpleDiscoService *parent;
	const char *parent_node;
	gboolean has_items = FALSE;

	disco_data = data;
	list_data = disco_data->list_data;
	list = list_data->list;

	--list_data->fetch_count;

	if (list_data->list == NULL) {
		if (list_data->fetch_count == 0)
			jabber_disco_list_data_destroy(list_data);

		return;
	}

	if (items == NULL) {
		if (list_data->fetch_count == 0)
			purple_disco_list_set_in_progress(list, FALSE);

		purple_disco_list_unref(list);
		return;
	}

	parent = disco_data->parent;
	parent_node = disco_data->node;

	for (l = items; l; l = l->next) {
		JabberDiscoItem *item = l->data;
		JabberIq *iq;
		struct _disco_data *req_data;
		char *full_node;

		if (parent_node) {
			if (item->node) {
				full_node = g_strdup_printf("%s/%s", parent_node, item->node);
			} else {
				continue;
			}
		} else {
			full_node = g_strdup(item->node);
		}

		req_data = g_new0(struct _disco_data, 1);
		req_data->list_data = list_data;
		req_data->parent = parent;
		req_data->node = full_node;

		has_items = TRUE;

		++list_data->fetch_count;
		purple_disco_list_ref(list);

		iq = jabber_iq_new_query(js, JABBER_IQ_GET, "http://jabber.org/protocol/disco#info");
		xmlnode_set_attrib(iq->node, "to", item->jid);
		if (full_node)
			xmlnode_set_attrib(xmlnode_get_child(iq->node, "query"),
			                   "node", full_node);
		jabber_iq_set_callback(iq, jabber_disco_service_info_cb, req_data);

		jabber_iq_send(iq);
	}

	g_slist_foreach(items, (GFunc)jabber_disco_item_free, NULL);
	g_slist_free(items);

	if (list_data->fetch_count == 0)
		purple_disco_list_set_in_progress(list, FALSE);

	purple_disco_list_unref(list);

	g_free(disco_data->node);
	g_free(disco_data);
}

static void
jabber_disco_service_info_cb(JabberStream *js, const char *from,
                             JabberIqType type, const char *id,
                             xmlnode *packet, gpointer data)
{
	struct _disco_data *disco_data = data;
	struct jabber_disco_list_data *list_data;
	PurpleDiscoList *list;
	PurpleDiscoService *parent;
	char *node;
	xmlnode *query, *ident, *child;
	const char *acat, *atype, *adesc, *anode;
	char *aname;
	PurpleDiscoService *s;
	PurpleDiscoServiceType service_type;
	const char *gateway_type = NULL;
	PurpleDiscoServiceFlags flags = PURPLE_DISCO_ADD;

	list_data = disco_data->list_data;
	list = list_data->list;
	parent = disco_data->parent;

	node = disco_data->node;
	disco_data->node = NULL;
	g_free(disco_data);

	--list_data->fetch_count;

	if (list_data->list == NULL) {
		if (list_data->fetch_count == 0)
			jabber_disco_list_data_destroy(list_data);

		return;
	}

	if (!from || type == JABBER_IQ_ERROR
			|| (!(query = xmlnode_get_child(packet, "query")))
			|| (!(ident = xmlnode_get_child(query, "identity")))) {
		if (list_data->fetch_count == 0)
			purple_disco_list_set_in_progress(list, FALSE);

		purple_disco_list_unref(list);
		return;
	}

	acat = xmlnode_get_attrib(ident, "category");
	atype = xmlnode_get_attrib(ident, "type");
	adesc = xmlnode_get_attrib(ident, "name");
	anode = xmlnode_get_attrib(query, "node");

	if (anode) {
		aname = g_new0(char, strlen(from) + strlen(anode) + 1);
		strcat(aname, from);
		strcat(aname, anode);
	} else {
		aname = g_strdup(from);
	}

	service_type = jabber_disco_category_from_string(acat);
	if (service_type == PURPLE_DISCO_SERVICE_TYPE_GATEWAY)
		gateway_type = jabber_disco_type_from_string(atype);

	for (child = xmlnode_get_child(query, "feature"); child;
			child = xmlnode_get_next_twin(child)) {
		const char *var;

		if (!(var = xmlnode_get_attrib(child, "var")))
			continue;
		
		if (!strcmp(var, "jabber:iq:register"))
			flags |= PURPLE_DISCO_REGISTER;
		
		if (!strcmp(var, "http://jabber.org/protocol/disco#items"))
			flags |= PURPLE_DISCO_BROWSE;

		if (!strcmp(var, "http://jabber.org/protocol/muc"))
			service_type = PURPLE_DISCO_SERVICE_TYPE_CHAT;
	}

	purple_debug_info("disco", "service %s, category %s (%d), type %s (%s), description %s, flags %04x\n",
			aname,
			acat, service_type,
			atype, gateway_type ? gateway_type : "(null)",
			adesc, flags);

	s = purple_disco_list_service_new(service_type, aname, adesc, flags);
	if (service_type == PURPLE_DISCO_SERVICE_TYPE_GATEWAY)
		purple_disco_service_set_gateway_type(s, gateway_type);

	purple_disco_list_service_add(list, s, parent);

	/* if (flags & PURPLE_DISCO_FLAG_BROWSE) - not all browsable services has this future */
	{
		++list_data->fetch_count;
		purple_disco_list_ref(list);
		disco_data = g_new0(struct _disco_data, 1);
		disco_data->list_data = list_data;
		disco_data->parent = s;

		jabber_disco_items_do(js, from, node, jabber_disco_service_items_cb,
		                      disco_data);
	}

	if (list_data->fetch_count == 0)
		purple_disco_list_set_in_progress(list, FALSE);

	purple_disco_list_unref(list);

	g_free(aname);
	g_free(node);
}

static void
jabber_disco_server_items_cb(JabberStream *js, const char *from,
                             JabberIqType type, const char *id,
                             xmlnode *packet, gpointer data)
{
	struct jabber_disco_list_data *list_data;
	xmlnode *query, *child;
	gboolean has_items = FALSE;

	if (!from || type == JABBER_IQ_ERROR)
		return;

	list_data = data;
	--list_data->fetch_count;

	if (list_data->list == NULL) {
		if (list_data->fetch_count == 0)
			jabber_disco_list_data_destroy(list_data);

		return;
	}

	query = xmlnode_get_child(packet, "query");

	for(child = xmlnode_get_child(query, "item"); child;
			child = xmlnode_get_next_twin(child)) {
		JabberIq *iq;
		const char *jid;
		struct _disco_data *disco_data;

		if(!(jid = xmlnode_get_attrib(child, "jid")))
			continue;

		disco_data = g_new0(struct _disco_data, 1);
		disco_data->list_data = list_data;

		has_items = TRUE;
		++list_data->fetch_count;
		purple_disco_list_ref(list_data->list);
		iq = jabber_iq_new_query(js, JABBER_IQ_GET, "http://jabber.org/protocol/disco#info");
		xmlnode_set_attrib(iq->node, "to", jid);
		jabber_iq_set_callback(iq, jabber_disco_service_info_cb, disco_data);

		jabber_iq_send(iq);
	}

	if (!has_items)
		purple_disco_list_set_in_progress(list_data->list, FALSE);

	purple_disco_list_unref(list_data->list);
}

static void
jabber_disco_server_info_cb(JabberStream *js, const char *who, JabberCapabilities caps, gpointer data)
{
	struct jabber_disco_list_data *list_data;

	list_data = data;
	--list_data->fetch_count;

	if (!list_data->list) {
		purple_disco_list_unref(list_data->list);

		if (list_data->fetch_count == 0)
			jabber_disco_list_data_destroy(list_data);

		return;
	}

	if (caps & JABBER_CAP_ITEMS) {
		JabberIq *iq = jabber_iq_new_query(js, JABBER_IQ_GET, "http://jabber.org/protocol/disco#items");
		xmlnode_set_attrib(iq->node, "to", who);
		jabber_iq_set_callback(iq, jabber_disco_server_items_cb, list_data);

		++list_data->fetch_count;
		purple_disco_list_ref(list_data->list);

		jabber_iq_send(iq);
	} else {
		purple_notify_error(NULL, _("Error"), _("Server doesn't support service discovery"), NULL); 
		purple_disco_list_set_in_progress(list_data->list, FALSE);
	}

	purple_disco_list_unref(list_data->list);
}

static void discolist_cancel_cb(struct jabber_disco_list_data *list_data, const char *server)
{
	purple_disco_list_set_in_progress(list_data->list, FALSE);
	purple_disco_list_unref(list_data->list);
}

static void discolist_ok_cb(struct jabber_disco_list_data *list_data, const char *server)
{
	JabberStream *js;

	js = list_data->js;

	if (!server || !*server) {
		purple_notify_error(js->gc, _("Invalid Server"), _("Invalid Server"), NULL);

		purple_disco_list_set_in_progress(list_data->list, FALSE);
		purple_disco_list_unref(list_data->list);
		return;
	}

	list_data->server = g_strdup(server);
	if (js->last_disco_server)
		g_free(js->last_disco_server);
	js->last_disco_server = g_strdup(server);

	purple_disco_list_set_in_progress(list_data->list, TRUE);

	++list_data->fetch_count;
	jabber_disco_info_do(js, list_data->server, jabber_disco_server_info_cb, list_data);
}

static void
jabber_disco_cancel(PurpleDiscoList *list)
{
	struct jabber_disco_list_data *list_data = purple_disco_list_get_protocol_data(list);
	purple_disco_list_set_protocol_data(list, NULL, NULL);

	if (list_data->fetch_count == 0) {
		/* Nothing outstanding, just free it now... */
		jabber_disco_list_data_destroy(list_data);
	} else {
		int i;
		/* Lose all our references to the PurpleDiscoList */
		for (i = 0; i < list_data->fetch_count; ++i) {
			purple_disco_list_unref(list);
		}

		/* We'll free list_data when fetch_count is down to 0 */
		list_data->list = NULL;
	}
}

static void
jabber_disco_service_register(PurpleConnection *gc, PurpleDiscoService *service)
{
	JabberStream *js = purple_connection_get_protocol_data(gc);

	jabber_register_gateway(js, purple_disco_service_get_name(service));
}


PurpleDiscoList *
jabber_disco_get_list(PurpleConnection *gc)
{
	PurpleAccount *account;
	PurpleDiscoList *list;
	JabberStream *js;
	struct jabber_disco_list_data *disco_list_data;

	account = purple_connection_get_account(gc);
	js = purple_connection_get_protocol_data(gc);

	/* We start with a ref */
	list = purple_disco_list_new(account);

	disco_list_data = g_new0(struct jabber_disco_list_data, 1);
	disco_list_data->list = list;
	disco_list_data->js = js;
	purple_disco_list_set_protocol_data(list, disco_list_data, disco_proto_data_destroy_cb);
	purple_disco_list_set_cancel_func(list, jabber_disco_cancel);
	purple_disco_list_set_register_func(list, jabber_disco_service_register);

	purple_request_input(gc, _("Server name request"), _("Enter an XMPP Server"),
			_("Select an XMPP server to query"),
			js->last_disco_server ? js->last_disco_server : js->user->domain,
			FALSE, FALSE, NULL,
			_("Find Services"), PURPLE_CALLBACK(discolist_ok_cb),
			_("Cancel"), PURPLE_CALLBACK(discolist_cancel_cb),
			account, NULL, NULL, disco_list_data);

	return list;
}

static void
jabber_disco_items_cb(JabberStream *js, const char *from, JabberIqType type,
                      const char *id, xmlnode *packet, gpointer data)
{
	struct _jabber_disco_items_cb_data *jdicd;
	xmlnode *query, *child;
	const char *node = NULL;
	GSList *items = NULL;

	jdicd = data;

	query = xmlnode_get_child(packet, "query");

	if (query)
		node = xmlnode_get_attrib(query, "node");

	if (!from || !query || type == JABBER_IQ_ERROR) {
		jdicd->callback(js, from, node, NULL, jdicd->data);
		g_free(jdicd);
		return;
	}

	for (child = xmlnode_get_child(query, "item"); child;
			child = xmlnode_get_next_twin(child)) {
		JabberDiscoItem *item;

		item = g_new0(JabberDiscoItem, 1);
		item->jid  = g_strdup(xmlnode_get_attrib(child, "jid"));
		item->node = g_strdup(xmlnode_get_attrib(child, "node"));
		item->name = g_strdup(xmlnode_get_attrib(child, "name"));

		items = g_slist_prepend(items, item);
	}

	items = g_slist_reverse(items);
	jdicd->callback(js, from, node, items, jdicd->data);
	g_free(jdicd);
}

void jabber_disco_items_do(JabberStream *js, const char *who, const char *node,
		JabberDiscoItemsCallback *callback, gpointer data)
{
	struct _jabber_disco_items_cb_data *jdicd;
	JabberIq *iq;

	jdicd = g_new0(struct _jabber_disco_items_cb_data, 1);
	jdicd->data = data;
	jdicd->callback = callback;

	iq = jabber_iq_new_query(js, JABBER_IQ_GET, "http://jabber.org/protocol/disco#items");
	if (node)
		xmlnode_set_attrib(xmlnode_get_child(iq->node, "query"), "node", node);

	xmlnode_set_attrib(iq->node, "to", who);

	jabber_iq_set_callback(iq, jabber_disco_items_cb, jdicd);
	jabber_iq_send(iq);
}

void jabber_disco_item_free(JabberDiscoItem *item)
{
	g_free((char *)item->jid);
	g_free((char *)item->node);
	g_free((char *)item->name);
	g_free(item);
}
