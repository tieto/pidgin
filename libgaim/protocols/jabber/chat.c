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
#include "prpl.h" /* for proto_chat_entry */
#include "notify.h"
#include "request.h"
#include "roomlist.h"
#include "util.h"

#include "chat.h"
#include "iq.h"
#include "message.h"
#include "presence.h"
#include "xdata.h"

GList *jabber_chat_info(GaimConnection *gc)
{
	GList *m = NULL;
	struct proto_chat_entry *pce;

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("_Room:");
	pce->identifier = "room";
	pce->required = TRUE;
	m = g_list_append(m, pce);

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("_Server:");
	pce->identifier = "server";
	pce->required = TRUE;
	m = g_list_append(m, pce);

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("_Handle:");
	pce->identifier = "handle";
	pce->required = TRUE;
	m = g_list_append(m, pce);

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("_Password:");
	pce->identifier = "password";
	pce->secret = TRUE;
	m = g_list_append(m, pce);

	return m;
}

GHashTable *jabber_chat_info_defaults(GaimConnection *gc, const char *chat_name)
{
	GHashTable *defaults;
	JabberStream *js = gc->proto_data;

	defaults = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);

	g_hash_table_insert(defaults, "handle", g_strdup(js->user->node));

	if (js->chat_servers)
		g_hash_table_insert(defaults, "server", g_strdup(js->chat_servers->data));
	else
		g_hash_table_insert(defaults, "server", g_strdup("conference.jabber.org"));

	if (chat_name != NULL) {
		JabberID *jid = jabber_id_new(chat_name);
		if(jid) {
			g_hash_table_insert(defaults, "room", g_strdup(jid->node));
			if(jid->domain)
				g_hash_table_replace(defaults, "server", g_strdup(jid->domain));
			if(jid->resource)
				g_hash_table_replace(defaults, "handle", g_strdup(jid->resource));
			jabber_id_free(jid);
		}
	}

	return defaults;
}

JabberChat *jabber_chat_find(JabberStream *js, const char *room,
		const char *server)
{
	JabberChat *chat = NULL;
	char *room_jid;

	if(NULL != js->chats)
	{
		room_jid = g_strdup_printf("%s@%s", room, server);

		chat = g_hash_table_lookup(js->chats, jabber_normalize(NULL, room_jid));
		g_free(room_jid);
	}

	return chat;
}

struct _find_by_id_data {
	int id;
	JabberChat *chat;
};

static void find_by_id_foreach_cb(gpointer key, gpointer value, gpointer user_data)
{
	JabberChat *chat = value;
	struct _find_by_id_data *fbid = user_data;

	if(chat->id == fbid->id)
		fbid->chat = chat;
}

JabberChat *jabber_chat_find_by_id(JabberStream *js, int id)
{
	JabberChat *chat;
	struct _find_by_id_data *fbid = g_new0(struct _find_by_id_data, 1);
	fbid->id = id;
	g_hash_table_foreach(js->chats, find_by_id_foreach_cb, fbid);
	chat = fbid->chat;
	g_free(fbid);
	return chat;
}

JabberChat *jabber_chat_find_by_conv(GaimConversation *conv)
{
	GaimAccount *account = gaim_conversation_get_account(conv);
	GaimConnection *gc = gaim_account_get_connection(account);
	JabberStream *js = gc->proto_data;
	int id = gaim_conv_chat_get_id(GAIM_CONV_CHAT(conv));

	return jabber_chat_find_by_id(js, id);
}

void jabber_chat_invite(GaimConnection *gc, int id, const char *msg,
		const char *name)
{
	JabberStream *js = gc->proto_data;
	JabberChat *chat;
	xmlnode *message, *body, *x, *invite;
	char *room_jid;

	chat = jabber_chat_find_by_id(js, id);
	if(!chat)
		return;

	message = xmlnode_new("message");

	room_jid = g_strdup_printf("%s@%s", chat->room, chat->server);

	if(chat->muc) {
		xmlnode_set_attrib(message, "to", room_jid);
		x = xmlnode_new_child(message, "x");
		xmlnode_set_namespace(x, "http://jabber.org/protocol/muc#user");
		invite = xmlnode_new_child(x, "invite");
		xmlnode_set_attrib(invite, "to", name);
		body = xmlnode_new_child(invite, "reason");
		xmlnode_insert_data(body, msg, -1);
	} else {
		xmlnode_set_attrib(message, "to", name);
		body = xmlnode_new_child(message, "body");
		xmlnode_insert_data(body, msg, -1);
		x = xmlnode_new_child(message, "x");
		xmlnode_set_attrib(x, "jid", room_jid);
		xmlnode_set_namespace(x, "jabber:x:conference");
	}

	jabber_send(js, message);
	xmlnode_free(message);
	g_free(room_jid);
}

void jabber_chat_member_free(JabberChatMember *jcm);

char *jabber_get_chat_name(GHashTable *data) {
	char *room, *server, *chat_name = NULL;

	room = g_hash_table_lookup(data, "room");
	server = g_hash_table_lookup(data, "server");

	if (room && server) {
		chat_name = g_strdup_printf("%s@%s", room, server);
	}
	return chat_name;
}

void jabber_chat_join(GaimConnection *gc, GHashTable *data)
{
	JabberChat *chat;
	char *room, *server, *handle, *passwd;
	xmlnode *presence, *x;
	char *tmp, *room_jid, *full_jid;
	JabberStream *js = gc->proto_data;
	GaimPresence *gpresence;
	GaimStatus *status;
	JabberBuddyState state;
	char *msg;
	int priority;

	room = g_hash_table_lookup(data, "room");
	server = g_hash_table_lookup(data, "server");
	handle = g_hash_table_lookup(data, "handle");
	passwd = g_hash_table_lookup(data, "password");

	if(!room || !server)
		return;

	if(!handle)
		handle = js->user->node;

	if(!jabber_nodeprep_validate(room)) {
		char *buf = g_strdup_printf(_("%s is not a valid room name"), room);
		gaim_notify_error(gc, _("Invalid Room Name"), _("Invalid Room Name"),
				buf);
		g_free(buf);
		return;
	} else if(!jabber_nameprep_validate(server)) {
		char *buf = g_strdup_printf(_("%s is not a valid server name"), server);
		gaim_notify_error(gc, _("Invalid Server Name"),
				_("Invalid Server Name"), buf);
		g_free(buf);
		return;
	} else if(!jabber_resourceprep_validate(handle)) {
		char *buf = g_strdup_printf(_("%s is not a valid room handle"), handle);
		gaim_notify_error(gc, _("Invalid Room Handle"),
				_("Invalid Room Handle"), buf);
	}

	if(jabber_chat_find(js, room, server))
		return;

	tmp = g_strdup_printf("%s@%s", room, server);
	room_jid = g_strdup(jabber_normalize(NULL, tmp));
	g_free(tmp);

	chat = g_new0(JabberChat, 1);
	chat->js = gc->proto_data;

	chat->room = g_strdup(room);
	chat->server = g_strdup(server);
	chat->handle = g_strdup(handle);

	chat->members = g_hash_table_new_full(g_str_hash, g_str_equal, NULL,
			(GDestroyNotify)jabber_chat_member_free);

	g_hash_table_insert(js->chats, room_jid, chat);

	gpresence = gaim_account_get_presence(gc->account);
	status = gaim_presence_get_active_status(gpresence);

	gaim_status_to_jabber(status, &state, &msg, &priority);

	presence = jabber_presence_create(state, msg, priority);
	full_jid = g_strdup_printf("%s/%s", room_jid, handle);
	xmlnode_set_attrib(presence, "to", full_jid);
	g_free(full_jid);
	g_free(msg);

	x = xmlnode_new_child(presence, "x");
	xmlnode_set_namespace(x, "http://jabber.org/protocol/muc");

	if(passwd && *passwd) {
		xmlnode *password = xmlnode_new_child(x, "password");
		xmlnode_insert_data(password, passwd, -1);
	}

	jabber_send(js, presence);
	xmlnode_free(presence);
}

void jabber_chat_leave(GaimConnection *gc, int id)
{
	JabberStream *js = gc->proto_data;
	JabberChat *chat = jabber_chat_find_by_id(js, id);


	if(!chat)
		return;

	jabber_chat_part(chat, NULL);

	chat->conv = NULL;
}

void jabber_chat_destroy(JabberChat *chat)
{
	JabberStream *js = chat->js;
	char *room_jid = g_strdup_printf("%s@%s", chat->room, chat->server);

	g_hash_table_remove(js->chats, jabber_normalize(NULL, room_jid));
	g_free(room_jid);
}

void jabber_chat_free(JabberChat *chat)
{
	if(chat->config_dialog_handle)
		gaim_request_close(chat->config_dialog_type, chat->config_dialog_handle);

	g_free(chat->room);
	g_free(chat->server);
	g_free(chat->handle);
	g_hash_table_destroy(chat->members);
	g_free(chat);
}

gboolean jabber_chat_find_buddy(GaimConversation *conv, const char *name)
{
	return gaim_conv_chat_find_user(GAIM_CONV_CHAT(conv), name);
}

char *jabber_chat_buddy_real_name(GaimConnection *gc, int id, const char *who)
{
	JabberStream *js = gc->proto_data;
	JabberChat *chat;

	chat = jabber_chat_find_by_id(js, id);

	if(!chat)
		return NULL;

	return g_strdup_printf("%s@%s/%s", chat->room, chat->server, who);
}

static void jabber_chat_room_configure_x_data_cb(JabberStream *js, xmlnode *result, gpointer data)
{
	JabberChat *chat = data;
	xmlnode *query;
	JabberIq *iq;
	char *to = g_strdup_printf("%s@%s", chat->room, chat->server);

	iq = jabber_iq_new_query(js, JABBER_IQ_SET, "http://jabber.org/protocol/muc#owner");
	xmlnode_set_attrib(iq->node, "to", to);
	g_free(to);

	query = xmlnode_get_child(iq->node, "query");

	xmlnode_insert_child(query, result);

	jabber_iq_send(iq);
}

static void jabber_chat_room_configure_cb(JabberStream *js, xmlnode *packet, gpointer data)
{
	xmlnode *query, *x;
	const char *type = xmlnode_get_attrib(packet, "type");
	const char *from = xmlnode_get_attrib(packet, "from");
	char *msg;
	JabberChat *chat;
	JabberID *jid;

	if(!type || !from)
		return;



	if(!strcmp(type, "result")) {
		jid = jabber_id_new(from);

		if(!jid)
			return;

		chat = jabber_chat_find(js, jid->node, jid->domain);
		jabber_id_free(jid);

		if(!chat)
			return;

		if(!(query = xmlnode_get_child(packet, "query")))
			return;

		for(x = xmlnode_get_child(query, "x"); x; x = xmlnode_get_next_twin(x)) {
			const char *xmlns;
			if(!(xmlns = xmlnode_get_namespace(x)))
				continue;

			if(!strcmp(xmlns, "jabber:x:data")) {
				chat->config_dialog_type = GAIM_REQUEST_FIELDS;
				chat->config_dialog_handle = jabber_x_data_request(js, x, jabber_chat_room_configure_x_data_cb, chat);
				return;
			}
		}
	} else if(!strcmp(type, "error")) {
		char *msg = jabber_parse_error(js, packet);

		gaim_notify_error(js->gc, _("Configuration error"), _("Configuration error"), msg);

		if(msg)
			g_free(msg);
		return;
	}

	msg = g_strdup_printf("Unable to configure room %s", from);

	gaim_notify_info(js->gc, _("Unable to configure"), _("Unable to configure"), msg);
	g_free(msg);

}

void jabber_chat_request_room_configure(JabberChat *chat) {
	JabberIq *iq;
	char *room_jid;

	if(!chat)
		return;

	chat->config_dialog_handle = NULL;

	if(!chat->muc) {
		gaim_notify_error(chat->js->gc, _("Room Configuration Error"), _("Room Configuration Error"),
				_("This room is not capable of being configured"));
		return;
	}

	iq = jabber_iq_new_query(chat->js, JABBER_IQ_GET,
			"http://jabber.org/protocol/muc#owner");
	room_jid = g_strdup_printf("%s@%s", chat->room, chat->server);

	xmlnode_set_attrib(iq->node, "to", room_jid);

	jabber_iq_set_callback(iq, jabber_chat_room_configure_cb, NULL);

	jabber_iq_send(iq);

	g_free(room_jid);
}

void jabber_chat_create_instant_room(JabberChat *chat) {
	JabberIq *iq;
	xmlnode *query, *x;
	char *room_jid;

	if(!chat)
		return;

	chat->config_dialog_handle = NULL;

	iq = jabber_iq_new_query(chat->js, JABBER_IQ_SET,
			"http://jabber.org/protocol/muc#owner");
	query = xmlnode_get_child(iq->node, "query");
	x = xmlnode_new_child(query, "x");
	room_jid = g_strdup_printf("%s@%s", chat->room, chat->server);

	xmlnode_set_attrib(iq->node, "to", room_jid);
	xmlnode_set_namespace(x, "jabber:x:data");
	xmlnode_set_attrib(x, "type", "submit");

	jabber_iq_send(iq);

	g_free(room_jid);
}

static void jabber_chat_register_x_data_result_cb(JabberStream *js, xmlnode *packet, gpointer data)
{
	const char *type = xmlnode_get_attrib(packet, "type");

	if(type && !strcmp(type, "error")) {
		char *msg = jabber_parse_error(js, packet);

		gaim_notify_error(js->gc, _("Registration error"), _("Registration error"), msg);

		if(msg)
			g_free(msg);
		return;
	}
}

static void jabber_chat_register_x_data_cb(JabberStream *js, xmlnode *result, gpointer data)
{
	JabberChat *chat = data;
	xmlnode *query;
	JabberIq *iq;
	char *to = g_strdup_printf("%s@%s", chat->room, chat->server);

	iq = jabber_iq_new_query(js, JABBER_IQ_SET, "jabber:iq:register");
	xmlnode_set_attrib(iq->node, "to", to);
	g_free(to);

	query = xmlnode_get_child(iq->node, "query");

	xmlnode_insert_child(query, result);

	jabber_iq_set_callback(iq, jabber_chat_register_x_data_result_cb, NULL);

	jabber_iq_send(iq);
}

static void jabber_chat_register_cb(JabberStream *js, xmlnode *packet, gpointer data)
{
	xmlnode *query, *x;
	const char *type = xmlnode_get_attrib(packet, "type");
	const char *from = xmlnode_get_attrib(packet, "from");
	char *msg;
	JabberChat *chat;
	JabberID *jid;

	if(!type || !from)
		return;

	if(!strcmp(type, "result")) {
		jid = jabber_id_new(from);

		if(!jid)
			return;

		chat = jabber_chat_find(js, jid->node, jid->domain);
		jabber_id_free(jid);

		if(!chat)
			return;

		if(!(query = xmlnode_get_child(packet, "query")))
			return;

		for(x = xmlnode_get_child(query, "x"); x; x = xmlnode_get_next_twin(x)) {
			const char *xmlns;

			if(!(xmlns = xmlnode_get_namespace(x)))
				continue;

			if(!strcmp(xmlns, "jabber:x:data")) {
				jabber_x_data_request(js, x, jabber_chat_register_x_data_cb, chat);
				return;
			}
		}
	} else if(!strcmp(type, "error")) {
		char *msg = jabber_parse_error(js, packet);

		gaim_notify_error(js->gc, _("Registration error"), _("Registration error"), msg);

		if(msg)
			g_free(msg);
		return;
	}

	msg = g_strdup_printf("Unable to configure room %s", from);

	gaim_notify_info(js->gc, _("Unable to configure"), _("Unable to configure"), msg);
	g_free(msg);

}

void jabber_chat_register(JabberChat *chat)
{
	JabberIq *iq;
	char *room_jid;

	if(!chat)
		return;

	room_jid = g_strdup_printf("%s@%s", chat->room, chat->server);

	iq = jabber_iq_new_query(chat->js, JABBER_IQ_GET, "jabber:iq:register");
	xmlnode_set_attrib(iq->node, "to", room_jid);
	g_free(room_jid);

	jabber_iq_set_callback(iq, jabber_chat_register_cb, NULL);

	jabber_iq_send(iq);
}

/* merge this with the function below when we get everyone on the same page wrt /commands */
void jabber_chat_change_topic(JabberChat *chat, const char *topic)
{
	if(topic && *topic) {
		JabberMessage *jm;
		jm = g_new0(JabberMessage, 1);
		jm->js = chat->js;
		jm->type = JABBER_MESSAGE_GROUPCHAT;
		jm->subject = gaim_markup_strip_html(topic);
		jm->to = g_strdup_printf("%s@%s", chat->room, chat->server);
		jabber_message_send(jm);
		jabber_message_free(jm);
	} else {
		const char *cur = gaim_conv_chat_get_topic(GAIM_CONV_CHAT(chat->conv));
		char *buf, *tmp, *tmp2;

		if(cur) {
			tmp = g_markup_escape_text(cur, -1);
			tmp2 = gaim_markup_linkify(tmp);
			buf = g_strdup_printf(_("current topic is: %s"), tmp2);
			g_free(tmp);
			g_free(tmp2);
		} else
			buf = g_strdup(_("No topic is set"));
		gaim_conv_chat_write(GAIM_CONV_CHAT(chat->conv), "", buf,
				GAIM_MESSAGE_SYSTEM | GAIM_MESSAGE_NO_LOG, time(NULL));
		g_free(buf);
	}

}

void jabber_chat_set_topic(GaimConnection *gc, int id, const char *topic)
{
	JabberStream *js = gc->proto_data;
	JabberChat *chat = jabber_chat_find_by_id(js, id);

	if(!chat)
		return;

	jabber_chat_change_topic(chat, topic);
}


void jabber_chat_change_nick(JabberChat *chat, const char *nick)
{
	xmlnode *presence;
	char *full_jid;
	GaimPresence *gpresence;
	GaimStatus *status;
	JabberBuddyState state;
	char *msg;
	int priority;

	if(!chat->muc) {
		gaim_conv_chat_write(GAIM_CONV_CHAT(chat->conv), "",
				_("Nick changing not supported in non-MUC chatrooms"),
				GAIM_MESSAGE_SYSTEM, time(NULL));
		return;
	}

	gpresence = gaim_account_get_presence(chat->js->gc->account);
	status = gaim_presence_get_active_status(gpresence);

	gaim_status_to_jabber(status, &state, &msg, &priority);

	presence = jabber_presence_create(state, msg, priority);
	full_jid = g_strdup_printf("%s@%s/%s", chat->room, chat->server, nick);
	xmlnode_set_attrib(presence, "to", full_jid);
	g_free(full_jid);
	g_free(msg);

	jabber_send(chat->js, presence);
	xmlnode_free(presence);
}

void jabber_chat_part(JabberChat *chat, const char *msg)
{
	char *room_jid;
	xmlnode *presence;

	room_jid = g_strdup_printf("%s@%s/%s", chat->room, chat->server,
			chat->handle);
	presence = xmlnode_new("presence");
	xmlnode_set_attrib(presence, "to", room_jid);
	xmlnode_set_attrib(presence, "type", "unavailable");
	if(msg) {
		xmlnode *status = xmlnode_new_child(presence, "status");
		xmlnode_insert_data(status, msg, -1);
	}
	jabber_send(chat->js, presence);
	xmlnode_free(presence);
	g_free(room_jid);
}

static void roomlist_disco_result_cb(JabberStream *js, xmlnode *packet, gpointer data)
{
	xmlnode *query;
	xmlnode *item;
	const char *type;

	if(!js->roomlist)
		return;

	if(!(type = xmlnode_get_attrib(packet, "type")) || strcmp(type, "result")) {
		char *err = jabber_parse_error(js,packet);
		gaim_notify_error(js->gc, _("Error"),
				_("Error retrieving room list"), err);
		gaim_roomlist_set_in_progress(js->roomlist, FALSE);
		gaim_roomlist_unref(js->roomlist);
		js->roomlist = NULL;
		g_free(err);
		return;
	}

	if(!(query = xmlnode_get_child(packet, "query"))) {
		char *err = jabber_parse_error(js, packet);
		gaim_notify_error(js->gc, _("Error"),
				_("Error retrieving room list"), err);
		gaim_roomlist_set_in_progress(js->roomlist, FALSE);
		gaim_roomlist_unref(js->roomlist);
		js->roomlist = NULL;
		g_free(err);
		return;
	}

	for(item = xmlnode_get_child(query, "item"); item;
			item = xmlnode_get_next_twin(item)) {
		const char *name;
		GaimRoomlistRoom *room;
		JabberID *jid;

		if(!(jid = jabber_id_new(xmlnode_get_attrib(item, "jid"))))
			continue;
		name = xmlnode_get_attrib(item, "name");


		room = gaim_roomlist_room_new(GAIM_ROOMLIST_ROOMTYPE_ROOM, jid->node, NULL);
		gaim_roomlist_room_add_field(js->roomlist, room, jid->node);
		gaim_roomlist_room_add_field(js->roomlist, room, jid->domain);
		gaim_roomlist_room_add_field(js->roomlist, room, name ? name : "");
		gaim_roomlist_room_add(js->roomlist, room);

		jabber_id_free(jid);
	}
	gaim_roomlist_set_in_progress(js->roomlist, FALSE);
	gaim_roomlist_unref(js->roomlist);
	js->roomlist = NULL;
}

static void roomlist_cancel_cb(JabberStream *js, const char *server) {
	if(js->roomlist) {
		gaim_roomlist_set_in_progress(js->roomlist, FALSE);
		gaim_roomlist_unref(js->roomlist);
		js->roomlist = NULL;
	}
}

static void roomlist_ok_cb(JabberStream *js, const char *server)
{
	JabberIq *iq;

	if(!js->roomlist)
		return;

	if(!server || !*server) {
		gaim_notify_error(js->gc, _("Invalid Server"), _("Invalid Server"), NULL);
		return;
	}

	gaim_roomlist_set_in_progress(js->roomlist, TRUE);

	iq = jabber_iq_new_query(js, JABBER_IQ_GET, "http://jabber.org/protocol/disco#items");

	xmlnode_set_attrib(iq->node, "to", server);

	jabber_iq_set_callback(iq, roomlist_disco_result_cb, NULL);

	jabber_iq_send(iq);
}

char *jabber_roomlist_room_serialize(GaimRoomlistRoom *room)
{

	return g_strdup_printf("%s@%s", (char*)room->fields->data, (char*)room->fields->next->data);
}

GaimRoomlist *jabber_roomlist_get_list(GaimConnection *gc)
{
	JabberStream *js = gc->proto_data;
	GList *fields = NULL;
	GaimRoomlistField *f;

	if(js->roomlist)
		gaim_roomlist_unref(js->roomlist);

	js->roomlist = gaim_roomlist_new(gaim_connection_get_account(js->gc));

	f = gaim_roomlist_field_new(GAIM_ROOMLIST_FIELD_STRING, "", "room", TRUE);
	fields = g_list_append(fields, f);

	f = gaim_roomlist_field_new(GAIM_ROOMLIST_FIELD_STRING, "", "server", TRUE);
	fields = g_list_append(fields, f);

	f = gaim_roomlist_field_new(GAIM_ROOMLIST_FIELD_STRING, _("Description"), "description", FALSE);
	fields = g_list_append(fields, f);

	gaim_roomlist_set_fields(js->roomlist, fields);


	gaim_request_input(gc, _("Enter a Conference Server"), _("Enter a Conference Server"),
			_("Select a conference server to query"),
			js->chat_servers ? js->chat_servers->data : "conference.jabber.org",
			FALSE, FALSE, NULL,
			_("Find Rooms"), GAIM_CALLBACK(roomlist_ok_cb),
			_("Cancel"), GAIM_CALLBACK(roomlist_cancel_cb), js);

	return js->roomlist;
}

void jabber_roomlist_cancel(GaimRoomlist *list)
{
	GaimConnection *gc;
	JabberStream *js;

	gc = gaim_account_get_connection(list->account);
	js = gc->proto_data;

	gaim_roomlist_set_in_progress(list, FALSE);

	if (js->roomlist == list) {
		js->roomlist = NULL;
		gaim_roomlist_unref(list);
	}
}

void jabber_chat_member_free(JabberChatMember *jcm)
{
	g_free(jcm->handle);
	g_free(jcm->jid);
	g_free(jcm);
}

void jabber_chat_track_handle(JabberChat *chat, const char *handle,
		const char *jid, const char *affiliation, const char *role)
{
	JabberChatMember *jcm = g_new0(JabberChatMember, 1);

	jcm->handle = g_strdup(handle);
	jcm->jid = g_strdup(jid);

	g_hash_table_replace(chat->members, jcm->handle, jcm);

	/* XXX: keep track of role and affiliation */
}

void jabber_chat_remove_handle(JabberChat *chat, const char *handle)
{
	g_hash_table_remove(chat->members, handle);
}

gboolean jabber_chat_ban_user(JabberChat *chat, const char *who, const char *why)
{
	JabberIq *iq;
	JabberChatMember *jcm = g_hash_table_lookup(chat->members, who);
	char *to;
	xmlnode *query, *item, *reason;

	if(!jcm || !jcm->jid)
		return FALSE;

	iq = jabber_iq_new_query(chat->js, JABBER_IQ_SET,
			"http://jabber.org/protocol/muc#admin");

	to = g_strdup_printf("%s@%s", chat->room, chat->server);
	xmlnode_set_attrib(iq->node, "to", to);
	g_free(to);

	query = xmlnode_get_child(iq->node, "query");
	item = xmlnode_new_child(query, "item");
	xmlnode_set_attrib(item, "jid", jcm->jid);
	xmlnode_set_attrib(item, "affiliation", "outcast");
	if(why) {
		reason = xmlnode_new_child(item, "reason");
		xmlnode_insert_data(reason, why, -1);
	}

	jabber_iq_send(iq);

	return TRUE;
}

gboolean jabber_chat_affiliate_user(JabberChat *chat, const char *who, const char *affiliation)
{
	char *to;
	JabberIq *iq;
	xmlnode *query, *item;
	JabberChatMember *jcm;

	jcm = g_hash_table_lookup(chat->members, who);

	if (!jcm || !jcm->jid)
		return FALSE;

	iq = jabber_iq_new_query(chat->js, JABBER_IQ_SET,
			"http://jabber.org/protocol/muc#admin");

	to = g_strdup_printf("%s@%s", chat->room, chat->server);
	xmlnode_set_attrib(iq->node, "to", to);
	g_free(to);

	query = xmlnode_get_child(iq->node, "query");
	item = xmlnode_new_child(query, "item");
	xmlnode_set_attrib(item, "jid", jcm->jid);
	xmlnode_set_attrib(item, "affiliation", affiliation);

	jabber_iq_send(iq);

	return TRUE;
}

gboolean jabber_chat_role_user(JabberChat *chat, const char *who, const char *role)
{
	char *to;
	JabberIq *iq;
	xmlnode *query, *item;
	JabberChatMember *jcm;

	jcm = g_hash_table_lookup(chat->members, who);

	if (!jcm || !jcm->handle)
		return FALSE;

	iq = jabber_iq_new_query(chat->js, JABBER_IQ_SET,
	                         "http://jabber.org/protocol/muc#admin");

	to = g_strdup_printf("%s@%s", chat->room, chat->server);
	xmlnode_set_attrib(iq->node, "to", to);
	g_free(to);

	query = xmlnode_get_child(iq->node, "query");
	item = xmlnode_new_child(query, "item");
	xmlnode_set_attrib(item, "nick", jcm->handle);
	xmlnode_set_attrib(item, "role", role);

	jabber_iq_send(iq);

	return TRUE;
}

gboolean jabber_chat_kick_user(JabberChat *chat, const char *who, const char *why)
{
	JabberIq *iq;
	JabberChatMember *jcm = g_hash_table_lookup(chat->members, who);
	char *to;
	xmlnode *query, *item, *reason;

	if(!jcm || !jcm->jid)
		return FALSE;

	iq = jabber_iq_new_query(chat->js, JABBER_IQ_SET,
			"http://jabber.org/protocol/muc#admin");

	to = g_strdup_printf("%s@%s", chat->room, chat->server);
	xmlnode_set_attrib(iq->node, "to", to);
	g_free(to);

	query = xmlnode_get_child(iq->node, "query");
	item = xmlnode_new_child(query, "item");
	xmlnode_set_attrib(item, "jid", jcm->jid);
	xmlnode_set_attrib(item, "role", "none");
	if(why) {
		reason = xmlnode_new_child(item, "reason");
		xmlnode_insert_data(reason, why, -1);
	}

	jabber_iq_send(iq);

	return TRUE;
}

static void jabber_chat_disco_traffic_cb(JabberStream *js, xmlnode *packet, gpointer data)
{
	JabberChat *chat;
	xmlnode *query;
	int id = GPOINTER_TO_INT(data);

	if(!(chat = jabber_chat_find_by_id(js, id)))
		return;

	/* defaults, in case the conference server doesn't
	 * support this request */
	chat->xhtml = TRUE;

	if(xmlnode_get_child(packet, "error")) {
		return;
	}

	if(!(query = xmlnode_get_child(packet, "query")))
		return;

	/* disabling this until more MUC servers support
	 * announcing this
	chat->xhtml = FALSE;

	for(x = xmlnode_get_child(query, "feature"); x; x = xmlnode_get_next_twin(x)) {
		const char *var = xmlnode_get_attrib(x, "var");

		if(var && !strcmp(var, "http://jabber.org/protocol/xhtml-im")) {
			chat->xhtml = TRUE;
		}
	}
	*/
}

void jabber_chat_disco_traffic(JabberChat *chat)
{
	JabberIq *iq;
	xmlnode *query;
	char *room_jid;

	room_jid = g_strdup_printf("%s@%s", chat->room, chat->server);

	iq = jabber_iq_new_query(chat->js, JABBER_IQ_GET,
			"http://jabber.org/protocol/disco#info");

	xmlnode_set_attrib(iq->node, "to", room_jid); 

	query = xmlnode_get_child(iq->node, "query");

	xmlnode_set_attrib(query, "node", "http://jabber.org/protocol/muc#traffic");

	jabber_iq_set_callback(iq, jabber_chat_disco_traffic_cb, GINT_TO_POINTER(chat->id));

	jabber_iq_send(iq);

	g_free(room_jid);
}



