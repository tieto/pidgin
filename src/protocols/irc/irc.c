/*
 * gaim - IRC Protocol Plugin
 *
 * Copyright (C) 2000-2001, Rob Flynn <rob@tgflinux.com>
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
 *
 * A large portion of this was copied more or less directly from X-Chat,
 * the world's most rocking IRC client. http://www.xchat.org/
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

#include <config.h>

#ifndef _WIN32
#include <unistd.h>
#else
#include <winsock.h>
#endif

#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <sys/stat.h>
#include <ctype.h>
#include "gaim.h"
#include "accountopt.h"
#include "multi.h"
#include "core.h"
#include "prpl.h"
#include "proxy.h"

#ifdef _WIN32
#include "win32dep.h"
#endif

#define IRC_BUF_LEN 4096
#define PDIWORDS 32

#define DEFAULT_SERVER "irc.freenode.net"

static GaimPlugin *my_protocol = NULL;

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif

/* Datastructs */
struct dcc_chat
{
	GaimConnection *gc;	
	char ip_address[INET6_ADDRSTRLEN];
	int port;		
	int fd;			
	int inpa;		
	char nick[80];		
};

struct irc_xfer_data
{
	char *ip;
	int port;

	struct irc_data *idata;
};

struct irc_data {
	int fd;
	gboolean online;
	guint32 timer;

	char *server;

	char *rxqueue;
	int rxlen;

	GString *str;
	int bc;

	char *chantypes;
	char *chanmodes;
	char *nickmodes;
	gboolean six_modes;

	gboolean in_whois;
	gboolean in_list;
	GString *liststr;
	GSList *file_transfers;
};

/* Prototypes */
static void irc_start_chat(GaimConnection *gc, const char *who);
static void irc_ctcp_clientinfo(GaimConnection *gc, const char *who);
static void irc_ctcp_userinfo(GaimConnection *gc, const char *who);
static void irc_ctcp_version(GaimConnection *gc, const char *who);
static void irc_ctcp_ping(GaimConnection *gc, const char *who);

static void irc_send_privmsg(GaimConnection *gc, const char *who, const char *what, gboolean fragment);
static void irc_send_notice(GaimConnection *gc, char *who, char *what);

static char *irc_send_convert(GaimConnection *gc, const char *string, int maxlen, int *done);
static char *irc_recv_convert(GaimConnection *gc, char *string);
static void irc_parse_notice(GaimConnection *gc, char *nick, char *ex,
                            char *word[], char *word_eol[]);
static void irc_parse_join(GaimConnection *gc, char *nick,
                          char *word[], char *word_eol[]);
static gboolean irc_parse_part(GaimConnection *gc, char *nick, char *cmd,
                          char *word[], char *word_eol[]);
static void irc_parse_topic(GaimConnection *gc, char *nick,
                           char *word[], char *word_eol[]);

static void dcc_chat_cancel(struct dcc_chat *);

/* Global variables */
GSList *dcc_chat_list = NULL;

struct dcc_chat *
find_dcc_chat (GaimConnection *gc, const char *nick)
{
	GSList *tmp;
	struct dcc_chat *data;
	tmp = dcc_chat_list;
	while (tmp != NULL)
		{
			data = (struct dcc_chat *) (tmp)->data;
			if (data 
			    && data->nick 
			    && strcmp (nick, data->nick) == 0
			    && gc == data->gc)
				{
					return data;
				}
			tmp = tmp->next;
		}
	return NULL;
}

static int 
irc_write(int fd, char *data, int len)
{
	gaim_debug(GAIM_DEBUG_MISC, "irc", "C: %s", data);
	return write(fd, data, len);
}

static char *
irc_send_convert(GaimConnection *gc, const char *string, int maxlen, int *done)
{
	char *converted = g_malloc(maxlen + 1);
	gchar *inptr = (gchar*)string, *outptr = converted;
	int inleft = strlen(string), outleft = maxlen;
	GIConv conv;
	
	conv = g_iconv_open(gaim_account_get_string(gaim_connection_get_account(gc), "charset", "UTF-8") , "UTF-8");
	if (g_iconv(conv, &inptr, &inleft, &outptr, &outleft) == -1) {
		gaim_debug(GAIM_DEBUG_ERROR, "irc", "Charset conversion error\n");
		gaim_debug(GAIM_DEBUG_ERROR, "irc",
				   "Sending as UTF-8 (this is a hack!)\n");
		g_free(converted);
		*done = maxlen;
		return(g_strndup(string, maxlen));
	}
	
	*done = strlen(string) - inleft;
	*outptr = '\0';
	return(converted);
}

static char *
irc_recv_convert(GaimConnection *gc, char *string)
{
	char *utf8;
	GError *err = NULL;
	
	utf8 = g_convert(string, strlen(string), "UTF-8",
			 gaim_account_get_string(gaim_connection_get_account(gc), "charset", "UTF-8") , NULL, NULL, &err);
	if (err) {
		gaim_debug(GAIM_DEBUG_ERROR, "irc",
				   "recv conversion error: %s\n", err->message);
		utf8 = g_strdup(_("(There was an error converting this message.  Check the 'Encoding' option in the Account Editor)"));
	}
	
	return (utf8);
}

static GaimConversation *
irc_find_chat(GaimConnection *gc, const char *name)
{
	GSList *bcs = gc->buddy_chats;

	while (bcs) {
		GaimConversation *b = bcs->data;
		if (!gaim_utf8_strcasecmp(b->name, name))
			return b;
		bcs = bcs->next;
	}
	return NULL;
}

static void 
process_data_init(char *buf, char *cmd, char *word[], char *eol[], gboolean quotes)
{
	int wordcount = 2;
	gboolean space = FALSE;
	gboolean quote = FALSE;
	int j = 0;

	word[1] = cmd;
	eol[1] = buf;

	while (TRUE) {
		switch (*cmd) {
		case 0:
			buf[j] = 0;
			for (j = wordcount; j < PDIWORDS; j++) {
				word[j] = "\000\000";
				eol[j] = "\000\000";
			}
			return;
		case '"':
			if (!quotes) {
				space = FALSE;
				buf[j++] = *cmd;
				break;
			}
			quote = !quote;
			break;
		case ' ':
			if (quote) {
				space = FALSE;
				buf[j++] = *cmd;
				break;
			}
			if (space)
				break;
			buf[j++] = 0;
			word[wordcount] = &buf[j];
			eol[wordcount++] = cmd + 1;
			if (wordcount == PDIWORDS - 1) {
				buf[j] = 0;
				return;
			}
			space = TRUE;
			break;
		default:
			space = FALSE;
			buf[j++] = *cmd;
		}
		cmd++;
	}
}

static void 
handle_005(GaimConnection *gc, char *word[], char *word_eol[])
{
	int w = 4;
	struct irc_data *id = gc->proto_data;

	while (w < PDIWORDS && *word[w]) {
		if (!strncmp(word[w], "MODES=", 5)) {
			if (atoi(word[w] + 6) >= 6)
				id->six_modes = TRUE;
		} else if (!strncmp(word[w], "CHANTYPES=", 10)) {
			g_free(id->chantypes);
			id->chantypes = g_strdup(word[w] + 10);
		} else if (!strncmp(word[w], "CHANMODES=", 10)) {
			g_free(id->chanmodes);
			id->chanmodes = g_strdup(word[w] + 10);
		} else if (!strncmp(word[w], "PREFIX=", 7)) {
			char *pre = strchr(word[w] + 7, ')');
			if (pre) {
				*pre = 0;
				g_free(id->nickmodes);
				id->nickmodes = g_strdup(word[w] + 8);
			}
		}
		w++;
	}
}

static const char *irc_colors[] = {
		"#000000", "#ffffff", "#000066", "#006600",
		"#ff0000", "#660000", "#660066", "#666600",
		"#cccc00", "#33cc33", "#00acac", "#00ccac",
		"#0000ff", "#cc00cc", "#666666", "#00ccac"
};

#define int_to_col(c) (irc_colors[(((c)<0 || (c)> 15)?0:c)])

static GString *
encode_html(char *msg)
{
	GString *str = g_string_new("");
	char *cur = msg, *end = msg;
	gboolean bold = FALSE, underline = FALSE, italics = FALSE;

	while ((end = strchr(cur, '<'))) {
		*end = 0;
		str = g_string_append(str, cur);
		cur = ++end;
		if (!g_ascii_strncasecmp(cur, "B>", 2)) {
			if (!bold) {
				bold = TRUE;
				str = g_string_append_c(str, '\2');
			}
			cur = cur + 2;
		} else if (!g_ascii_strncasecmp(cur, "I>", 2)) { /* use bold for italics too */
			if (!italics) {
				italics = TRUE;
				str = g_string_append_c(str, '\2');
			}
			cur = cur + 2;
		} else if (!g_ascii_strncasecmp(cur, "U>", 2)) {
			if (!underline) {
				underline = TRUE;
				str = g_string_append_c(str, '\37');
			}
			cur = cur + 2;
		}  else if (!g_ascii_strncasecmp(cur, "/B>", 3)) {
			if (bold) {
				bold = FALSE;
				str = g_string_append_c(str, '\2');
			}
			cur = cur + 3;
		}  else if (!g_ascii_strncasecmp(cur, "/I>", 3)) {
			if (italics) {
				italics = FALSE;
				str = g_string_append_c(str, '\2');
			}
			cur = cur + 3;
		}  else if (!g_ascii_strncasecmp(cur, "/U>", 3)) {
			if (underline) {
				underline = FALSE;
				str = g_string_append_c(str, '\37');
			}
			cur = cur + 3;
		}  else {
			str = g_string_append_c(str, '<');
		}

	}
	str = g_string_append(str, cur);
	return str;
}

static GString *
decode_html(char *msg)
{
	GString /* oo la la */ *str = g_string_new("");
	char *cur = msg, *end = msg;
	gboolean bold = FALSE, underline = FALSE, fg = FALSE, bg = FALSE;
	int fore, back;
	while (*end) {
		switch (*end) {
		case 02: /* ^B */
			*end = 0;
			str = g_string_append(str, cur);
			if (bold)
				str = g_string_append(str, "</B>");
			else
				str = g_string_append(str, "<B>");
			bold = !bold;
			cur = end + 1;
			break;
		case 03: /* ^C */
			*end++ = 0;
			str = g_string_append(str, cur);
			fore = back = -1;
			if (isdigit(*end)) {
				fore = *end++ - '0';
				if (isdigit(*end)) {
					fore *= 10;
					fore += *end++ - '0';
				}
				if (*end == ',' && isdigit(end[1])) {
					end++;
					back = *end++ - '0';
					if (isdigit(*end)) {
						back *= 10;
						back += *end++ - '0';
					}
				}
			}
			if (fore == -1) {
				if (fg)
					str = g_string_append(str, "</FONT>");
				if (bg)
					str = g_string_append(str, "</FONT>");
				fg = bg = FALSE;
			} else {
				fore %= 16;
				if (fg)
					str = g_string_append(str, "</FONT>");
				if (back != -1) {
					if (bg)
						str = g_string_append(str, "</FONT>");
					back %= 16;
					str = g_string_append(str, "<FONT BACK=");
					str = g_string_append(str, int_to_col(back));
					str = g_string_append_c(str, '>');
					bg = TRUE;
				}
				str = g_string_append(str, "<FONT COLOR=");
				str = g_string_append(str, int_to_col(fore));
				str = g_string_append_c(str, '>');
				fg = TRUE;
			}
			cur = end--;
			break;
		case 017: /* ^O */
			if (!bold && !underline && !fg && !bg)
				break;
			*end = 0;
			str = g_string_append(str, cur);
			if (bold)
				str = g_string_append(str, "</B>");
			if (underline)
				str = g_string_append(str, "</U>");
			if (fg)
				str = g_string_append(str, "</FONT>");
			if (bg)
				str = g_string_append(str, "</FONT>");
			bold = underline = fg = bg = FALSE;
			cur = end + 1;
			break;
		case 037: /* ^_ */
			*end = 0;
			str = g_string_append(str, cur);
			if (underline)
				str = g_string_append(str, "</U>");
			else
				str = g_string_append(str, "<U>");
			underline = !underline;
			cur = end + 1;
			break;
		}
		end++;
	}
	if (*cur)
		str = g_string_append(str, cur);
	return str;
}

static void 
irc_got_im(GaimConnection *gc, char *who, char *what, int flags, time_t t)
{
	char *utf8 = irc_recv_convert(gc, what);
	GString *str = decode_html(utf8);
	serv_got_im(gc, who, str->str, flags, t, -1);
	g_string_free(str, TRUE);
	g_free(utf8);
}

static void 
dcc_chat_cancel(struct dcc_chat *);

void
dcc_chat_in (gpointer data, gint source, GaimInputCondition condition)
{
	struct dcc_chat *chat = data;
	gchar buffer[IRC_BUF_LEN];
	gchar buf[128];
	int n = 0;
	GaimConversation *convo;
	gaim_debug(GAIM_DEBUG_MISC, "irc", "THIS IS TOO MUCH EFFORT\n");
	n = read (chat->fd, buffer, IRC_BUF_LEN);
	if (n > 0) {

		buffer[n] = 0;
		g_strstrip(buffer);

		/* Convert to HTML */
		if (strlen(buffer)) {
			gaim_debug(GAIM_DEBUG_INFO, "irc",
					   "DCC Message from: %s\n", chat->nick);
			irc_got_im(chat->gc, chat->nick, buffer, 0, 
				   time(NULL));
		}
	}	
	else	{
		g_snprintf (buf, sizeof buf, _("DCC Chat with %s closed"),
			    chat->nick);
		convo = gaim_conversation_new(GAIM_CONV_IM, chat->gc->account,
									  chat->nick);
		gaim_conversation_write(convo, NULL, buf, -1, WFLAG_SYSTEM,
								time(NULL));
		dcc_chat_cancel (chat);
	}
}

void 
irc_read_dcc_ack (gpointer data, gint source, GaimInputCondition condition) {
	/* Read ACK Here */

}

void 
dcc_send_callback (gpointer data, gint source, GaimInputCondition condition) {
#if 0
	struct irc_file_transfer *ift = data;
	struct sockaddr_in addr;
	int len = sizeof(addr);
		
	addr.sin_family = AF_INET;
	addr.sin_port = htons(ift->port);
	addr.sin_addr.s_addr = INADDR_ANY;

	ift->fd = accept(ift->fd, (struct sockaddr *)&addr, &len);
	if (!ift->fd) {
		/* FIXME: Handle this gracefully XXX */
		printf("Something bad happened here, bubba!\n");
		return;
	}
		
	/*	ift->awatcher = gaim_input_add(ift->fd, GAIM_INPUT_READ, irc_read_dcc_ack, ift); */
		
	if (transfer_out_do(ift->xfer, ift->fd, 0)) {
		gaim_input_remove(ift->watcher);
		ift->watcher = 0;
	}
#endif
}

void 
dcc_chat_callback (gpointer data, gint source, GaimInputCondition condition) {
	struct dcc_chat *chat = data;
	GaimConversation *convo;
	char buf[IRC_BUF_LEN];

	convo = gaim_conversation_new(GAIM_CONV_IM, chat->gc->account, chat->nick);

	chat->fd = source;
	g_snprintf (buf, sizeof buf,
		    _("DCC Chat with %s established"),
		    chat->nick);
	gaim_conversation_write(convo, NULL, buf, -1, WFLAG_SYSTEM, time(NULL));
	gaim_debug(GAIM_DEBUG_INFO, "irc",
			   "Chat with %s established\n", chat->nick);
	dcc_chat_list =  g_slist_append (dcc_chat_list, chat);
	gaim_input_remove(chat->inpa);
	chat->inpa = gaim_input_add(source, GAIM_INPUT_READ, dcc_chat_in, chat);
}

static void 
irc_got_chat_in(GaimConnection *gc, int id, char *who, int whisper, char *msg, time_t t)
{
	char *utf8 = irc_recv_convert(gc, msg);
	GString *str = decode_html(utf8);
	serv_got_chat_in(gc, id, who, whisper, str->str, t);
	g_string_free(str, TRUE);
	g_free(utf8);
}

static void 
handle_list(GaimConnection *gc, char *list)
{
	struct irc_data *id = gc->proto_data;
	char *tmp;
	GaimBlistNode *gnode, *bnode;

	tmp = g_utf8_strdown(list, -1);

	id->str = g_string_append_c(id->str, ' ');
	id->str = g_string_append(id->str, tmp);
	id->bc--;
	g_free(tmp);
	if (id->bc)
		return;


	for(gnode = gaim_get_blist()->root; gnode; gnode = gnode->next) {
		if(!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;
		for(bnode = gnode->child; bnode; bnode = bnode->next) {
			struct buddy *b = (struct buddy *)bnode;
			if(!GAIM_BLIST_NODE_IS_BUDDY(bnode))
				continue;
			if(b->account->gc == gc) {
				char *tmp = g_utf8_strdown(b->name, -1);
				char *x, *l;
				x = strstr(id->str->str, tmp);
				l = x + strlen(b->name);
				if (x && (*l != ' ' && *l != 0))
					x = 0;
				if (!GAIM_BUDDY_IS_ONLINE(b) && x)
					serv_got_update(gc, b->name, 1, 0, 0, 0, 0);
				else if (GAIM_BUDDY_IS_ONLINE(b) && !x)
					serv_got_update(gc, b->name, 0, 0, 0, 0, 0);
				g_free(tmp);
			}
		}
	}
	g_string_free(id->str, TRUE);
	id->str = g_string_new("");
}

static gboolean 
irc_request_buddy_update(gpointer data)
{
	GaimConnection *gc = data;
	struct irc_data *id = gc->proto_data;
	char buf[500];
	int n = g_snprintf(buf, sizeof(buf), "ISON");
	gboolean found = FALSE;

	GaimBlistNode *gnode, *bnode;

	if (id->bc)
		return TRUE;

	for(gnode = gaim_get_blist()->root; gnode; gnode = gnode->next) {
		if(!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;
		for(bnode = gnode->child; bnode; bnode = bnode->next) {
			struct buddy *b = (struct buddy *)bnode;
			if(!GAIM_BLIST_NODE_IS_BUDDY(bnode))
				continue;
			if(b->account->gc == gc) {
				if (n + strlen(b->name) + 2 > sizeof(buf)) {
					g_snprintf(buf + n, sizeof(buf) - n, "\r\n");
					irc_write(id->fd, buf, n);
					id->bc++;
					n = g_snprintf(buf, sizeof(buf), "ISON");
				}
				n += g_snprintf(buf + n, sizeof(buf) - n, " %s", b->name);

				found = TRUE;
			}
		}
	}

	if (found) {
		g_snprintf(buf + n, sizeof(buf) - n, "\r\n");
		irc_write(id->fd, buf, strlen(buf));
		id->bc++;
	}

	return TRUE;
}

static void 
handle_names(GaimConnection *gc, char *chan, char *names)
{
	GaimConversation *c = irc_find_chat(gc, chan);
	GaimChat *chat;
	char **buf, **tmp;

	if (!c) return;
	if (*names == ':') names++;

	chat = GAIM_CHAT(c);

	buf = g_strsplit(names, " ", -1);

	for (tmp = buf; *tmp; tmp++)
		gaim_chat_add_user(chat, *tmp, NULL);

	g_strfreev(buf);
}

static void 
handle_notopic(GaimConnection *gc, char *text)
{
	GaimConversation *c;

	if ((c = irc_find_chat(gc, text))) {
		char buf[IRC_BUF_LEN];

		g_snprintf(buf, sizeof(buf), _("No topic is set"));

		gaim_chat_set_topic(GAIM_CHAT(c), NULL, buf);
	}
}

static void 
handle_topic(GaimConnection *gc, char *text)
{
	GaimConversation *c;
	char *po = strchr(text, ' '), *buf;

	if (!po)
		return;

	*po = 0;
	po += 2;

	if ((c = irc_find_chat(gc, text))) {
                po = irc_recv_convert(gc, po);
		gaim_chat_set_topic(GAIM_CHAT(c), NULL, po);
		buf = g_strdup_printf(_("<B>%s has changed the topic to: %s</B>"), text, po);
		gaim_conversation_write(c, NULL, buf, -1, WFLAG_SYSTEM, time(NULL));
                g_free(buf);
                g_free(po);
	}
}

static gboolean 
mode_has_arg(GaimConnection *gc, char sign, char mode)
{
	struct irc_data *id = gc->proto_data;
	char *cm = id->chanmodes;
	int type = 0;

	if (strchr(id->nickmodes, mode))
		return TRUE;

	while (*cm) {
		if (*cm == ',')
			type++;
		else if (*cm == mode) {
			switch (type) {
			case 0:
			case 1:
				return TRUE;
			case 2:
				if (sign == '+')
					return TRUE;
			case 3:
				return FALSE;
			}
		}
		cm++;
	}

	return FALSE;
}

static void 
irc_chan_mode(GaimConnection *gc, char *room, char sign, char mode, char *argstr, char *who)
{
	GaimConversation *c = irc_find_chat(gc, room);
	char buf[IRC_BUF_LEN];
	char *nick = g_strndup(who, strchr(who, '!') - who);

	g_snprintf(buf, sizeof(buf), _("-:- mode/%s [%c%c %s] by %s"), 
		   room, sign, mode, strlen(argstr) ? argstr : "",
		   nick);
	g_free(nick);

	gaim_conversation_write(c, NULL, buf, -1, WFLAG_SYSTEM, time(NULL));
}

static void 
irc_user_mode(GaimConnection *gc, char *room, char sign, char mode, char *nick)
{
	GaimConversation *c = irc_find_chat(gc, room);
	GList *r;

	if (mode != 'o' && mode != 'v' && mode != 'h')
		return;

	if (!c)
		return;

	r = gaim_chat_get_users(GAIM_CHAT(c));
	while (r) {
		gboolean op = FALSE, halfop = FALSE, voice = FALSE;
		char *who = r->data;

		if (*who == '@') {
			op = TRUE;
			who++;
		}

		if (*who == '%') {
			halfop = TRUE;
			who++;
		}

		if (*who == '+') {
			voice = TRUE;
			who++;
		}

		if (!strcmp(who, nick)) {
			char *tmp, buf[IRC_BUF_LEN];

			if (mode == 'o') {
				if (sign == '-')
					op = FALSE;
				else
					op = TRUE;
			}

			if (mode == 'h') {
				if (sign == '-')
					halfop = FALSE;
				else
					halfop = TRUE;
			}

			if (mode == 'v') {
				if (sign == '-')
					voice = FALSE;
				else
					voice = TRUE;
			}

			tmp = g_strdup(r->data);
			g_snprintf(buf, sizeof(buf), "%s%s%s",
					   (op ? "@" : (halfop ? "%" : "")),
					   voice ? "+" : "", nick);
			gaim_chat_rename_user(GAIM_CHAT(c), tmp, buf);
			g_free(tmp);
			return;
		}
		r = r->next;
	}
}

static void 
handle_mode(GaimConnection *gc, char *word[], char *word_eol[], gboolean n324)
{
	struct irc_data *id = gc->proto_data;
	int offset = n324 ? 4 : 3;
	char *chan = word[offset];
	GaimConversation *c = irc_find_chat(gc, chan);
	char *modes = word[offset + 1];
	int len = strlen(word_eol[offset]) - 1;
	char sign = *modes++;
	int arg = 1;
	char *argstr;
	char *who = word[1];

	if (!c)
		return;

	if (word_eol[offset][len] == ' ')
		word_eol[offset][len] = 0;

	while (TRUE) {
		switch (*modes) {
		case 0:
			return;
		case '+':
		case '-':
			sign = *modes;
			break;
		default:
			if (mode_has_arg(gc, sign, *modes))
				argstr = word[++arg + offset];
			else
				argstr = "";
			if (strchr(id->nickmodes, *modes))
				irc_user_mode(gc, chan, sign, *modes, argstr);
			else if (strchr(who, '!'))				
				irc_chan_mode(gc, chan, sign, *modes, argstr, who);
		}
		modes++;
	}
}

static void
handle_version(GaimConnection *gc, char *word[], char *word_eol[], int num)
{
	struct irc_data *id = gc->proto_data;
	GString *str;

	id->liststr = g_string_new("");

	id->liststr = g_string_append(id->liststr, "<b>Version: </b>");
	id->liststr = g_string_append(id->liststr, word_eol[4]);

	str = decode_html(id->liststr->str);
	g_show_info_text(gc, NULL, 2, str->str, NULL);
	g_string_free(str, TRUE);
	g_string_free(id->liststr, TRUE);
	id->liststr = NULL;
}

static void 
handle_who(GaimConnection *gc, char *word[], char *word_eol[], int num)
{
	struct irc_data *id = gc->proto_data;
	char buf[IRC_BUF_LEN];

	if (!id->in_whois) {
		id->in_whois = TRUE;
		id->liststr = g_string_new("");
	}

	switch (num) {
	case 352:
		g_snprintf(buf, sizeof(buf), "<b>%s</b> (%s@%s): %s<br>", 
			   word[8], word[5], word[6], word_eol[11]);
		id->liststr = g_string_append(id->liststr, buf);
		break;
	}
}

/* Handle our whois stuff here.  You know what, I have a sore throat.  You know
 * what I think about that? I'm not too pleased with it.  Perhaps I should take
 * some medicine, or perhaps I should go to bed? Blah!! */

static void 
handle_whois(GaimConnection *gc, char *word[], char *word_eol[], int num)
{
	struct irc_data *id = gc->proto_data;
	char tmp[1024];

	if (!id->in_whois) {
		id->in_whois = TRUE;
		id->liststr = g_string_new("");
	} else {
		/* I can't decide if we should have one break or two */
		id->liststr = g_string_append(id->liststr, "<BR>");
		id->in_whois = TRUE;
	}

	switch (num) {
	case 311:
		g_snprintf(tmp, sizeof(tmp), "<b>%s: </b>", _("User"));
		id->liststr = g_string_append(id->liststr, tmp);
		break;
	case 312:
		g_snprintf(tmp, sizeof(tmp), "<b>%s: </b>", _("Server"));
		id->liststr = g_string_append(id->liststr, tmp);
		break;
	case 313:
		g_snprintf(tmp, sizeof(tmp), "<b>%s:</b> %s ", _("IRC Operator"), word[4]);
		id->liststr = g_string_append(id->liststr, tmp);
		break;
	case 314:
		g_snprintf(tmp, sizeof(tmp), "<b>%s: </b><b>%s</b> (%s@%s) %s",
			   _("User"), word[4], word[5], word[6], word_eol[8]);
		id->liststr = g_string_append(id->liststr, tmp);
		return;
	case 317:
		g_snprintf(tmp, sizeof(tmp), "<b>%s: </b>", _("Idle Time"));
		id->liststr = g_string_append(id->liststr, tmp);
		break;
	case 319:
		g_snprintf(tmp, sizeof(tmp), "<b>%s: </b>", _("Channels"));
		id->liststr = g_string_append(id->liststr, tmp);
		break;
	/* Numeric 320 is used by the freenode irc network for showing 
	 * that a user is identified to services (Jason Straw <misato@wopn.org>)*/
	case 320:
		g_snprintf(tmp, sizeof(tmp), _("%s is an Identified User"), word[4]);
		id->liststr = g_string_append(id->liststr, tmp);
		return;
	default:
		break;
	}

	if (word_eol[5][0] == ':')
		id->liststr = g_string_append(id->liststr, word_eol[5] + 1);
	/* Nicer idle time output, by jonas@birme.se */
	else if (isdigit(word_eol[5][0])) {
		time_t idle = atol(word_eol[5]);
		time_t signon = atol(strchr(word_eol[5], ' '));
				
		g_snprintf(tmp, sizeof(tmp), 
			   _("%ld seconds [signon: %s]"), (idle / 1000), ctime(&signon));
		id->liststr = g_string_append(id->liststr, tmp);
	}
	else
		id->liststr = g_string_append(id->liststr, word_eol[5]);
}

static void 
handle_roomlist(GaimConnection *gc, char *word[], char *word_eol[])
{
	struct irc_data *id = gc->proto_data;

	if (!id->in_list) {
		id->in_list = TRUE;
		id->liststr = g_string_new("");
	} else {
		id->liststr = g_string_append(id->liststr, "<BR>");
		id->in_list = TRUE;
	}

	id->liststr = g_string_append(id->liststr, word_eol[4]);
}

static void 
irc_change_nick(void *a, const char *b) {
	GaimConnection *gc = a;
	struct irc_data *id = gc->proto_data;
	char buf[IRC_BUF_LEN];	
	g_snprintf(buf, sizeof(buf), "NICK %s\r\n", b);
	irc_write(id->fd, buf, strlen(buf));
	gaim_connection_set_display_name(gc, b);
}

static void 
process_numeric(GaimConnection *gc, char *word[], char *word_eol[])
{
	const char *displayname = gaim_connection_get_display_name(gc);
	struct irc_data *id = gc->proto_data;
	char *text = word_eol[3];
	int n = atoi(word[2]);
	char tmp[1024];

	if (!g_ascii_strncasecmp(displayname, text, strlen(displayname)))
		text += strlen(displayname) + 1;
	if (*text == ':')
		text++;

	/* RPL_ and ERR_ */
	switch (n) {
	case 4:
		if (!strncmp(word[5], "u2.10", 5))
			id->six_modes = TRUE;
		else
			id->six_modes = FALSE;
		break;
	case 5:
		handle_005(gc, word, word_eol);
		break;
	case 301: /* RPL_AWAY */
		if (id->in_whois) {
			g_snprintf(tmp, sizeof(tmp), "<BR><b>%s: </b>", _("Away"));
			id->liststr = g_string_append(id->liststr, tmp);

			if (word_eol[5][0] == ':')
				id->liststr = g_string_append(id->liststr, word_eol[5] + 1);
			else
				id->liststr = g_string_append(id->liststr, word_eol[5]);
		} else
			irc_got_im(gc, word[4], word_eol[5], IM_FLAG_AWAY, time(NULL));
		break;
	case 303: /* RPL_ISON */
		handle_list(gc, &word_eol[4][1]);
		break;
	case 311: /* RPL_WHOISUSER */
	case 312: /* RPL_WHOISSERVER */
	case 313: /* RPL_WHOISOPERATOR */
	case 314: /* RPL_WHOWASUSER */
	case 317: /* RPL_WHOISIDLE */
	case 319: /* RPL_WHOISCHANNELS */
	case 320: /* FreeNode Identified */
		handle_whois(gc, word, word_eol, n);
		break;
	case 322: /* RPL_LIST */
		handle_roomlist(gc, word, word_eol);
		break;
	case 315: /* RPL_ENDOFWHO */
	case 318: /* RPL_ENDOFWHOIS */
	case 323: /* RPL_LISTEND */
	case 369: /* RPL_ENDOFWHOWAS */
		if ((id->in_whois || id->in_list) && id->liststr) {
			GString *str = decode_html(id->liststr->str);
			g_show_info_text(gc, NULL, 2, str->str, NULL);
			g_string_free(str, TRUE);
			g_string_free(id->liststr, TRUE);
			id->liststr = NULL;
			id->in_whois = FALSE;
			id->in_list = FALSE;
		}
		break;
	case 324: /* RPL_CHANNELMODEIS */
		handle_mode(gc, word, word_eol, TRUE);
		break;
	case 331: /* RPL_NOTOPIC */
		handle_notopic(gc, text);
		break;
	case 332: /* RPL_TOPIC */
		handle_topic(gc, text);
		break;
	case 351: /* RPL_VERSION */
		handle_version(gc, word, word_eol, n);
		break;
	case 352: /* RPL_WHOREPLY */
		handle_who(gc, word, word_eol, n);
		break;
	case 353: /* RPL_NAMREPLY */
		handle_names(gc, word[5], word_eol[6]);
		break;
	case 376: /* RPL_ENDOFMOTD */
		irc_request_buddy_update(gc);
		break;
	case 382: /* RPL_REHASHING */
		gaim_notify_error(gc, NULL, _("Rehashing server"), _("IRC Operator"));
		break;
	case 401: /* ERR_NOSUCHNICK */
		gaim_notify_error(gc, NULL, _("No such nick/channel"), _("IRC Error"));
		break;
	case 402: /* ERR_NOSUCHSERVER */
		gaim_notify_error(gc, NULL, _("No such server"), _("IRC Error"));
		break;
	case 422: /* ERR_NOMOTD */
		break;  /* drop it - bringing up dialog for NOMOTD is annoying */
	case 431: /* ERR_NONICKNAMEGIVEN */
		gaim_notify_error(gc, NULL, _("No nickname given"), _("IRC Error"));
		break;
	case 481: /* ERR_NOPRIVILEGES */
		gaim_notify_error(gc, NULL, _("You're not an IRC operator!"),
						  _("IRC Error"));
		break;		
	case 433:
		gaim_request_input(gc, NULL, _("That nick is already in use.  "
									   "Please enter a new nick"),
						   NULL, gaim_connection_get_display_name(gc), FALSE,
						   _("OK"), G_CALLBACK(irc_change_nick),
						   _("Cancel"), NULL, gc);
		break;
	default:
		/* Other error messages */
		if (n > 400 && n < 502) {
			char errmsg[IRC_BUF_LEN];
			char *errmsg1 = strrchr(text, ':');

			g_snprintf(errmsg, sizeof(errmsg), "IRC Error %d", n);

			if (errmsg) {
				gaim_notify_error(gc, NULL, errmsg,
								  (errmsg1 ? errmsg1 + 1 : NULL));
			}
		}

		break;
	}
}

static gboolean 
is_channel(GaimConnection *gc, const char *name)
{
	struct irc_data *id = gc->proto_data;
	if (strchr(id->chantypes, *name))
		return TRUE;
	return FALSE;
}

static void 
irc_rem_chat_bud(GaimConnection *gc, char *nick, GaimConversation *b, char *reason)
{

	GaimChat *chat;

	if (b) {
		GList *r;

		chat = GAIM_CHAT(b);

		r = gaim_chat_get_users(chat);

		while (r) {
			char *who = r->data;
			if (*who == '@')
				who++;
			if (*who == '%')
				who++;
			if (*who == '+')
				who++;
			if (!gaim_utf8_strcasecmp(who, nick)) {
				gaim_chat_remove_user(chat, who, reason);
				break;
			}
			r = r->next;
		}
	} else {
		GSList *bcs = gc->buddy_chats;
		while (bcs) {
			GaimConversation *bc = bcs->data;
			irc_rem_chat_bud(gc, nick, bc, reason);
			bcs = bcs->next;
		}
	}
}

static void 
irc_change_name(GaimConnection *gc, char *old, char *new)
{
	GSList *bcs = gc->buddy_chats;
	char buf[IRC_BUF_LEN];

	while (bcs) {
		GaimConversation *b = bcs->data;
		GaimChat *chat;
		GList *r;

		chat = GAIM_CHAT(b);

		r = gaim_chat_get_users(chat);

		while (r) {
			char *who = r->data;
			int n = 0;
			if (*who == '@')
				buf[n++] = *who++;
			if (*who == '%')
				buf[n++] = *who++;
			if (*who == '+')
				buf[n++] = *who++;
			g_snprintf(buf + n, sizeof(buf) - n, "%s", new);
			if (!strcmp(who, old)) {
				char *tmp = g_strdup(r->data);
				gaim_chat_rename_user(chat, tmp, buf);
				r = gaim_chat_get_users(chat);
				g_free(tmp);
				break;
			} else
				r = r->next;
		}
		bcs = bcs->next;
	}
}

static void 
handle_privmsg(GaimConnection *gc, char *to, char *nick, char *msg)
{
	if (is_channel(gc, to)) {
		GaimConversation *c = irc_find_chat(gc, to);
		if (!c)
			return;
		irc_got_chat_in(gc, gaim_chat_get_id(GAIM_CHAT(c)),
						nick, 0, msg, time(NULL));
	} else {
		char *tmp = g_malloc(strlen(nick) + 2);
		g_snprintf(tmp, strlen(nick) + 2, "@%s", nick);
		if (gaim_find_conversation(tmp))
			irc_got_im(gc, tmp, msg, 0, time(NULL));
		else {
			*tmp = '+';
			if (gaim_find_conversation(tmp))
				irc_got_im(gc, tmp, msg, 0, time(NULL));
			else
				irc_got_im(gc, nick, msg, 0, time(NULL));
		}
		g_free(tmp);
	}
}

static void 
dcc_chat_init(struct dcc_chat *data) {
	if (g_list_find(gaim_connections_get_all(), data->gc)) {
		gaim_proxy_connect(data->gc->account, data->ip_address, data->port, dcc_chat_callback, data);
	} else {
		g_free(data);
	}
}

static void 
dcc_chat_cancel(struct dcc_chat *data){
	if (g_list_find(gaim_connections_get_all(), data->gc) && find_dcc_chat(data->gc, data->nick)) {
		dcc_chat_list = g_slist_remove(dcc_chat_list, data); 
		gaim_input_remove (data->inpa);
		close (data->fd);
	}
	g_free(data);
}

static void 
irc_convo_closed(GaimConnection *gc, char *who)
{
	struct dcc_chat *dchat = find_dcc_chat(gc, who);
	if (!dchat)
		return;

	dcc_chat_cancel(dchat);
}

static void
irc_xfer_init(struct gaim_xfer *xfer)
{
	struct irc_xfer_data *data = (struct irc_xfer_data *)xfer->data;

	gaim_xfer_start(xfer, -1, data->ip, data->port);
}

static void
irc_xfer_end(struct gaim_xfer *xfer)
{
	struct irc_xfer_data *data = (struct irc_xfer_data *)xfer->data;

	data->idata->file_transfers = g_slist_remove(data->idata->file_transfers,
												 xfer);

	g_free(data);
	xfer->data = NULL;
}

static void
irc_xfer_cancel_send(struct gaim_xfer *xfer)
{
	struct irc_xfer_data *data = (struct irc_xfer_data *)xfer->data;

	data->idata->file_transfers = g_slist_remove(data->idata->file_transfers,
												 xfer);

	g_free(data);
	xfer->data = NULL;
}

static void
irc_xfer_cancel_recv(struct gaim_xfer *xfer)
{
	struct irc_xfer_data *data = (struct irc_xfer_data *)xfer->data;

	data->idata->file_transfers = g_slist_remove(data->idata->file_transfers,
												 xfer);

	g_free(data);
	xfer->data = NULL;
}

static void
irc_xfer_ack(struct gaim_xfer *xfer, const char *buffer, size_t size)
{
	guint32 pos;

	pos = htonl(gaim_xfer_get_bytes_sent(xfer));

	write(xfer->fd, (char *)&pos, 4);
}

/* NOTE: This was taken from irssi. Thanks irssi! */

static gboolean
is_numeric(const char *str, char end_char)
{
	g_return_val_if_fail(str != NULL, FALSE);

	if (*str == '\0' || *str == end_char)
		return FALSE;

	while (*str != '\0' && *str != end_char) {
		if (*str < '0' || *str > '9')
			return FALSE;

		str++;
	}

	return TRUE;
}

#define get_params_match(params, pos) \
	(is_numeric(params[pos], '\0') && \
	is_numeric(params[(pos)+1], '\0') && atol(params[(pos)+1]) < 65536 && \
	is_numeric(params[(pos)+2], '\0'))

/* Return number of parameters in `params' that belong to file name.
   Normally it's paramcount-3, but I don't think anything forbids of
   adding some extension where there could be more parameters after
   file size.

   MIRC sends filenames with spaces quoted ("file name"), but I'd rather
   not trust that entirely either. At least some clients that don't really
   understand the problem with spaces in file names sends the file name
   without any quotes. */
static int
get_file_params_count(char **params, int paramcount)
{
	int pos, best;

	if (*params[0] == '"') {
		/* quoted file name? */
		for (pos = 0; pos < paramcount - 3; pos++) {
			if (params[pos][strlen(params[pos]) - 1] == '"' &&
				get_params_match(params, pos + 1)) {

				return pos + 1;
			}
		}
	}

	best = paramcount - 3;

	for (pos = paramcount - 3; pos > 0; pos--) {
		if (get_params_match(params, pos))
			best = pos;
	}

	return best;
}

static void 
handle_ctcp(GaimConnection *gc, char *to, char *nick,
			char *msg, char *word[], char *word_eol[])
{
	struct irc_data *id = gc->proto_data;
	char buf[IRC_BUF_LEN];
	char out[IRC_BUF_LEN];

	if (!g_ascii_strncasecmp(msg, "VERSION", 7)) {
		g_snprintf(buf, sizeof(buf), "\001VERSION Gaim " VERSION ": The Penguin Pimpin' "
			   "Multi-protocol Messaging Client: " WEBSITE "\001");
		irc_send_notice (gc, nick, buf);
		g_snprintf(out, sizeof(out), ">> CTCP VERSION requested from %s", nick);
		gaim_notify_info(gc, NULL, out, _("IRC CTCP info"));
	}
	else if (!g_ascii_strncasecmp(msg, "CLIENTINFO", 10)) {
		g_snprintf(buf, sizeof(buf), "\001CLIENTINFO USERINFO CLIENTINFO VERSION\001");
		irc_send_notice (gc, nick, buf);
		g_snprintf(out, sizeof(out), ">> CTCP CLIENTINFO requested from %s", nick);
		gaim_notify_info(gc, NULL, out, _("IRC CTCP info"));
	}
	else if (!g_ascii_strncasecmp(msg, "USERINFO", 8)) {
		g_snprintf(buf, sizeof(buf), "\001USERINFO Alias: %s\001", gc->account->alias);
		irc_send_notice (gc, nick, buf);
		g_snprintf(out, sizeof(out), ">> CTCP USERINFO requested from %s", nick);
		gaim_notify_info(gc, NULL, out, _("IRC CTCP info"));
	}
	else if (!g_ascii_strncasecmp(msg, "ACTION", 6)) {
		char *po = strchr(msg + 6, 1);
		char *tmp;
		if (po) *po = 0;
		tmp = g_strconcat("/me", msg + 6, NULL);
		handle_privmsg(gc, to, nick, tmp);
		g_free(tmp);
	}
	else if (!g_ascii_strncasecmp(msg, "PING", 4)) {
		g_snprintf(buf, sizeof(buf), "\001%s\001", msg);
		irc_send_notice (gc, nick, buf);
		g_snprintf(out, sizeof(out), ">> CTCP PING requested from %s", nick);
		gaim_notify_info(gc, NULL, out, _("IRC CTCP info"));
	}
	else if (!g_ascii_strncasecmp(msg, "DCC CHAT", 8)) {
		char **chat_args = g_strsplit(msg, " ", 5);
		char ask[1024];
		struct dcc_chat *dccchat = g_new0(struct dcc_chat, 1);
		dccchat->gc = gc;	
		g_snprintf(dccchat->ip_address, sizeof(dccchat->ip_address), chat_args[3]);	
		dccchat->port=atoi(chat_args[4]);		
		g_snprintf(dccchat->nick, sizeof(dccchat->nick), nick);	
		g_snprintf(ask, sizeof(ask), _("%s would like to establish a DCC chat"), nick);

		gaim_request_action(gc, NULL, ask,
							_("This requires a direct connection to be "
							  "established between the two computers.  "
							  "Messages sent will not pass through the "
							  "IRC server"), 0, dccchat, 2,
							_("Connect"), G_CALLBACK(dcc_chat_init),
							_("Cancel"), G_CALLBACK(dcc_chat_cancel));
	}
	else if (!g_ascii_strncasecmp(msg, "DCC SEND", 8)) {
		struct gaim_xfer *xfer;
		char **send_args;
		char *ip, *filename;
		struct irc_xfer_data *xfer_data;
		size_t size;
		int param_count, file_params, len;
		int port;

		/* Okay, this is ugly, but should get us past "DCC SEND" */
		msg = strstr(msg, "DCC SEND");
		msg = strchr(msg, ' ') + 1;
		msg = strchr(msg, ' ') + 1;

		/* SEND <file name> <address> <port> <size> [...] */
		send_args = g_strsplit(msg, " ", -1);

		for (param_count = 0; send_args[param_count] != NULL; param_count++)
			;

		if (param_count < 4) {
			char buf[IRC_BUF_LEN];

			g_snprintf(buf, sizeof(buf),
					   _("Received an invalid file send request from %s."),
					   nick);

			gaim_notify_error(gc, NULL, buf, _("IRC Error"));

			return;
		}

		file_params = get_file_params_count(send_args, param_count);

		/* send_args[paramcount - 1][strlen(send_args[5])-1] = 0; */

		/* Give these better names. */
		ip       = send_args[file_params];
		port     = atoi(send_args[file_params + 1]);
		size     = atoi(send_args[file_params + 2]);

		send_args[file_params] = NULL;

		filename = g_strjoinv(" ", send_args);

		g_strfreev(send_args);

		len = strlen(filename);

		if (len > 1 && *filename == '"' && filename[len - 1] == '"') {
			/* "file name" - MIRC sends filenames with spaces like this */
			filename[len - 1] = '\0';
			g_memmove(filename, filename + 1, len);
		}

		/* Setup the IRC-specific transfer data. */
		xfer_data = g_malloc0(sizeof(struct irc_xfer_data));
		xfer_data->ip    = ip;
		xfer_data->port  = port;
		xfer_data->idata = id;

		/* Build the file transfer handle. */
		xfer = gaim_xfer_new(gc->account, GAIM_XFER_RECEIVE, nick);
		xfer->data = xfer_data;

		/* Set the info about the incoming file. */
		gaim_xfer_set_filename(xfer, filename);
		gaim_xfer_set_size(xfer, size);

		g_free(filename);

		/* Setup our I/O op functions. */
		gaim_xfer_set_init_fnc(xfer,        irc_xfer_init);
		gaim_xfer_set_end_fnc(xfer,         irc_xfer_end);
		gaim_xfer_set_cancel_send_fnc(xfer, irc_xfer_cancel_send);
		gaim_xfer_set_cancel_recv_fnc(xfer, irc_xfer_cancel_recv);
		gaim_xfer_set_ack_fnc(xfer,         irc_xfer_ack);

		/* Keep track of this transfer for later. */
		id->file_transfers = g_slist_append(id->file_transfers, xfer);

		/* Now perform the request! */
		gaim_xfer_request(xfer);
	}
}

static gboolean 
irc_parse(GaimConnection *gc, char *buf)
{
	struct irc_data *idata = gc->proto_data;
	gchar outbuf[IRC_BUF_LEN];
	char *word[PDIWORDS], *word_eol[PDIWORDS];
	char pdibuf[522];
	char *ex, ip[128], nick[128];
	char *cmd;

	/* Check for errors */
	
	if (*buf != ':') {
		if (!strncmp(buf, "NOTICE ", 7))
			buf += 7;
		if (!strncmp(buf, "PING ", 5)) {
			int r = FALSE;
			g_snprintf(outbuf, sizeof(outbuf), "PONG %s\r\n", buf + 5);
			if (irc_write(idata->fd, outbuf, strlen(outbuf)) < 0) {
				gaim_connection_error(gc, _("Unable to write"));
				r = TRUE;
			}
			return r;
		}
		/* XXX doesn't handle ERROR */
		return FALSE;
	}

	if (!idata->online) {
		/* Now lets sign ourselves on */
		gaim_connection_set_state(gc, GAIM_CONNECTED);
		serv_finish_login(gc);

		/* we don't call this now because otherwise some IRC servers might not like us */
		idata->timer = g_timeout_add(20000, irc_request_buddy_update, gc);
		idata->online = TRUE;
	}

	buf++;

	process_data_init(pdibuf, buf, word, word_eol, FALSE);

	if (atoi(word[2])) {
		if (*word_eol[3])
			process_numeric(gc, word, word_eol);
		return FALSE;
	}

	cmd = word[2];

	ex = strchr(pdibuf, '!');
	if (!ex) {
		strncpy(ip, pdibuf, sizeof(ip));
		ip[sizeof(ip)-1] = 0;
		strncpy(nick, pdibuf, sizeof(nick));
		nick[sizeof(nick)-1] = 0;
	} else {
		strncpy(ip, ex + 1, sizeof(ip));
		ip[sizeof(ip)-1] = 0;
		strncpy(nick, pdibuf, sizeof(nick));
		nick[sizeof(nick)-1] = 0;
		if ((ex - pdibuf) < sizeof (nick))
			nick[ex - pdibuf] = 0; /* cut the buffer at the '!' */
	}

	if (!strcmp(cmd, "INVITE")) {
		GHashTable *components = g_hash_table_new_full(g_str_hash, g_str_equal,
				g_free, g_free);

		g_hash_table_replace(components, g_strdup("channel"), g_strdup(word[4]));

		serv_got_chat_invite(gc, word[4] + 1, nick, NULL, components);
	} else if (!strcmp(cmd, "JOIN")) {
		irc_parse_join(gc, nick, word, word_eol);
	} else if (!strcmp(cmd, "KICK")) {
		if (!strcmp(gaim_connection_get_display_name(gc), word[4])) {
			GaimConversation *c = irc_find_chat(gc, word[3]);
			if (!c)
				return FALSE;
			gc->buddy_chats = g_slist_remove(gc->buddy_chats, c);
			gaim_conversation_set_account(c, NULL);
			g_snprintf(outbuf, sizeof(outbuf), _("You have been kicked from %s: %s"),
				   word[3], *word_eol[5] == ':' ? word_eol[5] + 1 : word_eol[5]);
			gaim_notify_error(gc, NULL, outbuf, _("IRC Error"));
		} else {
			char *reason = *word_eol[5] == ':' ? word_eol[5] + 1 : word_eol[5];
			char *msg = g_strdup_printf(_("Kicked by %s: %s"), nick, reason);
			GaimConversation *c = irc_find_chat(gc, word[3]);
			irc_rem_chat_bud(gc, word[4], c, msg);
			g_free(msg);
		}
	} else if (!strcmp(cmd, "KILL")) { /* */
	} else if (!strcmp(cmd, "MODE")) {
		handle_mode(gc, word, word_eol, FALSE);
	} else if (!strcmp(cmd, "NICK")) {
		char *new = *word_eol[3] == ':' ? word_eol[3] + 1 : word_eol[3];
		if (!strcmp(gaim_connection_get_display_name(gc), nick))
			gaim_connection_set_display_name(gc, new);
		irc_change_name(gc, nick, new);
	} else if (!strcmp(cmd, "NOTICE")) {
		irc_parse_notice(gc, nick, ex, word, word_eol);
	} else if (!strcmp(cmd, "PART")) {
		if (!irc_parse_part(gc, nick, cmd, word, word_eol))
			return FALSE;
	} else if (!strcmp(cmd, "PRIVMSG")) {
		char *to, *msg;
		if (!*word[3])
			return FALSE;
		to = word[3];
		msg = *word_eol[4] == ':' ? word_eol[4] + 1 : word_eol[4];
		if (msg[0] == 1 && msg[strlen (msg) - 1] == 1) { /* ctcp */
			if (!g_ascii_strncasecmp(msg + 1, "DCC ", 4))
				process_data_init(pdibuf, buf, word, word_eol, TRUE);
			handle_ctcp(gc, to, nick, msg + 1, word, word_eol);
		} else {
			handle_privmsg(gc, to, nick, msg);
		}
	} else if (!strcmp(cmd, "PONG")) { /* */
	} else if (!strcmp(cmd, "QUIT")) {
		irc_rem_chat_bud(gc, nick, irc_find_chat(gc, word[3]), *word_eol[3] == ':' ? word_eol[3] + 1 : word_eol[3]);
	} else if (!strcmp(cmd, "TOPIC")) {
		irc_parse_topic(gc, nick, word, word_eol);
	} else if (!strcmp(cmd, "WALLOPS")) { /* Don't know if a dialog box is the right way? */
		char *msg = strrchr(word_eol[0], ':');
		if (msg)
			gaim_notify_error(gc, NULL, msg+1, _("IRC Operator"));
	}

	return FALSE;
}

/* CTCP by jonas@birme.se */
static void
irc_parse_notice(GaimConnection *gc, char *nick, char *ex,
                 char *word[], char *word_eol[])
{
	char buf[IRC_BUF_LEN];

	if (!g_ascii_strcasecmp(word[4], ":\001CLIENTINFO")) {
		char *p = g_strrstr(word_eol[5], "\001");
		*p = 0;
		g_snprintf(buf, sizeof(buf), "CTCP Answer: %s", word_eol[5]);
		gaim_notify_info(gc, NULL, buf, _("CTCP ClientInfo"));

	} else if (!g_ascii_strcasecmp(word[4], ":\001USERINFO")) {
		char *p = g_strrstr(word_eol[5], "\001");
		*p = 0;
		g_snprintf(buf, sizeof(buf), "CTCP Answer: %s", word_eol[5]);
		gaim_notify_info(gc, NULL, buf, _("CTCP UserInfo"));

	} else if (!g_ascii_strcasecmp(word[4], ":\001VERSION")) {
		char *p = g_strrstr(word_eol[5], "\001");
		*p = 0;
		g_snprintf(buf, sizeof(buf), "CTCP Answer: %s", word_eol[5]);
		gaim_notify_info(gc, NULL, buf, _("CTCP Version"));

	} else if (!g_ascii_strcasecmp(word[4], ":\001PING")) {
		char *p = g_strrstr(word_eol[5], "\001");
		struct timeval ping_time;
		struct timeval now;
		gchar **vector;

		if (p)
			*p = 0;

		vector = g_strsplit(word_eol[5], ".", 2);

		if (gettimeofday(&now, NULL) == 0 && vector != NULL) {
			if (vector[1] && now.tv_usec - atol(vector[1]) < 0) {
				ping_time.tv_sec = now.tv_sec - atol(vector[0]) - 1;
				ping_time.tv_usec = now.tv_usec - atol(vector[1]) + 1000000;

			} else {
				ping_time.tv_sec = now.tv_sec - atol(vector[0]);
				if(vector[1])
					ping_time.tv_usec = now.tv_usec - atol(vector[1]);
			}

			g_snprintf(buf, sizeof(buf),
					   "CTCP Ping reply from %s: %lu.%.03lu seconds",
					   nick, ping_time.tv_sec, (ping_time.tv_usec/1000));

			gaim_notify_info(gc, NULL, buf, _("CTCP Ping"));
			g_strfreev(vector);
		}
	} else {
		if (*word_eol[4] == ':') word_eol[4]++;
		if (ex)
			irc_got_im(gc, nick, word_eol[4], 0, time(NULL));
	}
}

static void
irc_parse_join(GaimConnection *gc, char *nick,
               char *word[], char *word_eol[])
{
	char *chan = *word[3] == ':' ? word[3] + 1 : word[3];
	static int id = 1;
	GaimConversation *c;
	char *hostmask, *p;

	if (!gaim_utf8_strcasecmp(gaim_connection_get_display_name(gc), nick)) {
		serv_got_joined_chat(gc, id++, chan);
	} else {
		c = irc_find_chat(gc, chan);
		if (c) {
			hostmask = g_strdup(word[1]);
			p = strchr(hostmask, '!');
			if (p) {
				char *pend = strchr(p, ' ');
				if (pend) {
					*pend = 0;
				}

				gaim_chat_add_user(GAIM_CHAT(c), nick, p + 1);

				g_free(hostmask);
			}
		}
	}
}
	
static void
irc_parse_topic(GaimConnection *gc, char *nick,
                char *word[], char *word_eol[])
{
	GaimConversation *c = irc_find_chat(gc, word[3]);
	char *topic = irc_recv_convert(gc, *word_eol[4] == ':' ? word_eol[4] + 1 : word_eol[4]);
	char buf[IRC_BUF_LEN];

	if (c) {
		gaim_chat_set_topic(GAIM_CHAT(c), nick, topic);
		g_snprintf(buf, sizeof(buf),
				   _("<B>%s has changed the topic to: %s</B>"), nick, topic);

		gaim_conversation_write(c, NULL, buf, -1, WFLAG_SYSTEM, time(NULL));
	}
	g_free(topic);
}

static gboolean
irc_parse_part(GaimConnection *gc, char *nick, char *cmd,
               char *word[], char *word_eol[])
{
	char *chan = cmd + 5;
	GaimConversation *c;
	GaimChat *chat;
	char *reason = word_eol[4];
	GList *r;

	if (*chan == ':')
		chan++;
	if (*reason == ':')
		reason++;
	if (!(c = irc_find_chat(gc, chan)))
		return FALSE;
	if (!strcmp(nick, gaim_connection_get_display_name(gc))) {
		serv_got_chat_left(gc, gaim_chat_get_id(GAIM_CHAT(c)));
		return FALSE;
	}

	chat = GAIM_CHAT(c);

	r = gaim_chat_get_users(GAIM_CHAT(c));

	while (r) {
		char *who = r->data;
		if (*who == '@')
			who++;
		if (*who == '%')
			who++;
		if (*who == '+')
			who++;
		if (!gaim_utf8_strcasecmp(who, nick)) {
			gaim_chat_remove_user(chat, who, reason);
			break;
		}
		r = r->next;
	}
	return TRUE;
}

static void 
irc_callback(gpointer data, gint source, GaimInputCondition condition)
{
	GaimConnection *gc = data;
	struct irc_data *idata = gc->proto_data;
	int i = 0;
	gchar buf[1024];
	gboolean off;

	i = read(idata->fd, buf, 1024);
	if (i <= 0) {
		gaim_connection_error(gc, "Read error");
		return;
	}

	idata->rxqueue = g_realloc(idata->rxqueue, i + idata->rxlen + 1);
	memcpy(idata->rxqueue + idata->rxlen, buf, i);
	idata->rxlen += i;
	idata->rxqueue[idata->rxlen] = 0;

	while (1) {
		char *d, *e;
		int len;

		if (!idata->rxqueue || ((e = strchr(idata->rxqueue, '\n')) == NULL))
			return;

		len = e - idata->rxqueue + 1;
		d = g_strndup(idata->rxqueue, len);
		g_strchomp(d);
		gaim_debug(GAIM_DEBUG_MISC, "irc", "S: %s\n", d);

		/* REMOVE ME BEFORE SUBMIT! */
		/*fprintf(stderr, "IRC S: %s\n", d);*/

		idata->rxlen -= len;
		if (idata->rxlen) {
			char *tmp = g_strdup(e + 1);
			g_free(idata->rxqueue);
			idata->rxqueue = tmp;
		} else {
			g_free(idata->rxqueue);
			idata->rxqueue = NULL;
		}

		off = irc_parse(gc, d);

		g_free(d);

		if (off)
			return;
	}
}

static void 
irc_login_callback(gpointer data, gint source, GaimInputCondition condition)
{
	GaimConnection *gc = data;
	GaimAccount *account = gaim_connection_get_account(gc);
	struct irc_data *idata;
	char hostname[256];
	char buf[IRC_BUF_LEN];
	char *test;
	const char *alias;
	const char *charset = gaim_account_get_string(account, "charset", "UTF-8");
	GError *err = NULL;

	idata = gc->proto_data;

	if (source < 0) {
		gaim_connection_error(gc, "Write error");
		return;
	}
	idata->fd = source;
	
	/* Try a quick conversion to see if the specified encoding is OK */
	test = g_convert("test", strlen("test"), charset,
			 "UTF-8", NULL, NULL, &err);
	if (err) {
		gaim_debug(GAIM_DEBUG_ERROR, "irc",
			   "Couldn't initialize %s for IRC charset conversion, using ISO-8859-1\n",
			   charset);
		gaim_account_set_string(account, "charset", "UTF-8");
	}
	
	g_free(test);
	
	gethostname(hostname, sizeof(hostname) - 1);
	hostname[sizeof(hostname) - 1] = 0;

	if (!*hostname)
		g_snprintf(hostname, sizeof(hostname), "localhost");

	if (gaim_account_get_password(account) != NULL) {
		g_snprintf(buf, sizeof(buf), "PASS %s\r\n",
				   gaim_account_get_password(account));

		if (irc_write(idata->fd, buf, strlen(buf)) < 0) {
			gaim_connection_error(gc, "Write error");
			return;
		}
	}

	alias = gaim_account_get_alias(account);

	g_snprintf(buf, sizeof(buf), "USER %s %s %s :%s\r\n",
		   g_get_user_name(), hostname,
		   idata->server,
		   (alias == NULL ? "gaim" : alias));

	if (irc_write(idata->fd, buf, strlen(buf)) < 0) {
		gaim_connection_error(gc, "Write error");
		return;
	}

	g_snprintf(buf, sizeof(buf), "NICK %s\r\n",
			   gaim_connection_get_display_name(gc));

	if (irc_write(idata->fd, buf, strlen(buf)) < 0) {
		gaim_connection_error(gc, "Write error");
		return;
	}

	gc->inpa = gaim_input_add(idata->fd, GAIM_INPUT_READ, irc_callback, gc);
}

static void 
irc_login(GaimAccount *account)
{
	const char *username = gaim_account_get_username(account);
	char buf[IRC_BUF_LEN];
	int rc;

	GaimConnection *gc;
	struct irc_data *idata;
	char **parts;

	gc = gaim_account_get_connection(account);
	idata = gc->proto_data = g_new0(struct irc_data, 1);

	parts = g_strsplit(username, "@", 2);
	gaim_connection_set_display_name(gc, parts[0]);
	idata->server = g_strdup(parts[1]);
	g_strfreev(parts);

	g_snprintf(buf, sizeof(buf), _("Signon: %s"), username);
	gaim_connection_update_progress(gc, buf, 1, 2);

	idata->chantypes = g_strdup("#&!+");
	idata->chanmodes = g_strdup("beI,k,lnt");
	idata->nickmodes = g_strdup("ohv");
	idata->str = g_string_new("");
	idata->fd = -1;

	rc = gaim_proxy_connect(account, idata->server,
			   gaim_account_get_int(account, "port", 6667),
			   irc_login_callback, gc);
	
	if (rc != 0) {
		gaim_connection_error(gc, _("Unable to create socket"));
		return;
	}
}

static void 
irc_close(GaimConnection *gc)
{
	struct irc_data *idata = (struct irc_data *)gc->proto_data;

	gchar buf[IRC_BUF_LEN];

	if (idata->str->len > 0) {
		g_snprintf(buf, sizeof(buf), "QUIT :%s\r\n", idata->str->str);
	} else {
		g_snprintf(buf, sizeof(buf), 
			   "QUIT :Download Gaim [%s]\r\n", WEBSITE);
	}
	irc_write(idata->fd, buf, strlen(buf));

	if (idata->rxqueue)
		g_free(idata->rxqueue);

	idata->rxqueue = NULL;
	idata->rxlen = 0;

	/* Kill any existing transfers */
	while (idata->file_transfers) {
		struct gaim_xfer *xfer;

		xfer = (struct gaim_xfer *)idata->file_transfers->data;

		gaim_xfer_end(xfer);
		gaim_xfer_destroy(xfer);

		idata->file_transfers = idata->file_transfers->next;
	}
	idata->file_transfers = NULL;


	g_free(idata->chantypes);
	g_free(idata->chanmodes);
	g_free(idata->nickmodes);

	g_string_free(idata->str, TRUE);
	if (idata->liststr)
		g_string_free(idata->liststr, TRUE);

	if (idata->timer)
		g_source_remove(idata->timer);

	if (gc->inpa)
		gaim_input_remove(gc->inpa);

	close(idata->fd);
	g_free(gc->proto_data);
}

static void 
set_mode_3(GaimConnection *gc, const char *who, int sign, int mode,
		       int start, int end, char *word[])
{
	struct irc_data *id = gc->proto_data;
	char buf[IRC_BUF_LEN];
	int left;
	int i = start;

	while (1) {
		left = end - i;
		switch (left) {
		case 0:
			return;
		case 1:
			g_snprintf(buf, sizeof(buf), "MODE %s %c%c %s\r\n",
				   who, sign, mode, word[i]);
			i += 1;
			break;
		case 2:
			g_snprintf(buf, sizeof(buf), "MODE %s %c%c%c %s %s\r\n",
				   who, sign, mode, mode, word[i], word[i + 1]);
			i += 2;
			break;
		default:
			g_snprintf(buf, sizeof(buf), "MODE %s %c%c%c%c %s %s %s\r\n",
				   who, sign, mode, mode, mode,
				   word[i], word[i + 1], word[i + 2]);
			i += 2;
			break;
		}
		irc_write(id->fd, buf, strlen(buf));
		if (left < 3)
			return;
	}
}

static void 
set_mode_6(GaimConnection *gc, const char *who, int sign, int mode,
		       int start, int end, char *word[])
{
	struct irc_data *id = gc->proto_data;
	char buf[IRC_BUF_LEN];
	int left;
	int i = start;

	while (1) {
		left = end - i;
		switch (left) {
		case 0:
			return;
		case 1:
			g_snprintf(buf, sizeof(buf), "MODE %s %c%c %s\r\n",
				   who, sign, mode, word[i]);
			i += 1;
			break;
		case 2:
			g_snprintf(buf, sizeof(buf), "MODE %s %c%c%c %s %s\r\n",
				   who, sign, mode, mode, word[i], word[i + 1]);
			i += 2;
			break;
		case 3:
			g_snprintf(buf, sizeof(buf), "MODE %s %c%c%c%c %s %s %s\r\n",
				   who, sign, mode, mode, mode,
				   word[i], word[i + 1], word[i + 2]);
			i += 3;
			break;
		case 4:
			g_snprintf(buf, sizeof(buf), "MODE %s %c%c%c%c%c %s %s %s %s\r\n",
				   who, sign, mode, mode, mode, mode,
				   word[i], word[i + 1], word[i + 2], word[i + 3]);
			i += 4;
			break;
		case 5:
			g_snprintf(buf, sizeof(buf), "MODE %s %c%c%c%c%c%c %s %s %s %s %s\r\n",
				   who, sign, mode, mode, mode, mode, mode,
				   word[i], word[i + 1], word[i + 2],
				   word[i + 3], word[i + 4]);
			i += 5;
			break;
		default:
			g_snprintf(buf, sizeof(buf), "MODE %s %c%c%c%c%c%c%c %s %s %s %s %s %s\r\n",
				   who, sign, mode, mode, mode, mode, mode, mode,
				   word[i], word[i + 1], word[i + 2],
				   word[i + 3], word[i + 4], word[i + 5]);
			i += 6;
			break;
		}
		irc_write(id->fd, buf, strlen(buf));
		if (left < 6)
			return;
	}
}

static void 
set_mode(GaimConnection *gc, const char *who, int sign, int mode, char *word[])
{
	struct irc_data *id = gc->proto_data;
	int i = 2;

	while (1) {
		if (!*word[i]) {
			if (i == 2)
				return;
			if (id->six_modes)
				set_mode_6(gc, who, sign, mode, 2, i, word);
			else
				set_mode_3(gc, who, sign, mode, 2, i, word);
			return;
		}
		i++;
	}
}

static void 
set_chan_mode(GaimConnection *gc, const char *chan, const char *mode_str)
{
	struct irc_data *id = gc->proto_data;
	char buf[IRC_BUF_LEN];

	if ((mode_str[0] == '-') || (mode_str[0] == '+')) {
		g_snprintf(buf, sizeof(buf), "MODE %s %s\r\n", chan, mode_str);
		irc_write(id->fd, buf, strlen(buf));
	}
}

static int 
handle_command(GaimConnection *gc, const char *who, const char *in_what)
{
	char buf[IRC_BUF_LEN];
	char pdibuf[IRC_BUF_LEN];
	char *word[PDIWORDS], *word_eol[PDIWORDS];
	char *tmp = g_strdup(in_what);
	GString *str = encode_html(tmp);
	char *intl;
	int len;
	struct dcc_chat *dccchat = find_dcc_chat(gc, who);
	struct irc_data *id = gc->proto_data;
	char *what = str->str;

	g_free(tmp);

	if (*what != '/') {
		if (dccchat) {
			intl = irc_send_convert(gc, what, sizeof(buf), &len);
			g_snprintf(buf, sizeof(buf), "%s\r\n", intl);
			g_free(intl);
			irc_write(dccchat->fd, buf, strlen(buf));
			g_string_free(str, TRUE);
			return 1;
		}
		irc_send_privmsg (gc, who, what, TRUE);
		g_string_free(str, TRUE);
		return 1;
	}
	
	process_data_init(pdibuf, what + 1, word, word_eol, TRUE);
	g_string_free(str, FALSE);
	if (!g_ascii_strcasecmp(pdibuf, "ME")) {
		if (dccchat) {
			intl = irc_send_convert(gc, word_eol[2], sizeof(buf), &len);
			g_snprintf(buf, sizeof(buf), "\001ACTION %s\001\r\n", intl);
			g_free(intl);
			irc_write(dccchat->fd, buf, strlen(buf));
			g_free(what);
			return 1;
		}
		g_snprintf(buf, sizeof(buf), "\001ACTION %s\001", word_eol[2]);
		irc_send_privmsg (gc, who, buf, FALSE);
		g_free(what);
		return 1;
	} else if (!g_ascii_strcasecmp(pdibuf, "INVITE")) {
		char buf[IRC_BUF_LEN];
		g_snprintf(buf, sizeof(buf), "INVITE %s\r\n", word_eol[2]);
		irc_write(id->fd, buf, strlen(buf));
	} else if (!g_ascii_strcasecmp(pdibuf, "TOPIC")) {
		if (!*word_eol[2]) {
			GaimConversation *c;
			GaimChat *chat;

			c = irc_find_chat(gc, who);
			chat = GAIM_CHAT(c);

			g_snprintf(buf, sizeof(buf), _("Topic for %s is %s"),
					   who, (gaim_chat_get_topic(chat)
							 ? gaim_chat_get_topic(chat)
							 : "(no topic set)"));

			gaim_conversation_write(c, NULL, buf, -1,
									WFLAG_SYSTEM | WFLAG_NOLOG, time(NULL));
		} else {
			/* This could be too long */
			intl = irc_send_convert(gc, word_eol[2], sizeof(buf), &len);
			g_snprintf(buf, sizeof(buf), "TOPIC %s :%s\r\n", who, intl);
			g_free(intl);
			irc_write(id->fd, buf, strlen(buf));
		}
	} else if (!g_ascii_strcasecmp(pdibuf, "NICK")) {
		if (!*word_eol[2]) {
			g_free(what);
			return -EINVAL;
		}
		g_snprintf(buf, sizeof(buf), "NICK %s\r\n", word_eol[2]);
		irc_write(id->fd, buf, strlen(buf));
	} else if (!g_ascii_strcasecmp(pdibuf, "OP")) {
		set_mode(gc, who, '+', 'o', word);
	} else if (!g_ascii_strcasecmp(pdibuf, "DEOP")) {
		set_mode(gc, who, '-', 'o', word);
	} else if (!g_ascii_strcasecmp(pdibuf, "VOICE")) {
		set_mode(gc, who, '+', 'v', word);
	} else if (!g_ascii_strcasecmp(pdibuf, "DEVOICE")) {
		set_mode(gc, who, '-', 'v', word);
	} else if (!g_ascii_strcasecmp(pdibuf, "MODE")) {
		set_chan_mode(gc, who, word_eol[2]);
	} else if (!g_ascii_strcasecmp(pdibuf, "QUOTE")) {
		if (!*word_eol[2]) {
			g_free(what);
			return -EINVAL;
		}
		g_snprintf(buf, sizeof(buf), "%s\r\n", word_eol[2]);
		irc_write(id->fd, buf, strlen(buf));
	} else if (!g_ascii_strcasecmp(pdibuf, "SAY")) {
		if (!*word_eol[2]) {
			g_free(what);
			return -EINVAL;
		}
		irc_send_privmsg (gc, who, word_eol[2], TRUE);
		return 1;
	} else if (!g_ascii_strcasecmp(pdibuf, "MSG")) {
		if (!*word[2]) {
			g_free(what);
			return -EINVAL;
		}
		if (!*word_eol[3]) {
			g_free(what);
			return -EINVAL;
		}
		irc_send_privmsg (gc, word[2], word_eol[3], TRUE);
	} else if (!g_ascii_strcasecmp(pdibuf, "KICK")) {
		if (!*word[2]) {
			g_free(what);
			return -EINVAL;
		}
		if (*word_eol[3]) {
			intl = irc_send_convert(gc, word_eol[3], sizeof(buf), &len);
			g_snprintf(buf, sizeof(buf), "KICK %s %s :%s\r\n", who, word[2], intl);
			g_free(intl);
		} else
			g_snprintf(buf, sizeof(buf), "KICK %s %s\r\n", who, word[2]);
		irc_write(id->fd, buf, strlen(buf));
	} else if (!g_ascii_strcasecmp(pdibuf, "JOIN") || !g_ascii_strcasecmp(pdibuf, "J")) {
		if (!*word[2]) {
			g_free(what);
			return -EINVAL;
		}
		if (*word[3])
			g_snprintf(buf, sizeof(buf), "JOIN %s %s\r\n", word[2], word[3]);
		else
			g_snprintf(buf, sizeof(buf), "JOIN %s\r\n", word[2]);
		irc_write(id->fd, buf, strlen(buf));
	} else if (!g_ascii_strcasecmp(pdibuf, "PART")) {
		const char *chan = *word[2] ? word[2] : who;
		char *reason = word_eol[3];
		GaimConversation *c;
		if (!is_channel(gc, chan)) {
			g_free(what);
			return -EINVAL;
		}
		c = irc_find_chat(gc, chan);
		if (*reason) {
			intl = irc_send_convert(gc, reason, sizeof(buf), &len);
			g_snprintf(buf, sizeof(buf), "PART %s :%s\r\n", chan, intl);
			g_free(intl);
		} else
			g_snprintf(buf, sizeof(buf), "PART %s\r\n", chan);
		irc_write(id->fd, buf, strlen(buf));
		if (c) {
			gc->buddy_chats = g_slist_remove(gc->buddy_chats, c);
			gaim_conversation_set_account(c, NULL);
			g_snprintf(buf, sizeof(buf), _("You have left %s"), chan);
			gaim_notify_info(gc, NULL, buf, _("IRC Part"));
		}
	} else if (!g_ascii_strcasecmp(pdibuf, "WHOIS")) {
		g_snprintf(buf, sizeof(buf), "WHOIS %s\r\n", word_eol[2]);
		irc_write(id->fd, buf, strlen(buf));
	} else if (!g_ascii_strcasecmp(pdibuf, "WHOWAS")) {
		g_snprintf(buf, sizeof(buf), "WHOWAS %s\r\n", word_eol[2]);
		irc_write(id->fd, buf, strlen(buf));
	} else if (!g_ascii_strcasecmp(pdibuf, "LIST")) {
		g_snprintf(buf, sizeof(buf), "LIST\r\n");
		irc_write(id->fd, buf, strlen(buf));
	} else if (!g_ascii_strcasecmp(pdibuf, "QUIT")) {
		char *reason = word_eol[2];
		id->str = g_string_insert(id->str, 0, reason);
		do_quit();
	} else if (!g_ascii_strcasecmp(pdibuf, "VERSION")) {
		g_snprintf(buf, sizeof(buf), "VERSION\r\n");
		irc_write(id->fd, buf, strlen(buf));
	} else if (!g_ascii_strcasecmp(pdibuf, "W")) {
		g_snprintf(buf, sizeof(buf), "WHO *\r\n");
		irc_write(id->fd, buf, strlen(buf));
	} else if (!g_ascii_strcasecmp(pdibuf, "REHASH")) {
		g_snprintf(buf, sizeof(buf), "REHASH\r\n");
		irc_write(id->fd, buf, strlen(buf));
	} else if (!g_ascii_strcasecmp(pdibuf, "RESTART")) {
		g_snprintf(buf, sizeof(buf), "RESTART\r\n");
		irc_write(id->fd, buf, strlen(buf));
	} else if (!g_ascii_strcasecmp(pdibuf, "CTCP")) {
		if (!g_ascii_strcasecmp(word[2], "CLIENTINFO")) {
			if (word[3])
				irc_ctcp_clientinfo(gc, word[3]);
		} else if (!g_ascii_strcasecmp(word[2], "USERINFO")) {
			if (word[3])
				irc_ctcp_userinfo(gc, word[3]);
		} else if (!g_ascii_strcasecmp(word[2], "VERSION")) {
			if (word[3])
				irc_ctcp_version(gc, word[3]);

		} else if (!g_ascii_strcasecmp(word[2], "PING")) {
			if (word[3])
				irc_ctcp_ping(gc, word[3]);
		}
	} else if (!g_ascii_strcasecmp(pdibuf, "DCC")) {
		GaimConversation *c = NULL;
		if (!g_ascii_strcasecmp(word[2], "CHAT")) {
			if (word[3])
				irc_start_chat(gc, word[3]);
			
			if (is_channel(gc, who)) {
				c = irc_find_chat(gc, who);
			} else {
				c = gaim_find_conversation(who);
			}
			if (c) {
				gaim_conversation_write(c, NULL,
										_("<I>Requesting DCC CHAT</I>"),
										-1, WFLAG_SYSTEM, time(NULL));
			}
		}
	} else if (!g_ascii_strcasecmp(pdibuf, "HELP")) {
		GaimConversation *c = NULL;
		if (is_channel(gc, who)) {
			c = irc_find_chat(gc, who);
		} else {
			c = gaim_find_conversation(who);
		}
		if (!c) {
			g_free(what);
			return -EINVAL;
		}
		if (!g_ascii_strcasecmp(word[2], "OPER")) {
			gaim_conversation_write(c, NULL,
				_("<B>Operator commands:<BR>"
				  "REHASH RESTART</B>"),
				-1, WFLAG_NOLOG, time(NULL));
		} else if (!g_ascii_strcasecmp(word[2], "CTCP")) {
			gaim_conversation_write(c, NULL,
			_("<B>CTCP commands:<BR>"
			  "CLIENTINFO <nick><BR>"
			  "USERINFO <nick><BR>"
			  "VERSION <nick><BR>"
			  "PING <nick></B><BR>"),
			-1, WFLAG_NOLOG, time(NULL));
		} else if (!g_ascii_strcasecmp(word[2], "DCC")) {
			gaim_conversation_write(c, NULL,
				_("<B>DCC commands:<BR>"
				  "CHAT <nick></B>"),
				-1, WFLAG_NOLOG, time(NULL));
		} else {
			gaim_conversation_write(c, NULL,
				_("<B>Currently supported commands:<BR>"
				  "WHOIS INVITE NICK LIST<BR>"
				  "JOIN PART TOPIC KICK<BR>"
				  "OP DEOP VOICE DEVOICE<BR>"
				  "ME MSG QUOTE SAY QUIT<BR>"
				  "MODE VERSION W WHOWAS<BR>"
				  "Type /HELP OPER for operator commands<BR>"
				  "Type /HELP CTCP for CTCP commands<BR>"
				  "Type /HELP DCC for DCC commands"),
				-1, WFLAG_NOLOG, time(NULL));
		}
	} else {
		GaimConversation *c = NULL;
		if (is_channel(gc, who)) {
			c = irc_find_chat(gc, who);
		} else {
			c = gaim_find_conversation(who);
		}
		if (!c) {
			g_free(what);
			return -EINVAL;
		}

		gaim_conversation_write(c, NULL, _("<B>Unknown command</B>"),
								-1, WFLAG_NOLOG, time(NULL));
	}
	g_free(what);
	return 0;
}

static int 
send_msg(GaimConnection *gc, const char *who, const char *what)
{
	char *cr = strchr(what, '\n');
	if (cr) {
		int ret = 0;
		while (TRUE) {
			if (cr)
				*cr = 0;
			ret = handle_command(gc, who, what);
			if (!cr)
				break;
			what = cr + 1;
			if (!*what)
				break;
			*cr = '\n';
			cr = strchr(what, '\n');
		}
		return ret;
	} else
		return handle_command(gc, who, what);
}

static void 
irc_chat_invite(GaimConnection *gc, int idn, const char *message, const char *name) {
	char buf[IRC_BUF_LEN]; 
	struct irc_data *id = gc->proto_data;
	GaimConversation *c = gaim_find_chat(gc, idn);
	g_snprintf(buf, sizeof(buf), "INVITE %s %s\r\n", name, c->name);
	irc_write(id->fd, buf, strlen(buf));
}

static int 
irc_send_im(GaimConnection *gc, const char *who, const char *what, int len, int flags)
{
	if (*who == '@' || *who == '%' || *who == '+')
		return send_msg(gc, who + 1, what);
	return send_msg(gc, who, what);
}

/* IRC doesn't have a buddy list, but we can still figure out who's online with ISON */
static void 
irc_add_buddy(GaimConnection *gc, const char *who) {}
static void 
irc_remove_buddy(GaimConnection *gc, char *who, char *group) {}

static GList *
irc_chat_info(GaimConnection *gc)
{
	GList *m = NULL;
	struct proto_chat_entry *pce;

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("Channel:");
	pce->identifier = "channel";
	m = g_list_append(m, pce);

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("Password:");
	pce->identifier = "password";
	m = g_list_append(m, pce);

	return m;
}

static void
irc_join_chat(GaimConnection *gc, GHashTable *data)
{
	struct irc_data *id = gc->proto_data;
	char buf[IRC_BUF_LEN];
	char *name, *pass;

	if (!data)
		return;

	name = g_hash_table_lookup(data, "channel");
	pass = g_hash_table_lookup(data, "password");
	if (pass) {
		g_snprintf(buf, sizeof(buf), "JOIN %s %s\r\n", name, pass);
	} else
		g_snprintf(buf, sizeof(buf), "JOIN %s\r\n", name);
	irc_write(id->fd, buf, strlen(buf));
}

static void 
irc_chat_leave(GaimConnection *gc, int id)
{
	struct irc_data *idata = gc->proto_data;
	GaimConversation *c = gaim_find_chat(gc, id);
	char buf[IRC_BUF_LEN];

	if (!c) return;

	g_snprintf(buf, sizeof(buf), "PART %s\r\n", c->name);
	irc_write(idata->fd, buf, strlen(buf));
}

static int 
irc_chat_send(GaimConnection *gc, int id, char *what)
{
	GaimConversation *c = gaim_find_chat(gc, id);
	if (!c)
		return -EINVAL;
	if (send_msg(gc, c->name, what) > 0)
		serv_got_chat_in(gc, gaim_chat_get_id(GAIM_CHAT(c)),
			(char *)gaim_connection_get_display_name(gc), 0, what, time(NULL));
	return 0;
}

static GList *
irc_away_states(GaimConnection *gc)
{
	return g_list_append(NULL, GAIM_AWAY_CUSTOM);
}

static void 
irc_set_away(GaimConnection *gc, char *state, char *msg)
{
	struct irc_data *idata = gc->proto_data;
	char buf[IRC_BUF_LEN];

	if (gc->away) {
		g_free(gc->away);
		gc->away = NULL;
	}

	if (msg) {
		g_snprintf(buf, sizeof(buf), "AWAY :%s\r\n", msg);
		gc->away = g_strdup(msg);
	} else
		g_snprintf(buf, sizeof(buf), "AWAY\r\n");

	irc_write(idata->fd, buf, strlen(buf));
}

static const char *
irc_list_icon(GaimAccount *a, struct buddy *b)
{
	return "irc";
}

static void irc_list_emblems(struct buddy *b, char **se, char **sw, char **nw, char **ne)
{
	if (b->present == GAIM_BUDDY_OFFLINE)
		*se = "offline";
}

static int 
getlocalip(char *ip) /* Thanks, libfaim */
{
	struct hostent *hptr;
	char localhost[129];
	long unsigned add;
	
	/* XXX if available, use getaddrinfo() */
	/* XXX allow client to specify which IP to use for multihomed boxes */
	
	if (gethostname(localhost, 128) < 0)
		return -1;

	if (!(hptr = gethostbyname(localhost)))
		return -1;
	
	memcpy(&add, hptr->h_addr_list[0], 4);
	add = htonl(add);
	g_snprintf(ip, 11, "%lu", add);

	return 0;
}

static void 
dcc_chat_connected(gpointer data, gint source, GdkInputCondition condition)
{
	struct dcc_chat *chat = data;
	GaimConversation *convo;
	char buf[128];
	struct sockaddr_in addr;
	int addrlen = sizeof (addr);
	addr.sin_family = AF_INET;
	addr.sin_port = htons (chat->port);
	addr.sin_addr.s_addr = INADDR_ANY;
	chat->fd = accept (chat->fd, (struct sockaddr *) (&addr), &addrlen);
	if (!chat->fd) {
		dcc_chat_cancel (chat);
		convo = gaim_conversation_new(GAIM_CONV_IM, chat->gc->account,
									  chat->nick);
		g_snprintf (buf, sizeof buf, _("DCC Chat with %s closed"),
			    chat->nick);
		gaim_conversation_write(convo, NULL, buf, -1,
								WFLAG_SYSTEM, time(NULL));
		return;
	}
	chat->inpa =
		gaim_input_add (chat->fd, GAIM_INPUT_READ, dcc_chat_in, chat);
	convo = gaim_conversation_new(GAIM_CONV_IM, chat->gc->account, chat->nick);
	g_snprintf (buf, sizeof buf, _("DCC Chat with %s established"),
				chat->nick);
	gaim_conversation_write(convo, NULL, buf, -1, WFLAG_SYSTEM, time(NULL));
	gaim_debug(GAIM_DEBUG_INFO, "irc",
			   "Chat with %s established\n", chat->nick);
	dcc_chat_list = g_slist_append (dcc_chat_list, chat);
}
#if 0
static void 
irc_ask_send_file(GaimConnection *gc, char *destsn) {
	struct irc_data *id = (struct irc_data *)gc->proto_data;
	struct irc_file_transfer *ift = g_new0(struct irc_file_transfer, 1);
	char *localip = (char *)malloc(12);

	if (getlocalip(localip) == -1) {
		free(localip);
		return;
	} 

	ift->type = IFT_SENDFILE_OUT;
	ift->sn = g_strdup(destsn);
	ift->gc = gc;
	snprintf(ift->ip, sizeof(ift->ip), "%s", localip);
	id->file_transfers = g_slist_append(id->file_transfers, ift);

	ift->xfer = transfer_out_add(gc, ift->sn);
}

static struct 
irc_file_transfer *find_ift_by_xfer(GaimConnection *gc,
									struct file_transfer *xfer) {
		
	GSList *g = ((struct irc_data *)gc->proto_data)->file_transfers;
	struct irc_file_transfer *f = NULL;

	while (g) {
		f = (struct irc_file_transfer *)g->data;
		if (f->xfer == xfer)
			break;
		g = g->next;
		f = NULL;
	}

	return f;
}

static void 
irc_file_transfer_data_chunk(GaimConnection *gc, struct file_transfer *xfer, const char *data, int len) {
	struct irc_file_transfer *ift = find_ift_by_xfer(gc, xfer);
	guint32 pos;
	
	ift->cur += len;
	pos = htonl(ift->cur);
	write(ift->fd, (char *)&pos, 4);

	// FIXME: You should check to verify that they received the data when
	// you are sending a file ...
}

static void 
irc_file_transfer_cancel (GaimConnection *gc, struct file_transfer *xfer) {
	struct irc_data *id = (struct irc_data *)gc->proto_data;
	struct irc_file_transfer *ift = find_ift_by_xfer(gc, xfer);

	printf("Our shit got canceled, yo!\n");

	/* Remove the FT from our list of transfers */
	id->file_transfers = g_slist_remove(id->file_transfers, ift);

	gaim_input_remove(ift->watcher);

	/* Close our FT because we're done */
	close(ift->fd);

	g_free(ift->sn);
	g_free(ift->name);

	g_free(ift);
}

static void 
irc_file_transfer_done(GaimConnection *gc, struct file_transfer *xfer) {
	struct irc_data *id = (struct irc_data *)gc->proto_data;
	struct irc_file_transfer *ift = find_ift_by_xfer(gc, xfer);


	printf("Our shit be done, yo.\n");
	
	/* Remove the FT from our list of transfers */
	id->file_transfers = g_slist_remove(id->file_transfers, ift);

	gaim_input_remove(ift->watcher);

	/* Close our FT because we're done */
	close(ift->fd);

	g_free(ift->sn);
	g_free(ift->name);

	g_free(ift);
}

static void 
irc_file_transfer_out (GaimConnection *gc, struct file_transfer *xfer, const char *name, int totfiles, int totsize) {
	struct irc_file_transfer *ift = find_ift_by_xfer(gc, xfer);
	struct sockaddr_in addr;
	char buf[IRC_BUF_LEN];
	int len;
	
	
	ift->fd = socket (AF_INET, SOCK_STREAM, 0);
	addr.sin_family = AF_INET;
	addr.sin_port = 0;
	addr.sin_addr.s_addr = INADDR_ANY;
	bind (ift->fd, (struct sockaddr *) &addr, sizeof(addr));
	listen(ift->fd, 1);

	len = sizeof(addr);
	getsockname (ift->fd, (struct sockaddr *) &addr, &len);

	ift->port = ntohs(addr.sin_port);

	ift->watcher = gaim_input_add (ift->fd, GAIM_INPUT_READ, dcc_send_callback, ift);
	printf("watcher is %d\n", ift->watcher);

	snprintf(buf, sizeof(buf), "\001DCC SEND %s %s %d %d\001\n", name, ift->ip, ift->port, totsize);
	printf("Trying: %s\n", buf);
	irc_send_im (gc, ift->sn, buf, -1, 0);
}

static void 
irc_file_transfer_in(GaimConnection *gc,
				 struct file_transfer *xfer, int offset) {

	struct irc_file_transfer *ift = find_ift_by_xfer(gc, xfer);

	ift->xfer = xfer;
	gaim_proxy_connect(gc->account, ift->ip, ift->port, dcc_recv_callback, ift);
}
#endif

static void 
irc_ctcp_clientinfo(GaimConnection *gc, const char *who)
{
	char buf[IRC_BUF_LEN];

	snprintf (buf, sizeof buf, "\001CLIENTINFO\001");
	irc_send_privmsg(gc, who, buf, FALSE);
}

static void 
irc_ctcp_userinfo(GaimConnection *gc, const char *who)
{
	char buf[IRC_BUF_LEN];

	snprintf (buf, sizeof buf, "\001USERINFO\001");
	irc_send_privmsg(gc, who, buf, FALSE);
}

static void 
irc_ctcp_version(GaimConnection *gc, const char *who)
{
	char buf[IRC_BUF_LEN];

	snprintf (buf, sizeof buf, "\001VERSION\001");
	irc_send_privmsg(gc, who, buf, FALSE);
}

static void 
irc_ctcp_ping(GaimConnection *gc, const char *who)
{
	char buf[IRC_BUF_LEN];
	struct timeval now;

	gettimeofday(&now, NULL);
	g_snprintf (buf, sizeof(buf), "\001PING %lu.%.03lu\001", now.tv_sec,
			now.tv_usec/1000);
	irc_send_privmsg(gc, who, buf, FALSE);
}

static void 
irc_send_notice(GaimConnection *gc, char *who, char *what)
{
	char buf[IRC_BUF_LEN], *intl;
	struct irc_data *id = gc->proto_data;
	int len;
	
	intl = irc_send_convert(gc, what, 501, &len);
	g_snprintf(buf, sizeof(buf), "NOTICE %s :%s\r\n", who, intl);
	g_free(intl);
	irc_write(id->fd, buf, strlen(buf));
}

/* Don't call this guy with fragment = 1 for anything but straight
 * up privmsgs.  (no CTCP/whatever)  It's still dangerous for CTCPs
 * (it might not include the trailing \001), but I think this behavior
 * is generally better than not fragmenting at all on lots of our
 * packets. */
/* From RFC2812:
 * IRC messages are always lines of characters terminated with a CR-LF
 * (Carriage Return - Line Feed) pair, and these messages SHALL NOT
 * exceed 512 characters in length, counting all characters including
 * the trailing CR-LF. Thus, there are 510 characters maximum allowed
 * for the command and its parameters. */
/* So apparently that includes all the inter-server crap, which is up
 * to NINETY-THREE chars on dancer, which seems to be a pretty liberal
 * ircd.  My rough calculation for now is ":<nick>!~<user>@<host> ",
 * where <host> is a max of an (uncalculated) 63 chars.  Thanks to
 * trelane and #freenode for giving a hand here. */
static void 
irc_send_privmsg(GaimConnection *gc, const char *who, const char *what, gboolean fragment)
{
	char buf[IRC_BUF_LEN], *intl;
	struct irc_data *id = gc->proto_data;
	/* 512 - 12 (for PRIVMSG" "" :""\r\n") - namelen - nicklen - 68 */
	int nicklen = (gc->account->alias && strlen(gc->account->alias)) ? strlen(gc->account->alias) : 4;
	int max = 444 - strlen(who) - strlen(g_get_user_name()) - nicklen;
	
	int len;
	
	do {
		/* the \001 on CTCPs may cause a problem here for some 
		 * charsets, but probably not ones people use for IRC. */
		intl = irc_send_convert(gc, what, max, &len);
		g_snprintf(buf, sizeof(buf), "PRIVMSG %s :%s\r\n", who, intl);
		g_free(intl);
		irc_write(id->fd, buf, strlen(buf));
		what += len;
	} while (fragment && strlen(what));
}

static void 
irc_start_chat(GaimConnection *gc, const char *who) {
	struct dcc_chat *chat;
	int len;
	struct sockaddr_in addr;
	char buf[IRC_BUF_LEN];
	
	/* Create a socket */
	chat = g_new0 (struct dcc_chat, 1);
	chat->fd = socket (AF_INET, SOCK_STREAM, 0);
	chat->gc = gc;
	g_snprintf (chat->nick, sizeof (chat->nick), "%s", who);
	if (chat->fd < 0)  {
		dcc_chat_cancel (chat);
		return;
	}
	addr.sin_family = AF_INET;
	addr.sin_port = 0;
	addr.sin_addr.s_addr = INADDR_ANY;
	bind (chat->fd, (struct sockaddr *) &addr, sizeof (addr));
	listen (chat->fd, 1);
	len = sizeof (addr);
	getsockname (chat->fd, (struct sockaddr *) &addr, &len);
	chat->port = ntohs (addr.sin_port);
	getlocalip(chat->ip_address);
	chat->inpa =
		gaim_input_add (chat->fd, GAIM_INPUT_READ, dcc_chat_connected,
				chat);
	g_snprintf (buf, sizeof buf, "\001DCC CHAT chat %s %d\001\n",
		    chat->ip_address, chat->port);
	irc_send_im (gc, who, buf, -1, 0);
}

static void 
irc_get_info(GaimConnection *gc, const char *who)
{
	struct irc_data *idata = gc->proto_data;
	char buf[IRC_BUF_LEN];

	if (*who == '@')
		who++;
	if (*who == '%')
		who++;
	if (*who == '+')
		who++;

	g_snprintf(buf, sizeof(buf), "WHOIS %s\r\n", who);
	irc_write(idata->fd, buf, strlen(buf));
}

static GList *
irc_buddy_menu(GaimConnection *gc, const char *who)
{
	GList *m = NULL;
	struct proto_buddy_menu *pbm;

	pbm = g_new0(struct proto_buddy_menu, 1);
	pbm->label = _("DCC Chat");
	pbm->callback = irc_start_chat;
	pbm->gc = gc;
	m = g_list_append(m, pbm);
	/*
	  pbm = g_new0(struct proto_buddy_menu, 1);
	  pbm->label = _("DCC Send");
	  pbm->callback = irc_ask_send_file;
	  pbm->gc = gc;
	  m = g_list_append(m, pbm);
	*/
	
	pbm = g_new0(struct proto_buddy_menu, 1);
	pbm->label = _("CTCP ClientInfo");
	pbm->callback = irc_ctcp_clientinfo;
	pbm->gc = gc;
	m = g_list_append(m, pbm);
	
	pbm = g_new0(struct proto_buddy_menu, 1);
	pbm->label = _("CTCP UserInfo");
	pbm->callback = irc_ctcp_userinfo;
	pbm->gc = gc;
	m = g_list_append(m, pbm);

	pbm = g_new0(struct proto_buddy_menu, 1);
	pbm->label = _("CTCP Version");
	pbm->callback = irc_ctcp_version;
	pbm->gc = gc;
	m = g_list_append(m, pbm);

	pbm = g_new0(struct proto_buddy_menu, 1);
	pbm->label = _("CTCP Ping");
	pbm->callback = irc_ctcp_ping;
	pbm->gc = gc;
	m = g_list_append(m, pbm);

	return m;
}

static GaimPluginProtocolInfo prpl_info =
{
	GAIM_PROTO_IRC,
	OPT_PROTO_CHAT_TOPIC | OPT_PROTO_PASSWORD_OPTIONAL,
	NULL,
	NULL,
	irc_list_icon,
	irc_list_emblems,
	NULL,
	NULL,
	irc_away_states,
	NULL,
	irc_buddy_menu,
	irc_chat_info,
	irc_login,
	irc_close,
	irc_send_im,
	NULL,
	NULL,
	irc_get_info,
	irc_set_away,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	irc_add_buddy,
	NULL,
	irc_remove_buddy,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	irc_join_chat,
	irc_chat_invite,
	irc_chat_leave,
	NULL,
	irc_chat_send,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	irc_convo_closed,
	NULL
};

static GaimPluginInfo info =
{
	2,                                                /**< api_version    */
	GAIM_PLUGIN_PROTOCOL,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	"prpl-irc",                                       /**< id             */
	"IRC",                                            /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("IRC Protocol Plugin"),
	                                                  /**  description    */
	N_("IRC Protocol Plugin"),
	NULL,                                             /**< author         */
	WEBSITE,                                          /**< homepage       */

	NULL,                                             /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&prpl_info                                        /**< extra_info     */
};

static void
__init_plugin(GaimPlugin *plugin)
{
	GaimAccountUserSplit *split;
	GaimAccountOption *option;

	split = gaim_account_user_split_new(_("Server"), DEFAULT_SERVER, '@');
	prpl_info.user_splits = g_list_append(prpl_info.user_splits, split);


	option = gaim_account_option_int_new(_("Port"), "port", 6667);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);

	option = gaim_account_option_string_new(_("Encoding"), "charset",
											"ISO-8859-1");
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);

	my_protocol = plugin;
}

GAIM_INIT_PLUGIN(irc, __init_plugin, info);
