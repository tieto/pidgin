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

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <ctype.h>
#include "multi.h"
#include "prpl.h"
#include "gaim.h"
#include "proxy.h"

#include "pixmaps/irc_icon.xpm"

#define IRC_BUF_LEN 4096
#define PDIWORDS 32

#define USEROPT_SERV      0
#define USEROPT_PORT      1

struct dcc_chat
{
	struct gaim_connection *gc;	
	char ip_address[12];	
	int port;		
	int fd;			
	int inpa;		
	char nick[80];		
};

GSList *dcc_chat_list = NULL;

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
};

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

static char *irc_name()
{
	return "IRC";
}

static int irc_write(int fd, char *data, int len)
{
	debug_printf("IRC C: %s", data);
	return write(fd, data, len);
}

static struct conversation *irc_find_chat(struct gaim_connection *gc, char *name)
{
	GSList *bcs = gc->buddy_chats;

	while (bcs) {
		struct conversation *b = bcs->data;
		if (!g_strcasecmp(b->name, name))
			return b;
		bcs = bcs->next;
	}
	return NULL;
}

static struct conversation *irc_find_chat_by_id(struct gaim_connection *gc, int id)
{
	GSList *bcs = gc->buddy_chats;

	while (bcs) {
		struct conversation *b = bcs->data;
		if (b->id == id)
			return b;
		bcs = bcs->next;
	}
	return NULL;
}

static void process_data_init(char *buf, char *cmd, char *word[], char *eol[], gboolean quotes)
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

static void handle_005(struct gaim_connection *gc, char *word[], char *word_eol[])
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

static char *int_to_col(int c)
{
	switch(c) {
		case 1:
			return "#ffffff";
		case 2:
			return "#000066";
		case 3:
			return "#006600";
		case 4:
			return "#ff0000";
		case 5:
			return "#660000";
		case 6:
			return "#660066";
		case 7:
			return "#666600";
		case 8:
			return "#cccc00";
		case 9:
			return "#33cc33";
		case 10:
			return "#00acac";
		case 11:
			return "#00ccac";
		case 12:
			return "#0000ff";
		case 13:
			return "#cc00cc";
		case 14:
			return "#666666";
		case 15:
			return "#00ccac";
		default:
			return "#000000";
	}
}

static GString *encode_html(char *msg)
{
	GString *str = g_string_new("");
	char *cur = msg, *end = msg;
	gboolean bold = FALSE, underline = FALSE, italics = FALSE;
	
	while ((end = strchr(cur, '<'))) {
		*end = 0;
		str = g_string_append(str, cur);
		cur = ++end;
		if (!g_strncasecmp(cur, "B>", 2)) {
			bold = TRUE;
			str = g_string_append_c(str, '\2');
			cur = cur + 2;
		} else if (!g_strncasecmp(cur, "I>", 2)) { /* use bold for italics too */
			italics = TRUE;
			str = g_string_append_c(str, '\2');
			cur = cur + 2;
		} else if (!g_strncasecmp(cur, "U>", 2)) {
			underline = TRUE;
			str = g_string_append_c(str, '\37');
			cur = cur + 2;
		}  else if (!g_strncasecmp(cur, "/B>", 3) && bold) { 
			bold = FALSE;
			str = g_string_append_c(str, '\2');
			cur = cur + 3;
		}  else if (!g_strncasecmp(cur, "/I>", 3) && italics) { 
			bold = FALSE;
			str = g_string_append_c(str, '\2');
			cur = cur + 3;
		}  else if (!g_strncasecmp(cur, "/U>", 3) && underline) { 
			bold = FALSE;
			str = g_string_append_c(str, '\37');
			cur = cur + 3;
		}
	}
	str = g_string_append(str, cur);
	return str;
}

static GString *decode_html(char *msg)
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

static void irc_got_im(struct gaim_connection *gc, char *who, char *what, int flags, time_t t)
{
	GString *str = decode_html(what);
	serv_got_im(gc, who, str->str, flags, t, -1);
	g_string_free(str, TRUE);
}

static void dcc_chat_cancel(void *, struct dcc_chat *);

void
dcc_chat_in (gpointer data, gint source, GaimInputCondition condition)
{
	struct dcc_chat *chat = data;
	gchar buffer[IRC_BUF_LEN];
	gchar buf[128];
	int n = 0, l;
	struct conversation *convo;
	debug_printf("THIS IS TOO MUCH EFFORT\n");
	n = read (chat->fd, buffer, IRC_BUF_LEN);
	if (n > 0)
	  {
		  
		  /* Strip the terminating \n */
		  l = 0;
		  while (buffer[l] && buffer[l] != '\n' && l <= n) 
			  l++;
		  buffer[l] = '\000';
		  
		  /* Convert to HTML */
		  debug_printf ("DCC Message from: %s\n", chat->nick);
		  irc_got_im(chat->gc, chat->nick, buffer, 0, time(NULL));
	  }
	
	else	{
		g_snprintf (buf, sizeof buf, _("DCC Chat with %s closed"),
			    chat->nick);
		convo = new_conversation (chat->nick);
		write_to_conv (convo, buf, WFLAG_SYSTEM, NULL,
			       time ((time_t) NULL), -1);
		dcc_chat_cancel (NULL,chat);
	}
}

void dcc_chat_callback (gpointer data, gint source, GaimInputCondition condition) {
	struct dcc_chat *chat = data;
	struct conversation *convo = new_conversation (chat->nick);
	char buf[IRC_BUF_LEN];
	chat->fd = source;
	g_snprintf (buf, sizeof buf,
		    "DCC Chat with %s established",
		    chat->nick);
	write_to_conv (convo, buf, WFLAG_SYSTEM, NULL,
		       time ((time_t) NULL), -1);
	debug_printf ("Chat with %s established\n", chat->nick);
	dcc_chat_list =  g_slist_append (dcc_chat_list, chat);
	gaim_input_remove(chat->inpa);
	chat->inpa = gaim_input_add(source, GAIM_INPUT_READ, dcc_chat_in, chat);
}

static void irc_got_chat_in(struct gaim_connection *gc, int id, char *who, int whisper, char *msg, time_t t)
{
	GString *str = decode_html(msg);
	serv_got_chat_in(gc, id, who, whisper, str->str, t);
	g_string_free(str, TRUE);
}

static void handle_list(struct gaim_connection *gc, char *list)
{
	struct irc_data *id = gc->proto_data;
	GSList *gr;

	id->str = g_string_append_c(id->str, ' ');
	id->str = g_string_append(id->str, list);
	id->bc--;
	if (id->bc)
		return;

	g_strdown(id->str->str);
	gr = gc->groups;
	while (gr) {
		GSList *m = ((struct group *)gr->data)->members;
		while (m) {
			struct buddy *b = m->data;
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
			m = m->next;
		}
		gr = gr->next;
	}
	g_string_free(id->str, TRUE);
	id->str = g_string_new("");
}

static gboolean irc_request_buddy_update(gpointer data)
{
	struct gaim_connection *gc = data;
	struct irc_data *id = gc->proto_data;
	char buf[500];
	int n = g_snprintf(buf, sizeof(buf), "ISON");

	GSList *gr = gc->groups;
	if (!gr || id->bc)
		return TRUE;

	while (gr) {
		struct group *g = gr->data;
		GSList *m = g->members;
		while (m) {
			struct buddy *b = m->data;
			if (n + strlen(b->name) + 2 > sizeof(buf)) {
				g_snprintf(buf + n, sizeof(buf) - n, "\r\n");
				irc_write(id->fd, buf, n);
				id->bc++;
				n = g_snprintf(buf, sizeof(buf), "ISON");
			}
			n += g_snprintf(buf + n, sizeof(buf) - n, " %s", b->name);
			m = m->next;
		}
		gr = gr->next;
	}
	g_snprintf(buf + n, sizeof(buf) - n, "\r\n");
	irc_write(id->fd, buf, strlen(buf));
	id->bc++;
	return TRUE;
}

static void handle_names(struct gaim_connection *gc, char *chan, char *names)
{
	struct conversation *c = irc_find_chat(gc, chan);
	char **buf, **tmp;
	if (!c) return;
	if (*names == ':') names++;
	buf = g_strsplit(names, " ", -1);
	for (tmp = buf; *tmp; tmp++)
		add_chat_buddy(c, *tmp);
	g_strfreev(buf);
}

static void handle_topic(struct gaim_connection *gc, char *text)
{
	struct conversation *c;
	char *po = strchr(text, ' ');

	if (!po)
		return;

	*po = 0;
	po += 2;

	if ((c = irc_find_chat(gc, text))) {
		char buf[IRC_BUF_LEN];
		chat_set_topic(c, NULL, po);
		g_snprintf(buf, sizeof(buf), _("<B>%s has changed the topic to: %s</B>"),
				text, po);
		write_to_conv(c, buf, WFLAG_SYSTEM, NULL, time(NULL), -1);
	}
}

static gboolean mode_has_arg(struct gaim_connection *gc, char sign, char mode)
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

static void irc_user_mode(struct gaim_connection *gc, char *room, char sign, char mode, char *nick)
{
	struct conversation *c = irc_find_chat(gc, room);
	GList *r;

	if (mode != 'o' && mode != 'v')
		return;

	if (!c)
		return;

	r = c->in_room;
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
			rename_chat_buddy(c, tmp, buf);
			g_free(tmp);
			return;
		}
		r = r->next;
	}
}

static void handle_mode(struct gaim_connection *gc, char *word[], char *word_eol[], gboolean n324)
{
	struct irc_data *id = gc->proto_data;
	int offset = n324 ? 4 : 3;
	char *chan = word[offset];
	struct conversation *c = irc_find_chat(gc, chan);
	char *modes = word[offset + 1];
	int len = strlen(word_eol[offset]) - 1;
	char sign = *modes++;
	int arg = 1;
	char *argstr;

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
		}
		modes++;
	}
}

/* Handle our whois stuff here.  You know what, I have a sore throat.  You know
 * what I think about that? I'm not too pleased with it.  Perhaps I should take
 * some medicine, or perhaps I should go to bed? Blah!! */

static void handle_whois(struct gaim_connection *gc, char *word[], char *word_eol[], int num)
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
	else
		id->liststr = g_string_append(id->liststr, word_eol[5]);
}

static void handle_roomlist(struct gaim_connection *gc, char *word[], char *word_eol[])
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

static void process_numeric(struct gaim_connection *gc, char *word[], char *word_eol[])
{
	struct irc_data *id = gc->proto_data;
	char *text = word_eol[3];
	int n = atoi(word[2]);

	if (!g_strncasecmp(gc->displayname, text, strlen(gc->displayname)))
		text += strlen(gc->displayname) + 1;
	if (*text == ':')
		text++;

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
	case 301:
		if (id->in_whois) {
			id->liststr = g_string_append(id->liststr, "<BR><b>Away: </b>");

			if (word_eol[5][0] == ':')
				id->liststr = g_string_append(id->liststr, word_eol[5] + 1);
			else
				id->liststr = g_string_append(id->liststr, word_eol[5]);
		} else
			irc_got_im(gc, word[4], word_eol[5], IM_FLAG_AWAY, time(NULL));
		break;
	case 303:
		handle_list(gc, &word_eol[4][1]);
		break;
	case 311:
	case 312:
	case 313:
	case 317:
	case 319:
		handle_whois(gc, word, word_eol, n);
		break;
	case 322:
		handle_roomlist(gc, word, word_eol);
		break;
	case 323:
	case 318:
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
	case 324:
		handle_mode(gc, word, word_eol, TRUE);
		break;
	case 332:
		handle_topic(gc, text);
		break;
	case 353:
		handle_names(gc, word[5], word_eol[6]);
		break;
	case 376:
		irc_request_buddy_update(gc);
		break;
	case 401:
		do_error_dialog(_("No such nick/channel"), _("IRC Error"));
		break;
	case 402:
		do_error_dialog(_("No such server"), _("IRC Error"));
	case 431:
		do_error_dialog(_("No nickname given"), _("IRC Error"));
		break;
	}
}

static gboolean is_channel(struct gaim_connection *gc, char *name)
{
	struct irc_data *id = gc->proto_data;
	if (strchr(id->chantypes, *name))
		return TRUE;
	return FALSE;
}

static void irc_rem_chat_bud(struct gaim_connection *gc, char *nick, char *reason)
{
	GSList *bcs = gc->buddy_chats;

	while (bcs) {
		struct conversation *b = bcs->data;

		GList *r = b->in_room;
		while (r) {
			char *who = r->data;
			if (*who == '@')
				who++;
			if (*who == '+')
				who++;
			if (!g_strcasecmp(who, nick)) {
				char *tmp = g_strdup(r->data);
				remove_chat_buddy(b, tmp, reason);
				g_free(tmp);
				break;
			}
			r = r->next;
		}
		bcs = bcs->next;
	}
}

static void irc_change_name(struct gaim_connection *gc, char *old, char *new)
{
	GSList *bcs = gc->buddy_chats;
	char buf[IRC_BUF_LEN];

	while (bcs) {
		struct conversation *b = bcs->data;

		GList *r = b->in_room;
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
				rename_chat_buddy(b, tmp, buf);
				r = b->in_room;
				g_free(tmp);
				break;
			} else
				r = r->next;
		}
		bcs = bcs->next;
	}
}

static void handle_privmsg(struct gaim_connection *gc, char *to, char *nick, char *msg)
{
	if (is_channel(gc, to)) {
		struct conversation *c = irc_find_chat(gc, to);
		if (!c)
			return;
		irc_got_chat_in(gc, c->id, nick, 0, msg, time(NULL));
	} else {
		char *tmp = g_malloc(strlen(nick) + 2);
		g_snprintf(tmp, strlen(nick) + 2, "@%s", nick);
		if (find_conversation(tmp))
			irc_got_im(gc, tmp, msg, 0, time(NULL));
		else {
			*tmp = '+';
			if (find_conversation(tmp))
				irc_got_im(gc, tmp, msg, 0, time(NULL));
			else
				irc_got_im(gc, nick, msg, 0, time(NULL));
		}
		g_free(tmp);
	}
}

static void dcc_chat_init(gpointer obj, struct dcc_chat *data) {
	struct dcc_chat * chat = g_new0(struct dcc_chat, 1);
	
	memcpy(chat, data, sizeof(struct dcc_chat));  /* we have to make a new one
						       * because the old one get's freed by
						       * dcc_chat_cancel. */
	proxy_connect(chat->ip_address, chat->port, dcc_chat_callback, chat);
}

static void dcc_chat_cancel(gpointer obj, struct dcc_chat *data){
	if (find_dcc_chat(data->gc, data->nick)) {
		dcc_chat_list = g_slist_remove(dcc_chat_list, data); 
		gaim_input_remove (data->inpa);
		close (data->fd);
	}
	g_free(data);
}

static void irc_convo_closed(struct gaim_connection *gc, char *who)
{
	struct dcc_chat *dchat = find_dcc_chat(gc, who);
	if (!dchat)
		return;

	dcc_chat_cancel(NULL, dchat);
}

static void handle_ctcp(struct gaim_connection *gc, char *to, char *nick,
			char *msg, char *word[], char *word_eol[])
{
	struct irc_data *id = gc->proto_data;
	char buf[IRC_BUF_LEN];

	if (!g_strncasecmp(msg, "VERSION", 7)) {
		g_snprintf(buf, sizeof(buf), "NOTICE %s :\001VERSION Gaim " VERSION ": The Pimpin "
					     "Penguin AIM Clone: " WEBSITE "\001\r\n", nick);
		irc_write(id->fd, buf, strlen(buf));
	}
	if (!g_strncasecmp(msg, "ACTION", 6)) {
		char *po = strchr(msg + 6, 1);
		char *tmp;
		if (po) *po = 0;
		tmp = g_strconcat("/me", msg + 6, NULL);
		handle_privmsg(gc, to, nick, tmp);
		g_free(tmp);
	}
	if (!g_strncasecmp(msg, "DCC CHAT", 8)) {
		char **chat_args = g_strsplit(msg, " ", 5);
		char ask[1024];
		struct dcc_chat * dccchat = g_new0(struct dcc_chat, 1);
		dccchat->gc = gc;	
		g_snprintf(dccchat->ip_address, sizeof(dccchat->ip_address), chat_args[3]);	
		dccchat->port=atoi(chat_args[4]);		
		g_snprintf(dccchat->nick, sizeof(dccchat->nick), nick);	
		g_snprintf(ask, sizeof(ask), _("%s has requested a DCC chat.  "
					       "Would you like to establish the direct connection?"), nick);
		do_ask_dialog(ask, dccchat, dcc_chat_init, dcc_chat_cancel);
	}
	/* XXX should probably write_to_conv or something here */
}

static gboolean irc_parse(struct gaim_connection *gc, char *buf)
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

		if (bud_list_cache_exists(gc))
			do_import(gc, NULL);

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
		char *chan = *word[3] == ':' ? word[3] + 1 : word[3];
		if (!g_strcasecmp(gc->displayname, nick)) {
			static int id = 1;
			serv_got_joined_chat(gc, id++, chan);
		} else {
			struct conversation *c = irc_find_chat(gc, chan);
			if (c)
				add_chat_buddy(c, nick);
		}
	} else if (!strcmp(cmd, "KICK")) {
		if (!strcmp(gc->displayname, word[4])) {
			struct conversation *c = irc_find_chat(gc, word[3]);
			if (!c)
				return FALSE;
			gc->buddy_chats = g_slist_remove(gc->buddy_chats, c);
			c->gc = NULL;
			g_snprintf(outbuf, sizeof(outbuf), _("You have been kicked from %s: %s"),
					word[3], *word_eol[5] == ':' ? word_eol[5] + 1 : word_eol[5]);
			do_error_dialog(outbuf, _("IRC Error"));
		} else {
			char *reason = *word_eol[5] == ':' ? word_eol[5] + 1 : word_eol[5];
			char *msg = g_strdup_printf(_("Kicked by %s: %s"), nick, reason);
			irc_rem_chat_bud(gc, word[4], msg);
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
		if (*word_eol[4] == ':') word_eol[4]++;
		if (ex)
			irc_got_im(gc, nick, word_eol[4], 0, time(NULL));
	} else if (!strcmp(cmd, "PART")) {
		char *chan = cmd + 5;
		struct conversation *c;
		char *reason = word_eol[4];
		GList *r;
		if (*chan == ':')
			chan++;
		if (*reason == ':')
			reason++;
		if (!(c = irc_find_chat(gc, chan)))
			return FALSE;
		if (!strcmp(nick, gc->displayname)) {
			serv_got_chat_left(gc, c->id);
			return FALSE;
		}
		r = c->in_room;
		while (r) {
			char *who = r->data;
			if (*who == '@')
				who++;
			if (*who == '+')
				who++;
			if (!g_strcasecmp(who, nick)) {
				char *tmp = g_strdup(r->data);
				remove_chat_buddy(c, tmp, reason);
				g_free(tmp);
				break;
			}
			r = r->next;
		}
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
		irc_rem_chat_bud(gc, nick, *word_eol[3] == ':' ? word_eol[3] + 1 : word_eol[3]);
	} else if (!strcmp(cmd, "TOPIC")) {
		struct conversation *c = irc_find_chat(gc, word[3]);
		char *topic = *word_eol[4] == ':' ? word_eol[4] + 1 : word_eol[4];
		if (c) {
			char buf[IRC_BUF_LEN];
			chat_set_topic(c, nick, topic);
			g_snprintf(buf, sizeof(buf), _("<B>%s has changed the topic to: %s</B>"),
					nick, topic);
			write_to_conv(c, buf, WFLAG_SYSTEM, NULL, time(NULL), -1);
		}
	} else if (!strcmp(cmd, "WALLOPS")) { /* */
	}

	return FALSE;
}

static void irc_callback(gpointer data, gint source, GaimInputCondition condition)
{
	struct gaim_connection *gc = data;
	struct irc_data *idata = gc->proto_data;
	int i = 0;
	gchar buf[1024];
	gboolean off;

	i = read(idata->fd, buf, 1024);
	if (i <= 0) {
		hide_login_progress(gc, "Read error");
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

static void irc_login_callback(gpointer data, gint source, GaimInputCondition condition)
{
	struct gaim_connection *gc = data;
	struct irc_data *idata;
	char hostname[256];
	char buf[IRC_BUF_LEN];

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

	if (idata->fd != source)
		idata->fd = source;

	gethostname(hostname, sizeof(hostname) - 1);
	hostname[sizeof(hostname) - 1] = 0;
	if (!*hostname)
		g_snprintf(hostname, sizeof(hostname), "localhost");
	g_snprintf(buf, sizeof(buf), "USER %s %s %s :Gaim (%s)\r\n",
		   g_get_user_name(), hostname, gc->user->proto_opt[USEROPT_SERV], WEBSITE);
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

static void irc_login(struct aim_user *user)
{
	char buf[IRC_BUF_LEN];

	struct gaim_connection *gc = new_gaim_conn(user);
	struct irc_data *idata = gc->proto_data = g_new0(struct irc_data, 1);

	g_snprintf(gc->displayname, sizeof(gc->displayname), "%s", gc->username);

	g_snprintf(buf, sizeof(buf), "Signon: %s", gc->username);
	set_login_progress(gc, 2, buf);

	idata->chantypes = g_strdup("#&!+");
	idata->chanmodes = g_strdup("beI,k,l");
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

static void irc_close(struct gaim_connection *gc)
{
	struct irc_data *idata = (struct irc_data *)gc->proto_data;
	gchar buf[IRC_BUF_LEN];

	g_snprintf(buf, sizeof(buf), "QUIT :Download Gaim [%s]\r\n", WEBSITE);
	irc_write(idata->fd, buf, strlen(buf));

	if (idata->rxqueue)
		g_free(idata->rxqueue);
	idata->rxqueue = NULL;
	idata->rxlen = 0;

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

static GList *irc_user_opts()
{
	GList *m = NULL;
	struct proto_user_opt *puo;

	puo = g_new0(struct proto_user_opt, 1);
	puo->label = "Server:";
	puo->def = "irc.mozilla.org";
	puo->pos = USEROPT_SERV;
	m = g_list_append(m, puo);

	puo = g_new0(struct proto_user_opt, 1);
	puo->label = "Port:";
	puo->def = "6667";
	puo->pos = USEROPT_PORT;
	m = g_list_append(m, puo);

	return m;
}

static void set_mode_3(struct gaim_connection *gc, char *who, int sign, int mode,
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

static void set_mode_6(struct gaim_connection *gc, char *who, int sign, int mode,
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

static void set_mode(struct gaim_connection *gc, char *who, int sign, int mode, char *word[])
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

static int handle_command(struct gaim_connection *gc, char *who, char *what)
{
	char buf[IRC_BUF_LEN];
	char pdibuf[IRC_BUF_LEN];
	char *word[PDIWORDS], *word_eol[PDIWORDS];
	char *tmp = g_strdup(what);
	GString *str = encode_html(tmp);
	struct dcc_chat *dccchat = find_dcc_chat(gc, who);
	struct irc_data *id = gc->proto_data;
	g_free(tmp);
	if (*what != '/') {
		unsigned int max = 440 - strlen(who);
		char t;
		while (strlen(what) > max) {
			t = what[max];
			what[max] = 0;
			if (dccchat) {
				g_snprintf(buf, sizeof(buf), "%s\r\n", str->str);
				irc_write(dccchat->fd, buf, strlen(buf));
				g_string_free(str, TRUE);
				return 1;
			}
			g_snprintf(buf, sizeof(buf), "PRIVMSG %s :%s\r\n", who, str->str);
			irc_write(id->fd, buf, strlen(buf));
			what[max] = t;
			what = what + max;
		}
		if (dccchat) {
			g_snprintf(buf, sizeof(buf), "%s\r\n", str->str);
			irc_write(dccchat->fd, buf, strlen(buf));
			g_string_free(str, TRUE);
			return 1;
		}
		g_snprintf(buf, sizeof(buf), "PRIVMSG %s :%s\r\n", who, str->str);
		irc_write(id->fd, buf, strlen(buf));
		g_string_free(str, TRUE);
		return 1;
	}
	
	what++;
	process_data_init(pdibuf, str->str, word, word_eol, TRUE);
	g_string_free(str, TRUE);
	if (!g_strcasecmp(pdibuf, "ME")) {
		if (dccchat) {
			g_snprintf(buf, sizeof(buf), "\001ACTION %s\001\r\n", word_eol[2]);
			irc_write(dccchat->fd, buf, strlen(buf));
			return 1;
		}
		g_snprintf(buf, sizeof(buf), "PRIVMSG %s :\001ACTION %s\001\r\n", who, word_eol[2]);
		irc_write(id->fd, buf, strlen(buf));
		return 1;
	} else if (!g_strcasecmp(pdibuf, "INVITE")) {
		char buf[IRC_BUF_LEN];
		g_snprintf(buf, sizeof(buf), "INVITE %s\r\n", word_eol[2]);
		irc_write(id->fd, buf, strlen(buf));
	} else if (!g_strcasecmp(pdibuf, "TOPIC")) {
		if (!*word_eol[2])
			return -EINVAL;
		g_snprintf(buf, sizeof(buf), "TOPIC %s :%s\r\n", who, word_eol[2]);
		irc_write(id->fd, buf, strlen(buf));
	} else if (!g_strcasecmp(pdibuf, "NICK")) {
		if (!*word_eol[2])
			return -EINVAL;
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
	} else if (!g_strcasecmp(pdibuf, "QUOTE")) {
		if (!*word_eol[2])
			return -EINVAL;
		g_snprintf(buf, sizeof(buf), "%s\r\n", word_eol[2]);
		irc_write(id->fd, buf, strlen(buf));
	} else if (!g_strcasecmp(pdibuf, "SAY")) {
		if (!*word_eol[2])
			return -EINVAL;
		g_snprintf(buf, sizeof(buf), "PRIVMSG %s :%s\r\n", who, word_eol[2]);
		irc_write(id->fd, buf, strlen(buf));
		return 1;
	} else if (!g_strcasecmp(pdibuf, "MSG")) {
		if (!*word[2])
			return -EINVAL;
		if (!*word_eol[3])
			return -EINVAL;
		g_snprintf(buf, sizeof(buf), "PRIVMSG %s :%s\r\n", word[2], word_eol[3]);
		irc_write(id->fd, buf, strlen(buf));
	} else if (!g_strcasecmp(pdibuf, "KICK")) {
		if (!*word[2])
			return -EINVAL;
		if (*word_eol[3])
			g_snprintf(buf, sizeof(buf), "KICK %s %s :%s\r\n", who, word[2], word_eol[3]);
		else
			g_snprintf(buf, sizeof(buf), "KICK %s %s\r\n", who, word[2]);
		irc_write(id->fd, buf, strlen(buf));
	} else if (!g_strcasecmp(pdibuf, "JOIN") || !g_strcasecmp(pdibuf, "J")) {
		if (!*word[2])
			return -EINVAL;
		if (*word[3])
			g_snprintf(buf, sizeof(buf), "JOIN %s %s\r\n", word[2], word[3]);
		else
			g_snprintf(buf, sizeof(buf), "JOIN %s\r\n", word[2]);
		irc_write(id->fd, buf, strlen(buf));
	} else if (!g_strcasecmp(pdibuf, "PART")) {
		char *chan = *word[2] ? word[2] : who;
		char *reason = word_eol[3];
		struct conversation *c;
		if (!is_channel(gc, chan))
			return -EINVAL;
		c = irc_find_chat(gc, chan);
		g_snprintf(buf, sizeof(buf), "PART %s%s%s\r\n", chan,
				*reason ? " :" : "",
				*reason ? reason : "");
		irc_write(id->fd, buf, strlen(buf));
		if (c) {
			gc->buddy_chats = g_slist_remove(gc->buddy_chats, c);
			c->gc = NULL;
			g_snprintf(buf, sizeof(buf), _("You have left %s"), chan);
			do_error_dialog(buf, _("IRC Part"));
		}
	} else if (!g_strcasecmp(pdibuf, "WHOIS")) {
		g_snprintf(buf, sizeof(buf), "WHOIS %s\r\n", word_eol[2]);
		irc_write(id->fd, buf, strlen(buf));
	} else if (!g_strcasecmp(pdibuf, "LIST")) {
		g_snprintf(buf, sizeof(buf), "LIST\r\n");
		irc_write(id->fd, buf, strlen(buf));
	} else if (!g_strcasecmp(pdibuf, "HELP")) {
		struct conversation *c = NULL;
		if (is_channel(gc, who)) {
			c = irc_find_chat(gc, who);
		} else {
			c = find_conversation(who);
		}
		if (!c)
			return -EINVAL;
		write_to_conv(c, "<B>Currently supported commands:<BR>"
				 "WHOIS INVITE NICK LIST<BR>"
				 "JOIN PART TOPIC KICK<BR>"
				 "OP DEOP VOICE DEVOICE<BR>"
				 "ME MSG QUOTE SAY</B>",
				 WFLAG_NOLOG, NULL, time(NULL), -1);
	} else {
		struct conversation *c = NULL;
		if (is_channel(gc, who)) {
			c = irc_find_chat(gc, who);
		} else {
			c = find_conversation(who);
		}
		if (!c)
			return -EINVAL;
		write_to_conv(c, "<B>Unknown command</B>", WFLAG_NOLOG, NULL, time(NULL), -1);
	}

	return 0;
}

static int send_msg(struct gaim_connection *gc, char *who, char *what)
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

static void irc_chat_invite(struct gaim_connection *gc, int idn, char *message, char *name) {
	char buf[IRC_BUF_LEN]; 
	struct irc_data *id = gc->proto_data;
	struct conversation *c = irc_find_chat_by_id(gc, idn);
	g_snprintf(buf, sizeof(buf), "INVITE %s %s\r\n", name, c->name);
	irc_write(id->fd, buf, strlen(buf));
}

static int irc_send_im(struct gaim_connection *gc, char *who, char *what, int flags)
{
	if (*who == '@' || *who == '+')
		return send_msg(gc, who + 1, what);
	return send_msg(gc, who, what);
}

/* IRC doesn't have a buddy list, but we can still figure out who's online with ISON */
static void irc_add_buddy(struct gaim_connection *gc, char *who) {}
static void irc_remove_buddy(struct gaim_connection *gc, char *who, char *group) {}

static GList *irc_chat_info(struct gaim_connection *gc)
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

static void irc_join_chat(struct gaim_connection *gc, GList *data)
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

static void irc_chat_leave(struct gaim_connection *gc, int id)
{
	struct irc_data *idata = gc->proto_data;
	struct conversation *c = irc_find_chat_by_id(gc, id);
	char buf[IRC_BUF_LEN];

	if (!c) return;

	g_snprintf(buf, sizeof(buf), "PART %s\r\n", c->name);
	irc_write(idata->fd, buf, strlen(buf));
}

static int irc_chat_send(struct gaim_connection *gc, int id, char *what)
{
	struct conversation *c = irc_find_chat_by_id(gc, id);
	if (!c)
		return -EINVAL;
	if (send_msg(gc, c->name, what) > 0)
		serv_got_chat_in(gc, c->id, gc->displayname, 0, what, time(NULL));
	return 0;
}

static GList *irc_away_states(struct gaim_connection *gc)
{
	return g_list_append(NULL, GAIM_AWAY_CUSTOM);
}

static void irc_set_away(struct gaim_connection *gc, char *state, char *msg)
{
	struct irc_data *idata = gc->proto_data;
	char buf[IRC_BUF_LEN];

	if (gc->away)
		gc->away = NULL;

	if (msg) {
		g_snprintf(buf, sizeof(buf), "AWAY :%s\r\n", msg);
		gc->away = "";
	} else
		g_snprintf(buf, sizeof(buf), "AWAY\r\n");
	irc_write(idata->fd, buf, strlen(buf));
}

static char **irc_list_icon(int uc)
{
	return irc_icon_xpm;
}

static int getlocalip(char *ip) /* Thanks, libfaim */
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

static void dcc_chat_connected(gpointer data, gint source, GdkInputCondition condition)
{
	struct dcc_chat *chat = data;
	struct conversation *convo;
	char buf[128];
	struct sockaddr_in addr;
	int addrlen = sizeof (addr);
	addr.sin_family = AF_INET;
	addr.sin_port = htons (chat->port);
	addr.sin_addr.s_addr = INADDR_ANY;
	chat->fd = accept (chat->fd, (struct sockaddr *) (&addr), &addrlen);
	if (!chat->fd)
	  {
		  dcc_chat_cancel (NULL,chat);
		  convo = new_conversation (chat->nick);
		  g_snprintf (buf, sizeof buf, _("DCC Chat with %s closed"),
			      chat->nick);
		  write_to_conv (convo, buf, WFLAG_SYSTEM, NULL,
				 time ((time_t) NULL), -1);
		  return;
	  }
	chat->inpa =
		gaim_input_add (chat->fd, GAIM_INPUT_READ, dcc_chat_in, chat);
	convo = new_conversation (chat->nick);
	g_snprintf (buf, sizeof buf, "DCC Chat with %s established",
		    chat->nick);
	write_to_conv (convo, buf, WFLAG_SYSTEM, NULL, time ((time_t) NULL), -1);
	debug_printf ("Chat with %s established\n", chat->nick);
	dcc_chat_list = g_slist_append (dcc_chat_list, chat);
}

static void irc_start_chat(struct gaim_connection *gc, char *who) {
	struct dcc_chat *chat;
	int len;
	struct sockaddr_in addr;
	char buf[200];
	
	/* Create a socket */
	chat = g_new0 (struct dcc_chat, 1);
	chat->fd = socket (AF_INET, SOCK_STREAM, 0);
	chat->gc = gc;
	g_snprintf (chat->nick, sizeof (chat->nick), "%s", who);
	if (chat->fd < 0)  {
		  dcc_chat_cancel (NULL,chat);
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
	snprintf (buf, sizeof buf, "\001DCC CHAT chat %s %d\001\n",
		  chat->ip_address, chat->port);
	irc_send_im (gc, who, buf, 0);
}

static void irc_get_info(struct gaim_connection *gc, char *who)
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

static GList *irc_buddy_menu(struct gaim_connection *gc, char *who)
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
	return m;
}

static struct prpl *my_protocol = NULL;

void irc_init(struct prpl *ret)
{
	ret->protocol = PROTO_IRC;
	ret->options = OPT_PROTO_CHAT_TOPIC | OPT_PROTO_NO_PASSWORD;
	ret->name = irc_name;
	ret->user_opts = irc_user_opts;
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
	my_protocol = ret;
}

#ifndef STATIC

char *gaim_plugin_init(GModule *handle)
{
	load_protocol(irc_init, sizeof(struct prpl));
	return NULL;
}

void gaim_plugin_remove()
{
	struct prpl *p = find_prpl(PROTO_IRC);
	if (p == my_protocol)
		unload_protocol(p);
}

char *name()
{
	return "IRC";
}

char *description()
{
	return PRPL_DESC("IRC");
}

#endif
