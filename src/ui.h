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

#ifndef _UI_H_
#define _UI_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#define GAIM_DIALOG(x)	x = gtk_window_new(GTK_WINDOW_TOPLEVEL); \
			gtk_window_set_type_hint(GTK_WINDOW(x), GDK_WINDOW_TYPE_HINT_DIALOG)
#define GAIM_WINDOW_ICONIFIED(x) (gdk_window_get_state(GTK_WIDGET(x)->window) & GDK_WINDOW_STATE_ICONIFIED)

#define DEFAULT_FONT_FACE "Helvetica"

#define BROWSER_NETSCAPE              0
#define BROWSER_KONQ                  1
#define BROWSER_MANUAL                2
/*#define BROWSER_INTERNAL              3*/
#define BROWSER_GNOME                 4
#define BROWSER_OPERA                 5	
#define BROWSER_GALEON                6
#define BROWSER_MOZILLA               7

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

#define GAIM_LOGO 0
#define GAIM_ERROR 1
#define GAIM_WARNING 2
#define GAIM_INFO 3

typedef enum {
		GAIM_BUTTON_HORIZONTAL,
		GAIM_BUTTON_VERTICAL
} GaimButtonStyle;


struct debug_window {
	GtkWidget *window;
	GtkWidget *entry;
};

/* XXX CUI: save_pos and window_size are used by gaimrc.c which is core.
 * Need to figure out options saving. Same goes for several global variables as well. */
struct save_pos {
        int x;
        int y;
        int width;
        int height;
};

struct window_size {
	int width;
	int height;
	int entry_height;
};

/* struct buddy_chat went away and got merged with this. */
struct conversation {
	struct gaim_connection *gc;

	/* stuff used for both IM and chat */
	GtkWidget *window;
	char name[80];
	GtkWidget *toolbar;
	GtkWidget *text;
	GtkWidget *entry;
	GtkWidget *italic;
	GtkWidget *bold;
	GtkWidget *underline;
	GtkWidget *fgcolorbtn;
	GtkWidget *bgcolorbtn;
	GtkWidget *link;
/*	GtkWidget *sendfile_btn; */
	GtkWidget *wood;
	GtkWidget *viewer_button;
	GtkWidget *log_button;
	GtkWidget *strike;
	GtkWidget *font;
	GtkWidget *smiley;
	GtkWidget *imagebtn;
	GtkWidget *image_menubtn;
	GtkWidget *speaker;
	GtkWidget *speaker_p;
	GtkWidget *fg_color_dialog;
	GtkWidget *bg_color_dialog;
	GtkWidget *font_dialog;
	GtkWidget *smiley_dialog;
	GtkWidget *link_dialog;
	GtkWidget *log_dialog;
	GtkSizeGroup *sg;
	int makesound;
	char fontface[128];
	int hasfont;
	GdkColor bgcol;
	int hasbg;
	GdkColor fgcol;
	int hasfg;

	GList *send_history;
	GString *history;

	GtkWidget *send;

	/* stuff used just for IM */
	GtkWidget *lbox;
	GtkWidget *bbox;
	GtkWidget *sw;
	GtkWidget *info;
	GtkWidget *warn;
	GtkWidget *block;
	GtkWidget *add;
	GtkWidget *sep1;
	GtkWidget *sep2;
	GtkWidget *menu;
	GtkWidget *check;
	GtkWidget *progress;
	GSList    *images;  /* A list of filenames to embed */
	gint unseen;
	guint typing_timeout;
	time_t type_again;
	guint type_again_timeout;

	/* stuff used just for chat */
        GList *in_room;
        GList *ignored;
	char *topic;
        int id;
	GtkWidget *count;
	GtkWidget *list;
	GtkWidget *whisper;
	GtkWidget *invite;
	GtkWidget *close;
	GtkWidget *topic_text;

	/* something to distinguish */
	gboolean is_chat;

	/* buddy icon stuff. sigh. */
	GtkWidget *icon;
	GdkPixbufAnimation *anim;
	guint32 icon_timer;
	GdkPixbufAnimationIter *iter;
	GtkWidget *save_icon;
};

struct log_conversation {
	char name[80];
	char filename[512];
        struct log_conversation *next;
};

/* XXX CUI: away messages aren't really anything more than char* but we need two char*'s
 * for the UI so that people can name their away messages when they save them. So these
 * are really a UI function and struct away_message should be removed from the core. */
struct away_message {
	char name[80];
	char message[2048];
};

/****************************
 * I thought I'd place these here by the same reasoning used above (for away_message)
 * This helps aleviate warnings from dialogs.c where the show_new_bp function references
 * buddy_pounce in the parameter list when ui.h doesn't know about buddy_pounce
 * **************************
 */
struct buddy_pounce {
        char name[80];
        char message[2048];
        char command[2048];
        char sound[2048];
        
        char pouncer[80];
        int protocol;

        int options;
};


/* this is used for queuing messages received while away. This is really a UI function
 * which is why the struct is here. */
struct queued_message {
	char name[80];
	char *message;
	time_t tm;
	struct gaim_connection *gc;
	int flags;
	int len;
};

struct mod_user {
	struct aim_user *user;

	/* these are temporary */
	int options;
	int protocol;
	char proto_opt[7][256];

	/* stuff for modify window */
	GtkWidget *mod;
	GtkWidget *main;
	GtkWidget *name;
	GtkWidget *alias;
	GtkWidget *pwdbox;
	GtkWidget *pass;
	GtkWidget *rempass;
	GtkWidget *user_frame;
	GtkWidget *proto_frame;
	GtkSizeGroup *sg;
	GList *opt_entries;

	/* stuff for icon selection */
	char iconfile[256];
	GtkWidget *iconsel;
	GtkWidget *iconentry;
	GtkWidget *icondlg;

	/* stuff for mail check prompt */
	GtkWidget *checkmail;

	/* stuff for register with server */
	GtkWidget *register_user;
};

#define EDIT_GC    0
#define EDIT_GROUP 1
#define EDIT_BUDDY 2

/* Globals in aim.c */
extern GList *log_conversations; /* this should be moved to conversations.c */
extern GSList *away_messages; /* this should be moved to away.c */
extern GtkWidget *mainwindow;

/* Globals in away.c */
extern struct away_message *awaymessage;
extern struct away_message *default_away;
extern int auto_away;
extern GtkWidget *awaymenu;
extern GtkWidget *clistqueue; 

/* Globals in buddy.c */
extern GtkWidget *buddies;
extern GtkWidget *bpmenu;
extern GtkWidget *blist;
extern int docklet_count;

/* Globals in buddy_chat.c */
extern GList *chats;	/* list of all chats (only use for tabbing!) */
extern GtkWidget *all_chats;
extern GtkWidget *joinchat;

/* Globals in conversation.c */
extern GtkWidget *all_convos;

/* Globals in dialog.c */
extern char fontface[128];
extern char fontxfld[256];
extern int fontsize;
extern GdkColor bgcolor;
extern GdkColor fgcolor;
extern int smiley_array[FACE_TOTAL];
extern GtkWidget *fgcseld;
extern GtkWidget *bgcseld;

/* Globals in prefs.c */
extern GtkWidget *prefs;
extern struct debug_window *dw;
extern GtkWidget *fontseld;

/* Globals in prpl.c */
extern GtkWidget *protomenu;

/* Globals in sound.c */
extern gboolean mute_sounds;

/* Functions in about.c */
extern void show_about(GtkWidget *, void *);
extern void gaim_help(GtkWidget *, void *);

/* Functions in aim.c */
extern void show_login();
extern void gaim_setup(struct gaim_connection *gc);

/* Functions in away.c */
extern void rem_away_mess(GtkWidget *, struct away_message *);
extern void do_away_message(GtkWidget *, struct away_message *);
extern void do_away_menu();
extern void away_list_unclicked(GtkWidget *, struct away_message *);
extern void away_list_clicked(GtkWidget *, struct away_message *);
extern void toggle_away_queue();
extern void purge_away_queue(GSList*);

/* Functions in browser.c */
extern void open_url(GtkWidget *, char *);
extern void add_bookmark(GtkWidget *, char *);

/* Functions in buddy.c */
extern void handle_group_rename(struct group *, char *);
extern void handle_buddy_rename(struct buddy *, char *);
extern void destroy_buddy();
extern void update_button_pix();
extern void toggle_show_empty_groups();
extern void update_all_buddies();
extern void update_num_groups(void);
extern void show_buddy_list();
extern void signoff_all();
extern void do_im_back();
extern void set_buddy(struct gaim_connection *, struct buddy *);
extern void build_edit_tree();
extern void do_bp_menu();
extern void ui_add_buddy(struct gaim_connection *, struct group *, struct buddy *);
extern void ui_remove_buddy(struct gaim_connection *, struct group *, struct buddy *);
extern void ui_add_group(struct gaim_connection *, struct group *);
extern void ui_remove_group(struct gaim_connection *, struct group *);
extern void toggle_buddy_pixmaps();
extern void gaim_separator(GtkWidget *);
extern void redo_buddy_list(); /* you really shouldn't call this function */
extern void set_blist_tab();
extern void hide_buddy_list();
extern void unhide_buddy_list();
extern void docklet_add();
extern void docklet_remove();
extern void docklet_toggle();
extern GtkWidget *gaim_new_item(GtkWidget *, const char *);

/* Functions in buddy_chat.c */
extern void join_chat();
extern void chat_write(struct conversation *, char *, int, char *, time_t);
extern void delete_chat(struct conversation *);
extern void build_imchat_box(gboolean);
extern void update_chat_button_pix();
extern void update_im_button_pix();
extern void update_chat_tabs();
extern void update_im_tabs();
extern void update_idle_times();
extern void do_join_chat();

/* Functions in conversation.c */
extern void gaim_setup_imhtml(GtkWidget *);
extern void update_convo_add_button(struct conversation *);
extern void raise_convo_tab(struct conversation *);
extern void set_convo_title(struct conversation *);
extern void show_conv(struct conversation *);
void set_convo_name(struct conversation *c, const char *nname);
extern struct conversation *new_conversation(char *);
extern void delete_conversation(struct conversation *);
extern void surround(GtkWidget *, char *, char *);
extern int is_logging(char *);
extern void set_state_lock(int);
extern void rm_log(struct log_conversation *);
extern struct log_conversation *find_log_info(char *);
extern void remove_tags(GtkWidget *, char *);
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
extern void set_font_face(char *, struct conversation *);
extern void redo_convo_menus();
extern void convo_menu_remove(struct gaim_connection *);
extern void remove_icon_data(struct gaim_connection *);
extern void got_new_icon(struct gaim_connection *, char *);
extern void toggle_spellchk();
extern void set_convo_gc(struct conversation *, struct gaim_connection *);
extern void update_buttons_by_protocol(struct conversation *);
extern void toggle_fg_color(GtkWidget *, struct conversation *);
extern void toggle_smileys();
extern void toggle_timestamps();
extern void update_pixmaps();
extern void im_tabize();
extern void chat_tabize();
extern void convo_tabize();
extern void update_convo_color();
extern void update_convo_font();
extern void set_hide_icons();
extern void set_convo_titles();
extern void update_progress(struct conversation *, float);
extern void update_convo_status(struct conversation *, int);
extern gboolean reset_typing(char *);
extern void set_anim();

/* Functions in dialogs.c */
extern void alias_dialog_bud(struct buddy *);
extern void show_warn_dialog(struct gaim_connection *, char *);
extern void show_im_dialog();
extern void show_info_dialog();
extern void show_add_buddy(struct gaim_connection *, char *, char *, char *);
extern void show_add_group(struct gaim_connection *);
extern void show_add_perm(struct gaim_connection *, char *, gboolean);
extern void destroy_all_dialogs();
extern void show_import_dialog();
extern void show_export_dialog();
extern void show_new_bp(char *, struct gaim_connection *, int, int, struct buddy_pounce *);
extern void conv_show_log(GtkWidget *, gpointer);
extern void chat_show_log(GtkWidget *, gpointer);
extern void show_log(char *);
extern void show_log_dialog(struct conversation *);
extern void show_fgcolor_dialog(struct conversation *c, GtkWidget *color);
extern void show_bgcolor_dialog(struct conversation *c, GtkWidget *color);
extern void cancel_fgcolor(GtkWidget *widget, struct conversation *c);
extern void cancel_bgcolor(GtkWidget *widget, struct conversation *c);
extern void create_away_mess(GtkWidget *, void *);
extern void show_ee_dialog(int);
extern void show_add_link(GtkWidget *,struct conversation *);
extern void show_smiley_dialog(struct conversation *, GtkWidget *);
extern void close_smiley_dialog(GtkWidget *widget, struct conversation *c);
extern void set_smiley_array(GtkWidget *widget, int smiley_type);
extern void insert_smiley_text(GtkWidget *widget, struct conversation *c);
extern void cancel_log(GtkWidget *, struct conversation *);
extern void cancel_link(GtkWidget *, struct conversation *);
extern void show_font_dialog(struct conversation *c, GtkWidget *font);
extern void cancel_font(GtkWidget *widget, struct conversation *c);
extern void apply_font(GtkWidget *widget, GtkFontSelection *fontsel);
extern void set_color_selection(GtkWidget *selection, GdkColor color);
extern void show_rename_group(GtkWidget *, struct group *);
extern void show_rename_buddy(GtkWidget *, struct buddy *);
extern void load_perl_script();
extern GtkWidget *picture_button(GtkWidget *, char *, char **);
extern GtkWidget *picture_button2(GtkWidget *, char *, char **, short);
extern GtkWidget *gaim_pixbuf_button(char *, char *, GaimButtonStyle);
extern GtkWidget *gaim_pixbuf_button_from_stock(char *, char *, GaimButtonStyle);
extern GtkWidget *gaim_pixbuf_toolbar_button_from_stock(char *);
extern GtkWidget *gaim_pixbuf_toolbar_button_from_file(char *);
extern int file_is_dir(const char *, GtkWidget *);
extern void update_privacy_connections();
extern void show_privacy_options();
extern void build_allow_list();
extern void build_block_list();
extern void destroy_fontsel(GtkWidget *w, gpointer d);

/* Functions in multi.c */
extern void account_editor(GtkWidget *, GtkWidget *);

/* Functions in plugins.c */
#ifdef GAIM_PLUGINS
extern void show_plugins(GtkWidget *, gpointer);
extern void update_show_plugins(); /* this is a hack and will be removed */
#endif

/* Functions in prefs.c */
extern void set_option(GtkWidget *, int *);
extern void show_prefs();
extern void show_debug();
extern void update_color(GtkWidget *, GtkWidget *);
extern void set_default_away(GtkWidget *, gpointer);
extern void default_away_menu_init(GtkWidget *);
extern void build_allow_list();
extern void build_block_list();
extern GtkWidget *make_frame(GtkWidget *, char *);
extern GtkWidget *prefs_away_list;
extern GtkWidget *prefs_away_menu;
extern GtkWidget *pref_fg_picture;
extern GtkWidget *pref_bg_picture;
extern void apply_font_dlg(GtkWidget *, GtkWidget *);
extern void apply_color_dlg(GtkWidget *, gpointer);
extern void destroy_colorsel(GtkWidget *, gpointer);


/* Functions in prpl.c */
extern void register_dialog();

/* Functions in server.c */
/* server.c is in desperate need need of a split */
extern int find_queue_total_by_name(char *);

/* Functions in sound.c */
extern void play_sound(int);
extern void play_file(char *);

/* Fucnctions in util.c */
extern GtkWidget *gaim_pixmap(char *, char *);
extern GdkPixbuf *gaim_pixbuf(char *, char *);
extern GtkWidget *gaim_new_item(GtkWidget *menu, const char *str);
extern GtkWidget *gaim_new_item_with_pixmap(GtkWidget *, const char *, char **, GtkSignalFunc, gpointer, guint, guint, char *);
extern GtkWidget *gaim_new_item_from_stock(GtkWidget *, const char *, const char *, GtkSignalFunc, gpointer, guint, guint, char *);
extern GtkWidget *gaim_new_item_from_pixbuf(GtkWidget *, const char *, char *, GtkSignalFunc, gpointer, guint, guint, char *);

#endif /* _UI_H_ */
