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

#ifndef _GAIM_H_
#define _GAIM_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "core.h"
#include "blist.h"
#include "ui.h"
#include "util.h"

#define XPATCH BAD /* Because Kalla Said So */

/* XXX CUI: when this is done being split, the only things below should be things
 * both the core and the uis depend on e.g. the protocol definitions, etc, and
 * it won't include core.h or ui.h (i.e. it'll mostly be #define's) */

/* this is the basis of the CUI protocol. */
#define CUI_TYPE_META		1
#define CUI_TYPE_PLUGIN		2
#define CUI_TYPE_USER		3
#define CUI_TYPE_CONN		4
#define CUI_TYPE_BUDDY		5	/* BUDDY_LIST, i.e., both groups and buddies */
#define CUI_TYPE_MESSAGE	6
#define CUI_TYPE_CHAT		7
#define CUI_TYPE_REMOTE         8       
					/* This is used to send commands to other UI's, 
					 * like "Open new conversation" or "send IM".
					 * Even though there's much redundancy with the
					 * other CUI_TYPES, we're better keeping this stuff
					 * seperate because it's intended use is so different */

#define CUI_META_LIST		1	
					/* 1 is always list; this is ignored by the core.
					   If we move to TCP this can be a keepalive */
#define CUI_META_QUIT		2
#define CUI_META_DETACH		3	
					/* you don't need to send this, you can just close
					   the socket. the core will understand. */
#define CUI_META_PING           4
#define CUI_META_ACK            5

#define CUI_PLUGIN_LIST		1
#define CUI_PLUGIN_LOAD		2
#define CUI_PLUGIN_UNLOAD	3

#define CUI_USER_LIST		1
#define CUI_USER_ADD		2
#define CUI_USER_REMOVE		3
#define CUI_USER_MODIFY		4	/* this handles moving them in the list too */
#define CUI_USER_SIGNON		5

#define CUI_CONN_LIST		1
#define CUI_CONN_PROGRESS	2
#define CUI_CONN_ONLINE		3
#define CUI_CONN_OFFLINE	4	/* this may send a "reason" for why it was killed */

#define CUI_BUDDY_LIST		1
#define CUI_BUDDY_STATE		2	
					/* notifies the UI of state changes; UI can use it to
					   request the current status from the core */
#define CUI_BUDDY_ADD		3
#define CUI_BUDDY_REMOVE	4
#define CUI_BUDDY_MODIFY	5

#define CUI_MESSAGE_LIST	1	/* no idea */
#define CUI_MESSAGE_SEND	2
#define CUI_MESSAGE_RECV	3

#define CUI_CHAT_LIST		1
#define CUI_CHAT_HISTORY	2	/* is this necessary? should we have one for IMs? */
#define CUI_CHAT_JOIN		3	/* handles other people joining/parting too */
#define CUI_CHAT_PART		4
#define CUI_CHAT_SEND		5
#define CUI_CHAT_RECV		6

#define CUI_REMOTE_CONNECTIONS  2       /* Get a list of gaim_connections */
#define CUI_REMOTE_URI          3       /* Have the core handle aim:// URI's */
#define CUI_REMOTE_BLIST        4       /* Return a copy of the buddy list */
#define CUI_REMOTE_STATE        5       /* Given a buddy, return his presence. */
#define CUI_REMOTE_NEW_CONVO    6       /* Must give a user, can give an optional message */
#define CUI_REMOTE_SEND         7       /* Sends a message, a 'quiet' flag determines whether
					 * a convo window is displayed or not. */
#define CUI_REMOTE_ADD_BUDDY    8       /* Adds buddy to list */
#define CUI_REMOTE_REMOVE_BUDDY 9       /* Removes buddy from list */
#define CUI_REMOTE_JOIN_CHAT    10       /* Joins a chat. */
                              /* What else?? */


#define IM_FLAG_AWAY     0x01
#define IM_FLAG_CHECKBOX 0x02
#define IM_FLAG_GAIMUSER 0x04

#define PERMIT_ALL	1
#define PERMIT_NONE	2
#define PERMIT_SOME	3
#define DENY_SOME	4

#define NOT_TYPING 0
#define TYPING     1
#define TYPED      2

#define WFLAG_SEND	0x01
#define WFLAG_RECV	0x02
#define WFLAG_AUTO	0x04
#define WFLAG_WHISPER	0x08
#define WFLAG_FILERECV	0x10
#define WFLAG_SYSTEM	0x20
#define WFLAG_NICK	0x40
#define WFLAG_NOLOG	0x80
#define WFLAG_COLORIZE  0x100

#define AUTO_RESPONSE "&lt;AUTO-REPLY&gt; : "

#define WEBSITE "http://gaim.sourceforge.net/"

#ifdef ENABLE_NLS
#  include <libintl.h>
#  define _(x) gettext(x)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define N_(String) (String)
#  define _(x) (x)
#endif

#define DEFAULT_INFO "Visit the Gaim website at <A HREF=\"http://gaim.sourceforge.net/\">http://gaim.sourceforge.net/</A>."

enum log_event {
	log_signon = 0,
	log_signoff,
	log_away,
	log_back,
	log_idle,
	log_unidle,
	log_quit
};

#define OPT_POUNCE_POPUP	0x001
#define OPT_POUNCE_SEND_IM	0x002
#define OPT_POUNCE_COMMAND	0x004
#define OPT_POUNCE_SOUND	0x008

#define OPT_POUNCE_SIGNON	0x010
#define OPT_POUNCE_UNAWAY	0x020
#define OPT_POUNCE_UNIDLE	0x040
#define OPT_POUNCE_TYPING       0x080
#define OPT_POUNCE_SAVE		0x100

#define OPT_POUNCE_NOTIFY	0x200

/* These should all be runtime selectable */

#define MSG_LEN 2048
/* The above should normally be the same as BUF_LEN,
 * but just so we're explictly asking for the max message
 * length. */
#define BUF_LEN MSG_LEN
#define BUF_LONG BUF_LEN * 2

/* Globals in main.c */
extern int opt_away;
extern char *opt_away_arg;
extern char *opt_rcfile_arg;
extern int opt_debug;

extern GSList *message_queue;
extern GSList *unread_message_queue;
extern GSList *away_time_queue;

/* Functions in main.c */
extern void do_quit();

/* Functions in dialogs.c */
extern void g_show_info_text(GaimConnection *, const char *, int, const char *, ...);
extern void show_change_passwd(GaimConnection *);
extern void show_set_dir(GaimConnection *);
extern void show_find_email(GaimConnection *);
extern void show_find_info(GaimConnection *);
extern void show_set_info(GaimConnection *);
extern void show_confirm_del(GaimConnection *, gchar *);
extern void show_confirm_del_group(struct group *);
extern void show_confirm_del_chat(struct chat *);

/* Functions in gaimrc.c */
extern gint sort_awaymsg_list(gconstpointer, gconstpointer);

/* Functions in html.c */
struct g_url {
	char address[255];
	int port;
	char page[255];
};

extern void grab_url(char *, gboolean, void (*callback)(gpointer, char *, unsigned long), gpointer);
extern gchar *strip_html(const gchar *);
extern void html_to_xhtml(const char *, char **, char **);
struct g_url *parse_url(char *url);

/* Functions in idle.c */
extern gint check_idle(gpointer);

/* Functions in server.c */
/* input to serv */
extern void serv_login(GaimAccount *);
extern void serv_close(GaimConnection *);
extern void serv_touch_idle(GaimConnection *);
extern int  serv_send_im(GaimConnection *, char *, char *, int, int);
extern void serv_get_info(GaimConnection *, char *);
extern void serv_get_dir(GaimConnection *, char *);
extern void serv_set_idle(GaimConnection *, int);
extern void serv_set_info(GaimConnection *, char *);
extern void serv_set_away(GaimConnection *, char *, char *);
extern void serv_set_away_all(char *);
extern int  serv_send_typing(GaimConnection *, char *, int);
extern void serv_change_passwd(GaimConnection *, const char *, const char *);
extern void serv_add_buddy(GaimConnection *, const char *);
extern void serv_add_buddies(GaimConnection *, GList *);
extern void serv_remove_buddy(GaimConnection *, char *, char *);
extern void serv_remove_buddies(GaimConnection *, GList *, char *);
extern void serv_add_permit(GaimConnection *, const char *);
extern void serv_add_deny(GaimConnection *, const char *);
extern void serv_rem_permit(GaimConnection *, const char *);
extern void serv_rem_deny(GaimConnection *, const char *);
extern void serv_set_permit_deny(GaimConnection *);
extern void serv_warn(GaimConnection *, char *, int);
extern void serv_set_dir(GaimConnection *, const char *, const char *, const char *, const char *, const char *, const char *, const char *, int);
extern void serv_dir_search(GaimConnection *, const char *, const char *, const char *, const char *, const char *, const char *, const char *, const char *);
extern void serv_join_chat(GaimConnection *, GHashTable *);
extern void serv_chat_invite(GaimConnection *, int, const char *, const char *);
extern void serv_chat_leave(GaimConnection *, int);
extern void serv_chat_whisper(GaimConnection *, int, char *, char *);
extern int  serv_chat_send(GaimConnection *, int, char *);
extern void serv_got_popup(char *, char *, int, int);
extern void serv_get_away(GaimConnection *, const char *);
extern void serv_alias_buddy(struct buddy *);
extern void serv_move_buddy(struct buddy *, struct group *, struct group *);
extern void serv_rename_group(GaimConnection *, struct group *, const char *);

/* Functions in log.h */
extern FILE *open_log_file (const char *, int);
extern void system_log(enum log_event, GaimConnection *, struct buddy *, int);
extern void rm_log(struct log_conversation *);
extern struct log_conversation *find_log_info(const char *);
extern void update_log_convs();
extern void save_convo(GtkWidget *save, GaimConversation *c);
extern char *html_logize(const char *p);

/*------------------------------------------------------------------------*/
/*  Multi-Entry dialog and vCard dialog support                           */
/*------------------------------------------------------------------------*/

/*
 * Struct for "instructions" dialog data
 */
typedef struct multi_instr_dlg {
	GtkWidget *label;		/* dialog instructions widget */
	gchar *text;			/* dialog instructions */
} MultiInstrData;

/*
 * Struct for multiple-entry dialog data
 */
typedef struct multi_entry_data {
	GtkWidget *widget;		/* entry widget object */
	char *label;			/* label text pointer */
	char *text;			/* entry text pointer */
	int  visible;			/* should entry field be "visible?" */
	int  editable;			/* should entry field be editable? */
} MultiEntryData;

/*
 * Struct for multiple-textbox dialog data
 */
typedef struct multi_text_data {
	char *label;			/* frame label */
	GtkWidget *textbox;		/* text entry widget object */
	char *text;			/* textbox text pointer */
} MultiTextData;

/*
 * Struct to create a multi-entry dialog
 */
typedef struct multi_entry_dlg {
	GtkWidget *window;			/* dialog main window */
	gchar *role;				/* window role */
	char *title;				/* window title */

	GaimAccount *account;			/* user info - needed for most everything */

	MultiInstrData *instructions;		/* instructions (what else?) */

	GtkWidget *entries_table;		/* table widget containing m-e lables & entries */
	GtkWidget *entries_frame;		/* frame widget containing the table widget */
	gchar *entries_title;			/* title of multi-entries list */
	GSList *multi_entry_items;		/* entry dialogs parameters */

	GtkWidget *texts_ibox;			/* inner vbox containing multi-text frames */
	GtkWidget *texts_obox;			/* outer vbox containing multi-text frames */
	GSList *multi_text_items;		/* text dialogs parameters */

	void * (*custom)(struct multi_entry_dlg *);	/* Custom function that may be used by */
							/* multi-entry dialog "wrapper" functions */
							/* (Not used by multi-entry dialog routines) */

	void (*ok)(GtkWidget *, gpointer);	/* "Save/OK" action */
	void (*cancel)(GtkWidget *, gpointer);	/* "Cancel" action */
} MultiEntryDlg;

extern MultiTextData *multi_text_list_update(GSList **, const char *, const char *, int);
extern void multi_text_items_free_all(GSList **);
extern MultiEntryData *multi_entry_list_update(GSList **, const char *, const char *, int);
extern void multi_entry_items_free_all(GSList **);

extern void re_show_multi_entry_instr(MultiInstrData *);
extern void re_show_multi_entry_entries(GtkWidget **, GtkWidget *, GSList *);
extern void re_show_multi_entry_textboxes(GtkWidget **, GtkWidget *, GSList *);

extern MultiEntryDlg *multi_entry_dialog_new(void);
extern void show_multi_entry_dialog(gpointer);

extern void show_set_vcard(MultiEntryDlg *);

/*------------------------------------------------------------------------*/
/*  End Multi-Entry dialog and vCard dialog support                       */
/*------------------------------------------------------------------------*/

#endif /* _GAIM_H_ */
