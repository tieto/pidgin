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
#ifdef USE_APPLET
#include <applet-widget.h>
#endif /* USE_APPLET */
#ifdef USE_GNOME
#include <gnome.h>
#endif
#if USE_PIXBUF
#include <gdk-pixbuf/gdk-pixbuf.h>
#endif

#define BROWSER_NETSCAPE              0
#define BROWSER_KFM                   1
#define BROWSER_MANUAL                2
/*#define BROWSER_INTERNAL              3*/
#define BROWSER_GNOME                 4

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
	int xoff;
	int yoff;
};

struct window_size {
	int width;
	int height;
	int entry_height;
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

/* this is used for queuing messages received while away. This is really a UI function
 * which is why the struct is here. */
struct queued_message {
	char name[80];
	char *message;
	time_t tm;
	struct gaim_connection *gc;
	int flags;
};

#define EDIT_GC    0
#define EDIT_GROUP 1
#define EDIT_BUDDY 2

/* Globals in aim.c */
extern GList *log_conversations; /* this should be moved to conversations.c */
extern GSList *away_messages; /* this should be moved to away.c */

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

/* Globals in buddy_chat.c */
/* it is very important that you don't use this for anything.
 * its sole purpose is to allow all group chats to be in one
 * window. use struct gaim_connection's buddy_chats instead. */
extern GList *chats;
/* these are ok to use */
extern GtkWidget *all_chats;
extern GtkWidget *chat_notebook;
extern GtkWidget *joinchat;

/* Globals in dialog.c */
extern char fontface[64];
extern int fontsize;
extern GdkColor bgcolor;
extern GdkColor fgcolor;
extern int smiley_array[FACE_TOTAL];

/* Globals in prefs.c */
extern struct debug_window *dw;

/* Globals in prpl.c */
extern GtkWidget *protomenu;

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
extern void purge_away_queue();

/* Functions in browser.c */
extern void open_url(GtkWidget *, char *);
extern void open_url_nw(GtkWidget *, char *);
extern void add_bookmark(GtkWidget *, char *);

/* Functions in buddy.c */
extern void handle_group_rename(struct group *, char *);
extern void handle_buddy_rename(struct buddy *, char *);
extern void destroy_buddy();
extern void update_button_pix();
extern void toggle_show_empty_groups();
extern void update_all_buddies();
extern void update_num_groups();
extern void show_buddy_list();
extern void refresh_buddy_window();
extern void signoff_all(gpointer, gpointer);
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
extern void set_convo_tab_label(struct conversation *, char *);
extern void show_conv(struct conversation *);
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
extern void toggle_smileys();
extern void toggle_timestamps();
extern void update_pixmaps();
extern void tabize();
extern void chat_tabize();
extern void update_convo_color();
extern void update_convo_font();
extern void set_hide_icons();

/* Functions in dialogs.c */
extern void alias_dialog_bud(struct buddy *);
extern void show_warn_dialog(struct gaim_connection *, char *);
extern void show_im_dialog();
extern void show_info_dialog();
extern void show_add_buddy(struct gaim_connection *, char *, char *);
extern void show_add_group(struct gaim_connection *);
extern void show_add_perm(struct gaim_connection *, char *, gboolean);
extern void destroy_all_dialogs();
extern void show_import_dialog();
extern void show_export_dialog();
extern void show_new_bp();
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
extern void aol_icon(GdkWindow *);
extern GtkWidget *picture_button(GtkWidget *, char *, char **);
extern GtkWidget *picture_button2(GtkWidget *, char *, char **, short);

/* Functions in multi.c */
extern void account_editor(GtkWidget *, GtkWidget *);

/* Functions in plugins.c */
#ifdef GAIM_PLUGINS
extern void show_plugins(GtkWidget *, gpointer);
#endif

/* Functions in prefs.c */
extern void set_option(GtkWidget *, int *);
extern void show_prefs();
extern void show_debug();
extern void update_color(GtkWidget *, GtkWidget *);
extern void set_default_away(GtkWidget *, gpointer);
extern void default_away_menu_init(GtkWidget *);
extern void update_connection_dependent_prefs();
extern void build_allow_list();
extern void build_block_list();
extern GtkWidget *prefs_away_list;
extern GtkWidget *prefs_away_menu;
extern GtkWidget *pref_fg_picture;
extern GtkWidget *pref_bg_picture;

/* Functions in sound.c */
extern void play_sound(int);
extern void play_file(char *);

/* Fucntions in ticker.c */
void SetTickerPrefs();
void BuddyTickerSignOff();
void BuddyTickerAddUser(char *, GdkPixmap *, GdkBitmap *);
void BuddyTickerSetPixmap(char *, GdkPixmap *, GdkBitmap *);
void BuddyTickerSignoff();

#endif /* _UI_H_ */
