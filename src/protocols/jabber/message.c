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
#include "notify.h"
#include "server.h"
#include "util.h"

#include "buddy.h"
#include "chat.h"
#include "message.h"
#include "xmlnode.h"

#define JABBER_TYPING_NOTIFY_INT 15

void jabber_message_free(JabberMessage *jm)
{
	if(jm->from)
		g_free(jm->from);
	if(jm->to)
		g_free(jm->to);
	if(jm->subject)
		g_free(jm->subject);
	if(jm->body)
		g_free(jm->body);
	if(jm->xhtml)
		g_free(jm->xhtml);
	if(jm->password)
		g_free(jm->password);

	g_free(jm);
}

void handle_chat(JabberMessage *jm)
{
	JabberID *jid = jabber_id_new(jm->from);
	char *from;

	JabberBuddy *jb;
	JabberBuddyResource *jbr;

	jb = jabber_buddy_find(jm->js, jm->from, TRUE);
	jbr = jabber_buddy_find_resource(jb, jabber_get_resource(jm->from));

	if(gaim_find_conversation_with_account(jm->from, jm->js->gc->account))
		from = g_strdup(jm->from);
	else if(jid->node)
		from = g_strdup_printf("%s@%s", jid->node, jid->domain);
	else
		from = g_strdup(jid->domain);

	if(!jm->xhtml && !jm->body) {
		if(jm->events & JABBER_MESSAGE_EVENT_COMPOSING)
			serv_got_typing(jm->js->gc, from, 0, GAIM_TYPING);
		else
			serv_got_typing_stopped(jm->js->gc, from);
	} else {
		if(jbr && jm->events & JABBER_MESSAGE_EVENT_COMPOSING)
			jbr->capabilities |= JABBER_CAP_COMPOSING;
		serv_got_im(jm->js->gc, from, jm->xhtml ? jm->xhtml : jm->body, 0,
				jm->sent);
	}

	g_free(from);
	jabber_id_free(jid);
}

void handle_groupchat(JabberMessage *jm)
{
	JabberID *jid = jabber_id_new(jm->from);
	JabberChat *chat = jabber_chat_find(jm->js, jid->node, jid->domain);

	if(!chat)
		return;

	if(jm->subject)
		gaim_chat_set_topic(GAIM_CHAT(chat->conv), jid->resource, jm->subject);

	serv_got_chat_in(jm->js->gc, chat->id, jabber_get_resource(jm->from),
			0, jm->xhtml ? jm->xhtml : jm->body, jm->sent);
	jabber_id_free(jid);
}

void handle_groupchat_invite(JabberMessage *jm)
{
	GHashTable *components = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, g_free);
	JabberID *jid = jabber_id_new(jm->to);

	g_hash_table_replace(components, g_strdup("room"), jid->node);
	g_hash_table_replace(components, g_strdup("server"), jid->node);
	g_hash_table_replace(components, g_strdup("handle"), jm->js->user->node);
	g_hash_table_replace(components, g_strdup("password"), jm->password);

	jabber_id_free(jid);
	serv_got_chat_invite(jm->js->gc, jm->to, jm->from, jm->body, components);
}

void handle_error(JabberMessage *jm)
{
	char *buf;

	if(!jm->body)
		return;

	buf = g_strdup_printf(_("Message delivery to %s failed: %s"),
			jm->from, jm->error);

	gaim_notify_error(jm->js->gc, _("Jabber Message Error"), buf, jm->body);

	g_free(buf);
}

void jabber_message_parse(JabberStream *js, xmlnode *packet)
{
	JabberMessage *jm;
	const char *type;
	xmlnode *child;

	if(strcmp(packet->name, "message"))
		return;

	jm = g_new0(JabberMessage, 1);
	jm->js = js;
	jm->sent = time(NULL);

	type = xmlnode_get_attrib(packet, "type");

	if(type) {
		if(!strcmp(type, "normal"))
			jm->type = JABBER_MESSAGE_NORMAL;
		else if(!strcmp(type, "chat"))
			jm->type = JABBER_MESSAGE_CHAT;
		else if(!strcmp(type, "groupchat"))
			jm->type = JABBER_MESSAGE_GROUPCHAT;
		else if(!strcmp(type, "headline"))
			jm->type = JABBER_MESSAGE_HEADLINE;
		else if(!strcmp(type, "error"))
			jm->type = JABBER_MESSAGE_ERROR;
		else
			jm->type = JABBER_MESSAGE_OTHER;
	} else {
		jm->type = JABBER_MESSAGE_NORMAL;
	}

	jm->from = g_strdup(xmlnode_get_attrib(packet, "from"));
	jm->to = g_strdup(xmlnode_get_attrib(packet, "to"));

	for(child = packet->child; child; child = child->next) {
		if(child->type != NODE_TYPE_TAG)
			continue;

		if(!strcmp(child->name, "subject")) {
			if(!jm->subject)
				jm->subject = xmlnode_get_data(child);
		} else if(!strcmp(child->name, "body")) {
			if(!jm->body)
				jm->body = xmlnode_get_data(child);
		} else if(!strcmp(child->name, "html") && child->child) {
			/* check to see if the <html> actually contains anything,
			 * otherwise we'll ignore it */
			char *txt = xmlnode_get_data(child);
			if(!jm->xhtml && txt)
				jm->xhtml = xmlnode_to_str(child);
			g_free(txt);
		} else if(!strcmp(child->name, "error")) {
			const char *code = xmlnode_get_attrib(child, "code");
			char *code_txt = NULL;
			char *text = xmlnode_get_data(child);

			if(code)
				code_txt = g_strdup_printf(_(" (Code %s)"), code);

			if(!jm->error)
				jm->error = g_strdup_printf("%s%s", text ? text : "",
						code_txt ? code_txt : "");

			g_free(code_txt);
			g_free(text);
		} else if(!strcmp(child->name, "x")) {
			const char *xmlns = xmlnode_get_attrib(child, "xmlns");
			if(xmlns && !strcmp(xmlns, "jabber:x:event")) {
				if(xmlnode_get_child(child, "composing"))
					jm->events |= JABBER_MESSAGE_EVENT_COMPOSING;
			} else if(xmlns && !strcmp(xmlns, "jabber:x:delay")) {
				const char *timestamp = xmlnode_get_attrib(child, "stamp");
				if(timestamp)
					jm->sent = str_to_time(timestamp);
			} else if(xmlns && !strcmp(xmlns, "jabber:x:conference") &&
					jm->type != JABBER_MESSAGE_GROUPCHAT_INVITE) {
				const char *jid = xmlnode_get_attrib(child, "jid");
				if(jid) {
					jm->type = JABBER_MESSAGE_GROUPCHAT_INVITE;
					g_free(jm->to);
					jm->to = g_strdup(jid);
				}
			} else if(xmlns && !strcmp(xmlns,
						"http://jabber.org/protocol/muc#user")) {
				xmlnode *invite = xmlnode_get_child(child, "invite");
				if(invite) {
					xmlnode *reason, *password;
					const char *jid = xmlnode_get_attrib(child, "from");
					g_free(jm->to);
					jm->to = jm->from;
					jm->from = g_strdup(jid);
					if((reason = xmlnode_get_child(invite, "reason"))) {
						g_free(jm->body);
						jm->body = xmlnode_get_data(reason);
					}
					if((password = xmlnode_get_child(invite, "password")))
						jm->password = xmlnode_get_data(password);

					jm->type = JABBER_MESSAGE_GROUPCHAT_INVITE;
				}
			}
		}
	}

	switch(jm->type) {
		case JABBER_MESSAGE_NORMAL:
		case JABBER_MESSAGE_CHAT:
		case JABBER_MESSAGE_HEADLINE:
			handle_chat(jm);
			break;
		case JABBER_MESSAGE_GROUPCHAT:
			handle_groupchat(jm);
			break;
		case JABBER_MESSAGE_GROUPCHAT_INVITE:
			handle_groupchat_invite(jm);
			break;
		case JABBER_MESSAGE_ERROR:
			handle_error(jm);
			break;
		case JABBER_MESSAGE_OTHER:
			gaim_debug(GAIM_DEBUG_INFO, "jabber",
					"Received message of unknown type: %s\n", type);
			break;
	}
	jabber_message_free(jm);
}

void jabber_message_send(JabberMessage *jm)
{
	xmlnode *message, *child;
	const char *type = NULL;

	message = xmlnode_new("message");

	switch(jm->type) {
		case JABBER_MESSAGE_NORMAL:
			type = "normal";
			break;
		case JABBER_MESSAGE_CHAT:
		case JABBER_MESSAGE_GROUPCHAT_INVITE:
			type = "chat";
			break;
		case JABBER_MESSAGE_HEADLINE:
			type = "headline";
			break;
		case JABBER_MESSAGE_GROUPCHAT:
			type = "groupchat";
			break;
		case JABBER_MESSAGE_ERROR:
			type = "error";
			break;
		case JABBER_MESSAGE_OTHER:
			type = NULL;
			break;
	}

	if(type)
		xmlnode_set_attrib(message, "type", type);

	xmlnode_set_attrib(message, "to", jm->to);

	if(jm->events || (!jm->body && !jm->xhtml)) {
		child = xmlnode_new_child(message, "x");
		xmlnode_set_attrib(child, "xmlns", "jabber:x:event");
		if(jm->events & JABBER_MESSAGE_EVENT_COMPOSING)
			xmlnode_new_child(child, "composing");
	}

	if(jm->subject) {
		child = xmlnode_new_child(message, "subject");
		xmlnode_insert_data(child, jm->subject, -1);
	}

	if(jm->body) {
		child = xmlnode_new_child(message, "body");
		xmlnode_insert_data(child, jm->body, -1);
	}

	if(jm->xhtml) {
		child = xmlnode_from_str(jm->xhtml, -1);
		if(child) {
			xmlnode_insert_child(message, child);
		} else {
			gaim_debug(GAIM_DEBUG_ERROR, "jabber",
					"XHTML translation/validation failed, returning: %s\n",
					jm->xhtml);
		}
	}

	jabber_send(jm->js, message);

	xmlnode_free(message);
}

int jabber_message_send_im(GaimConnection *gc, const char *who, const char *msg,
		GaimImFlags flags)
{
	JabberMessage *jm;
	JabberBuddy *jb;
	JabberBuddyResource *jbr;
	char *buf;
	char *xhtml, *plain;

	if(!who || !msg)
		return 0;

	jb = jabber_buddy_find(gc->proto_data, who, TRUE);
	jbr = jabber_buddy_find_resource(jb, jabber_get_resource(who));

	jm = g_new0(JabberMessage, 1);
	jm->js = gc->proto_data;
	jm->type = JABBER_MESSAGE_CHAT;
	jm->events = JABBER_MESSAGE_EVENT_COMPOSING;
	jm->to = g_strdup(who);

	buf = g_strdup_printf("<html xmlns='http://jabber.org/protocol/xhtml-im'><body>%s</body></html>", msg);

	gaim_markup_html_to_xhtml(buf, &xhtml, &plain);
	g_free(buf);

	jm->body = plain;
	if(!jbr || jbr->capabilities & JABBER_CAP_XHTML)
		jm->xhtml = xhtml;
	else
		g_free(xhtml);

	jabber_message_send(jm);
	jabber_message_free(jm);
	return 1;
}

int jabber_message_send_chat(GaimConnection *gc, int id, const char *message)
{
	JabberChat *chat;
	JabberMessage *jm;
	JabberStream *js = gc->proto_data;

	if(!message)
		return 0;

	chat = jabber_chat_find_by_id(js, id);

	jm = g_new0(JabberMessage, 1);
	jm->js = gc->proto_data;
	jm->type = JABBER_MESSAGE_CHAT;
	jm->to = g_strdup_printf("%s@%s", chat->room, chat->server);

	gaim_markup_html_to_xhtml(message, NULL, &jm->body);

	jabber_message_send(jm);
	jabber_message_free(jm);
	return 1;
}

int jabber_send_typing(GaimConnection *gc, const char *who, int typing)
{
	JabberMessage *jm;
	JabberBuddy *jb;
	JabberBuddyResource *jbr;

	jb = jabber_buddy_find(gc->proto_data, who, TRUE);
	jbr = jabber_buddy_find_resource(jb, jabber_get_resource(who));

	if(jbr && !(jbr->capabilities & JABBER_CAP_COMPOSING))
		return 0;

	jm = g_new0(JabberMessage, 1);
	jm->js = gc->proto_data;
	jm->type = JABBER_MESSAGE_CHAT;
	jm->to = g_strdup(who);

	if(typing == GAIM_TYPING)
		jm->events = JABBER_MESSAGE_EVENT_COMPOSING;

	jabber_message_send(jm);
	jabber_message_free(jm);

	return JABBER_TYPING_NOTIFY_INT;
}

