/**
 * @file ui.h Main GTK+ UI include file
 * @defgroup gtkui GTK+ User Interface
 *
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
 */

#ifndef _UI_H_
#define _UI_H_

#if 0
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "core.h"
#include "gtkconv.h"
#include "pounce.h"
#include "gtkft.h"
#include "gtkprefs.h"
#include "gtkutils.h"
#include "stock.h"
#endif

#include "account.h"
#include "conversation.h"

/**
 * Our UI's identifier.
 */
#define GAIM_GTK_UI "gtk-gaim"


#define GAIM_DIALOG(x)	x = gtk_window_new(GTK_WINDOW_TOPLEVEL); \
			gtk_window_set_type_hint(GTK_WINDOW(x), GDK_WINDOW_TYPE_HINT_DIALOG)
#define GAIM_WINDOW_ICONIFIED(x) (gdk_window_get_state(GTK_WIDGET(x)->window) & GDK_WINDOW_STATE_ICONIFIED)

/* This is backwards-compatibility code for old versions of GTK+ (2.2.1 and
 * earlier).  It defines the new wrap behavior (unknown in earlier versions)
 * as the old (slightly buggy) wrap behavior.
 */
#ifndef GTK_WRAP_WORD_CHAR
#define GTK_WRAP_WORD_CHAR GTK_WRAP_WORD
#endif

#define DEFAULT_FONT_FACE "Helvetica"


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
	GaimAccount *account;
	GaimMessageFlags flags;
	int len;
};

struct smiley_theme {
	char *path;
	char *name;
	char *desc;
	char *icon;
	char *author;
	
	struct smiley_list *list;
};

/* Globals in aim.c */
extern GtkWidget *mainwindow;
extern int docklet_count;

/* Globals in away.c */
extern GSList *away_messages;
extern struct away_message *awaymessage;
extern GtkWidget *awaymenu;
extern GtkWidget *awayqueue;
extern GtkListStore *awayqueuestore;

/* Globals in dialog.c */
extern char fontxfld[256];
extern GtkWidget *fgcseld;
extern GtkWidget *bgcseld;

/* Globals in session.c */
extern gboolean session_managed;

/* Globals in themes.c */
extern struct smiley_theme *current_smiley_theme;
extern GSList *smiley_themes;


/* Functions in about.c */
extern void show_about(GtkWidget *, void *);

/* Functions in main.c */
extern void show_login();
extern void gaim_setup(GaimConnection *);

/* Functions in away.c */
extern void rem_away_mess(GtkWidget *, struct away_message *);
extern void do_away_message(GtkWidget *, struct away_message *);
extern void do_away_menu();
extern void toggle_away_queue();
extern void purge_away_queue(GSList**);
extern void do_im_back(GtkWidget *w, GtkWidget *x);

/* Functions in browser.c */
void *gaim_gtk_notify_uri(const char *uri);

/* Functions in dialogs.c */
extern void alias_dialog_bud(struct buddy *);
extern void alias_dialog_chat(struct chat *);
extern void show_warn_dialog(GaimConnection *, char *);
extern void show_im_dialog();
extern void show_info_dialog();
extern void show_add_buddy(GaimConnection *, char *, char *, char *);
extern void show_add_chat(GaimAccount *, struct group *);
extern void show_add_group(GaimConnection *);
extern void destroy_all_dialogs();
extern void show_import_dialog();
extern void show_export_dialog();
extern void conv_show_log(GtkWidget *, gpointer);
extern void chat_show_log(GtkWidget *, gpointer);
extern void show_log(char *);
extern void show_log_dialog(GaimConversation *);
extern void show_fgcolor_dialog(GaimConversation *c, GtkWidget *color);
extern void show_bgcolor_dialog(GaimConversation *c, GtkWidget *color);
extern void cancel_fgcolor(GtkWidget *widget, GaimConversation *c);
extern void cancel_bgcolor(GtkWidget *widget, GaimConversation *c);
extern void create_away_mess(GtkWidget *, void *);
extern void show_ee_dialog(int);
extern void show_insert_link(GtkWidget *,GaimConversation *);
extern void show_smiley_dialog(GaimConversation *, GtkWidget *);
extern void close_smiley_dialog(GtkWidget *widget, GaimConversation *c);
extern void set_smiley_array(GtkWidget *widget, int smiley_type);
extern void insert_smiley_text(GtkWidget *widget, GaimConversation *c);
extern void cancel_log(GtkWidget *, GaimConversation *);
extern void cancel_link(GtkWidget *, GaimConversation *);
extern void show_font_dialog(GaimConversation *c, GtkWidget *font);
extern void cancel_font(GtkWidget *widget, GaimConversation *c);
extern void apply_font(GtkWidget *widget, GtkFontSelection *fontsel);
extern void show_rename_group(GtkWidget *, struct group *);
extern void destroy_fontsel(GtkWidget *w, gpointer d);
extern void join_chat();

/* Functions in server.c */
/* server.c is in desperate need need of a split */
extern int find_queue_total_by_name(char *);

/* Functions in session.c */
extern void session_init(gchar *, gchar *);
extern void session_end();

/* Functions in themes.c */
extern void smiley_themeize(GtkWidget *);
extern void smiley_theme_probe();
extern void load_smiley_theme(const char *file, gboolean load);
extern GSList *get_proto_smileys(int protocol);

#endif /* _UI_H_ */
