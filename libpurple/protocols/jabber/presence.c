/*
 * purple - Jabber Protocol Plugin
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

#include "account.h"
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

	if(!chat->conv || chat->left)
		return;

	chat_full_jid = g_strdup_printf("%s@%s/%s", chat->room, chat->server,
			chat->handle);

	xmlnode_set_attrib(presence, "to", chat_full_jid);
	jabber_send(chat->js, presence);
	g_free(chat_full_jid);
}

void jabber_presence_fake_to_self(JabberStream *js, PurpleStatus *status)
{
	PurpleAccount *account;
	PurplePresence *presence;
	const char *username;

	g_return_if_fail(js->user != NULL);

	account = purple_connection_get_account(js->gc);
	username = purple_connection_get_display_name(js->gc);
	presence = purple_account_get_presence(account);
	if (status == NULL)
		status = purple_presence_get_active_status(presence);

	if (purple_find_buddy(account, username)) {
		JabberBuddy *jb = jabber_buddy_find(js, username, TRUE);
		JabberBuddyResource *jbr;
		JabberBuddyState state;
		char *msg;
		int priority;

		g_return_if_fail(jb != NULL);

		purple_status_to_jabber(status, &state, &msg, &priority);

		if (state == JABBER_BUDDY_STATE_UNAVAILABLE ||
				state == JABBER_BUDDY_STATE_UNKNOWN) {
			jabber_buddy_remove_resource(jb, js->user->resource);
		} else {
			jbr = jabber_buddy_track_resource(jb, js->user->resource, priority,
					state, msg);
			jbr->idle = purple_presence_is_idle(presence) ?
					purple_presence_get_idle_time(presence) : 0;
		}

		if ((jbr = jabber_buddy_find_resource(jb, NULL))) {
			purple_prpl_got_user_status(account, username,
					jabber_buddy_state_get_status_id(jbr->state),
					"priority", jbr->priority,
					jbr->status ? "message" : NULL, jbr->status,
					NULL);
			purple_prpl_got_user_idle(account, username, jbr->idle, jbr->idle);
		} else {
			purple_prpl_got_user_status(account, username, "offline",
					msg ? "message" : NULL, msg,
					NULL);
		}
		g_free(msg);
	}
}

void jabber_set_status(PurpleAccount *account, PurpleStatus *status)
{
	PurpleConnection *gc;
	JabberStream *js;

	if (!purple_account_is_connected(account))
		return;

	if (purple_status_is_exclusive(status) && !purple_status_is_active(status)) {
		/* An exclusive status can't be deactivated. You should just
		 * activate some other exclusive status. */
		return;
	}

	gc = purple_account_get_connection(account);
	js = purple_connection_get_protocol_data(gc);
	jabber_presence_send(js, FALSE);
}

void jabber_presence_send(JabberStream *js, gboolean force)
{
	PurpleAccount *account;
	xmlnode *presence, *x, *photo;
	char *stripped = NULL;
	JabberBuddyState state;
	int priority;
	const char *artist = NULL, *title = NULL, *source = NULL, *uri = NULL, *track = NULL;
	int length = -1;
	gboolean allowBuzz;
	PurplePresence *p;
	PurpleStatus *status, *tune;

	account = purple_connection_get_account(js->gc);
	p = purple_account_get_presence(account);
	status = purple_presence_get_active_status(p);

	/* we don't want to send presence before we've gotten our roster */
	if (js->state != JABBER_STREAM_CONNECTED) {
		purple_debug_info("jabber", "attempt to send presence before roster retrieved\n");
		return;
	}

	purple_status_to_jabber(status, &state, &stripped, &priority);

	/* check for buzz support */
	allowBuzz = purple_status_get_attr_boolean(status,"buzz");
	/* changing the buzz state has to trigger a re-broadcasting of the presence for caps */

	tune = purple_presence_get_status(p, "tune");
	if (js->googletalk && !stripped && purple_status_is_active(tune)) {
		stripped = jabber_google_presence_outgoing(tune);
	}

#define CHANGED(a,b) ((!a && b) || (a && a[0] == '\0' && b && b[0] != '\0') || \
					  (a && !b) || (a && a[0] != '\0' && b && b[0] == '\0') || (a && b && strcmp(a,b)))
	/* check if there are any differences to the <presence> and send them in that case */
	if (force || allowBuzz != js->allowBuzz || js->old_state != state ||
		CHANGED(js->old_msg, stripped) || js->old_priority != priority ||
		CHANGED(js->old_avatarhash, js->avatar_hash) || js->old_idle != js->idle) {
		/* Need to update allowBuzz before creating the presence (with caps) */
		js->allowBuzz = allowBuzz;

		presence = jabber_presence_create_js(js, state, stripped, priority);

		/* Per XEP-0153 4.1, we must always send the <x> */
		x = xmlnode_new_child(presence, "x");
		xmlnode_set_namespace(x, "vcard-temp:x:update");
		/*
		 * FIXME: Per XEP-0153 4.3.2 bullet 2, we must not publish our
		 * image hash if another resource has logged in and updated the
		 * vcard avatar. Requires changes in jabber_presence_parse.
		 */
		if (js->vcard_fetched) {
			/* Always publish a <photo>; it's empty if we have no image. */
			photo = xmlnode_new_child(x, "photo");
			if (js->avatar_hash)
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
		js->old_idle = js->idle;
	}
	g_free(stripped);

	/* next, check if there are any changes to the tune values */
	if (purple_status_is_active(tune)) {
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
#ifdef USE_VV
	gboolean audio_enabled, video_enabled;
#endif

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

	/* if we are idle and not offline, include idle */
	if (js->idle && state != JABBER_BUDDY_STATE_UNAVAILABLE) {
		xmlnode *query = xmlnode_new_child(presence, "query");
		gchar seconds[10];
		g_snprintf(seconds, 10, "%d", (int) (time(NULL) - js->idle));

		xmlnode_set_namespace(query, NS_LAST_ACTIVITY);
		xmlnode_set_attrib(query, "seconds", seconds);
	}

	/* JEP-0115 */
	/* calculate hash */
	jabber_caps_calculate_own_hash(js);
	/* create xml */
	c = xmlnode_new_child(presence, "c");
	xmlnode_set_namespace(c, "http://jabber.org/protocol/caps");
	xmlnode_set_attrib(c, "node", CAPS0115_NODE);
	xmlnode_set_attrib(c, "hash", "sha-1");
	xmlnode_set_attrib(c, "ver", jabber_caps_get_own_hash(js));

#ifdef USE_VV
	/*
	 * MASSIVE HUGE DISGUSTING HACK
	 * This is a huge hack. As far as I can tell, Google Talk's gmail client
	 * doesn't bother to check the actual features we advertise; they
	 * just assume that if we specify a 'voice-v1' ext (ignoring that
	 * these are to be assigned no semantic value), we support receiving voice
	 * calls.
	 *
	 * Ditto for 'video-v1'.
	 */
	audio_enabled = jabber_audio_enabled(js, NULL /* unused */);
	video_enabled = jabber_video_enabled(js, NULL /* unused */);

	if (audio_enabled && video_enabled)
		xmlnode_set_attrib(c, "ext", "voice-v1 camera-v1 video-v1");
	else if (audio_enabled)
		xmlnode_set_attrib(c, "ext", "voice-v1");
	else if (video_enabled)
		xmlnode_set_attrib(c, "ext", "camera-v1 video-v1");
#endif

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

static void
jabber_vcard_parse_avatar(JabberStream *js, const char *from,
                          JabberIqType type, const char *id,
                          xmlnode *packet, gpointer blah)
{
	JabberBuddy *jb = NULL;
	xmlnode *vcard, *photo, *binval, *fn, *nick;
	char *text;

	if(!from)
		return;

	jb = jabber_buddy_find(js, from, TRUE);

	js->pending_avatar_requests = g_slist_remove(js->pending_avatar_requests, jb);

	if((vcard = xmlnode_get_child(packet, "vCard")) ||
			(vcard = xmlnode_get_child_with_namespace(packet, "query", "vcard-temp"))) {
		/* The logic here regarding the nickname and full name is copied from
		 * buddy.c:jabber_vcard_parse. */
		gchar *nickname = NULL;
		if ((fn = xmlnode_get_child(vcard, "FN")))
			nickname = xmlnode_get_data(fn);

		if ((nick = xmlnode_get_child(vcard, "NICKNAME"))) {
			char *tmp = xmlnode_get_data(nick);
			char *bare_jid = jabber_get_bare_jid(from);
			if (tmp && strstr(bare_jid, tmp) == NULL) {
				g_free(nickname);
				nickname = tmp;
			} else if (tmp)
				g_free(tmp);

			g_free(bare_jid);
		}

		if (nickname) {
			serv_got_alias(js->gc, from, nickname);
			g_free(nickname);
		}

		if ((photo = xmlnode_get_child(vcard, "PHOTO")) &&
				(binval = xmlnode_get_child(photo, "BINVAL")) &&
				(text = xmlnode_get_data(binval))) {
			guchar *data;
			gsize size;

			data = purple_base64_decode(text, &size);
			if (data) {
				gchar *hash = jabber_calculate_data_sha1sum(data, size);
				purple_buddy_icons_set_for_user(js->gc->account, from, data,
				                                size, hash);
				g_free(hash);
			}

			g_free(text);
		}
	}
}

typedef struct _JabberPresenceCapabilities {
	JabberStream *js;
	JabberBuddy *jb;
	char *from;
} JabberPresenceCapabilities;

static void
jabber_presence_set_capabilities(JabberCapsClientInfo *info, GList *exts,
                                 JabberPresenceCapabilities *userdata)
{
	JabberBuddyResource *jbr;
	char *resource = g_utf8_strchr(userdata->from, -1, '/');

	if (resource)
		resource += 1;

	jbr = jabber_buddy_find_resource(userdata->jb, resource);
	if (!jbr) {
		g_free(userdata->from);
		g_free(userdata);
		if (exts) {
			g_list_foreach(exts, (GFunc)g_free, NULL);
			g_list_free(exts);
		}
		return;
	}

	/* Any old jbr->caps.info is owned by the caps code */
	if (jbr->caps.exts) {
		g_list_foreach(jbr->caps.exts, (GFunc)g_free, NULL);
		g_list_free(jbr->caps.exts);
	}

	jbr->caps.info = info;
	jbr->caps.exts = exts;

	if (info == NULL)
		goto out;

	if (!jbr->commands_fetched && jabber_resource_has_capability(jbr, "http://jabber.org/protocol/commands")) {
		JabberIq *iq = jabber_iq_new_query(userdata->js, JABBER_IQ_GET, NS_DISCO_ITEMS);
		xmlnode *query = xmlnode_get_child_with_namespace(iq->node, "query", NS_DISCO_ITEMS);
		xmlnode_set_attrib(iq->node, "to", userdata->from);
		xmlnode_set_attrib(query, "node", "http://jabber.org/protocol/commands");
		jabber_iq_set_callback(iq, jabber_adhoc_disco_result_cb, NULL);
		jabber_iq_send(iq);

		jbr->commands_fetched = TRUE;
	}

#if 0
	/*
	 * Versions of libpurple before 2.6.0 didn't advertise this capability, so
	 * we can't yet use Entity Capabilities to determine whether or not the
	 * other client supports Chat States.
	 */
	if (jabber_resource_has_capability(jbr, "http://jabber.org/protocol/chatstates"))
		jbr->chat_states = JABBER_CHAT_STATES_SUPPORTED;
	else
		jbr->chat_states = JABBER_CHAT_STATES_UNSUPPORTED;
#endif

out:
	g_free(userdata->from);
	g_free(userdata);
}

void jabber_presence_parse(JabberStream *js, xmlnode *packet)
{
	const char *from;
	const char *type;
	char *status = NULL;
	int priority = 0;
	JabberID *jid;
	JabberChat *chat;
	JabberBuddy *jb;
	JabberBuddyResource *jbr = NULL, *found_jbr = NULL;
	PurpleConvChatBuddyFlags flags = PURPLE_CBFLAGS_NONE;
	gboolean delayed = FALSE;
	const gchar *stamp = NULL; /* from <delayed/> element */
	PurpleBuddy *b = NULL;
	char *buddy_name;
	JabberBuddyState state = JABBER_BUDDY_STATE_UNKNOWN;
	xmlnode *y;
	char *avatar_hash = NULL;
	xmlnode *caps = NULL;
	int idle = 0;
	gchar *nickname = NULL;
	gboolean signal_return;

	from = xmlnode_get_attrib(packet, "from");
	type = xmlnode_get_attrib(packet, "type");

	jb = jabber_buddy_find(js, from, TRUE);
	g_return_if_fail(jb != NULL);

	signal_return = GPOINTER_TO_INT(purple_signal_emit_return_1(purple_connection_get_prpl(js->gc),
			"jabber-receiving-presence", js->gc, type, from, packet));
	if (signal_return)
		return;

	jid = jabber_id_new(from);
	if (jid == NULL) {
		purple_debug_error("jabber", "Ignoring presence with malformed 'from' "
		                   "JID: %s\n", from);
		return;
	}

	if(jb->error_msg) {
		g_free(jb->error_msg);
		jb->error_msg = NULL;
	}

	if (type == NULL) {
		xmlnode *show;
		char *show_data = NULL;

		state = JABBER_BUDDY_STATE_ONLINE;

		show = xmlnode_get_child(packet, "show");
		if (show) {
			show_data = xmlnode_get_data(show);
			if (show_data) {
				state = jabber_buddy_show_get_state(show_data);
				g_free(show_data);
			} else
				purple_debug_warning("jabber", "<show/> present on presence, "
				                     "but no contents!\n");
		}
	} else if (g_str_equal(type, "error")) {
		char *msg = jabber_parse_error(js, packet, NULL);

		state = JABBER_BUDDY_STATE_ERROR;
		jb->error_msg = msg ? msg : g_strdup(_("Unknown Error in presence"));
	} else if (g_str_equal(type, "subscribe")) {
		struct _jabber_add_permit *jap = g_new0(struct _jabber_add_permit, 1);
		gboolean onlist = FALSE;
		PurpleAccount *account;
		PurpleBuddy *buddy;
		JabberBuddy *jb = NULL;
		xmlnode *nick;

		account = purple_connection_get_account(js->gc);
		buddy = purple_find_buddy(account, from);
		nick = xmlnode_get_child_with_namespace(packet, "nick", "http://jabber.org/protocol/nick");
		if (nick)
			nickname = xmlnode_get_data(nick);

		if (buddy) {
			jb = jabber_buddy_find(js, from, TRUE);
			if ((jb->subscription & (JABBER_SUB_TO | JABBER_SUB_PENDING)))
				onlist = TRUE;
		}

		jap->gc = js->gc;
		jap->who = g_strdup(from);
		jap->js = js;

		purple_account_request_authorization(account, from, NULL, nickname,
				NULL, onlist, authorize_add_cb, deny_add_cb, jap);

		g_free(nickname);
		jabber_id_free(jid);
		return;
	} else if (g_str_equal(type, "subscribed")) {
		/* we've been allowed to see their presence, but we don't care */
		jabber_id_free(jid);
		return;
	} else if (g_str_equal(type, "unsubscribe")) {
		/* XXX I'm not sure this is the right way to handle this, it
		 * might be better to add "unsubscribe" to the presence status
		 * if lower down, but I'm not sure. */
		/* they are unsubscribing from our presence, we don't care */
		/* Well, maybe just a little, we might want/need to start
		 * acknowledging this (and the others) at some point. */
		jabber_id_free(jid);
		return;
	} else if (g_str_equal(type, "probe")) {
		purple_debug_warning("jabber", "Ignoring presence probe\n");
		jabber_id_free(jid);
		return;
	} else if (g_str_equal(type, "unavailable")) {
		state = JABBER_BUDDY_STATE_UNAVAILABLE;
	} else if (g_str_equal(type, "unsubscribed")) {
		state = JABBER_BUDDY_STATE_UNKNOWN;
	} else {
		purple_debug_warning("jabber", "Ignoring presence with invalid type "
		                     "'%s'\n", type);
		jabber_id_free(jid);
		return;
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
		} else if(xmlns == NULL) {
			/* The rest of the cases used to check xmlns individually. */
			continue;
		} else if(!strcmp(y->name, "delay") && !strcmp(xmlns, NS_DELAYED_DELIVERY)) {
			/* XXX: compare the time.  jabber:x:delay can happen on presence packets that aren't really and truly delayed */
			delayed = TRUE;
			stamp = xmlnode_get_attrib(y, "stamp");
		} else if(!strcmp(y->name, "c") && !strcmp(xmlns, "http://jabber.org/protocol/caps")) {
			caps = y; /* store for later, when creating buddy resource */
		} else if (g_str_equal(y->name, "nick") && g_str_equal(xmlns, "http://jabber.org/protocol/nick")) {
			nickname = xmlnode_get_data(y);
		} else if(!strcmp(y->name, "x")) {
			if(!strcmp(xmlns, NS_DELAYED_DELIVERY_LEGACY)) {
				/* XXX: compare the time.  jabber:x:delay can happen on presence packets that aren't really and truly delayed */
				delayed = TRUE;
				stamp = xmlnode_get_attrib(y, "stamp");
			} else if(!strcmp(xmlns, "http://jabber.org/protocol/muc#user")) {
			} else if(!strcmp(xmlns, "vcard-temp:x:update")) {
				xmlnode *photo = xmlnode_get_child(y, "photo");
				if(photo) {
					g_free(avatar_hash);
					avatar_hash = xmlnode_get_data(photo);
				}
			}
		} else if (!strcmp(y->name, "query") &&
			!strcmp(xmlnode_get_namespace(y), NS_LAST_ACTIVITY)) {
			/* resource has specified idle */
			const gchar *seconds = xmlnode_get_attrib(y, "seconds");
			if (seconds) {
				/* we may need to take "delayed" into account here */
				idle = atoi(seconds);
			}
		}
	}

	if (idle && delayed && stamp) {
		/* if we have a delayed presence, we need to add the delay to the idle
		 value */
		time_t offset = time(NULL) - purple_str_to_time(stamp, TRUE, NULL, NULL,
			NULL);
		purple_debug_info("jabber", "got delay %s yielding %ld s offset\n",
			stamp, offset);
		idle += offset;
	}

	if(jid->node && (chat = jabber_chat_find(js, jid->node, jid->domain))) {
		static int i = 1;

		if(state == JABBER_BUDDY_STATE_ERROR) {
			char *title, *msg = jabber_parse_error(js, packet, NULL);

			if (!chat->conv) {
				title = g_strdup_printf(_("Error joining chat %s"), from);
				purple_serv_got_join_chat_failed(js->gc, chat->components);
			} else {
				title = g_strdup_printf(_("Error in chat %s"), from);
				if (g_hash_table_size(chat->members) == 0)
					serv_got_chat_left(js->gc, chat->id);
			}
			purple_notify_error(js->gc, title, title, msg);
			g_free(title);
			g_free(msg);

			if (g_hash_table_size(chat->members) == 0)
				/* Only destroy the chat if the error happened while joining */
				jabber_chat_destroy(chat);
			jabber_id_free(jid);
			g_free(status);
			g_free(avatar_hash);
			g_free(nickname);
			return;
		}

		if (type == NULL) {
			xmlnode *x;
			const char *real_jid = NULL;
			const char *affiliation = NULL;
			const char *role = NULL;
			gboolean is_our_resource = FALSE; /* Is the presence about us? */

			/*
			 * XEP-0045 mandates the presence to include a resource (which is
			 * treated as the chat nick). Some non-compliant servers allow
			 * joining without a nick.
			 */
			if (!jid->resource) {
				jabber_id_free(jid);
				g_free(avatar_hash);
				g_free(nickname);
				g_free(status);
				return;
			}

			x = xmlnode_get_child_with_namespace(packet, "x",
					"http://jabber.org/protocol/muc#user");
			if (x) {
				xmlnode *status_node;
				xmlnode *item_node;

				for (status_node = xmlnode_get_child(x, "status"); status_node;
						status_node = xmlnode_get_next_twin(status_node)) {
					const char *code = xmlnode_get_attrib(status_node, "code");
					if (!code)
						continue;

					if (g_str_equal(code, "110")) {
						is_our_resource = TRUE;
					} else if (g_str_equal(code, "201")) {
						if ((chat = jabber_chat_find(js, jid->node, jid->domain))) {
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
					} else if (g_str_equal(code, "210")) {
						/* server rewrote room-nick */
						if((chat = jabber_chat_find(js, jid->node, jid->domain))) {
							g_free(chat->handle);
							chat->handle = g_strdup(jid->resource);
						}
					}
				}

				item_node = xmlnode_get_child(x, "item");
				if (item_node) {
					real_jid    = xmlnode_get_attrib(item_node, "jid");
					affiliation = xmlnode_get_attrib(item_node, "affiliation");
					role        = xmlnode_get_attrib(item_node, "role");

					if (purple_strequal(affiliation, "owner"))
						flags |= PURPLE_CBFLAGS_FOUNDER;
					if (role) {
						if (g_str_equal(role, "moderator"))
							flags |= PURPLE_CBFLAGS_OP;
						else if (g_str_equal(role, "participant"))
							flags |= PURPLE_CBFLAGS_VOICE;
					}
				}
			}

			if(!chat->conv) {
				char *room_jid = g_strdup_printf("%s@%s", jid->node, jid->domain);
				chat->id = i++;
				chat->muc = (x != NULL);
				chat->conv = serv_got_joined_chat(js->gc, chat->id, room_jid);
				purple_conv_chat_set_nick(PURPLE_CONV_CHAT(chat->conv), chat->handle);

				jabber_chat_disco_traffic(chat);
				g_free(room_jid);
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
		} else if (g_str_equal(type, "unavailable")) {
			xmlnode *x;
			gboolean nick_change = FALSE;
			gboolean kick = FALSE;
			gboolean is_our_resource = FALSE; /* Is the presence about us? */

			/* If the chat nick is invalid, we haven't yet joined, or we've
			 * already left (it was probably us leaving after we closed the
			 * chat), we don't care.
			 */
			if (!jid->resource || !chat->conv || chat->left) {
				if (chat->left &&
						jid->resource && chat->handle && !strcmp(jid->resource, chat->handle))
					jabber_chat_destroy(chat);
				jabber_id_free(jid);
				g_free(status);
				g_free(avatar_hash);
				g_free(nickname);
				return;
			}

			is_our_resource = (0 == g_utf8_collate(jid->resource, chat->handle));

			jabber_buddy_remove_resource(jb, jid->resource);

			x = xmlnode_get_child_with_namespace(packet, "x",
					"http://jabber.org/protocol/muc#user");
			if (chat->muc && x) {
				const char *nick;
				const char *item_jid = NULL;
				const char *to;
				xmlnode *stat;
				xmlnode *item;

				item = xmlnode_get_child(x, "item");
				if (item)
					item_jid = xmlnode_get_attrib(item, "jid");

				for (stat = xmlnode_get_child(x, "status"); stat;
						stat = xmlnode_get_next_twin(stat)) {
					const char *code = xmlnode_get_attrib(stat, "code");

					if (!code)
						continue;

					if (g_str_equal(code, "110")) {
						is_our_resource = TRUE;
					} else if(!strcmp(code, "301")) {
						/* XXX: we got banned */
					} else if(!strcmp(code, "303") && item &&
							(nick = xmlnode_get_attrib(item, "nick"))) {
						nick_change = TRUE;
						if(!strcmp(jid->resource, chat->handle)) {
							g_free(chat->handle);
							chat->handle = g_strdup(nick);
						}

						/* TODO: This should probably be moved out of the loop */
						purple_conv_chat_rename_user(PURPLE_CONV_CHAT(chat->conv), jid->resource, nick);
						jabber_chat_remove_handle(chat, jid->resource);
						continue;
					} else if(!strcmp(code, "307")) {
						/* Someone was kicked from the room */
						xmlnode *reason = NULL, *actor = NULL;
						const char *actor_name = NULL;
						char *reason_text = NULL;
						char *tmp;

						kick = TRUE;

						if (item) {
							reason = xmlnode_get_child(item, "reason");
							actor = xmlnode_get_child(item, "actor");

							if (reason != NULL)
								reason_text = xmlnode_get_data(reason);
							if (actor != NULL)
								actor_name = xmlnode_get_attrib(actor, "jid");
						}

						if (reason_text == NULL)
							reason_text = g_strdup(_("No reason"));

						if (is_our_resource) {
							if (actor_name != NULL)
								tmp = g_strdup_printf(_("You have been kicked by %s: (%s)"),
										actor_name, reason_text);
							else
								tmp = g_strdup_printf(_("You have been kicked: (%s)"),
										reason_text);
						} else {
							if (actor_name != NULL)
								tmp = g_strdup_printf(_("Kicked by %s (%s)"),
										actor_name, reason_text);
							else
								tmp = g_strdup_printf(_("Kicked (%s)"),
										reason_text);
						}

						g_free(reason_text);
						g_free(status);
						status = tmp;
					} else if(!strcmp(code, "321")) {
						/* XXX: removed due to an affiliation change */
					} else if(!strcmp(code, "322")) {
						/* XXX: removed because room is now members-only */
					} else if(!strcmp(code, "332")) {
						/* XXX: removed due to system shutdown */
					}
				}

				/*
				 * Possibly another connected resource of our JID (see XEP-0045
				 * v1.24 section 7.1.10) being disconnected. Should be
				 * distinguished by the item_jid.
				 * Also possibly works around bits of an Openfire bug. See
				 * #8319.
				 */
				to = xmlnode_get_attrib(packet, "to");
				if (is_our_resource && item_jid && !purple_strequal(to, item_jid)) {
					/* TODO: When the above is a loop, this needs to still act
					 * sanely for all cases (this code is a little fragile). */
					if (!kick && !nick_change)
						/* Presumably, kicks and nick changes also affect us. */
						is_our_resource = FALSE;
				}
			}
			if(!nick_change) {
				if (is_our_resource) {
					if (kick)
						purple_conv_chat_write(PURPLE_CONV_CHAT(chat->conv), jid->resource,
								status, PURPLE_MESSAGE_SYSTEM, time(NULL));

					serv_got_chat_left(js->gc, chat->id);
					jabber_chat_destroy(chat);
				} else {
					purple_conv_chat_remove_user(PURPLE_CONV_CHAT(chat->conv), jid->resource,
							status);
					jabber_chat_remove_handle(chat, jid->resource);
				}
			}
		} else {
			/* A type that isn't available or unavailable */
			purple_debug_error("jabber", "MUC presence with bad type: %s\n",
			                   type);

			jabber_id_free(jid);
			g_free(avatar_hash);
			g_free(status);
			g_free(nickname);
			g_return_if_reached();
		}
	} else {
		buddy_name = g_strdup_printf("%s%s%s", jid->node ? jid->node : "",
									 jid->node ? "@" : "", jid->domain);
		if((b = purple_find_buddy(js->gc->account, buddy_name)) == NULL) {
			if (jb != js->user_jb) {
				purple_debug_warning("jabber", "Got presence for unknown buddy %s on account %s (%p)\n",
									 buddy_name, purple_account_get_username(js->gc->account), js->gc->account);
				jabber_id_free(jid);
				g_free(avatar_hash);
				g_free(buddy_name);
				g_free(nickname);
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
				(type && (g_str_equal(type, "unavailable") ||
				          g_str_equal(type, "unsubscribed")))) {
			PurpleConversation *conv;

			jabber_buddy_remove_resource(jb, jid->resource);
			if((conv = jabber_find_unnormalized_conv(from, js->gc->account)))
				purple_conversation_set_name(conv, buddy_name);

		} else {
			jbr = jabber_buddy_track_resource(jb, jid->resource, priority,
					state, status);
			if (idle) {
				jbr->idle = time(NULL) - idle;
			} else {
				jbr->idle = 0;
			}
		}

		if((found_jbr = jabber_buddy_find_resource(jb, NULL))) {
			jabber_google_presence_incoming(js, buddy_name, found_jbr);
			purple_prpl_got_user_status(js->gc->account, buddy_name, jabber_buddy_state_get_status_id(found_jbr->state), "priority", found_jbr->priority, "message", found_jbr->status, NULL);
			purple_prpl_got_user_idle(js->gc->account, buddy_name, found_jbr->idle, found_jbr->idle);
			if (nickname)
				serv_got_alias(js->gc, buddy_name, nickname);
		} else {
			purple_prpl_got_user_status(js->gc->account, buddy_name, "offline", status ? "message" : NULL, status, NULL);
		}
		g_free(buddy_name);
	}

	if (caps && !type) {
		/* handle Entity Capabilities (XEP-0115) */
		const char *node = xmlnode_get_attrib(caps, "node");
		const char *ver  = xmlnode_get_attrib(caps, "ver");
		const char *hash = xmlnode_get_attrib(caps, "hash");
		const char *ext  = xmlnode_get_attrib(caps, "ext");

		/* v1.3 uses: node, ver, and optionally ext.
		 * v1.5 uses: node, ver, and hash. */
		if (node && *node && ver && *ver) {
			gchar **exts = ext && *ext ? g_strsplit(ext, " ", -1) : NULL;
			jbr = jabber_buddy_find_resource(jb, jid->resource);

			/* Look it up if we don't already have all this information */
			if (!jbr || !jbr->caps.info ||
					!g_str_equal(node, jbr->caps.info->tuple.node) ||
					!g_str_equal(ver, jbr->caps.info->tuple.ver) ||
					!purple_strequal(hash, jbr->caps.info->tuple.hash) ||
					!jabber_caps_exts_known(jbr->caps.info, (gchar **)exts)) {
				JabberPresenceCapabilities *userdata = g_new0(JabberPresenceCapabilities, 1);
				userdata->js = js;
				userdata->jb = jb;
				userdata->from = g_strdup(from);
				jabber_caps_get_info(js, from, node, ver, hash, exts,
				    (jabber_caps_get_info_cb)jabber_presence_set_capabilities,
				    userdata);
			} else {
				if (exts)
					g_strfreev(exts);
			}
		}
	}

	g_free(nickname);
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
			if(formatted_msg && *formatted_msg)
				*msg = purple_markup_strip_html(formatted_msg);
		}

		if(priority)
			*priority = purple_status_get_attr_int(status, "priority");
	}
}
