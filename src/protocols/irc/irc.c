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
#include "multi.h"
#include "prpl.h"
#include "gaim.h"
#include "proxy.h"

#ifdef _WIN32
#include "win32dep.h"
#endif

#include "pixmaps/protocols/irc/irc_icon.xpm"

#define IRC_BUF_LEN 4096
#define PDIWORDS 32

#define USEROPT_SERV      0
#define USEROPT_PORT      1
#define USEROPT_CHARSET   2

static struct prpl *my_protocol = NULL;

/* for win32 compatability */
G_MODULE_IMPORT GSList *connections;


/* Datastructs */
struct dcc_chat
{
	struct gaim_connection *gc;	
	char ip_address[12];	
	int port;		
	int fd;			
	int inpa;		
	char nick[80];		
};

struct irc_file_transfer {
	enum { IFT_SENDFILE_IN, IFT_SENDFILE_OUT } type;
	struct file_transfer *xfer;
	char *sn;
	char *name;
	int len;
	int watcher;
	int awatcher;
	char ip[12];
	int port;
	int fd;
	int cur;
	struct gaim_connection *gc;
};

struct irc_data {
	int fd;
	gboolean online;
	guint32 timer;

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
static void irc_start_chat(struct gaim_connection *gc, char *who);
static void irc_ctcp_clientinfo(struct gaim_connection *gc, char *who);
static void irc_ctcp_userinfo(struct gaim_connection *gc, char *who);
static void irc_ctcp_version(struct gaim_connection *gc, char *who);
static void irc_ctcp_ping(struct gaim_connection *gc, char *who);

static void irc_send_privmsg(struct gaim_connection *gc, char *who, char *what, gboolean fragment);
static void irc_send_notice(struct gaim_connection *gc, char *who, char *what);

static char *irc_send_convert(struct gaim_connection *gc, char *string, int maxlen, int *done);
static char *irc_recv_convert(struct gaim_connection *gc, char *string);
static void irc_parse_notice(struct gaim_connection *gc, char *nick, char *ex,
                            char *word[], char *word_eol[]);
static void irc_parse_join(struct gaim_connection *gc, char *nick,
                          char *word[], char *word_eol[]);
static gboolean irc_parse_part(struct gaim_connection *gc, char *nick, char *cmd,
                          char *word[], char *word_eol[]);
static void irc_parse_topic(struct gaim_connection *gc, char *nick,
                           char *word[], char *word_eol[]);

static void dcc_chat_cancel(struct dcc_chat *);

/* Global variables */
GSList *dcc_chat_list = NULL;

struct dcc_chat *
find_dcc_chat (struct gaim_connection *gc, char *nick)
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
	debug_printf("IRC C: %s", data);
	return write(fd, data, len);
}

static char *
irc_send_convert(struct gaim_connection *gc, char *string, int maxlen, int *done)
{
	char *converted = g_malloc(maxlen + 1);
	gchar *inptr = string, *outptr = converted;
	int inleft = strlen(string), outleft = maxlen;
	GIConv conv;
	
	conv = g_iconv_open(gc->user->proto_opt[USEROPT_CHARSET], "UTF-8");
	if (g_iconv(conv, &inptr, &inleft, &outptr, &outleft) == -1) {
		debug_printf("IRC charset conversion error\n");
		debug_printf("Sending as UTF-8 (this is a hack!)\n");
		g_free(converted);
		*done = maxlen;
		return(g_strndup(string, maxlen));
	}
	
	*done = strlen(string) - inleft;
	*outptr = '\0';
	return(converted);
}

static char *
irc_recv_convert(struct gaim_connection *gc, char *string)
{
	char *utf8;
	GError *err = NULL;
	
	utf8 = g_convert(string, strlen(string), "UTF-8",
			 gc->user->proto_opt[USEROPT_CHARSET], NULL, NULL, &err);
	if (err) {
		debug_printf("IRC recv conversion error: %s\n", err->message);
		utf8 = g_strdup(_("(There was an error converting this message.  Check the 'Encoding' option in the Account Editor)"));
	}
	
	return (utf8);
}

static struct gaim_conversation *
irc_find_chat(struct gaim_connection *gc, char *name)
{
	GSList *bcs = gc->buddy_chats;

	while (bcs) {
		struct gaim_conversation *b = bcs->data;
		if (!g_strcasecmp(b->name, name))
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
handle_005(struct gaim_connection *gc, char *word[], char *word_eol[])
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
		if (!g_strncasecmp(cur, "B>", 2)) {
			if (!bold) {
				bold = TRUE;
				str = g_string_append_c(str, '\2');
			}
			cur = cur + 2;
		} else if (!g_strncasecmp(cur, "I>", 2)) { /* use bold for italics too */
			if (!italics) {
				italics = TRUE;
				str = g_string_append_c(str, '\2');
			}
			cur = cur + 2;
		} else if (!g_strncasecmp(cur, "U>", 2)) {
			if (!underline) {
				underline = TRUE;
				str = g_string_append_c(str, '\37');
			}
			cur = cur + 2;
		}  else if (!g_strncasecmp(cur, "/B>", 3)) { 
			if (bold) {
				bold = FALSE;
				str = g_string_append_c(str, '\2');
			}
			cur = cur + 3;
		}  else if (!g_strncasecmp(cur, "/I>", 3)) { 
			if (italics) {
				italics = FALSE;
				str = g_string_append_c(str, '\2');
			}
			cur = cur + 3;
		}  else if (!g_strncasecmp(cur, "/U>", 3)) { 
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
irc_got_im(struct gaim_connection *gc, char *who, char *what, int flags, time_t t)
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
	struct gaim_conversation *convo;
	debug_printf("THIS IS TOO MUCH EFFORT\n");
	n = read (chat->fd, buffer, IRC_BUF_LEN);
	if (n > 0) {

		buffer[n] = 0;
		g_strstrip(buffer);

		/* Convert to HTML */
		if (strlen(buffer)) {
			debug_printf ("DCC Message from: %s\n", chat->nick);
			irc_got_im(chat->gc, chat->nick, buffer, 0, 
				   time(NULL));
		}
	}	
	else	{
		g_snprintf (buf, sizeof buf, _("DCC Chat with %s closed"),
			    chat->nick);
		convo = gaim_conversation_new(GAIM_CONV_IM, chat->nick);
		gaim_conversation_write(convo, NULL, buf, -1, WFLAG_SYSTEM,
								time(NULL));
		dcc_chat_cancel (chat);
	}
}

static void 
irc_file_transfer_do(struct gaim_connection *gc, struct irc_file_transfer *ift) {
	/* Ok, we better be receiving some crap here boyeee */
	if (transfer_in_do(ift->xfer, ift->fd, ift->name, ift->len)) {
		gaim_input_remove(ift->watcher);
		ift->watcher = 0;
	}
}


void 
irc_read_dcc_ack (gpointer data, gint source, GaimInputCondition condition) {
	/* Read ACK Here */

}

void 
dcc_send_callback (gpointer data, gint source, GaimInputCondition condition) {
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
}

void 
dcc_recv_callback (gpointer data, gint source, GaimInputCondition condition) {
	struct irc_file_transfer *ift = data;
	
	ift->fd = source;
	irc_file_transfer_do(ift->gc, ift);
}

void 
dcc_chat_callback (gpointer data, gint source, GaimInputCondition condition) {
	struct dcc_chat *chat = data;
	struct gaim_conversation *convo;
	char buf[IRC_BUF_LEN];

	convo = gaim_conversation_new(GAIM_CONV_IM, chat->nick);

	chat->fd = source;
	g_snprintf (buf, sizeof buf,
		    _("DCC Chat with %s established"),
		    chat->nick);
	gaim_conversation_write(convo, NULL, buf, -1, WFLAG_SYSTEM, time(NULL));
	debug_printf ("Chat with %s established\n", chat->nick);
	dcc_chat_list =  g_slist_append (dcc_chat_list, chat);
	gaim_input_remove(chat->inpa);
	chat->inpa = gaim_input_add(source, GAIM_INPUT_READ, dcc_chat_in, chat);
}

static void 
irc_got_chat_in(struct gaim_connection *gc, int id, char *who, int whisper, char *msg, time_t t)
{
	char *utf8 = irc_recv_convert(gc, msg);
	GString *str = decode_html(utf8);
	serv_got_chat_in(gc, id, who, whisper, str->str, t);
	g_string_free(str, TRUE);
	g_free(utf8);
}

static void 
handle_list(struct gaim_connection *gc, char *list)
{
	struct irc_data *id = gc->proto_data;
	GSList *gr;

	id->str = g_string_append_c(id->str, ' ');
	id->str = g_string_append(id->str, list);
	id->bc--;
	if (id->bc)
		return;

	g_strdown(id->str->str);
	gr = groups;
	while (gr) {
		GSList *m = ((struct group *)gr->data)->members;
		while (m) {
			struct buddy *b = m->data;
			if(b->user->gc == gc) {
				char *tmp = g_strdup(b->name);
				char *x, *l;
				g_strdown(tmp);
				x = strstr(id->str->str, tmp);
				l = x + strlen(b->name);
				if (x && (*l != ' ' && *l != 0))
					x = 0;
				if (!b->present && x)
					serv_got_update(gc, b->name, 1, 0, 0, 0, 0, 0);
				else if (b->present && !x)
					serv_got_update(gc, b->name, 0, 0, 0, 0, 0, 0);
				g_free(tmp);
			}
			m = m->next;
		}
		gr = gr->next;
	}
	g_string_free(id->str, TRUE);
	id->str = g_string_new("");
}

static gboolean 
irc_request_buddy_update(gpointer data)
{
	struct gaim_connection *gc = data;
	struct irc_data *id = gc->proto_data;
	char buf[500];
	int n = g_snprintf(buf, sizeof(buf), "ISON");

	GSList *gr = groups;
	if (!gr || id->bc)
		return TRUE;

	while (gr) {
		struct group *g = gr->data;
		GSList *m = g->members;
		while (m) {
			struct buddy *b = m->data;
			if(b->user->gc == gc) {
				if (n + strlen(b->name) + 2 > sizeof(buf)) {
					g_snprintf(buf + n, sizeof(buf) - n, "\r\n");
					irc_write(id->fd, buf, n);
					id->bc++;
					n = g_snprintf(buf, sizeof(buf), "ISON");
				}
				n += g_snprintf(buf + n, sizeof(buf) - n, " %s", b->name);
			}
			m = m->next;
		}
		gr = gr->next;
	}
	g_snprintf(buf + n, sizeof(buf) - n, "\r\n");
	irc_write(id->fd, buf, strlen(buf));
	id->bc++;
	return TRUE;
}

static void 
handle_names(struct gaim_connection *gc, char *chan, char *names)
{
	struct gaim_conversation *c = irc_find_chat(gc, chan);
	struct gaim_chat *chat;
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
handle_notopic(struct gaim_connection *gc, char *text)
{
	struct gaim_conversation *c;

	if ((c = irc_find_chat(gc, text))) {
		char buf[IRC_BUF_LEN];

		g_snprintf(buf, sizeof(buf), _("No topic is set"));

		gaim_chat_set_topic(GAIM_CHAT(c), NULL, buf);
	}
}

static void 
handle_topic(struct gaim_connection *gc, char *text)
{
	struct gaim_conversation *c;
	char *po = strchr(text, ' ');

	if (!po)
		return;

	*po = 0;
	po += 2;

	if ((c = irc_find_chat(gc, text))) {
		char buf[IRC_BUF_LEN];
		gaim_chat_set_topic(GAIM_CHAT(c), NULL, po);
		g_snprintf(buf, sizeof(buf), _("<B>%s has changed the topic to: %s</B>"),
			   text, po);

		gaim_conversation_write(c, NULL, buf, -1, WFLAG_SYSTEM, time(NULL));
	}
}

static gboolean 
mode_has_arg(struct gaim_connection *gc, char sign, char mode)
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
irc_chan_mode(struct gaim_connection *gc, char *room, char sign, char mode, char *argstr, char *who)
{
	struct gaim_conversation *c = irc_find_chat(gc, room);
	char buf[IRC_BUF_LEN];
	char *nick = g_strndup(who, strchr(who, '!') - who);

	g_snprintf(buf, sizeof(buf), _("-:- mode/%s [%c%c %s] by %s"), 
		   room, sign, mode, strlen(argstr) ? argstr : "",
		   nick);
	g_free(nick);

	gaim_conversation_write(c, NULL, buf, -1, WFLAG_SYSTEM, time(NULL));
}

static void 
irc_user_mode(struct gaim_connection *gc, char *room, char sign, char mode, char *nick)
{
	struct gaim_conversation *c = irc_find_chat(gc, room);
	GList *r;

	if (mode != 'o' && mode != 'v')
		return;

	if (!c)
		return;

	r = gaim_chat_get_users(GAIM_CHAT(c));
	while (r) {
		gboolean op = FALSE, voice = FALSE;
		char *who = r->data;
		if (*who == '@') {
			op = TRUE;
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
			if (mode == 'v') {
				if (sign == '-')
					voice = FALSE;
				else
					voice = TRUE;
			}
			tmp = g_strdup(r->data);
			g_snprintf(buf, sizeof(buf), "%s%s%s", op ? "@" : "",
				   voice ? "+" : "", nick);
			gaim_chat_rename_user(GAIM_CHAT(c), tmp, buf);
			g_free(tmp);
			return;
		}
		r = r->next;
	}
}

static void 
handle_mode(struct gaim_connection *gc, char *word[], char *word_eol[], gboolean n324)
{
	struct irc_data *id = gc->proto_data;
	int offset = n324 ? 4 : 3;
	char *chan = word[offset];
	struct gaim_conversation *c = irc_find_chat(gc, chan);
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
handle_version(struct gaim_connection *gc, char *word[], char *word_eol[], int num)
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
handle_who(struct gaim_connection *gc, char *word[], char *word_eol[], int num)
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
handle_whois(struct gaim_connection *gc, char *word[], char *word_eol[], int num)
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
		id->liststr = g_string_append(id->liststr, "<b>User: </b>");
		break;
	case 312:
		id->liststr = g_string_append(id->liststr, "<b>Server: </b>");
		break;
	case 313:
		g_snprintf(tmp, sizeof(tmp), "<b>IRC Operator:</b> %s ", word[4]);
		id->liststr = g_string_append(id->liststr, tmp);
		break;
	case 314:
		id->liststr = g_string_append(id->liststr, "<b>User: </b>");
		g_snprintf(tmp, sizeof(tmp), "<b>%s</b> (%s@%s) %s",
			   word[4], word[5], word[6], word_eol[8]);
		id->liststr = g_string_append(id->liststr, tmp);
		return;
	case 317:
		id->liststr = g_string_append(id->liststr, "<b>Idle Time: </b>");
		break;
	case 319:
		id->liststr = g_string_append(id->liststr, "<b>Channels: </b>");
		break;
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
			   "%ld seconds [signon: %s]", (idle / 1000), ctime(&signon));
		id->liststr = g_string_append(id->liststr, tmp);
	}
	else
		id->liststr = g_string_append(id->liststr, word_eol[5]);
}

static void 
handle_roomlist(struct gaim_connection *gc, char *word[], char *word_eol[])
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
irc_change_nick(void *a, char *b) {
	struct gaim_connection *gc = a;
	struct irc_data *id = gc->proto_data;
	char buf[IRC_BUF_LEN];	
	g_snprintf(buf, sizeof(buf), "NICK %s\r\n", b);
	irc_write(id->fd, buf, strlen(buf));
}

static void 
process_numeric(struct gaim_connection *gc, char *word[], char *word_eol[])
{
	struct irc_data *id = gc->proto_data;
	char *text = word_eol[3];
	int n = atoi(word[2]);

	if (!g_strncasecmp(gc->displayname, text, strlen(gc->displayname)))
		text += strlen(gc->displayname) + 1;
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
			id->liststr = g_string_append(id->liststr, "<BR><b>Away: </b>");

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
		do_error_dialog(_("Rehashing server"), _("IRC Operator"), GAIM_ERROR);
		break;
	case 401: /* ERR_NOSUCHNICK */
		do_error_dialog(_("No such nick/channel"), _("IRC Error"), GAIM_ERROR);
		break;
	case 402: /* ERR_NOSUCHSERVER */
		do_error_dialog(_("No such server"), _("IRC Error"), GAIM_ERROR);
	case 431: /* ERR_NONICKNAMEGIVEN */
		do_error_dialog(_("No nickname given"), _("IRC Error"), GAIM_ERROR);
		break;
	case 481: /* ERR_NOPRIVILEGES */
		do_error_dialog(_("You're not an IRC operator!"), _("IRC Error"), GAIM_ERROR);
		break;		
	case 433:
		do_prompt_dialog(_("That nick is already in use.  Please enter a new nick"), gc->displayname, gc, irc_change_nick, NULL);
		break;
	default:
		/* Other error messages */
		if (n > 400 && n < 502) {
			char errmsg[IRC_BUF_LEN];
			char *errmsg1 = strrchr(text, ':');
			g_snprintf(errmsg, sizeof(errmsg), "IRC Error %d", n);
			if (errmsg)
				do_error_dialog(errmsg, errmsg1 ? errmsg1+1 : NULL, GAIM_ERROR);
		}
		break;
	}
}

static gboolean 
is_channel(struct gaim_connection *gc, char *name)
{
	struct irc_data *id = gc->proto_data;
	if (strchr(id->chantypes, *name))
		return TRUE;
	return FALSE;
}

static void 
irc_rem_chat_bud(struct gaim_connection *gc, char *nick, struct gaim_conversation *b, char *reason)
{

	struct gaim_chat *chat;

	if (b) {
		GList *r;

		chat = GAIM_CHAT(b);

		r = gaim_chat_get_users(chat);

		while (r) {
			char *who = r->data;
			if (*who == '@')
				who++;
			if (*who == '+')
				who++;
			if (!g_strcasecmp(who, nick)) {
				char *tmp = g_strdup(r->data);
				gaim_chat_remove_user(chat, tmp, reason);
				g_free(tmp);
				break;
			}
			r = r->next;
		}
	} else {
		GSList *bcs = gc->buddy_chats;
		while (bcs) {
			struct gaim_conversation *bc = bcs->data;
			irc_rem_chat_bud(gc, nick, bc, reason);
			bcs = bcs->next;
		}
	}
}

static void 
irc_change_name(struct gaim_connection *gc, char *old, char *new)
{
	GSList *bcs = gc->buddy_chats;
	char buf[IRC_BUF_LEN];

	while (bcs) {
		struct gaim_conversation *b = bcs->data;
		struct gaim_chat *chat;
		GList *r;

		chat = GAIM_CHAT(b);

		r = gaim_chat_get_users(chat);

		while (r) {
			char *who = r->data;
			int n = 0;
			if (*who == '@')
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
handle_privmsg(struct gaim_connection *gc, char *to, char *nick, char *msg)
{
	if (is_channel(gc, to)) {
		struct gaim_conversation *c = irc_find_chat(gc, to);
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
	if (g_slist_find(connections, data->gc)) {
		proxy_connect(data->ip_address, data->port, dcc_chat_callback, data);
	} else {
		g_free(data);
	}
}

static void 
dcc_chat_cancel(struct dcc_chat *data){
	if (g_slist_find(connections, data->gc) && find_dcc_chat(data->gc, data->nick)) {
		dcc_chat_list = g_slist_remove(dcc_chat_list, data); 
		gaim_input_remove (data->inpa);
		close (data->fd);
	}
	g_free(data);
}

static void 
irc_convo_closed(struct gaim_connection *gc, char *who)
{
	struct dcc_chat *dchat = find_dcc_chat(gc, who);
	if (!dchat)
		return;

	dcc_chat_cancel(dchat);
}

static void 
handle_ctcp(struct gaim_connection *gc, char *to, char *nick,
			char *msg, char *word[], char *word_eol[])
{
	struct irc_data *id = gc->proto_data;
	char buf[IRC_BUF_LEN];
	char out[IRC_BUF_LEN];

	if (!g_strncasecmp(msg, "VERSION", 7)) {
		g_snprintf(buf, sizeof(buf), "\001VERSION Gaim " VERSION ": The Penguin Pimpin' "
			   "Multi-protocol Messaging Client: " WEBSITE "\001");
		irc_send_notice (gc, nick, buf);
		g_snprintf(out, sizeof(out), ">> CTCP VERSION requested from %s", nick);
		do_error_dialog(out, _("IRC CTCP info"), GAIM_INFO);
	}
	if (!g_strncasecmp(msg, "CLIENTINFO", 10)) {
		g_snprintf(buf, sizeof(buf), "\001CLIENTINFO USERINFO CLIENTINFO VERSION\001");
		irc_send_notice (gc, nick, buf);
		g_snprintf(out, sizeof(out), ">> CTCP CLIENTINFO requested from %s", nick);
		do_error_dialog(out, _("IRC CTCP info"), GAIM_INFO);
	}
	if (!g_strncasecmp(msg, "USERINFO", 8)) {
		g_snprintf(buf, sizeof(buf), "\001USERINFO Alias: %s\001", gc->user->alias);
		irc_send_notice (gc, nick, buf);
		g_snprintf(out, sizeof(out), ">> CTCP USERINFO requested from %s", nick);
		do_error_dialog(out, _("IRC CTCP info"), GAIM_INFO);
	}
	if (!g_strncasecmp(msg, "ACTION", 6)) {
		char *po = strchr(msg + 6, 1);
		char *tmp;
		if (po) *po = 0;
		tmp = g_strconcat("/me", msg + 6, NULL);
		handle_privmsg(gc, to, nick, tmp);
		g_free(tmp);
	}
	if (!g_strncasecmp(msg, "PING", 4)) {
		g_snprintf(buf, sizeof(buf), "\001%s\001", msg);	
		irc_send_notice (gc, nick, buf);
		g_snprintf(out, sizeof(out), ">> CTCP PING requested from %s", nick);
		do_error_dialog(out, _("IRC CTCP info"), GAIM_INFO);		
	}
	if (!g_strncasecmp(msg, "DCC CHAT", 8)) {
		char **chat_args = g_strsplit(msg, " ", 5);
		char ask[1024];
		struct dcc_chat *dccchat = g_new0(struct dcc_chat, 1);
		dccchat->gc = gc;	
		g_snprintf(dccchat->ip_address, sizeof(dccchat->ip_address), chat_args[3]);	
		printf("DCC CHAT DEBUG CRAP: %s\n", dccchat->ip_address);
		dccchat->port=atoi(chat_args[4]);		
		g_snprintf(dccchat->nick, sizeof(dccchat->nick), nick);	
		g_snprintf(ask, sizeof(ask), _("%s would like to establish a DCC chat"), nick);
		do_ask_dialog(ask, _("This requires a direct connection to be established between the two computers.  Messages sent will not pass through the IRC server"), dccchat, _("Connect"), dcc_chat_init, _("Cancel"), dcc_chat_cancel, my_protocol->plug ? my_protocol->plug->handle : NULL, FALSE);
	}


	if (!g_strncasecmp(msg, "DCC SEND", 8)) {
		struct irc_file_transfer *ift = g_new0(struct irc_file_transfer, 1);
		char **send_args = g_strsplit(msg, " ", 6);
		send_args[5][strlen(send_args[5])-1] = 0;

		ift->type = IFT_SENDFILE_IN;
		ift->sn = g_strdup(nick);
		ift->gc = gc;
		g_snprintf(ift->ip, sizeof(ift->ip), send_args[3]);	
		ift->port = atoi(send_args[4]);
		ift->len = atoi(send_args[5]);
		ift->name = g_strdup(send_args[2]);
		ift->cur = 0;

		id->file_transfers = g_slist_append(id->file_transfers, ift);

		ift->xfer = transfer_in_add(gc, nick, ift->name, ift->len, 1, NULL);
	}

	/*write_to_conv(c, out, WFLAG_SYSTEM, NULL, time(NULL), -1);*/
}

static gboolean 
irc_parse(struct gaim_connection *gc, char *buf)
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
				hide_login_progress(gc, _("Unable to write"));
				signoff(gc);
				r = TRUE;
			}
			return r;
		}
		/* XXX doesn't handle ERROR */
		return FALSE;
	}

	if (!idata->online) {
		/* Now lets sign ourselves on */
		account_online(gc);
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
		char *chan = g_strdup(word[4]);
		serv_got_chat_invite(gc, chan + 1, nick, NULL, g_list_append(NULL, chan));
	} else if (!strcmp(cmd, "JOIN")) {
		irc_parse_join(gc, nick, word, word_eol);
	} else if (!strcmp(cmd, "KICK")) {
		if (!strcmp(gc->displayname, word[4])) {
			struct gaim_conversation *c = irc_find_chat(gc, word[3]);
			if (!c)
				return FALSE;
			gc->buddy_chats = g_slist_remove(gc->buddy_chats, c);
			gaim_conversation_set_user(c, NULL);
			g_snprintf(outbuf, sizeof(outbuf), _("You have been kicked from %s: %s"),
				   word[3], *word_eol[5] == ':' ? word_eol[5] + 1 : word_eol[5]);
			do_error_dialog(outbuf, _("IRC Error"), GAIM_ERROR);
		} else {
			char *reason = *word_eol[5] == ':' ? word_eol[5] + 1 : word_eol[5];
			char *msg = g_strdup_printf(_("Kicked by %s: %s"), nick, reason);
			struct gaim_conversation *c = irc_find_chat(gc, word[3]);
			irc_rem_chat_bud(gc, word[4], c, msg);
			g_free(msg);
		}
	} else if (!strcmp(cmd, "KILL")) { /* */
	} else if (!strcmp(cmd, "MODE")) {
		handle_mode(gc, word, word_eol, FALSE);
	} else if (!strcmp(cmd, "NICK")) {
		char *new = *word_eol[3] == ':' ? word_eol[3] + 1 : word_eol[3];
		if (!strcmp(gc->displayname, nick))
			g_snprintf(gc->displayname, sizeof(gc->displayname), "%s", new);
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
			if (!g_strncasecmp(msg + 1, "DCC ", 4))
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
			do_error_dialog(msg+1, _("IRC Operator"), GAIM_ERROR);
	}

	return FALSE;
}

/* CTCP by jonas@birme.se */
static void
irc_parse_notice(struct gaim_connection *gc, char *nick, char *ex,
                 char *word[], char *word_eol[])
{
	char buf[IRC_BUF_LEN];

	if (!g_strcasecmp(word[4], ":\001CLIENTINFO")) {
		char *p = g_strrstr(word_eol[5], "\001");
		*p = 0;
		g_snprintf(buf, sizeof(buf), "CTCP Answer: %s", word_eol[5]);
		do_error_dialog(buf, _("CTCP ClientInfo"), GAIM_INFO);

	} else if (!g_strcasecmp(word[4], ":\001USERINFO")) {
		char *p = g_strrstr(word_eol[5], "\001");
		*p = 0;
		g_snprintf(buf, sizeof(buf), "CTCP Answer: %s", word_eol[5]);
		do_error_dialog(buf, _("CTCP UserInfo"), GAIM_INFO);

	} else if (!g_strcasecmp(word[4], ":\001VERSION")) {
		char *p = g_strrstr(word_eol[5], "\001");
		*p = 0;
		g_snprintf(buf, sizeof(buf), "CTCP Answer: %s", word_eol[5]);
		do_error_dialog(buf, _("CTCP Version"), GAIM_INFO);

	} else if (!g_strcasecmp(word[4], ":\001PING")) {
		char *p = g_strrstr(word_eol[5], "\001");
		struct timeval ping_time;
		struct timeval now;
		gchar **vector;

		if (p)
			*p = 0;

		vector = g_strsplit(word_eol[5], " ", 2);

		if (gettimeofday(&now, NULL) == 0 && vector != NULL) {
			if (now.tv_usec - atol(vector[1]) < 0) {
				ping_time.tv_sec = now.tv_sec - atol(vector[0]) - 1;
				ping_time.tv_usec = now.tv_usec - atol(vector[1]) + 1000000;

			} else {
				ping_time.tv_sec = now.tv_sec - atol(vector[0]);
				ping_time.tv_usec = now.tv_usec - atol(vector[1]);
			}

			g_snprintf(buf, sizeof(buf),
					   "CTCP Ping reply from %s: %lu.%.03lu seconds",
					   nick, ping_time.tv_sec, (ping_time.tv_usec/1000));

			do_error_dialog(buf, _("CTCP Ping"), GAIM_INFO);
			g_strfreev(vector);
		}
	} else {
		if (*word_eol[4] == ':') word_eol[4]++;
		if (ex)
			irc_got_im(gc, nick, word_eol[4], 0, time(NULL));
	}
}

static void
irc_parse_join(struct gaim_connection *gc, char *nick,
               char *word[], char *word_eol[])
{
	char *chan = *word[3] == ':' ? word[3] + 1 : word[3];
	static int id = 1;
	struct gaim_conversation *c;
	char *hostmask, *p;

	if (!g_strcasecmp(gc->displayname, nick)) {
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
irc_parse_topic(struct gaim_connection *gc, char *nick,
                char *word[], char *word_eol[])
{
	struct gaim_conversation *c = irc_find_chat(gc, word[3]);
	char *topic = *word_eol[4] == ':' ? word_eol[4] + 1 : word_eol[4];
	char buf[IRC_BUF_LEN];

	if (c) {
		gaim_chat_set_topic(GAIM_CHAT(c), nick, topic);
		g_snprintf(buf, sizeof(buf),
				   _("<B>%s has changed the topic to: %s</B>"), nick, topic);

		gaim_conversation_write(c, NULL, buf, -1, WFLAG_SYSTEM, time(NULL));
	}
}

static gboolean
irc_parse_part(struct gaim_connection *gc, char *nick, char *cmd,
               char *word[], char *word_eol[])
{
	char *chan = cmd + 5;
	struct gaim_conversation *c;
	struct gaim_chat *chat;
	char *reason = word_eol[4];
	GList *r;

	if (*chan == ':')
		chan++;
	if (*reason == ':')
		reason++;
	if (!(c = irc_find_chat(gc, chan)))
		return FALSE;
	if (!strcmp(nick, gc->displayname)) {
		serv_got_chat_left(gc, gaim_chat_get_id(GAIM_CHAT(c)));
		return FALSE;
	}

	chat = GAIM_CHAT(c);

	r = gaim_chat_get_users(GAIM_CHAT(c));

	while (r) {
		char *who = r->data;
		if (*who == '@')
			who++;
		if (*who == '+')
			who++;
		if (!g_strcasecmp(who, nick)) {
			char *tmp = g_strdup(r->data);
			gaim_chat_remove_user(chat, tmp, reason);
			g_free(tmp);
			break;
		}
		r = r->next;
	}
	return TRUE;
}

static void 
irc_callback(gpointer data, gint source, GaimInputCondition condition)
{
	struct gaim_connection *gc = data;
	struct irc_data *idata = gc->proto_data;
	int i = 0;
	gchar buf[1024];
	gboolean off;

	i = read(idata->fd, buf, 1024);
	if (i <= 0) {
		hide_login_progress_error(gc, "Read error");
		signoff(gc);
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
		debug_printf("IRC S: %s\n", d);

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
	struct gaim_connection *gc = data;
	struct irc_data *idata;
	char hostname[256];
	char buf[IRC_BUF_LEN];
	char *test;
	GError *err = NULL;
	
	if (!g_slist_find(connections, gc)) {
		close(source);
		return;
	}

	idata = gc->proto_data;

	if (source == -1) {
		hide_login_progress(gc, "Write error");
		signoff(gc);
		return;
	}

	
	/* Try a quick conversion to see if the specified encoding is OK */
	test = g_convert("test", strlen("test"), gc->user->proto_opt[USEROPT_CHARSET],
			 "UTF-8", NULL, NULL, &err);
	if (err) {
		debug_printf("Couldn't initialize %s for IRC charset conversion, using ISO-8859-1\n",
			     gc->user->proto_opt[USEROPT_CHARSET]);
		strcpy(gc->user->proto_opt[USEROPT_CHARSET], "ISO-8859-1");
	}
	
	g_free(test);
	
	if (idata->fd != source)
		idata->fd = source;

	gethostname(hostname, sizeof(hostname) - 1);
	hostname[sizeof(hostname) - 1] = 0;
	if (!*hostname)
		g_snprintf(hostname, sizeof(hostname), "localhost");

	if (*gc->user->password) {
		g_snprintf(buf, sizeof(buf), "PASS %s\r\n", gc->user->password);

		if (irc_write(idata->fd, buf, strlen(buf)) < 0) {
			hide_login_progress(gc, "Write error");
			signoff(gc);
			return;
		}
	}

	g_snprintf(buf, sizeof(buf), "USER %s %s %s :%s\r\n",
		   g_get_user_name(), hostname, 
		   gc->user->proto_opt[USEROPT_SERV], 
		   gc->user->alias && strlen(gc->user->alias) ? gc->user->alias : "gaim");
	if (irc_write(idata->fd, buf, strlen(buf)) < 0) {
		hide_login_progress(gc, "Write error");
		signoff(gc);
		return;
	}

	g_snprintf(buf, sizeof(buf), "NICK %s\r\n", gc->username);
	if (irc_write(idata->fd, buf, strlen(buf)) < 0) {
		hide_login_progress(gc, "Write error");
		signoff(gc);
		return;
	}

	gc->inpa = gaim_input_add(idata->fd, GAIM_INPUT_READ, irc_callback, gc);
}

static void 
irc_login(struct aim_user *user)
{
	char buf[IRC_BUF_LEN];

	struct gaim_connection *gc = new_gaim_conn(user);
	struct irc_data *idata = gc->proto_data = g_new0(struct irc_data, 1);

	g_snprintf(gc->displayname, sizeof(gc->displayname), "%s", gc->username);

	g_snprintf(buf, sizeof(buf), "Signon: %s", gc->username);
	set_login_progress(gc, 2, buf);

	idata->chantypes = g_strdup("#&!+");
	idata->chanmodes = g_strdup("beI,k,lnt");
	idata->nickmodes = g_strdup("ohv");
	idata->str = g_string_new("");

	idata->fd = proxy_connect(user->proto_opt[USEROPT_SERV],
				  user->proto_opt[USEROPT_PORT][0] ? atoi(user->
									  proto_opt[USEROPT_PORT]) :
				  6667, irc_login_callback, gc);
	if (!user->gc || (idata->fd < 0)) {
		hide_login_progress(gc, "Unable to create socket");
		signoff(gc);
		return;
	}
}

static void 
irc_close(struct gaim_connection *gc)
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
		struct irc_file_transfer *ift = (struct irc_file_transfer *)idata->file_transfers->data;

		g_free(ift->sn);
		g_free(ift->name);
		gaim_input_remove(ift->watcher);

		close(ift->fd);

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
set_mode_3(struct gaim_connection *gc, char *who, int sign, int mode,
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
set_mode_6(struct gaim_connection *gc, char *who, int sign, int mode,
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
set_mode(struct gaim_connection *gc, char *who, int sign, int mode, char *word[])
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
set_chan_mode(struct gaim_connection *gc, char *chan, char *mode_str)
{
	struct irc_data *id = gc->proto_data;
	char buf[IRC_BUF_LEN];

	if ((mode_str[0] == '-') || (mode_str[0] == '+')) {
		g_snprintf(buf, sizeof(buf), "MODE %s %s\r\n", chan, mode_str);
		irc_write(id->fd, buf, strlen(buf));
	}
}

static int 
handle_command(struct gaim_connection *gc, char *who, char *what)
{
	char buf[IRC_BUF_LEN];
	char pdibuf[IRC_BUF_LEN];
	char *word[PDIWORDS], *word_eol[PDIWORDS];
	char *tmp = g_strdup(what);
	GString *str = encode_html(tmp);
	char *intl;
	int len;
	struct dcc_chat *dccchat = find_dcc_chat(gc, who);
	struct irc_data *id = gc->proto_data;

	g_free(tmp);
	what = str->str;

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
	if (!g_strcasecmp(pdibuf, "ME")) {
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
	} else if (!g_strcasecmp(pdibuf, "INVITE")) {
		char buf[IRC_BUF_LEN];
		g_snprintf(buf, sizeof(buf), "INVITE %s\r\n", word_eol[2]);
		irc_write(id->fd, buf, strlen(buf));
	} else if (!g_strcasecmp(pdibuf, "TOPIC")) {
		if (!*word_eol[2]) {
			struct gaim_conversation *c;
			struct gaim_chat *chat;

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
	} else if (!g_strcasecmp(pdibuf, "NICK")) {
		if (!*word_eol[2]) {
			g_free(what);
			return -EINVAL;
		}
		g_snprintf(buf, sizeof(buf), "NICK %s\r\n", word_eol[2]);
		irc_write(id->fd, buf, strlen(buf));
	} else if (!g_strcasecmp(pdibuf, "OP")) {
		set_mode(gc, who, '+', 'o', word);
	} else if (!g_strcasecmp(pdibuf, "DEOP")) {
		set_mode(gc, who, '-', 'o', word);
	} else if (!g_strcasecmp(pdibuf, "VOICE")) {
		set_mode(gc, who, '+', 'v', word);
	} else if (!g_strcasecmp(pdibuf, "DEVOICE")) {
		set_mode(gc, who, '-', 'v', word);
	} else if (!g_strcasecmp(pdibuf, "MODE")) {
		char *chan = who;
		set_chan_mode(gc, chan, word_eol[2]);
	} else if (!g_strcasecmp(pdibuf, "QUOTE")) {
		if (!*word_eol[2]) {
			g_free(what);
			return -EINVAL;
		}
		g_snprintf(buf, sizeof(buf), "%s\r\n", word_eol[2]);
		irc_write(id->fd, buf, strlen(buf));
	} else if (!g_strcasecmp(pdibuf, "SAY")) {
		if (!*word_eol[2]) {
			g_free(what);
			return -EINVAL;
		}
		irc_send_privmsg (gc, who, word_eol[2], TRUE);
		return 1;
	} else if (!g_strcasecmp(pdibuf, "MSG")) {
		if (!*word[2]) {
			g_free(what);
			return -EINVAL;
		}
		if (!*word_eol[3]) {
			g_free(what);
			return -EINVAL;
		}
		irc_send_privmsg (gc, word[2], word_eol[3], TRUE);
	} else if (!g_strcasecmp(pdibuf, "KICK")) {
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
	} else if (!g_strcasecmp(pdibuf, "JOIN") || !g_strcasecmp(pdibuf, "J")) {
		if (!*word[2]) {
			g_free(what);
			return -EINVAL;
		}
		if (*word[3])
			g_snprintf(buf, sizeof(buf), "JOIN %s %s\r\n", word[2], word[3]);
		else
			g_snprintf(buf, sizeof(buf), "JOIN %s\r\n", word[2]);
		irc_write(id->fd, buf, strlen(buf));
	} else if (!g_strcasecmp(pdibuf, "PART")) {
		char *chan = *word[2] ? word[2] : who;
		char *reason = word_eol[3];
		struct gaim_conversation *c;
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
			gaim_conversation_set_user(c, NULL);
			g_snprintf(buf, sizeof(buf), _("You have left %s"), chan);
			do_error_dialog(buf, _("IRC Part"), GAIM_INFO);
		}
	} else if (!g_strcasecmp(pdibuf, "WHOIS")) {
		g_snprintf(buf, sizeof(buf), "WHOIS %s\r\n", word_eol[2]);
		irc_write(id->fd, buf, strlen(buf));
	} else if (!g_strcasecmp(pdibuf, "WHOWAS")) {
		g_snprintf(buf, sizeof(buf), "WHOWAS %s\r\n", word_eol[2]);
		irc_write(id->fd, buf, strlen(buf));
	} else if (!g_strcasecmp(pdibuf, "LIST")) {
		g_snprintf(buf, sizeof(buf), "LIST\r\n");
		irc_write(id->fd, buf, strlen(buf));
	} else if (!g_strcasecmp(pdibuf, "QUIT")) {
		char *reason = word_eol[2];
		id->str = g_string_insert(id->str, 0, reason);
		do_quit();
	} else if (!g_strcasecmp(pdibuf, "VERSION")) {
		g_snprintf(buf, sizeof(buf), "VERSION\r\n");
		irc_write(id->fd, buf, strlen(buf));
	} else if (!g_strcasecmp(pdibuf, "W")) {
		g_snprintf(buf, sizeof(buf), "WHO *\r\n");
		irc_write(id->fd, buf, strlen(buf));
	} else if (!g_strcasecmp(pdibuf, "REHASH")) {
		g_snprintf(buf, sizeof(buf), "REHASH\r\n");
		irc_write(id->fd, buf, strlen(buf));		
	} else if (!g_strcasecmp(pdibuf, "RESTART")) {
		g_snprintf(buf, sizeof(buf), "RESTART\r\n");
		irc_write(id->fd, buf, strlen(buf));
	} else if (!g_strcasecmp(pdibuf, "CTCP")) {
		if (!g_strcasecmp(word[2], "CLIENTINFO")) {
			if (word[3])
				irc_ctcp_clientinfo(gc, word[3]);
		} else if (!g_strcasecmp(word[2], "USERINFO")) {
			if (word[3])
				irc_ctcp_userinfo(gc, word[3]);
		} else if (!g_strcasecmp(word[2], "VERSION")) {
			if (word[3])
				irc_ctcp_version(gc, word[3]);
		
		} else if (!g_strcasecmp(word[2], "PING")) {		
			if (word[3])
				irc_ctcp_ping(gc, word[3]);
		}
	} else if (!g_strcasecmp(pdibuf, "DCC")) {
		struct gaim_conversation *c = NULL;
		if (!g_strcasecmp(word[2], "CHAT")) {
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
	} else if (!g_strcasecmp(pdibuf, "HELP")) {
		struct gaim_conversation *c = NULL;
		if (is_channel(gc, who)) {
			c = irc_find_chat(gc, who);
		} else {
			c = gaim_find_conversation(who);
		}
		if (!c) {
			g_free(what);
			return -EINVAL;
		}
		if (!g_strcasecmp(word[2], "OPER")) {
			gaim_conversation_write(c, NULL,
				_("<B>Operator commands:<BR>"
				  "REHASH RESTART</B>"),
				-1, WFLAG_NOLOG, time(NULL));
		} else if (!g_strcasecmp(word[2], "CTCP")) {
			gaim_conversation_write(c, NULL,
			_("<B>CTCP commands:<BR>"
			  "CLIENTINFO <nick><BR>"
			  "USERINFO <nick><BR>"
			  "VERSION <nick><BR>"
			  "PING <nick></B><BR>"),
			-1, WFLAG_NOLOG, time(NULL));
		} else if (!g_strcasecmp(word[2], "DCC")) {
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
		struct gaim_conversation *c = NULL;
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
send_msg(struct gaim_connection *gc, char *who, char *what)
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
irc_chat_invite(struct gaim_connection *gc, int idn, const char *message, const char *name) {
	char buf[IRC_BUF_LEN]; 
	struct irc_data *id = gc->proto_data;
	struct gaim_conversation *c = gaim_find_chat(gc, idn);
	g_snprintf(buf, sizeof(buf), "INVITE %s %s\r\n", name, c->name);
	irc_write(id->fd, buf, strlen(buf));
}

static int 
irc_send_im(struct gaim_connection *gc, char *who, char *what, int len, int flags)
{
	if (*who == '@' || *who == '+')
		return send_msg(gc, who + 1, what);
	return send_msg(gc, who, what);
}

/* IRC doesn't have a buddy list, but we can still figure out who's online with ISON */
static void 
irc_add_buddy(struct gaim_connection *gc, const char *who) {}
static void 
irc_remove_buddy(struct gaim_connection *gc, char *who, char *group) {}

static GList *
irc_chat_info(struct gaim_connection *gc)
{
	GList *m = NULL;
	struct proto_chat_entry *pce;

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("Channel:");
	m = g_list_append(m, pce);

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("Password:");
	m = g_list_append(m, pce);

	return m;
}

static void 
irc_join_chat(struct gaim_connection *gc, GList *data)
{
	struct irc_data *id = gc->proto_data;
	char buf[IRC_BUF_LEN];
	char *name, *pass;

	if (!data)
		return;
	name = data->data;
	if (data->next) {
		pass = data->next->data;
		g_snprintf(buf, sizeof(buf), "JOIN %s %s\r\n", name, pass);
	} else
		g_snprintf(buf, sizeof(buf), "JOIN %s\r\n", name);
	irc_write(id->fd, buf, strlen(buf));
}

static void 
irc_chat_leave(struct gaim_connection *gc, int id)
{
	struct irc_data *idata = gc->proto_data;
	struct gaim_conversation *c = gaim_find_chat(gc, id);
	char buf[IRC_BUF_LEN];

	if (!c) return;

	g_snprintf(buf, sizeof(buf), "PART %s\r\n", c->name);
	irc_write(idata->fd, buf, strlen(buf));
}

static int 
irc_chat_send(struct gaim_connection *gc, int id, char *what)
{
	struct gaim_conversation *c = gaim_find_chat(gc, id);
	if (!c)
		return -EINVAL;
	if (send_msg(gc, c->name, what) > 0)
		serv_got_chat_in(gc, gaim_chat_get_id(GAIM_CHAT(c)),
						 gc->displayname, 0, what, time(NULL));
	return 0;
}

static GList *
irc_away_states(struct gaim_connection *gc)
{
	return g_list_append(NULL, GAIM_AWAY_CUSTOM);
}

static void 
irc_set_away(struct gaim_connection *gc, char *state, char *msg)
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

static char **
irc_list_icon(int uc)
{
	return irc_icon_xpm;
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
	struct gaim_conversation *convo;
	char buf[128];
	struct sockaddr_in addr;
	int addrlen = sizeof (addr);
	addr.sin_family = AF_INET;
	addr.sin_port = htons (chat->port);
	addr.sin_addr.s_addr = INADDR_ANY;
	chat->fd = accept (chat->fd, (struct sockaddr *) (&addr), &addrlen);
	if (!chat->fd) {
		dcc_chat_cancel (chat);
		convo = gaim_conversation_new(GAIM_CONV_IM, chat->nick);
		g_snprintf (buf, sizeof buf, _("DCC Chat with %s closed"),
			    chat->nick);
		gaim_conversation_write(convo, NULL, buf, -1,
								WFLAG_SYSTEM, time(NULL));
		return;
	}
	chat->inpa =
		gaim_input_add (chat->fd, GAIM_INPUT_READ, dcc_chat_in, chat);
	convo = gaim_conversation_new(GAIM_CONV_IM, chat->nick);
	g_snprintf (buf, sizeof buf, _("DCC Chat with %s established"),
				chat->nick);
	gaim_conversation_write(convo, NULL, buf, -1, WFLAG_SYSTEM, time(NULL));
	debug_printf ("Chat with %s established\n", chat->nick);
	dcc_chat_list = g_slist_append (dcc_chat_list, chat);
}
#if 0
static void 
irc_ask_send_file(struct gaim_connection *gc, char *destsn) {
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
#endif
static struct 
irc_file_transfer *find_ift_by_xfer(struct gaim_connection *gc, 
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
irc_file_transfer_data_chunk(struct gaim_connection *gc, struct file_transfer *xfer, const char *data, int len) {
	struct irc_file_transfer *ift = find_ift_by_xfer(gc, xfer);
	guint32 pos;
	
	ift->cur += len;
	pos = htonl(ift->cur);
	write(ift->fd, (char *)&pos, 4);

	// FIXME: You should check to verify that they received the data when
	// you are sending a file ...
}

static void 
irc_file_transfer_cancel (struct gaim_connection *gc, struct file_transfer *xfer) {
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
irc_file_transfer_done(struct gaim_connection *gc, struct file_transfer *xfer) {
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
irc_file_transfer_out (struct gaim_connection *gc, struct file_transfer *xfer, const char *name, int totfiles, int totsize) {
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
irc_file_transfer_in(struct gaim_connection *gc,
				 struct file_transfer *xfer, int offset) {

	struct irc_file_transfer *ift = find_ift_by_xfer(gc, xfer);

	ift->xfer = xfer;
	proxy_connect(ift->ip, ift->port, dcc_recv_callback, ift);
}

static void 
irc_ctcp_clientinfo(struct gaim_connection *gc, char *who)
{
	char buf[IRC_BUF_LEN];

	snprintf (buf, sizeof buf, "\001CLIENTINFO\001");
	irc_send_privmsg(gc, who, buf, FALSE);
}

static void 
irc_ctcp_userinfo(struct gaim_connection *gc, char *who)
{
	char buf[IRC_BUF_LEN];

	snprintf (buf, sizeof buf, "\001USERINFO\001");
	irc_send_privmsg(gc, who, buf, FALSE);
}

static void 
irc_ctcp_version(struct gaim_connection *gc, char *who)
{
	char buf[IRC_BUF_LEN];

	snprintf (buf, sizeof buf, "\001VERSION\001");
	irc_send_privmsg(gc, who, buf, FALSE);
}

static void 
irc_ctcp_ping(struct gaim_connection *gc, char *who)
{
	char buf[IRC_BUF_LEN];

	g_snprintf (buf, sizeof(buf), "\001PING %ld\001", time(NULL));
	irc_send_privmsg(gc, who, buf, FALSE);
}

static void 
irc_send_notice(struct gaim_connection *gc, char *who, char *what)
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
irc_send_privmsg(struct gaim_connection *gc, char *who, char *what, gboolean fragment)
{
	char buf[IRC_BUF_LEN], *intl;
	struct irc_data *id = gc->proto_data;
	/* 512 - 12 (for PRIVMSG" "" :""\r\n") - namelen - nicklen - 68 */
	int nicklen = (gc->user->alias && strlen(gc->user->alias)) ? strlen(gc->user->alias) : 4;
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
irc_start_chat(struct gaim_connection *gc, char *who) {
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
irc_get_info(struct gaim_connection *gc, char *who)
{
	struct irc_data *idata = gc->proto_data;
	char buf[IRC_BUF_LEN];

	if (*who == '@')
		who++;
	if (*who == '+')
		who++;

	g_snprintf(buf, sizeof(buf), "WHOIS %s\r\n", who);
	irc_write(idata->fd, buf, strlen(buf));
}

static GList *
irc_buddy_menu(struct gaim_connection *gc, char *who)
{
	GList *m = NULL;
	struct proto_buddy_menu *pbm;

	pbm = g_new0(struct proto_buddy_menu, 1);
	pbm->label = _("Get Info");
	pbm->callback = irc_get_info;
	pbm->gc = gc;
	m = g_list_append(m, pbm);

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

G_MODULE_EXPORT void 
irc_init(struct prpl *ret)
{
	struct proto_user_opt *puo;
	ret->protocol = PROTO_IRC;
	ret->options = OPT_PROTO_CHAT_TOPIC | OPT_PROTO_PASSWORD_OPTIONAL;
	ret->name = g_strdup("IRC");
	ret->list_icon = irc_list_icon;
	ret->login = irc_login;
	ret->close = irc_close;
	ret->send_im = irc_send_im;
	ret->add_buddy = irc_add_buddy;
	ret->remove_buddy = irc_remove_buddy;
	ret->chat_info = irc_chat_info;
	ret->join_chat = irc_join_chat;
	ret->chat_leave = irc_chat_leave;
	ret->chat_send = irc_chat_send;
	ret->away_states = irc_away_states;
	ret->set_away = irc_set_away;
	ret->get_info = irc_get_info;
	ret->buddy_menu = irc_buddy_menu;
	ret->chat_invite = irc_chat_invite;
	ret->convo_closed = irc_convo_closed;
	ret->file_transfer_out = irc_file_transfer_out; 
	ret->file_transfer_in = irc_file_transfer_in;
	ret->file_transfer_data_chunk = irc_file_transfer_data_chunk;
	ret->file_transfer_done = irc_file_transfer_done;
	ret->file_transfer_cancel =irc_file_transfer_cancel;

	puo = g_new0(struct proto_user_opt, 1);
	puo->label = g_strdup(_("Server:"));
	puo->def = g_strdup("irc.freenode.net");
	puo->pos = USEROPT_SERV;
	ret->user_opts = g_list_append(ret->user_opts, puo);

	puo = g_new0(struct proto_user_opt, 1);
	puo->label = g_strdup(_("Port:"));
	puo->def = g_strdup("6667");
	puo->pos = USEROPT_PORT;
	ret->user_opts = g_list_append(ret->user_opts, puo);

	puo = g_new0(struct proto_user_opt, 1);
	puo->label = g_strdup(_("Encoding:"));
	puo->def = g_strdup("ISO-8859-1");
	puo->pos = USEROPT_CHARSET;
	ret->user_opts = g_list_append(ret->user_opts, puo);
	
	my_protocol = ret;
}

#ifndef STATIC
G_MODULE_EXPORT void 
gaim_prpl_init(struct prpl* prpl)
{
	irc_init(prpl);
	prpl->plug->desc.api_version = PLUGIN_API_VERSION;
}

#endif
