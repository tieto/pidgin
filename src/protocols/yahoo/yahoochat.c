/*
 * gaim
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "debug.h"
#include "prpl.h"

#include "conversation.h"
#include "notify.h"
#include "util.h"
#include "multi.h"
#include "internal.h"

#include "yahoo.h"
#include "yahoochat.h"

#define YAHOO_CHAT_ID (1)

/* special function to log us on to the yahoo chat service */
static void yahoo_chat_online(GaimConnection *gc)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt;


	pkt = yahoo_packet_new(YAHOO_SERVICE_CHATONLINE, YAHOO_STATUS_AVAILABLE,0);
	yahoo_packet_hash(pkt, 1, gaim_connection_get_display_name(gc));
	yahoo_packet_hash(pkt, 109, gaim_connection_get_display_name(gc));
	yahoo_packet_hash(pkt, 6, "abcde");

	yahoo_send_packet(yd, pkt);

	yahoo_packet_free(pkt);
}

static gint _mystrcmpwrapper(gconstpointer a, gconstpointer b)
{
	return strcmp(a, b);
}

/* this is slow, and different from the gaim_* version in that it (hopefully) won't add a user twice */
static void yahoo_chat_add_users(GaimChat *chat, GList *newusers)
{
	GList *users, *i, *j;

	users = gaim_chat_get_users(chat);

	for (i = newusers; i; i = i->next) {
		j = g_list_find_custom(users, i->data, _mystrcmpwrapper);
		if (j)
			continue;
		gaim_chat_add_user(chat, i->data, NULL);
	}
}

static void yahoo_chat_add_user(GaimChat *chat, const char *user, const char *reason)
{
	GList *users;

	users = gaim_chat_get_users(chat);

	if ((g_list_find_custom(users, user, _mystrcmpwrapper)))
		return;

	gaim_chat_add_user(chat, user, reason);
}

static GaimConversation *yahoo_find_conference(GaimConnection *gc, const char *name)
{
	struct yahoo_data *yd;
	GSList *l;

	yd = gc->proto_data;

	for (l = yd->confs; l; l = l->next) {
		GaimConversation *c = l->data;
		if (!gaim_utf8_strcasecmp(gaim_conversation_get_name(c), name))
			return c;
	}
	return NULL;
}


void yahoo_process_conference_invite(GaimConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l;
	char *room = NULL;
	char *who = NULL;
	char *msg = NULL;
	GString *members = NULL;
	GHashTable *components;


	if (pkt->status == 2)
		return; /* XXX */

	members = g_string_sized_new(512);

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 1: /* us, but we already know who we are */
			break;
		case 57:
			room = pair->value;
			break;
		case 50: /* inviter */
			who = pair->value;
			g_string_append_printf(members, "%s\n", who);
			break;
		case 52: /* members */
			g_string_append_printf(members, "%s\n", pair->value);
			break;
		case 58:
			msg = pair->value;
			break;
		case 13: /* ? */
			break;
		}
	}

	if (!room) {
		g_string_free(members, TRUE);
		return;
	}

	components = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	g_hash_table_replace(components, g_strdup("room"), g_strdup(room));
	if (msg)
		g_hash_table_replace(components, g_strdup("topic"), g_strdup(msg));
	g_hash_table_replace(components, g_strdup("type"), g_strdup("Conference"));
	if (members) {
		g_hash_table_replace(components, g_strdup("members"), g_strdup(members->str));
	}
	serv_got_chat_invite(gc, room, who, msg, components);

	g_string_free(members, TRUE);
}

void yahoo_process_conference_decline(GaimConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l;
	char *room = NULL;
	char *who = NULL;
	char *msg = NULL;

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 57:
			room = pair->value;
			break;
		case 54:
			who = pair->value;
			break;
		case 14:
			msg = pair->value;
			break;
		}
	}

	if (who && room) {
		char *tmp;

		tmp = g_strdup_printf(_("%s declined your conference invitation to room \"%s\" because \"%s\"."),
						who, room, msg?msg:"");
		gaim_notify_info(gc, NULL, _("Invitation Rejected"), tmp);
		g_free(tmp);
	}
}

void yahoo_process_conference_logon(GaimConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l;
	char *room = NULL;
	char *who = NULL;
	GaimConversation *c;

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 57:
			room = pair->value;
			break;
		case 53:
			who = pair->value;
			break;
		}
	}

	if (who && room) {
		c = yahoo_find_conference(gc, room);
		if (c)
			yahoo_chat_add_user(GAIM_CHAT(c), who, NULL);
	}
}

void yahoo_process_conference_logoff(GaimConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l;
	char *room = NULL;
	char *who = NULL;
	GaimConversation *c;

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 57:
			room = pair->value;
			break;
		case 56:
			who = pair->value;
			break;
		}
	}

	if (who && room) {
		c = yahoo_find_conference(gc, room);
		if (c)
			gaim_chat_remove_user(GAIM_CHAT(c), who, NULL);
	}
}

void yahoo_process_conference_message(GaimConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l;
	char *room = NULL;
	char *who = NULL;
	char *msg = NULL;
	GaimConversation *c;

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 57:
			room = pair->value;
			break;
		case 3:
			who = pair->value;
			break;
		case 14:
			msg = pair->value;
			break;
		}
	}

		if (room && who && msg) {
			c = yahoo_find_conference(gc, room);
			if (!c)
				return;
			msg = yahoo_codes_to_html(msg);
			serv_got_chat_in(gc, gaim_chat_get_id(GAIM_CHAT(c)), who, 0, msg, time(NULL));
			g_free(msg);
		}

}


/* this is a comfirmation of yahoo_chat_online(); */
void yahoo_process_chat_online(GaimConnection *gc, struct yahoo_packet *pkt)
{
	struct yahoo_data *yd = (struct yahoo_data *) gc->proto_data;

	if (pkt->status == 1)
		yd->chat_online = 1;
}

/* this is basicly the opposite of chat_online */
void yahoo_process_chat_logout(GaimConnection *gc, struct yahoo_packet *pkt)
{
	struct yahoo_data *yd = (struct yahoo_data *) gc->proto_data;

	if (pkt->status == 1)
		yd->chat_online = 0;
}

void yahoo_process_chat_join(GaimConnection *gc, struct yahoo_packet *pkt)
{
	struct yahoo_data *yd = (struct yahoo_data *) gc->proto_data;
	GaimConversation *c = NULL;
	GSList *l;
	GList *members = NULL;
	char *room = NULL;
	char *topic = NULL;
	char *someid, *someotherid, *somebase64orhashosomething, *somenegativenumber;

	if (pkt->status == -1) {
		gaim_notify_error(gc, NULL, _("Failed to join chat"), _("Maybe the room is full?"));
		return;
	}

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {

		case 104:
			room = pair->value;
			break;
		case 105:
			topic = pair->value;
			break;
		case 128:
			someid = pair->value;
			break;
		case 108: /* number of joiners */
			break;
		case 129:
			someotherid = pair->value;
			break;
		case 130:
			somebase64orhashosomething = pair->value;
			break;
		case 126:
			somenegativenumber = pair->value;
			break;
		case 13: /* this is 1. maybe its the type of room? (normal, user created, private, etc?) */
			break;
		case 61: /*this looks similiar to 130 */
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

	if (!room)
		return;

	if (yd->chat_name && gaim_utf8_strcasecmp(room, yd->chat_name))
		yahoo_c_leave(gc, YAHOO_CHAT_ID);

	c = gaim_find_chat(gc, YAHOO_CHAT_ID);

	if (!c) {
		c = serv_got_joined_chat(gc, YAHOO_CHAT_ID, room);
		if (topic)
			gaim_chat_set_topic(GAIM_CHAT(c), NULL, topic);
		yd->in_chat = 1;
		yd->chat_name = g_strdup(room);
		gaim_chat_add_users(GAIM_CHAT(c), members);
	} else {
		yahoo_chat_add_users(GAIM_CHAT(c), members);
	}

	g_list_free(members);
}

void yahoo_process_chat_exit(GaimConnection *gc, struct yahoo_packet *pkt)
{
	char *who = NULL;
	GSList *l;
	struct yahoo_data *yd;

	yd = gc->proto_data;

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		if (pair->key == 109)
			who = pair->value;
	}


	if (who) {
		GaimConversation *c = gaim_find_chat(gc, YAHOO_CHAT_ID);
		if (c)
			gaim_chat_remove_user(GAIM_CHAT(c), who, NULL);

	}
}

void yahoo_process_chat_message(GaimConnection *gc, struct yahoo_packet *pkt)
{
	char *room = NULL, *who = NULL, *msg = NULL;
	int msgtype = 1;
	GaimConversation *c = NULL;
	GSList *l;

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {

		case 104:
			room = pair->value;
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

	if (!who)
		return;

	c = gaim_find_chat(gc, YAHOO_CHAT_ID);
	if (!c) {
		/* we still get messages after we part, funny that */
		return;
	}

	if (!msg) {
		gaim_debug(GAIM_DEBUG_MISC, "yahoo", "Got a message packet with no message.\nThis probably means something important, but we're ignoring it.\n");
		return;
	}
	msg = yahoo_codes_to_html(msg);

	if (msgtype == 2 || msgtype == 3) {
		char *tmp;
		tmp = g_strdup_printf("/me %s", msg);
		g_free(msg);
		msg = tmp;
	}

	serv_got_chat_in(gc, YAHOO_CHAT_ID, who, 0, msg, time(NULL));
	g_free(msg);
}

void yahoo_process_chat_addinvite(GaimConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l;
	char *room = NULL;
	char *msg = NULL;
	char *who = NULL;


	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 104:
			room = pair->value;
			break;
		case 129: /* room id? */
			break;
		case 126: /* ??? */
			break;
		case 117:
			msg = pair->value;
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

		components = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
		g_hash_table_replace(components, g_strdup("room"), g_strdup(room));
		serv_got_chat_invite(gc, room, who, msg, components);
	}
}

void yahoo_process_chat_goto(GaimConnection *gc, struct yahoo_packet *pkt)
{
	if (pkt->status == -1)
		gaim_notify_error(gc, NULL, _("Failed to join buddy in chat"),
						_("Maybe they're not in a chat?"));
}


/*
 * Functions dealing with conferences
 */

static void yahoo_conf_leave(struct yahoo_data *yd, const char *room, const char *dn, GList *who)
{
	struct yahoo_packet *pkt;
	GList *w;


	pkt = yahoo_packet_new(YAHOO_SERVICE_CONFLOGOFF, YAHOO_STATUS_AVAILABLE, 0);

	yahoo_packet_hash(pkt, 1, dn);
	for (w = who; w; w = w->next) {
		yahoo_packet_hash(pkt, 3, (char *)w->data);
	}

	yahoo_packet_hash(pkt, 57, room);

	yahoo_send_packet(yd, pkt);

	yahoo_packet_free(pkt);
}

static int yahoo_conf_send(struct yahoo_data *yd, const char *dn, const char *room,
							GList *members, const char *what)
{
	struct yahoo_packet *pkt;
	GList *who;

	pkt = yahoo_packet_new(YAHOO_SERVICE_CONFMSG, YAHOO_STATUS_AVAILABLE, 0);

	yahoo_packet_hash(pkt, 1, dn);
	for (who = members; who; who = who->next)
		yahoo_packet_hash(pkt, 53, (char *)who->data);
	yahoo_packet_hash(pkt, 57, room);
	yahoo_packet_hash(pkt, 14, what);
	yahoo_packet_hash(pkt, 97, "1"); /* utf-8 */

	yahoo_send_packet(yd, pkt);

	yahoo_packet_free(pkt);

	return 0;
}

static void yahoo_conf_join(struct yahoo_data *yd, GaimConversation *c, const char *dn, const char *room,
						const char *topic, const char *members)
{
	struct yahoo_packet *pkt;
	char **memarr = NULL;
	int i;

	if (members)
		memarr = g_strsplit(members, "\n", 0);


	pkt = yahoo_packet_new(YAHOO_SERVICE_CONFLOGON, YAHOO_STATUS_AVAILABLE, 0);

	yahoo_packet_hash(pkt, 1, dn);
	yahoo_packet_hash(pkt, 3, dn);
	yahoo_packet_hash(pkt, 57, room);
	if (memarr) {
		for(i = 0 ; memarr[i]; i++) {
			if (!strcmp(memarr[i], "") || !strcmp(memarr[i], dn))
					continue;
			yahoo_packet_hash(pkt, 3, memarr[i]);
			gaim_chat_add_user(GAIM_CHAT(c), memarr[i], NULL);
		}
	}
	yahoo_send_packet(yd, pkt);

	yahoo_packet_free(pkt);

	if (memarr)
		g_strfreev(memarr);
}

static void yahoo_conf_invite(struct yahoo_data *yd, GaimConversation *c,
		const char *dn, const char *buddy, const char *room, const char *msg)
{
	struct yahoo_packet *pkt;
	GList *members;

	members = gaim_chat_get_users(GAIM_CHAT(c));

	pkt = yahoo_packet_new(YAHOO_SERVICE_CONFADDINVITE, YAHOO_STATUS_AVAILABLE, 0);

	yahoo_packet_hash(pkt, 1, dn);
	yahoo_packet_hash(pkt, 51, buddy);
	yahoo_packet_hash(pkt, 57, room);
	yahoo_packet_hash(pkt, 58, msg?msg:"");
	yahoo_packet_hash(pkt, 13, "0");
	for(; members; members = members->next) {
		if (!strcmp(members->data, dn))
			continue;
		yahoo_packet_hash(pkt, 52, (char *)members->data);
		yahoo_packet_hash(pkt, 53, (char *)members->data);
	}
	yahoo_send_packet(yd, pkt);

	yahoo_packet_free(pkt);
}

/*
 * Functions dealing with chats
 */

static void yahoo_chat_leave(struct yahoo_data *yd, const char *room, const char *dn)
{
	struct yahoo_packet *pkt;

	pkt = yahoo_packet_new(YAHOO_SERVICE_CHATEXIT, YAHOO_STATUS_AVAILABLE, 0);

	yahoo_packet_hash(pkt, 104, room);
	yahoo_packet_hash(pkt, 109, dn);
	yahoo_packet_hash(pkt, 108, "1");
	yahoo_packet_hash(pkt, 112, "0"); /* what does this one mean? */

	yahoo_send_packet(yd, pkt);

	yahoo_packet_free(pkt);

	yd->in_chat = 0;
	if (yd->chat_name) {
		g_free(yd->chat_name);
		yd->chat_name = NULL;
	}

}

static int yahoo_chat_send(struct yahoo_data *yd, const char *dn, const char *room, const char *what)
{
	struct yahoo_packet *pkt;
	const char *msg;
	int me = 0;

	if (!g_ascii_strncasecmp(what, "/me ", 4)) { /* XXX fix this to ignore leading html */
		me = 1;
		msg = what + 4;
	} else
		msg = what;

	pkt = yahoo_packet_new(YAHOO_SERVICE_COMMENT, YAHOO_STATUS_AVAILABLE, 0);

	yahoo_packet_hash(pkt, 1, dn);
	yahoo_packet_hash(pkt, 104, room);
	yahoo_packet_hash(pkt, 117, msg);
	if (me)
		yahoo_packet_hash(pkt, 124, "2");
	else
		yahoo_packet_hash(pkt, 124, "1");
	/* fixme: what about /think? (124=3) */

	yahoo_send_packet(yd, pkt);
	yahoo_packet_free(pkt);

	return 0;
}

static void yahoo_chat_join(struct yahoo_data *yd, const char *dn, const char *room, const char *topic)
{
	struct yahoo_packet *pkt;

	pkt = yahoo_packet_new(YAHOO_SERVICE_CHATJOIN, YAHOO_STATUS_AVAILABLE, 0);

	yahoo_packet_hash(pkt, 62, "2");
	yahoo_packet_hash(pkt, 104, room);
	yahoo_packet_hash(pkt, 129, "0");

	yahoo_send_packet(yd, pkt);

	yahoo_packet_free(pkt);
}

static void yahoo_chat_invite(struct yahoo_data *yd, const char *dn, const char *buddy,
							const char *room, const char *msg)
{
	struct yahoo_packet *pkt;

	pkt = yahoo_packet_new(YAHOO_SERVICE_CHATADDINVITE, YAHOO_STATUS_AVAILABLE, 0);

	yahoo_packet_hash(pkt, 1, dn);
	yahoo_packet_hash(pkt, 118, buddy);
	yahoo_packet_hash(pkt, 104, room);
	yahoo_packet_hash(pkt, 117, (msg?msg:""));
	yahoo_packet_hash(pkt, 129, "0");

	yahoo_send_packet(yd, pkt);
	yahoo_packet_free(pkt);
}

void yahoo_chat_goto(GaimConnection *gc, const char *name)
{
	struct yahoo_data *yd;
	struct yahoo_packet *pkt;

	yd = gc->proto_data;

	if (!yd->chat_online)
		yahoo_chat_online(gc);

	pkt = yahoo_packet_new(YAHOO_SERVICE_CHATGOTO, YAHOO_STATUS_AVAILABLE, 0);

	yahoo_packet_hash(pkt, 109, name);
	yahoo_packet_hash(pkt, 1, gaim_connection_get_display_name(gc));
	yahoo_packet_hash(pkt, 62, "2");

	yahoo_send_packet(yd, pkt);
	yahoo_packet_free(pkt);
}
/*
 * These are the functions registered with the core
 * which get called for both chats and conferences.
 */

void yahoo_c_leave(GaimConnection *gc, int id)
{
	struct yahoo_data *yd = (struct yahoo_data *) gc->proto_data;
	GaimConversation *c;

	if (!yd)
		return;


	c = gaim_find_chat(gc, id);
	if (!c)
		return;

	if (id != YAHOO_CHAT_ID) {
		yahoo_conf_leave(yd, gaim_conversation_get_name(c),
			gaim_connection_get_display_name(gc), gaim_chat_get_users(GAIM_CHAT(c)));
			yd->confs = g_slist_remove(yd->confs, c);
	} else {
		yahoo_chat_leave(yd, gaim_conversation_get_name(c), gaim_connection_get_display_name(gc));
	}

	serv_got_chat_left(gc, id);
}

int yahoo_c_send(GaimConnection *gc, int id, const char *what)
{
	GaimConversation *c;
	int ret;
	struct yahoo_data *yd;
	char *msg;

	yd = (struct yahoo_data *) gc->proto_data;
	if (!yd)
		return -1;

	c = gaim_find_chat(gc, id);
	if (!c)
		return -1;

	msg = yahoo_html_to_codes(what);

	if (id != YAHOO_CHAT_ID) {
		ret = yahoo_conf_send(yd, gaim_connection_get_display_name(gc),
				gaim_conversation_get_name(c), gaim_chat_get_users(GAIM_CHAT(c)), msg);
	} else {
		ret = yahoo_chat_send(yd, gaim_connection_get_display_name(gc),
						gaim_conversation_get_name(c), msg);
		if (!ret)
			serv_got_chat_in(gc, gaim_chat_get_id(GAIM_CHAT(c)),
					gaim_connection_get_display_name(gc), 0, what, time(NULL));
	}

	g_free(msg);
	return ret;
}

GList *yahoo_c_info(GaimConnection *gc)
{
	GList *m = NULL;
	struct proto_chat_entry *pce;

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("Room:");
	pce->identifier = "room";
	m = g_list_append(m, pce);

	return m;
}

void yahoo_c_join(GaimConnection *gc, GHashTable *data)
{
	struct yahoo_data *yd;
	char *room, *topic, *members, *type;
	int id;
	GaimConversation *c;

	yd = (struct yahoo_data *) gc->proto_data;
	if (!yd)
		return;

	room = g_hash_table_lookup(data, "room");
	if (!room)
		return;

	topic = g_hash_table_lookup(data, "topic");
	if (!topic)
		topic = "";

	members = g_hash_table_lookup(data, "members");


	if ((type = g_hash_table_lookup(data, "type")) && !strcmp(type, "Conference")) {
		id = yd->conf_id++;
		c = serv_got_joined_chat(gc, id, room);
		yd->confs = g_slist_prepend(yd->confs, c);
		gaim_chat_set_topic(GAIM_CHAT(c), gaim_connection_get_display_name(gc), topic);
		yahoo_conf_join(yd, c, gaim_connection_get_display_name(gc), room, topic, members);
		return;
	} else {
		if (yd->in_chat)
			yahoo_c_leave(gc, YAHOO_CHAT_ID);
		if (!yd->chat_online)
			yahoo_chat_online(gc);
		yahoo_chat_join(yd, gaim_connection_get_display_name(gc), room, topic);
		return;
	}
}

void yahoo_c_invite(GaimConnection *gc, int id, const char *msg, const char *name)
{
	struct yahoo_data *yd = (struct yahoo_data *) gc->proto_data;
	GaimConversation *c;

	c = gaim_find_chat(gc, id);
	if (!c || !c->name)
		return;

	if (id != YAHOO_CHAT_ID) {
		yahoo_conf_invite(yd, c, gaim_connection_get_display_name(gc), name,
							gaim_conversation_get_name(c), msg);
	} else {
		yahoo_chat_invite(yd, gaim_connection_get_display_name(gc), name,
							gaim_conversation_get_name(c), msg);
	}
}

