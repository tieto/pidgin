/*
 * gaim
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
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
#include <config.h>
#endif
#include <string.h>

#ifndef _WIN32
#include <sys/time.h>
#include <unistd.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include "gaim.h"
#include "prpl.h"
#include "proxy.h"

#ifdef _WIN32
#include "win32dep.h"
#endif

/* for people like myself, who are too lazy to add an away msg :) */
#define BORING_DEFAULT_AWAY_MSG "sorry, i ran out for a while. bbl"
#define MAX_VALUES 10

GSList *aim_users = NULL;
guint misc_options;
guint logging_options;
guint blist_options;
guint convo_options;
guint im_options;
guint chat_options;
guint font_options;
guint sound_options;
guint away_options;
guint away_resend;
static guint is_loading_prefs = 0;
static guint request_save_prefs = 0;
static guint is_saving_prefs = 0;
static guint request_load_prefs = 0;
guint proxy_info_is_from_gaimrc = 1; /* Only save proxy info if it
				      * was loaded from the file
				      * or otherwise explicitly requested */

int report_idle;
int web_browser;
struct save_pos blist_pos;
struct window_size conv_size, buddy_chat_size;
char web_command[2048] = "";
char *sound_file[NUM_SOUNDS];
#ifndef _WIN32
char sound_cmd[2048];
#endif

struct parse {
	char option[256];
	char value[MAX_VALUES][4096];
};

static struct parse *parse_line(char *line, struct parse *p)
{
	char *c = line;
	int inopt = 1, inval = 0, curval = -1;
	int optlen = 0, vallen = 0, last_non_space = 0;
	int x;

	for (x = 0; x < MAX_VALUES; x++) {
		p->value[x][0] = 0;
	}

	while (*c) {
		if (*c == '\t') {
			c++;
			continue;
		}
		
		if (inopt) {
			if ((*c < 'a' || *c > 'z') && *c != '_' && (*c < 'A' || *c > 'Z')) {
				inopt = 0;
				p->option[optlen] = 0;
				c++;
				continue;
			}

			p->option[optlen] = *c;
			optlen++;
			c++;
			continue;
		} else if (inval) {
			if (*c == '\\') {
				/* if we have a \ take the char after it literally.. */
				c++;
				p->value[curval][vallen] = *c;

				vallen++;
				last_non_space = vallen;
				c++;
				continue;
			} else if (*c == '}') {
				/* } that isn't escaped should end this chunk of data, and
				 * should have a space before it.. */
				p->value[curval][last_non_space] = 0;
				inval = 0;
				c++;
				continue;
			} else {
				p->value[curval][vallen] = *c;

				vallen++;
				if (! isspace(*c))
					last_non_space = vallen;
				c++;
				continue;
			}
		} else if (*c == '{') {
			/* i really don't think this if ever succeeds, but i'm
			 * not brave enough to take it out... */ 
			if (*(c - 1) == '\\') {
				p->value[curval][vallen] = *c;
				c++;
				continue;
			} else {
				/* { that isn't escaped should signify the start of a
				 * piece of data and should have a space after it.. */
				curval++;
				vallen = 0;
				last_non_space = vallen;
				inval = 1;
				c++;
				while (*c && isspace(*c))
				  c++;
				continue;
			}
		}
		c++;
	}

	return p;
}


static int gaimrc_parse_tag(FILE *f)
{
	char buf[2048];
	char tag[256];
	buf[0] = '#';

	while (buf[0] == '#' && !feof(f))
		fgets(buf, sizeof(buf), f);

	if (feof(f))
		return -1;

	sscanf(buf, "%s {", tag);

	if (!strcmp(tag, "users")) {
		return 0;
	} else if (!strcmp(tag, "options")) {
		return 1;
	} else if (!strcmp(tag, "away")) {
		return 2;
	} else if (!strcmp(tag, "plugins")) {
		return 3;
	} else if (!strcmp(tag, "pounce")) {
		return 4;
	} else if (!strcmp(tag, "sound_files")) {
		return 6;
	} else if (!strcmp(tag, "proxy")) {
		return 7;
	} else if (!strcmp(tag, "wgaim")) {
		return 8;
	}
	return -1;
}

static void filter_break(char *msg)
{
	char *c;
	int mc;
	int cc;

	c = g_malloc(strlen(msg) + 1);
	strcpy(c, msg);

	mc = 0;
	cc = 0;
	while (c[cc] != '\0') {
		if (c[cc] == '\\') {
			cc++;
			msg[mc] = c[cc];
		} else {
			msg[mc] = c[cc];
		}
		mc++;
		cc++;
	}
	msg[mc] = 0;
	g_free(c);
}

static char *escape_text2(const char *msg)
{
	char *c, *cpy;
	char *woo;
	int cnt = 0;
	/* Assumes you have a buffer able to cary at least BUF_LEN * 2 bytes */

	woo = malloc(strlen(msg) * 4 + 1);
	cpy = g_strndup(msg, 2048);
	c = cpy;
	while (*c) {
		switch (*c) {
		case '\n':
			woo[cnt++] = '<';
			woo[cnt++] = 'B';
			woo[cnt++] = 'R';
			woo[cnt++] = '>';
			break;
		case '{':
		case '}':
		case '\\':
		case '"':
			woo[cnt++] = '\\';
			/* Fall through */
		default:
			woo[cnt++] = *c;
		}
		c++;
	}
	woo[cnt] = '\0';

	g_free(cpy);
	return woo;
}

static void gaimrc_read_away(FILE *f)
{
	struct parse parse_buffer;
	struct parse *p;
	char buf[4096];
	struct away_message *a;

	buf[0] = 0;

	while (buf[0] != '}') {
		if (!fgets(buf, sizeof(buf), f))
			return;

		if (buf[0] == '}')
			return;

		p = parse_line(buf, &parse_buffer);
		if (!strcmp(p->option, "message")) {
			a = g_new0(struct away_message, 1);

			g_snprintf(a->name, sizeof(a->name), "%s", p->value[0]);
			g_snprintf(a->message, sizeof(a->message), "%s", p->value[1]);
			filter_break(a->name);
			filter_break(a->message);
			away_messages = g_slist_insert_sorted(away_messages, a, sort_awaymsg_list);
		}
		/* auto { time } { default message } */
		else if (!strcmp(p->option, "auto")) {
			auto_away = atoi(p->value[0]);
			default_away = g_slist_nth_data(away_messages, atoi(p->value[1]));
		}
	}
	if (!away_messages) {
		a = g_new0(struct away_message, 1);
		g_snprintf(a->name, sizeof(a->name), "boring default");
		g_snprintf(a->message, sizeof(a->message), "%s", BORING_DEFAULT_AWAY_MSG);
		away_messages = g_slist_append(away_messages, a);
	}
}

static void gaimrc_write_away(FILE *f)
{
	GSList *awy = away_messages;
	struct away_message *a;

	fprintf(f, "away {\n");

	if (awy) {
		while (awy) {
			char *str1, *str2;

			a = (struct away_message *)awy->data;

			str1 = escape_text2(a->name);
			str2 = escape_text2(a->message);

			fprintf(f, "\tmessage { %s } { %s }\n", str1, str2);

			/* escape_text2 uses malloc(), so we don't want to g_free these */
			free(str1);
			free(str2);

			awy = g_slist_next(awy);
		}
		fprintf(f, "\tauto { %d } { %d }\n", auto_away,
			g_slist_index(away_messages, default_away));
	} else {
		fprintf(f, "\tmessage { boring default } { %s }\n", BORING_DEFAULT_AWAY_MSG);
		fprintf(f, "\tauto { 0 } { 0 }\n");
	}

	fprintf(f, "}\n");
}

static void gaimrc_read_pounce(FILE *f)
{
	struct parse parse_buffer;
	struct parse *p;
	char buf[4096];
	struct buddy_pounce *b;

	buf[0] = 0;

	while (buf[0] != '}') {
		if (!fgets(buf, sizeof(buf), f))
			return;

		if (buf[0] == '}')
			return;

		p = parse_line(buf, &parse_buffer);
		if (!strcmp(p->option, "entry")) {
			b = g_new0(struct buddy_pounce, 1);

			g_snprintf(b->name, sizeof(b->name), "%s", p->value[0]);
			filter_break(b->name);
			g_snprintf(b->message, sizeof(b->message), "%s", p->value[1]);
			filter_break(b->message);
			g_snprintf(b->command, sizeof(b->command), "%s", p->value[2]);
			filter_break(b->command);

			b->options = atoi(p->value[3]);

			g_snprintf(b->pouncer, sizeof(b->pouncer), "%s", p->value[4]);
			filter_break(b->pouncer);
			b->protocol = atoi(p->value[5]);

			g_snprintf(b->sound, sizeof(b->sound), "%s", p->value[6]);
			filter_break(b->sound);

			buddy_pounces = g_list_append(buddy_pounces, b);
		}
	}
}

static void gaimrc_write_pounce(FILE *f)
{
	GList *pnc = buddy_pounces;
	struct buddy_pounce *b;

	fprintf(f, "pounce {\n");

	while (pnc) {
		char *str1, *str2, *str3, *str4;

		b = (struct buddy_pounce *)pnc->data;

		str1 = escape_text2(b->name);
		if (strlen(b->message))
			str2 = escape_text2(b->message);
		else {
			str2 = malloc(1);
			str2[0] = 0;
		}
		if (strlen(b->command))
			str3 = escape_text2(b->command);
		else {
			str3 = malloc(1);
			str3[0] = 0;
		}
		if (strlen(b->sound))
			str4 = escape_text2(b->sound);
		else {
			str4 = malloc(1);
			str4[0] = 0;
		}

		fprintf(f, "\tentry { %s } { %s } { %s } { %d } { %s } { %d } { %s }\n",
			str1, str2, str3, b->options, b->pouncer, b->protocol, str4);

		/* escape_text2 uses malloc(), so we don't want to g_free these */
		free(str1);
		free(str2);
		free(str3);
		free(str4);

		pnc = pnc->next;
	}

	fprintf(f, "}\n");
}

#ifdef GAIM_PLUGINS
static void gaimrc_write_plugins(FILE *f)
{
	GList *pl = plugins;
	struct gaim_plugin *p;

	fprintf(f, "plugins {\n");

	while (pl) {
		char *path;

		p = (struct gaim_plugin *)pl->data;

		path = escape_text2(p->path);
			
		fprintf(f, "\tplugin { %s }\n", path);

		free(path);

		pl = pl->next;
	}

	fprintf(f, "}\n");
}

static void gaimrc_read_plugins(FILE *f)
{
	struct parse parse_buffer;
	struct parse *p;
	char buf[4096];
	GSList *load = NULL;

	buf[0] = 0;

	while (buf[0] != '}') {
		if (!fgets(buf, sizeof(buf), f))
			break;

		if (buf[0] == '}')
			break;

		p = parse_line(buf, &parse_buffer);
		if (!strcmp(p->option, "plugin")) {
#ifndef _WIN32
			filter_break(p->value[0]);
#endif
			load = g_slist_append(load, g_strdup(p->value[0]));
		}
	}
	/* this is such a fucked up hack. the reason we do this is because after
	 * we load a plugin the gaimrc file gets rewrit. so we have to remember
	 * which ones to load before loading them. */
	while (load) {
		if (load->data)
			load_plugin(load->data);
		g_free(load->data);
		load = g_slist_remove(load, load->data);
	}
}
#endif /* GAIM_PLUGINS */

static struct aim_user *gaimrc_read_user(FILE *f)
{
	struct parse parse_buffer;
	struct parse *p;
	struct aim_user *u;
	int i;
	char buf[4096];

	if (!fgets(buf, sizeof(buf), f))
		return NULL;

	p = parse_line(buf, &parse_buffer);

	if (strcmp(p->option, "ident"))
		return NULL;

	u = g_new0(struct aim_user, 1);

	strcpy(u->username, p->value[0]);
	strcpy(u->password, p->value[1]);

	u->user_info[0] = 0;
	u->options = OPT_USR_REM_PASS;
	u->protocol = DEFAULT_PROTO;
	u->permit = u->deny = NULL;

	if (!fgets(buf, sizeof(buf), f))
		return u;

	if (strcmp(buf, "\t\tuser_info {\n")) {
		return u;
	}

	if (!fgets(buf, sizeof(buf), f))
		return u;

	while (strncmp(buf, "\t\t}", 3)) {
		if (strlen(buf) > 3)
			strcat(u->user_info, buf + 3);

		if (!fgets(buf, sizeof(buf), f)) {
			return u;
		}
	}

	if ((i = strlen(u->user_info))) {
		u->user_info[i - 1] = '\0';
	}

	if (!fgets(buf, sizeof(buf), f)) {
		return u;
	}

	if (!strcmp(buf, "\t}")) {
		return u;
	}

	p = parse_line(buf, &parse_buffer);

	if (strcmp(p->option, "user_opts"))
		return u;

	u->options = atoi(p->value[0]);
	u->protocol = atoi(p->value[1]);

	if (!fgets(buf, sizeof(buf), f))
		return u;

	if (!strcmp(buf, "\t}"))
		return u;

	p = parse_line(buf, &parse_buffer);

	if (strcmp(p->option, "proto_opts"))
		return u;

	for (i = 0; i < 7; i++)
		g_snprintf(u->proto_opt[i], sizeof u->proto_opt[i], "%s", p->value[i]);

	if (!fgets(buf, sizeof(buf), f))
		return u;

	if (!strcmp(buf, "\t}"))
		return u;

	p = parse_line(buf, &parse_buffer);

	if (strcmp(p->option, "iconfile"))
		return u;

	g_snprintf(u->iconfile, sizeof(u->iconfile), "%s", p->value[0]);

	if (!fgets(buf, sizeof(buf), f))
		return u;

	if (!strcmp(buf, "\t}"))
		return u;

	p = parse_line(buf, &parse_buffer);

	if (strcmp(p->option, "alias"))
		return u;

	g_snprintf(u->alias, sizeof(u->alias), "%s", p->value[0]);

	return u;

}

static void gaimrc_write_user(FILE *f, struct aim_user *u)
{
	char *c;
	int nl = 1, i;

	if (u->options & OPT_USR_REM_PASS) {
		fprintf(f, "\t\tident { %s } { %s }\n", u->username, (c = escape_text2(u->password)));
		free(c);
	} else {
		fprintf(f, "\t\tident { %s } {  }\n", u->username);
	}
	fprintf(f, "\t\tuser_info {");
	c = u->user_info;
	while (*c) {
		/* This is not as silly as it looks. */
		if (*c == '\n') {
			nl++;
		} else {
			if (nl) {
				while (nl) {
					fprintf(f, "\n\t\t\t");
					nl--;
				}
			}
			fprintf(f, "%c", *c);
		}
		c++;
	}
	fprintf(f, "\n\t\t}\n");
	fprintf(f, "\t\tuser_opts { %d } { %d }\n", u->options, u->protocol);
	fprintf(f, "\t\tproto_opts");
	for (i = 0; i < 7; i++)
		fprintf(f, " { %s }", u->proto_opt[i]);
	fprintf(f, "\n");
	fprintf(f, "\t\ticonfile { %s }\n", u->iconfile);
	fprintf(f, "\t\talias { %s }\n", u->alias);
}

static void gaimrc_read_users(FILE *f)
{
	char buf[2048];
	struct aim_user *u;
	struct parse parse_buffer;
	struct parse *p;

	buf[0] = 0;

	while (buf[0] != '}') {
		if (buf[0] == '#')
			continue;

		if (!fgets(buf, sizeof(buf), f))
			return;



		p = parse_line(buf, &parse_buffer);

		if (!strcmp(p->option, "current_user")) {
		} else if (strcmp(p->option, "user")) {
			continue;
		} else {
		}

		u = gaimrc_read_user(f);

		aim_users = g_slist_append(aim_users, u);
	}
}

static void gaimrc_write_users(FILE *f)
{
	GSList *usr = aim_users;
	struct aim_user *u;

	fprintf(f, "users {\n");

	while (usr) {
		u = (struct aim_user *)usr->data;
		fprintf(f, "\tuser {\n");
		gaimrc_write_user(f, u);

		fprintf(f, "\t}\n");

		usr = usr->next;
	}

	fprintf(f, "}\n");
}

struct replace {
	int old;
	guint *val;
	int new;
};

static struct replace gen_replace[] = {
{ /* OPT_GEN_ENTER_SENDS */		0x00000001, &convo_options, OPT_CONVO_ENTER_SENDS },
{ /* OPT_GEN_APP_BUDDY_SHOW */		0x00000010, &blist_options, OPT_BLIST_APP_BUDDY_SHOW },
{ /* OPT_GEN_POPUP_WINDOWS */		0x00000020,    &im_options, OPT_IM_POPUP },
{ /* OPT_GEN_SEND_LINKS */		0x00000040, &convo_options, OPT_CONVO_SEND_LINKS },
{ /* OPT_GEN_DEBUG */			0x00000100,  &misc_options, OPT_MISC_DEBUG },
{ /* OPT_GEN_BROWSER_POPUP */		0x00000800,  &misc_options, OPT_MISC_BROWSER_POPUP },
{ /* OPT_GEN_SAVED_WINDOWS */		0x00001000, &blist_options, OPT_BLIST_SAVED_WINDOWS },
{ /* OPT_GEN_DISCARD_WHEN_AWAY */	0x00002000,  &away_options, OPT_AWAY_DISCARD },
{ /* OPT_GEN_NEAR_APPLET */		0x00004000, &blist_options, OPT_BLIST_NEAR_APPLET },
{ /* OPT_GEN_CHECK_SPELLING */		0x00008000, &convo_options, OPT_CONVO_CHECK_SPELLING },
{ /* OPT_GEN_POPUP_CHAT */		0x00010000,  &chat_options, OPT_CHAT_POPUP },
{ /* OPT_GEN_BACK_ON_IM */		0x00020000,  &away_options, OPT_AWAY_BACK_ON_IM },
{ /* OPT_GEN_CTL_CHARS */		0x00080000, &convo_options, OPT_CONVO_CTL_CHARS },
#if 0
{ /* OPT_GEN_TIK_HACK */		0x00100000,  &away_options, OPT_AWAY_TIK_HACK },
#endif
{ /* OPT_GEN_CTL_SMILEYS */		0x00200000, &convo_options, OPT_CONVO_CTL_SMILEYS },
{ /* OPT_GEN_AUTO_AWAY */		0x00800000,  &away_options, OPT_AWAY_AUTO },
{ /* OPT_GEN_ESC_CAN_CLOSE */		0x01000000, &convo_options, OPT_CONVO_ESC_CAN_CLOSE },
{ /* OPT_GEN_CTL_ENTER */		0x02000000, &convo_options, OPT_CONVO_CTL_ENTER },
{ /* OPT_GEN_F2_TOGGLES */		0x04000000, &convo_options, OPT_CONVO_F2_TOGGLES },
{ /* OPT_GEN_NO_AUTO_RESP */		0x08000000,  &away_options, OPT_AWAY_NO_AUTO_RESP },
{ /* OPT_GEN_QUEUE_WHEN_AWAY */		0x10000000,  &away_options, OPT_AWAY_QUEUE },
{ 0,NULL,0 }
};

#define OPT_GEN_LOG_ALL		0x00000004
#define OPT_GEN_STRIP_HTML	0x00000008

static struct replace disp_replace[] = {
{ /* OPT_DISP_SHOW_TIME */		0x00000001, &convo_options, OPT_CONVO_SHOW_TIME },
{ /* OPT_DISP_SHOW_GRPNUM */		0x00000002, &blist_options, OPT_BLIST_SHOW_GRPNUM },
{ /* OPT_DISP_SHOW_PIXMAPS */		0x00000004, &blist_options, OPT_BLIST_SHOW_PIXMAPS },
{ /* OPT_DISP_SHOW_IDLETIME */		0x00000008, &blist_options, OPT_BLIST_SHOW_IDLETIME },
{ /* OPT_DISP_SHOW_BUTTON_XPM */	0x00000010, &blist_options, OPT_BLIST_SHOW_BUTTON_XPM },
{ /* OPT_DISP_IGNORE_COLOUR */		0x00000020, &convo_options, OPT_CONVO_IGNORE_COLOUR },
{ /* OPT_DISP_SHOW_LOGON */		0x00000040,    &im_options, OPT_IM_LOGON },
{ /* OPT_DISP_SHOW_SMILEY */		0x00000100, &convo_options, OPT_CONVO_SHOW_SMILEY },
{ /* OPT_DISP_COOL_LOOK */		0x00000400, &misc_options, 0},
{ /* OPT_DISP_CHAT_LOGON */		0x00000800,  &chat_options, OPT_CHAT_LOGON },
{ /* OPT_DISP_NO_BUTTONS */		0x00002000, &blist_options, OPT_BLIST_NO_BUTTONS },
{ /* OPT_DISP_CONV_BUTTON_TEXT */	0x00004000,    &im_options, OPT_IM_BUTTON_TEXT },
{ /* OPT_DISP_CHAT_BUTTON_TEXT */	0x00008000,  &chat_options, OPT_CHAT_BUTTON_TEXT },
{ /* OPT_DISP_NO_MT_GRP */		0x00040000, &blist_options, OPT_BLIST_NO_MT_GRP },
{ /* OPT_DISP_CONV_BUTTON_XPM */	0x00080000,    &im_options, OPT_IM_BUTTON_XPM },
{ /* OPT_DISP_CHAT_BUTTON_XPM */	0x00100000,  &chat_options, OPT_CHAT_BUTTON_XPM },
{ /* OPT_DISP_SHOW_WARN */		0x00200000, &blist_options, OPT_BLIST_SHOW_WARN },
{ /* OPT_DISP_IGNORE_FONTS */		0x00400000, &convo_options, OPT_CONVO_IGNORE_FONTS },
{ /* OPT_DISP_IGNORE_SIZES */		0x00800000, &convo_options, OPT_CONVO_IGNORE_SIZES },
{ /* OPT_DISP_ONE_WINDOW */		0x01000000,    &im_options, OPT_IM_ONE_WINDOW },
{ /* OPT_DISP_ONE_CHAT_WINDOW */	0x02000000,  &chat_options, OPT_CHAT_ONE_WINDOW },
{ /* OPT_DISP_CONV_SIDE_TAB */		0x04000000,    &im_options, OPT_IM_SIDE_TAB },
{ /* OPT_DISP_CONV_BR_TAB */		0x08000000,    &im_options, OPT_IM_BR_TAB },
{ /* OPT_DISP_CHAT_SIDE_TAB */		0x10000000,  &chat_options, OPT_CHAT_SIDE_TAB },
{ /* OPT_DISP_CHAT_BR_TAB */		0x20000000,  &chat_options, OPT_CHAT_BR_TAB },
{ 0,NULL,0 }
};

static void gaimrc_read_options(FILE *f)
{
	char buf[2048];
	struct parse parse_buffer;
	struct parse *p;
	gboolean read_logging = FALSE, read_general = FALSE, read_display = FALSE;
	int general_options = 0, display_options = 0;
	int i;

	buf[0] = 0;

	while (buf[0] != '}') {
		if (buf[0] == '#')
			continue;

		if (!fgets(buf, sizeof(buf), f))
			return;

		p = parse_line(buf, &parse_buffer);

		if (!strcmp(p->option, "general_options")) {
			general_options = atoi(p->value[0]);
			read_general = TRUE;
		} else if (!strcmp(p->option, "display_options")) {
			display_options = atoi(p->value[0]);
			read_display = TRUE;
		} else if (!strcmp(p->option, "misc_options")) {
			misc_options = atoi(p->value[0]);
		} else if (!strcmp(p->option, "logging_options")) {
			logging_options = atoi(p->value[0]);
			read_logging = TRUE;
		} else if (!strcmp(p->option, "blist_options")) {
			blist_options = atoi(p->value[0]);
		} else if (!strcmp(p->option, "convo_options")) {
			convo_options = atoi(p->value[0]);
		} else if (!strcmp(p->option, "im_options")) {
			im_options = atoi(p->value[0]);
		} else if (!strcmp(p->option, "chat_options")) {
			chat_options = atoi(p->value[0]);
		} else if (!strcmp(p->option, "font_options")) {
			font_options = atoi(p->value[0]);
		} else if (!strcmp(p->option, "sound_options")) {
			sound_options = atoi(p->value[0]);
		} else if (!strcmp(p->option, "away_options")) {
			away_options = atoi(p->value[0]);
			away_resend = atoi(p->value[1]);
		} else if (!strcmp(p->option, "font_face")) {
			g_snprintf(fontface, sizeof(fontface), "%s", p->value[0]);
		} else if (!strcmp(p->option, "font_size")) {
			fontsize = atoi(p->value[0]);
		} else if (!strcmp(p->option, "foreground")) {
			fgcolor.red = atoi(p->value[0]);
			fgcolor.green = atoi(p->value[1]);
			fgcolor.blue = atoi(p->value[2]);
		} else if (!strcmp(p->option, "background")) {
			bgcolor.red = atoi(p->value[0]);
			bgcolor.green = atoi(p->value[1]);
			bgcolor.blue = atoi(p->value[2]);
		} else if (!strcmp(p->option, "report_idle")) {
			report_idle = atoi(p->value[0]);
		} else if (!strcmp(p->option, "web_browser")) {
			web_browser = atoi(p->value[0]);
		} else if (!strcmp(p->option, "web_command")) {
			strcpy(web_command, p->value[0]);
		} else if (!strcmp(p->option, "smiley_theme")) {
			load_smiley_theme(p->value[0], TRUE);
		} else if (!strcmp(p->option, "conv_size")) {
			conv_size.width = atoi(p->value[0]);
			conv_size.height = atoi(p->value[1]);
			conv_size.entry_height = atoi(p->value[2]);
		} else if (!strcmp(p->option, "buddy_chat_size")) {
			buddy_chat_size.width = atoi(p->value[0]);
			buddy_chat_size.height = atoi(p->value[1]);
			buddy_chat_size.entry_height = atoi(p->value[2]);
		} else if (!strcmp(p->option, "blist_pos")) {
			blist_pos.x = atoi(p->value[0]);
			blist_pos.y = atoi(p->value[1]);
			blist_pos.width = atoi(p->value[2]);
			blist_pos.height = atoi(p->value[3]);
		}

	}

	/* this is where we do bugs and compatibility stuff */
	if (!(sound_options & (OPT_SOUND_BEEP | OPT_SOUND_ESD | OPT_SOUND_NORMAL | OPT_SOUND_NAS | OPT_SOUND_ARTSC | OPT_SOUND_CMD)))
		sound_options |= OPT_SOUND_ESD | OPT_SOUND_NORMAL | OPT_SOUND_NAS | OPT_SOUND_ARTSC;

	if (conv_size.width == 0 &&
	    conv_size.height == 0 &&
	    conv_size.entry_height == 0) {
		conv_size.width = 410;
		conv_size.height = 160;
		conv_size.entry_height = 50;
	}

	if (buddy_chat_size.width == 0 &&
	    buddy_chat_size.height == 0 &&
	    buddy_chat_size.entry_height == 0) {
		buddy_chat_size.width = 410;
		buddy_chat_size.height = 160;
		buddy_chat_size.entry_height = 50;
	}

	if (read_general) {
		if (!read_logging) {
			logging_options = 0;
			if (general_options & OPT_GEN_LOG_ALL)
  				logging_options |= OPT_LOG_CONVOS | OPT_LOG_CHATS; 
			if (general_options & OPT_GEN_STRIP_HTML)
				logging_options |= OPT_LOG_STRIP_HTML;
		}
		for (i = 0; gen_replace[i].val; i++)
			if (general_options & gen_replace[i].old)
				*gen_replace[i].val |= gen_replace[i].new;
	}

	if (read_display)
		for (i = 0; disp_replace[i].val; i++)
			if (display_options & disp_replace[i].old)
				*disp_replace[i].val |= disp_replace[i].new;

	if (!away_resend)
		away_resend = 120;

	if (misc_options & OPT_MISC_BUDDY_TICKER) {
#ifdef GAIM_PLUGINS
		gchar* buf;

		buf = g_strconcat(LIBDIR, G_DIR_SEPARATOR_S, 
#ifndef _WIN32
				  "ticker.so",
#else
				  "ticker.dll",
#endif
				  NULL);
		load_plugin(buf);
		g_free(buf);
#endif
		misc_options &= ~OPT_MISC_BUDDY_TICKER;
	} 
}

static void gaimrc_write_options(FILE *f)
{

	fprintf(f, "options {\n");

	fprintf(f, "\tmisc_options { %u }\n", misc_options);
	fprintf(f, "\tlogging_options { %u }\n", logging_options);
	fprintf(f, "\tblist_options { %u }\n", blist_options);
	fprintf(f, "\tconvo_options { %u }\n", convo_options);
	fprintf(f, "\tim_options { %u }\n", im_options);
	fprintf(f, "\tchat_options { %u }\n", chat_options);
	fprintf(f, "\tfont_options { %u }\n", font_options);
	fprintf(f, "\tsound_options { %u }\n", sound_options);
	fprintf(f, "\taway_options { %u } { %u }\n", away_options, away_resend);
	fprintf(f, "\tfont_face { %s }\n", fontface);
	fprintf(f, "\tfont_size { %d }\n", fontsize);
	fprintf(f, "\tforeground { %d } { %d } { %d }\n", fgcolor.red, fgcolor.green, fgcolor.blue);
	fprintf(f, "\tbackground { %d } { %d } { %d }\n", bgcolor.red, bgcolor.green, bgcolor.blue);
	fprintf(f, "\treport_idle { %d }\n", report_idle);
	fprintf(f, "\tweb_browser { %d }\n", web_browser);
	fprintf(f, "\tweb_command { %s }\n", web_command);
	if (current_smiley_theme)
		fprintf(f, "\tsmiley_theme { %s }\n", current_smiley_theme->path);
	fprintf(f, "\tblist_pos { %d } { %d } { %d } { %d }\n",
		blist_pos.x, blist_pos.y, blist_pos.width, blist_pos.height);
	fprintf(f, "\tconv_size { %d } { %d } { %d }\n",
		conv_size.width, conv_size.height, conv_size.entry_height);
	fprintf(f, "\tbuddy_chat_size { %d } { %d } { %d }\n",
		buddy_chat_size.width, buddy_chat_size.height, buddy_chat_size.entry_height);

	fprintf(f, "}\n");
}


static void gaimrc_read_sounds(FILE *f)
{
	int i;
	char buf[2048];
	struct parse parse_buffer;
	struct parse *p;

	buf[0] = 0;

	for (i = 0; i < NUM_SOUNDS; i++)
		sound_file[i] = NULL;
#ifndef _WIN32
	sound_cmd[0] = 0;
#endif
	while (buf[0] != '}') {
		if (buf[0] == '#')
			continue;

		if (!fgets(buf, sizeof(buf), f))
			return;

		p = parse_line(buf, &parse_buffer);
#ifndef _WIN32
		if (!strcmp(p->option, "sound_cmd")) {
			g_snprintf(sound_cmd, sizeof(sound_cmd), "%s", p->value[0]);
		} else 
#endif
		if (!strncmp(p->option, "sound", strlen("sound"))) {
			i = p->option[strlen("sound")] - 'A';

			if (p->value[0][0])
				sound_file[i] = g_strdup(p->value[0]);
		}
	}
}

static void gaimrc_write_sounds(FILE *f)
{
	int i;
	fprintf(f, "sound_files {\n");
	for (i = 0; i < NUM_SOUNDS; i++)
		if (sound_file[i])
			fprintf(f, "\tsound%c { %s }\n", i + 'A', sound_file[i]);
		else
			fprintf(f, "\tsound%c {  }\n", i + 'A');
#ifndef _WIN32
	fprintf(f, "\tsound_cmd { %s }\n", sound_cmd);
#endif
	fprintf(f, "}\n");
}

static gboolean gaimrc_parse_proxy_uri(const char *proxy)
{
	char *c, *d;
	char buffer[2048];

	char host[128];
	char user[128];
	char pass[128];
	int  port = 0;
	int  len  = 0;

	host[0] = '\0';
	user[0] = '\0';
	pass[0] = '\0';

	debug_printf("gaimrc_parse_proxy_uri(%s)\n", proxy);

	if ((c = strchr(proxy, ':')) == NULL)
	{
	  debug_printf("no uri detected\n");
		/* No URI detected. */
		return FALSE;
	}

	len = c - proxy;

	if (!strncmp(proxy, "http://", len + 3))
		proxytype = PROXY_HTTP;
	else
		return FALSE;

	debug_printf("found http proxy\n");
	/* Get past "://" */
	c += 3;

	debug_printf("looking at %s\n", c);

	for (;;)
	{
		*buffer = '\0';
		d = buffer;

		while (*c != '\0' && *c != '@' && *c != ':' && *c != '/')
			*d++ = *c++;

		*d = '\0';

		if (*c == ':')
		{
			/*
			 * If there is a '@' in there somewhere, we are in the auth part.
			 * If not, host.
			 */
			if (strchr(c, '@') != NULL)
				strcpy(user, buffer);
			else
				strcpy(host, buffer);
		}
		else if (*c == '@')
		{
			if (user[0] == '\0')
				strcpy(user, buffer);
			else
				strcpy(pass, buffer);
		}
		else if (*c == '/' || *c == '\0')
		{
			if (host[0] == '\0')
				strcpy(host, buffer);
			else
				port = atoi(buffer);

			/* Done. */
			break;
		}

		c++;
	}

	/* NOTE: HTTP_PROXY takes precendence. */
	if (host[0])
		strcpy(proxyhost, host);
	else
		*proxyhost = '\0';

	if (user[0])
		strcpy(proxyuser, user);
	else
		*proxyuser = '\0';

	if (pass[0])
		strcpy(proxypass, pass);
	else
		*proxypass = '\0';
	
	proxyport = port;

	debug_printf("host '%s'\nuser '%s'\npassword '%s'\nport %d\n",
		proxyhost, proxyuser, proxypass, proxyport);

	return TRUE;
}

static void gaimrc_read_proxy(FILE *f)
{
	char buf[2048];
	struct parse parse_buffer;
	struct parse *p;

	buf[0] = 0;
	proxyhost[0] = 0;
	debug_printf("gaimrc_read_proxy\n");

	while (buf[0] != '}') {
		if (buf[0] == '#')
			continue;

		if (!fgets(buf, sizeof(buf), f))
			return;

		p = parse_line(buf, &parse_buffer);

		if (!strcmp(p->option, "host")) {
			g_snprintf(proxyhost, sizeof(proxyhost), "%s",
				   p->value[0]);
			debug_printf("set proxyhost %s\n", proxyhost);
		} else if (!strcmp(p->option, "port")) {
			proxyport = atoi(p->value[0]);
		} else if (!strcmp(p->option, "type")) {
			proxytype = atoi(p->value[0]);
		} else if (!strcmp(p->option, "user")) {
			g_snprintf(proxyuser, sizeof(proxyuser), "%s",
				   p->value[0]);
		} else if (!strcmp(p->option, "pass")) {
			g_snprintf(proxypass, sizeof(proxypass), "%s",
				   p->value[0]);
		}
	}
	if (proxyhost[0])
		proxy_info_is_from_gaimrc = 1;
	else if (!proxyhost[0]) {
	  
		gboolean getVars = TRUE;
		proxy_info_is_from_gaimrc = 0;
		
		if (g_getenv("HTTP_PROXY"))
			g_snprintf(proxyhost, sizeof(proxyhost), "%s",
				   g_getenv("HTTP_PROXY"));
		else if (g_getenv("http_proxy"))
			g_snprintf(proxyhost, sizeof(proxyhost), "%s",
				   g_getenv("http_proxy"));
		else if (g_getenv("HTTPPROXY"))
			g_snprintf(proxyhost, sizeof(proxyhost), "%s",
				   g_getenv("HTTPPROXY"));

		if (*proxyhost != '\0')
			getVars = !gaimrc_parse_proxy_uri(proxyhost);

		if (getVars)
		{
			if (g_getenv("HTTP_PROXY_PORT"))
				proxyport = atoi(g_getenv("HTTP_PROXY_PORT"));
			else if (g_getenv("http_proxy_port"))
				proxyport = atoi(g_getenv("http_proxy_port"));
			else if (g_getenv("HTTPPROXYPORT"))
				proxyport = atoi(g_getenv("HTTPPROXYPORT"));

			if (g_getenv("HTTP_PROXY_USER"))
				g_snprintf(proxyuser, sizeof(proxyuser), "%s",
					   g_getenv("HTTP_PROXY_USER"));
			else if (g_getenv("http_proxy_user"))
				g_snprintf(proxyuser, sizeof(proxyuser), "%s",
					   g_getenv("http_proxy_user"));
			else if (g_getenv("HTTPPROXYUSER"))
				g_snprintf(proxyuser, sizeof(proxyuser), "%s",
					   g_getenv("HTTPPROXYUSER"));

			if (g_getenv("HTTP_PROXY_PASS"))
				g_snprintf(proxypass, sizeof(proxypass), "%s",
					   g_getenv("HTTP_PROXY_PASS"));
			else if (g_getenv("http_proxy_pass"))
				g_snprintf(proxypass, sizeof(proxypass), "%s",
					   g_getenv("http_proxy_pass"));
			else if (g_getenv("HTTPPROXYPASS"))
				g_snprintf(proxypass, sizeof(proxypass), "%s",
					   g_getenv("HTTPPROXYPASS"));

			
			if (proxyhost[0])
				proxytype = PROXY_HTTP;
		}
	}
}

static void gaimrc_write_proxy(FILE *f)
{
	char *str;

	fprintf(f, "proxy {\n");
	if (proxy_info_is_from_gaimrc) {
		fprintf(f, "\thost { %s }\n", proxyhost);
		fprintf(f, "\tport { %d }\n", proxyport);
		fprintf(f, "\ttype { %d }\n", proxytype);
		fprintf(f, "\tuser { %s }\n", proxyuser);
		fprintf(f, "\tpass { %s }\n", (str = escape_text2(proxypass)));
		free(str);
	} else {
		fprintf(f, "\thost { %s }\n", "");
		fprintf(f, "\tport { %d }\n", 0);
		fprintf(f, "\ttype { %d }\n", 0);
		fprintf(f, "\tuser { %s }\n", "");
		fprintf(f, "\tpass { %s }\n", "");
	}
	fprintf(f, "}\n");
}

static void set_defaults()
{
	int i;
	struct away_message *a;

	misc_options =
		OPT_MISC_USE_SERVER_ALIAS;

	logging_options = 0;

	blist_options =
		OPT_BLIST_APP_BUDDY_SHOW |
		OPT_BLIST_SAVED_WINDOWS |
		OPT_BLIST_NEAR_APPLET |
		OPT_BLIST_SHOW_GRPNUM |
		OPT_BLIST_SHOW_PIXMAPS |
		OPT_BLIST_SHOW_IDLETIME |
		OPT_BLIST_GREY_IDLERS |
		OPT_BLIST_SHOW_BUTTON_XPM;

	convo_options =
		OPT_CONVO_ENTER_SENDS |
		OPT_CONVO_SEND_LINKS |
		OPT_CONVO_CTL_CHARS |
		OPT_CONVO_CTL_SMILEYS |
		OPT_CONVO_SHOW_TIME |
		OPT_CONVO_SHOW_SMILEY |
		OPT_CONVO_CHECK_SPELLING;

	im_options =
		OPT_IM_LOGON |
		OPT_IM_BUTTON_XPM |
		OPT_IM_ONE_WINDOW ;

	chat_options =
		OPT_CHAT_LOGON |
		OPT_CHAT_BUTTON_XPM |
		OPT_CHAT_TAB_COMPLETE |
		OPT_CHAT_ONE_WINDOW;

	font_options = 0;

	away_options =
		OPT_AWAY_BACK_ON_IM;

	for (i = 0; i < NUM_SOUNDS; i++)
		sound_file[i] = NULL;
	font_options = 0;
	/* Enable all of the sound players that might be available.  The first
	   available one will be used. */
	sound_options =
	    OPT_SOUND_LOGIN |
	    OPT_SOUND_LOGOUT |
	    OPT_SOUND_RECV |
	    OPT_SOUND_SEND |
	    OPT_SOUND_SILENT_SIGNON |
	    OPT_SOUND_NORMAL |
	    OPT_SOUND_NAS |
	    OPT_SOUND_ARTSC |
	    OPT_SOUND_ESD;

#ifdef USE_SCREENSAVER
	report_idle = IDLE_SCREENSAVER;
#else
	report_idle = IDLE_GAIM;
#endif
	web_browser = BROWSER_NETSCAPE;
	g_snprintf(web_command, sizeof(web_command), "xterm -e lynx %%s");

	auto_away = 10;
	a = g_new0(struct away_message, 1);
	g_snprintf(a->name, sizeof(a->name), "boring default");
	g_snprintf(a->message, sizeof(a->message), "%s", BORING_DEFAULT_AWAY_MSG);
	away_messages = g_slist_append(away_messages, a);
	default_away = a;

	blist_pos.width = 0;
	blist_pos.height = 0;
	blist_pos.x = 0;
	blist_pos.y = 0;

	conv_size.width = 320;
	conv_size.height = 175;
	conv_size.entry_height = 50;

	buddy_chat_size.width = 320;
	buddy_chat_size.height = 160;
	buddy_chat_size.entry_height = 50;
}

void load_prefs()
{
	FILE *f;
	char buf[1024];
	int ver = 0;
	debug_printf("load_prefs\n");
	if (is_saving_prefs) {
		request_load_prefs = 1;
		debug_printf("currently saving, will request load\n");
		return;
	}

	if (opt_rcfile_arg)
		g_snprintf(buf, sizeof(buf), "%s", opt_rcfile_arg);
	else if (gaim_home_dir())
		g_snprintf(buf, sizeof(buf), "%s" G_DIR_SEPARATOR_S ".gaimrc", gaim_home_dir());
	else {
		set_defaults();
		return;
	}

	if ((f = fopen(buf, "r"))) {
		is_loading_prefs = 1;
		debug_printf("start load_prefs\n");
		fgets(buf, sizeof(buf), f);
		sscanf(buf, "# .gaimrc v%d", &ver);
		if ((ver <= 3) || (buf[0] != '#'))
			set_defaults();

		while (!feof(f)) {
			int tag = gaimrc_parse_tag(f);
			debug_printf("starting read tag %d\n", tag);
			switch (tag) {
			case -1:
				/* Let the loop end, EOF */
				break;
			case 0:
				gaimrc_read_users(f);
				break;
			case 1:
				gaimrc_read_options(f);
				break;
			case 2:
				gaimrc_read_away(f);
				break;
#ifdef GAIM_PLUGINS
			case 3:
				gaimrc_read_plugins(f);
				break;
#endif
			case 4:
				gaimrc_read_pounce(f);
				break;
			case 6:
				gaimrc_read_sounds(f);
				break;
			case 7:
				gaimrc_read_proxy(f);
				break;
			default:
				/* NOOP */
				break;
			}
			debug_printf("ending read tag %d\n", tag);
		}
		fclose(f);
		is_loading_prefs = 0;
		debug_printf("end load_prefs\n");
		if (request_save_prefs) {
			debug_printf("saving prefs on request\n");
			save_prefs();
			request_save_prefs = 0;
		}
	} else if (opt_rcfile_arg) {
		g_snprintf(buf, sizeof(buf), _("Could not open config file %s."), opt_rcfile_arg);
		do_error_dialog(buf, NULL, GAIM_ERROR);
		set_defaults();
	} else {
		set_defaults();
		save_prefs();
	}
}

void save_prefs()
{
	FILE *f;
	char buf[BUF_LONG];

	debug_printf("enter save_prefs\n");  
	if (is_loading_prefs) {
		request_save_prefs = 1;
		debug_printf("currently loading, will request save\n");
		return;
	}

	if (opt_rcfile_arg) {
		g_snprintf(buf, sizeof(buf), "%s", opt_rcfile_arg);
	}
	else if (gaim_home_dir()) {
		g_snprintf(buf, sizeof(buf), "%s" G_DIR_SEPARATOR_S ".gaimrc", gaim_home_dir());
	}
	else {
		return;
	}
	if ((f = fopen(buf, "w"))) {
		is_saving_prefs = 1;
		fprintf(f, "# .gaimrc v%d\n", 4);
		gaimrc_write_users(f);
		gaimrc_write_options(f);
		gaimrc_write_sounds(f);
		gaimrc_write_away(f);
		gaimrc_write_pounce(f);
#ifdef GAIM_PLUGINS
		gaimrc_write_plugins(f);
#endif
		gaimrc_write_proxy(f);
		fclose(f);

		chmod(buf, S_IRUSR | S_IWUSR);
		is_saving_prefs = 0;
	}
	else
	  debug_printf("Error opening .gaimrc\n");
	if (request_load_prefs) {
		debug_printf("loading prefs on request\n");
		load_prefs();
		request_load_prefs = 0;
	}
	debug_printf("exit save_prefs\n");
}



/* This function is called by g_slist_insert_sorted to compare the item
 * being compared to the rest of the items on the list.
 */

gint sort_awaymsg_list(gconstpointer a, gconstpointer b)
{
	struct away_message *msg_a;
	struct away_message *msg_b;

	msg_a = (struct away_message *)a;
	msg_b = (struct away_message *)b;

	return (strcmp(msg_a->name, msg_b->name));

}
