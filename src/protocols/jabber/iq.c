/*
 * gaim - Jabber Protocol Plugin
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include "internal.h"
#include "debug.h"
#include "prefs.h"

#include "buddy.h"
#include "iq.h"
#include "oob.h"
#include "roster.h"
#include "si.h"

#ifdef _WIN32
#include "utsname.h"
#endif

JabberIq *jabber_iq_new(JabberStream *js, JabberIqType type)
{
	JabberIq *iq;

	iq = g_new0(JabberIq, 1);

	iq->type = type;

	iq->node = xmlnode_new("iq");
	switch(iq->type) {
		case JABBER_IQ_SET:
			xmlnode_set_attrib(iq->node, "type", "set");
			break;
		case JABBER_IQ_GET:
			xmlnode_set_attrib(iq->node, "type", "get");
			break;
		case JABBER_IQ_ERROR:
			xmlnode_set_attrib(iq->node, "type", "error");
			break;
		case JABBER_IQ_RESULT:
			xmlnode_set_attrib(iq->node, "type", "result");
			break;
		case JABBER_IQ_NONE:
			/* this shouldn't ever happen */
			break;
	}

	iq->js = js;

	if(type == JABBER_IQ_GET || type == JABBER_IQ_SET) {
		iq->id = jabber_get_next_id(js);
		xmlnode_set_attrib(iq->node, "id", iq->id);
	}

	return iq;
}

JabberIq *jabber_iq_new_query(JabberStream *js, JabberIqType type,
		const char *xmlns)
{
	JabberIq *iq = jabber_iq_new(js, type);
	xmlnode *query;

	query = xmlnode_new_child(iq->node, "query");
	xmlnode_set_attrib(query, "xmlns", xmlns);

	return iq;
}

typedef struct _JabberCallbackData {
	JabberIqCallback *callback;
	gpointer data;
} JabberCallbackData;

void
jabber_iq_set_callback(JabberIq *iq, JabberIqCallback *callback, gpointer data)
{
	iq->callback = callback;
	iq->callback_data = data;
}

void jabber_iq_set_id(JabberIq *iq, const char *id)
{
	if(iq->id)
		g_free(iq->id);

	if(id) {
		xmlnode_set_attrib(iq->node, "id", id);
		iq->id = g_strdup(id);
	} else {
		xmlnode_remove_attrib(iq->node, "id");
		iq->id = NULL;
	}
}

void jabber_iq_send(JabberIq *iq)
{
	JabberCallbackData *jcd;
	g_return_if_fail(iq != NULL);

	jabber_send(iq->js, iq->node);

	if(iq->id && iq->callback) {
		jcd = g_new0(JabberCallbackData, 1);
		jcd->callback = iq->callback;
		jcd->data = iq->callback_data;
		g_hash_table_insert(iq->js->callbacks, g_strdup(iq->id), jcd);
	}

	jabber_iq_free(iq);
}

void jabber_iq_free(JabberIq *iq)
{
	g_return_if_fail(iq != NULL);

	g_free(iq->id);
	xmlnode_free(iq->node);
	g_free(iq);
}

static void jabber_iq_handle_last(JabberStream *js, xmlnode *packet)
{
	JabberIq *iq;
	const char *type;
	const char *from;
	const char *id;
	xmlnode *query;
	char *idle_time;

	type = xmlnode_get_attrib(packet, "type");
	from = xmlnode_get_attrib(packet, "from");
	id = xmlnode_get_attrib(packet, "id");

	if(type && !strcmp(type, "get")) {
		iq = jabber_iq_new_query(js, JABBER_IQ_RESULT, "jabber:iq:last");
		jabber_iq_set_id(iq, id);
		xmlnode_set_attrib(iq->node, "to", from);

		query = xmlnode_get_child(iq->node, "query");

		idle_time = g_strdup_printf("%ld", js->idle ? time(NULL) - js->idle : 0);
		xmlnode_set_attrib(query, "seconds", idle_time);
		g_free(idle_time);

		jabber_iq_send(iq);
	}
}

static void jabber_iq_handle_time(JabberStream *js, xmlnode *packet)
{
	const char *type, *from, *id;
	JabberIq *iq;
	char buf[1024];
	xmlnode *query;
	time_t now_t;
	struct tm now;
	time(&now_t);
	localtime_r(&now_t, &now);

	type = xmlnode_get_attrib(packet, "type");
	from = xmlnode_get_attrib(packet, "from");
	id = xmlnode_get_attrib(packet, "id");

	if(type && !strcmp(type, "get")) {

		iq = jabber_iq_new_query(js, JABBER_IQ_RESULT, "jabber:iq:time");
		jabber_iq_set_id(iq, id);
		xmlnode_set_attrib(iq->node, "to", from);

		query = xmlnode_get_child(iq->node, "query");

		strftime(buf, sizeof(buf), "%Y%m%dT%T", &now);
		xmlnode_insert_data(xmlnode_new_child(query, "utc"), buf, -1);
		strftime(buf, sizeof(buf), "%Z", &now);
		xmlnode_insert_data(xmlnode_new_child(query, "tz"), buf, -1);
		strftime(buf, sizeof(buf), "%d %b %Y %T", &now);
		xmlnode_insert_data(xmlnode_new_child(query, "display"), buf, -1);

		jabber_iq_send(iq);
	}
}

static void jabber_iq_handle_version(JabberStream *js, xmlnode *packet)
{
	JabberIq *iq;
	const char *type, *from, *id;
	xmlnode *query;
	char *os = NULL;

	type = xmlnode_get_attrib(packet, "type");

	if(type && !strcmp(type, "get")) {

		if(!gaim_prefs_get_bool("/plugins/prpl/jabber/hide_os")) {
			struct utsname osinfo;

			uname(&osinfo);
			os = g_strdup_printf("%s %s %s", osinfo.sysname, osinfo.release,
					osinfo.machine);
		}

		from = xmlnode_get_attrib(packet, "from");
		id = xmlnode_get_attrib(packet, "id");

		iq = jabber_iq_new_query(js, JABBER_IQ_RESULT, "jabber:iq:version");
		xmlnode_set_attrib(iq->node, "to", from);
		jabber_iq_set_id(iq, id);

		query = xmlnode_get_child(iq->node, "query");

		xmlnode_insert_data(xmlnode_new_child(query, "name"), PACKAGE, -1);
		xmlnode_insert_data(xmlnode_new_child(query, "version"), VERSION, -1);
		if(os) {
			xmlnode_insert_data(xmlnode_new_child(query, "os"), os, -1);
			g_free(os);
		}

		jabber_iq_send(iq);
	}
}

#define SUPPORT_FEATURE(x) \
	feature = xmlnode_new_child(query, "feature"); \
	xmlnode_set_attrib(feature, "var", x);


void jabber_disco_info_parse(JabberStream *js, xmlnode *packet) {
	const char *from = xmlnode_get_attrib(packet, "from");
	const char *type = xmlnode_get_attrib(packet, "type");

	if(!from || !type)
		return;

	if(!strcmp(type, "get")) {
		xmlnode *query, *identity, *feature;
		JabberIq *iq = jabber_iq_new_query(js, JABBER_IQ_RESULT,
				"http://jabber.org/protocol/disco#info");

		jabber_iq_set_id(iq, xmlnode_get_attrib(packet, "id"));

		xmlnode_set_attrib(iq->node, "to", from);
		query = xmlnode_get_child(iq->node, "query");

		identity = xmlnode_new_child(query, "identity");
		xmlnode_set_attrib(identity, "category", "client");
		xmlnode_set_attrib(identity, "type", "pc"); /* XXX: bot, console,
													 * handheld, pc, phone,
													 * web */

		SUPPORT_FEATURE("jabber:iq:last")
		SUPPORT_FEATURE("jabber:iq:oob")
		SUPPORT_FEATURE("jabber:iq:time")
		SUPPORT_FEATURE("jabber:iq:version")
		SUPPORT_FEATURE("jabber:x:conference")
		/*
		SUPPORT_FEATURE("http://jabber.org/protocol/bytestreams")
		*/
		SUPPORT_FEATURE("http://jabber.org/protocol/disco#info")
		SUPPORT_FEATURE("http://jabber.org/protocol/disco#items")
		SUPPORT_FEATURE("http://jabber.org/protocol/muc")
		SUPPORT_FEATURE("http://jabber.org/protocol/muc#user")
		/*
		SUPPORT_FEATURE("http://jabber.org/protocol/si")
		SUPPORT_FEATURE("http://jabber.org/protocol/si/profile/file-transfer")
		*/
		SUPPORT_FEATURE("http://jabber.org/protocol/xhtml-im")

		jabber_iq_send(iq);
	} else if(!strcmp(type, "result")) {
		xmlnode *query = xmlnode_get_child(packet, "query");
		xmlnode *child;
		JabberID *jid;
		JabberBuddy *jb;
		JabberBuddyResource *jbr = NULL;

		if(!(jid = jabber_id_new(from)))
			return;

		if(jid->resource && (jb = jabber_buddy_find(js, from, TRUE)))
			jbr = jabber_buddy_find_resource(jb, jid->resource);
		jabber_id_free(jid);

		for(child = query->child; child; child = child->next) {
			if(child->type != XMLNODE_TYPE_TAG)
				continue;

			if(!strcmp(child->name, "identity")) {
				const char *category = xmlnode_get_attrib(child, "category");
				const char *type = xmlnode_get_attrib(child, "type");
				if(!category || !type)
					continue;

				/* we found a groupchat or MUC server, add it to the list */
				/* XXX: actually check for protocol/muc or gc-1.0 support */
				if(!strcmp(category, "conference") && !strcmp(type, "text"))
					js->chat_servers = g_list_append(js->chat_servers, g_strdup(from));

			} else if(!strcmp(child->name, "feature")) {
				const char *var = xmlnode_get_attrib(child, "var");
				if(!var)
					continue;

				if(jbr && !strcmp(var, "http://jabber.org/protocol/si"))
					jbr->capabilities |= JABBER_CAP_SI;
				else if(jbr && !strcmp(var,
							"http://jabber.org/protocol/si/profile/file-transfer"))
					jbr->capabilities |= JABBER_CAP_SI_FILE_XFER;
				else if(jbr && !strcmp(var, "http://jabber.org/protocol/bytestreams"))
					jbr->capabilities |= JABBER_CAP_BYTESTREAMS;
			}
		}
	}
}

void jabber_disco_items_parse(JabberStream *js, xmlnode *packet) {
	const char *from = xmlnode_get_attrib(packet, "from");
	const char *type = xmlnode_get_attrib(packet, "type");

	if(!strcmp(type, "get")) {
		JabberIq *iq = jabber_iq_new_query(js, JABBER_IQ_RESULT,
				"http://jabber.org/protocol/disco#items");

		jabber_iq_set_id(iq, xmlnode_get_attrib(packet, "id"));

		xmlnode_set_attrib(iq->node, "to", from);
		jabber_iq_send(iq);
	}
}

static void
jabber_iq_disco_server_result_cb(JabberStream *js, xmlnode *packet, gpointer data)
{
	xmlnode *query, *child;
	const char *from = xmlnode_get_attrib(packet, "from");
	const char *type = xmlnode_get_attrib(packet, "type");

	if(!from || !type)
		return;

	if(strcmp(from, js->user->domain))
		return;

	if(strcmp(type, "result"))
		return;

	while(js->chat_servers) {
		g_free(js->chat_servers->data);
		js->chat_servers = g_list_delete_link(js->chat_servers, js->chat_servers);
	}

	query = xmlnode_get_child(packet, "query");

	for(child = xmlnode_get_child(query, "item"); child;
			child = xmlnode_get_next_twin(child)) {
		JabberIq *iq;
		const char *jid;

		if(!(jid = xmlnode_get_attrib(child, "jid")))
			continue;

		iq = jabber_iq_new_query(js, JABBER_IQ_GET, "http://jabber.org/protocol/disco#info");
		xmlnode_set_attrib(iq->node, "to", jid);
		jabber_iq_send(iq);
	}
}

void jabber_iq_disco_server(JabberStream *js)
{
	JabberIq *iq = jabber_iq_new_query(js, JABBER_IQ_GET,
			"http://jabber.org/protocol/disco#items");

	xmlnode_set_attrib(iq->node, "to", js->user->domain);

	jabber_iq_set_callback(iq, jabber_iq_disco_server_result_cb, NULL);
	jabber_iq_send(iq);
}


void jabber_iq_parse(JabberStream *js, xmlnode *packet)
{
	JabberCallbackData *jcd;
	xmlnode *query;
	const char *xmlns;
	const char *type, *id, *from;
	JabberIq *iq;

	query = xmlnode_get_child(packet, "query");
	type = xmlnode_get_attrib(packet, "type");
	from = xmlnode_get_attrib(packet, "from");

	if(type && query && (xmlns = xmlnode_get_attrib(query, "xmlns"))) {
		if(!strcmp(type, "set")) {
			if(!strcmp(xmlns, "jabber:iq:roster")) {
				jabber_roster_parse(js, packet);
				return;
			} else if(!strcmp(xmlns, "jabber:iq:oob")) {
				jabber_oob_parse(js, packet);
				return;
			}
		} else if(!strcmp(type, "get")) {
			if(!strcmp(xmlns, "jabber:iq:last")) {
				jabber_iq_handle_last(js, packet);
				return;
			} else if(!strcmp(xmlns, "jabber:iq:time")) {
				jabber_iq_handle_time(js, packet);
				return;
			} else if(!strcmp(xmlns, "jabber:iq:version")) {
				jabber_iq_handle_version(js, packet);
				return;
			} else if(!strcmp(xmlns, "http://jabber.org/protocol/disco#info")) {
				jabber_disco_info_parse(js, packet);
				return;
			} else if(!strcmp(xmlns, "http://jabber.org/protocol/disco#items")) {
				jabber_disco_items_parse(js, packet);
				return;
			}
		} else if(!strcmp(type, "result")) {
			if(!strcmp(xmlns, "jabber:iq:roster")) {
				jabber_roster_parse(js, packet);
				return;
			} else if(!strcmp(xmlns, "jabber:iq:register")) {
				jabber_register_parse(js, packet);
				return;
			} else if(!strcmp(xmlns, "http://jabber.org/protocol/disco#info")) {
				jabber_disco_info_parse(js, packet);
				return;
			}
		}
	}

	/* If we got here, no pre-defined handlers got it, lets see if a special
	 * callback got registered */

	id = xmlnode_get_attrib(packet, "id");

	if(type && (!strcmp(type, "result") || !strcmp(type, "error")) && id
			&& *id && (jcd = g_hash_table_lookup(js->callbacks, id))) {
		jcd->callback(js, packet, jcd->data);
		g_hash_table_remove(js->callbacks, id);
		return;
	}

	/* Default error reply mandated by XMPP-CORE */

	iq = jabber_iq_new(js, JABBER_IQ_ERROR);
	xmlnode_set_attrib(iq->node, "id", id);
	xmlnode_set_attrib(iq->node, "to", from);

	for(query = packet->child; query; query = query->next) {
		switch(query->type) {
			case XMLNODE_TYPE_TAG:
				break;
			case XMLNODE_TYPE_ATTRIB:
				break;
			case XMLNODE_TYPE_DATA:
				break;
		}
	}
}

