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
#include "request.h"
#include "server.h"
#include "util.h"

#include "buddy.h"
#include "chat.h"
#include "presence.h"
#include "iq.h"
#include "jutil.h"
#include "xmlnode.h"


static void chats_send_presence_foreach(gpointer key, gpointer val,
		gpointer user_data)
{
	JabberChat *chat = val;
	xmlnode *presence = user_data;
	char *chat_jid = key;

	xmlnode_set_attrib(presence, "to", chat_jid);
	jabber_send(chat->js, presence);
}


void jabber_presence_send(GaimConnection *gc, const char *state,
		const char *msg)
{
	JabberStream *js = gc->proto_data;
	xmlnode *presence;
	char *stripped = NULL;

	if(msg) {
		gaim_markup_html_to_xhtml(msg, NULL, &stripped);
	} else if(!state || strcmp(state, GAIM_AWAY_CUSTOM)) {
		/* i can't wait until someone re-writes the status/away stuff */
		stripped = g_strdup("");
	}

	if(gc->away)
		g_free(gc->away);
	gc->away = stripped;

	presence = jabber_presence_create(state, msg);
	jabber_send(js, presence);
	g_hash_table_foreach(js->chats, chats_send_presence_foreach, presence);
	xmlnode_free(presence);
}

xmlnode *jabber_presence_create(const char *state, const char *msg)
{
	xmlnode *show, *status, *presence;


	presence = xmlnode_new("presence");

	if(state) {
		const char *show_string = NULL;
		if(!strcmp(state, _("Chatty")))
			show_string = "chat";
		else if(!strcmp(state, _("Away")) ||
				(msg && !strcmp(state, GAIM_AWAY_CUSTOM)))
			show_string = "away";
		else if(!strcmp(state, _("Extended Away")))
			show_string = "xa";
		else if(!strcmp(state, _("Do Not Disturb")))
			show_string = "dnd";
		else if(!strcmp(state, _("Invisible"))) {
			xmlnode_set_attrib(presence, "type", "invisible");
		}

		if(show_string) {
			show = xmlnode_new_child(presence, "show");
			xmlnode_insert_data(show, show_string, -1);
		}
	}

	if(msg && *msg) {
		status = xmlnode_new_child(presence, "status");
		xmlnode_insert_data(status, msg, -1);
	}

	return presence;
}

struct _jabber_add_permit {
	GaimConnection *gc;
	char *who;
};

static void authorize_add_cb(struct _jabber_add_permit *jap)
{
	if(g_list_find(gaim_connections_get_all(), jap->gc)) {
		jabber_presence_subscription_set(jap->gc->proto_data, jap->who,
				"subscribed");

		if(!gaim_find_buddy(jap->gc->account, jap->who))
			gaim_account_notify_added(jap->gc->account, NULL, jap->who, NULL, NULL);
	}

	g_free(jap->who);
	g_free(jap);
}

static void deny_add_cb(struct _jabber_add_permit *jap)
{
	if(g_list_find(gaim_connections_get_all(), jap->gc)) {
		jabber_presence_subscription_set(jap->gc->proto_data, jap->who,
				"unsubscribed");
	}

	g_free(jap->who);
	g_free(jap);
}

void jabber_presence_parse(JabberStream *js, xmlnode *packet)
{
	const char *from = xmlnode_get_attrib(packet, "from");
	const char *type = xmlnode_get_attrib(packet, "type");
	char *status = NULL;
	int priority = 0;
	JabberID *jid;
	JabberChat *chat;
	JabberBuddy *jb;
	JabberBuddyResource *jbr;
	GaimBuddy *b;
	char *buddy_name;
	int state = 0;
	xmlnode *y;
	gboolean muc = FALSE;

	jb = jabber_buddy_find(js, from, TRUE);

	if(jb->error_msg) {
		g_free(jb->error_msg);
		jb->error_msg = NULL;
	}

	if(type && !strcasecmp(type, "error")) {
		state = JABBER_STATE_ERROR;
		if((y = xmlnode_get_child(packet, "error")) != NULL) {
			/* XXX: need to handle new XMPP-style errors */
			char *txt = xmlnode_get_data(y);
			jb->error_msg = g_strdup_printf(_("%s (Code %s)"),
					txt ? txt : "Error", xmlnode_get_attrib(y, "code"));
			if(txt)
				g_free(txt);
		} else {
			jb->error_msg = g_strdup(_("Unknown Error in presence"));
		}
	} else if(type && !strcasecmp(type, "subscribe")) {
		struct _jabber_add_permit *jap = g_new0(struct _jabber_add_permit, 1);
		char *msg = g_strdup_printf(_("The user %s wants to add you to their buddy list."), from);
		jap->gc = js->gc;
		jap->who = g_strdup(from);

		gaim_request_action(js->gc, NULL, msg, NULL, 0, jap, 2,
				_("Authorize"), G_CALLBACK(authorize_add_cb),
				_("Deny"), G_CALLBACK(deny_add_cb));
		g_free(msg);
		return;
	} else if(type && (!strcmp(type, "subscribed") ||
				!strcmp(type, "unsubscribed"))) {
		/* we've been allowed to see their presence, but we don't care */
		return;
	} else {
		if((y = xmlnode_get_child(packet, "show"))) {
			char *show = xmlnode_get_data(y);
			if(!show) {
				state = 0;
			} else if(!strcasecmp(show, "away")) {
				state = JABBER_STATE_AWAY;
			} else if(!strcasecmp(show, "chat")) {
				state = JABBER_STATE_CHAT;
			} else if(!strcasecmp(show, "xa")) {
				state = JABBER_STATE_XA;
			} else if(!strcasecmp(show, "dnd")) {
				state = JABBER_STATE_DND;
			}
			g_free(show);
		} else {
			state = 0;
		}
	}

	for(y = packet->child; y; y = y->next) {
		if(y->type != NODE_TYPE_TAG)
			continue;

		if(!strcmp(y->name, "status")) {
			status = xmlnode_get_data(y);
		} else if(!strcmp(y->name, "priority")) {
			char *p = xmlnode_get_data(y);
			if(p) {
				priority = atoi(p);
				g_free(p);
			}
		} else if(!strcmp(y->name, "x")) {
			const char *xmlns = xmlnode_get_attrib(y, "xmlns");
			if(xmlns && !strcmp(xmlns, "http://jabber.org/protocol/muc#user")) {
				/* this is where we'd normally get the "op" status of the
				 * user, but since we don't have a good way to show that yet
				 * we'll ignore it */
				muc = TRUE;
			}
		}
	}

	jid = jabber_id_new(from);

	if((chat = jabber_chat_find(js, jid->node, jid->domain))) {
		static int i = 0;
		char *room_jid = g_strdup_printf("%s@%s", jid->node, jid->domain);

		if(state == JABBER_STATE_ERROR) {
			const char *code = NULL;
			char *text = NULL;
			char *buf;
			xmlnode *error = xmlnode_get_child(packet, "error");
			if(error) {
				/* I should make my own messages so they can be
				 * translated, but i'm tired */
				code = xmlnode_get_attrib(error, "code");
				text = xmlnode_get_data(error);
			}

			if(!code)
				code = "";
			if(!text)
				text = g_strdup(_("Unable to join chat"));

			buf = g_strdup_printf("Error %s joining chat %s: %s",
					code, from, text);
			gaim_notify_error(js->gc, _("Error"), _("Error"), buf);
			g_free(text);
			g_free(buf);

			jabber_chat_destroy(chat);
			return;
		}


		if(!chat->conv) {
			chat->id = i++;
			chat->muc = muc;
			chat->conv = serv_got_joined_chat(js->gc, chat->id, room_jid);
		}

		if(type && !strcasecmp(type, "unavailable")) {
			if(!strcmp(jid->resource, chat->nick)) {
				serv_got_chat_left(js->gc, chat->id);
				jabber_chat_destroy(chat);
			} else {
				gaim_conv_chat_remove_user(GAIM_CONV_CHAT(chat->conv), jid->resource,
						NULL);
			}
		} else {
			if(!jabber_chat_find_buddy(chat->conv, jid->resource))
				gaim_conv_chat_add_user(GAIM_CONV_CHAT(chat->conv), jid->resource,
						NULL);
		}
		g_free(room_jid);
	} else {
		if(state != JABBER_STATE_ERROR && !(jb->subscription & JABBER_SUB_TO)) {
			gaim_debug(GAIM_DEBUG_INFO, "jabber",
					"got unexpected presence from %s, ignoring\n", from);
			jabber_id_free(jid);
			return;
		}

		buddy_name = g_strdup_printf("%s@%s", jid->node, jid->domain);
		if((b = gaim_find_buddy(js->gc->account, buddy_name)) == NULL) {
			jabber_id_free(jid);
			g_free(buddy_name);
			return;
		}

		if(state == JABBER_STATE_ERROR ||
				(type && !strcasecmp(type, "unavailable")))
			jabber_buddy_remove_resource(jb, jid->resource);
		else
			jabber_buddy_track_resource(jb, jid->resource, priority, state,
					status);

		jbr = jabber_buddy_find_resource(jb, jid->resource);

		if(jbr)
			serv_got_update(js->gc, buddy_name, 1, 0, b->signon, b->idle,
					jbr->state);
		else
			serv_got_update(js->gc, buddy_name, 0, 0, 0, 0, 0);

		g_free(buddy_name);
	}
	g_free(status);
	jabber_id_free(jid);
}

void jabber_presence_subscription_set(JabberStream *js, const char *who, const char *type)
{
	xmlnode *presence = xmlnode_new("presence");

	xmlnode_set_attrib(presence, "to", who);
	xmlnode_set_attrib(presence, "type", type);

	jabber_send(js, presence);
	xmlnode_free(presence);
}
