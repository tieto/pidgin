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
#include "ui.h"

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

#define IDLE_NONE        0
#define IDLE_GAIM        1
#define IDLE_SCREENSAVER 2

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

#define OPT_USR_AUTO		0x00000001
/*#define OPT_USR_KEEPALV	0x00000002 this shouldn't be optional */
#define OPT_USR_REM_PASS	0x00000004
#define OPT_USR_MAIL_CHECK      0x00000008
/*do not use OPT_USR		0x00000010  talk to robot101 about automatic
 						name stuff with this option
 */

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

#define CONVERSATION_TITLE "%s - Gaim"
#define LOG_CONVERSATION_TITLE "%s - Gaim (logged)"

/* These should all be runtime selectable */

#define MSG_LEN 2048
/* The above should normally be the same as BUF_LEN,
 * but just so we're explictly asking for the max message
 * length. */
#define BUF_LEN MSG_LEN
#define BUF_LONG BUF_LEN * 2

/* Globals in aim.c */
extern GList *buddy_pounces;
extern GList *conversations;
extern int opt_away;
extern char *opt_away_arg;
extern char *opt_rcfile_arg;
extern int opt_debug;

extern guint misc_options;
#define OPT_MISC_DEBUG			0x00000001
#define OPT_MISC_BROWSER_POPUP		0x00000002
#define OPT_MISC_BUDDY_TICKER           0x00000004
#define OPT_MISC_STEALTH_TYPING         0x00000010
#define OPT_MISC_USE_SERVER_ALIAS		0x00000020

extern guint logging_options;
#define OPT_LOG_CONVOS			0x00000001
#define OPT_LOG_STRIP_HTML		0x00000002
#define OPT_LOG_BUDDY_SIGNON		0x00000004
#define OPT_LOG_BUDDY_IDLE		0x00000008
#define OPT_LOG_BUDDY_AWAY		0x00000010
#define OPT_LOG_MY_SIGNON		0x00000020
#define OPT_LOG_INDIVIDUAL		0x00000040
#define OPT_LOG_CHATS			0x00000100

extern guint blist_options;
#define OPT_BLIST_APP_BUDDY_SHOW	0x00000001
#define OPT_BLIST_SAVED_WINDOWS		0x00000002
#define OPT_BLIST_NEAR_APPLET		0x00000004
#define OPT_BLIST_SHOW_GRPNUM		0x00000008
#define OPT_BLIST_SHOW_PIXMAPS		0x00000010
#define OPT_BLIST_SHOW_IDLETIME		0x00000020
#define OPT_BLIST_SHOW_BUTTON_XPM	0x00000040
#define OPT_BLIST_NO_BUTTONS		0x00000080
#define OPT_BLIST_NO_MT_GRP		0x00000100
#define OPT_BLIST_SHOW_WARN		0x00000200
#define OPT_BLIST_GREY_IDLERS		0x00000400
#define OPT_BLIST_BOTTOM_TAB		0X00000800
#define OPT_BLIST_POPUP                 0x00001000

extern guint convo_options;
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

extern guint im_options;
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

extern guint chat_options;
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

extern guint font_options;
#define OPT_FONT_BOLD			0x00000001
#define OPT_FONT_ITALIC			0x00000002
#define OPT_FONT_UNDERLINE		0x00000008
#define OPT_FONT_STRIKE			0x00000010
#define OPT_FONT_FACE			0x00000020
#define OPT_FONT_FGCOL			0x00000040
#define OPT_FONT_BGCOL			0x00000080
#define OPT_FONT_SIZE			0x00000100

extern guint sound_options;
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
#define OPT_SOUND_ARTSC			0x00008000
#define OPT_SOUND_ESD			0x00010000
#define OPT_SOUND_CMD			0x00020000
#define OPT_SOUND_CHAT_NICK             0x00040000
/* remember to also change the struct in sound.c */

#define SND_BUDDY_ARRIVE 0
#define SND_BUDDY_LEAVE 1
#define SND_RECEIVE 2
#define SND_FIRST_RECEIVE 3
#define SND_SEND 4
#define SND_CHAT_JOIN 5
#define SND_CHAT_LEAVE 6
#define SND_CHAT_YOU_SAY 7
#define SND_CHAT_SAY 8
#define SND_POUNCE_DEFAULT 9
#define SND_CHAT_NICK 10
#define NUM_SOUNDS 11

extern char *sound_file[NUM_SOUNDS];

/* global sound struct */
struct sound_struct {
	char *label;
	guint opt;
	char *def;
};
extern struct sound_struct sounds[];

extern guint away_options;
#define OPT_AWAY_DISCARD		0x00000001
#define OPT_AWAY_BACK_ON_IM		0x00000002
/* #define OPT_AWAY_TIK_HACK		0x00000004 */
#define OPT_AWAY_AUTO			0x00000008
#define OPT_AWAY_NO_AUTO_RESP		0x00000010
#define OPT_AWAY_QUEUE			0x00000020
#define OPT_AWAY_IDLE_RESP		0x00000040
#define OPT_AWAY_QUEUE_UNREAD           0x00000080
#define OPT_AWAY_DELAY_IN_USE		0x00000100

extern guint away_resend;
extern int report_idle;
extern int web_browser;
extern GSList *aim_users;
extern GSList *message_queue;
extern GSList *unread_message_queue;
extern GSList *away_time_queue;
extern char sound_cmd[2048];
extern char web_command[2048];
extern struct save_pos blist_pos;
extern struct window_size conv_size, buddy_chat_size;

/* Functions in aim.c */
extern void do_quit();

/* Functions in buddy.c */
extern void signoff(struct gaim_connection *);
extern void do_pounce(struct gaim_connection *, char *, int);
void create_prpl_icon(GtkWidget *widget, struct gaim_connection *gc,
					  GdkPixmap **pixmap, GdkBitmap **mask);

/* Functions in buddy_chat.c */
extern void show_new_buddy_chat(struct conversation *);
extern void chat_set_topic(struct conversation*, char*, char*);
/*extern void add_chat_buddy(struct conversation *, char *);*/
extern void add_chat_buddy(struct conversation *, char *, char *);
extern void rename_chat_buddy(struct conversation *, char *, char *);
extern void remove_chat_buddy(struct conversation *, char *, char *);

/* Functions in conversation.c */
extern void write_to_conv(struct conversation *, char *, int, char *, time_t, int);
extern struct conversation *find_conversation(const char *);
extern void set_anim();
extern void set_blist_tab();

/* Functions in dialogs.c */
extern void g_show_info_text(struct gaim_connection *, char *, int, char *, ...);
extern GtkWidget *do_error_dialog(const char *, const char *, int type);
extern void show_change_passwd(struct gaim_connection *);
extern void show_set_dir(struct gaim_connection *);
extern void show_find_email(struct gaim_connection *);
extern void show_find_info(struct gaim_connection *);
extern void show_set_info(struct gaim_connection *);
extern void show_confirm_del(struct gaim_connection *, gchar *);

/* Functions in gaimrc.c */
extern gint sort_awaymsg_list(gconstpointer, gconstpointer);

/* Functions in html.c */
struct g_url {
	char address[255];
	int port;
	char page[255];
};

extern void grab_url(char *, gboolean, void (*callback)(gpointer, char *), gpointer);
extern gchar *strip_html(gchar *);
struct g_url *parse_url(char *url);

/* Functions in idle.c */
extern gint check_idle(gpointer);

/* Functions in prefs.c */
extern void debug_printf(char * fmt, ...) G_GNUC_PRINTF(1,2);

/* Functions in server.c */
/* input to serv */
extern void serv_login(struct aim_user *);
extern void serv_close(struct gaim_connection *);
extern void serv_touch_idle(struct gaim_connection *);
extern int  serv_send_im(struct gaim_connection *, char *, char *, int, int);
extern void serv_get_info(struct gaim_connection *, char *);
extern void serv_get_dir(struct gaim_connection *, char *);
extern void serv_set_idle(struct gaim_connection *, int);
extern void serv_set_info(struct gaim_connection *, char *);
extern void serv_set_away(struct gaim_connection *, char *, char *);
extern void serv_set_away_all(char *);
extern int  serv_send_typing(struct gaim_connection *, char *, int);
extern void serv_change_passwd(struct gaim_connection *, const char *, const char *);
extern void serv_add_buddy(struct gaim_connection *, const char *);
extern void serv_add_buddies(struct gaim_connection *, GList *);
extern void serv_remove_buddy(struct gaim_connection *, char *, char *);
extern void serv_remove_buddies(struct gaim_connection *, GList *, char *);
extern void serv_add_permit(struct gaim_connection *, char *);
extern void serv_add_deny(struct gaim_connection *, char *);
extern void serv_rem_permit(struct gaim_connection *, char *);
extern void serv_rem_deny(struct gaim_connection *, char *);
extern void serv_set_permit_deny(struct gaim_connection *);
extern void serv_warn(struct gaim_connection *, char *, int);
extern void serv_set_dir(struct gaim_connection *, const char *, const char *, const char *, const char *, const char *, const char *, const char *, int);
extern void serv_dir_search(struct gaim_connection *, const char *, const char *, const char *, const char *, const char *, const char *, const char *, const char *);
extern void serv_join_chat(struct gaim_connection *, GList *);
extern void serv_chat_invite(struct gaim_connection *, int, const char *, const char *);
extern void serv_chat_leave(struct gaim_connection *, int);
extern void serv_chat_whisper(struct gaim_connection *, int, char *, char *);
extern int  serv_chat_send(struct gaim_connection *, int, char *);
extern void serv_got_popup(char *, char *, int, int);
extern void serv_get_away(struct gaim_connection *, char *);
extern void serv_alias_buddy(struct buddy *);
extern void serv_move_buddy(struct buddy *, struct group *, struct group *);
extern void serv_rename_group(struct gaim_connection *, struct group *, const char *);

/* Functions in util.c */
extern char *normalize(const char *);
extern char *tobase64(const char *);
extern void frombase64(const char *, char **, int *);
extern void clean_pid();
extern char *date();
extern gint linkify_text(char *);
extern char *sec_to_text(guint);
extern struct aim_user *find_user(const char *, int) G_GNUC_PURE;
extern char *full_date() G_GNUC_PURE;
extern void check_gaim_versions();
extern char *away_subs(char *, char *);
extern char *stylize(gchar *, int);
extern void show_usage (int, char *);
extern const gchar *gaim_home_dir();
extern char *gaim_user_dir();
extern void strncpy_nohtml(gchar *, const gchar *, size_t);
extern void strncpy_withhtml(gchar *, const gchar *, size_t);
extern gchar *strdup_withhtml(const gchar *);
extern void away_on_login(char *);
extern char *add_cr(char *);
extern void strip_linefeed(char *);
extern time_t get_time(int, int, int, int, int, int) G_GNUC_CONST;
extern FILE *gaim_mkstemp(gchar **);
extern char *convert_string(char *, const char *, const char *);
extern const char *handle_uri(char *);

/* Functions in log.h */
extern FILE *open_log_file (const char *, int);
extern void system_log(enum log_event, struct gaim_connection *, struct buddy *, int);
extern void rm_log(struct log_conversation *);
extern struct log_conversation *find_log_info(const char *);
extern void update_log_convs();
extern void save_convo(GtkWidget *save, struct conversation *c);
extern char *html_logize(char *p);

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

	struct aim_user *user;			/* user info - needed for most everything */

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
