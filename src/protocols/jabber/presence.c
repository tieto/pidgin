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
#include "status.h"
#include "util.h"

#include "buddy.h"
#include "chat.h"
#include "presence.h"
#include "iq.h"
#include "jutil.h"
#include "sha.h"
#include "xmlnode.h"


static void chats_send_presence_foreach(gpointer key, gpointer val,
		gpointer user_data)
{
	JabberChat *chat = val;
	xmlnode *presence = user_data;
	char *chat_full_jid;

	if(!chat->conv)
		return;

	chat_full_jid = g_strdup_printf("%s@%s/%s", chat->room, chat->server,
			chat->handle);

	xmlnode_set_attrib(presence, "to", chat_full_jid);
	jabber_send(chat->js, presence);
	g_free(chat_full_jid);
}

void jabber_presence_fake_to_self(JabberStream *js, const GaimStatus *gstatus) {
	char *my_base_jid;

	if(!js->user)
		return;

	my_base_jid = g_strdup_printf("%s@%s", js->user->node, js->user->domain);
	if(gaim_find_buddy(js->gc->account, my_base_jid)) {
		JabberBuddy *jb;
		JabberBuddyResource *jbr;
		if((jb = jabber_buddy_find(js, my_base_jid, TRUE))) {
			JabberBuddyState state;
			const char *msg;
			int priority;

			gaim_status_to_jabber(gstatus, &state, &msg, &priority);

			if (state == JABBER_BUDDY_STATE_UNAVAILABLE) {
				jabber_buddy_remove_resource(jb, js->user->resource);
			} else {
				jabber_buddy_track_resource(jb, js->user->resource, priority, state, msg);
			}
			if((jbr = jabber_buddy_find_resource(jb, NULL))) {
				gaim_prpl_got_user_status(js->gc->account, my_base_jid, jabber_buddy_state_get_status_id(jbr->state), "priority", jbr->priority, jbr->status ? "message" : NULL, jbr->status, NULL);
			} else {
				gaim_prpl_got_user_status(js->gc->account, my_base_jid, "offline", msg ? "message" : NULL, msg, NULL);
			}
		}
	}
	g_free(my_base_jid);
}


void jabber_presence_send(GaimAccount *account, GaimStatus *status)
{
	GaimConnection *gc = NULL;
	JabberStream *js = NULL;
	xmlnode *presence, *x, *photo;
	char *stripped = NULL;
	const char *msg;
	JabberBuddyState state;
	int priority;

	if(!gaim_status_is_active(status))
		return;

	if(!account) return ;
	gc = account->gc;

	if(!gc) return ;
	js= gc->proto_data;

	gaim_status_to_jabber(status, &state, &msg, &priority);

	if(msg)
		gaim_markup_html_to_xhtml(msg, NULL, &stripped);

	presence = jabber_presence_create(state, stripped, priority);
	g_free(stripped);

	jabber_send(js, presence);

	if(js->avatar_hash) {
		x = xmlnode_new_child(presence, "x");
		xmlnode_set_attrib(x, "xmlns", "vcard-temp:x:update");
		photo = xmlnode_new_child(x, "photo");
		xmlnode_insert_data(photo, js->avatar_hash, -1);
	}

	g_hash_table_foreach(js->chats, chats_send_presence_foreach, presence);
	xmlnode_free(presence);

	jabber_presence_fake_to_self(js, status);

}

xmlnode *jabber_presence_create(JabberBuddyState state, const char *msg, int priority)
{
	xmlnode *show, *status, *presence;
	const char *show_string = NULL;


	presence = xmlnode_new("presence");


	if(state == JABBER_BUDDY_STATE_UNAVAILABLE)
		xmlnode_set_attrib(presence, "type", "unavailable");
	else if(state != JABBER_BUDDY_STATE_ONLINE &&
			state != JABBER_BUDDY_STATE_UNKNOWN &&
			state != JABBER_BUDDY_STATE_ERROR)
		show_string = jabber_buddy_state_get_status_id(state);

	if(show_string) {
		show = xmlnode_new_child(presence, "show");
		xmlnode_insert_data(show, show_string, -1);
	}

	if(msg) {
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

static void jabber_vcard_parse_avatar(JabberStream *js, xmlnode *packet, gpointer blah)
{
	GaimBuddy *b = NULL;
	xmlnode *vcard, *photo;
	char *text, *data;
	int size;
	const char *from = xmlnode_get_attrib(packet, "from");

	if(!from)
		return;

	if((vcard = xmlnode_get_child(packet, "vCard")) ||
			(vcard = xmlnode_get_child_with_namespace(packet, "query", "vcard-temp"))) {
		if((photo = xmlnode_get_child(vcard, "PHOTO"))) {
			if((text = xmlnode_get_data(photo))) {
				gaim_base64_decode(text, &data, &size);

				gaim_buddy_icons_set_for_user(js->gc->account, from, data, size);
				if((b = gaim_find_buddy(js->gc->account, from))) {
					unsigned char hashval[20];
					char hash[41], *p;
					int i;

					shaBlock((unsigned char *)data, size, hashval);
					p = hash;
					for(i=0; i<20; i++, p+=2)
						snprintf(p, 3, "%02x", hashval[i]);
					gaim_blist_node_set_string((GaimBlistNode*)b, "avatar_hash", hash);
				}
			}
		}
	}
}

void jabber_presence_parse(JabberStream *js, xmlnode *packet)
{
	const char *from = xmlnode_get_attrib(packet, "from");
	const char *type = xmlnode_get_attrib(packet, "type");
	const char *real_jid = NULL;
	const char *affiliation = NULL;
	const char *role = NULL;
	char *status = NULL;
	int priority = 0;
	JabberID *jid;
	JabberChat *chat;
	JabberBuddy *jb;
	JabberBuddyResource *jbr = NULL, *found_jbr = NULL;
	GaimConvChatBuddyFlags flags = GAIM_CBFLAGS_NONE;
	gboolean delayed = FALSE;
	GaimBuddy *b = NULL;
	char *buddy_name;
	JabberBuddyState state = JABBER_BUDDY_STATE_UNKNOWN;
	xmlnode *y;
	gboolean muc = FALSE;
	char *avatar_hash = NULL;


	if(!(jb = jabber_buddy_find(js, from, TRUE)))
		return;

	if(!(jid = jabber_id_new(from)))
		return;

	if(jb->error_msg) {
		g_free(jb->error_msg);
		jb->error_msg = NULL;
	}

	if(type && !strcmp(type, "error")) {
		char *msg = jabber_parse_error(js, packet);

		state = JABBER_BUDDY_STATE_ERROR;
		jb->error_msg = msg ? msg : g_strdup(_("Unknown Error in presence"));
	} else if(type && !strcmp(type, "subscribe")) {
		struct _jabber_add_permit *jap = g_new0(struct _jabber_add_permit, 1);
		char *msg = g_strdup_printf(_("The user %s wants to add you to their buddy list."), from);
		jap->gc = js->gc;
		jap->who = g_strdup(from);

		gaim_request_action(js->gc, NULL, msg, NULL, GAIM_DEFAULT_ACTION_NONE,
				jap, 2,
				_("Authorize"), G_CALLBACK(authorize_add_cb),
				_("Deny"), G_CALLBACK(deny_add_cb));
		g_free(msg);
		jabber_id_free(jid);
		return;
	} else if(type && !strcmp(type, "subscribed")) {
		/* we've been allowed to see their presence, but we don't care */
		jabber_id_free(jid);
		return;
	} else {
		if((y = xmlnode_get_child(packet, "show"))) {
			char *show = xmlnode_get_data(y);
			state = jabber_buddy_status_id_get_state(show);
			g_free(show);
		} else {
			state = JABBER_BUDDY_STATE_ONLINE;
		}
	}


	for(y = packet->child; y; y = y->next) {
		if(y->type != XMLNODE_TYPE_TAG)
			continue;

		if(!strcmp(y->name, "status")) {
			g_free(status);
			status = xmlnode_get_data(y);
		} else if(!strcmp(y->name, "priority")) {
			char *p = xmlnode_get_data(y);
			if(p) {
				priority = atoi(p);
				g_free(p);
			}
		} else if(!strcmp(y->name, "x")) {
			const char *xmlns = xmlnode_get_attrib(y, "xmlns");
			if(xmlns && !strcmp(xmlns, "jabber:x:delay")) {
				/* XXX: compare the time.  jabber:x:delay can happen on presence packets that aren't really and truly delayed */
				delayed = TRUE;
			} else if(xmlns && !strcmp(xmlns, "http://jabber.org/protocol/muc#user")) {
				xmlnode *z;

				muc = TRUE;
				if((z = xmlnode_get_child(y, "status"))) {
					const char *code = xmlnode_get_attrib(z, "code");
					if(code && !strcmp(code, "201")) {
						chat = jabber_chat_find(js, jid->node, jid->domain);
						chat->config_dialog_type = GAIM_REQUEST_ACTION;
						chat->config_dialog_handle =
							gaim_request_action(js->gc, _("Create New Room"),
								_("Create New Room"),
								_("You are creating a new room.  Would you like to "
									"configure it, or accept the default settings?"),
								1, chat, 2, _("Configure Room"),
								G_CALLBACK(jabber_chat_request_room_configure),
								_("Accept Defaults"),
								G_CALLBACK(jabber_chat_create_instant_room));
					}
				}
				if((z = xmlnode_get_child(y, "item"))) {
					real_jid = xmlnode_get_attrib(z, "jid");
					affiliation = xmlnode_get_attrib(z, "affiliation");
					role = xmlnode_get_attrib(z, "role");
					if(affiliation != NULL && !strcmp(affiliation, "owner"))
						flags |= GAIM_CBFLAGS_FOUNDER;
					if (role != NULL) {
						if (!strcmp(role, "moderator"))
							flags |= GAIM_CBFLAGS_OP;
						else if (!strcmp(role, "participant"))
							flags |= GAIM_CBFLAGS_VOICE;
					}
				}
			} else if(xmlns && !strcmp(xmlns, "vcard-temp:x:update")) {
				xmlnode *photo = xmlnode_get_child(y, "photo");
				if(photo) {
					if(avatar_hash)
						g_free(avatar_hash);
					avatar_hash = xmlnode_get_data(photo);
				}
			}
		}
	}


	if(jid->node && (chat = jabber_chat_find(js, jid->node, jid->domain))) {
		static int i = 1;
		char *room_jid = g_strdup_printf("%s@%s", jid->node, jid->domain);

		if(state == JABBER_BUDDY_STATE_ERROR) {
			char *title, *msg = jabber_parse_error(js, packet);

			if(chat->conv) {
				title = g_strdup_printf(_("Error in chat %s"), from);
				serv_got_chat_left(js->gc, chat->id);
			} else {
				title = g_strdup_printf(_("Error joining chat %s"), from);
			}
			gaim_notify_error(js->gc, title, title, msg);
			g_free(title);
			g_free(msg);

			jabber_chat_destroy(chat);
			jabber_id_free(jid);
			g_free(status);
			g_free(room_jid);
			if(avatar_hash)
				g_free(avatar_hash);
			return;
		}


		if(type && !strcmp(type, "unavailable")) {
			gboolean nick_change = FALSE;

			/* If we haven't joined the chat yet, we don't care that someone
			 * left, or it was us leaving after we closed the chat */
			if(!chat->conv) {
				if(!strcmp(jid->resource, chat->handle))
					jabber_chat_destroy(chat);
				jabber_id_free(jid);
				g_free(status);
				g_free(room_jid);
				if(avatar_hash)
					g_free(avatar_hash);
				return;
			}

			jabber_buddy_remove_resource(jb, jid->resource);
			if(chat->muc) {
				xmlnode *x;
				for(x = xmlnode_get_child(packet, "x"); x; x = xmlnode_get_next_twin(x)) {
					const char *xmlns, *nick, *code;
					xmlnode *stat, *item;
					if(!(xmlns = xmlnode_get_attrib(x, "xmlns")) ||
							strcmp(xmlns, "http://jabber.org/protocol/muc#user"))
						continue;
					if(!(stat = xmlnode_get_child(x, "status")))
						continue;
					if(!(code = xmlnode_get_attrib(stat, "code")))
						continue;
					if(!strcmp(code, "301")) {
						/* XXX: we got banned */
					} else if(!strcmp(code, "303")) {
						if(!(item = xmlnode_get_child(x, "item")))
							continue;
						if(!(nick = xmlnode_get_attrib(item, "nick")))
							continue;
						nick_change = TRUE;
						if(!strcmp(jid->resource, chat->handle)) {
							g_free(chat->handle);
							chat->handle = g_strdup(nick);
						}
						gaim_conv_chat_rename_user(GAIM_CONV_CHAT(chat->conv), jid->resource, nick);
						jabber_chat_remove_handle(chat, jid->resource);
						break;
					} else if(!strcmp(code, "307")) {
						/* XXX: we got kicked */
					} else if(!strcmp(code, "321")) {
						/* XXX: removed due to an affiliation change */
					} else if(!strcmp(code, "322")) {
						/* XXX: removed because room is now members-only */
					} else if(!strcmp(code, "332")) {
						/* XXX: removed due to system shutdown */
					}
				}
			}
			if(!nick_change) {
				if(!g_utf8_collate(jid->resource, chat->handle)) {
					serv_got_chat_left(js->gc, chat->id);
					jabber_chat_destroy(chat);
				} else {
					gaim_conv_chat_remove_user(GAIM_CONV_CHAT(chat->conv), jid->resource,
							status);
					jabber_chat_remove_handle(chat, jid->resource);
				}
			}
		} else {
			if(!chat->conv) {
				chat->id = i++;
				chat->muc = muc;
				chat->conv = serv_got_joined_chat(js->gc, chat->id, room_jid);
				g_free(chat->handle);
				chat->handle = g_strdup(jid->resource);
				gaim_conv_chat_set_nick(GAIM_CONV_CHAT(chat->conv), jid->resource);

				/* <iq to='room@server'
				   type='get'>
				   <query xmlns='http://jabber.org/protocol/disco#info'
				   node='http://jabber.org/protocol/muc#traffic'/>
				   </iq>
				   */
				/* expected response format:
				   <iq from='room@server'
				   type='get'>
				   <query xmlns='http://jabber.org/protocol/disco#info'
				   node='http://jabber.org/protocol/muc#traffic'>
				   <feature var='http://jabber.org/protocol/xhtml-im'/>
				   <feature var='jabber:x:roster/'/>
				   </query>
				   </iq>
				   */
				/*
				 * XXX: i'm not sure if we turn off XHTML unless we get
				 * xhtml back in this, or if we turn it off only if we
				 * get a response, and it's not there.  Ask stpeter to
				 * clarify.
				 */
			}

			jabber_buddy_track_resource(jb, jid->resource, priority, state,
					status);

			jabber_chat_track_handle(chat, jid->resource, real_jid, affiliation, role);

			if(!jabber_chat_find_buddy(chat->conv, jid->resource))
				gaim_conv_chat_add_user(GAIM_CONV_CHAT(chat->conv), jid->resource,
						real_jid, flags, !delayed);
			else
				gaim_conv_chat_user_set_flags(GAIM_CONV_CHAT(chat->conv), jid->resource,
						flags);
		}
		g_free(room_jid);
	} else {
		if(state != JABBER_BUDDY_STATE_ERROR && !(jb->subscription & JABBER_SUB_TO || jb->subscription & JABBER_SUB_PENDING)) {
			gaim_debug(GAIM_DEBUG_INFO, "jabber",
					"got unexpected presence from %s, ignoring\n", from);
			jabber_id_free(jid);
			g_free(status);
			if(avatar_hash)
				g_free(avatar_hash);
			return;
		}

		buddy_name = g_strdup_printf("%s%s%s", jid->node ? jid->node : "",
				jid->node ? "@" : "", jid->domain);
		if((b = gaim_find_buddy(js->gc->account, buddy_name)) == NULL) {
			jabber_id_free(jid);
			if(avatar_hash)
				g_free(avatar_hash);
			g_free(buddy_name);
			g_free(status);
			return;
		}

		if(avatar_hash) {
			const char *avatar_hash2 = gaim_blist_node_get_string((GaimBlistNode*)b, "avatar_hash");
			if(!avatar_hash2 || strcmp(avatar_hash, avatar_hash2)) {
				JabberIq *iq;
				xmlnode *vcard;

				iq = jabber_iq_new(js, JABBER_IQ_GET);
				xmlnode_set_attrib(iq->node, "to", buddy_name);
				vcard = xmlnode_new_child(iq->node, "vCard");
				xmlnode_set_attrib(vcard, "xmlns", "vcard-temp");

				jabber_iq_set_callback(iq, jabber_vcard_parse_avatar, NULL);
				jabber_iq_send(iq);
			}
		}

		if(state == JABBER_BUDDY_STATE_ERROR ||
				(type && (!strcmp(type, "unavailable") ||
						  !strcmp(type, "unsubscribed")))) {
			GaimConversation *conv;

			jabber_buddy_remove_resource(jb, jid->resource);
			if((conv = jabber_find_unnormalized_conv(from, js->gc->account)))
				gaim_conversation_set_name(conv, buddy_name);

		} else {
			jbr = jabber_buddy_track_resource(jb, jid->resource, priority,
					state, status);
		}

		if((found_jbr = jabber_buddy_find_resource(jb, NULL))) {
			if(!jbr || jbr == found_jbr) {
				gaim_prpl_got_user_status(js->gc->account, buddy_name, jabber_buddy_state_get_status_id(state), "priority", found_jbr->priority, found_jbr->status ? "message" : NULL, found_jbr->status, NULL);
			}
		} else {
			gaim_prpl_got_user_status(js->gc->account, buddy_name, "offline", status ? "message" : NULL, status, NULL);
		}
		g_free(buddy_name);
	}
	g_free(status);
	jabber_id_free(jid);
	if(avatar_hash)
		g_free(avatar_hash);
}

void jabber_presence_subscription_set(JabberStream *js, const char *who, const char *type)
{
	xmlnode *presence = xmlnode_new("presence");

	xmlnode_set_attrib(presence, "to", who);
	xmlnode_set_attrib(presence, "type", type);

	jabber_send(js, presence);
	xmlnode_free(presence);
}

void gaim_status_to_jabber(const GaimStatus *status, JabberBuddyState *state, const char **msg, int *priority)
{
	const char *status_id = NULL;

	*state = JABBER_BUDDY_STATE_UNKNOWN;
	*msg = NULL;
	*priority = 0;

	if(!status) {
		*state = JABBER_BUDDY_STATE_UNAVAILABLE;
	} else {
		if(state) {
			status_id = gaim_status_get_id(status);
			*state = jabber_buddy_status_id_get_state(status_id);
		}

		if(msg)
			*msg = gaim_status_get_attr_string(status, "message");

		if(priority)
			*priority = gaim_status_get_attr_int(status, "priority");
	}

}
