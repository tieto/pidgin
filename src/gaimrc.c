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
#include "../config.h"
#endif
#include <string.h>
#include <sys/time.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include "gaim.h"
#include "prpl.h"
#include "proxy.h"

/* for people like myself, who are too lazy to add an away msg :) */
#define BORING_DEFAULT_AWAY_MSG "sorry, i ran out for a while. bbl"
#define MAX_VALUES 10

GList *aim_users = NULL;
int general_options;
int display_options;
int sound_options;
int font_options;
int logging_options;

int report_idle, web_browser;
struct save_pos blist_pos;
struct window_size conv_size, buddy_chat_size;
char web_command[2048];
char *sound_file[NUM_SOUNDS];
char sound_cmd[2048];

struct parse {
	char option[256];
	char value[MAX_VALUES][256];
};

static struct parse *parse_line(char *line)
{
	char *c = line;
	int inopt = 1, inval = 0, curval = -1;
	int optlen = 0, vallen = 0;
	static struct parse p;
	int x;

	for (x = 0; x < MAX_VALUES; x++) {
		p.value[x][0] = 0;
	}


	while (*c) {
		if (*c == '\t') {
			c++;
			continue;
		}
		if (inopt) {
			//   if ((*c < 'a' || *c > 'z') && *c != '_') {
			if ((*c < 'a' || *c > 'z') && *c != '_' && (*c < 'A' || *c > 'Z')) {
				inopt = 0;
				p.option[optlen] = 0;
				c++;
				continue;
			}

			p.option[optlen] = *c;
			optlen++;
			c++;
			continue;
		} else if (inval) {
			if ((*c == '}')) {
				if (*(c - 1) == '\\') {
					p.value[curval][vallen - 1] = *c;
					c++;
					continue;
				} else {
					p.value[curval][vallen - 1] = 0;
					inval = 0;
					c++;
					continue;
				}
			} else {
				p.value[curval][vallen] = *c;
				vallen++;
				c++;
				continue;
			}
		} else if (*c == '{') {
			if (*(c - 1) == '\\') {
				p.value[curval][vallen - 1] = *c;
				c++;
				continue;
			} else {
				curval++;
				vallen = 0;
				inval = 1;
				c++;
				c++;
				continue;
			}
		}
		c++;
	}

	return &p;
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
	} else if (!strcmp(tag, "chat")) {
		return 5;
	} else if (!strcmp(tag, "sound_files")) {
		return 6;
	} else if (!strcmp(tag, "proxy")) {
		return 7;
	}

	return -1;
}

void filter_break(char *msg)
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


static void gaimrc_read_away(FILE *f)
{
	struct parse *p;
	char buf[4096];
	struct away_message *a;

	buf[0] = 0;

	while (buf[0] != '}') {
		if (!fgets(buf, sizeof(buf), f))
			return;

		if (buf[0] == '}')
			return;

		p = parse_line(buf);
		if (!strcmp(p->option, "message")) {
			a = g_new0(struct away_message, 1);

			g_snprintf(a->name, sizeof(a->name), "%s", p->value[0]);
			g_snprintf(a->message, sizeof(a->message), "%s", p->value[1]);
			filter_break(a->message);
			away_messages = g_slist_insert_sorted(away_messages, a, sort_awaymsg_list);
		}
		/* auto { time } { default message } */
		else if (!strcmp(p->option, "auto")) {
			auto_away = atoi(p->value[0]);
			default_away = g_slist_nth_data(away_messages,
							atoi(p->value[1]));
		}
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
	struct parse *p;
	char buf[4096];
	struct buddy_pounce *b;

	buf[0] = 0;

	while (buf[0] != '}') {
		if (!fgets(buf, sizeof(buf), f))
			return;

		if (buf[0] == '}')
			return;

		p = parse_line(buf);
		if (!strcmp(p->option, "entry")) {
			b = g_new0(struct buddy_pounce, 1);

			g_snprintf(b->name, sizeof(b->name), "%s", p->value[0]);
			g_snprintf(b->message, sizeof(b->message), "%s", p->value[1]);
			g_snprintf(b->command, sizeof(b->command), "%s", p->value[2]);

			b->options = atoi(p->value[3]);

			g_snprintf(b->pouncer, sizeof(b->pouncer), "%s", p->value[4]);
			b->protocol = atoi(p->value[5]);

			g_snprintf(b->sound, sizeof(b->sound), "%s", p->value[6]);
			
			filter_break(b->message);
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

static void gaimrc_read_chat(FILE *f)
{
	struct parse *p;
	char buf[4096];
	struct chat_room *b;

	buf[0] = 0;

	while (buf[0] != '}') {
		if (!fgets(buf, sizeof(buf), f))
			return;

		if (buf[0] == '}')
			return;

		p = parse_line(buf);
		if (!strcmp(p->option, "entry")) {
			b = g_new0(struct chat_room, 1);

			g_snprintf(b->name, sizeof(b->name), "%s", p->value[0]);

			b->exchange = atoi(p->value[1]);

			chat_rooms = g_list_append(chat_rooms, b);
		}
	}
}

static void gaimrc_write_chat(FILE *f)
{
	GList *pnc = chat_rooms;
	struct chat_room *b;

	fprintf(f, "chat {\n");

	if (pnc) {
		while (pnc) {
			char *str1;

			b = (struct chat_room *)pnc->data;

			str1 = escape_text2(b->name);

			fprintf(f, "\tentry { %s } { %d }\n", str1, b->exchange);

			/* escape_text2 uses malloc(), so we don't want to g_free these */
			free(str1);

			pnc = pnc->next;
		}
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

		path = escape_text2(g_module_name(p->handle));

		fprintf(f, "\tplugin { %s }\n", path);

		free(path);

		pl = pl->next;
	}

	fprintf(f, "}\n");
}

static void gaimrc_read_plugins(FILE *f)
{
	struct parse *p;
	char buf[4096];
	GSList *load = NULL;

	buf[0] = 0;

	while (buf[0] != '}') {
		if (!fgets(buf, sizeof(buf), f))
			break;

		if (buf[0] == '}')
			break;

		p = parse_line(buf);
		if (!strcmp(p->option, "plugin")) {
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
	struct parse *p;
	struct aim_user *u;
	int i;
	char buf[4096];

	if (!fgets(buf, sizeof(buf), f))
		return NULL;

	p = parse_line(buf);

	if (strcmp(p->option, "ident"))
		return NULL;

	u = g_new0(struct aim_user, 1);

	strcpy(u->username, p->value[0]);
	strcpy(u->password, p->value[1]);

	u->user_info[0] = 0;
	u->options = OPT_USR_REM_PASS;
	u->protocol = DEFAULT_PROTO;

	if (!fgets(buf, sizeof(buf), f))
		return u;

	if (strcmp(buf, "\t\tuser_info {\n")) {
		return u;
	}

	if (!fgets(buf, sizeof(buf), f))
		return u;

	while (strncmp(buf, "\t\t}", 3)) {
		if (strlen(buf) > 3)
			strcat(u->user_info, &buf[3]);

		if (!fgets(buf, sizeof(buf), f)) {
			return u;
		}
	}

	if ((i = strlen(u->user_info))) {
		u->user_info[i-1] = '\0';
	}

	if (!fgets(buf, sizeof(buf), f)) {
		return u;
	}

	if (!strcmp(buf, "\t}")) {
		return u;
	}

	p = parse_line(buf);

	if (strcmp(p->option, "user_opts"))
		return u;

	u->options = atoi(p->value[0]);
	u->protocol = atoi(p->value[1]);

	if (!fgets(buf, sizeof(buf), f))
		return u;

	if (!strcmp(buf, "\t}"))
		return u;

	p = parse_line(buf);

	if (strcmp(p->option, "proto_opts"))
		return u;

	for (i = 0; i < 7; i++)
		g_snprintf(u->proto_opt[i], sizeof u->proto_opt[i], "%s", p->value[i]);

	return u;

}

static void gaimrc_write_user(FILE *f, struct aim_user *u)
{
	char *c;
	int nl = 1, i;
	if (u->options & OPT_USR_REM_PASS)
		fprintf(f, "\t\tident { %s } { %s }\n", u->username, u->password);
	else
		fprintf(f, "\t\tident { %s } {  }\n", u->username);
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
}


static void gaimrc_read_users(FILE *f)
{
	char buf[2048];
	struct aim_user *u;
	struct parse *p;

	buf[0] = 0;

	while (buf[0] != '}') {
		if (buf[0] == '#')
			continue;

		if (!fgets(buf, sizeof(buf), f))
			return;



		p = parse_line(buf);

		if (!strcmp(p->option, "current_user")) {
		} else if (strcmp(p->option, "user")) {
			continue;
		} else {
		}

		u = gaimrc_read_user(f);

		aim_users = g_list_append(aim_users, u);
	}
}

static void gaimrc_write_users(FILE *f)
{
	GList *usr = aim_users;
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




static void gaimrc_read_options(FILE *f)
{
	char buf[2048];
	struct parse *p;
	gboolean read_logging = FALSE;

	buf[0] = 0;

	while (buf[0] != '}') {
		if (buf[0] == '#')
			continue;

		if (!fgets(buf, sizeof(buf), f))
			return;

		p = parse_line(buf);

		if (!strcmp(p->option, "general_options")) {
			general_options = atoi(p->value[0]);
		} else if (!strcmp(p->option, "display_options")) {
			display_options = atoi(p->value[0]);
		} else if (!strcmp(p->option, "sound_options")) {
			sound_options = atoi(p->value[0]);
		} else if (!strcmp(p->option, "font_options")) {
			font_options = atoi(p->value[0]);
		} else if (!strcmp(p->option, "logging_options")) {
			logging_options = atoi(p->value[0]);
			read_logging = TRUE;
		} else if (!strcmp(p->option, "font_face")) {
			if (p->value[0] != NULL)
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
			blist_pos.xoff = atoi(p->value[4]);
			blist_pos.yoff = atoi(p->value[5]);
		}

	}

	if (!read_logging) {
		logging_options = 0;
		if (general_options & OPT_GEN_LOG_ALL)
			logging_options |= OPT_LOG_ALL;
		if (general_options & OPT_GEN_STRIP_HTML)
			logging_options |= OPT_LOG_STRIP_HTML;
	}

}

static void gaimrc_write_options(FILE *f)
{

	fprintf(f, "options {\n");
	fprintf(f, "\tgeneral_options { %d }\n", general_options);
	fprintf(f, "\tdisplay_options { %d }\n", display_options);
	fprintf(f, "\tsound_options { %d }\n", sound_options);
	fprintf(f, "\tfont_options { %d }\n", font_options);
	fprintf(f, "\tlogging_options { %d }\n", logging_options);
	if (fontface)
		fprintf(f, "\tfont_face { %s }\n", fontface);
	fprintf(f, "\tfont_size { %d }\n", fontsize);
	fprintf(f, "\tforeground { %d } { %d } { %d }\n", fgcolor.red, fgcolor.green, fgcolor.blue);
	fprintf(f, "\tbackground { %d } { %d } { %d }\n", bgcolor.red, bgcolor.green, bgcolor.blue);
	fprintf(f, "\treport_idle { %d }\n", report_idle);
	fprintf(f, "\tweb_browser { %d }\n", web_browser);
	fprintf(f, "\tweb_command { %s }\n", web_command);
	fprintf(f, "\tblist_pos { %d } { %d } { %d } { %d } { %d } { %d }\n",
		blist_pos.x, blist_pos.y, blist_pos.width, blist_pos.height,
		blist_pos.xoff, blist_pos.yoff);
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
	struct parse *p;

	buf[0] = 0;

	for (i = 0; i < NUM_SOUNDS; i++)
		sound_file[i] = NULL;
	sound_cmd[0] = 0;

	while (buf[0] != '}') {
		if (buf[0] == '#')
			continue;

		if (!fgets(buf, sizeof(buf), f))
			return;

		p = parse_line(buf);

		if (!strcmp(p->option, "sound_cmd")) {
			g_snprintf(sound_cmd, sizeof(sound_cmd), "%s", p->value[0]);
		} else if (!strncmp(p->option, "sound", strlen("sound"))) {
			sscanf(p->option, "sound%c", (char *)&i);
			i -= 'A';

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
	fprintf(f, "\tsound_cmd { %s }\n", sound_cmd);
	fprintf(f, "}\n");
}

static void gaimrc_read_proxy(FILE *f)
{
	char buf[2048];
	struct parse *p;

	buf[0] = 0;

	while (buf[0] != '}') {
		if (buf[0] == '#')
			continue;

		if (!fgets(buf, sizeof(buf), f))
			return;

		p = parse_line(buf);

		if (!strcmp(p->option, "host")) {
			g_snprintf(proxyhost, sizeof(proxyhost), "%s", p->value[0]);
		} else if (!strcmp(p->option, "port")) {
			proxyport = atoi(p->value[0]);
		} else if (!strcmp(p->option, "type")) {
			proxytype = atoi(p->value[0]);
		} else if (!strcmp(p->option, "user")) {
			g_snprintf(proxyuser, sizeof(proxyuser), "%s", p->value[0]);
		} else if (!strcmp(p->option, "pass")) {
			g_snprintf(proxypass, sizeof(proxypass), "%s", p->value[0]);
		}
	}
}

static void gaimrc_write_proxy(FILE *f)
{
	fprintf(f, "proxy {\n");
	fprintf(f, "\thost { %s }\n", proxyhost);
	fprintf(f, "\tport { %d }\n", proxyport);
	fprintf(f, "\ttype { %d }\n", proxytype);
	fprintf(f, "\tuser { %s }\n", proxyuser);
	fprintf(f, "\tpass { %s }\n", proxypass);
	fprintf(f, "}\n");
}


void set_defaults(int saveinfo)
{
	if (!saveinfo) {
		if (aim_users) {
			g_list_free(aim_users);
			aim_users = NULL;
		}
		if (away_messages) {
			g_slist_free(away_messages);
			away_messages = NULL;
		}
	}

	general_options =
	    OPT_GEN_SEND_LINKS |
	    OPT_GEN_ENTER_SENDS |
	    OPT_GEN_SAVED_WINDOWS |
	    /* OPT_GEN_REMEMBER_PASS | */
	    OPT_GEN_REGISTERED |
	    OPT_GEN_NEAR_APPLET |
	    OPT_GEN_CTL_SMILEYS |
	    OPT_GEN_CTL_CHARS;

	display_options =
	    OPT_DISP_SHOW_IDLETIME |
	    OPT_DISP_SHOW_TIME |
	    OPT_DISP_SHOW_PIXMAPS |
	    OPT_DISP_SHOW_BUDDYTICKER |
	    OPT_DISP_SHOW_BUTTON_XPM |
	    OPT_DISP_SHOW_SMILEY |
	    OPT_DISP_COOL_LOOK |
	    OPT_DISP_CONV_BUTTON_XPM |
	    OPT_DISP_CHAT_BUTTON_TEXT;

	if (!saveinfo) {
		int i;
		for (i = 0; i < 7; i++)
			sound_file[i] = NULL;
		font_options = 0;
		sound_options =
		    OPT_SOUND_LOGIN |
		    OPT_SOUND_LOGOUT |
		    OPT_SOUND_RECV |
		    OPT_SOUND_SEND |
		    OPT_SOUND_SILENT_SIGNON;
		report_idle = IDLE_SCREENSAVER;
		web_browser = BROWSER_NETSCAPE;
		auto_away = 10;
		default_away = NULL;

		g_snprintf(web_command, sizeof(web_command), "xterm -e lynx %%s");

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
}


void load_prefs()
{
	FILE *f;
	char buf[1024];
	int ver = 0;

	if (opt_rcfile_arg)
		g_snprintf(buf, sizeof(buf), "%s", opt_rcfile_arg);
	else if (getenv("HOME"))
		g_snprintf(buf, sizeof(buf), "%s/.gaimrc", getenv("HOME"));
	else {
		set_defaults(TRUE);
		return;
	}

	if ((f = fopen(buf, "r"))) {
		fgets(buf, sizeof(buf), f);
		sscanf(buf, "# .gaimrc v%d", &ver);
		if ((ver <= 1) || (buf[0] != '#')) {
			fclose(f);
			set_defaults(FALSE);
			save_prefs();
			load_prefs();
			return;
		}

		while (!feof(f)) {
			switch (gaimrc_parse_tag(f)) {
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
			case 5:
				gaimrc_read_chat(f);
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
		}
		fclose(f);
	} else if (opt_rcfile_arg) {
		g_snprintf(buf, sizeof(buf), _("Could not open config file %s."), opt_rcfile_arg);
		do_error_dialog(buf, _("Preferences Error"));
	}

	if ((ver == 2) || (buf[0] != '#')) {
		set_defaults(TRUE);
	}
}

void save_prefs()
{
	FILE *f;
	char buf[BUF_LONG];

	if (opt_rcfile_arg)
		g_snprintf(buf, sizeof(buf), "%s", opt_rcfile_arg);
	else if (getenv("HOME"))
		g_snprintf(buf, sizeof(buf), "%s/.gaimrc", getenv("HOME"));
	else
		return;

	if ((f = fopen(buf, "w"))) {
		fprintf(f, "# .gaimrc v%d\n", 4);
		gaimrc_write_users(f);
		gaimrc_write_options(f);
		gaimrc_write_sounds(f);
		gaimrc_write_away(f);
		gaimrc_write_pounce(f);
		gaimrc_write_chat(f);
#ifdef GAIM_PLUGINS
		gaimrc_write_plugins(f);
#endif
		gaimrc_write_proxy(f);
		fclose(f);
		chmod(buf, S_IRUSR | S_IWUSR);
	}
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
