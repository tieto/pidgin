/**
 * @file parse.c
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

#include "accountopt.h"
#include "conversation.h"
#include "notify.h"
#include "debug.h"
#include "irc.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

static char *irc_send_convert(struct irc_conn *irc, const char *string);
static char *irc_recv_convert(struct irc_conn *irc, const char *string);

char *irc_mirc2html(const char *string);

static void irc_parse_error_cb(struct irc_conn *irc, char *input);

static char *irc_mirc_colors[16] = {
	"white", "black", "blue", "dark green", "red", "brown", "purple",
		"orange", "yellow", "green", "teal", "cyan", "light blue",
		"pink", "grey", "light grey" };

/*typedef void (*IRCMsgCallback)(struct irc_conn *irc, char *from, char *name, char **args);*/
static struct _irc_msg {
	char *name;
	char *format;
	void (*cb)(struct irc_conn *irc, const char *name, const char *from, char **args);
} _irc_msgs[] = {
	{ "301", "nn:", irc_msg_away },		/* User is away			*/
	{ "303", "n:", irc_msg_ison },		/* ISON reply			*/
	{ "311", "nnvvv:", irc_msg_whois },	/* Whois user			*/
	{ "312", "nnv:", irc_msg_whois },	/* Whois server			*/
	{ "313", "nn:", irc_msg_whois },	/* Whois ircop			*/
	{ "317", "nnvv", irc_msg_whois },	/* Whois idle			*/
	{ "318", "nt:", irc_msg_endwhois },	/* End of WHOIS			*/
	{ "319", "nn:", irc_msg_whois },	/* Whois channels		*/
	{ "320", "nn:", irc_msg_whois },	/* Whois (fn ident)		*/
	{ "324", "ncv:", irc_msg_chanmode },	/* Channel modes		*/
	{ "331", "nc:",	irc_msg_topic },	/* No channel topic		*/
	{ "332", "nc:", irc_msg_topic },	/* Channel topic		*/
	{ "333", "*", irc_msg_ignore },		/* Topic setter stuff		*/
	{ "353", "nvc:", irc_msg_names },	/* Names list			*/
	{ "366", "nc:", irc_msg_names },	/* End of names			*/
	{ "372", "n:", irc_msg_motd },		/* MOTD				*/
	{ "375", "n:", irc_msg_motd },		/* Start MOTD			*/
	{ "376", "n:", irc_msg_endmotd },	/* End of MOTD			*/
	{ "401", "nt:", irc_msg_nonick },	/* No such nick/chan		*/
	{ "404", "nt:", irc_msg_nosend },	/* Cannot send to chan		*/
	{ "421", "nv:", irc_msg_unknown },	/* Unknown command		*/
	{ "422", "nv:", irc_msg_endmotd },	/* No MOTD available		*/
	{ "433", "vn:", irc_msg_nickused },	/* Nickname already in use	*/
	{ "438", "nn:", irc_msg_nochangenick },	/* Nick may not change		*/
	{ "442", "nc:", irc_msg_notinchan },	/* Not in channel		*/
	{ "473", "nc:", irc_msg_inviteonly },	/* Tried to join invite-only	*/
	{ "474", "nc:", irc_msg_banned },	/* Banned from channel		*/
	{ "482", "nc:", irc_msg_notop },	/* Need to be op to do that	*/
	{ "501", "n:", irc_msg_badmode },	/* Unknown mode flag		*/
	{ "515", "nc:", irc_msg_regonly },	/* Registration required	*/
	{ "invite", "n:", irc_msg_invite },	/* Invited			*/
	{ "join", ":", irc_msg_join },		/* Joined a channel		*/
	{ "kick", "cn:", irc_msg_kick },	/* KICK				*/
	{ "mode", "tv:", irc_msg_mode },	/* MODE for channel		*/
	{ "nick", ":", irc_msg_nick },		/* Nick change			*/
	{ "notice", "t:", irc_msg_notice },	/* NOTICE recv			*/
	{ "part", "c:", irc_msg_part },		/* Parted a channel		*/
	{ "ping", ":", irc_msg_ping },		/* Received PING from server	*/
	{ "pong", "v:", irc_msg_pong },		/* Received PONG from server	*/
	{ "privmsg", "t:", irc_msg_privmsg },	/* Received private message	*/
	{ "topic", "c:", irc_msg_topic },	/* TOPIC command		*/
	{ "quit", ":", irc_msg_quit },		/* QUIT notice			*/
	{ "wallops", ":", irc_msg_wallops },	/* WALLOPS command		*/
	{ NULL, NULL, NULL }
};

static struct _irc_user_cmd {
	char *name;
	char *format;
	IRCCmdCallback cb;
} _irc_cmds[] = {
	{ "away", ":", irc_cmd_away },
	{ "deop", ":", irc_cmd_op },
	{ "devoice", ":", irc_cmd_op },
	{ "help", "v", irc_cmd_help },
	{ "invite", ":", irc_cmd_invite },
	{ "j", "cv", irc_cmd_join },
	{ "join", "cv", irc_cmd_join },
	{ "kick", "n:", irc_cmd_kick },
	{ "me", ":", irc_cmd_ctcp_action },
	{ "mode", ":", irc_cmd_mode },
	{ "msg", "t:", irc_cmd_privmsg },
	{ "names", "c", irc_cmd_names },
	{ "nick", "n", irc_cmd_nick },
	{ "op", ":", irc_cmd_op },
	{ "operwall", ":", irc_cmd_wallops },
	{ "part", "c:", irc_cmd_part },
	{ "ping", "n", irc_cmd_ping },
	{ "query", "n:", irc_cmd_query },
	{ "quit", ":", irc_cmd_quit },
	{ "quote", "*", irc_cmd_quote },
	{ "remove", "n:", irc_cmd_remove },
	{ "topic", ":", irc_cmd_topic },
	{ "umode", ":", irc_cmd_mode },
	{ "voice", ":", irc_cmd_op },
	{ "wallops", ":", irc_cmd_wallops },
	{ "whois", "n", irc_cmd_whois },
	{ NULL, NULL }
};

static char *irc_send_convert(struct irc_conn *irc, const char *string)
{
	char *utf8;
	GError *err = NULL;
	
	utf8 = g_convert(string, strlen(string), 
			 gaim_account_get_string(irc->account, "encoding", IRC_DEFAULT_CHARSET),
			 "UTF-8", NULL, NULL, &err);
	if (err) {
		gaim_debug(GAIM_DEBUG_ERROR, "irc", "send conversion error: %s\n", err->message);
		gaim_debug(GAIM_DEBUG_ERROR, "irc", "Sending raw, which probably isn't right\n");
		utf8 = g_strdup(string);
	}
	
	return utf8;
}

static char *irc_recv_convert(struct irc_conn *irc, const char *string)
{
	char *utf8;
	GError *err = NULL;
	
	utf8 = g_convert(string, strlen(string), "UTF-8",
			 gaim_account_get_string(irc->account, "encoding", IRC_DEFAULT_CHARSET),
			 NULL, NULL, &err);
	if (err) {
		gaim_debug(GAIM_DEBUG_ERROR, "irc", "recv conversion error: %s\n", err->message);
		utf8 = g_strdup(_("(There was an error converting this message.  Check the 'Encoding' option in the Account Editor)"));
	}
	
	return utf8;
}

/* XXX tag closings are not necessarily correctly nested here!  If we
 *     get a ^O or reach the end of the string and there are open
 *     tags, they are closed in a fixed order ... this means, for
 *     example, you might see <FONT COLOR="blue">some text <B>with
 *     various attributes</FONT></B> (notice that B and FONT overlap
 *     and are not cleanly nested).  This is imminently fixable but
 *     I am not fixing it right now.
 */
char *irc_mirc2html(const char *string)
{
	const char *cur, *end;
	char fg[3] = "\0\0", bg[3] = "\0\0";
	int fgnum, bgnum;
	int font = 0, bold = 0, underline = 0;
	GString *decoded = g_string_sized_new(strlen(string));

	cur = string;
	do {
		end = strpbrk(cur, "\002\003\007\017\026\037");

		decoded = g_string_append_len(decoded, cur, end ? end - cur : strlen(cur));
		cur = end ? end : cur + strlen(cur);

		switch (*cur) {
		case '\002':
			cur++;
			if (!bold) {
				decoded = g_string_append(decoded, "<B>");
				bold = TRUE;
			} else {
				decoded = g_string_append(decoded, "</B>");
				bold = FALSE;
			}
			break;
		case '\003':
			cur++;
			fg[0] = fg[1] = bg[0] = bg[1] = '\0';
			if (isdigit(*cur))
				fg[0] = *cur++;
			if (isdigit(*cur))
				fg[1] = *cur++;
			if (*cur == ',') {
				cur++;
				if (isdigit(*cur))
					bg[0] = *cur++;
				if (isdigit(*cur))
					bg[1] = *cur++;
			}
			if (font) {
				decoded = g_string_append(decoded, "</FONT>");
				font = FALSE;
			}

			if (fg[0]) {
				fgnum = atoi(fg);
				if (fgnum < 0 || fgnum > 15)
					continue;
				font = TRUE;
				g_string_append_printf(decoded, "<FONT COLOR=\"%s\"", irc_mirc_colors[fgnum]);
				if (bg[0]) {
					bgnum = atoi(bg);
					if (bgnum >= 0 && bgnum < 16)
						g_string_append_printf(decoded, " BACK=\"%s\"", irc_mirc_colors[bgnum]);
				}
				decoded = g_string_append_c(decoded, '>');
			}
			break;
		case '\037':
			cur++;
			if (!underline) {
				decoded = g_string_append(decoded, "<U>");
				underline = TRUE;
			} else {
				decoded = g_string_append(decoded, "</U>");
				underline = TRUE;
			}
			break;
		case '\007':
		case '\026':
			cur++;
			break;
		case '\017':
			cur++;
			/* fallthrough */
		case '\000':
			if (bold)
				decoded = g_string_append(decoded, "</B>");
			if (underline)
				decoded = g_string_append(decoded, "</U>");
			if (font)
				decoded = g_string_append(decoded, "</FONT>");
			break;
		default:
			gaim_debug(GAIM_DEBUG_ERROR, "irc", "Unexpected mIRC formatting character %d\n", *cur);
		}
	} while (*cur);

	return g_string_free(decoded, FALSE);
}

char *irc_parse_ctcp(struct irc_conn *irc, const char *from, const char *to, const char *msg, int notice)
{
	GaimConnection *gc;
	const char *cur = msg + 1;
	char *buf, *ctcp;
	time_t timestamp;

	/* Note that this is NOT correct w.r.t. multiple CTCPs in one
	 * message and low-level quoting ... but if you want that crap,
	 * use a real IRC client. */

	if (msg[0] != '\001' || msg[strlen(msg) - 1] != '\001')
		return g_strdup(msg);

	if (!strncmp(cur, "ACTION ", 7)) {
		cur += 7;
		buf = g_strdup_printf("/me %s", cur);
		buf[strlen(buf) - 1] = '\0';
		return buf;
	} else if (!strncmp(cur, "PING ", 5)) {
		if (notice) { /* reply */
			sscanf(cur, "PING %lu", &timestamp);
			gc = gaim_account_get_connection(irc->account);
			if (!gc)
				return NULL;
			buf = g_strdup_printf(_("Reply time from %s: %lu seconds"), from, time(NULL) - timestamp);
			gaim_notify_info(gc, _("PONG"), _("CTCP PING reply"), buf);
			g_free(buf);
			return NULL;
		} else {
			buf = irc_format(irc, "vt:", "NOTICE", from, msg);
			irc_send(irc, buf);
			g_free(buf);
			gc = gaim_account_get_connection(irc->account);
		}
	} else if (!strncmp(cur, "VERSION", 7) && !notice) {
		buf = irc_format(irc, "vt:", "NOTICE", from, "\001VERSION Gaim IRC\001");
		irc_send(irc, buf);
		g_free(buf);
	}

	ctcp = g_strdup(msg + 1);
	ctcp[strlen(ctcp) - 1] = '\0';
	buf = g_strdup_printf("Received CTCP '%s' (to %s) from %s", ctcp, to, from);
	g_free(ctcp);
	return buf;
}

void irc_msg_table_build(struct irc_conn *irc)
{
	int i;

	if (!irc || !irc->msgs) {
		gaim_debug(GAIM_DEBUG_ERROR, "irc", "Attempt to build a message table on a bogus structure\n");
		return;
	}

	for (i = 0; _irc_msgs[i].name; i++) {
		g_hash_table_insert(irc->msgs, (gpointer)_irc_msgs[i].name, (gpointer)&_irc_msgs[i]);
	}
}

void irc_cmd_table_build(struct irc_conn *irc)
{
	int i;

	if (!irc || !irc->cmds) {
		gaim_debug(GAIM_DEBUG_ERROR, "irc", "Attempt to build a command table on a bogus structure\n");
		return;
	}

	for (i = 0; _irc_cmds[i].name ; i++) {
		g_hash_table_insert(irc->cmds, (gpointer)_irc_cmds[i].name, (gpointer)&_irc_cmds[i]);
	}
}

char *irc_format(struct irc_conn *irc, const char *format, ...)
{
	GString *string = g_string_new("");
	char *tok, *tmp;
	const char *cur;
	va_list ap;

	va_start(ap, format);
	for (cur = format; *cur; cur++) {
		if (cur != format)
			g_string_append_c(string, ' ');

		tok = va_arg(ap, char *);
		switch (*cur) {
		case 'v':
			g_string_append(string, tok);
			break;
		case ':':
			g_string_append_c(string, ':');
			/* no break! */
		case 't':
		case 'n':
		case 'c':
			tmp = irc_send_convert(irc, tok);
			g_string_append(string, tmp);
			g_free(tmp);
			break;
		default:
			gaim_debug(GAIM_DEBUG_ERROR, "irc", "Invalid format character '%c'\n", *cur);
			break;
		}
	}
	va_end(ap);
	g_string_append(string, "\r\n");
	return (g_string_free(string, FALSE));
}

void irc_parse_msg(struct irc_conn *irc, char *input)
{
	struct _irc_msg *msgent;
	char *cur, *end, *tmp, *from, *msgname, *fmt, **args, *msg;
	int i;

	if (!strncmp(input, "PING ", 5)) {
		msg = irc_format(irc, "vv", "PONG", input + 5);
		irc_send(irc, msg);
		g_free(msg);
		return;
	} else if (!strncmp(input, "ERROR ", 6)) {
		gaim_connection_error(gaim_account_get_connection(irc->account), _("Disconnected"));
		return;
	}

	if (input[0] != ':' || (cur = strchr(input, ' ')) == NULL) {
		irc_parse_error_cb(irc, input);
		return;
	}

	from = g_strndup(&input[1], cur - &input[1]);
	cur++;
	end = strchr(cur, ' ');
	if (!end)
		end = cur + strlen(cur);

	tmp = g_strndup(cur, end - cur);
	msgname = g_ascii_strdown(tmp, -1);
	g_free(tmp);

	if ((msgent = g_hash_table_lookup(irc->msgs, msgname)) == NULL) {
		irc_msg_default(irc, "", from, &input);
		g_free(msgname);
		g_free(from);
		return;
	}
	g_free(msgname);

	args = g_new0(char *, strlen(msgent->format));
	for (cur = end, fmt = msgent->format, i = 0; fmt[i] && *cur++; i++) {
		switch (fmt[i]) {
		case 'v':
			if (!(end = strchr(cur, ' '))) end = cur + strlen(cur);
			args[i] = g_strndup(cur, end - cur);
			cur += end - cur;
			break;
		case 't':
		case 'n':
		case 'c':
			if (!(end = strchr(cur, ' '))) end = cur + strlen(cur);
			tmp = g_strndup(cur, end - cur);
			args[i] = irc_recv_convert(irc, tmp);
			g_free(tmp);
			cur += end - cur;
			break;
		case ':':
			if (*cur == ':') cur++;
			args[i] = irc_recv_convert(irc, cur);
			cur = cur + strlen(cur);
			break;
		case '*':
			args[i] = g_strdup(cur);
			cur = cur + strlen(cur);
			break;
		default:
			gaim_debug(GAIM_DEBUG_ERROR, "irc", "invalid message format character '%c'\n", fmt[i]);
			break;
		}
	}
	tmp = irc_recv_convert(irc, from);
	(msgent->cb)(irc, msgent->name, tmp, args);
	g_free(tmp);
	for (i = 0; i < strlen(msgent->format); i++) {
		g_free(args[i]);
	}
	g_free(args);
	g_free(from);
}

int irc_parse_cmd(struct irc_conn *irc, const char *target, const char *cmdstr)
{
	const char *cur, *end, *fmt; 
	char *tmp, *cmd, **args;
	struct _irc_user_cmd *cmdent;
	int i, ret;

	cur = cmdstr;
	end = strchr(cmdstr, ' ');
	if (!end)
		end = cur + strlen(cur);

	tmp = g_strndup(cur, end - cur);
	cmd = g_utf8_strdown(tmp, -1);
	g_free(tmp);

	if ((cmdent = g_hash_table_lookup(irc->cmds, cmd)) == NULL) {
		ret = irc_cmd_default(irc, cmd, target, &cmdstr);
		g_free(cmd);
		return ret;
	}

	args = g_new0(char *, strlen(cmdent->format));
	for (cur = end, fmt = cmdent->format, i = 0; fmt[i] && *cur++; i++) {
		switch (fmt[i]) {
		case 'v':
			if (!(end = strchr(cur, ' '))) end = cur + strlen(cur);
			args[i] = g_strndup(cur, end - cur);
			cur += end - cur;
			break;
		case 't':
		case 'n':
		case 'c':
			if (!(end = strchr(cur, ' '))) end = cur + strlen(cur);
			args[i] = g_strndup(cur, end - cur);
			cur += end - cur;
			break;
		case ':':
		case '*':
			args[i] = g_strdup(cur);
			cur = cur + strlen(cur);
			break;
		default:
			gaim_debug(GAIM_DEBUG_ERROR, "irc", "invalid command format character '%c'\n", fmt[i]);
			break;
		}
	}
	ret = (cmdent->cb)(irc, cmd, target, (const char **)args);
	for (i = 0; i < strlen(cmdent->format); i++)
		g_free(args[i]);
	g_free(args);

	g_free(cmd);
	return ret;
}

static void irc_parse_error_cb(struct irc_conn *irc, char *input)
{
	gaim_debug(GAIM_DEBUG_WARNING, "irc", "Unrecognized string: %s\n", input);
}
