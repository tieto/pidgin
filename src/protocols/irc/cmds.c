/**
 * @file cmds.c
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

#include "conversation.h"
#include "debug.h"
#include "notify.h"
#include "util.h"

#include "irc.h"


static void irc_do_mode(struct irc_conn *irc, const char *target, const char *sign, char **ops);

int irc_cmd_default(struct irc_conn *irc, const char *cmd, const char *target, const char **args)
{
	GaimConversation *convo = gaim_find_conversation_with_account(GAIM_CONV_TYPE_ANY, target, irc->account);
	char *buf;

	if (!convo)
		return 1;

	buf = g_strdup_printf(_("Unknown command: %s"), cmd);
	if (gaim_conversation_get_type(convo) == GAIM_CONV_TYPE_IM)
		gaim_conv_im_write(GAIM_CONV_IM(convo), "", buf, GAIM_MESSAGE_SYSTEM|GAIM_MESSAGE_NO_LOG, time(NULL));
	else
		gaim_conv_chat_write(GAIM_CONV_CHAT(convo), "", buf, GAIM_MESSAGE_SYSTEM|GAIM_MESSAGE_NO_LOG, time(NULL));
	g_free(buf);

	return 1;
}

int irc_cmd_away(struct irc_conn *irc, const char *cmd, const char *target, const char **args)
{
	char *buf, *message;

	if (args[0] && strcmp(cmd, "back")) {
		message = gaim_markup_strip_html(args[0]);
		gaim_util_chrreplace(message, '\n', ' ');
		buf = irc_format(irc, "v:", "AWAY", message);
		g_free(message);
	} else {
		buf = irc_format(irc, "v", "AWAY");
	}
	irc_send(irc, buf);
	g_free(buf);

	return 0;
}

int irc_cmd_ctcp_action(struct irc_conn *irc, const char *cmd, const char *target, const char **args)
{
	GaimConnection *gc = gaim_account_get_connection(irc->account);
	char *action, *escaped, *dst, **newargs;
	const char *src;
	GaimConversation *convo;

	if (!args || !args[0] || !gc)
		return 0;

	action = g_malloc(strlen(args[0]) + 10);

	sprintf(action, "\001ACTION ");

	src = args[0];
	dst = action + 8;
	while (*src) {
		if (*src == '\n') {
			if (*(src + 1) == '\0') {
				break;
			} else {
				*dst++ = ' ';
				src++;
				continue;
			}
		}
		*dst++ = *src++;
	}
	*dst++ = '\001';
	*dst = '\0';

	newargs = g_new0(char *, 2);
	newargs[0] = g_strdup(target);
	newargs[1] = action;
	irc_cmd_privmsg(irc, cmd, target, (const char **)newargs);
	g_free(newargs[0]);
	g_free(newargs[1]);
	g_free(newargs);

	convo = gaim_find_conversation_with_account(GAIM_CONV_TYPE_ANY, target, irc->account);
	if (convo) {
		escaped = g_markup_escape_text(args[0], -1);
		action = g_strdup_printf("/me %s", escaped);
		g_free(escaped);
		if (action[strlen(action) - 1] == '\n')
			action[strlen(action) - 1] = '\0';
		if (gaim_conversation_get_type(convo) == GAIM_CONV_TYPE_CHAT)
			serv_got_chat_in(gc, gaim_conv_chat_get_id(GAIM_CONV_CHAT(convo)),
			         	 gaim_connection_get_display_name(gc),
				         0, action, time(NULL));
		else
			gaim_conv_im_write(GAIM_CONV_IM(convo), gaim_connection_get_display_name(gc),
			                  action, 0, time(NULL));
		g_free(action);
	}

	return 1;
}

int irc_cmd_invite(struct irc_conn *irc, const char *cmd, const char *target, const char **args)
{
	char *buf;

	if (!args || !args[0] || !(args[1] || target))
		return 0;

	buf = irc_format(irc, "vnc", "INVITE", args[0], args[1] ? args[1] : target);
	irc_send(irc, buf);
	g_free(buf);

	return 0;
}

int irc_cmd_join(struct irc_conn *irc, const char *cmd, const char *target, const char **args)
{
	char *buf;

	if (!args || !args[0])
		return 0;

	if (args[1])
		buf = irc_format(irc, "vcv", "JOIN", args[0], args[1]);
	else
		buf = irc_format(irc, "vc", "JOIN", args[0]);
	irc_send(irc, buf);
	g_free(buf);

	return 0;
}

int irc_cmd_kick(struct irc_conn *irc, const char *cmd, const char *target, const char **args)
{
	char *buf;
	GaimConversation *convo;

	if (!args || !args[0])
		return 0;

	convo = gaim_find_conversation_with_account(GAIM_CONV_TYPE_CHAT, target, irc->account);
	if (!convo)
		return 0;

	if (args[1])
		buf = irc_format(irc, "vcn:", "KICK", target, args[0], args[1]);
	else
		buf = irc_format(irc, "vcn", "KICK", target, args[0]);
	irc_send(irc, buf);
	g_free(buf);

	return 0;
}

int irc_cmd_list(struct irc_conn *irc, const char *cmd, const char *target, const char **args)
{
	gaim_roomlist_show_with_account(irc->account);

	return 0;
}

int irc_cmd_mode(struct irc_conn *irc, const char *cmd, const char *target, const char **args)
{
	GaimConnection *gc;
	char *buf;

	if (!args)
		return 0;

	if (!strcmp(cmd, "mode")) {
		if (!args[0] && irc_ischannel(target))
			buf = irc_format(irc, "vc", "MODE", target);
		else if (args[0] && (*args[0] == '+' || *args[0] == '-'))
			buf = irc_format(irc, "vcv", "MODE", target, args[0]);
		else if (args[0])
			buf = irc_format(irc, "vv", "MODE", args[0]);
		else
			return 0;
	} else if (!strcmp(cmd, "umode")) {
		if (!args[0])
			return 0;
		gc = gaim_account_get_connection(irc->account);
		buf = irc_format(irc, "vnv", "MODE", gaim_connection_get_display_name(gc), args[0]);
	} else {
		return 0;
	}

	irc_send(irc, buf);
	g_free(buf);

	return 0;
}

int irc_cmd_names(struct irc_conn *irc, const char *cmd, const char *target, const char **args)
{
	char *buf;

	if (!args || (!args[0] && !irc_ischannel(target)))
		return 0;

	buf = irc_format(irc, "vc", "NAMES", args[0] ? args[0] : target);
	irc_send(irc, buf);
	g_free(buf);

	irc->nameconv = g_strdup(target);

	return 0;
}

int irc_cmd_nick(struct irc_conn *irc, const char *cmd, const char *target, const char **args)
{
	char *buf;

	if (!args || !args[0])
		return 0;

	buf = irc_format(irc, "v:", "NICK", args[0]);
	irc_send(irc, buf);
	g_free(buf);

	return 0;
}

int irc_cmd_op(struct irc_conn *irc, const char *cmd, const char *target, const char **args)
{
	char **nicks, **ops, *sign, *mode;
	int i = 0, used = 0;

	if (!args || !args[0] || !*args[0])
		return 0;

	if (!strcmp(cmd, "op")) {
		sign = "+";
		mode = "o";
	} else if (!strcmp(cmd, "deop")) {
		sign = "-";
		mode = "o";
	} else if (!strcmp(cmd, "voice")) {
		sign = "+";
		mode = "v";
	} else if (!strcmp(cmd, "devoice")) {
		sign = "-";
		mode = "v";
	} else {
		gaim_debug(GAIM_DEBUG_ERROR, "irc", "invalid 'op' command '%s'\n", cmd);
		return 0;
	}

	nicks = g_strsplit(args[0], " ", -1);

	for (i = 0; nicks[i]; i++)
		/* nothing */;
	ops = g_new0(char *, i * 2 + 1);

	for (i = 0; nicks[i]; i++) {
		if (!*nicks[i])
			continue;
		ops[used++] = mode;
		ops[used++] = nicks[i];
	}

	irc_do_mode(irc, target, sign, ops);
	g_free(ops);

	return 0;
}

int irc_cmd_part(struct irc_conn *irc, const char *cmd, const char *target, const char **args)
{
	char *buf;

	if (!args)
		return 0;

	if (args[1])
		buf = irc_format(irc, "vc:", "PART", args[0] ? args[0] : target, args[1]);
	else
		buf = irc_format(irc, "vc", "PART", args[0] ? args[0] : target);
	irc_send(irc, buf);
	g_free(buf);

	return 0;
}

int irc_cmd_ping(struct irc_conn *irc, const char *cmd, const char *target, const char **args)
{
	char *stamp;
	char *buf;

	if (args && args[0]) {
		if (irc_ischannel(args[0]))
			return 0;
		stamp = g_strdup_printf("\001PING %lu\001", time(NULL));
		buf = irc_format(irc, "vn:", "PRIVMSG", args[0], stamp);
		g_free(stamp);
	} else {
		stamp = g_strdup_printf("%s %lu", target, time(NULL));
		buf = irc_format(irc, "v:", "PING", stamp);
		g_free(stamp);
	}
	irc_send(irc, buf);
	g_free(buf);

	return 0;
}

int irc_cmd_privmsg(struct irc_conn *irc, const char *cmd, const char *target, const char **args)
{
	const char *cur, *end;
	char *msg, *buf;

	if (!args || !args[0] || !args[1])
		return 0;

	cur = args[1];
	end = args[1];
	while (*end && *cur) {
		end = strchr(cur, '\n');
		if (!end)
			end = cur + strlen(cur);
		msg = g_strndup(cur, end - cur);
		buf = irc_format(irc, "vt:", "PRIVMSG", args[0], msg);
		irc_send(irc, buf);
		g_free(msg);
		g_free(buf);
		cur = end + 1;
	}

	return 0;
}

int irc_cmd_quit(struct irc_conn *irc, const char *cmd, const char *target, const char **args)
{
	char *buf;

	if (!irc->quitting) {
		/*
		 * Use gaim_account_get_string(irc->account, "quitmsg", IRC_DEFAULT_QUIT)
		 * and uncomment the appropriate account preference in irc.c if we
		 * decide we want custom quit messages.
		 */
		buf = irc_format(irc, "v:", "QUIT", (args && args[0]) ? args[0] : IRC_DEFAULT_QUIT);
		irc_send(irc, buf);
		g_free(buf);

		irc->quitting = TRUE;

		gaim_account_set_status(irc->account, "offline", TRUE, NULL);
	}

	return 0;
}

int irc_cmd_quote(struct irc_conn *irc, const char *cmd, const char *target, const char **args)
{
	char *buf;

	if (!args || !args[0])
		return 0;

	buf = irc_format(irc, "v", args[0]);
	irc_send(irc, buf);
	g_free(buf);

	return 0;
}

int irc_cmd_query(struct irc_conn *irc, const char *cmd, const char *target, const char **args)
{
	GaimConversation *convo;
	GaimConnection *gc;

	if (!args || !args[0])
		return 0;

	convo = gaim_conversation_new(GAIM_CONV_TYPE_IM, irc->account, args[0]);
	gaim_conversation_present(convo);

	if (args[1]) {
		gc = gaim_account_get_connection(irc->account);
		irc_cmd_privmsg(irc, cmd, target, args);
		gaim_conv_im_write(GAIM_CONV_IM(convo), gaim_connection_get_display_name(gc),
			      args[1], GAIM_MESSAGE_SEND, time(NULL));
	}

	return 0;
}

int irc_cmd_remove(struct irc_conn *irc, const char *cmd, const char *target, const char **args)
{
	char *buf;

	if (!args || !args[0])
		return 0;

	if (!irc_ischannel(target)) /* not a channel, punt */
		return 0;

	if (args[1])
		buf = irc_format(irc, "vcn:", "REMOVE", target, args[0], args[1]);
	else
		buf = irc_format(irc, "vcn", "REMOVE", target, args[0]);
	irc_send(irc, buf);
	g_free(buf);

	return 0;
}

int irc_cmd_service(struct irc_conn *irc, const char *cmd, const char *target, const char **args)
{
	char *capital_cmd, *buf;

	if (!args || !args[0])
		return 0;

	/* cmd will be one of nickserv, chanserv, memoserv or operserv */
	capital_cmd = g_ascii_strup(cmd, -1);
	buf = irc_format(irc, "v:", capital_cmd, args[0]);
	irc_send(irc, buf);
	g_free(capital_cmd);
	g_free(buf);

	return 0;
}

int irc_cmd_time(struct irc_conn *irc, const char *cmd, const char *target, const char **args)
{
	char *buf;

	buf = irc_format(irc, "v", "TIME");
	irc_send(irc, buf);
	g_free(buf);

	return 0;
}

int irc_cmd_topic(struct irc_conn *irc, const char *cmd, const char *target, const char **args)
{
	char *buf;
	const char *topic;
	GaimConversation *convo;

	if (!args)
		return 0;

	convo = gaim_find_conversation_with_account(GAIM_CONV_TYPE_CHAT, target, irc->account);
	if (!convo)
		return 0;

	if (!args[0]) {
		topic = gaim_conv_chat_get_topic (GAIM_CONV_CHAT(convo));

		if (topic) {
			char *tmp, *tmp2;
			tmp = g_markup_escape_text(topic, -1);
			tmp2 = gaim_markup_linkify(tmp);
			buf = g_strdup_printf(_("current topic is: %s"), tmp2);
			g_free(tmp);
			g_free(tmp2);
		} else
			buf = g_strdup(_("No topic is set"));
		gaim_conv_chat_write(GAIM_CONV_CHAT(convo), target, buf, GAIM_MESSAGE_SYSTEM|GAIM_MESSAGE_NO_LOG, time(NULL));
		g_free(buf);

		return 0;
	}

	buf = irc_format(irc, "vt:", "TOPIC", target, args[0]);
	irc_send(irc, buf);
	g_free(buf);

	return 0;
}

int irc_cmd_wallops(struct irc_conn *irc, const char *cmd, const char *target, const char **args)
{
	char *buf;

	if (!args || !args[0])
		return 0;

	if (!strcmp(cmd, "wallops"))
		buf = irc_format(irc, "v:", "WALLOPS", args[0]);
	else if (!strcmp(cmd, "operwall"))
		buf = irc_format(irc, "v:", "OPERWALL", args[0]);
	else
		return 0;

	irc_send(irc, buf);
	g_free(buf);

	return 0;
}

int irc_cmd_whois(struct irc_conn *irc, const char *cmd, const char *target, const char **args)
{
	char *buf;

	if (!args || !args[0])
		return 0;

	if (args[1]) {
		buf = irc_format(irc, "vvn", "WHOIS", args[0], args[1]);
		irc->whois.nick = g_strdup(args[1]);
	} else {
		buf = irc_format(irc, "vn", "WHOIS", args[0]);
		irc->whois.nick = g_strdup(args[0]);
	}

	irc_send(irc, buf);
	g_free(buf);

	return 0;
}

static void irc_do_mode(struct irc_conn *irc, const char *target, const char *sign, char **ops)
{
	char *buf, mode[5];
	int i = 0;

	if (!sign)
		return;

	while (ops[i]) {
		if (ops[i + 2] && ops[i + 4]) {
			g_snprintf(mode, sizeof(mode), "%s%s%s%s", sign,
				   ops[i], ops[i + 2], ops[i + 4]);
			buf = irc_format(irc, "vcvnnn", "MODE", target, mode,
					 ops[i + 1], ops[i + 3], ops[i + 5]);
			i += 6;
		} else if (ops[i + 2]) {
			g_snprintf(mode, sizeof(mode), "%s%s%s",
				   sign, ops[i], ops[i + 2]);
			buf = irc_format(irc, "vcvnn", "MODE", target, mode,
					 ops[i + 1], ops[i + 3]);
			i += 4;
		} else {
			g_snprintf(mode, sizeof(mode), "%s%s", sign, ops[i]);
			buf = irc_format(irc, "vcvn", "MODE", target, mode, ops[i + 1]);
			i += 2;
		}
		irc_send(irc, buf);
		g_free(buf);
	}

	return;
}
