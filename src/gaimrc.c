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
#include "internal.h"

#include "account.h"
#include "debug.h"
#include "log.h"
#include "notify.h"
#include "pounce.h"
#include "prefs.h"
#include "proxy.h"
#include "prpl.h"
#include "sound.h"
#include "ui.h"
#include "util.h"

#include "gaim.h"

#include "gtkpounce.h"
#include "gtksound.h"

/* for people like myself, who are too lazy to add an away msg :) */
#define BORING_DEFAULT_AWAY_MSG _("sorry, i ran out for a while. bbl")
#define MAX_VALUES 10

#define OPT_FONT_BOLD			0x00000001
#define OPT_FONT_ITALIC			0x00000002
#define OPT_FONT_UNDERLINE		0x00000008
#define OPT_FONT_STRIKE			0x00000010
#define OPT_FONT_FACE			0x00000020
#define OPT_FONT_FGCOL			0x00000040
#define OPT_FONT_BGCOL			0x00000080
#define OPT_FONT_SIZE			0x00000100

#define OPT_MISC_DEBUG			0x00000001
#define OPT_MISC_BROWSER_POPUP		0x00000002
#define OPT_MISC_BUDDY_TICKER		0x00000004
#define OPT_MISC_STEALTH_TYPING		0x00000010
#define OPT_MISC_USE_SERVER_ALIAS	0x00000020

#define OPT_LOG_CONVOS			0x00000001
#define OPT_LOG_STRIP_HTML		0x00000002
#define OPT_LOG_INDIVIDUAL		0x00000040
#define OPT_LOG_CHATS			0x00000100

#define OPT_BLIST_SHOW_GRPNUM		0x00000008
#define OPT_BLIST_SHOW_PIXMAPS		0x00000010
#define OPT_BLIST_SHOW_IDLETIME		0x00000020
#define OPT_BLIST_SHOW_BUTTON_XPM	0x00000040
#define OPT_BLIST_NO_BUTTON_TEXT	0x00000080
#define OPT_BLIST_NO_MT_GRP		0x00000100
#define OPT_BLIST_SHOW_WARN		0x00000200
#define OPT_BLIST_GREY_IDLERS		0x00000400
#define OPT_BLIST_POPUP                 0x00001000
#define OPT_BLIST_SHOW_ICONS            0x00002000
#define OPT_BLIST_SHOW_OFFLINE          0x00004000

#define OPT_CONVO_ENTER_SENDS		0x00000001
#define OPT_CONVO_SEND_LINKS		0x00000002
#define OPT_CONVO_CHECK_SPELLING	0x00000004
#define OPT_CONVO_CTL_CHARS		0x00000008
#define OPT_CONVO_CTL_SMILEYS		0x00000010
#define OPT_CONVO_ESC_CAN_CLOSE		0x00000020
#define OPT_CONVO_CTL_ENTER		0x00000040
#define OPT_CONVO_F2_TOGGLES		0x00000080
#define OPT_CONVO_SHOW_TIME		0x00000100
#define OPT_CONVO_IGNORE_COLOUR		0x00000200
#define OPT_CONVO_SHOW_SMILEY		0x00000400
#define OPT_CONVO_IGNORE_FONTS		0x00000800
#define OPT_CONVO_IGNORE_SIZES		0x00001000
#define OPT_CONVO_COMBINE		0x00002000
#define OPT_CONVO_CTL_W_CLOSES          0x00004000
#define OPT_CONVO_NO_X_ON_TAB		0x00008000

#define OPT_IM_POPUP			0x00000001
#define OPT_IM_LOGON			0x00000002
#define OPT_IM_BUTTON_TEXT		0x00000004
#define OPT_IM_BUTTON_XPM		0x00000008
#define OPT_IM_ONE_WINDOW		0x00000010
#define OPT_IM_SIDE_TAB			0x00000020
#define OPT_IM_BR_TAB			0x00000040
#define OPT_IM_HIDE_ICONS		0x00000080
#define OPT_IM_NO_ANIMATION		0x00000100
#define OPT_IM_ALIAS_TAB		0x00002000
#define OPT_IM_POPDOWN			0x00004000

#define OPT_CHAT_ONE_WINDOW		0x00000001
#define OPT_CHAT_BUTTON_TEXT		0x00000002
#define OPT_CHAT_BUTTON_XPM		0x00000004
#define OPT_CHAT_LOGON			0x00000008
#define OPT_CHAT_POPUP			0x00000010
#define OPT_CHAT_SIDE_TAB		0x00000020
#define OPT_CHAT_BR_TAB			0x00000040
#define OPT_CHAT_TAB_COMPLETE		0x00000080
#define OPT_CHAT_OLD_STYLE_TAB		0x00000100
#define OPT_CHAT_COLORIZE               0x00000200

#define OPT_SOUND_LOGIN			0x00000001
#define OPT_SOUND_LOGOUT		0x00000002
#define OPT_SOUND_RECV			0x00000004
#define OPT_SOUND_SEND			0x00000008
#define OPT_SOUND_FIRST_RCV		0x00000010
#define OPT_SOUND_WHEN_AWAY		0x00000020
#define OPT_SOUND_SILENT_SIGNON		0x00000040
#define OPT_SOUND_THROUGH_GNOME		0x00000080
#define OPT_SOUND_CHAT_JOIN		0x00000100
#define OPT_SOUND_CHAT_SAY		0x00000200
#define OPT_SOUND_BEEP			0x00000400
#define OPT_SOUND_CHAT_PART		0x00000800
#define OPT_SOUND_CHAT_YOU_SAY		0x00001000
#define OPT_SOUND_NORMAL		0x00002000
#define OPT_SOUND_NAS			0x00004000
#define OPT_SOUND_ARTS			0x00008000
#define OPT_SOUND_ESD			0x00010000
#define OPT_SOUND_CMD			0x00020000
#define OPT_SOUND_CHAT_NICK             0x00040000

#define OPT_AWAY_BACK_ON_IM		0x00000002
#define OPT_AWAY_AUTO			0x00000008
#define OPT_AWAY_NO_AUTO_RESP		0x00000010
#define OPT_AWAY_QUEUE			0x00000020
#define OPT_AWAY_IDLE_RESP		0x00000040
#define OPT_AWAY_QUEUE_UNREAD		0x00000080
#define OPT_AWAY_DELAY_IN_USE		0x00000100

#define OPT_ACCT_AUTO		0x00000001
/*#define OPT_ACCT_KEEPALV	0x00000002 this shouldn't be optional */
#define OPT_ACCT_REM_PASS	0x00000004
#define OPT_ACCT_MAIL_CHECK      0x00000008

#define IDLE_NONE        0
#define IDLE_GAIM        1
#define IDLE_SCREENSAVER 2

#define BROWSER_NETSCAPE              0
#define BROWSER_KONQ                  1
#define BROWSER_MANUAL                2
/*#define BROWSER_INTERNAL              3*/
#define BROWSER_GNOME                 4
#define BROWSER_OPERA                 5 
#define BROWSER_GALEON                6
#define BROWSER_MOZILLA               7


static guint misc_options;
static guint logging_options;
static guint blist_options;
static guint convo_options;
static guint im_options;
static guint chat_options;
static guint font_options;
static guint sound_options;
static guint away_options;
static guint is_loading_prefs = 0;
static guint request_save_prefs = 0;
static guint is_saving_prefs = 0;
static guint request_load_prefs = 0;
static guint prefs_initial_load = 0;
guint proxy_info_is_from_gaimrc = 1; /* Only save proxy info if it
				      * was loaded from the file
				      * or otherwise explicitly requested */

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

	if (sscanf(buf, "%s {", tag) != 1)
		return -1;

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
			gaim_prefs_set_int("/core/away/mins_before_away",
					atoi(p->value[0]));
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

/*
 * This is temporary, and we're using it to translate the new event
 * and action values into the old ones. We're also adding entries for
 * new types, but if you go and use an older gaim, these will be nuked.
 * When we have a better prefs system, this can go away.
 *
 *   -- ChipX86
 */
typedef enum
{
	GAIM_GTKPOUNCE_NONE       = 0x00, /**< No action.          */
	GAIM_GTKPOUNCE_OPEN_WIN   = 0x01, /**< Open an IM window.  */
	GAIM_GTKPOUNCE_POPUP      = 0x02, /**< Popup notification. */
	GAIM_GTKPOUNCE_SEND_MSG   = 0x04, /**< Send a message.     */
	GAIM_GTKPOUNCE_EXEC_CMD   = 0x08, /**< Execute a command.  */
	GAIM_GTKPOUNCE_PLAY_SOUND = 0x10  /**< Play a sound.       */

} GaimGtkPounceAction;

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

static GaimAccount *gaimrc_read_user(FILE *f)
{
	struct parse parse_buffer;
	struct parse *p;
	GaimAccount *account;
	int i;
	char buf[4096];
	char user_info[2048];
	int flags;
	char *tmp;

	if (!fgets(buf, sizeof(buf), f))
		return NULL;

	p = parse_line(buf, &parse_buffer);

	if (strcmp(p->option, "ident"))
		return NULL;

	account = gaim_account_new(p->value[0], GAIM_PROTO_DEFAULT);

	gaim_account_set_password(account, p->value[1]);
	gaim_account_set_remember_password(account, TRUE);

	gaim_accounts_add(account);

	if (!fgets(buf, sizeof(buf), f))
		return account;

	if (strcmp(buf, "\t\tuser_info {\n")) {
		return account;
	}

	if (!fgets(buf, sizeof(buf), f))
		return account;

	*user_info = '\0';

	while (strncmp(buf, "\t\t}", 3)) {
		if (strlen(buf) > 3)
			strcat(user_info, buf + 3);

		if (!fgets(buf, sizeof(buf), f)) {
			gaim_account_set_user_info(account, user_info);

			return account;
		}
	}

	if ((i = strlen(user_info)))
		user_info[i - 1] = '\0';

	if (*user_info != '.')
		gaim_account_set_user_info(account, user_info);

	if (!fgets(buf, sizeof(buf), f)) {
		return account;
	}

	if (!strcmp(buf, "\t}")) {
		return account;
	}

	p = parse_line(buf, &parse_buffer);

	if (strcmp(p->option, "user_opts"))
		return account;

	flags = atoi(p->value[0]);

	if (flags & OPT_ACCT_AUTO)
		gaim_account_set_auto_login(account, GAIM_GTK_UI, TRUE);

	if (flags & OPT_ACCT_MAIL_CHECK)
		gaim_account_set_check_mail(account, TRUE);

	if (!(flags & OPT_ACCT_REM_PASS))
		gaim_account_set_remember_password(account, FALSE);

	gaim_account_set_protocol(account, atoi(p->value[1]));

	if (!fgets(buf, sizeof(buf), f))
		return account;

	if (!strcmp(buf, "\t}"))
		return account;

	p = parse_line(buf, &parse_buffer);

	if (strcmp(p->option, "proto_opts"))
		return account;

	/* I hate this part. We must convert the protocol options. */
	switch (gaim_account_get_protocol(account)) {
		case GAIM_PROTO_TOC:
		case GAIM_PROTO_OSCAR:
			gaim_account_set_string(account, "server", p->value[0]);
			gaim_account_set_int(account, "port", atoi(p->value[1]));
			break;

		case GAIM_PROTO_JABBER:
			gaim_account_set_string(account, "connect_server", p->value[1]);
			gaim_account_set_int(account, "port", atoi(p->value[0]));
			break;

		case GAIM_PROTO_MSN:
		case GAIM_PROTO_NAPSTER:
		case GAIM_PROTO_YAHOO:
			gaim_account_set_string(account, "server", p->value[3]);
			gaim_account_set_int(account, "port", atoi(p->value[4]));
			break;

		case GAIM_PROTO_IRC:
			if(strlen(p->value[0]) && !strchr(account->username, '@')) {
				tmp = g_strdup_printf("%s@%s", account->username, p->value[0]);
				gaim_account_set_username(account, tmp);
				g_free(tmp);
			}
			gaim_account_set_int(account, "port", atoi(p->value[1]));
			gaim_account_set_string(account, "charset", p->value[2]);
			break;

		default:
			break;
	}

	if (!fgets(buf, sizeof(buf), f))
		return account;

	if (!strcmp(buf, "\t}"))
		return account;

	p = parse_line(buf, &parse_buffer);

	if (strcmp(p->option, "iconfile"))
		return account;

	if (*p->value[0] != '\n' && *p->value[0] != '\0')
		gaim_account_set_buddy_icon(account, p->value[0]);

	if (!fgets(buf, sizeof(buf), f))
		return account;

	if (!strcmp(buf, "\t}"))
		return account;

	p = parse_line(buf, &parse_buffer);

	if (strcmp(p->option, "alias"))
		return account;

	if (*p->value[0] != '\n' && *p->value[0] != '\0')
		gaim_account_set_alias(account, p->value[0]);

	if (!fgets(buf, sizeof(buf), f))
		return account;

	if (!strcmp(buf, "\t}"))
		return account;

	p = parse_line(buf, &parse_buffer);

	if (strcmp(p->option, "proxy_opts"))
		return account;

	if(atoi(p->value[0]) != GAIM_PROXY_USE_GLOBAL) {
		GaimProxyInfo *info;

		info = gaim_proxy_info_new();
		gaim_proxy_info_set_type(info, atoi(p->value[0]));
		gaim_proxy_info_set_host(info, p->value[1]);
		gaim_proxy_info_set_port(info, atoi(p->value[2]));
		gaim_proxy_info_set_username(info, p->value[3]);
		gaim_proxy_info_set_password(info, p->value[4]);

		gaim_account_set_proxy_info(account, info);
	}

	return account;

}

static void gaimrc_read_users(FILE *f)
{
	char buf[2048];
	GaimAccount *account = NULL;
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
			if((account=gaimrc_read_user(f))==NULL) {
				gaim_debug(GAIM_DEBUG_ERROR, "gaimrc",
						   "Error reading in users from .gaimrc\n");
				return;
			}
		}
	}
}

struct replace {
	int old;
	guint *val;
	int new;
};

#define OPT_GEN_ENTER_SENDS     0x00000001
#define OPT_GEN_LOG_ALL         0x00000004
#define OPT_GEN_STRIP_HTML      0x00000008
#define OPT_GEN_POPUP_WINDOWS   0x00000020
#define OPT_GEN_SEND_LINKS      0x00000040
#define OPT_GEN_DEBUG           0x00000100
#define OPT_GEN_BROWSER_POPUP   0x00000800
#define OPT_GEN_CHECK_SPELLING  0x00008000
#define OPT_GEN_POPUP_CHAT      0x00010000
#define OPT_GEN_BACK_ON_IM      0x00020000
#define OPT_GEN_CTL_CHARS       0x00080000
#define OPT_GEN_CTL_SMILEYS     0x00200000
#define OPT_GEN_AUTO_AWAY       0x00800000
#define OPT_GEN_ESC_CAN_CLOSE   0x01000000
#define OPT_GEN_CTL_ENTER       0x02000000
#define OPT_GEN_F2_TOGGLES      0x04000000
#define OPT_GEN_NO_AUTO_RESP    0x08000000
#define OPT_GEN_QUEUE_WHEN_AWAY 0x10000000

#define OPT_DISP_SHOW_TIME        0x00000001
#define OPT_DISP_SHOW_GRPNUM      0x00000002
#define OPT_DISP_SHOW_IDLETIME    0x00000008
#define OPT_DISP_SHOW_BUTTON_XPM  0x00000010
#define OPT_DISP_IGNORE_COLOUR    0x00000020
#define OPT_DISP_SHOW_LOGON       0x00000040
#define OPT_DISP_SHOW_SMILEY      0x00000100
#define OPT_DISP_CHAT_LOGON       0x00000800
#define OPT_DISP_NO_BUTTONS       0x00002000
#define OPT_DISP_CONV_BUTTON_TEXT 0x00004000
#define OPT_DISP_CHAT_BUTTON_TEXT 0x00008000
#define OPT_DISP_NO_MT_GRP        0x00040000
#define OPT_DISP_CONV_BUTTON_XPM  0x00080000
#define OPT_DISP_CHAT_BUTTON_XPM  0x00100000
#define OPT_DISP_SHOW_WARN        0x00200000
#define OPT_DISP_IGNORE_FONTS     0x00400000
#define OPT_DISP_IGNORE_SIZES     0x00800000
#define OPT_DISP_ONE_CHAT_WINDOW  0x02000000
#define OPT_DISP_CONV_SIDE_TAB    0x04000000
#define OPT_DISP_CONV_BR_TAB      0x08000000

static void gaimrc_read_options(FILE *f)
{
	char buf[2048];
	struct parse parse_buffer;
	struct parse *p;
	gboolean read_logging = FALSE, read_general = FALSE;
	int general_options = 0, display_options = 0;

	buf[0] = 0;

	while (buf[0] != '}') {
		if (buf[0] == '#')
			continue;

		if (!fgets(buf, sizeof(buf), f))
			return;

		p = parse_line(buf, &parse_buffer);

		/* XXX: what are we going to do about ancient prefs? */
		if (!strcmp(p->option, "general_options")) {
			general_options = atoi(p->value[0]);
			read_general = TRUE;
			gaim_prefs_set_bool("/gaim/gtk/conversations/enter_sends",
					general_options & OPT_GEN_ENTER_SENDS);
			gaim_prefs_set_bool("/gaim/gtk/conversations/im/raise_on_events",
					general_options & OPT_GEN_POPUP_WINDOWS);
			gaim_prefs_set_bool("/core/conversations/send_urls_as_links",
					general_options & OPT_GEN_SEND_LINKS);
			gaim_prefs_set_bool("/gaim/gtk/debug/enabled",
					general_options & OPT_GEN_DEBUG);
			gaim_prefs_set_bool("/gaim/gtk/browsers/new_window",
					general_options & OPT_GEN_BROWSER_POPUP);
			gaim_prefs_set_bool("/gaim/gtk/conversations/spellecheck",
					general_options & OPT_GEN_CHECK_SPELLING);
			gaim_prefs_set_bool("/gaim/gtk/conversations/chat/raise_on_events",
					general_options & OPT_GEN_POPUP_CHAT);
			gaim_prefs_set_bool("/core/conversations/away_back_on_send",
					general_options & OPT_GEN_BACK_ON_IM);
			gaim_prefs_set_bool("/gaim/gtk/conversations/html_shortcuts",
					general_options & OPT_GEN_CTL_CHARS);
			gaim_prefs_set_bool("/gaim/gtk/conversations/smiley_shortcuts",
					general_options & OPT_GEN_CTL_SMILEYS);
			gaim_prefs_set_bool("/core/away/away_when_idle",
					general_options & OPT_GEN_AUTO_AWAY);
			gaim_prefs_set_bool("/gaim/gtk/conversations/escape_closes",
					general_options & OPT_GEN_ESC_CAN_CLOSE);
			gaim_prefs_set_bool("/gaim/gtk/conversations/ctrl_enter_sends",
					general_options & OPT_GEN_CTL_ENTER);
			gaim_prefs_set_bool("/core/away/auto_response/enabled",
					!(general_options & OPT_GEN_NO_AUTO_RESP));
			gaim_prefs_set_bool("/gaim/gtk/away/queu_messages",
					general_options & OPT_AWAY_QUEUE);
		} else if (!strcmp(p->option, "display_options")) {
			display_options = atoi(p->value[0]);
			gaim_prefs_set_bool("/gaim/gtk/conversations/show_timestamps",
					display_options & OPT_DISP_SHOW_TIME);
			gaim_prefs_set_bool("/gaim/gtk/blist/show_group_count",
					display_options & OPT_DISP_SHOW_GRPNUM);
			gaim_prefs_set_bool("/gaim/gtk/blist/show_idle_time",
					display_options & OPT_DISP_SHOW_IDLETIME);
			gaim_prefs_set_int("/gaim/gtk/blist/button_style",
					((display_options & OPT_DISP_SHOW_BUTTON_XPM)
					 ? ((display_options & OPT_DISP_NO_BUTTONS)
						 ? GAIM_BUTTON_IMAGE : GAIM_BUTTON_TEXT_IMAGE)
					 : ((display_options & OPT_DISP_NO_BUTTONS)
						 ? GAIM_BUTTON_NONE : GAIM_BUTTON_TEXT)));
			gaim_prefs_set_bool("/gaim/gtk/conversations/ignore_colors",
					display_options & OPT_DISP_IGNORE_COLOUR);
			gaim_prefs_set_bool("/core/conversations/im/show_login",
					display_options & OPT_DISP_SHOW_LOGON);
			gaim_prefs_set_bool("/gaim/gtk/conversations/show_smileys",
					display_options & OPT_DISP_SHOW_SMILEY);
			gaim_prefs_set_bool("/core/conversations/chat/show_join",
					display_options & OPT_DISP_CHAT_LOGON);
			gaim_prefs_set_bool("/gaim/gtk/conversations/im/button_type",
					((display_options & OPT_DISP_CONV_BUTTON_XPM)
					 ? ((display_options & OPT_DISP_CONV_BUTTON_TEXT)
						 ? GAIM_BUTTON_TEXT_IMAGE : GAIM_BUTTON_IMAGE)
					 : ((display_options & OPT_DISP_CONV_BUTTON_TEXT)
						 ? GAIM_BUTTON_TEXT : GAIM_BUTTON_NONE)));
			gaim_prefs_set_bool("/gaim/gtk/conversations/chat/button_type",
					((display_options & OPT_DISP_CHAT_BUTTON_XPM)
					 ? ((display_options & OPT_DISP_CHAT_BUTTON_TEXT)
						 ? GAIM_BUTTON_TEXT_IMAGE : GAIM_BUTTON_IMAGE)
					 : ((display_options & OPT_DISP_CHAT_BUTTON_TEXT)
						 ? GAIM_BUTTON_TEXT : GAIM_BUTTON_NONE)));
			gaim_prefs_set_bool("/gaim/gtk/blist/show_warning_level",
					display_options & OPT_DISP_SHOW_WARN);
			gaim_prefs_set_bool("/gaim/gtk/conversations/ignore_fonts",
					display_options & OPT_DISP_IGNORE_FONTS);
			gaim_prefs_set_bool("/gaim/gtk/conversations/ignore_font_sizes",
					display_options & OPT_DISP_IGNORE_SIZES);
			gaim_prefs_set_int("/gaim/gtk/conversations/tab_side",
					((display_options & OPT_DISP_CONV_SIDE_TAB)
					 ? ((display_options & OPT_DISP_CONV_BR_TAB)
						 ? GTK_POS_RIGHT : GTK_POS_LEFT)
					 : ((display_options & OPT_DISP_CONV_BR_TAB)
						 ? GTK_POS_BOTTOM : GTK_POS_TOP)));
		} else if (!strcmp(p->option, "misc_options")) {
			misc_options = atoi(p->value[0]);
			gaim_prefs_set_bool("/gaim/gtk/debug/enabled",
					misc_options & OPT_MISC_DEBUG);
			gaim_prefs_set_bool("/gaim/gtk/browsers/new_window",
					misc_options & OPT_MISC_BROWSER_POPUP);
			gaim_prefs_set_bool("/gaim/gtk/conversations/im/send_typing",
					!(misc_options & OPT_MISC_STEALTH_TYPING));
			gaim_prefs_set_bool("/gaim/gtk/buddies/use_server_alias",
					misc_options & OPT_MISC_USE_SERVER_ALIAS);
		} else if (!strcmp(p->option, "logging_options")) {
			logging_options = atoi(p->value[0]);
			read_logging = TRUE;
			gaim_prefs_set_bool("/gaim/gtk/logging/log_ims",
					logging_options & OPT_LOG_CONVOS);
			gaim_prefs_set_bool("/gaim/gtk/logging/strip_html",
					logging_options & OPT_LOG_STRIP_HTML);
			gaim_prefs_set_bool("/gaim/gtk/logging/individual_logs",
					logging_options & OPT_LOG_INDIVIDUAL);
			gaim_prefs_set_bool("/gaim/gtk/logging/log_chats",
					logging_options & OPT_LOG_CHATS);
			gaim_prefs_set_bool("/gaim/gtk/logging/log_signon_signoff",
					logging_options & OPT_LOG_BUDDY_SIGNON);
			gaim_prefs_set_bool("/gaim/gtk/logging/log_idle_state",
					logging_options & OPT_LOG_BUDDY_IDLE);
			gaim_prefs_set_bool("/gaim/gtk/logging/log_away_state",
					logging_options & OPT_LOG_BUDDY_AWAY);
			gaim_prefs_set_bool("/gaim/gtk/logging/log_own_states",
					logging_options & OPT_LOG_MY_SIGNON);
		} else if (!strcmp(p->option, "blist_options")) {
			blist_options = atoi(p->value[0]);
			gaim_prefs_set_bool("/gaim/gtk/blist/show_group_count",
					blist_options & OPT_BLIST_SHOW_GRPNUM);
			gaim_prefs_set_bool("/gaim/gtk/blist/show_idle_time",
					blist_options & OPT_BLIST_SHOW_IDLETIME);
			gaim_prefs_set_bool("/gaim/gtk/blist/show_empty_groups",
					!(blist_options & OPT_BLIST_NO_MT_GRP));
			gaim_prefs_set_bool("/gaim/gtk/blist/show_warning_level",
					blist_options & OPT_BLIST_SHOW_WARN);
			gaim_prefs_set_bool("/gaim/gtk/blist/grey_idle_buddies",
					blist_options & OPT_BLIST_GREY_IDLERS);
			gaim_prefs_set_bool("/gaim/gtk/blist/raise_on_events",
					blist_options & OPT_BLIST_POPUP);
			gaim_prefs_set_bool("/gaim/gtk/blist/show_buddy_icons",
					blist_options & OPT_BLIST_SHOW_ICONS);
			gaim_prefs_set_bool("/gaim/gtk/blist/show_offline_buddies",
					blist_options & OPT_BLIST_SHOW_OFFLINE);
			gaim_prefs_set_int("/gaim/gtk/blist/button_style",
					((blist_options & OPT_BLIST_SHOW_BUTTON_XPM)
					 ? ((blist_options & OPT_BLIST_NO_BUTTON_TEXT)
						 ? GAIM_BUTTON_IMAGE : GAIM_BUTTON_TEXT_IMAGE)
					 : ((blist_options & OPT_BLIST_NO_BUTTON_TEXT)
						 ? GAIM_BUTTON_NONE : GAIM_BUTTON_TEXT)));
		} else if (!strcmp(p->option, "convo_options")) {
			convo_options = atoi(p->value[0]);
			gaim_prefs_set_bool("/gaim/gtk/conversations/enter_sends",
					convo_options & OPT_CONVO_ENTER_SENDS);
			gaim_prefs_set_bool("/core/conversations/send_urls_as_links",
					convo_options & OPT_CONVO_SEND_LINKS);
			gaim_prefs_set_bool("/gaim/gtk/conversations/spellcheck",
					convo_options & OPT_CONVO_CHECK_SPELLING);
			gaim_prefs_set_bool("/gaim/gtk/conversations/html_shortcuts",
					convo_options & OPT_CONVO_CTL_CHARS);
			gaim_prefs_set_bool("/gaim/gtk/conversations/smiley_shortcuts",
					convo_options & OPT_CONVO_CTL_SMILEYS);
			gaim_prefs_set_bool("/gaim/gtk/conversations/escape_closes",
					convo_options & OPT_CONVO_ESC_CAN_CLOSE);
			gaim_prefs_set_bool("/gaim/gtk/conversations/ctrl_enter_sends",
					convo_options & OPT_CONVO_CTL_ENTER);
			gaim_prefs_set_bool("/gaim/gtk/conversations/show_timestamps",
					convo_options & OPT_CONVO_SHOW_TIME);
			gaim_prefs_set_bool("/gaim/gtk/conversations/ignore_colors",
					convo_options & OPT_CONVO_IGNORE_COLOUR);
			gaim_prefs_set_bool("/gaim/gtk/conversations/show_smileys",
					convo_options & OPT_CONVO_SHOW_SMILEY);
			gaim_prefs_set_bool("/gaim/gtk/conversations/ignore_fonts",
					convo_options & OPT_CONVO_IGNORE_FONTS);
			gaim_prefs_set_bool("/gaim/gtk/conversations/ignore_font_sizes",
					convo_options & OPT_CONVO_IGNORE_SIZES);
			gaim_prefs_set_bool("/core/conversations/combine_chat_im",
					convo_options & OPT_CONVO_COMBINE);
			gaim_prefs_set_bool("/gaim/gtk/conversations/ctrl_w_closes",
					convo_options & OPT_CONVO_CTL_W_CLOSES);
			gaim_prefs_set_bool("/gaim/gtk/conversations/close_on_tabs",
					!(convo_options & OPT_CONVO_NO_X_ON_TAB));
		} else if (!strcmp(p->option, "im_options")) {
			im_options = atoi(p->value[0]);

			gaim_prefs_set_bool("/gaim/gtk/conversations/im/raise_on_events",
					im_options & OPT_IM_POPUP);
			gaim_prefs_set_bool("/core/conversations/im/show_login",
					im_options & OPT_IM_LOGON);
			gaim_prefs_set_bool("/gaim/gtk/conversations/im/button_type",
					((im_options & OPT_IM_BUTTON_XPM)
					 ? ((im_options & OPT_IM_BUTTON_TEXT)
						 ? GAIM_BUTTON_TEXT_IMAGE : GAIM_BUTTON_IMAGE)
					 : ((im_options & OPT_IM_BUTTON_TEXT)
						 ? GAIM_BUTTON_TEXT : GAIM_BUTTON_NONE)));
			gaim_prefs_set_int("/gaim/gtk/conversations/tab_side",
					((im_options & OPT_IM_SIDE_TAB)
					 ? ((im_options & OPT_IM_BR_TAB)
						 ? GTK_POS_RIGHT : GTK_POS_LEFT)
					 : ((im_options & OPT_IM_BR_TAB)
						 ? GTK_POS_BOTTOM : GTK_POS_TOP)));
			gaim_prefs_set_bool("/gaim/gtk/conversations/im/show_buddy_icons",
					!(im_options & OPT_IM_HIDE_ICONS));
			gaim_prefs_set_bool("/gaim/gtk/conversations/im/animate_buddy_icons",
					!(im_options & OPT_IM_NO_ANIMATION));
			gaim_prefs_set_bool("/core/conversations/use_alias_for_title",
					im_options & OPT_IM_ALIAS_TAB);
			gaim_prefs_set_bool("/gaim/gtk/conversations/im/hide_on_send",
								(im_options & OPT_IM_POPDOWN));

		} else if (!strcmp(p->option, "conv_placement")) {
			switch(atoi(p->value[0])) {
				case 1:
					gaim_prefs_set_string("/core/conversations/placement",
							"new");
				break;
				case 2:
					gaim_prefs_set_string("/core/conversations/placement",
							"group");
				break;
				case 3:
					gaim_prefs_set_string("/core/conversations/placement",
							"account");
				break;
				case 0:
				default:
					gaim_prefs_set_string("/core/conversations/placement",
							"last");
				break;
			}
		} else if (!strcmp(p->option, "chat_options")) {
			chat_options = atoi(p->value[0]);

			gaim_prefs_set_bool("/gaim/gtk/conversations/chat/button_type",
					((chat_options & OPT_CHAT_BUTTON_XPM)
					 ? ((chat_options & OPT_CHAT_BUTTON_TEXT)
						 ? GAIM_BUTTON_TEXT_IMAGE : GAIM_BUTTON_IMAGE)
					 : ((chat_options & OPT_CHAT_BUTTON_TEXT)
						 ? GAIM_BUTTON_TEXT : GAIM_BUTTON_NONE)));
			gaim_prefs_set_bool("/core/conversations/chat/show_join",
					chat_options & OPT_CHAT_LOGON);
			gaim_prefs_set_bool("/gaim/gtk/conversations/chat/raise_on_events",
					chat_options & OPT_CHAT_POPUP);
			gaim_prefs_set_bool("/gaim/gtk/conversations/chat/tab_completion",
					chat_options & OPT_CHAT_TAB_COMPLETE);
			gaim_prefs_set_bool("/gaim/gtk/conversations/chat/old_tab_complete",
					chat_options & OPT_CHAT_OLD_STYLE_TAB);
			gaim_prefs_set_bool("/gaim/gtk/conversations/chat/color_nicks",
					chat_options & OPT_CHAT_COLORIZE);

		} else if (!strcmp(p->option, "font_options")) {
			font_options = atoi(p->value[0]);

			gaim_prefs_set_bool("/gaim/gtk/conversations/send_bold",
					font_options & OPT_FONT_BOLD);
			gaim_prefs_set_bool("/gaim/gtk/conversations/send_italic",
					font_options & OPT_FONT_ITALIC);
			gaim_prefs_set_bool("/gaim/gtk/conversations/send_underline",
					font_options & OPT_FONT_UNDERLINE);
			gaim_prefs_set_bool("/gaim/gtk/conversations/send_strikethrough",
					font_options & OPT_FONT_STRIKE);
			gaim_prefs_set_bool("/gaim/gtk/conversations/use_custom_font",
					font_options & OPT_FONT_FACE);
			gaim_prefs_set_bool("/gaim/gtk/conversations/use_custom_size",
					font_options & OPT_FONT_SIZE);
			gaim_prefs_set_bool("/gaim/gtk/conversations/use_custom_fgcolor",
					font_options & OPT_FONT_FGCOL);
			gaim_prefs_set_bool("/gaim/gtk/conversations/use_custom_bgcolor",
					font_options & OPT_FONT_BGCOL);

		} else if (!strcmp(p->option, "sound_options")) {
			sound_options = atoi(p->value[0]);
			gaim_prefs_set_bool("/gaim/gtk/sound/enabled/login",
					sound_options & OPT_SOUND_LOGIN);
			gaim_prefs_set_bool("/gaim/gtk/sound/enabled/logout",
					sound_options & OPT_SOUND_LOGOUT);
			gaim_prefs_set_bool("/gaim/gtk/sound/enabled/im_recv",
					sound_options & OPT_SOUND_RECV);
			gaim_prefs_set_bool("/gaim/gtk/sound/enabled/send_im",
					sound_options & OPT_SOUND_SEND);
			gaim_prefs_set_bool("/gaim/gtk/sound/enabled/first_im_recv",
					sound_options & OPT_SOUND_FIRST_RCV);
			gaim_prefs_set_bool("/core/sound/when_away",
					sound_options & OPT_SOUND_WHEN_AWAY);
			gaim_prefs_set_bool("/gaim/gtk/sound/silent_signon",
					sound_options & OPT_SOUND_SILENT_SIGNON);
			gaim_prefs_set_bool("/gaim/gtk/sound/enabled/join_chat",
					sound_options & OPT_SOUND_CHAT_JOIN);
			gaim_prefs_set_bool("/gaim/gtk/sound/enabled/chat_msg_recv",
					sound_options & OPT_SOUND_CHAT_SAY);
			gaim_prefs_set_bool("/gaim/gtk/sound/enabled/left_chat",
					sound_options & OPT_SOUND_CHAT_PART);
			gaim_prefs_set_bool("/gaim/gtk/sound/enabled/send_chat_msg",
					sound_options & OPT_SOUND_CHAT_YOU_SAY);
			gaim_prefs_set_bool("/gaim/gtk/sound/enabled/nick_said",
					sound_options & OPT_SOUND_CHAT_NICK);
			if(sound_options & OPT_SOUND_NAS) {
				gaim_prefs_set_string("/gaim/gtk/sound/method", "nas");
			} else if(!(sound_options & OPT_SOUND_NORMAL)) {
				if(sound_options & OPT_SOUND_ESD)
					gaim_prefs_set_string("/gaim/gtk/sound/method", "esd");
				else if(sound_options & OPT_SOUND_ARTS)
					gaim_prefs_set_string("/gaim/gtk/sound/method", "arts");
				else if(sound_options & OPT_SOUND_CMD)
					gaim_prefs_set_string("/gaim/gtk/sound/method", "custom");
				else if(sound_options & OPT_SOUND_BEEP)
					gaim_prefs_set_string("/gaim/gtk/sound/method", "beep");
			}
		} else if (!strcmp(p->option, "away_options")) {
			away_options = atoi(p->value[0]);
			gaim_prefs_set_bool("/core/conversations/away_back_on_send",
					away_options & OPT_AWAY_BACK_ON_IM);
			gaim_prefs_set_bool("/core/away/away_when_idle",
					away_options & OPT_AWAY_AUTO);
			gaim_prefs_set_bool("/core/away/auto_response/enabled",
					!(away_options & OPT_AWAY_NO_AUTO_RESP));
			gaim_prefs_set_bool("/gaim/gtk/away/queue_messages",
					away_options & OPT_AWAY_QUEUE);
			gaim_prefs_set_bool("/core/away/auto_response/idle_only",
					away_options & OPT_AWAY_IDLE_RESP);
			gaim_prefs_set_bool("/plugins/gtk/docklet/queue_messages",
					away_options & OPT_AWAY_QUEUE_UNREAD);
			gaim_prefs_set_bool("/core/away/auto_response/in_active_conv",
					!(away_options & OPT_AWAY_DELAY_IN_USE));
			gaim_prefs_set_int("/core/away/auto_response/sec_before_resend",
					atoi(p->value[1]));
		} else if (!strcmp(p->option, "font_face")) {
			gaim_prefs_set_string("/gaim/gtk/conversations/font_face",
								  p->value[0]);
		} else if (!strcmp(p->option, "font_size")) {
			gaim_prefs_set_int("/gaim/gtk/conversations/font_size", atoi(p->value[0]));
		} else if (!strcmp(p->option, "foreground")) {
			char buf[14];

			g_snprintf(buf, sizeof(buf), "#%04x%04x%04x",
					   atoi(p->value[0]), atoi(p->value[1]), atoi(p->value[2]));
			gaim_prefs_set_string("/gaim/gtk/conversations/fgcolor", buf);

		} else if (!strcmp(p->option, "background")) {
			char buf[14];

			g_snprintf(buf, sizeof(buf), "#%04x%04x%04x",
					   atoi(p->value[0]), atoi(p->value[1]), atoi(p->value[2]));
			gaim_prefs_set_string("/gaim/gtk/conversations/bgcolor", buf);

		} else if (!strcmp(p->option, "report_idle")) {
			switch(atoi(p->value[0])) {
				case IDLE_SCREENSAVER:
					gaim_prefs_set_string("/gaim/gtk/idle/reporting_method",
							"system");
					break;
				case IDLE_GAIM:
					gaim_prefs_set_string("/gaim/gtk/idle/reporting_method",
							"gaim");
					break;
				default:
					gaim_prefs_set_string("/gaim/gtk/idle/reporting_method",
							"none");
					break;
			}
		} else if (!strcmp(p->option, "web_browser")) {
			switch(atoi(p->value[0])) {
				case BROWSER_NETSCAPE:
					gaim_prefs_set_string("/gaim/gtk/browsers/browser",
							"netscape");
					break;
				case BROWSER_KONQ:
					gaim_prefs_set_string("/gaim/gtk/browsers/browser",
							"kfmclient");
					break;
				case BROWSER_MANUAL:
					gaim_prefs_set_string("/gaim/gtk/browsers/browser",
							"manual");
					break;
				case BROWSER_GNOME:
					gaim_prefs_set_string("/gaim/gtk/browsers/browser",
							"gnome");
					break;
				case BROWSER_OPERA:
					gaim_prefs_set_string("/gaim/gtk/browsers/browser",
							"opera");
					break;
				case BROWSER_GALEON:
					gaim_prefs_set_string("/gaim/gtk/browsers/browser",
							"galeon");
					break;
				case BROWSER_MOZILLA:
				default:
					gaim_prefs_set_string("/gaim/gtk/browsers/browser",
							"mozilla");
					break;
			}
		} else if (!strcmp(p->option, "web_command")) {
			gaim_prefs_set_string("/gaim/gtk/browsers/command", p->value[0]);
		} else if (!strcmp(p->option, "smiley_theme")) {
			gaim_prefs_set_string("/gaim/gtk/smileys/theme", p->value[0]);
		} else if (!strcmp(p->option, "conv_size")) {
			gaim_prefs_set_int("/gaim/gtk/conversations/im/default_width",
					atoi(p->value[0]));
			gaim_prefs_set_int("/gaim/gtk/conversations/im/default_height",
					atoi(p->value[1]));
			gaim_prefs_set_int("/gaim/gtk/conversations/im/entry_right",
					atoi(p->value[2]));
		} else if (!strcmp(p->option, "buddy_chat_size")) {
			gaim_prefs_set_int("/gaim/gtk/conversations/chat/default_width",
					atoi(p->value[0]));
			gaim_prefs_set_int("/gaim/gtk/conversations/chat/default_height",
					atoi(p->value[1]));
			gaim_prefs_set_int("/gaim/gtk/conversations/chat/entry_right",
					atoi(p->value[2]));
		} else if (!strcmp(p->option, "blist_pos")) {
			gaim_prefs_set_int("/gaim/gtk/blist/x", atoi(p->value[0]));
			gaim_prefs_set_int("/gaim/gtk/blist/y", atoi(p->value[1]));
			gaim_prefs_set_int("/gaim/gtk/blist/width", atoi(p->value[2]));
			gaim_prefs_set_int("/gaim/gtk/blist/height", atoi(p->value[3]));
		} else if (!strcmp(p->option, "sort_method")) {
			gaim_prefs_set_string("/gaim/gtk/blist/sort_type", p->value[0]);
		}

	}

	if (read_general) {
		if (!read_logging) {
			logging_options = 0;
			gaim_prefs_set_bool("/gaim/gtk/logging/log_ims",
					general_options & OPT_GEN_LOG_ALL);
			gaim_prefs_set_bool("/gaim/gtk/logging/log_chats",
					general_options & OPT_GEN_LOG_ALL);
			gaim_prefs_set_bool("/gaim/gtk/logging/strip_html",
					general_options & OPT_GEN_STRIP_HTML);
		}
	}

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

static void gaimrc_read_sounds(FILE *f)
{
	int i;
	char buf[2048];
	struct parse parse_buffer;
	struct parse *p;
	char *pref_name;

	buf[0] = 0;

	for(i=0; i<GAIM_NUM_SOUNDS; i++) {
		pref_name = g_strdup_printf("/gaim/gtk/sound/file/%s",
				gaim_gtk_sound_get_event_option(i));
		gaim_prefs_set_string(pref_name, "");
		g_free(pref_name);
	}

	while (buf[0] != '}') {
		if (buf[0] == '#')
			continue;

		if (!fgets(buf, sizeof(buf), f))
			return;

		p = parse_line(buf, &parse_buffer);
#ifndef _WIN32
		if (!strcmp(p->option, "sound_cmd")) {
			gaim_prefs_set_string("/gaim/gtk/sound/command", p->value[0]);
		} else
#endif
		if (!strncmp(p->option, "sound", strlen("sound"))) {
			i = p->option[strlen("sound")] - 'A';

			pref_name = g_strdup_printf("/gaim/gtk/sound/file/%s",
					gaim_gtk_sound_get_event_option(i));
			gaim_prefs_set_string(pref_name, p->value[0]);
		}
	}
}

static gboolean gaimrc_parse_proxy_uri(const char *proxy)
{
	GaimProxyInfo *info;

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

	if (strncmp(proxy, "http://", len + 3))
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
	info = gaim_global_proxy_get_info();

	if (*host) gaim_proxy_info_set_host(info, host);
	if (*user) gaim_proxy_info_set_username(info, user);
	if (*pass) gaim_proxy_info_set_password(info, pass);

	gaim_proxy_info_set_port(info, port);

	gaim_debug(GAIM_DEBUG_MISC, "gaimrc",
			   "Host: '%s', User: '%s', Password: '%s', Port: %d\n",
			   gaim_proxy_info_get_host(info),
			   gaim_proxy_info_get_username(info),
			   gaim_proxy_info_get_password(info),
			   gaim_proxy_info_get_port(info));

	return TRUE;
}

static void gaimrc_read_proxy(FILE *f)
{
	char buf[2048];
	struct parse parse_buffer;
	struct parse *p;
	GaimProxyInfo *info;

	info = gaim_global_proxy_get_info();

	buf[0] = 0;
	gaim_debug(GAIM_DEBUG_MISC, "gaimrc", "gaimrc_read_proxy\n");

	while (buf[0] != '}') {
		if (buf[0] == '#')
			continue;

		if (!fgets(buf, sizeof(buf), f))
			return;

		p = parse_line(buf, &parse_buffer);

		if (!strcmp(p->option, "host")) {
			gaim_proxy_info_set_host(info, p->value[0]);
			gaim_debug(GAIM_DEBUG_MISC, "gaimrc",
					   "Set proxyhost %s\n", p->value[0]);
		} else if (!strcmp(p->option, "port")) {
			gaim_proxy_info_set_port(info, atoi(p->value[0]));
		} else if (!strcmp(p->option, "type")) {
			gaim_proxy_info_set_type(info, atoi(p->value[0]));
		} else if (!strcmp(p->option, "user")) {
			gaim_proxy_info_set_username(info, p->value[0]);
		} else if (!strcmp(p->option, "pass")) {
			gaim_proxy_info_set_password(info, p->value[0]);
		}
	}

	if (gaim_proxy_info_get_host(info) != NULL)
		gaim_global_proxy_set_from_prefs(TRUE);
	else {
		const char *host;
		gboolean getVars = TRUE;

		if ((host = g_getenv("HTTP_PROXY")) != NULL ||
			(host = g_getenv("http_proxy")) != NULL ||
			(host = g_getenv("HTTPPROXY")) != NULL) {

			gaim_proxy_info_set_host(info, host);
		}

		if (gaim_proxy_info_get_host(info) != NULL)
			getVars = !gaimrc_parse_proxy_uri(gaim_proxy_info_get_host(info));

		if (getVars)
		{
			const char *port_str, *user, *pass;

			if ((port_str = g_getenv("HTTP_PROXY_PORT")) != NULL ||
				(port_str = g_getenv("http_proxy_port")) != NULL ||
				(port_str = g_getenv("HTTPPROXYPORT")) != NULL) {

				gaim_proxy_info_set_port(info, atoi(port_str));
			}

			if ((user = g_getenv("HTTP_PROXY_USER")) != NULL ||
				(user = g_getenv("http_proxy_user")) != NULL ||
				(user = g_getenv("HTTPPROXYUSER")) != NULL) {

				gaim_proxy_info_set_username(info, user);
			}

			if ((pass = g_getenv("HTTP_PROXY_PASS")) != NULL ||
				(pass = g_getenv("http_proxy_pass")) != NULL ||
				(pass = g_getenv("HTTPPROXYPASS")) != NULL) {

				gaim_proxy_info_set_password(info, pass);
			}
		}
	}
}

static void set_defaults()
{
#if 0
	int i;
	struct away_message *a;

	misc_options =
		OPT_MISC_USE_SERVER_ALIAS;

	logging_options = 0;

	blist_options =
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
#endif
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
				/* Do nothing--either EOF or empty line */
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
			request_save_prefs = 0;
		}
	} else if (opt_rcfile_arg) {
		g_snprintf(buf, sizeof(buf), _("Could not open config file %s."), opt_rcfile_arg);
		gaim_notify_error(NULL, NULL, buf, NULL);
		set_defaults();
	} else {
		set_defaults();
	}

	prefs_initial_load = 1;
}

void save_prefs()
{
	gaim_debug(GAIM_DEBUG_INFO, "gaimrc", "save_prefs() called. Rejected!\n");
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
	GaimPounce *pounce;
	GaimAccount *account;

	for (l = buddy_pounces; l != NULL; l = l->next) {
		GaimPounceEvent events = GAIM_POUNCE_NONE;
		GaimGtkPounceAction actions = GAIM_GTKPOUNCE_NONE;
		ph = (struct pounce_placeholder *)l->data;

		account = gaim_accounts_find(ph->pouncer, ph->protocol);

		old_pounce_opts_to_new(ph->options, &events, &actions);

		pounce = gaim_gtkpounce_new(account, ph->name, events);

		gaim_pounce_action_set_enabled(pounce, "open-window",
			(actions & GAIM_GTKPOUNCE_OPEN_WIN));
		gaim_pounce_action_set_enabled(pounce, "popup-notify",
			(actions & GAIM_GTKPOUNCE_POPUP));
		gaim_pounce_action_set_enabled(pounce, "send-message",
			(actions & GAIM_GTKPOUNCE_SEND_MSG));
		gaim_pounce_action_set_enabled(pounce, "execute-command",
			(actions & GAIM_GTKPOUNCE_EXEC_CMD));
		gaim_pounce_action_set_enabled(pounce, "play-sound",
			(actions & GAIM_GTKPOUNCE_PLAY_SOUND));

		gaim_pounce_action_set_attribute(pounce, "send-message", "message",
			(*ph->message == '\0' ? NULL : ph->message));
		gaim_pounce_action_set_attribute(pounce, "execute-command", "command",
			(*ph->sound == '\0' ? NULL : ph->message));
		gaim_pounce_action_set_attribute(pounce, "play-sound", "filename",
			(*ph->sound == '\0' ? NULL : ph->message));

		gaim_pounce_set_save(pounce, (ph->options & 0x100));

		g_free(ph);
	}

	g_list_free(buddy_pounces);
	buddy_pounces = NULL;

	/* 
	 * < ChipX86|Coding> why do we save prefs just after reading them?
	 * <      faceprint> ChipX86|Coding: because we're cool like that
	 * <SeanEgan|Coding> damn straight
	 */
	/* save_prefs(); -- I like the above comment :( */
}
