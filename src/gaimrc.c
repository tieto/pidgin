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
#include "sound.h"
#include "pounce.h"
#include "gtkpounce.h"

#ifdef _WIN32
#include "win32dep.h"
#endif

/* for people like myself, who are too lazy to add an away msg :) */
#define BORING_DEFAULT_AWAY_MSG _("sorry, i ran out for a while. bbl")
#define MAX_VALUES 10

GSList *gaim_accounts = NULL;
guint misc_options;
guint logging_options;
guint blist_options;
guint convo_options;
guint im_options;
guint conv_placement_option;
guint chat_options;
guint font_options;
guint sound_options;
guint away_options;
guint away_resend;
static guint is_loading_prefs = 0;
static guint request_save_prefs = 0;
static guint is_saving_prefs = 0;
static guint request_load_prefs = 0;
static guint prefs_initial_load = 0;
guint proxy_info_is_from_gaimrc = 1; /* Only save proxy info if it
				      * was loaded from the file
				      * or otherwise explicitly requested */

int report_idle;
int web_browser;
struct save_pos blist_pos;
struct window_size conv_size, buddy_chat_size;
char web_command[2048] = "";

struct parse {
	char option[256];
	char value[MAX_VALUES][4096];
};

/*
 * This is absolutely necessary, unfortunately. It is used to grab
 * the information on the pounce, so that we can then later register
 * them. The reason we do this (well, one of them) is because the buddy
 * list isn't processed yet.
 *
 *  -- ChipX86
 */
struct pounce_placeholder
{
	char name[80];
	char message[2048];
	char command[2048];
	char sound[2048];
	char pouncer[80];

	int protocol;
	int options;
};

static GList *buddy_pounces = NULL;

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
				if (isspace(*c))
					last_non_space = vallen - 1;
				else
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
		g_snprintf(a->name, sizeof(a->name), _("boring default"));
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
		fprintf(f, "\tmessage { %s } { %s }\n", _("boring default"), BORING_DEFAULT_AWAY_MSG);
		fprintf(f, "\tauto { 0 } { 0 }\n");
	}

	fprintf(f, "}\n");
}

/*
 * This is temporary, and we're using it to translate the new event
 * and action values into the old ones. We're also adding entries for
 * new types, but if you go and use an older gaim, these will be nuked.
 * When we have a better prefs system, this can go away.
 *
 *   -- ChipX86
 */
static int pounce_evt_trans_table[] =
{
	0x010, GAIM_POUNCE_SIGNON,
	0x020, GAIM_POUNCE_AWAY_RETURN,
	0x040, GAIM_POUNCE_IDLE_RETURN,
	0x080, GAIM_POUNCE_TYPING,
	/* 0x100, save, is handled separately. */
	0x400, GAIM_POUNCE_SIGNOFF,
	0x800, GAIM_POUNCE_AWAY,
	0x1000, GAIM_POUNCE_IDLE,
	0x2000, GAIM_POUNCE_TYPING_STOPPED
};

static int pounce_act_trans_table[] =
{
	0x001, GAIM_GTKPOUNCE_OPEN_WIN,
	0x002, GAIM_GTKPOUNCE_SEND_MSG,
	0x004, GAIM_GTKPOUNCE_EXEC_CMD,
	0x008, GAIM_GTKPOUNCE_PLAY_SOUND,
	/* 0x100, save, is handled separately. */
	0x200, GAIM_GTKPOUNCE_POPUP
};

static int pounce_evt_trans_table_size =
	(sizeof(pounce_evt_trans_table) / sizeof(*pounce_evt_trans_table));

static int pounce_act_trans_table_size =
	(sizeof(pounce_act_trans_table) / sizeof(*pounce_act_trans_table));

static int
new_pounce_opts_to_old(struct gaim_pounce *pounce)
{
	struct gaim_gtkpounce_data *gtkpounce;

	int opts = 0;
	int i;

	gtkpounce = GAIM_GTKPOUNCE(pounce);

	/* First, convert events */
	for (i = 0; i < pounce_evt_trans_table_size; i += 2)
	{
		GaimPounceEvent evt = pounce_evt_trans_table[i + 1];

		if ((gaim_pounce_get_events(pounce) & evt) == evt)
			opts |= pounce_evt_trans_table[i];
	}

	for (i = 0; i < pounce_act_trans_table_size; i += 2)
	{
		GaimGtkPounceAction act = pounce_act_trans_table[i + 1];

		if ((gtkpounce->actions & act) == act)
			opts |= pounce_act_trans_table[i];
	}

	if (gtkpounce->save)
		opts |= 0x100;

	return opts;
}

static void
old_pounce_opts_to_new(int opts, GaimPounceEvent *events,
					   GaimGtkPounceAction *actions)
{
	int i;

	*events = 0;
	*actions = 0;

	/* First, convert events */
	for (i = 0; i < pounce_evt_trans_table_size; i += 2)
	{
		int evt = pounce_evt_trans_table[i];

		if ((opts & evt) == evt)
			*events |= pounce_evt_trans_table[i + 1];
	}

	for (i = 0; i < pounce_act_trans_table_size; i += 2)
	{
		int act = pounce_act_trans_table[i];

		if ((opts & act) == act)
			*actions |= pounce_act_trans_table[i + 1];
	
	}
}

static void
gaimrc_read_pounce(FILE *f)
{
	struct parse parse_buffer;
	struct parse *p;
	char buf[4096];
	struct pounce_placeholder *b;

	buf[0] = 0;

	while (buf[0] != '}') {
		if (!fgets(buf, sizeof(buf), f))
			return;

		if (buf[0] == '}')
			return;

		p = parse_line(buf, &parse_buffer);
		if (!strcmp(p->option, "entry")) {
			b = g_new0(struct pounce_placeholder, 1);

			g_snprintf(b->name, sizeof(b->name), "%s", p->value[0]);
			g_snprintf(b->message, sizeof(b->message), "%s", p->value[1]);
			g_snprintf(b->command, sizeof(b->command), "%s", p->value[2]);

			b->options = atoi(p->value[3]);

			g_snprintf(b->pouncer, sizeof(b->pouncer), "%s", p->value[4]);
			b->protocol = atoi(p->value[5]);

			g_snprintf(b->sound, sizeof(b->sound), "%s", p->value[6]);

			buddy_pounces = g_list_append(buddy_pounces, b);
		}
	}
}

static void
gaimrc_write_pounce(FILE *f)
{
	GList *pnc;
	struct gaim_pounce *pounce;
	struct gaim_gtkpounce_data *pounce_data;

	fprintf(f, "pounce {\n");

	for (pnc = gaim_get_pounces(); pnc != NULL; pnc = pnc->next) {
		char *str1, *str2, *str3, *str4;
		struct gaim_account *account;

		pounce      = (struct gaim_pounce *)pnc->data;
		pounce_data = GAIM_GTKPOUNCE(pounce);
		account     = gaim_pounce_get_pouncer(pounce);

		/* Pouncee name */
		str1 = escape_text2(gaim_pounce_get_pouncee(pounce));

		if (pounce_data == NULL)
		{
			fprintf(f, "\tentry { %s } {  } {  } { %d } { %s } { %d } {  }\n",
					str1, new_pounce_opts_to_old(pounce),
					account->username, account->protocol);

			free(str1);

			continue;
		}

		/* Message */
		if (pounce_data->message != NULL)
			str2 = escape_text2(pounce_data->message);
		else {
			str2 = malloc(1);
			*str2 = '\0';
		}

		/* Command */
		if (pounce_data->command != NULL)
			str3 = escape_text2(pounce_data->command);
		else {
			str3 = malloc(1);
			*str3 = '\0';
		}

		/* Sound file */
		if (pounce_data->sound != NULL)
			str4 = escape_text2(pounce_data->sound);
		else {
			str4 = malloc(1);
			*str4 = '\0';
		}

		fprintf(f, "\tentry { %s } { %s } { %s } { %d } { %s } { %d } { %s }\n",
				str1, str2, str3, new_pounce_opts_to_old(pounce),
				account->username, account->protocol, str4);

		/* escape_text2 uses malloc(), so we don't want to g_free these */
		free(str1);
		free(str2);
		free(str3);
		free(str4);
	}

	fprintf(f, "}\n");
}

static void gaimrc_write_plugins(FILE *f)
{
	GList *pl;
	GaimPlugin *p;

	fprintf(f, "plugins {\n");

	for (pl = gaim_plugins_get_loaded(); pl != NULL; pl = pl->next) {
		char *path;

		p = (GaimPlugin *)pl->data;

		if (p->info->type != GAIM_PLUGIN_PROTOCOL) {
			path = escape_text2(p->path);
			fprintf(f, "\tplugin { %s }\n", path);
			free(path);
		}
	}

	fprintf(f, "}\n");
}

static void gaimrc_read_plugins(FILE *f)
{
	struct parse parse_buffer;
	struct parse *p;
	char buf[4096];

	buf[0] = 0;

	while (buf[0] != '}') {
		if (!fgets(buf, sizeof(buf), f))
			break;

		if (buf[0] == '}')
			break;

		p = parse_line(buf, &parse_buffer);
		if (!strcmp(p->option, "plugin")) {
			gaim_plugin_load(gaim_plugin_probe(p->value[0]));
		}
	}
}

static struct gaim_account *gaimrc_read_user(FILE *f)
{
	struct parse parse_buffer;
	struct parse *p;
	struct gaim_account *account;
	int i;
	char buf[4096];

	if (!fgets(buf, sizeof(buf), f))
		return NULL;

	p = parse_line(buf, &parse_buffer);

	if (strcmp(p->option, "ident"))
		return NULL;

	account = g_new0(struct gaim_account, 1);

	strcpy(account->username, p->value[0]);
	strcpy(account->password, p->value[1]);

	account->user_info[0] = 0;
	account->options = OPT_ACCT_REM_PASS;
	account->protocol = GAIM_PROTO_DEFAULT;
	account->permit = account->deny = NULL;

	if (!fgets(buf, sizeof(buf), f))
		return account;

	if (strcmp(buf, "\t\tuser_info {\n")) {
		return account;
	}

	if (!fgets(buf, sizeof(buf), f))
		return account;

	while (strncmp(buf, "\t\t}", 3)) {
		if (strlen(buf) > 3)
			strcat(account->user_info, buf + 3);

		if (!fgets(buf, sizeof(buf), f)) {
			return account;
		}
	}

	if ((i = strlen(account->user_info))) {
		account->user_info[i - 1] = '\0';
	}

	if (!fgets(buf, sizeof(buf), f)) {
		return account;
	}

	if (!strcmp(buf, "\t}")) {
		return account;
	}

	p = parse_line(buf, &parse_buffer);

	if (strcmp(p->option, "user_opts"))
		return account;

	account->options = atoi(p->value[0]);
	account->protocol = atoi(p->value[1]);

	if (!fgets(buf, sizeof(buf), f))
		return account;

	if (!strcmp(buf, "\t}"))
		return account;

	p = parse_line(buf, &parse_buffer);

	if (strcmp(p->option, "proto_opts"))
		return account;

	for (i = 0; i < 7; i++)
		g_snprintf(account->proto_opt[i], sizeof account->proto_opt[i], "%s", p->value[i]);

	if (!fgets(buf, sizeof(buf), f))
		return account;

	if (!strcmp(buf, "\t}"))
		return account;

	p = parse_line(buf, &parse_buffer);

	if (strcmp(p->option, "iconfile"))
		return account;

	g_snprintf(account->iconfile, sizeof(account->iconfile), "%s", p->value[0]);

	if (!fgets(buf, sizeof(buf), f))
		return account;

	if (!strcmp(buf, "\t}"))
		return account;

	p = parse_line(buf, &parse_buffer);

	if (strcmp(p->option, "alias"))
		return account;

	g_snprintf(account->alias, sizeof(account->alias), "%s", p->value[0]);

	if (!fgets(buf, sizeof(buf), f))
		return account;

	if (!strcmp(buf, "\t}"))
		return account;

	p = parse_line(buf, &parse_buffer);

	if (strcmp(p->option, "proxy_opts"))
		return account;

	if(atoi(p->value[0]) != PROXY_USE_GLOBAL) {
		account->gpi = g_new0(struct gaim_proxy_info, 1);
		account->gpi->proxytype = atoi(p->value[0]);
		g_snprintf(account->gpi->proxyhost, sizeof(account->gpi->proxyhost),
				"%s", p->value[1]);
		account->gpi->proxyport = atoi(p->value[2]);
		g_snprintf(account->gpi->proxyuser, sizeof(account->gpi->proxyuser),
				"%s", p->value[3]);
		g_snprintf(account->gpi->proxypass, sizeof(account->gpi->proxypass),
				"%s", p->value[4]);
	}

	return account;

}

static void gaimrc_write_user(FILE *f, struct gaim_account *account)
{
	char *c, *d;
	int nl = 1, i;

	if (account->options & OPT_ACCT_REM_PASS) {
		fprintf(f, "\t\tident { %s } { %s }\n", (d = escape_text2(account->username)), (c = escape_text2(account->password)));
		free(c);
		free(d);
	} else {
		fprintf(f, "\t\tident { %s } {  }\n", (d = escape_text2(account->username)));
		free(d);
	}
	fprintf(f, "\t\tuser_info {");
	c = account->user_info;
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
	fprintf(f, "\t\tuser_opts { %d } { %d }\n", account->options, account->protocol);
	fprintf(f, "\t\tproto_opts");
	for (i = 0; i < 7; i++)
		fprintf(f, " { %s }", account->proto_opt[i]);
	fprintf(f, "\n");
#ifndef _WIN32
	fprintf(f, "\t\ticonfile { %s }\n", account->iconfile);
#else
	{
		/* Make sure windows dir speparators arn't swallowed up when
		   path is read back in from resource file */
		char* tmp=wgaim_escape_dirsep(account->iconfile);
		fprintf(f, "\t\ticonfile { %s }\n", tmp);
		g_free(tmp);
	}
#endif
	fprintf(f, "\t\talias { %s }\n", account->alias);
	fprintf(f, "\t\tproxy_opts ");
	if(account->gpi) {
		fprintf(f, "{ %d } { %s } { %d } { %s } { %s }\n",
				account->gpi->proxytype, account->gpi->proxyhost,
				account->gpi->proxyport, account->gpi->proxyuser,
				(c = escape_text2(account->gpi->proxypass)));
		free(c);
	} else {
		fprintf(f, "{ %d }\n", PROXY_USE_GLOBAL);
	}
}

static void gaimrc_read_users(FILE *f)
{
	char buf[2048];
	struct gaim_account *account = NULL;
	struct parse parse_buffer;
	struct parse *p=NULL;

	buf[0] = 0;

	while (fgets(buf, sizeof(buf), f)) {
		if (buf[0] == '#')
			continue;
		else if(buf[0] == '}')
			break;

		p = parse_line(buf, &parse_buffer);

		if (strcmp(p->option, "user")==0 ||
		    strcmp(p->option, "current_user")==0) {
			if((account=gaimrc_read_user(f))!=NULL)
				gaim_accounts = g_slist_append(gaim_accounts, account);
			else {
				gaim_debug(GAIM_DEBUG_ERROR, "gaimrc",
						   "Error reading in users from .gaimrc\n");
				return;
			}
		}
	}
}

static void gaimrc_write_users(FILE *f)
{
	GSList *usr = gaim_accounts;
	struct gaim_account *account;

	fprintf(f, "users {\n");

	while (usr) {
		account = (struct gaim_account *)usr->data;
		fprintf(f, "\tuser {\n");
		gaimrc_write_user(f, account);

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
{ /* OPT_GEN_NEAR_APPLET */		0x00004000, &blist_options, OPT_BLIST_NEAR_APPLET },
{ /* OPT_GEN_CHECK_SPELLING */		0x00008000, &convo_options, OPT_CONVO_CHECK_SPELLING },
{ /* OPT_GEN_POPUP_CHAT */		0x00010000,  &chat_options, OPT_CHAT_POPUP },
{ /* OPT_GEN_BACK_ON_IM */		0x00020000,  &away_options, OPT_AWAY_BACK_ON_IM },
{ /* OPT_GEN_CTL_CHARS */		0x00080000, &convo_options, OPT_CONVO_CTL_CHARS },
#if 0
{ /* OPT_GEN_TOMBSTONE */		0x00100000,  &away_options, OPT_AWAY_TOMBSTONE },
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
{ /* OPT_DISP_NO_BUTTONS */		0x00002000, &blist_options, OPT_BLIST_NO_BUTTON_TEXT },
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
		} else if (!strcmp(p->option, "conv_placement")) {
			conv_placement_option = atoi(p->value[0]);
			gaim_conv_placement_set_active(conv_placement_option);
		} else if (!strcmp(p->option, "chat_options")) {
			chat_options = atoi(p->value[0]);
		} else if (!strcmp(p->option, "font_options")) {
			font_options = atoi(p->value[0]);
		} else if (!strcmp(p->option, "sound_options")) {
			sound_options = atoi(p->value[0]);
			gaim_sound_change_output_method();
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
	if (!(sound_options & (OPT_SOUND_BEEP | OPT_SOUND_NORMAL | OPT_SOUND_ESD
					| OPT_SOUND_ARTS | OPT_SOUND_NAS | OPT_SOUND_CMD))) {
		sound_options |= OPT_SOUND_NORMAL;
		gaim_sound_change_output_method();
	}

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
		if (gaim_plugins_enabled()) {
			gchar* buf;

			buf = g_strconcat(LIBDIR, G_DIR_SEPARATOR_S, 
#ifndef _WIN32
					  "ticker.so",
#else
					  "ticker.dll",
#endif
					  NULL);

			gaim_plugin_load(gaim_plugin_probe(buf));
			g_free(buf);
		}

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
	fprintf(f, "\tconv_placement { %u }\n", conv_placement_option);
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

	for(i=0; i<GAIM_NUM_SOUNDS; i++)
		gaim_sound_set_event_file(i, NULL);

	while (buf[0] != '}') {
		if (buf[0] == '#')
			continue;

		if (!fgets(buf, sizeof(buf), f))
			return;

		p = parse_line(buf, &parse_buffer);
#ifndef _WIN32
		if (!strcmp(p->option, "sound_cmd")) {
			gaim_sound_set_command(p->value[0]);
		} else
#endif
		if (!strncmp(p->option, "sound", strlen("sound"))) {
			i = p->option[strlen("sound")] - 'A';

			if (p->value[0][0])
				gaim_sound_set_event_file(i, p->value[0]);
		}
	}
}

static void gaimrc_write_sounds(FILE *f)
{
	int i;
#ifndef _WIN32
	char *cmd;
#endif
	fprintf(f, "sound_files {\n");
	for (i = 0; i < GAIM_NUM_SOUNDS; i++) {
		char *file = gaim_sound_get_event_file(i);
		if (file) {
#ifndef _WIN32
			fprintf(f, "\tsound%c { %s }\n", i + 'A', file);
#else
			/* Make sure windows dir speparators arn't swallowed up when
			   path is read back in from resource file */
			char* tmp=wgaim_escape_dirsep(file);
			fprintf(f, "\tsound%c { %s }\n", i + 'A', tmp);
			g_free(tmp);
#endif
		}
		else
			fprintf(f, "\tsound%c {  }\n", i + 'A');
	}
#ifndef _WIN32
	cmd = gaim_sound_get_command();
	if(cmd)
		fprintf(f, "\tsound_cmd { %s }\n", cmd);
	else
		fprintf(f, "\tsound_cmd {  }\n");
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

	gaim_debug(GAIM_DEBUG_MISC, "gaimrc",
			   "gaimrc_parse_proxy_uri(%s)\n", proxy);

	if ((c = strchr(proxy, ':')) == NULL)
	{
		gaim_debug(GAIM_DEBUG_ERROR, "gaimrc",
				   "No URI detected.\n");
		/* No URI detected. */
		return FALSE;
	}

	len = c - proxy;

	if (!strncmp(proxy, "http://", len + 3))
		global_proxy_info.proxytype = PROXY_HTTP;
	else
		return FALSE;

	gaim_debug(GAIM_DEBUG_MISC, "gaimrc", "Found HTTP proxy.\n");
	/* Get past "://" */
	c += 3;

	gaim_debug(GAIM_DEBUG_MISC, "gaimrc", "Looking at %s\n", c);

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
		strcpy(global_proxy_info.proxyhost, host);
	else
		*global_proxy_info.proxyhost = '\0';

	if (user[0])
		strcpy(global_proxy_info.proxyuser, user);
	else
		*global_proxy_info.proxyuser = '\0';

	if (pass[0])
		strcpy(global_proxy_info.proxypass, pass);
	else
		*global_proxy_info.proxypass = '\0';

	global_proxy_info.proxyport = port;

	gaim_debug(GAIM_DEBUG_MISC, "gaimrc",
			   "Host: '%s', User: '%s', Password: '%s', Port: %d\n",
			   global_proxy_info.proxyhost, global_proxy_info.proxyuser,
			   global_proxy_info.proxypass, global_proxy_info.proxyport);

	return TRUE;
}

static void gaimrc_read_proxy(FILE *f)
{
	char buf[2048];
	struct parse parse_buffer;
	struct parse *p;

	buf[0] = 0;
	global_proxy_info.proxyhost[0] = 0;
	gaim_debug(GAIM_DEBUG_MISC, "gaimrc", "gaimrc_read_proxy\n");

	while (buf[0] != '}') {
		if (buf[0] == '#')
			continue;

		if (!fgets(buf, sizeof(buf), f))
			return;

		p = parse_line(buf, &parse_buffer);

		if (!strcmp(p->option, "host")) {
			g_snprintf(global_proxy_info.proxyhost,
					sizeof(global_proxy_info.proxyhost), "%s", p->value[0]);
			gaim_debug(GAIM_DEBUG_MISC, "gaimrc",
					   "Set proxyhost %s\n", global_proxy_info.proxyhost);
		} else if (!strcmp(p->option, "port")) {
			global_proxy_info.proxyport = atoi(p->value[0]);
		} else if (!strcmp(p->option, "type")) {
			global_proxy_info.proxytype = atoi(p->value[0]);
		} else if (!strcmp(p->option, "user")) {
			g_snprintf(global_proxy_info.proxyuser,
					sizeof(global_proxy_info.proxyuser), "%s", p->value[0]);
		} else if (!strcmp(p->option, "pass")) {
			g_snprintf(global_proxy_info.proxypass,
					sizeof(global_proxy_info.proxypass), "%s", p->value[0]);
		}
	}
	if (global_proxy_info.proxyhost[0])
		proxy_info_is_from_gaimrc = 1;
	else {
		gboolean getVars = TRUE;
		proxy_info_is_from_gaimrc = 0;

		if (g_getenv("HTTP_PROXY"))
			g_snprintf(global_proxy_info.proxyhost,
					sizeof(global_proxy_info.proxyhost), "%s",
					g_getenv("HTTP_PROXY"));
		else if (g_getenv("http_proxy"))
			g_snprintf(global_proxy_info.proxyhost,
					sizeof(global_proxy_info.proxyhost), "%s",
					g_getenv("http_proxy"));
		else if (g_getenv("HTTPPROXY"))
			g_snprintf(global_proxy_info.proxyhost,
					sizeof(global_proxy_info.proxyhost), "%s",
					g_getenv("HTTPPROXY"));

		if (*global_proxy_info.proxyhost != '\0')
			getVars = !gaimrc_parse_proxy_uri(global_proxy_info.proxyhost);

		if (getVars)
		{
			if (g_getenv("HTTP_PROXY_PORT"))
				global_proxy_info.proxyport = atoi(g_getenv("HTTP_PROXY_PORT"));
			else if (g_getenv("http_proxy_port"))
				global_proxy_info.proxyport = atoi(g_getenv("http_proxy_port"));
			else if (g_getenv("HTTPPROXYPORT"))
				global_proxy_info.proxyport = atoi(g_getenv("HTTPPROXYPORT"));

			if (g_getenv("HTTP_PROXY_USER"))
				g_snprintf(global_proxy_info.proxyuser,
						sizeof(global_proxy_info.proxyuser), "%s",
						g_getenv("HTTP_PROXY_USER"));
			else if (g_getenv("http_proxy_user"))
				g_snprintf(global_proxy_info.proxyuser,
						sizeof(global_proxy_info.proxyuser), "%s",
						g_getenv("http_proxy_user"));
			else if (g_getenv("HTTPPROXYUSER"))
				g_snprintf(global_proxy_info.proxyuser,
						sizeof(global_proxy_info.proxyuser), "%s",
						g_getenv("HTTPPROXYUSER"));

			if (g_getenv("HTTP_PROXY_PASS"))
				g_snprintf(global_proxy_info.proxypass,
						sizeof(global_proxy_info.proxypass), "%s",
						g_getenv("HTTP_PROXY_PASS"));
			else if (g_getenv("http_proxy_pass"))
				g_snprintf(global_proxy_info.proxypass,
						sizeof(global_proxy_info.proxypass), "%s",
						g_getenv("http_proxy_pass"));
			else if (g_getenv("HTTPPROXYPASS"))
				g_snprintf(global_proxy_info.proxypass,
						sizeof(global_proxy_info.proxypass), "%s",
						g_getenv("HTTPPROXYPASS"));

			if (global_proxy_info.proxyhost[0])
				global_proxy_info.proxytype = PROXY_HTTP;
		}
	}
}

static void gaimrc_write_proxy(FILE *f)
{
	char *str;

	fprintf(f, "proxy {\n");
	if (proxy_info_is_from_gaimrc) {
		fprintf(f, "\thost { %s }\n", global_proxy_info.proxyhost);
		fprintf(f, "\tport { %d }\n", global_proxy_info.proxyport);
		fprintf(f, "\ttype { %d }\n", global_proxy_info.proxytype);
		fprintf(f, "\tuser { %s }\n", global_proxy_info.proxyuser);
		fprintf(f, "\tpass { %s }\n",
				(str = escape_text2(global_proxy_info.proxypass)));
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
		OPT_BLIST_SHOW_BUTTON_XPM |
		OPT_BLIST_SHOW_ICONS;

	convo_options =
		OPT_CONVO_ENTER_SENDS |
		OPT_CONVO_SEND_LINKS |
		OPT_CONVO_CTL_CHARS |
		OPT_CONVO_CTL_SMILEYS |
		OPT_CONVO_SHOW_TIME |
		OPT_CONVO_SHOW_SMILEY |
		OPT_CONVO_CHECK_SPELLING;

	conv_placement_option = 0;

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

	for (i = 0; i < GAIM_NUM_SOUNDS; i++)
		gaim_sound_set_event_file(i, NULL);

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
		OPT_SOUND_NAS;

#ifdef USE_SCREENSAVER
	report_idle = IDLE_SCREENSAVER;
#else
	report_idle = IDLE_GAIM;
#endif
	web_browser = BROWSER_NETSCAPE;
	g_snprintf(web_command, sizeof(web_command), "xterm -e lynx %%s");

	auto_away = 10;
	a = g_new0(struct away_message, 1);
	g_snprintf(a->name, sizeof(a->name), _("boring default"));
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

	gaim_debug(GAIM_DEBUG_INFO, "gaimrc", "Loading preferences.\n");

	if (is_saving_prefs) {
		request_load_prefs = 1;
		gaim_debug(GAIM_DEBUG_INFO, "gaimrc",
				   "Currently saving. Will request load.\n");
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
		gaim_debug(GAIM_DEBUG_MISC, "gaimrc", "start load_prefs\n");
		fgets(buf, sizeof(buf), f);
		sscanf(buf, "# .gaimrc v%d", &ver);
		if ((ver <= 3) || (buf[0] != '#'))
			set_defaults();

		while (!feof(f)) {
			int tag = gaimrc_parse_tag(f);
			gaim_debug(GAIM_DEBUG_MISC, "gaimrc",
					   "starting read tag %d\n", tag);
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
			case 3:
				if (gaim_plugins_enabled())
					gaimrc_read_plugins(f);
				break;
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
			gaim_debug(GAIM_DEBUG_MISC, "gaimrc",
					   "ending read tag %d\n", tag);
		}
		fclose(f);
		is_loading_prefs = 0;
		gaim_debug(GAIM_DEBUG_MISC, "gaimrc", "end load_prefs\n");
		if (request_save_prefs) {
			gaim_debug(GAIM_DEBUG_INFO, "gaimrc",
					   "Saving preferences on request\n");
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

	prefs_initial_load = 1;
}

void save_prefs()
{
	FILE *f;
	gchar *filename;
	gchar *filename_temp;

	gaim_debug(GAIM_DEBUG_INFO, "gaimrc", "Entering save_prefs\n");
	if (!prefs_initial_load)
		return;

	if (is_loading_prefs) {
		request_save_prefs = 1;
		gaim_debug(GAIM_DEBUG_INFO, "gaimrc",
				   "Currently loading. Will request save.\n");
		return;
	}

	if (opt_rcfile_arg)
		filename = g_build_filename(opt_rcfile_arg, NULL);
	else if (gaim_home_dir())
		filename = g_build_filename(gaim_home_dir(), ".gaimrc", NULL);
	else
		return;
	filename_temp = g_strdup_printf("%s.save", filename);

	if ((f = fopen(filename_temp, "w"))) {
		chmod(filename_temp, S_IRUSR | S_IWUSR);
		is_saving_prefs = 1;
		fprintf(f, "# .gaimrc v%d\n", 4);
		gaimrc_write_users(f);
		gaimrc_write_options(f);
		gaimrc_write_sounds(f);
		gaimrc_write_away(f);
		gaimrc_write_pounce(f);

		if (gaim_plugins_enabled())
			gaimrc_write_plugins(f);

		gaimrc_write_proxy(f);
		fclose(f);
		if (rename(filename_temp, filename) < 0)
			gaim_debug(GAIM_DEBUG_ERROR, "gaimrc",
					   "Error renaming %s to %s\n", filename_temp, filename);
		is_saving_prefs = 0;
	} else
		gaim_debug(GAIM_DEBUG_ERROR, "gaimrc",
				   "Error opening %s\n", filename_temp);

	if (request_load_prefs) {
		gaim_debug(GAIM_DEBUG_INFO, "gaimrc",
				   "Loading preferences on request.\n");
		load_prefs();
		request_load_prefs = 0;
	}

	g_free(filename);
	g_free(filename_temp);

	gaim_debug(GAIM_DEBUG_INFO, "gaimrc", "Exiting save_prefs\n");
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

void
load_pounces()
{
	GList *l;
	struct pounce_placeholder *ph;
	struct gaim_pounce *pounce;
	struct gaim_account *account;

	for (l = buddy_pounces; l != NULL; l = l->next) {
		GaimPounceEvent events = GAIM_POUNCE_NONE;
		GaimGtkPounceAction actions = GAIM_GTKPOUNCE_NONE;
		ph = (struct pounce_placeholder *)l->data;

		account = gaim_account_find(ph->pouncer, ph->protocol);

		old_pounce_opts_to_new(ph->options, &events, &actions);

		pounce = gaim_gtkpounce_new(account, ph->name, events, actions,
			(*ph->message == '\0' ? NULL : ph->message),
			(*ph->command == '\0' ? NULL : ph->command),
			(*ph->sound   == '\0' ? NULL : ph->sound),
			(ph->options & 0x100));

		g_free(ph);
	}

	g_list_free(buddy_pounces);
	buddy_pounces = NULL;

	/* 
	 * < ChipX86|Coding> why do we save prefs just after reading them?
	 * <      faceprint> ChipX86|Coding: because we're cool like that
	 * <SeanEgan|Coding> damn straight
	 */
	save_prefs();
}
