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
#include <gtk/gtk.h>
#include <time.h>
#include <stdio.h>
#include <aim.h>
#ifdef USE_APPLET
#include <applet-widget.h>
#endif /* USE_APPLET */
#ifdef USE_GNOME
#include <gnome.h>
#endif


/*
	1.  gethostbyname();
	2.  connect();
	3.  toc_signon();
	4.  toc_wait_signon();
	5.  toc_wait_config();
	6.  actually done..
*/

#define STATE_OFFLINE 0
#define STATE_FLAPON 1
#define STATE_SIGNON_REQUEST 2
#define STATE_SIGNON_ACK 3
#define STATE_CONFIG 4
#define STATE_ONLINE 5

#define BROWSER_NETSCAPE              0
#define BROWSER_KFM                   1
#define BROWSER_MANUAL                2
#define BROWSER_INTERNAL              3

#define PERMIT_ALL	1
#define PERMIT_NONE	2
#define PERMIT_SOME	3
#define DENY_SOME	4
#define PERMIT_BUDDY	5 /* TOC doesn't have this,
			     but we can fake it */

#define UC_AOL		1
#define UC_ADMIN 	2
#define UC_UNCONFIRMED	4
#define UC_NORMAL	8
#define UC_UNAVAILABLE  16

#define IDLE_NONE       0
#define IDLE_GAIM       1
#define IDLE_SYSTEM     2

#define WFLAG_SEND 1
#define WFLAG_RECV 2
#define WFLAG_AUTO 4
#define WFLAG_WHISPER 8
#define WFLAG_FILERECV 16
#define WFLAG_SYSTEM 32

#define AUTO_RESPONSE "<AUTO-REPLY> : "

#define PLUGIN_DIR ".gaim/plugins/"

#define REG_EMAIL_ADDR "gaiminfo@blueridge.net"
#define REG_SRVR "blueridge.net"
#define REG_PORT 25

#define FACE_ANGEL 0
#define FACE_BIGSMILE 1
#define FACE_BURP 2
#define FACE_CROSSEDLIPS 3
#define FACE_CRY 4
#define FACE_EMBARRASSED 5
#define FACE_KISS 6
#define FACE_MONEYMOUTH 7
#define FACE_SAD 8
#define FACE_SCREAM 9
#define FACE_SMILE 10
#define FACE_SMILE8 11
#define FACE_THINK 12
#define FACE_TONGUE 13
#define FACE_WINK 14
#define FACE_YELL 15
#define FACE_TOTAL 16

#ifndef USE_GNOME
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
#endif

extern struct debug_window *dw;

struct aim_user {
	char username[64];
        char password[32];
        char user_info[2048];
};

struct save_pos {
        int x;
        int y;
        int width;
        int height;
        int xoff;
        int yoff;
};


struct option_set {
        int *options;
        int option;
};

struct g_url {
	char address[255];
	int port;
        char page[255];
};

#ifdef GAIM_PLUGINS
struct gaim_plugin {
	char *name;
	char *filename;
	char *description;
	void *handle;
	int   remove;
};

enum gaim_event {
	event_signon = 0,
	event_signoff,
	event_away,
	event_back,
	event_im_recv,
	event_im_send,
	event_buddy_signon,
	event_buddy_signoff,
	event_buddy_away,
	event_buddy_back,
	event_blist_update,
	event_chat_invited,
	event_chat_join,
	event_chat_leave,
	event_chat_buddy_join,
	event_chat_buddy_leave,
	event_chat_recv,
	event_chat_send,
	event_warned,
	event_error,
	event_quit,
	/* any others? it's easy to add... */
};

struct gaim_callback {
	void *handle;
	enum gaim_event event;
	void *function;
	void *data;
};

extern GList *plugins;
extern GList *callbacks;
#endif

struct buddy {
	char name[80];
	GtkWidget *item;
	GtkWidget *label;
	GtkWidget *pix;
        GtkWidget *idletime;
        int present;
        int log_timer;
	int evil;
	time_t signon;
	time_t idle;
        int uc;
	u_short caps; /* woohoo! */
};

struct log_conversation {
	char name[80];
	char filename[512];
        struct log_conversation *next;
};

struct buddy_pounce {
        char name[80];
        char message[2048];
	int popup;
	int sendim;
};

struct away_message {
	char name[80];
	char message[2048];
};

struct group {
	GtkWidget *item;
        GtkWidget *label;
        GtkWidget *tree;
	char name[80];
	GList *members;
};

struct chat_room {
        char name[128];
        int exchange;
};

struct chat_connection {
	char *name;
	int fd; /* this is redundant since we have the conn below */
	struct aim_conn_t *conn;
	int inpa;
};

struct debug_window {
	GtkWidget *window;
	GtkWidget *entry;
};

/* struct buddy_chat went away and got merged with this. */
struct conversation {
	/* stuff used for both IM and chat */
	GtkWidget *window;
	char name[80];
	GtkWidget *text;
	GtkWidget *entry;
	GtkWidget *italic;
	GtkWidget *bold;
	GtkWidget *underline;
	GtkWidget *palette;
	GtkWidget *link;
	GtkWidget *wood;
	GtkWidget *log_button;
	GtkWidget *strike;
	GtkWidget *font;
	GtkWidget *smiley;
	GtkWidget *color_dialog;
	GtkWidget *font_dialog;
	GtkWidget *smiley_dialog;
	GtkWidget *link_dialog;
	GtkWidget *log_dialog;
	int makesound;

	/* stuff used just for IM */
	GtkWidget *add_button;
 	time_t sent_away;

	/* stuff used just for chat */
        GList *in_room;
        GList *ignored;
        int id;
	GtkWidget *list;

	/* something to distinguish */
	gboolean is_chat;

	/* DirectIM stuff */
	gboolean is_direct;
	struct aim_conn_t *conn; /* needed for Oscar */
	int watcher;
};

struct file_header {
	char  magic[4];		/* 0 */
	short hdrlen;		/* 4 */
	short hdrtype;		/* 6 */
	char  bcookie[8];	/* 8 */
	short encrypt;		/* 16 */
	short compress;		/* 18 */
	short totfiles;		/* 20 */
	short filesleft;	/* 22 */
	short totparts;		/* 24 */
	short partsleft;	/* 26 */
	long  totsize;		/* 28 */
	long  size;		/* 32 */
	long  modtime;		/* 36 */
	long  checksum;		/* 40 */
	long  rfrcsum;		/* 44 */
	long  rfsize;		/* 48 */
	long  cretime;		/* 52 */
	long  rfcsum;		/* 56 */
	long  nrecvd;		/* 60 */
	long  recvcsum;		/* 64 */
	char  idstring[32];	/* 68 */
	char  flags;		/* 100 */
	char  lnameoffset;	/* 101 */
	char  lsizeoffset;	/* 102 */
	char  dummy[69];	/* 103 */
	char  macfileinfo[16];	/* 172 */
	short nencode;		/* 188 */
	short nlanguage;	/* 190 */
	char  name[64];		/* 192 */
				/* 256 */
};

struct file_transfer {
        GtkWidget *window;
        char *cookie;
        char *ip;
        char *message;
        int port;
        int size;
        int accepted;
        char *filename;
        char *lfilename;
        char *user;
        FILE *f;
        int fd;
	char UID[2048];
};

struct sflap_hdr {
	unsigned char ast;
	unsigned char type;
	unsigned short seqno;
	unsigned short len;
};

struct signon {
	unsigned int ver;
	unsigned short tag;
	unsigned short namelen;
	char username[80];
};

#define LOGIN_STEPS 5

#define CONVERSATION_TITLE "Gaim - Conversation with %s"
#define LOG_CONVERSATION_TITLE "Gaim - Conversation with %s (logged)"

#define VOICE_UID     "09461341-4C7F-11D1-8222-444553540000"
#define FILE_SEND_UID "09461343-4C7F-11D1-8222-444553540000"
#define IMAGE_UID     "09461345-4C7F-11D1-8222-444553540000"
#define B_ICON_UID    "09461346-4C7F-11D1-8222-444553540000"
#define FILE_GET_UID  "09461348-4C7F-11D1-8222-444553540000"

#define AOL_SRCHSTR "/community/aimcheck.adp/url="

/* These should all be runtime selectable */

#define TOC_HOST "toc.oscar.aol.com"
#define TOC_PORT 9898
#define AUTH_HOST "login.oscar.aol.com"
#define AUTH_PORT 5190
#define LANGUAGE "english"

#define MSG_LEN 2048
/* The above should normally be the same as BUF_LEN,
 * but just so we're explictly asking for the max message
 * length. */
#define BUF_LEN MSG_LEN
#define BUF_LONG BUF_LEN * 2


#define TYPE_SIGNON    1  
#define TYPE_DATA      2
#define TYPE_ERROR     3
#define TYPE_SIGNOFF   4
#define TYPE_KEEPALIVE 5

#define REVISION "gaim:$Revision: 688 $"
#define FLAPON "FLAPON\r\n\r\n"

#define ROAST "Tic/Toc"


#define BUDDY_ARRIVE 0
#define BUDDY_LEAVE 1
#define SEND 2
#define RECEIVE 3
#define FIRST_RECEIVE 4
#define AWAY 5


#ifdef USE_APPLET
extern gboolean buddy_created;
extern GtkWidget *applet;
#endif /* USE_APPLET */

/* Globals in oscar.c */
extern struct aim_session_t *gaim_sess;
extern struct aim_conn_t    *gaim_conn;
extern GList *oscar_chats;

/* Globals in server.c */
extern int correction_time;

/* Globals in dialog.c */
extern char fontface[64];
extern int bgcolor;
extern int fgcolor;
extern int smiley_array[FACE_TOTAL];

/* Globals in network.c */

/* Globals in toc.c */

/* Globals in aim.c */
extern GList *permit;  /* The list of people permitted */
extern GList *deny;    /* The list of people denied */
extern GList *log_conversations;
extern GList *buddy_pounces;
extern GList *away_messages;
extern GList *groups;
extern GList *buddy_chats;
extern GList *conversations;
extern GList *chat_rooms;
extern GtkWidget *mainwindow;
extern char *quad_addr;
extern char toc_addy[16];

/* Globals in away.c */
extern struct away_message *awaymessage;
extern GtkWidget *awaymenu;

/* Globals in buddy.c */
extern int permdeny;
extern GtkWidget *buddies;
extern GtkWidget *bpmenu;
extern GtkWidget *blist;

extern int general_options;
#define OPT_GEN_ENTER_SENDS      0x00000001
#define OPT_GEN_AUTO_LOGIN       0x00000002
#define OPT_GEN_LOG_ALL          0x00000004
#define OPT_GEN_STRIP_HTML       0x00000008
#define OPT_GEN_APP_BUDDY_SHOW   0x00000010
#define OPT_GEN_POPUP_WINDOWS    0x00000020
#define OPT_GEN_SEND_LINKS       0x00000040
#define OPT_GEN_DEBUG            0x00000100
#define OPT_GEN_REMEMBER_PASS    0x00000200
#define OPT_GEN_REGISTERED       0x00000400
#define OPT_GEN_BROWSER_POPUP    0x00000800
#define OPT_GEN_SAVED_WINDOWS    0x00001000
#define OPT_GEN_DISCARD_WHEN_AWAY 0x00002000
#define OPT_GEN_NEAR_APPLET	0x00004000
#define OPT_GEN_CHECK_SPELLING	0x00008000
#define OPT_GEN_POPUP_CHAT	0x00010000
#define OPT_GEN_BACK_ON_IM	0x00020000
#define OPT_GEN_USE_OSCAR	0x00040000
#define OPT_GEN_CTL_CHARS	0x00080000
extern int USE_OSCAR;

extern int display_options;
#define OPT_DISP_SHOW_TIME       0x00000001
#define OPT_DISP_SHOW_GRPNUM     0x00000002
#define OPT_DISP_SHOW_PIXMAPS    0x00000004
#define OPT_DISP_SHOW_IDLETIME   0x00000008
#define OPT_DISP_SHOW_BUTTON_XPM 0x00000010
#define OPT_DISP_IGNORE_COLOUR   0x00000020
#define OPT_DISP_SHOW_LOGON      0x00000040
#define OPT_DISP_DEVIL_PIXMAPS   0x00000080
#define OPT_DISP_SHOW_SMILEY	 0x00000100
#define OPT_DISP_SHOW_BUDDYTICKER 0x00000200
#define OPT_DISP_COOL_LOOK       0x00000400
#define OPT_DISP_CHAT_LOGON      0x00000800
#define OPT_DISP_IGN_WHITE       0x00001000
 
extern int sound_options;
#define OPT_SOUND_LOGIN          0x00000001
#define OPT_SOUND_LOGOUT         0x00000002
#define OPT_SOUND_RECV           0x00000004
#define OPT_SOUND_SEND           0x00000008
#define OPT_SOUND_FIRST_RCV      0x00000010
#define OPT_SOUND_WHEN_AWAY      0x00000020
#define OPT_SOUND_SILENT_SIGNON  0x00000040
#define OPT_SOUND_THROUGH_GNOME  0x00000080
#define OPT_SOUND_CHAT_JOIN	 0x00000100
#define OPT_SOUND_CHAT_SAY	 0x00000200


extern int font_options;
#define OPT_FONT_BOLD		 0x00000001
#define OPT_FONT_ITALIC          0x00000002
#define OPT_FONT_UNDERLINE       0x00000008
#define OPT_FONT_STRIKE          0x00000010
#define OPT_FONT_FACE            0x00000020
#define OPT_FONT_FGCOL           0x00000040
#define OPT_FONT_BGCOL           0x00000080

#define DEFAULT_INFO "Visit the GAIM website at <A HREF=\"http://www.marko.net/gaim\">http://www.marko.net/gaim</A>."

extern int report_idle;
extern int web_browser;
extern struct aim_user *current_user;
extern GList *aim_users;
extern char web_command[2048];
extern char debug_buff[BUF_LONG];
extern char aim_host[512];
extern int aim_port;
extern char login_host[512];
extern int login_port;
extern struct save_pos blist_pos;
extern char latest_ver[25];

/* Functions in about.c */
extern void show_about(GtkWidget *, void *);


/* Functions in buddy_chat.c */
extern void join_chat();
extern void chat_write(struct conversation *, char *, int, char *);
extern void add_chat_buddy(struct conversation *, char *);
extern void remove_chat_buddy(struct conversation *, char *);
extern void show_new_buddy_chat(struct conversation *);
extern void setup_buddy_chats();
extern void do_quit();



/* Functions in html.c */
extern char *fix_url(char *);
extern struct g_url parse_url(char *);
extern char *grab_url(char *);
extern gchar *strip_html(gchar *);

/* Functions in util.c */
extern char *normalize(const char *);
extern int escape_text(char *);
extern char *escape_text2(char *);
extern int escape_message(char *msg);
extern char *frombase64(char *);
extern gint clean_pid(void *);
extern char *date();
extern gint linkify_text(char *);
extern void aol_icon(GdkWindow *);
extern int query_state();
extern void set_state(int);
extern FILE *open_log_file (char *);
extern char *sec_to_text(int);
extern struct aim_user *find_user(const char *);
extern char *full_date();
extern void check_gaim_versions();
extern void spell_checker(GtkWidget *);
extern char *away_subs(char *, char *);
extern GtkWidget *picture_button(GtkWidget *, char *, char **);
extern GtkWidget *picture_button2(GtkWidget *, char *, char **);

/* Functions in server.c */
/* input to serv */
extern int serv_login(char *, char *);
extern void serv_close();
extern void serv_touch_idle();
extern void serv_finish_login();
extern void serv_send_im(char *, char *, int);
extern void serv_get_info(char *);
extern void serv_get_away_msg(char *);
extern void serv_get_dir(char *);
extern void serv_set_idle(int);
extern void serv_set_info(char *);
extern void serv_set_away(char *);
extern void serv_change_passwd(char *, char *);
extern void serv_add_buddy(char *);
extern void serv_add_buddies(GList *);
extern void serv_remove_buddy(char *);
extern void serv_add_permit(char *);
extern void serv_add_deny(char *);
extern void serv_set_permit_deny();
extern void serv_build_config(char *, int);
extern void serv_save_config();
extern void serv_warn(char *, int);
extern void serv_set_dir(char *, char *, char *, char *, char *, char *, char *, int);
extern void serv_dir_search(char *, char *, char *, char *, char *, char *, char *, char *);
extern void serv_accept_chat(int);
extern void serv_join_chat(int, char *);
extern void serv_chat_invite(int, char *, char *);
extern void serv_chat_leave(int);
extern void serv_chat_whisper(int, char *, char *);
extern void serv_chat_send(int, char *);
extern void serv_do_imimage(GtkWidget *, char *);
extern void serv_got_imimage(char *, char *, char *, struct aim_conn_t *, int);

/* output from serv */
extern void serv_got_update(char *, int, int, time_t, time_t, int, u_short);
extern void serv_got_im(char *, char *, int);
extern void serv_got_eviled(char *, int);
extern void serv_got_chat_invite(char *, int, char *, char *);
extern void serv_got_joined_chat(int, char *);
extern void serv_got_chat_left(int);
extern void serv_got_chat_in(int, char *, int, char *);
extern void serv_rvous_accept(char *, char *, char *);
extern void serv_rvous_cancel(char *, char *, char *);

/* Functions in conversation.c */
extern void write_html_with_smileys(GtkWidget *, GtkWidget *, char *);
extern void make_direct(struct conversation *, gboolean, struct aim_conn_t *, gint);
extern void write_to_conv(struct conversation *, char *, int, char *);
extern void show_conv(struct conversation *);
extern struct conversation *new_conversation(char *);
extern struct conversation *find_conversation(char *);
extern void delete_conversation(struct conversation *);
extern void surround(GtkWidget *, char *, char *);
extern int is_logging(char *);
extern void set_state_lock(int );
extern void rm_log(struct log_conversation *a);
extern struct log_conversation *find_log_info(char *name);
extern void remove_tags(GtkWidget *entry, char *tag);
extern void update_log_convs();
extern void update_transparency();
extern void update_font_buttons();
extern void toggle_sensitive(GtkWidget *widget, GtkWidget *to_toggle);
extern void do_bold(GtkWidget *, GtkWidget *);
extern void do_italic(GtkWidget *, GtkWidget *);
extern void do_underline(GtkWidget *, GtkWidget *);
extern void do_strike(GtkWidget *, GtkWidget *);
extern void do_small(GtkWidget *, GtkWidget *);
extern void do_normal(GtkWidget *, GtkWidget *);
extern void do_big(GtkWidget *, GtkWidget *);
extern void toggle_link(GtkWidget *, struct conversation *);
extern int invert_tags(GtkWidget *, char *, char *, int);
extern void quiet_set(GtkWidget *, int);
extern int count_tag(GtkWidget *, char *, char *);
extern void set_font_face(char *, struct conversation *);

/* Functions in network.c */
extern unsigned int *get_address(char *);
extern int connect_address(unsigned int, unsigned short);

/* Functions in oscar.c */
extern int oscar_login(char *, char *);
extern void oscar_close();
extern struct chat_connection *find_oscar_chat(char *name);
extern void oscar_do_directim(char *);

/* Functions in toc.c */
extern void toc_close();
extern int toc_login(char *, char *);
extern int toc_wait_signon(void);
extern char *toc_wait_config(void);
extern int sflap_send(char *, int , int );
extern void parse_toc_buddy_list(char *, int);


/* Functions in buddy.c */
extern void destroy_buddy();
extern void update_num_groups();
extern void update_show_idlepix();
extern void update_button_pix();
extern void update_all_buddies();
extern void show_buddy_list();
extern void refresh_buddy_window();
extern void toc_build_config(char *, int len);
extern void signoff();
extern void do_im_back();
extern void set_buddy(struct buddy *);
extern struct person *add_person(char *, char *);
extern struct group *add_group(char *);
extern void add_category(char *);
extern void build_edit_tree();
extern void remove_person(struct group *, struct buddy *);
extern void remove_category(struct group *);
extern void do_pounce(char *);
extern void do_bp_menu();
extern struct buddy *find_buddy(char *);
extern struct group *find_group(char *);
extern struct group *find_group_by_buddy(char *);
extern void remove_buddy(struct group *, struct buddy *);
extern struct buddy *add_buddy(char *, char *);
extern void remove_group(struct group *);
extern void update_lagometer(int);

/* Functions in away.c */
extern void rem_away_mess(GtkWidget *, struct away_message *);
extern void do_away_message(GtkWidget *, struct away_message *);
extern void do_away_menu();
extern void away_list_unclicked(GtkWidget *, struct away_message *);
extern void away_list_clicked(GtkWidget *, struct away_message *);

/* Functions in aim.c */
extern void hide_login_progress(char *);
extern void set_login_progress(int, char *);
extern void show_login();
extern void gaim_setup();
#ifdef USE_APPLET
extern void createOnlinePopup();
extern void applet_show_login(AppletWidget *, gpointer);
extern void gnome_buddy_show();
extern void gnome_buddy_hide();
extern void gnome_buddy_set_pos( gint x, gint y );
GtkRequisition gnome_buddy_get_dimentions();
#endif


/* Functions in sound.c */
extern void play_sound(int);


#ifdef GAIM_PLUGINS
/* Functions in plugins.c */
extern void show_plugins(GtkWidget *, gpointer);
extern void load_plugin (char *);
extern void gaim_signal_connect(void *, enum gaim_event, void *, void *);
extern void gaim_signal_disconnect(void *, enum gaim_event, void *);
extern void gaim_plugin_unload(void *);
#endif

/* Functions in prefs.c */
extern void debug_print( char * chars );
extern void set_general_option(GtkWidget *, int *);
extern void set_option(GtkWidget *, int *);
extern void show_prefs();
extern void show_debug(GtkObject *);
extern void build_permit_tree();
extern GtkWidget *prefs_away_list;

/* Functions in gaimrc.c */
extern void set_defaults();
extern void load_prefs();
extern void save_prefs();


/* Functions in dialogs.c */
extern void do_export(GtkWidget *, void *);
extern void show_warn_dialog(char *);
extern void do_error_dialog(char *, char *);
extern void show_error_dialog(char *);
extern void show_im_dialog(GtkWidget *, GtkWidget *);
extern void show_add_buddy(char *, char *);
extern void show_add_group();
extern void show_add_perm();
extern void destroy_all_dialogs();
extern void show_export_dialog();
extern void show_import_dialog();
extern void show_new_bp();
extern void show_log_dialog(struct conversation *);
extern void show_find_email();
extern void show_find_info();
extern void g_show_info (char *);
extern void g_show_info_text (char *);
extern void show_register_dialog();
extern void show_set_info();
extern void show_set_dir();
extern void show_color_dialog(struct conversation *c, GtkWidget *color);
extern void cancel_color(GtkWidget *widget, struct conversation *c);
extern void create_away_mess(GtkWidget *, void *);
extern void show_ee_dialog(int);
extern void show_add_link(GtkWidget *,struct conversation *);
extern void show_change_passwd();
extern void do_import(GtkWidget *, void *);
extern int bud_list_cache_exists();
extern void show_smiley_dialog(struct conversation *, GtkWidget *);
extern void close_smiley_dialog(GtkWidget *widget, struct conversation *c);
extern void set_smiley_array(GtkWidget *widget, int smiley_type);
extern void insert_smiley_text(GtkWidget *widget, struct conversation *c);
extern void cancel_log(GtkWidget *, struct conversation *);
extern void cancel_link(GtkWidget *, struct conversation *);
extern void show_font_dialog(struct conversation *c, GtkWidget *font);
extern void cancel_font(GtkWidget *widget, struct conversation *c);
extern void apply_font(GtkWidget *widget, GtkFontSelection *fontsel);

/* Functions in rvous.c */
extern void accept_file_dialog(struct file_transfer *);

/* Functions in browser.c */
extern void open_url(GtkWidget *, char *);
extern void open_url_nw(GtkWidget *, char *);
extern void add_bookmark(GtkWidget *, char *);

/* functions for appletmgr */
extern char * getConfig();

/* fucntions in ticker.c */
void SetTickerPrefs();
void BuddyTickerSignOff();
void BuddyTickerAddUser(char *, GdkPixmap *, GdkBitmap *);
void BuddyTickerSetPixmap(char *, GdkPixmap *, GdkBitmap *);
void BuddyTickerSignoff();
