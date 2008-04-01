/*
 * purple - Jabber Protocol Plugin
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

#include "account.h"
#include "cipher.h"
#include "conversation.h"
#include "debug.h"
#include "notify.h"
#include "request.h"
#include "server.h"
#include "status.h"
#include "util.h"
#include "xmlnode.h"

#include "buddy.h"
#include "chat.h"
#include "google.h"
#include "presence.h"
#include "iq.h"
#include "jutil.h"
#include "adhoccommands.h"

#include "usertune.h"


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

void jabber_presence_fake_to_self(JabberStream *js, const PurpleStatus *gstatus) {
	char *my_base_jid;

	if(!js->user)
		return;

	my_base_jid = g_strdup_printf("%s@%s", js->user->node, js->user->domain);
	if(purple_find_buddy(js->gc->account, my_base_jid)) {
		JabberBuddy *jb;
		JabberBuddyResource *jbr;
		if((jb = jabber_buddy_find(js, my_base_jid, TRUE))) {
			JabberBuddyState state;
			char *msg;
			int priority;

			purple_status_to_jabber(gstatus, &state, &msg, &priority);

			if (state == JABBER_BUDDY_STATE_UNAVAILABLE || state == JABBER_BUDDY_STATE_UNKNOWN) {
				jabber_buddy_remove_resource(jb, js->user->resource);
			} else {
				jabber_buddy_track_resource(jb, js->user->resource, priority, state, msg);
			}
			if((jbr = jabber_buddy_find_resource(jb, NULL))) {
				purple_prpl_got_user_status(js->gc->account, my_base_jid, jabber_buddy_state_get_status_id(jbr->state), "priority", jbr->priority, jbr->status ? "message" : NULL, jbr->status, NULL);
			} else {
				purple_prpl_got_user_status(js->gc->account, my_base_jid, "offline", msg ? "message" : NULL, msg, NULL);
			}

			g_free(msg);
		}
	}
	g_free(my_base_jid);
}


void jabber_presence_send(PurpleAccount *account, PurpleStatus *status)
{
	PurpleConnection *gc = NULL;
	JabberStream *js = NULL;
	xmlnode *presence, *x, *photo;
	char *stripped = NULL;
	JabberBuddyState state;
	int priority;
	const char *artist = NULL, *title = NULL, *source = NULL, *uri = NULL, *track = NULL;
	int length = -1;
	gboolean allowBuzz;
	PurplePresence *p;
	PurpleStatus *tune;

	if (purple_account_is_disconnected(account))
		return;

	p = purple_account_get_presence(account);
	if (NULL == status) {
		status = purple_presence_get_active_status(p);
	}

	if (purple_status_is_exclusive(status)) {
		/* An exclusive status can't be deactivated. You should just
		 * activate some other exclusive status. */
		if (!purple_status_is_active(status))
			return;
	} else {
		/* Work with the exclusive status. */
		status = purple_presence_get_active_status(p);
	}

	gc = purple_account_get_connection(account);
	js = gc->proto_data;

	/* we don't want to send presence before we've gotten our roster */
	if(!js->roster_parsed) {
		purple_debug_info("jabber", "attempt to send presence before roster retrieved\n");
		return;
	}

	purple_status_to_jabber(status, &state, &stripped, &priority);
	
	/* check for buzz support */
	allowBuzz = purple_status_get_attr_boolean(status,"buzz");
	/* changing the buzz state has to trigger a re-broadcasting of the presence for caps */
	
#define CHANGED(a,b) ((!a && b) || (a && a[0] == '\0' && b && b[0] != '\0') || \
					  (a && !b) || (a && a[0] != '\0' && b && b[0] == '\0') || (a && b && strcmp(a,b)))
	/* check if there are any differences to the <presence> and send them in that case */
	if (allowBuzz != js->allowBuzz || js->old_state != state || CHANGED(js->old_msg, stripped) ||
		js->old_priority != priority || CHANGED(js->old_avatarhash, js->avatar_hash)) {
		js->allowBuzz = allowBuzz;

		if (js->googletalk && stripped == NULL && purple_presence_is_status_primitive_active(p, PURPLE_STATUS_TUNE)) {
			tune = purple_presence_get_status(p, "tune");
			stripped = jabber_google_presence_outgoing(tune);
		}

		presence = jabber_presence_create_js(js, state, stripped, priority);

		if(js->avatar_hash) {
			x = xmlnode_new_child(presence, "x");
			xmlnode_set_namespace(x, "vcard-temp:x:update");
			photo = xmlnode_new_child(x, "photo");
			xmlnode_insert_data(photo, js->avatar_hash, -1);
		}

		jabber_send(js, presence);

		g_hash_table_foreach(js->chats, chats_send_presence_foreach, presence);
		xmlnode_free(presence);
		
		/* update old values */
		
		if(js->old_msg)
			g_free(js->old_msg);
		if(js->old_avatarhash)
			g_free(js->old_avatarhash);
		js->old_msg = g_strdup(stripped);
		js->old_avatarhash = g_strdup(js->avatar_hash);
		js->old_state = state;
		js->old_priority = priority;
		g_free(stripped);
	}
					  	
	/* next, check if there are any changes to the tune values */
	tune = purple_presence_get_status(p, "tune");
	if (tune && purple_status_is_active(tune)) {
		artist = purple_status_get_attr_string(tune, PURPLE_TUNE_ARTIST);
		title = purple_status_get_attr_string(tune, PURPLE_TUNE_TITLE);
		source = purple_status_get_attr_string(tune, PURPLE_TUNE_ALBUM);
		uri = purple_status_get_attr_string(tune, PURPLE_TUNE_URL);
		track = purple_status_get_attr_string(tune, PURPLE_TUNE_TRACK);
		length = (!purple_status_get_attr_value(tune, PURPLE_TUNE_TIME)) ? -1 :
				purple_status_get_attr_int(tune, PURPLE_TUNE_TIME);
	}
	
	if(CHANGED(artist, js->old_artist) || CHANGED(title, js->old_title) || CHANGED(source, js->old_source) ||
	   CHANGED(uri, js->old_uri) || CHANGED(track, js->old_track) || (length != js->old_length)) {
		PurpleJabberTuneInfo tuneinfo = {
			(char*)artist,
			(char*)title,
			(char*)source,
			(char*)track,
			length,
			(char*)uri
		};
		jabber_tune_set(js->gc, &tuneinfo);
		
		/* update old values */
		g_free(js->old_artist);
		g_free(js->old_title);
		g_free(js->old_source);
		g_free(js->old_uri);
		g_free(js->old_track);
		js->old_artist = g_strdup(artist);
		js->old_title = g_strdup(title);
		js->old_source = g_strdup(source);
		js->old_uri = g_strdup(uri);
		js->old_length = length;
		js->old_track = g_strdup(track);
	}

#undef CHANGED

	jabber_presence_fake_to_self(js, status);
}

xmlnode *jabber_presence_create(JabberBuddyState state, const char *msg, int priority)
{
    return jabber_presence_create_js(NULL, state, msg, priority);
}

xmlnode *jabber_presence_create_js(JabberStream *js, JabberBuddyState state, const char *msg, int priority)
{
	xmlnode *show, *status, *presence, *pri, *c;
	const char *show_string = NULL;

	presence = xmlnode_new("presence");

	if(state == JABBER_BUDDY_STATE_UNAVAILABLE)
		xmlnode_set_attrib(presence, "type", "unavailable");
	else if(state != JABBER_BUDDY_STATE_ONLINE &&
			state != JABBER_BUDDY_STATE_UNKNOWN &&
			state != JABBER_BUDDY_STATE_ERROR)
		show_string = jabber_buddy_state_get_show(state);

	if(show_string) {
		show = xmlnode_new_child(presence, "show");
		xmlnode_insert_data(show, show_string, -1);
	}

	if(msg) {
		status = xmlnode_new_child(presence, "status");
		xmlnode_insert_data(status, msg, -1);
	}

	if(priority) {
		char *pstr = g_strdup_printf("%d", priority);
		pri = xmlnode_new_child(presence, "priority");
		xmlnode_insert_data(pri, pstr, -1);
		g_free(pstr);
	}

	/* JEP-0115 */
	c = xmlnode_new_child(presence, "c");
	xmlnode_set_namespace(c, "http://jabber.org/protocol/caps");
	xmlnode_set_attrib(c, "node", CAPS0115_NODE);
	xmlnode_set_attrib(c, "ver", VERSION);
	
	if(js != NULL) {
		/* add the extensions */
		char extlist[1024];
		unsigned remaining = 1023; /* one less for the \0 */
		GList *feature;
		
		extlist[0] = '\0';
		for(feature = jabber_features; feature && remaining > 0; feature = feature->next) {
			JabberFeature *feat = (JabberFeature*)feature->data;
			unsigned featlen;
			
			if(feat->is_enabled != NULL && feat->is_enabled(js, feat->shortname, feat->namespace) == FALSE)
				continue; /* skip this feature */
			
			featlen = strlen(feat->shortname);
			
			/* cut off when we don't have any more space left in our buffer (too bad) */
			if(featlen > remaining)
				break;
			
			strncat(extlist,feat->shortname,remaining);
			remaining -= featlen;
			if(feature->next) { /* no space at the end */
				strncat(extlist," ",remaining);
				--remaining;
			}
		}
		/* did we add anything? */
		if(remaining < 1023)
			xmlnode_set_attrib(c, "ext", extlist);
	}
	
	return presence;
}

struct _jabber_add_permit {
	PurpleConnection *gc;
	JabberStream *js;
	char *who;
};

static void authorize_add_cb(gpointer data)
{
	struct _jabber_add_permit *jap = data;
	if(PURPLE_CONNECTION_IS_VALID(jap->gc))
		jabber_presence_subscription_set(jap->gc->proto_data,
			jap->who, "subscribed");
	g_free(jap->who);
	g_free(jap);
}

static void deny_add_cb(gpointer data)
{
	struct _jabber_add_permit *jap = data;
	if(PURPLE_CONNECTION_IS_VALID(jap->gc))
		jabber_presence_subscription_set(jap->gc->proto_data,
			jap->who, "unsubscribed");
	g_free(jap->who);
	g_free(jap);
}

static void jabber_vcard_parse_avatar(JabberStream *js, xmlnode *packet, gpointer blah)
{
	JabberBuddy *jb = NULL;
	xmlnode *vcard, *photo, *binval;
	char *text;
	guchar *data;
	gsize size;
	const char *from = xmlnode_get_attrib(packet, "from");

	if(!from)
		return;

	jb = jabber_buddy_find(js, from, TRUE);

	js->pending_avatar_requests = g_slist_remove(js->pending_avatar_requests, jb);

	if((vcard = xmlnode_get_child(packet, "vCard")) ||
			(vcard = xmlnode_get_child_with_namespace(packet, "query", "vcard-temp"))) {
		if((photo = xmlnode_get_child(vcard, "PHOTO")) &&
				(( (binval = xmlnode_get_child(photo, "BINVAL")) &&
				(text = xmlnode_get_data(binval))) ||
				(text = xmlnode_get_data(photo)))) {
			unsigned char hashval[20];
			char hash[41], *p;
			int i;

			data = purple_base64_decode(text, &size);

			purple_cipher_digest_region("sha1", data, size,
					sizeof(hashval), hashval, NULL);
			p = hash;
			for(i=0; i<20; i++, p+=2)
				snprintf(p, 3, "%02x", hashval[i]);

			purple_buddy_icons_set_for_user(js->gc->account, from, data, size, hash);
			g_free(text);
		}
	}
}

typedef struct _JabberPresenceCapabilities {
	JabberStream *js;
	JabberBuddyResource *jbr;
	char *from;
} JabberPresenceCapabilities;

static void jabber_presence_set_capabilities(JabberCapsClientInfo *info, gpointer user_data) {
	JabberPresenceCapabilities *userdata = user_data;
	GList *iter;

	if(userdata->jbr->caps)
		jabber_caps_free_clientinfo(userdata->jbr->caps);
	userdata->jbr->caps = info;

	if (info) {
		for(iter = info->features; iter; iter = g_list_next(iter)) {
			if(!strcmp((const char*)iter->data, "http://jabber.org/protocol/commands")) {
				JabberIq *iq = jabber_iq_new_query(userdata->js, JABBER_IQ_GET, "http://jabber.org/protocol/disco#items");
				xmlnode *query = xmlnode_get_child_with_namespace(iq->node,"query","http://jabber.org/protocol/disco#items");
				xmlnode_set_attrib(iq->node, "to", userdata->from);
				xmlnode_set_attrib(query, "node", "http://jabber.org/protocol/commands");

				jabber_iq_set_callback(iq, jabber_adhoc_disco_result_cb, NULL);
				jabber_iq_send(iq);
				break;
			}
		}
	}

	g_free(userdata->from);
	g_free(userdata);
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
	PurpleConvChatBuddyFlags flags = PURPLE_CBFLAGS_NONE;
	gboolean delayed = FALSE;
	PurpleBuddy *b = NULL;
	char *buddy_name;
	JabberBuddyState state = JABBER_BUDDY_STATE_UNKNOWN;
	xmlnode *y;
	gboolean muc = FALSE;
	char *avatar_hash = NULL;
	xmlnode *caps = NULL;

	if(!(jb = jabber_buddy_find(js, from, TRUE)))
		return;

	if(!(jid = jabber_id_new(from)))
		return;

	if(jb->error_msg) {
		g_free(jb->error_msg);
		jb->error_msg = NULL;
	}

	if(type && !strcmp(type, "error")) {
		char *msg = jabber_parse_error(js, packet, NULL);

		state = JABBER_BUDDY_STATE_ERROR;
		jb->error_msg = msg ? msg : g_strdup(_("Unknown Error in presence"));
	} else if(type && !strcmp(type, "subscribe")) {
		struct _jabber_add_permit *jap = g_new0(struct _jabber_add_permit, 1);
		gboolean onlist = FALSE;
		PurpleBuddy *buddy = purple_find_buddy(purple_connection_get_account(js->gc), from);
		JabberBuddy *jb = NULL;

		if (buddy) {
			jb = jabber_buddy_find(js, from, TRUE);
			if ((jb->subscription & JABBER_SUB_TO))
				onlist = TRUE;
		}

		jap->gc = js->gc;
		jap->who = g_strdup(from);
		jap->js = js;

		purple_account_request_authorization(purple_connection_get_account(js->gc), from, NULL, NULL, NULL, onlist,
				authorize_add_cb, deny_add_cb, jap);
		jabber_id_free(jid);
		return;
	} else if(type && !strcmp(type, "subscribed")) {
		/* we've been allowed to see their presence, but we don't care */
		jabber_id_free(jid);
		return;
	} else if(type && !strcmp(type, "unsubscribe")) {
		/* XXX I'm not sure this is the right way to handle this, it
		 * might be better to add "unsubscribe" to the presence status
		 * if lower down, but I'm not sure. */
		/* they are unsubscribing from our presence, we don't care */
		/* Well, maybe just a little, we might want/need to start
		 * acknowledging this (and the others) at some point. */
		jabber_id_free(jid);
		return;
	} else {
		if((y = xmlnode_get_child(packet, "show"))) {
			char *show = xmlnode_get_data(y);
			state = jabber_buddy_show_get_state(show);
			g_free(show);
		} else {
			state = JABBER_BUDDY_STATE_ONLINE;
		}
	}


	for(y = packet->child; y; y = y->next) {
		const char *xmlns;
		if(y->type != XMLNODE_TYPE_TAG)
			continue;
		xmlns = xmlnode_get_namespace(y);

		if(!strcmp(y->name, "status")) {
			g_free(status);
			status = xmlnode_get_data(y);
		} else if(!strcmp(y->name, "priority")) {
			char *p = xmlnode_get_data(y);
			if(p) {
				priority = atoi(p);
				g_free(p);
			}
		} else if(!strcmp(y->name, "delay") && !strcmp(xmlns, "urn:xmpp:delay")) {
			/* XXX: compare the time.  jabber:x:delay can happen on presence packets that aren't really and truly delayed */
			delayed = TRUE;
		} else if(!strcmp(y->name, "c") && !strcmp(xmlns, "http://jabber.org/protocol/caps")) {
			caps = y; /* store for later, when creating buddy resource */
		} else if(!strcmp(y->name, "x")) {
			const char *xmlns = xmlnode_get_namespace(y);
			if(xmlns && !strcmp(xmlns, "jabber:x:delay")) {
				/* XXX: compare the time.  jabber:x:delay can happen on presence packets that aren't really and truly delayed */
				delayed = TRUE;
			} else if(xmlns && !strcmp(xmlns, "http://jabber.org/protocol/muc#user")) {
				xmlnode *z;

				muc = TRUE;
				if((z = xmlnode_get_child(y, "status"))) {
					const char *code = xmlnode_get_attrib(z, "code");
					if(code && !strcmp(code, "201")) {
						if((chat = jabber_chat_find(js, jid->node, jid->domain))) {
							chat->config_dialog_type = PURPLE_REQUEST_ACTION;
							chat->config_dialog_handle =
								purple_request_action(js->gc,
										_("Create New Room"),
										_("Create New Room"),
										_("You are creating a new room.  Would"
											" you like to configure it, or"
											" accept the default settings?"),
										/* Default Action */ 1,
										purple_connection_get_account(js->gc), NULL, chat->conv,
										chat, 2,
										_("_Configure Room"), G_CALLBACK(jabber_chat_request_room_configure),
										_("_Accept Defaults"), G_CALLBACK(jabber_chat_create_instant_room));
						}
					} else if(code && !strcmp(code, "210")) {
						/*  server rewrote room-nick */
						if((chat = jabber_chat_find(js, jid->node, jid->domain))) {
							g_free(chat->handle);
							chat->handle = g_strdup(jid->resource);
						}
					}
				}
				if((z = xmlnode_get_child(y, "item"))) {
					real_jid = xmlnode_get_attrib(z, "jid");
					affiliation = xmlnode_get_attrib(z, "affiliation");
					role = xmlnode_get_attrib(z, "role");
					if(affiliation != NULL && !strcmp(affiliation, "owner"))
						flags |= PURPLE_CBFLAGS_FOUNDER;
					if (role != NULL) {
						if (!strcmp(role, "moderator"))
							flags |= PURPLE_CBFLAGS_OP;
						else if (!strcmp(role, "participant"))
							flags |= PURPLE_CBFLAGS_VOICE;
					}
				}
			} else if(xmlns && !strcmp(xmlns, "vcard-temp:x:update")) {
				xmlnode *photo = xmlnode_get_child(y, "photo");
				if(photo) {
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
			char *title, *msg = jabber_parse_error(js, packet, NULL);

			if(chat->conv) {
				title = g_strdup_printf(_("Error in chat %s"), from);
				if (g_hash_table_size(chat->members) == 0)
					serv_got_chat_left(js->gc, chat->id);
			} else {
				title = g_strdup_printf(_("Error joining chat %s"), from);
			}
			purple_notify_error(js->gc, title, title, msg);
			g_free(title);
			g_free(msg);

			if (g_hash_table_size(chat->members) == 0)
				/* Only destroy the chat if the error happened while joining */
				jabber_chat_destroy(chat);
			jabber_id_free(jid);
			g_free(status);
			g_free(room_jid);
			g_free(avatar_hash);
			return;
		}


		if(type && !strcmp(type, "unavailable")) {
			gboolean nick_change = FALSE;

			/* If we haven't joined the chat yet, we don't care that someone
			 * left, or it was us leaving after we closed the chat */
			if(!chat->conv) {
				if(jid->resource && chat->handle && !strcmp(jid->resource, chat->handle))
					jabber_chat_destroy(chat);
				jabber_id_free(jid);
				g_free(status);
				g_free(room_jid);
				g_free(avatar_hash);
				return;
			}

			jabber_buddy_remove_resource(jb, jid->resource);
			if(chat->muc) {
				xmlnode *x;
				for(x = xmlnode_get_child(packet, "x"); x; x = xmlnode_get_next_twin(x)) {
					const char *xmlns, *nick, *code;
					xmlnode *stat, *item;
					if(!(xmlns = xmlnode_get_namespace(x)) ||
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
						purple_conv_chat_rename_user(PURPLE_CONV_CHAT(chat->conv), jid->resource, nick);
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
					purple_conv_chat_remove_user(PURPLE_CONV_CHAT(chat->conv), jid->resource,
							status);
					jabber_chat_remove_handle(chat, jid->resource);
				}
			}
		} else {
			if(!chat->conv) {
				chat->id = i++;
				chat->muc = muc;
				chat->conv = serv_got_joined_chat(js->gc, chat->id, room_jid);
				purple_conv_chat_set_nick(PURPLE_CONV_CHAT(chat->conv), chat->handle);

				jabber_chat_disco_traffic(chat);
			}

			jabber_buddy_track_resource(jb, jid->resource, priority, state,
					status);

			jabber_chat_track_handle(chat, jid->resource, real_jid, affiliation, role);

			if(!jabber_chat_find_buddy(chat->conv, jid->resource))
				purple_conv_chat_add_user(PURPLE_CONV_CHAT(chat->conv), jid->resource,
						real_jid, flags, !delayed);
			else
				purple_conv_chat_user_set_flags(PURPLE_CONV_CHAT(chat->conv), jid->resource,
						flags);
		}
		g_free(room_jid);
	} else {
		buddy_name = g_strdup_printf("%s%s%s", jid->node ? jid->node : "",
									 jid->node ? "@" : "", jid->domain);
		if((b = purple_find_buddy(js->gc->account, buddy_name)) == NULL) {
			if(!jid->node || strcmp(jid->node,js->user->node) || strcmp(jid->domain,js->user->domain)) {
				purple_debug_warning("jabber", "Got presence for unknown buddy %s on account %s (%p)\n",
									 buddy_name, purple_account_get_username(js->gc->account), js->gc->account);
				jabber_id_free(jid);
				g_free(avatar_hash);
				g_free(buddy_name);
				g_free(status);
				return;
			} else {
				/* this is a different resource of our own account. Resume even when this account isn't on our blist */
			}
		}

		if(b && avatar_hash) {
			const char *avatar_hash2 = purple_buddy_icons_get_checksum_for_user(b);
			if(!avatar_hash2 || strcmp(avatar_hash, avatar_hash2)) {
				JabberIq *iq;
				xmlnode *vcard;

				/* XXX this is a crappy way of trying to prevent
				 * someone from spamming us with presence packets
				 * and causing us to DoS ourselves...what we really
				 * need is a queue system that can throttle itself,
				 * but i'm too tired to write that right now */
				if(!g_slist_find(js->pending_avatar_requests, jb)) {

					js->pending_avatar_requests = g_slist_prepend(js->pending_avatar_requests, jb);

					iq = jabber_iq_new(js, JABBER_IQ_GET);
					xmlnode_set_attrib(iq->node, "to", buddy_name);
					vcard = xmlnode_new_child(iq->node, "vCard");
					xmlnode_set_namespace(vcard, "vcard-temp");

					jabber_iq_set_callback(iq, jabber_vcard_parse_avatar, NULL);
					jabber_iq_send(iq);
				}
			}
		}

		if(state == JABBER_BUDDY_STATE_ERROR ||
				(type && (!strcmp(type, "unavailable") ||
						  !strcmp(type, "unsubscribed")))) {
			PurpleConversation *conv;

			jabber_buddy_remove_resource(jb, jid->resource);
			if((conv = jabber_find_unnormalized_conv(from, js->gc->account)))
				purple_conversation_set_name(conv, buddy_name);

		} else {
			jbr = jabber_buddy_track_resource(jb, jid->resource, priority,
					state, status);
			if(caps) {
				const char *node = xmlnode_get_attrib(caps,"node");
				const char *ver = xmlnode_get_attrib(caps,"ver");
				const char *ext = xmlnode_get_attrib(caps,"ext");
				
				if(node && ver) {
					JabberPresenceCapabilities *userdata = g_new0(JabberPresenceCapabilities, 1);
					userdata->js = js;
					userdata->jbr = jbr;
					userdata->from = g_strdup(from);
					jabber_caps_get_info(js, from, node, ver, ext, jabber_presence_set_capabilities, userdata);
				}
			}
		}

		if((found_jbr = jabber_buddy_find_resource(jb, NULL))) {
			jabber_google_presence_incoming(js, buddy_name, found_jbr);
			purple_prpl_got_user_status(js->gc->account, buddy_name, jabber_buddy_state_get_status_id(found_jbr->state), "priority", found_jbr->priority, "message", found_jbr->status, NULL);
		} else {
			purple_prpl_got_user_status(js->gc->account, buddy_name, "offline", status ? "message" : NULL, status, NULL);
		}
		g_free(buddy_name);
	}
	g_free(status);
	jabber_id_free(jid);
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

void purple_status_to_jabber(const PurpleStatus *status, JabberBuddyState *state, char **msg, int *priority)
{
	const char *status_id = NULL;
	const char *formatted_msg = NULL;

	if(state) *state = JABBER_BUDDY_STATE_UNKNOWN;
	if(msg) *msg = NULL;
	if(priority) *priority = 0;

	if(!status) {
		if(state) *state = JABBER_BUDDY_STATE_UNAVAILABLE;
	} else {
		if(state) {
			status_id = purple_status_get_id(status);
			*state = jabber_buddy_status_id_get_state(status_id);
		}

		if(msg) {
			formatted_msg = purple_status_get_attr_string(status, "message");

			/* if the message is blank, then there really isn't a message */
			if(formatted_msg && !*formatted_msg)
				formatted_msg = NULL;

			if(formatted_msg)
				*msg = purple_markup_strip_html(formatted_msg);
		}

		if(priority)
			*priority = purple_status_get_attr_int(status, "priority");
	}
}
