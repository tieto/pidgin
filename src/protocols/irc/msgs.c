/**
 * @file msgs.c
 * 
 * gaim
 *
 * Copyright (C) 2003, Ethan Blanton <eblanton@cs.purdue.edu>
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
 */

#include "internal.h"

/* XXX g_show_info_text */
#include "gaim.h"

#include "conversation.h"
#include "blist.h"
#include "notify.h"
#include "util.h"
#include "debug.h"
#include "irc.h"

#include <stdio.h>

static char *irc_mask_nick(const char *mask);
static char *irc_mask_userhost(const char *mask);
static void irc_chat_remove_buddy(GaimConversation *convo, char *data[2]);
static void irc_buddy_status(char *name, struct irc_buddy *ib, struct irc_conn *irc);

static char *irc_mask_nick(const char *mask)
{
	char *end, *buf;

	end = strchr(mask, '!');
	if (!end)
		buf = g_strdup(mask);
	else
		buf = g_strndup(mask, end - mask);

	return buf;
}

static char *irc_mask_userhost(const char *mask)
{
	return g_strdup(strchr(mask, '!') + 1);
}

static void irc_chat_remove_buddy(GaimConversation *convo, char *data[2])
{
	GList *users = gaim_chat_get_users(GAIM_CHAT(convo));
	char *message = g_strdup_printf("quit: %s", data[1]);

	if (g_list_find_custom(users, data[0], (GCompareFunc)(strcmp)))
		gaim_chat_remove_user(GAIM_CHAT(convo), data[0], message);

	g_free(message);
}

void irc_msg_default(struct irc_conn *irc, const char *name, const char *from, char **args)
{
	gaim_debug(GAIM_DEBUG_INFO, "irc", "Unrecognized message: %s\n", args[0]);
}

void irc_msg_away(struct irc_conn *irc, const char *name, const char *from, char **args)
{
	GaimConnection *gc;

	if (!args || !args[1])
		return;

	if (irc->whois.nick && !gaim_utf8_strcasecmp(irc->whois.nick, args[1])) {
		/* We're doing a whois, show this in the whois dialog */
		irc_msg_whois(irc, name, from, args);
		return;
	}

	gc = gaim_account_get_connection(irc->account);
	if (gc)
		serv_got_im(gc, args[1], args[2], IM_FLAG_AWAY, time(NULL), -1);
}

void irc_msg_badmode(struct irc_conn *irc, const char *name, const char *from, char **args)
{
	GaimConnection *gc = gaim_account_get_connection(irc->account);

	if (!args || !args[1] || !gc)
		return;

	gaim_notify_error(gc, NULL, _("Bad mode"), args[1]);
}

void irc_msg_banned(struct irc_conn *irc, const char *name, const char *from, char **args)
{
	GaimConnection *gc = gaim_account_get_connection(irc->account);
	char *buf;

	if (!args || !args[1] || !gc)
		return;

	buf = g_strdup_printf(_("You are banned from %s."), args[1]);
	gaim_notify_error(gc, _("Banned"), _("Banned"), buf);
	g_free(buf);
}

void irc_msg_chanmode(struct irc_conn *irc, const char *name, const char *from, char **args)
{
	GaimConversation *convo;
	char *buf;

	if (!args || !args[1] || !args[2])
		return;

	convo = gaim_find_conversation_with_account(args[1], irc->account);
	if (!convo)	/* XXX punt on channels we are not in for now */
		return;

	buf = g_strdup_printf("mode for %s: %s %s", args[1], args[2], args[3] ? args[3] : "");
	gaim_chat_write(GAIM_CHAT(convo), "", buf, WFLAG_SYSTEM|WFLAG_NOLOG, time(NULL));
	g_free(buf);

	return;
}

void irc_msg_whois(struct irc_conn *irc, const char *name, const char *from, char **args)
{
	if (!irc->whois.nick) {
		gaim_debug(GAIM_DEBUG_WARNING, "irc", "Unexpected WHOIS reply for %s\n", args[1]);
		return;
	}

	if (gaim_utf8_strcasecmp(irc->whois.nick, args[1])) {
		gaim_debug(GAIM_DEBUG_WARNING, "irc", "Got WHOIS reply for %s while waiting for %s\n", args[1], irc->whois.nick);
		return;
	}

	if (!strcmp(name, "301")) {
		irc->whois.away = g_strdup(args[2]);
	} else if (!strcmp(name, "311")) {
		irc->whois.userhost = g_strdup_printf("%s@%s", args[2], args[3]);
		irc->whois.name = g_strdup(args[5]);
	} else if (!strcmp(name, "312")) {
		irc->whois.server = g_strdup(args[2]);
		irc->whois.serverinfo = g_strdup(args[3]);
	} else if (!strcmp(name, "313")) {
		irc->whois.ircop = 1;
	} else if (!strcmp(name, "317")) {
		irc->whois.idle = atoi(args[2]);
		if (args[3])
			irc->whois.signon = (time_t)atoi(args[3]);
	} else if (!strcmp(name, "319")) {
		irc->whois.channels = g_strdup(args[2]);
	} else if (!strcmp(name, "320")) {
		irc->whois.identified = 1;
	}
}

void irc_msg_endwhois(struct irc_conn *irc, const char *name, const char *from, char **args)
{
	GaimConnection *gc;
	GString *info;
	char *str;
	int idle_days, idle_hours, idle_minutes, idle_seconds;

	if (!irc->whois.nick) {
		gaim_debug(GAIM_DEBUG_WARNING, "irc", "Unexpected End of WHOIS for %s\n", args[1]);
		return;
	}
	if (gaim_utf8_strcasecmp(irc->whois.nick, args[1])) {
		gaim_debug(GAIM_DEBUG_WARNING, "irc", "Received end of WHOIS for %s, expecting %s\n", args[1], irc->whois.nick);
		return;
	}

	info = g_string_new("");
	g_string_append_printf(info, "<b>%s:</b> %s%s%s<br>", _("Nick"), args[1],
			       irc->whois.ircop ? _(" <i>(ircop)</i>") : "",
			       irc->whois.identified ? _(" <i>(identified)</i>") : "");
	if (irc->whois.away) {
		g_string_append_printf(info, "<b>%s:</b> %s<br>", _("Away"), irc->whois.away);
		g_free(irc->whois.away);
	}
	if (irc->whois.userhost) {
		g_string_append_printf(info, "<b>%s:</b> %s<br>", _("Username"), irc->whois.userhost);
		g_string_append_printf(info, "<b>%s:</b> %s<br>", _("Realname"), irc->whois.name);
		g_free(irc->whois.userhost);
		g_free(irc->whois.name);
	}
	if (irc->whois.server) {
		g_string_append_printf(info, "<b>%s:</b> %s (%s)<br>", _("Server"), irc->whois.server, irc->whois.serverinfo);
		g_free(irc->whois.server);
		g_free(irc->whois.serverinfo);
	}
	if (irc->whois.channels) {
		g_string_append_printf(info, "<b>%s:</b> %s<br>", _("Currently on"), irc->whois.channels);
		g_free(irc->whois.channels);
	}
	if (irc->whois.idle) {
		idle_days=irc->whois.idle / 86400;
		idle_hours=(irc->whois.idle % 86400) / 3600;
		idle_minutes=(irc->whois.idle % 3600) / 60;
		idle_seconds=irc->whois.idle % 60;
		g_string_append_printf(info, ngettext(
				       "<b>Idle for:</b> %d day, %02d:%02d:%02d<br>",
				       "<b>Idle for:</b> %d days, %02d:%02d:%02d<br>",
				       idle_days),
				       idle_days, idle_hours, idle_minutes, idle_seconds);
		g_string_append_printf(info, "<b>%s:</b> %s", _("Online since"), ctime(&irc->whois.signon));
	}
	if (!strcmp(irc->whois.nick, "Paco-Paco")) {
		g_string_append_printf(info, _("<br><b>Defining adjective:</b> Glorious<br>"));
	}

	gc = gaim_account_get_connection(irc->account);
	str = g_string_free(info, FALSE);
	g_show_info_text(gc, irc->whois.nick, 2, str, NULL);
	g_free(str);
	memset(&irc->whois, 0, sizeof(irc->whois));
}

void irc_msg_topic(struct irc_conn *irc, const char *name, const char *from, char **args)
{
	char *chan, *topic, *msg, *nick;
	GaimConversation *convo;

	if (!strcmp(name, "topic")) {
		chan = args[0];
		topic = args[1];
	} else {
		chan = args[1];
		topic = args[2];
	}

	convo = gaim_find_conversation_with_account(chan, irc->account);
	if (!convo) {
		gaim_debug(GAIM_DEBUG_ERROR, "irc", "Got a topic for %s, which doesn't exist\n", chan);
	}
	gaim_chat_set_topic(GAIM_CHAT(convo), NULL, topic);
	/* If this is an interactive update, print it out */
	if (!strcmp(name, "topic")) {
		nick = irc_mask_nick(from);
		msg = g_strdup_printf(_("%s has changed the topic to: %s"), nick, topic);
		g_free(nick);
		gaim_chat_write(GAIM_CHAT(convo), from, msg, WFLAG_SYSTEM, time(NULL));
		g_free(msg);
	} else {
		msg = g_strdup_printf(_("The topic for %s is: %s"), chan, topic);
		gaim_chat_write(GAIM_CHAT(convo), "", msg, WFLAG_SYSTEM, time(NULL));
		g_free(msg);
	}
}

void irc_msg_unknown(struct irc_conn *irc, const char *name, const char *from, char **args)
{
	GaimConnection *gc = gaim_account_get_connection(irc->account);
	char *buf;

	if (!args || !args[1] || !gc)
		return;

	buf = g_strdup_printf(_("Unknown message '%s'"), args[1]);
	gaim_notify_error(gc, _("Unknown message"), buf, _("Gaim has sent a message the IRC server did not understand."));
	g_free(buf);
}

void irc_msg_names(struct irc_conn *irc, const char *name, const char *from, char **args)
{
	char *names, *cur, *end, *tmp, *msg;
	GaimConversation *convo;

	if (!strcmp(name, "366")) {
		convo = gaim_find_conversation_with_account(irc->nameconv ? irc->nameconv : args[1], irc->account);
		if (!convo) {
			gaim_debug(GAIM_DEBUG_ERROR, "irc", "Got a NAMES list for %s, which doesn't exist\n", args[2]);
			g_string_free(irc->names, TRUE);
			irc->names = NULL;
			g_free(irc->nameconv);
			irc->nameconv = NULL;
			return;
		}

		names = cur = g_string_free(irc->names, FALSE);
		irc->names = NULL;
		if (irc->nameconv) {
			msg = g_strdup_printf("Users on %s: %s", args[1], names);
			if (gaim_conversation_get_type(convo) == GAIM_CONV_CHAT)
				gaim_chat_write(GAIM_CHAT(convo), "", msg, WFLAG_SYSTEM|WFLAG_NOLOG, time(NULL));
			else
				gaim_im_write(GAIM_IM(convo), "", msg, -1, WFLAG_SYSTEM|WFLAG_NOLOG, time(NULL));
			g_free(msg);
			g_free(irc->nameconv);
			irc->nameconv = NULL;
		} else {
			while (*cur) {
				end = strchr(cur, ' ');
				if (!end)
					end = cur + strlen(cur);
				if (*cur == '@' || *cur == '%' || *cur == '+')
					cur++;
				tmp = g_strndup(cur, end - cur);
				gaim_chat_add_user(GAIM_CHAT(convo), tmp, NULL);
				g_free(tmp);
				cur = end;
				if (*cur)
					cur++;
			}
		}
		g_free(names);
	} else {
		if (!irc->names)
			irc->names = g_string_new("");

		irc->names = g_string_append(irc->names, args[3]);
	}
}

void irc_msg_motd(struct irc_conn *irc, const char *name, const char *from, char **args)
{
	GaimConnection *gc;
	if (!strcmp(name, "375")) {
		gc = gaim_account_get_connection(irc->account);
		if (gc)
			gaim_connection_set_display_name(gc, args[0]);
	}

	if (!irc->motd)
		irc->motd = g_string_new("");

	g_string_append_printf(irc->motd, "%s<br>", args[1]);
}

void irc_msg_endmotd(struct irc_conn *irc, const char *name, const char *from, char **args)
{
	GaimConnection *gc;

	gc = gaim_account_get_connection(irc->account);
	if (!gc)
		return;

	gaim_connection_set_state(gc, GAIM_CONNECTED);

	irc_blist_timeout(irc);
	irc->timer = g_timeout_add(45000, (GSourceFunc)irc_blist_timeout, (gpointer)irc);
}

void irc_msg_nonick(struct irc_conn *irc, const char *name, const char *from, char **args)
{
	GaimConnection *gc;
	GaimConversation *convo;

	convo = gaim_find_conversation_with_account(args[1], irc->account);
	if (convo) {
		if (gaim_conversation_get_type(convo) == GAIM_CONV_CHAT) /* does this happen? */
			gaim_chat_write(GAIM_CHAT(convo), args[1], _("no such channel"),
					WFLAG_SYSTEM|WFLAG_NOLOG, time(NULL));
		else
			gaim_im_write(GAIM_IM(convo), args[1], _("User is not logged in"), -1,
				      WFLAG_SYSTEM|WFLAG_NOLOG, time(NULL));
	} else {
		if ((gc = gaim_account_get_connection(irc->account)) == NULL)
			return;
		gaim_notify_error(gc, NULL, _("No such nick or channel"), args[1]);
	}

	if (irc->whois.nick && !gaim_utf8_strcasecmp(irc->whois.nick, args[1])) {
		g_free(irc->whois.nick);
		irc->whois.nick = NULL;
	}
}

void irc_msg_nosend(struct irc_conn *irc, const char *name, const char *from, char **args)
{
	GaimConnection *gc;
	GaimConversation *convo;

	convo = gaim_find_conversation_with_account(args[1], irc->account);
	if (convo) {
		gaim_chat_write(GAIM_CHAT(convo), args[1], args[2], WFLAG_SYSTEM|WFLAG_NOLOG, time(NULL));
	} else {
		if ((gc = gaim_account_get_connection(irc->account)) == NULL)
			return;
		gaim_notify_error(gc, NULL, _("Could not send"), args[2]);
	}
}

void irc_msg_notinchan(struct irc_conn *irc, const char *name, const char *from, char **args)
{
	GaimConversation *convo = gaim_find_conversation_with_account(args[1], irc->account);

	gaim_debug(GAIM_DEBUG_INFO, "irc", "We're apparently not in %s, but tried to use it\n", args[1]);
	if (convo) {
		/*g_slist_remove(irc->gc->buddy_chats, convo);
		  gaim_conversation_set_account(convo, NULL);*/
		gaim_chat_write(GAIM_CHAT(convo), args[1], args[2], WFLAG_SYSTEM|WFLAG_NOLOG, time(NULL));
	}
}

void irc_msg_notop(struct irc_conn *irc, const char *name, const char *from, char **args)
{
	GaimConversation *convo;

	if (!args || !args[1] || !args[2])
		return;

	convo = gaim_find_conversation_with_account(args[1], irc->account);
	if (!convo)
		return;

	gaim_chat_write(GAIM_CHAT(convo), "", args[2], WFLAG_SYSTEM, time(NULL));
}

void irc_msg_invite(struct irc_conn *irc, const char *name, const char *from, char **args)
{
	GaimConnection *gc = gaim_account_get_connection(irc->account);
	GHashTable *components = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	char *nick = irc_mask_nick(from);

	if (!args || !args[1] || !gc) {
		g_free(nick);
		g_hash_table_destroy(components);
		return;
	}

	g_hash_table_insert(components, strdup("channel"), strdup(args[1]));

	serv_got_chat_invite(gc, args[1], nick, NULL, components);
	g_free(nick);
}

void irc_msg_inviteonly(struct irc_conn *irc, const char *name, const char *from, char **args)
{
	GaimConnection *gc = gaim_account_get_connection(irc->account);
	char *buf;

	if (!args || !args[1] || !gc)
		return;

	buf = g_strdup_printf(_("Joining %s requires an invitation."), args[1]);
	gaim_notify_error(gc, _("Invitation only"), _("Invitation only"), buf);
	g_free(buf);
}

void irc_msg_ison(struct irc_conn *irc, const char *name, const char *from, char **args)
{
	char **nicks;
	struct irc_buddy *ib;
	int i;

	if (!args || !args[1])
		return;

	nicks = g_strsplit(args[1], " ", -1);

	for (i = 0; nicks[i]; i++) {
		if ((ib = g_hash_table_lookup(irc->buddies, (gconstpointer)nicks[i])) == NULL) {
			continue;
		}
		ib->flag = TRUE;
	}

	g_strfreev(nicks);

	g_hash_table_foreach(irc->buddies, (GHFunc)irc_buddy_status, (gpointer)irc);
}

static void irc_buddy_status(char *name, struct irc_buddy *ib, struct irc_conn *irc)
{
	GaimConnection *gc = gaim_account_get_connection(irc->account);
	struct buddy *buddy = gaim_find_buddy(irc->account, name);

	if (!gc || !buddy)
		return;

	if (ib->online && !ib->flag) {
		serv_got_update(gc, buddy->name, 0, 0, 0, 0, 0);
		ib->online = FALSE;
	}

	if (!ib->online && ib->flag) {
		serv_got_update(gc, buddy->name, 1, 0, 0, 0, 0);
		ib->online = TRUE;
	}
}

void irc_msg_join(struct irc_conn *irc, const char *name, const char *from, char **args)
{
	GaimConnection *gc = gaim_account_get_connection(irc->account);
	GaimConversation *convo;
	char *nick = irc_mask_nick(from), *userhost;
	static int id = 1;

	if (!gc) {
		g_free(nick);
		return;
	}

	if (!gaim_utf8_strcasecmp(nick, gaim_connection_get_display_name(gc))) {
		/* We are joining a channel for the first time */
		serv_got_joined_chat(gc, id++, args[0]);
		g_free(nick);
		return;
	}

	convo = gaim_find_conversation_with_account(args[0], irc->account);
	if (convo == NULL) {
		gaim_debug(GAIM_DEBUG_ERROR, "irc", "JOIN for %s failed\n", args[0]);
		g_free(nick);
		return;
	}

	userhost = irc_mask_userhost(from);
	gaim_chat_add_user(GAIM_CHAT(convo), nick, userhost);
	g_free(userhost);
	g_free(nick);
}

void irc_msg_kick(struct irc_conn *irc, const char *name, const char *from, char **args)
{
	GaimConnection *gc = gaim_account_get_connection(irc->account);
	GaimConversation *convo = gaim_find_conversation_with_account(args[0], irc->account);
	char *nick = irc_mask_nick(from), *buf;

	if (!gc) {
		g_free(nick);
		return;
	}

	if (!convo) {
		gaim_debug(GAIM_DEBUG_ERROR, "irc", "Recieved a KICK for unknown channel %s\n", args[0]);
		g_free(nick);
		return;
	}

	if (!gaim_utf8_strcasecmp(gaim_connection_get_display_name(gc), args[1])) {
		buf = g_strdup_printf(_("You have been kicked by %s: (%s)"), nick, args[2]);
		gaim_chat_write(GAIM_CHAT(convo), args[0], buf, WFLAG_SYSTEM, time(NULL));
		g_free(buf);
		/*g_slist_remove(irc->gc->buddy_chats, convo);
		  gaim_conversation_set_account(convo, NULL);*/
		/*g_list_free(gaim_chat_get_users(GAIM_CHAT(convo)));
		  gaim_chat_set_users(GAIM_CHAT(convo), NULL);*/
	} else {
		buf = g_strdup_printf(_("Kicked by %s (%s)"), nick, args[2]);
		gaim_chat_remove_user(GAIM_CHAT(convo), args[1], buf);
		g_free(buf);
	}

	g_free(nick);
	return;
}

void irc_msg_mode(struct irc_conn *irc, const char *name, const char *from, char **args)
{
	GaimConversation *convo;
	char *nick = irc_mask_nick(from), *buf;

	if (*args[0] == '#' || *args[0] == '&') {	/* Channel	*/
		convo = gaim_find_conversation_with_account(args[0], irc->account);
		if (!convo) {
			gaim_debug(GAIM_DEBUG_ERROR, "irc", "MODE received for %s, which we are not in\n", args[0]);
			g_free(nick);
			return;
		}
		buf = g_strdup_printf(_("mode (%s %s) by %s"), args[1], args[2] ? args[2] : "", nick);
		gaim_chat_write(GAIM_CHAT(convo), args[0], buf, WFLAG_SYSTEM|WFLAG_NOLOG, time(NULL));
		g_free(buf);
	} else {					/* User		*/
	}
	g_free(nick);
}

void irc_msg_nick(struct irc_conn *irc, const char *name, const char *from, char **args)
{
	GaimConnection *gc = gaim_account_get_connection(irc->account);
	GSList *chats;
	char *nick = irc_mask_nick(from);

	if (!gc) {
		g_free(nick);
		return;
	}
	chats = gc->buddy_chats;

	if (!gaim_utf8_strcasecmp(nick, gaim_connection_get_display_name(gc))) {
		gaim_connection_set_display_name(gc, args[0]);
	}

	while (chats) {
		GaimChat *chat = GAIM_CHAT(chats->data);
		GList *users = gaim_chat_get_users(chat);

		while (users) {
			char *user = users->data;

			if (!strcmp(nick, user)) {
				gaim_chat_rename_user(chat, user, args[0]);
				users = gaim_chat_get_users(chat);
				break;
			}
			users = users->next;
		}
		chats = chats->next;
	}
	g_free(nick);
}

void irc_msg_nickused(struct irc_conn *irc, const char *name, const char *from, char **args)
{
	char *newnick, *buf, *end;

	if (!args || !args[1])
		return;

	newnick = strdup(args[1]);
	end = newnick + strlen(newnick) - 1;
	/* try three fallbacks */
	if (*end == 2) *end = '3';
	else if (*end == 1) *end = '2';
	else *end = '1';

	buf = irc_format(irc, "vn", "NICK", newnick);
	irc_send(irc, buf);
	g_free(buf);
}

void irc_msg_notice(struct irc_conn *irc, const char *name, const char *from, char **args)
{
	char *newargs[2];

	newargs[0] = " notice ";	/* The spaces are magic, leave 'em in! */
	newargs[1] = args[1];
	irc_msg_privmsg(irc, name, from, newargs);
}

void irc_msg_part(struct irc_conn *irc, const char *name, const char *from, char **args)
{
	GaimConnection *gc = gaim_account_get_connection(irc->account);
	GaimConversation *convo;
	char *nick, *msg;

	if (!args || !args[0] || !args[1] || !gc)
		return;

	convo = gaim_find_conversation_with_account(args[0], irc->account);
	if (!convo) {
		gaim_debug(GAIM_DEBUG_INFO, "irc", "Got a PART on %s, which doesn't exist -- probably closed\n", args[0]);
		return;
	}

	nick = irc_mask_nick(from);
	if (!gaim_utf8_strcasecmp(nick, gaim_connection_get_display_name(gc))) {
		msg = g_strdup_printf(_("You have parted the channel%s%s"), *args[1] ? ": " : "", args[1]);
		gaim_chat_write(GAIM_CHAT(convo), args[0], msg, WFLAG_SYSTEM, time(NULL));
		g_free(msg);
	} else {
		gaim_chat_remove_user(GAIM_CHAT(convo), nick, args[1]);
	}
	g_free(nick);
}

void irc_msg_ping(struct irc_conn *irc, const char *name, const char *from, char **args)
{
	char *buf;
	if (!args || !args[0])
		return;

	buf = irc_format(irc, "v:", "PONG", args[0]);
	irc_send(irc, buf);
	g_free(buf);
}

void irc_msg_pong(struct irc_conn *irc, const char *name, const char *from, char **args)
{
	GaimConversation *convo;
	GaimConnection *gc;
	char **parts, *msg;
	time_t oldstamp;

	if (!args || !args[1])
		return;

	parts = g_strsplit(args[1], " ", 2);

	if (!parts[0] || !parts[1]) {
		g_strfreev(parts);
		return;
	}

	if (sscanf(parts[1], "%lu", &oldstamp) != 1) {
		msg = g_strdup(_("Error: invalid PONG from server"));
	} else {
		msg = g_strdup_printf(_("PING reply -- Lag: %lu seconds"), time(NULL) - oldstamp);
	}

	convo = gaim_find_conversation_with_account(parts[0], irc->account);
	g_strfreev(parts);
	if (convo) {
		if (gaim_conversation_get_type (convo) == GAIM_CONV_CHAT)
			gaim_chat_write(GAIM_CHAT(convo), "PONG", msg, WFLAG_SYSTEM|WFLAG_NOLOG, time(NULL));
		else
			gaim_im_write(GAIM_IM(convo), "PONG", msg, -1, WFLAG_SYSTEM|WFLAG_NOLOG, time(NULL));
	} else {
		gc = gaim_account_get_connection(irc->account);
		if (!gc) {
			g_free(msg);
			return;
		}
		gaim_notify_info(gc, NULL, "PONG", msg);
	}
	g_free(msg);
}

void irc_msg_privmsg(struct irc_conn *irc, const char *name, const char *from, char **args)
{
	GaimConnection *gc = gaim_account_get_connection(irc->account);
	GaimConversation *convo;
	char *nick = irc_mask_nick(from), *tmp, *msg;
	int notice = 0;

	if (!args || !args[0] || !args[1] || !gc) {
		g_free(nick);
		return;
	}

	notice = !strcmp(args[0], " notice ");
	tmp = irc_parse_ctcp(irc, nick, args[0], args[1], notice);
	if (!tmp) {
		g_free(nick);
		return;
	}
	msg = irc_mirc2html(tmp);
	g_free(tmp);
	if (notice) {
		tmp = g_strdup_printf("(notice) %s", msg);
		g_free(msg);
		msg = tmp;
	}

	if (!gaim_utf8_strcasecmp(args[0], gaim_connection_get_display_name(gc))) {
		serv_got_im(gc, nick, msg, 0, time(NULL), -1);
	} else if (notice) {
		serv_got_im(gc, nick, msg, 0, time(NULL), -1);
	} else {
		convo = gaim_find_conversation_with_account(args[0], irc->account);
		if (convo)
			serv_got_chat_in(gc, gaim_chat_get_id(GAIM_CHAT(convo)), nick, 0, msg, time(NULL));
		else
			gaim_debug(GAIM_DEBUG_ERROR, "irc", "Got a PRIVMSG on %s, which does not exist\n", args[0]);
	}
	g_free(msg);
	g_free(nick);
}

void irc_msg_quit(struct irc_conn *irc, const char *name, const char *from, char **args)
{
	GaimConnection *gc = gaim_account_get_connection(irc->account);
	char *data[2];

	if (!args || !args[0] || !gc)
		return;

	data[0] = irc_mask_nick(from);
	data[1] = args[0];
	/* XXX this should have an API, I shouldn't grab this directly */
	g_slist_foreach(gc->buddy_chats, (GFunc)irc_chat_remove_buddy, data);
	g_free(data[0]);

	return;
}

void irc_msg_wallops(struct irc_conn *irc, const char *name, const char *from, char **args)
{
	GaimConnection *gc = gaim_account_get_connection(irc->account);
	char *nick, *msg;

	if (!args || !args[0] || !gc)
		return;

	nick = irc_mask_nick(from);
	msg = g_strdup_printf (_("Wallops from %s"), nick);
	g_free(nick);
	gaim_notify_info(gc, NULL, msg, args[0]);
	g_free(msg);
}

void irc_msg_ignore(struct irc_conn *irc, const char *name, const char *from, char **args)
{
	return;
}
