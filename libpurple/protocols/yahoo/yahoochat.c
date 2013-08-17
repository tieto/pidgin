/*
 * purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * Some code copyright 2003 Tim Ringenbach <omarvo@hotmail.com>
 * (marv on irc.freenode.net)
 * Some code borrowed from libyahoo2, copyright (C) 2002, Philip
 * S Tellis <philip . tellis AT gmx . net>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "debug.h"
#include "http.h"
#include "protocol.h"

#include "conversation.h"
#include "notify.h"
#include "util.h"

#include "libymsg.h"
#include "yahoo_packet.h"
#include "yahoochat.h"
#include "ycht.h"

#define YAHOO_CHAT_ID (1)

/* prototype(s) */
static void yahoo_chat_leave(PurpleConnection *gc, const char *room, const char *dn, gboolean logout);

/* special function to log us on to the yahoo chat service */
static void yahoo_chat_online(PurpleConnection *gc)
{
	YahooData *yd = purple_connection_get_protocol_data(gc);
	struct yahoo_packet *pkt;
	const char *rll;

	if (yd->wm) {
		ycht_connection_open(gc);
		return;
	}

	rll = purple_account_get_string(purple_connection_get_account(gc),
								  "room_list_locale", YAHOO_ROOMLIST_LOCALE);

	pkt = yahoo_packet_new(YAHOO_SERVICE_CHATONLINE, YAHOO_STATUS_AVAILABLE, yd->session_id);
	yahoo_packet_hash(pkt, "sssss",
					  109, purple_connection_get_display_name(gc),
					  1, purple_connection_get_display_name(gc),
					  6, "abcde",
					/* I'm not sure this is the correct way to set this. */
					  98, rll,
					  135, yd->jp ? YAHOO_CLIENT_VERSION : YAHOOJP_CLIENT_VERSION);
	yahoo_packet_send_and_free(pkt, yd);
}

/* this is slow, and different from the purple_* version in that it (hopefully) won't add a user twice */
void yahoo_chat_add_users(PurpleChatConversation *chat, GList *newusers)
{
	GList *i;

	for (i = newusers; i; i = i->next) {
		if (purple_chat_conversation_has_user(chat, i->data))
			continue;
		purple_chat_conversation_add_user(chat, i->data, NULL, PURPLE_CHAT_USER_NONE, TRUE);
	}
}

void yahoo_chat_add_user(PurpleChatConversation *chat, const char *user, const char *reason)
{
	if (purple_chat_conversation_has_user(chat, user))
		return;

	purple_chat_conversation_add_user(chat, user, reason, PURPLE_CHAT_USER_NONE, TRUE);
}

static PurpleChatConversation *yahoo_find_conference(PurpleConnection *gc, const char *name)
{
	YahooData *yd;
	GSList *l;

	yd = purple_connection_get_protocol_data(gc);

	for (l = yd->confs; l; l = l->next) {
		PurpleChatConversation *c = l->data;
		if (!purple_utf8_strcasecmp(purple_conversation_get_name(PURPLE_CONVERSATION(c)), name))
			return c;
	}
	return NULL;
}


void yahoo_process_conference_invite(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	PurpleAccount *account;
	GSList *l;
	char *room = NULL;
	char *who = NULL;
	char *msg = NULL;
	GString *members = NULL;
	GHashTable *components;

	if ( (pkt->status == 2) || (pkt->status == 11) )
		return; /* Status is 11 when we are being notified about invitation being sent to someone else */

	account = purple_connection_get_account(gc);

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;
		if (pair->key == 57)
		{
			room = yahoo_string_decode(gc, pair->value, FALSE);
			if (yahoo_find_conference(gc, room) != NULL)
			{
				/* Looks like we got invited to an already open conference. */
				/* Laters: Should we accept this conference rather than ignoring the invitation ? */
				purple_debug_info("yahoo","Ignoring invitation for an already existing chat, room:%s\n",room);
				g_free(room);
				return;
			}
		}
	}

	members = g_string_sized_new(512);

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 1: /* us, but we already know who we are */
			break;
		case 57:
			g_free(room);
			room = yahoo_string_decode(gc, pair->value, FALSE);
			break;
		case 50: /* inviter */
			who = pair->value;
			g_string_append_printf(members, "%s\n", who);
			break;
		case 51: /* This user is being invited to the conference. Comes with status = 11, so we wont reach here */
			break;
		case 52: /* Invited users. Assuming us invited, since we got this packet */
			break; /* break needed, or else we add the users to the conference before they accept the invitation */
		case 53: /* members who have already joined the conference */
			g_string_append_printf(members, "%s\n", pair->value);
			break;
		case 58:
			g_free(msg);
			msg = yahoo_string_decode(gc, pair->value, FALSE);
			break;
		case 13: /* ? */
			break;
		}
	}

	if (!room) {
		g_string_free(members, TRUE);
		g_free(msg);
		return;
	}

	if (!purple_account_privacy_check(account, who) ||
			(purple_account_get_bool(account, "ignore_invites", FALSE)))
	{
		purple_debug_info("yahoo",
		    "Invite to conference %s from %s has been dropped.\n", room, who);
		g_free(room);
		g_free(msg);
		g_string_free(members, TRUE);
		return;
	}

	components = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	g_hash_table_replace(components, g_strdup("room"), room);
	if (msg)
		g_hash_table_replace(components, g_strdup("topic"), msg);
	g_hash_table_replace(components, g_strdup("type"), g_strdup("Conference"));
	g_hash_table_replace(components, g_strdup("members"), g_string_free(members, FALSE));
	serv_got_chat_invite(gc, room, who, msg, components);

}

void yahoo_process_conference_decline(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l;
	char *room = NULL;
	char *who = NULL;
	char *msg = NULL;
	PurpleChatConversation *c = NULL;
	int utf8 = 0;

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 57:
			g_free(room);
			room = yahoo_string_decode(gc, pair->value, FALSE);
			break;
		case 54:
			who = pair->value;
			break;
		case 14:
			g_free(msg);
			msg = yahoo_string_decode(gc, pair->value, FALSE);
			break;
		case 97:
			utf8 = strtol(pair->value, NULL, 10);
			break;
		}
	}
	if (!purple_account_privacy_check(purple_connection_get_account(gc), who))
	{
		g_free(room);
		g_free(msg);
		return;
	}

	if (who && room) {
		/* make sure we're in the room before we process a decline message for it */
		if((c = yahoo_find_conference(gc, room))) {
			char *tmp = NULL, *msg_tmp = NULL;
			if(msg)
			{
				msg_tmp = yahoo_string_decode(gc, msg, utf8);
				msg = yahoo_codes_to_html(msg_tmp);
				serv_got_chat_in(gc, purple_chat_conversation_get_id(c), who, 0, msg, time(NULL));
				g_free(msg_tmp);
				g_free(msg);
			}

			tmp = g_strdup_printf(_("%s has declined to join."), who);
			purple_conversation_write(PURPLE_CONVERSATION(c), NULL, tmp,
					PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_NO_LINKIFY, time(NULL));

			g_free(tmp);
		}

		g_free(room);
	}
}

void yahoo_process_conference_logon(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l;
	char *room = NULL;
	char *who = NULL;
	PurpleChatConversation *c;

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 57:
			g_free(room);
			room = yahoo_string_decode(gc, pair->value, FALSE);
			break;
		case 53:
			who = pair->value;
			break;
		}
	}

	if (who && room) {
		c = yahoo_find_conference(gc, room);
		if (c)
		{	/* Prevent duplicate users in the chat */
			if( !purple_chat_conversation_has_user(c, who) )
				yahoo_chat_add_user(c, who, NULL);
		}
		g_free(room);
	}
}

void yahoo_process_conference_logoff(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l;
	char *room = NULL;
	char *who = NULL;
	PurpleChatConversation *c;

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 57:
			g_free(room);
			room = yahoo_string_decode(gc, pair->value, FALSE);
			break;
		case 56:
			who = pair->value;
			break;
		}
	}

	if (who && room) {
		c = yahoo_find_conference(gc, room);
		if (c)
			purple_chat_conversation_remove_user(c, who, NULL);
		g_free(room);
	}
}

void yahoo_process_conference_message(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l;
	char *room = NULL;
	char *who = NULL;
	char *msg = NULL;
	int utf8 = 0;
	PurpleChatConversation *c;

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 57:
			g_free(room);
			room = yahoo_string_decode(gc, pair->value, FALSE);
			break;
		case 3:
			who = pair->value;
			break;
		case 14:
			msg = pair->value;
			break;
		case 97:
			utf8 = strtol(pair->value, NULL, 10);
			break;
		}
	}

	if (room && who && msg) {
		char *msg2;

		c = yahoo_find_conference(gc, room);
		if (!c) {
			g_free(room);
			return;
		}

		msg2 = yahoo_string_decode(gc, msg, utf8);
		msg = yahoo_codes_to_html(msg2);
		serv_got_chat_in(gc, purple_chat_conversation_get_id(c), who, 0, msg, time(NULL));
		g_free(msg);
		g_free(msg2);
	}

	g_free(room);
}

static void yahoo_chat_join(PurpleConnection *gc, const char *dn, const char *room, const char *topic, const char *id)
{
	YahooData *yd = purple_connection_get_protocol_data(gc);
	struct yahoo_packet *pkt;
	char *room2;
	gboolean utf8 = TRUE;

	if (yd->wm) {
		g_return_if_fail(yd->ycht != NULL);
		ycht_chat_join(yd->ycht, room);
		return;
	}

	/* apparently room names are always utf8, or else always not utf8,
	 * so we don't have to actually pass the flag in the packet. Or something. */
	room2 = yahoo_string_encode(gc, room, &utf8);

	pkt = yahoo_packet_new(YAHOO_SERVICE_CHATJOIN, YAHOO_STATUS_AVAILABLE, yd->session_id);
	yahoo_packet_hash(pkt, "ssss",
						1, purple_connection_get_display_name(gc),
						104, room2,
						62, "2",
						129, id ? id : "0");
	yahoo_packet_send_and_free(pkt, yd);
	g_free(room2);
}

/* this is a confirmation of yahoo_chat_online(); */
void yahoo_process_chat_online(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	YahooData *yd = purple_connection_get_protocol_data(gc);

	if (pkt->status == 1) {
		yd->chat_online = TRUE;

		/* We need to goto a user in chat */
		if (yd->pending_chat_goto) {
			struct yahoo_packet *pkt = yahoo_packet_new(YAHOO_SERVICE_CHATGOTO, YAHOO_STATUS_AVAILABLE, yd->session_id);
			yahoo_packet_hash(pkt, "sss",
				109, yd->pending_chat_goto,
				1, purple_connection_get_display_name(gc),
				62, "2");
			yahoo_packet_send_and_free(pkt, yd);
		} else if (yd->pending_chat_room) {
			yahoo_chat_join(gc, purple_connection_get_display_name(gc), yd->pending_chat_room,
				yd->pending_chat_topic, yd->pending_chat_id);
		}

		g_free(yd->pending_chat_room);
		yd->pending_chat_room = NULL;
		g_free(yd->pending_chat_id);
		yd->pending_chat_id = NULL;
		g_free(yd->pending_chat_topic);
		yd->pending_chat_topic = NULL;
		g_free(yd->pending_chat_goto);
		yd->pending_chat_goto = NULL;
	}
}

/* this is basicly the opposite of chat_online */
void yahoo_process_chat_logout(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	YahooData *yd = purple_connection_get_protocol_data(gc);
	GSList *l;

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		if (pair->key == 1)
			if (g_ascii_strcasecmp(pair->value,
					purple_connection_get_display_name(gc)))
				return;
	}

	if (pkt->status == 1) {
		yd->chat_online = FALSE;
		g_free(yd->pending_chat_room);
		yd->pending_chat_room = NULL;
		g_free(yd->pending_chat_id);
		yd->pending_chat_id = NULL;
		g_free(yd->pending_chat_topic);
		yd->pending_chat_topic = NULL;
		g_free(yd->pending_chat_goto);
		yd->pending_chat_goto = NULL;
		if (yd->in_chat)
			yahoo_c_leave(gc, YAHOO_CHAT_ID);
	}
}

void yahoo_process_chat_join(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	YahooData *yd = purple_connection_get_protocol_data(gc);
	PurpleChatConversation *c = NULL;
	GSList *l;
	GList *members = NULL;
	GList *roomies = NULL;
	char *room = NULL;
	char *topic = NULL;

	if (pkt->status == -1) {
		/* We can't join */
		struct yahoo_pair *pair = pkt->hash->data;
		gchar const *failed_to_join = _("Failed to join chat");
		switch (atoi(pair->value)) {
			case 0xFFFFFFFA: /* -6 */
				purple_notify_error(gc, NULL, failed_to_join, _("Unknown room"));
				break;
			case 0xFFFFFFF1: /* -15 */
				purple_notify_error(gc, NULL, failed_to_join, _("Maybe the room is full"));
				break;
			case 0xFFFFFFDD: /* -35 */
				purple_notify_error(gc, NULL, failed_to_join, _("Not available"));
				break;
			default:
				purple_notify_error(gc, NULL, failed_to_join,
						_("Unknown error. You may need to logout and wait five minutes before being able to rejoin a chatroom"));
		}
		return;
	}

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {

		case 104:
			g_free(room);
			room = yahoo_string_decode(gc, pair->value, TRUE);
			break;
		case 105:
			g_free(topic);
			topic = yahoo_string_decode(gc, pair->value, TRUE);
			break;
		case 128: /* some id */
			break;
		case 108: /* number of joiners */
			break;
		case 129: /* some other id */
			break;
		case 130: /* some base64 or hash or something */
			break;
		case 126: /* some negative number */
			break;
		case 13: /* this is 1. maybe its the type of room? (normal, user created, private, etc?) */
			break;
		case 61: /*this looks similar to 130 */
			break;

		/* the previous section was just room info. this next section is
		   info about individual room members, (including us) */

		case 109: /* the yahoo id */
			members = g_list_append(members, pair->value);
			break;
		case 110: /* age */
			break;
		case 141: /* nickname */
			break;
		case 142: /* location */
			break;
		case 113: /* bitmask */
			break;
		}
	}

	if (room && yd->chat_name && purple_utf8_strcasecmp(room, yd->chat_name))
		yahoo_chat_leave(gc, room,
				purple_connection_get_display_name(gc), FALSE);

	c = purple_conversations_find_chat(gc, YAHOO_CHAT_ID);

	if (room && (!c || purple_chat_conversation_has_left(c)) &&
	    members && (members->next ||
	     !g_ascii_strcasecmp(members->data, purple_connection_get_display_name(gc)))) {
		GList *l;
		GList *flags = NULL;
		for (l = members; l; l = l->next)
			flags = g_list_prepend(flags, GINT_TO_POINTER(PURPLE_CHAT_USER_NONE));
		if (c && purple_chat_conversation_has_left(c)) {
			/* this might be a hack, but oh well, it should nicely */
			char *tmpmsg;

			purple_conversation_set_name(PURPLE_CONVERSATION(c), room);

			c = serv_got_joined_chat(gc, YAHOO_CHAT_ID, room);
			if (topic) {
				purple_chat_conversation_set_topic(c, NULL, topic);
				/* Also print the topic to the backlog so that the captcha link is clickable */
				purple_conversation_write_message(PURPLE_CONVERSATION(c), "", topic, PURPLE_MESSAGE_SYSTEM, time(NULL));
			}
			yd->in_chat = 1;
			yd->chat_name = g_strdup(room);
			purple_chat_conversation_add_users(c, members, NULL, flags, FALSE);

			tmpmsg = g_strdup_printf(_("You are now chatting in %s."), room);
			purple_conversation_write_message(PURPLE_CONVERSATION(c), "", tmpmsg, PURPLE_MESSAGE_SYSTEM, time(NULL));
			g_free(tmpmsg);
		} else {
			c = serv_got_joined_chat(gc, YAHOO_CHAT_ID, room);
			if (topic) {
				purple_chat_conversation_set_topic(c, NULL, topic);
				/* Also print the topic to the backlog so that the captcha link is clickable */
				purple_conversation_write_message(PURPLE_CONVERSATION(c), "", topic, PURPLE_MESSAGE_SYSTEM, time(NULL));
			}
			yd->in_chat = 1;
			yd->chat_name = g_strdup(room);
			purple_chat_conversation_add_users(c, members, NULL, flags, FALSE);
		}
		g_list_free(flags);
	} else if (c) {
		if (topic) {
			const char *cur_topic = purple_chat_conversation_get_topic(c);
			if (cur_topic == NULL || strcmp(cur_topic, topic) != 0)
				purple_chat_conversation_set_topic(c, NULL, topic);
		}
		yahoo_chat_add_users(c, members);
	}

	if (purple_account_privacy_get_denied(account) && c) {
		PurpleConversationUiOps *ops = purple_conversation_get_ui_ops(PURPLE_CONVERSATION(c));
		for (l = purple_account_privacy_get_denied(account); l != NULL; l = l->next) {
			for (roomies = members; roomies; roomies = roomies->next) {
				if (!purple_utf8_strcasecmp((char *)l->data, roomies->data)) {
					purple_debug_info("yahoo", "Ignoring room member %s in room %s\n" , (char *)roomies->data, room ? room : "");
					purple_chat_conversation_ignore(c,roomies->data);
					ops->chat_update_user(purple_chat_conversation_find_user(c, roomies->data));
				}
			}
		}
	}
	g_list_free(roomies);
	g_list_free(members);
	g_free(room);
	g_free(topic);
}

void yahoo_process_chat_exit(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	char *who = NULL;
	char *room = NULL;
	GSList *l;

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		if (pair->key == 104) {
			g_free(room);
			room = yahoo_string_decode(gc, pair->value, TRUE);
		}
		if (pair->key == 109)
			who = pair->value;
	}

	if (who && room) {
		PurpleChatConversation *c = purple_conversations_find_chat(gc, YAHOO_CHAT_ID);
		if (c && !purple_utf8_strcasecmp(purple_conversation_get_name(
				PURPLE_CONVERSATION(c)), room))
			purple_chat_conversation_remove_user(c, who, NULL);

	}
	g_free(room);
}

void yahoo_process_chat_message(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	char *room = NULL, *who = NULL, *msg = NULL, *msg2;
	int msgtype = 1, utf8 = 1; /* default to utf8 */
	PurpleChatConversation *c = NULL;
	GSList *l;

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {

		case 97:
			utf8 = strtol(pair->value, NULL, 10);
			break;
		case 104:
			g_free(room);
			room = yahoo_string_decode(gc, pair->value, TRUE);
			break;
		case 109:
			who = pair->value;
			break;
		case 117:
			msg = pair->value;
			break;
		case 124:
			msgtype = strtol(pair->value, NULL, 10);
			break;
		}
	}

	c = purple_conversations_find_chat(gc, YAHOO_CHAT_ID);
	if (!who || !c) {
		if (room)
			g_free(room);
		/* we still get messages after we part, funny that */
		return;
	}

	if (!msg) {
		purple_debug_misc("yahoo", "Got a message packet with no message.\nThis probably means something important, but we're ignoring it.\n");
		return;
	}
	msg2 = yahoo_string_decode(gc, msg, utf8);
	msg = yahoo_codes_to_html(msg2);
	g_free(msg2);

	if (msgtype == 2 || msgtype == 3) {
		char *tmp;
		tmp = g_strdup_printf("/me %s", msg);
		g_free(msg);
		msg = tmp;
	}

	serv_got_chat_in(gc, YAHOO_CHAT_ID, who, 0, msg, time(NULL));
	g_free(msg);
	g_free(room);
}

void yahoo_process_chat_addinvite(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	PurpleAccount *account;
	GSList *l;
	char *room = NULL;
	char *msg = NULL;
	char *who = NULL;

	account = purple_connection_get_account(gc);

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 104:
			g_free(room);
			room = yahoo_string_decode(gc, pair->value, TRUE);
			break;
		case 129: /* room id? */
			break;
		case 126: /* ??? */
			break;
		case 117:
			g_free(msg);
			msg = yahoo_string_decode(gc, pair->value, FALSE);
			break;
		case 119:
			who = pair->value;
			break;
		case 118: /* us */
			break;
		}
	}

	if (room && who) {
		GHashTable *components;

		if (!purple_account_privacy_check(account, who) ||
				(purple_account_get_bool(account, "ignore_invites", FALSE)))
		{
			purple_debug_info("yahoo", "Invite to room %s from %s has been dropped.\n", room, who);
			g_free(room);
			g_free(msg);
			return;
		}

		components = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
		g_hash_table_replace(components, g_strdup("room"), g_strdup(room));
		serv_got_chat_invite(gc, room, who, msg, components);
	}

	g_free(room);
	g_free(msg);
}

void yahoo_process_chat_goto(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	if (pkt->status == -1)
		purple_notify_error(gc, NULL, _("Failed to join buddy in chat"),
						_("Maybe they're not in a chat?"));
}

/*
 * Functions dealing with conferences
 * I think conference names are always ascii.
 */

void yahoo_conf_leave(YahooData *yd, const char *room, const char *dn, GList *who)
{
	struct yahoo_packet *pkt;
	GList *w;

	purple_debug_misc("yahoo", "leaving conference %s\n", room);

	pkt = yahoo_packet_new(YAHOO_SERVICE_CONFLOGOFF, YAHOO_STATUS_AVAILABLE, yd->session_id);

	yahoo_packet_hash_str(pkt, 1, dn);
	for (w = who; w; w = w->next) {
		const char *name = purple_chat_user_get_name(w->data);
		yahoo_packet_hash_str(pkt, 3, name);
	}

	yahoo_packet_hash_str(pkt, 57, room);
	yahoo_packet_send_and_free(pkt, yd);
}

static int yahoo_conf_send(PurpleConnection *gc, const char *dn, const char *room,
							GList *members, const char *what)
{
	YahooData *yd = purple_connection_get_protocol_data(gc);
	struct yahoo_packet *pkt;
	GList *who;
	char *msg, *msg2;
	int utf8 = 1;

	msg = yahoo_html_to_codes(what);
	msg2 = yahoo_string_encode(gc, msg, &utf8);

	pkt = yahoo_packet_new(YAHOO_SERVICE_CONFMSG, YAHOO_STATUS_AVAILABLE, yd->session_id);

	yahoo_packet_hash_str(pkt, 1, dn);
	for (who = members; who; who = who->next) {
		const char *name = purple_chat_user_get_name(who->data);
		yahoo_packet_hash_str(pkt, 53, name);
	}
	yahoo_packet_hash(pkt, "ss", 57, room, 14, msg2);
	if (utf8)
		yahoo_packet_hash_str(pkt, 97, "1"); /* utf-8 */

	yahoo_packet_send_and_free(pkt, yd);
	g_free(msg);
	g_free(msg2);

	return 0;
}

static void yahoo_conf_join(YahooData *yd, PurpleChatConversation *c, const char *dn, const char *room,
						const char *topic, const char *members)
{
	struct yahoo_packet *pkt;
	char **memarr = NULL;
	int i;

	if (members)
		memarr = g_strsplit(members, "\n", 0);

	pkt = yahoo_packet_new(YAHOO_SERVICE_CONFLOGON, YAHOO_STATUS_AVAILABLE, yd->session_id);

	yahoo_packet_hash(pkt, "sss", 1, dn, 3, dn, 57, room);
	if (memarr) {
		for(i = 0 ; memarr[i]; i++) {
			if (!strcmp(memarr[i], "") || !strcmp(memarr[i], dn))
					continue;
			yahoo_packet_hash_str(pkt, 3, memarr[i]);
			purple_chat_conversation_add_user(c, memarr[i], NULL, PURPLE_CHAT_USER_NONE, TRUE);
		}
	}
	yahoo_packet_send_and_free(pkt, yd);

	if (memarr)
		g_strfreev(memarr);
}

static void yahoo_conf_invite(PurpleConnection *gc, PurpleChatConversation *c,
		const char *dn, const char *buddy, const char *room, const char *msg)
{
	YahooData *yd = purple_connection_get_protocol_data(gc);
	struct yahoo_packet *pkt;
	GList *members;
	char *msg2 = NULL;

	if (msg)
		msg2 = yahoo_string_encode(gc, msg, NULL);

	members = purple_chat_conversation_get_users(c);

	pkt = yahoo_packet_new(YAHOO_SERVICE_CONFADDINVITE, YAHOO_STATUS_AVAILABLE, yd->session_id);

	yahoo_packet_hash(pkt, "sssss", 1, dn, 51, buddy, 57, room, 58, msg?msg2:"", 13, "0");
	for(; members; members = members->next) {
		const char *name = purple_chat_user_get_name(members->data);
		if (!strcmp(name, dn))
			continue;
		yahoo_packet_hash(pkt, "ss", 52, name, 53, name);
	}

	yahoo_packet_send_and_free(pkt, yd);
	g_free(msg2);
}

/*
 * Functions dealing with chats
 */

static void yahoo_chat_leave(PurpleConnection *gc, const char *room, const char *dn, gboolean logout)
{
	YahooData *yd = purple_connection_get_protocol_data(gc);
	struct yahoo_packet *pkt;

	char *eroom;
	gboolean utf8 = 1;

	if (yd->wm) {
		g_return_if_fail(yd->ycht != NULL);

		ycht_chat_leave(yd->ycht, room, logout);
		return;
	}

	eroom = yahoo_string_encode(gc, room, &utf8);

	pkt = yahoo_packet_new(YAHOO_SERVICE_CHATEXIT, YAHOO_STATUS_AVAILABLE, yd->session_id);
	yahoo_packet_hash(pkt, "sss", 104, eroom, 109, dn, 108, "1");
	yahoo_packet_hash_str(pkt, 112, "0"); /* what does this one mean? */
	yahoo_packet_send_and_free(pkt, yd);

	yd->in_chat = 0;
	if (yd->chat_name) {
		g_free(yd->chat_name);
		yd->chat_name = NULL;
	}

	if (purple_conversations_find_chat(gc, YAHOO_CHAT_ID) != NULL)
		serv_got_chat_left(gc, YAHOO_CHAT_ID);

	if (!logout)
		return;

	pkt = yahoo_packet_new(YAHOO_SERVICE_CHATLOGOUT,
			YAHOO_STATUS_AVAILABLE, yd->session_id);
	yahoo_packet_hash_str(pkt, 1, dn);
	yahoo_packet_send_and_free(pkt, yd);

	yd->chat_online = FALSE;
	g_free(yd->pending_chat_room);
	yd->pending_chat_room = NULL;
	g_free(yd->pending_chat_id);
	yd->pending_chat_id = NULL;
	g_free(yd->pending_chat_topic);
	yd->pending_chat_topic = NULL;
	g_free(yd->pending_chat_goto);
	yd->pending_chat_goto = NULL;
	g_free(eroom);
}

static int yahoo_chat_send(PurpleConnection *gc, const char *dn, const char *room, const char *what, PurpleMessageFlags flags)
{
	YahooData *yd = purple_connection_get_protocol_data(gc);
	struct yahoo_packet *pkt;
	int me = 0;
	char *msg1, *msg2, *room2;
	gboolean utf8 = TRUE;

	if (yd->wm) {
		g_return_val_if_fail(yd->ycht != NULL, 1);

		return ycht_chat_send(yd->ycht, room, what);
	}

	msg1 = g_strdup(what);

	if (purple_message_meify(msg1, -1))
		me = 1;

	msg2 = yahoo_html_to_codes(msg1);
	g_free(msg1);
	msg1 = yahoo_string_encode(gc, msg2, &utf8);
	g_free(msg2);
	room2 = yahoo_string_encode(gc, room, NULL);

	pkt = yahoo_packet_new(YAHOO_SERVICE_COMMENT, YAHOO_STATUS_AVAILABLE, yd->session_id);

	yahoo_packet_hash(pkt, "sss", 1, dn, 104, room2, 117, msg1);
	if (me)
		yahoo_packet_hash_str(pkt, 124, "2");
	else
		yahoo_packet_hash_str(pkt, 124, "1");
	/* fixme: what about /think? (124=3) */
	if (utf8)
		yahoo_packet_hash_str(pkt, 97, "1");

	yahoo_packet_send_and_free(pkt, yd);
	g_free(msg1);
	g_free(room2);

	return 0;
}


static void yahoo_chat_invite(PurpleConnection *gc, const char *dn, const char *buddy,
							const char *room, const char *msg)
{
	YahooData *yd = purple_connection_get_protocol_data(gc);
	struct yahoo_packet *pkt;
	char *room2, *msg2 = NULL;
	gboolean utf8 = TRUE;

	if (yd->wm) {
		g_return_if_fail(yd->ycht != NULL);
		ycht_chat_send_invite(yd->ycht, room, buddy, msg);
		return;
	}

	room2 = yahoo_string_encode(gc, room, &utf8);
	if (msg)
		msg2 = yahoo_string_encode(gc, msg, NULL);

	pkt = yahoo_packet_new(YAHOO_SERVICE_CHATADDINVITE, YAHOO_STATUS_AVAILABLE, yd->session_id);
	yahoo_packet_hash(pkt, "sssss", 1, dn, 118, buddy, 104, room2, 117, (msg2?msg2:""), 129, "0");
	yahoo_packet_send_and_free(pkt, yd);

	g_free(room2);
	g_free(msg2);
}

void yahoo_chat_goto(PurpleConnection *gc, const char *name)
{
	YahooData *yd;
	struct yahoo_packet *pkt;

	yd = purple_connection_get_protocol_data(gc);

	if (yd->wm) {
		g_return_if_fail(yd->ycht != NULL);
		ycht_chat_goto_user(yd->ycht, name);
		return;
	}

	if (!yd->chat_online) {
		yahoo_chat_online(gc);
		g_free(yd->pending_chat_room);
		yd->pending_chat_room = NULL;
		g_free(yd->pending_chat_id);
		yd->pending_chat_id = NULL;
		g_free(yd->pending_chat_topic);
		yd->pending_chat_topic = NULL;
		g_free(yd->pending_chat_goto);
		yd->pending_chat_goto = g_strdup(name);
		return;
	}

	pkt = yahoo_packet_new(YAHOO_SERVICE_CHATGOTO, YAHOO_STATUS_AVAILABLE, yd->session_id);
	yahoo_packet_hash(pkt, "sss", 109, name, 1, purple_connection_get_display_name(gc), 62, "2");
	yahoo_packet_send_and_free(pkt, yd);
}
/*
 * These are the functions registered with the core
 * which get called for both chats and conferences.
 */

void yahoo_c_leave(PurpleConnection *gc, int id)
{
	YahooData *yd = purple_connection_get_protocol_data(gc);
	PurpleChatConversation *c;

	if (!yd)
		return;

	c = purple_conversations_find_chat(gc, id);
	if (!c)
		return;

	if (id != YAHOO_CHAT_ID) {
		yahoo_conf_leave(yd, purple_conversation_get_name(PURPLE_CONVERSATION(c)),
			purple_connection_get_display_name(gc), purple_chat_conversation_get_users(c));
			yd->confs = g_slist_remove(yd->confs, c);
	} else {
		yahoo_chat_leave(gc, purple_conversation_get_name(PURPLE_CONVERSATION(c)),
				purple_connection_get_display_name(gc), TRUE);
	}

	serv_got_chat_left(gc, id);
}

int yahoo_c_send(PurpleConnection *gc, int id, const char *what, PurpleMessageFlags flags)
{
	PurpleChatConversation *c;
	int ret;
	YahooData *yd;

	yd = purple_connection_get_protocol_data(gc);
	if (!yd)
		return -1;

	c = purple_conversations_find_chat(gc, id);
	if (!c)
		return -1;

	if (id != YAHOO_CHAT_ID) {
		ret = yahoo_conf_send(gc, purple_connection_get_display_name(gc),
				purple_conversation_get_name(PURPLE_CONVERSATION(c)),
						purple_chat_conversation_get_users(c), what);
	} else {
		ret = yahoo_chat_send(gc, purple_connection_get_display_name(gc),
						purple_conversation_get_name(PURPLE_CONVERSATION(c)), what, flags);
		if (!ret)
			serv_got_chat_in(gc, purple_chat_conversation_get_id(c),
					purple_connection_get_display_name(gc), flags, what, time(NULL));
	}
	return ret;
}

GList *yahoo_c_info(PurpleConnection *gc)
{
	GList *m = NULL;
	struct proto_chat_entry *pce;

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("_Room:");
	pce->identifier = "room";
	pce->required = TRUE;
	m = g_list_append(m, pce);

	return m;
}

GHashTable *yahoo_c_info_defaults(PurpleConnection *gc, const char *chat_name)
{
	GHashTable *defaults;

	defaults = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);

	if (chat_name != NULL)
		g_hash_table_insert(defaults, "room", g_strdup(chat_name));

	return defaults;
}

char *yahoo_get_chat_name(GHashTable *data)
{
	return g_strdup(g_hash_table_lookup(data, "room"));
}

void yahoo_c_join(PurpleConnection *gc, GHashTable *data)
{
	YahooData *yd;
	char *room, *topic, *type;
	PurpleChatConversation *c;

	yd = purple_connection_get_protocol_data(gc);
	if (!yd)
		return;

	room = g_hash_table_lookup(data, "room");
	if (!room)
		return;

	topic = g_hash_table_lookup(data, "topic");
	if (!topic)
		topic = "";

	if ((type = g_hash_table_lookup(data, "type")) && !strcmp(type, "Conference")) {
		int id;
		const char *members = g_hash_table_lookup(data, "members");
		id = yd->conf_id++;
		c = serv_got_joined_chat(gc, id, room);
		yd->confs = g_slist_prepend(yd->confs, c);
		purple_chat_conversation_set_topic(c, purple_connection_get_display_name(gc), topic);
		yahoo_conf_join(yd, c, purple_connection_get_display_name(gc), room, topic, members);
		return;
	} else {
		const char *id;
		/*if (yd->in_chat)
			yahoo_chat_leave(gc, room,
					purple_connection_get_display_name(gc),
					FALSE);*/

		id = g_hash_table_lookup(data, "id");

		if (!yd->chat_online) {
			yahoo_chat_online(gc);
			g_free(yd->pending_chat_room);
			yd->pending_chat_room = g_strdup(room);
			g_free(yd->pending_chat_id);
			yd->pending_chat_id = g_strdup(id);
			g_free(yd->pending_chat_topic);
			yd->pending_chat_topic = g_strdup(topic);
			g_free(yd->pending_chat_goto);
			yd->pending_chat_goto = NULL;
		} else {
			yahoo_chat_join(gc, purple_connection_get_display_name(gc), room, topic, id);
		}
		return;
	}
}

void yahoo_c_invite(PurpleConnection *gc, int id, const char *msg, const char *name)
{
	PurpleChatConversation *c;

	c = purple_conversations_find_chat(gc, id);
	if (!c || !purple_conversation_get_name(PURPLE_CONVERSATION(c)))
		return;

	if (id != YAHOO_CHAT_ID) {
		yahoo_conf_invite(gc, c, purple_connection_get_display_name(gc), name,
							purple_conversation_get_name(PURPLE_CONVERSATION(c)), msg);
	} else {
		yahoo_chat_invite(gc, purple_connection_get_display_name(gc), name,
							purple_conversation_get_name(PURPLE_CONVERSATION(c)), msg);
	}
}

struct yahoo_roomlist {
	PurpleHttpConnection *hc;
	gchar *url;

	PurpleRoomlist *list;
	PurpleRoomlistRoom *cat;
	PurpleRoomlistRoom *ucat;
};

static void yahoo_roomlist_destroy(struct yahoo_roomlist *yrl)
{
	purple_http_conn_cancel(yrl->hc);
	g_free(yrl->url);

	g_free(yrl);
}

enum yahoo_room_type {
	yrt_yahoo,
	yrt_user
};

struct yahoo_chatxml_state {
	PurpleRoomlist *list;
	struct yahoo_roomlist *yrl;
	GQueue *q;
	struct {
		enum yahoo_room_type type;
		char *name;
		char *topic;
		char *id;
		int users, voices, webcams;
	} room;
};

struct yahoo_lobby {
	int count, users, voices, webcams;
};

static struct yahoo_chatxml_state *yahoo_chatxml_state_new(PurpleRoomlist *list, struct yahoo_roomlist *yrl)
{
	struct yahoo_chatxml_state *s;

	s = g_new0(struct yahoo_chatxml_state, 1);
	s->list = list;
	s->yrl = yrl;
	s->q = g_queue_new();

	return s;
}

static void yahoo_chatxml_state_destroy(struct yahoo_chatxml_state *s)
{
	g_queue_free(s->q);
	g_free(s->room.name);
	g_free(s->room.topic);
	g_free(s->room.id);
	g_free(s);
}

static void yahoo_chatlist_start_element(GMarkupParseContext *context,
                                  const gchar *ename, const gchar **anames,
                                  const gchar **avalues, gpointer user_data,
                                  GError **error)
{
	struct yahoo_chatxml_state *s = user_data;
	PurpleRoomlist *list = s->list;
	PurpleRoomlistRoom *r;
	PurpleRoomlistRoom *parent;
	int i;

	if (!strcmp(ename, "category")) {
		const gchar *name = NULL, *id = NULL;

		for (i = 0; anames[i]; i++) {
			if (!strcmp(anames[i], "id"))
				id = avalues[i];
			if (!strcmp(anames[i], "name"))
				name = avalues[i];
		}
		if (!name || !id)
			return;

		parent = g_queue_peek_head(s->q);
		r = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_CATEGORY, name, parent);
		purple_roomlist_room_add_field(list, r, (gpointer)name);
		purple_roomlist_room_add_field(list, r, (gpointer)id);
		purple_roomlist_room_add(list, r);
		g_queue_push_head(s->q, r);
	} else if (!strcmp(ename, "room")) {
		s->room.users = s->room.voices = s->room.webcams = 0;

		for (i = 0; anames[i]; i++) {
			if (!strcmp(anames[i], "id")) {
				g_free(s->room.id);
				s->room.id = g_strdup(avalues[i]);
			} else if (!strcmp(anames[i], "name")) {
				g_free(s->room.name);
				s->room.name = g_strdup(avalues[i]);
			} else if (!strcmp(anames[i], "topic")) {
				g_free(s->room.topic);
				s->room.topic = g_strdup(avalues[i]);
			} else if (!strcmp(anames[i], "type")) {
				if (!strcmp("yahoo", avalues[i]))
					s->room.type = yrt_yahoo;
				else
					s->room.type = yrt_user;
			}
		}

	} else if (!strcmp(ename, "lobby")) {
		struct yahoo_lobby *lob = g_new0(struct yahoo_lobby, 1);

		for (i = 0; anames[i]; i++) {
			if (!strcmp(anames[i], "count")) {
				lob->count = strtol(avalues[i], NULL, 10);
			} else if (!strcmp(anames[i], "users")) {
				s->room.users += lob->users = strtol(avalues[i], NULL, 10);
			} else if (!strcmp(anames[i], "voices")) {
				s->room.voices += lob->voices = strtol(avalues[i], NULL, 10);
			} else if (!strcmp(anames[i], "webcams")) {
				s->room.webcams += lob->webcams = strtol(avalues[i], NULL, 10);
			}
		}
		g_queue_push_head(s->q, lob);
	}
}

static void yahoo_chatlist_end_element(GMarkupParseContext *context, const gchar *ename,
                                       gpointer user_data, GError **error)
{
	struct yahoo_chatxml_state *s = user_data;

	if (!strcmp(ename, "category")) {
		g_queue_pop_head(s->q);
	} else if (!strcmp(ename, "room")) {
		struct yahoo_lobby *lob;
		PurpleRoomlistRoom *r, *l;

		if (s->room.type == yrt_yahoo)
			r = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_CATEGORY|PURPLE_ROOMLIST_ROOMTYPE_ROOM,
		                                   s->room.name, s->yrl->cat);
		else
			r = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_CATEGORY|PURPLE_ROOMLIST_ROOMTYPE_ROOM,
		                                   s->room.name, s->yrl->ucat);

		purple_roomlist_room_add_field(s->list, r, s->room.name);
		purple_roomlist_room_add_field(s->list, r, s->room.id);
		purple_roomlist_room_add_field(s->list, r, GINT_TO_POINTER(s->room.users));
		purple_roomlist_room_add_field(s->list, r, GINT_TO_POINTER(s->room.voices));
		purple_roomlist_room_add_field(s->list, r, GINT_TO_POINTER(s->room.webcams));
		purple_roomlist_room_add_field(s->list, r, s->room.topic);
		purple_roomlist_room_add(s->list, r);

		while ((lob = g_queue_pop_head(s->q))) {
			char *name = g_strdup_printf("%s:%d", s->room.name, lob->count);
			l = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_ROOM, name, r);

			purple_roomlist_room_add_field(s->list, l, name);
			purple_roomlist_room_add_field(s->list, l, s->room.id);
			purple_roomlist_room_add_field(s->list, l, GINT_TO_POINTER(lob->users));
			purple_roomlist_room_add_field(s->list, l, GINT_TO_POINTER(lob->voices));
			purple_roomlist_room_add_field(s->list, l, GINT_TO_POINTER(lob->webcams));
			purple_roomlist_room_add_field(s->list, l, s->room.topic);
			purple_roomlist_room_add(s->list, l);

			g_free(name);
			g_free(lob);
		}
	}
}

static GMarkupParser parser = {
	yahoo_chatlist_start_element,
	yahoo_chatlist_end_element,
	NULL,
	NULL,
	NULL
};

static void yahoo_roomlist_cleanup(PurpleRoomlist *list, struct yahoo_roomlist *yrl)
{
	purple_roomlist_set_in_progress(list, FALSE);

	if (yrl) {
		GList *proto_data = purple_roomlist_get_proto_data(list);
		proto_data = g_list_remove(proto_data, yrl);
		purple_roomlist_set_proto_data(list, proto_data);
		yahoo_roomlist_destroy(yrl);
	}

	purple_roomlist_unref(list);
}

static void
yahoo_roomlist_got(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, gpointer _yrl)
{
	struct yahoo_roomlist *yrl = _yrl;
	PurpleConnection *gc;
	GMarkupParseContext *parse;

	yrl->hc = NULL;

	gc = purple_account_get_connection(purple_roomlist_get_account(
		yrl->list));

	if (!purple_http_response_is_successful(response)) {
		purple_notify_error(gc, NULL, _("Unable to connect"),
			_("Fetching the room list failed."));
		yahoo_roomlist_cleanup(yrl->list, yrl);
		return;
	}

	parse = g_markup_parse_context_new(&parser, 0,
		yahoo_chatxml_state_new(yrl->list, yrl),
		(GDestroyNotify)yahoo_chatxml_state_destroy);

	if (!g_markup_parse_context_parse(parse,
		purple_http_response_get_data(response, NULL),
		purple_http_response_get_data_len(response), NULL))
	{
		purple_debug_error("yahoo", "Couldn't parse the room list\n");
		yahoo_roomlist_cleanup(yrl->list, yrl);
		return;
	}

	g_markup_parse_context_free(parse);
}

static void
yahoo_roomlist_make_request(struct yahoo_roomlist *yrl)
{
	PurpleRoomlist *list = yrl->list;
	PurpleAccount *account = purple_roomlist_get_account(list);
	PurpleConnection *pc = purple_account_get_connection(account);
	YahooData *yd = purple_connection_get_protocol_data(pc);
	PurpleHttpRequest *req;
	PurpleHttpCookieJar *cjar;
	PurpleConnection *gc;

	gc = purple_account_get_connection(purple_roomlist_get_account(
		yrl->list));

	req = purple_http_request_new(yrl->url);
	cjar = purple_http_request_get_cookie_jar(req);
	purple_http_cookie_jar_set(cjar, "Y", yd->cookie_y);
	purple_http_cookie_jar_set(cjar, "T", yd->cookie_t);

	yrl->hc = purple_http_request(gc, req, yahoo_roomlist_got, yrl);
	purple_http_request_unref(req);
}

PurpleRoomlist *yahoo_roomlist_get_list(PurpleConnection *gc)
{
	PurpleAccount *account;
	PurpleRoomlist *rl;
	PurpleRoomlistField *f;
	GList *fields = NULL;
	struct yahoo_roomlist *yrl;
	const char *rll, *rlurl;
	char *url;
	GList *proto_data;

	account = purple_connection_get_account(gc);

	/* for Yahoo Japan, it appears there is only one valid URL and locale */
	if(purple_account_get_bool(account, "yahoojp", FALSE)) {
		rll = YAHOOJP_ROOMLIST_LOCALE;
		rlurl = YAHOOJP_ROOMLIST_URL;
	}
	else { /* but for the rest of the world that isn't the case */
		rll = purple_account_get_string(account, "room_list_locale", YAHOO_ROOMLIST_LOCALE);
		rlurl = purple_account_get_string(account, "room_list", YAHOO_ROOMLIST_URL);
	}

	url = g_strdup_printf("%s?chatcat=0&intl=%s", rlurl, rll);

	yrl = g_new0(struct yahoo_roomlist, 1);
	rl = purple_roomlist_new(account);
	yrl->list = rl;

	yrl->url = url;

	f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, "", "room", TRUE);
	fields = g_list_append(fields, f);

	f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, "", "id", TRUE);
	fields = g_list_append(fields, f);

	f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_INT, _("Users"), "users", FALSE);
	fields = g_list_append(fields, f);

	f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_INT, _("Voices"), "voices", FALSE);
	fields = g_list_append(fields, f);

	f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_INT, _("Webcams"), "webcams", FALSE);
	fields = g_list_append(fields, f);

	f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, _("Topic"), "topic", FALSE);
	fields = g_list_append(fields, f);

	purple_roomlist_set_fields(rl, fields);

	proto_data = purple_roomlist_get_proto_data(rl);
	proto_data = g_list_append(proto_data, yrl);
	purple_roomlist_set_proto_data(rl, proto_data);

	yahoo_roomlist_make_request(yrl);
	purple_roomlist_set_in_progress(rl, TRUE);

	return rl;
}

void yahoo_roomlist_cancel(PurpleRoomlist *list)
{
	GList *l, *k;

	k = l = purple_roomlist_get_proto_data(list);
	purple_roomlist_set_proto_data(list, NULL);

	purple_roomlist_set_in_progress(list, FALSE);

	for (; l; l = l->next) {
		yahoo_roomlist_destroy(l->data);
		purple_roomlist_unref(list);
	}
	g_list_free(k);
}

void yahoo_roomlist_expand_category(PurpleRoomlist *list, PurpleRoomlistRoom *category)
{
	PurpleAccount *account;
	struct yahoo_roomlist *yrl;
	char *url;
	char *id;
	const char *rll;
	GList *proto_data;

	if (purple_roomlist_room_get_type(category) != PURPLE_ROOMLIST_ROOMTYPE_CATEGORY)
		return;

	if (!(id = g_list_nth_data(purple_roomlist_room_get_fields(category), 1))) {
		purple_roomlist_set_in_progress(list, FALSE);
		return;
	}

	account = purple_roomlist_get_account(list);
	rll = purple_account_get_string(account, "room_list_locale",
								  YAHOO_ROOMLIST_LOCALE);

	if (rll != NULL && *rll != '\0') {
		url = g_strdup_printf("%s?chatroom_%s=0&intl=%s",
	       purple_account_get_string(account,"room_list",
	       YAHOO_ROOMLIST_URL), id, rll);
	} else {
		url = g_strdup_printf("%s?chatroom_%s=0",
	       purple_account_get_string(account,"room_list",
	       YAHOO_ROOMLIST_URL), id);
	}

	yrl = g_new0(struct yahoo_roomlist, 1);
	yrl->list = list;
	yrl->cat = category;

	proto_data = purple_roomlist_get_proto_data(list);
	proto_data = g_list_append(proto_data, yrl);
	purple_roomlist_set_proto_data(list, proto_data);

	yrl->url = url;

	yrl->ucat = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_CATEGORY, _("User Rooms"), yrl->cat);
	purple_roomlist_room_add(list, yrl->ucat);

	yahoo_roomlist_make_request(yrl);
	purple_roomlist_set_in_progress(list, TRUE);
	purple_roomlist_ref(list);
}
