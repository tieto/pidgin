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

/* for people like myself, who are too lazy to add an away msg :) */
#define BORING_DEFAULT_AWAY_MSG "sorry, i ran out for a while. bbl"
#define MAX_VALUES 10

GList *aim_users = NULL;
int general_options;
int display_options;
int sound_options;
int font_options;

int report_idle, web_browser;
struct save_pos blist_pos;
char web_command[2048];
char latest_ver[25];
char *sound_file[NUM_SOUNDS];

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
	
	
        while(*c) {
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
                if ( (*c == '}') ) {
                                if (*(c-1) == '\\') {
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
                        continue; }
                } else if (*c == '{') {
			if (*(c-1) == '\\') {
				p.value[curval][vallen-1] = *c;
				c++;
				continue;
			}
			else
			{
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
	}

	return -1;
}

void filter_break(char *msg)
{
        char *c;
        int mc;
	int cc;
	
        c = g_malloc(strlen(msg)+1);
	strcpy(c, msg);

        mc = 0;
	cc = 0;
        while (c[cc] != '\0')
        {
                if (c[cc] == '\\') {
                        cc++;
                        msg[mc] = c[cc]; 
                }       
                else {
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
	
	while (buf[0] != '}')
	{
		if (!fgets(buf, sizeof(buf), f))
			return;
		
		if (buf[0] == '}')
			return;

		p = parse_line(buf);
		if (!strcmp(p->option, "message"))
		{
			a = g_new0(struct away_message, 1);

			g_snprintf(a->name, sizeof(a->name),  "%s", p->value[0]);
			g_snprintf(a->message, sizeof(a->message), "%s", p->value[1]);
			filter_break(a->message);
			away_messages = g_slist_insert_sorted(away_messages, a, sort_awaymsg_list);
		}
		/* auto { time } { default message } */
		else if (!strcmp(p->option, "auto"))
		{
			auto_away = atoi(p->value[0]);
			default_away = atoi(p->value[1]);
		}
	}
}

static void gaimrc_write_away(FILE *f)
{
	GSList *awy = away_messages;
	struct away_message *a;

	fprintf(f, "away {\n");

	if (awy)
	{
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
		fprintf(f, "\tauto { %d } { %d }\n", auto_away, default_away);
	}
	else
	{
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
	
	while (buf[0] != '}')
	{
		if (!fgets(buf, sizeof(buf), f))
			return;
		
		if (buf[0] == '}')
			return;

		p = parse_line(buf);
		if (!strcmp(p->option, "entry"))
		{
			b = g_new0(struct buddy_pounce, 1);

			g_snprintf(b->name, sizeof(b->name),  "%s", p->value[0]);
			g_snprintf(b->message, sizeof(b->message), "%s", p->value[1]);

			b->popup = atoi(p->value[2]);
			b->sendim = atoi(p->value[3]);

			/* Let's check our version and see what's going on here */
			if ((p->value[4]) && (strlen(p->value[4]) > 0))
			{
				/* If we have data, lets use it */
				g_snprintf(b->pouncer, sizeof(b->pouncer), "%s", p->value[4]);
				b->protocol = atoi(p->value[5]);
				b->signon = atoi(p->value[6]);		
				b->unaway = atoi(p->value[7]);		
				b->unidle = atoi(p->value[8]);		
			}
			else
			{
				/* Otherwise, we have old info.  Let's adjust */
				b->signon = 1;
				b->unaway = 0;
				b->unidle = 0;
			}

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

	if (pnc)
	{
		while (pnc) {
			char *str1, *str2;

			b = (struct buddy_pounce *)pnc->data;

			str1 = escape_text2(b->name);
			if (strlen(b->message))
				str2 = escape_text2(b->message);
			else {
				str2 = malloc(1);
				str2[0] = 0;
			}

			fprintf(f, "\tentry { %s } { %s } { %d } { %d } { %s } { %d } { %d } { %d } { %d }\n", str1, str2, b->popup, b->sendim, b->pouncer, b->protocol, b->signon, b->unaway, b->unidle);

			/* escape_text2 uses malloc(), so we don't want to g_free these */
			free(str1);
			free(str2);
	
			pnc = pnc->next;
		}
	}

	fprintf(f, "}\n");
}

static void gaimrc_read_chat(FILE *f)
{
	struct parse *p;
	char buf[4096];
	struct chat_room *b;

	buf[0] = 0;
	
	while (buf[0] != '}')
	{
		if (!fgets(buf, sizeof(buf), f))
			return;
		
		if (buf[0] == '}')
			return;

		p = parse_line(buf);
		if (!strcmp(p->option, "entry"))
		{
			b = g_new0(struct chat_room, 1);

			g_snprintf(b->name, sizeof(b->name),  "%s", p->value[0]);

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

	if (pnc)
	{
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

	buf[0] = 0;
	
	while (buf[0] != '}')
	{
		if (!fgets(buf, sizeof(buf), f))
			return;
		
		if (buf[0] == '}')
			return;

		p = parse_line(buf);
		if (!strcmp(p->option, "plugin"))
		{
			load_plugin(p->value[0]);
		}
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
	u->protocol = 0; /* PROTO_TOC */

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

	for (i = 0; i < 6; i++)
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
        while(*c) {
                /* This is not as silly as it looks. */
                if (*c == '\n') {
                        nl++;
                } else {
                        if (nl) {
                                while(nl) {
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
	for (i = 0; i < 6; i++)
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
	
	while(usr) {
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
		} else if (!strcmp(p->option, "font_face")) {
			if (p->value[0] != NULL)
					g_snprintf(fontface, sizeof(fontface), "%s", p->value[0]);
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
                } else if (!strcmp(p->option, "blist_pos")) {
                        blist_pos.x = atoi(p->value[0]);
                        blist_pos.y = atoi(p->value[1]);
                        blist_pos.width = atoi(p->value[2]);
                        blist_pos.height = atoi(p->value[3]);
                        blist_pos.xoff = atoi(p->value[4]);
                        blist_pos.yoff = atoi(p->value[5]);
		}

        }

}

static void gaimrc_write_options(FILE *f)
{

	fprintf(f, "options {\n");
        fprintf(f, "\tgeneral_options { %d }\n", general_options);
        fprintf(f, "\tdisplay_options { %d }\n", display_options);
        fprintf(f, "\tsound_options { %d }\n", sound_options);
	fprintf(f, "\tfont_options { %d }\n", font_options);
	if (fontface)
		fprintf(f, "\tfont_face { %s }\n", fontface);
	fprintf(f, "\tforeground { %d } { %d } { %d }\n", fgcolor.red, fgcolor.green, fgcolor.blue);
	fprintf(f, "\tbackground { %d } { %d } { %d }\n", bgcolor.red, bgcolor.green, bgcolor.blue);
        fprintf(f, "\treport_idle { %d }\n", report_idle);
        fprintf(f, "\tweb_browser { %d }\n", web_browser);
        fprintf(f, "\tweb_command { %s }\n", web_command);
        fprintf(f, "\tblist_pos { %d } { %d } { %d } { %d } { %d } { %d }\n",
                blist_pos.x, blist_pos.y, blist_pos.width, blist_pos.height,
                blist_pos.xoff, blist_pos.yoff);
	fprintf(f, "\tlatest_ver { %s }\n", latest_ver);
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
        
	while (buf[0] != '}') {
		if (buf[0] == '#')
			continue;
		
		if (!fgets(buf, sizeof(buf), f))
			return;

                p = parse_line(buf);
                
		sscanf(p->option, "sound%c", (char *)&i);
		i -= 'A';
		
		if (p->value[0][0])
			sound_file[i] = g_strdup(p->value[0]);
/*		else
			sound_file[i] = NULL;
		Removed by Rob.  Kill me if this is retarded.
		Tis was causing segfaults on PPC machines, though.
*/
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
	fprintf(f, "}\n");
}


void set_defaults(int saveinfo)
{
	if (!saveinfo)
	{	
        	if (aim_users)
        	{
        		g_list_free(aim_users);
        		aim_users = NULL;
       	 	}
       	 	if (away_messages)
       	 	{
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
		OPT_DISP_CONV_BIG_ENTRY |
		OPT_DISP_CHAT_BIG_ENTRY |
		OPT_DISP_CONV_BUTTON_XPM | 
		OPT_DISP_CONV_BUTTON_TEXT |
		OPT_DISP_CHAT_BUTTON_XPM | 
		OPT_DISP_CHAT_BUTTON_TEXT;

	if (!saveinfo)
	{
		int i;
		for (i = 0; i < 7; i++)
			sound_file[i] = NULL;
		font_options = 0; 
        	sound_options = OPT_SOUND_LOGIN | OPT_SOUND_LOGOUT | OPT_SOUND_RECV | OPT_SOUND_SEND | OPT_SOUND_SILENT_SIGNON;
        	report_idle = IDLE_SCREENSAVER;
        	web_browser = BROWSER_NETSCAPE;
		auto_away = 10;
		default_away = 0;
        
        	g_snprintf(web_command, sizeof(web_command), "xterm -e lynx %%s");
        	blist_pos.width = 0;
        	blist_pos.height = 0;
        	blist_pos.x = 0;
        	blist_pos.y = 0;
        	blist_pos.xoff = 0;
        	blist_pos.yoff = 0;
		g_snprintf(latest_ver, BUF_LONG, "%s", VERSION);
	}
}


void load_prefs()
{
	FILE *f;
	char buf[1024];
	int ver = 0;

        if (getenv("HOME")) {
                g_snprintf(buf, sizeof(buf), "%s/.gaimrc", getenv("HOME"));
		if ((f = fopen(buf,"r"))) {
			fgets(buf, sizeof(buf), f);
			sscanf(buf, "# .gaimrc v%d", &ver);
			if ( (ver <= 1) || (buf[0] != '#')) {
                                fclose(f);
				set_defaults(FALSE);
				save_prefs();
				load_prefs();
                                return;
			}
			
			while(!feof(f)) {
				switch(gaimrc_parse_tag(f)) {
				case -1:
					/* Let the loop end, EOF*/
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
				default:
					/* NOOP */
					break;
				}
			}
			fclose(f);
		}
	}

	if ( (ver == 2) || (buf[0] != '#')) {
		set_defaults(TRUE);
	}
}

void save_prefs()
{
	FILE *f;
	char buf[BUF_LONG];

	if (getenv("HOME")) {
		g_snprintf(buf, sizeof(buf), "%s/.gaimrc", getenv("HOME"));
		if ((f = fopen(buf,"w"))) {
			fprintf(f, "# .gaimrc v%d\n", 4);
			gaimrc_write_users(f);
			gaimrc_write_options(f);
			gaimrc_write_sounds(f);
			gaimrc_write_away(f);
#ifdef GAIM_PLUGINS
			gaimrc_write_plugins(f);
#endif
			gaimrc_write_pounce(f);
			gaimrc_write_chat(f);
			fclose(f);
			chmod(buf, S_IRUSR | S_IWUSR);
		}
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
