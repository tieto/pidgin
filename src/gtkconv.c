/*
 * gaim
 *
 * Copyright (C) 2002-2003, Christian Hammond <chipx86@gnupdate.org>
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
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#endif /*_WIN32*/
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <gtk/gtk.h>
#ifdef USE_GTKSPELL
#include <gtkspell/gtkspell.h>
#endif
#include <gdk/gdkkeysyms.h>
#include "prpl.h"
#include "gtkimhtml.h"
#include "dnd-hints.h"
#include "sound.h"
#include "gtkblist.h"
#include "notify.h"
#include "prefs.h"

#ifdef _WIN32
#include "win32dep.h"
#include "wspell.h"
#endif

static char nick_colors[][8] = {
	"#ba55d3",              /* Medium Orchid */
	"#ee82ee",              /* Violet */
	"#c715b4",              /* Medium Violet Red */
	"#ff69b4",              /* Hot Pink */
	"#ff6347",              /* Tomato */
	"#fa8c00",              /* Dark Orange */
	"#fa8072",              /* Salmon */
	"#b22222",              /* Fire Brick */
	"#f4a460",              /* Sandy Brown */
	"#cd5c5c",              /* Indian Red */
	"#bc8f8f",              /* Rosy Brown */
	"#f0e68c",              /* Khaki */
	"#bdb76b",              /* Dark Khaki */
	"#228b22",              /* Forest Green */
	"#9acd32",              /* Yellow Green */
	"#32cd32",              /* Lime Green */
	"#3cb371",              /* Medium Sea Green */
	"#2e8b57",              /* Sea Green */
	"#8fbc8f",              /* Dark Sea Green */
	"#66cdaa",              /* Medium Aquamarine */
	"#5f9ea0",              /* Cadet Blue */
	"#48d1cc",              /* Medium Turquoise */
	"#00ced1",              /* Dark Turquoise */
	"#4682b4",              /* Stell Blue */
	"#00bfff",              /* Deep Sky Blue */
	"#1690ff",              /* Dodger Blue */
	"#4169ff",              /* Royal Blue */
	"#6a5acd",              /* Slate Blue */
	"#6495ed",              /* Cornflower Blue */
	"#708090",              /* Slate gray */
	"#ffdead",              /* Navajo White */
};
#define NUM_NICK_COLORS (sizeof(nick_colors) / sizeof(*nick_colors))

#define SCALE(x) \
	((gdk_pixbuf_animation_get_width(x) <= 48 && \
	  gdk_pixbuf_animation_get_height(x) <= 48) ? 48 : 50)

typedef struct
{
	GtkWidget *window;

	GtkWidget *entry;
	GtkWidget *message;

	GaimConversation *conv;

} InviteBuddyInfo;

char fontface[128] = { 0 };
int fontsize = 3;

static GtkWidget *invite_dialog = NULL;

/* Prototypes. <-- because Paco-Paco hates this comment. */
static void check_everything(GtkTextBuffer *buffer);
static void set_toggle(GtkWidget *tb, gboolean active);
static void move_next_tab(GaimConversation *conv);
static void do_bold(GtkWidget *bold, GaimGtkConversation *gtkconv);
static void do_italic(GtkWidget *italic, GaimGtkConversation *gtkconv);
static void do_underline(GtkWidget *underline, GaimGtkConversation *gtkconv);
static void do_small(GtkWidget *small, GaimGtkConversation *gtkconv);
static void do_normal(GtkWidget *small, GaimGtkConversation *gtkconv);
static void do_big(GtkWidget *small, GaimGtkConversation *gtkconv);
static void toggle_font(GtkWidget *font, GaimConversation *conv);
static void toggle_fg_color(GtkWidget *color, GaimConversation *conv);
static void toggle_bg_color(GtkWidget *color, GaimConversation *conv);
static void got_typing_keypress(GaimConversation *conv, gboolean first);
static GList *generate_invite_user_names(GaimConnection *gc);
static void add_chat_buddy_common(GaimConversation *conv,
								  const char *name, int pos);
static void tab_complete(GaimConversation *conv);
static void update_typing_icon(GaimConversation *conv);
static gboolean update_send_as_selection(GaimWindow *win);
static char *item_factory_translate_func (const char *path, gpointer func_data);

/**************************************************************************
 * Callbacks
 **************************************************************************/
static void
do_insert_image_cb(GObject *obj, GtkWidget *wid)
{
	GaimConversation *conv;
	GaimGtkConversation *gtkconv;
	GaimIm *im;
	const char *name;
	const char *filename;
	char *buf;
	struct stat st;
	int id;

	conv    = g_object_get_data(G_OBJECT(wid), "user_data");
	gtkconv = GAIM_GTK_CONVERSATION(conv);
	im      = GAIM_IM(conv);
	name    = gtk_file_selection_get_filename(GTK_FILE_SELECTION(wid));
	id      = g_slist_length(im->images) + 1;
	
	if (file_is_dir(name, wid))
		return;

	gtk_widget_destroy(wid);

	if (!name)
		return;

	if (stat(name, &st) != 0) {
		gaim_debug(GAIM_DEBUG_ERROR, "gtkconv",
				   "Could not stat image %s\n", name);
		return;
	}

	filename = name;
	while (strchr(filename, '/')) 
		filename = strchr(filename, '/') + 1;

	buf = g_strdup_printf("<IMG SRC=\"file://%s\" ID=\"%d\" DATASIZE=\"%d\">",
						  filename, id, (int)st.st_size);
	im->images = g_slist_append(im->images, g_strdup(name));
	gtk_text_buffer_insert_at_cursor(GTK_TEXT_BUFFER(gtkconv->entry_buffer),
									 buf, -1);
	g_free(buf);

	set_toggle(gtkconv->toolbar.image, FALSE);
}

static gint
close_win_cb(GtkWidget *w, GdkEventAny *e, gpointer d)
{
	GaimWindow *win = (GaimWindow *)d;

	gaim_window_destroy(win);

	return TRUE;
}

static gint
close_conv_cb(GtkWidget *w, gpointer d)
{
	GaimConversation *conv = (GaimConversation *)d;

	gaim_conversation_destroy(conv);

	return TRUE;
}

static void
cancel_insert_image_cb(GtkWidget *unused, GaimGtkConversation *gtkconv)
{
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtkconv->toolbar.image),
								 FALSE);

	if (gtkconv->dialogs.image)
		gtk_widget_destroy(gtkconv->dialogs.image);

	gtkconv->dialogs.image = NULL;
}

static void
insert_image_cb(GtkWidget *save, GaimConversation *conv)
{
	GaimGtkConversation *gtkconv;
	char buf[BUF_LONG];
	GtkWidget *window;

	gtkconv = GAIM_GTK_CONVERSATION(conv);

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtkconv->toolbar.image))) {
		window = gtk_file_selection_new(_("Gaim - Insert Image"));
		g_snprintf(buf, sizeof(buf), "%s" G_DIR_SEPARATOR_S, gaim_home_dir());
		gtk_file_selection_set_filename(GTK_FILE_SELECTION(window), buf);

		g_object_set_data(G_OBJECT(window), "user_data", conv);
		g_signal_connect(G_OBJECT(GTK_FILE_SELECTION(window)->ok_button),
				"clicked", G_CALLBACK(do_insert_image_cb), window);
		g_signal_connect(G_OBJECT(GTK_FILE_SELECTION(window)->cancel_button),
						 "clicked", G_CALLBACK(cancel_insert_image_cb), gtkconv);

		gtk_widget_show(window);
		gtkconv->dialogs.image = window;
	} else {
		gtk_widget_grab_focus(gtkconv->entry);
		if(gtkconv->dialogs.image)
			gtk_widget_destroy(gtkconv->dialogs.image);
		gtkconv->dialogs.image = NULL;
	}
}

static void
insert_link_cb(GtkWidget *w, GaimConversation *conv)
{
	GaimGtkConversation *gtkconv;

	gtkconv = GAIM_GTK_CONVERSATION(conv);

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtkconv->toolbar.link)))
		show_insert_link(gtkconv->toolbar.link, conv);
	else if (gtkconv->dialogs.link)
		cancel_link(gtkconv->toolbar.link, conv);
	else
		gaim_gtk_advance_past(gtkconv, "<A HREF>", "</A>");

	gtk_widget_grab_focus(gtkconv->entry);
}

static void
insert_smiley_cb(GtkWidget *smiley, GaimConversation *conv)
{
	GaimGtkConversation *gtkconv;

	gtkconv = GAIM_GTK_CONVERSATION(conv);

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(smiley)))
		show_smiley_dialog(conv, smiley);
	else if (gtkconv->dialogs.smiley)
		close_smiley_dialog(smiley, conv);

	gtk_widget_grab_focus(gtkconv->entry);
}

static void
menu_save_as_cb(gpointer data, guint action, GtkWidget *widget)
{
	GaimWindow *win = (GaimWindow *)data;

	save_convo(NULL, gaim_window_get_active_conversation(win));
}

static void
menu_view_log_cb(gpointer data, guint action, GtkWidget *widget)
{
	GaimWindow *win = (GaimWindow *)data;
	GaimConversation *conv;

	conv = gaim_window_get_active_conversation(win);

	conv_show_log(NULL, (char *)gaim_conversation_get_name(conv));
}
static void
menu_insert_link_cb(gpointer data, guint action, GtkWidget *widget)
{
	GaimWindow *win = (GaimWindow *)data;
	GaimConversation *conv;
	GaimGtkConversation *gtkconv;

	conv    = gaim_window_get_active_conversation(win);
	gtkconv = GAIM_GTK_CONVERSATION(conv);

	show_insert_link(gtkconv->toolbar.link, conv);
}

static void
menu_insert_image_cb(gpointer data, guint action, GtkWidget *widget)
{
	GaimWindow *win = (GaimWindow *)data;
	GaimGtkConversation *gtkconv;

	gtkconv = GAIM_GTK_CONVERSATION(gaim_window_get_active_conversation(win));

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gtkconv->toolbar.image),
		!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gtkconv->toolbar.image)));
}

static void
menu_close_conv_cb(gpointer data, guint action, GtkWidget *widget)
{
	GaimWindow *win = (GaimWindow *)data;

	close_conv_cb(NULL, gaim_window_get_active_conversation(win));
}

static void
menu_logging_cb(gpointer data, guint action, GtkWidget *widget)
{
	GaimWindow *win = (GaimWindow *)data;
	GaimConversation *conv;

	conv = gaim_window_get_active_conversation(win);

	if (conv == NULL)
		return;

	gaim_conversation_set_logging(conv,
			gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)));
}

static void
menu_sounds_cb(gpointer data, guint action, GtkWidget *widget)
{
	GaimWindow *win = (GaimWindow *)data;
	GaimConversation *conv;
	GaimGtkConversation *gtkconv;

	conv = gaim_window_get_active_conversation(win);

	if (!conv)
		return;

	gtkconv = GAIM_GTK_CONVERSATION(conv);

	gtkconv->make_sound =
		gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));
}

static gboolean
entry_key_pressed_cb_1(GtkTextBuffer *buffer)
{
	check_everything(buffer);

	return FALSE;
}

static void
send_cb(GtkWidget *widget, GaimConversation *conv)
{
	GaimGtkConversation *gtkconv;
	char *buf, *buf2;
	GtkTextIter start_iter, end_iter;
	int limit;
	GaimConnection *gc = gaim_conversation_get_gc(conv);

	gtkconv = GAIM_GTK_CONVERSATION(conv);

	gtk_text_buffer_get_start_iter(gtkconv->entry_buffer, &start_iter);
	gtk_text_buffer_get_end_iter(gtkconv->entry_buffer, &end_iter);
	buf2 = gtk_text_buffer_get_text(gtkconv->entry_buffer,
									&start_iter, &end_iter, FALSE);

	set_toggle(gtkconv->toolbar.bold,        FALSE);
	set_toggle(gtkconv->toolbar.italic,      FALSE);
	set_toggle(gtkconv->toolbar.underline,   FALSE);
	set_toggle(gtkconv->toolbar.larger_size, FALSE);
	set_toggle(gtkconv->toolbar.normal_size, FALSE);
	set_toggle(gtkconv->toolbar.smaller_size,FALSE);
	set_toggle(gtkconv->toolbar.font,        FALSE);
	set_toggle(gtkconv->toolbar.fgcolor,     FALSE);
	set_toggle(gtkconv->toolbar.bgcolor,     FALSE);
	set_toggle(gtkconv->toolbar.link,        FALSE);

	gtk_widget_grab_focus(gtkconv->entry);

	limit = 32 * 1024; /* This will be done again in gaim_im_send. *shrug* */

	buf = g_malloc(limit);
	strncpy(buf, buf2, limit);

	g_free(buf2);

	if (strlen(buf) == 0) {
		g_free(buf);

		return;
	}

	buf2 = g_malloc(limit);

	if (gc && gc->flags & OPT_CONN_HTML) {
		if (gaim_prefs_get_bool("/gaim/gtk/conversations/send_bold")) {
			g_snprintf(buf2, limit, "<B>%s</B>", buf);
			strcpy(buf, buf2);
		}

		if (gaim_prefs_get_bool("/gaim/gtk/conversations/send_italic")) {
			g_snprintf(buf2, limit, "<I>%s</I>", buf);
			strcpy(buf, buf2);
		}

		if (gaim_prefs_get_bool("/gaim/gtk/conversations/send_underline")) {
			g_snprintf(buf2, limit, "<U>%s</U>", buf);
			strcpy(buf, buf2);
		}

		if (gaim_prefs_get_bool("/gaim/gtk/conversations/send_strikethrough")) {
			g_snprintf(buf2, limit, "<STRIKE>%s</STRIKE>", buf);
			strcpy(buf, buf2);
		}

		if (gaim_prefs_get_bool("/gaim/gtk/conversations/use_custom_font") ||
			gtkconv->has_font) {

			g_snprintf(buf2, limit,
					   "<FONT FACE=\"%s\">%s</FONT>", gtkconv->fontface, buf);
			strcpy(buf, buf2);
		}

		if (gaim_prefs_get_bool("/gaim/gtk/conversations/use_custom_size")) {
			g_snprintf(buf2, limit,
					   "<FONT SIZE=\"%d\">%s</FONT>", fontsize, buf);
			strcpy(buf, buf2);
		}

		if (gaim_prefs_get_bool("/gaim/gtk/conversations/use_custom_fgcolor")) {
			g_snprintf(buf2, limit,
					   "<FONT COLOR=\"#%02X%02X%02X\">%s</FONT>",
					   gtkconv->fg_color.red   / 256,
					   gtkconv->fg_color.green / 256,
					   gtkconv->fg_color.blue  / 256, buf);
			strcpy(buf, buf2);
		}

		if (gaim_prefs_get_bool("/gaim/gtk/conversations/use_custom_bgcolor")) {
			g_snprintf(buf2, limit,
					   "<BODY BGCOLOR=\"#%02X%02X%02X\">%s</BODY>",
					   gtkconv->bg_color.red   / 256,
					   gtkconv->bg_color.green / 256,
					   gtkconv->bg_color.blue  / 256, buf);
			strcpy(buf, buf2);
		}
	}

	g_free(buf2);

	if (gaim_conversation_get_type(conv) == GAIM_CONV_IM)
		gaim_im_send(GAIM_IM(conv), buf);
	else
		gaim_chat_send(GAIM_CHAT(conv), buf);

	if (gaim_prefs_get_bool("/gaim/gtk/conversations/im/hide_on_send"))
		gaim_window_hide(gaim_conversation_get_window(conv));

	g_free(buf);

	gtk_text_buffer_set_text(gtkconv->entry_buffer, "", -1);
}

static void
add_cb(GtkWidget *widget, GaimConversation *conv)
{
	GaimConnection *gc;
	struct buddy *b;
	const char *name;

	gc   = gaim_conversation_get_gc(conv);
	name = gaim_conversation_get_name(conv);
	b    = gaim_find_buddy(gc->account, name);

	if (b != NULL)
		show_confirm_del(gc, (char *)name);
	else if (gc != NULL)
		show_add_buddy(gc, (char *)name, NULL, NULL);

	gtk_widget_grab_focus(GAIM_GTK_CONVERSATION(conv)->entry);
}

static void
info_cb(GtkWidget *widget, GaimConversation *conv)
{
	GaimGtkConversation *gtkconv;

	gtkconv = GAIM_GTK_CONVERSATION(conv);

	if (gaim_conversation_get_type(conv) == GAIM_CONV_CHAT) {
		GaimGtkChatPane *gtkchat;
		GtkTreeIter iter;
		GtkTreeModel *model;
		GtkTreeSelection *sel;
		const char *name;

		gtkchat = gtkconv->u.chat;

		model = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));
		sel   = gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkchat->list));

		if (gtk_tree_selection_get_selected(sel, NULL, &iter))
			gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 1, &name, -1);
		else
			return;

		serv_get_info(gaim_conversation_get_gc(conv), (char *)name);
	}
	else {
		serv_get_info(gaim_conversation_get_gc(conv),
					  (char *)gaim_conversation_get_name(conv));

		gtk_widget_grab_focus(gtkconv->entry);
	}
}

static void
warn_cb(GtkWidget *widget, GaimConversation *conv)
{
	show_warn_dialog(gaim_conversation_get_gc(conv),
					 (char *)gaim_conversation_get_name(conv));

	gtk_widget_grab_focus(GAIM_GTK_CONVERSATION(conv)->entry);
}

static void
block_cb(GtkWidget *widget, GaimConversation *conv)
{
	GaimConnection *gc;

	gc = gaim_conversation_get_gc(conv);

	if (gc != NULL)
		show_add_perm(gc, (char *)gaim_conversation_get_name(conv), FALSE);

	gtk_widget_grab_focus(GAIM_GTK_CONVERSATION(conv)->entry);
}

void
im_cb(GtkWidget *widget, GaimConversation *conv)
{
	GaimConversation *conv2;
	GaimGtkConversation *gtkconv;
	GaimGtkChatPane *gtkchat;
	GaimAccount *account;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeSelection *sel;
	const char *name;

	gtkconv = GAIM_GTK_CONVERSATION(conv);
	gtkchat = gtkconv->u.chat;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));
	sel   = gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkchat->list));

	if (gtk_tree_selection_get_selected(sel, NULL, &iter))
		gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 1, &name, -1);
	else
		return;

	if (*name == '@') name++;
	if (*name == '%') name++;
	if (*name == '+') name++;

	account = gaim_conversation_get_account(conv);

	conv2 = gaim_find_conversation(name);

	if (conv2 != NULL) {
		gaim_window_raise(gaim_conversation_get_window(conv2));
		gaim_conversation_set_account(conv2, account);
	}
	else
		conv2 = gaim_conversation_new(GAIM_CONV_IM, account, name);
}

static void
ignore_cb(GtkWidget *w, GaimConversation *conv)
{
	GaimGtkConversation *gtkconv;
	GaimGtkChatPane *gtkchat;
	GaimChat *chat;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeSelection *sel;
	const char *name;
	int pos;

	chat    = GAIM_CHAT(conv);
	gtkconv = GAIM_GTK_CONVERSATION(conv);
	gtkchat = gtkconv->u.chat;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));
	sel   = gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkchat->list));

	if (gtk_tree_selection_get_selected(sel, NULL, &iter)) {
		gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 1, &name, -1);
		gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
	}
	else
		return;

	pos = g_list_index(gaim_chat_get_users(chat), name);

	if (gaim_chat_is_user_ignored(chat, name))
		gaim_chat_unignore(chat, name);
	else
		gaim_chat_ignore(chat, name);

	add_chat_buddy_common(conv, name, pos);
}

static void
menu_im_cb(GtkWidget *w, GaimConversation *conv)
{
	const char *who;
	GaimConversation *conv2;
	GaimAccount *account;

	who = g_object_get_data(G_OBJECT(w), "user_data");

	account = gaim_conversation_get_account(conv);

	conv2 = gaim_find_conversation(who);

	if (conv2 != NULL)
		gaim_window_show(gaim_conversation_get_window(conv2));
	else
		conv2 = gaim_conversation_new(GAIM_CONV_IM, account, who);
}

static void
menu_info_cb(GtkWidget *w, GaimConversation *conv)
{
	GaimPluginProtocolInfo *prpl_info = NULL;
	GaimConnection *gc;
	char *who;

	gc = gaim_conversation_get_gc(conv);
	who = g_object_get_data(G_OBJECT(w), "user_data");

	if (gc != NULL) {
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

		/*
		 * If there are special needs for getting info on users in
		 * buddy chat "rooms"...
		 */
		if (prpl_info->get_cb_info != NULL)
			prpl_info->get_cb_info(gc, gaim_chat_get_id(GAIM_CHAT(conv)), who);
		else
			prpl_info->get_info(gc, who);
	}
}

static void
menu_away_cb(GtkWidget *w, GaimConversation *conv)
{
	GaimPluginProtocolInfo *prpl_info = NULL;
	GaimConnection *gc;
	char *who;

	gc  = gaim_conversation_get_gc(conv);
	who = g_object_get_data(G_OBJECT(w), "user_data");

	if (gc != NULL) {
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

		/*
		 * May want to expand this to work similarly to menu_info_cb?
		 */

		if (prpl_info->get_cb_away != NULL)
			prpl_info->get_cb_away(gc, gaim_chat_get_id(GAIM_CHAT(conv)), who);
	}
}

static void
menu_add_cb(GtkWidget *w, GaimConversation *conv)
{
	GaimConnection *gc;
	struct buddy *b;
	char *name;

	gc   = gaim_conversation_get_gc(conv);
	name = g_object_get_data(G_OBJECT(w), "user_data");
	b    = gaim_find_buddy(gc->account, name);

	if (b != NULL)
		show_confirm_del(gc, name);
	else if (gc != NULL)
		show_add_buddy(gc, name, NULL, NULL);

	gtk_widget_grab_focus(GAIM_GTK_CONVERSATION(conv)->entry);
}

static gint
right_click_chat_cb(GtkWidget *widget, GdkEventButton *event,
					GaimConversation *conv)
{
	GaimPluginProtocolInfo *prpl_info = NULL;
	GaimGtkConversation *gtkconv;
	GaimGtkChatPane *gtkchat;
	GaimConnection *gc;
	GaimAccount *account;
	GtkTreePath *path;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeViewColumn *column;
	gchar *who;
	int x, y;

	gtkconv = GAIM_GTK_CONVERSATION(conv);
	gtkchat = gtkconv->u.chat;
	account = gaim_conversation_get_account(conv);
	gc      = account->gc;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));

	gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(gtkchat->list),
								  event->x, event->y, &path, &column, &x, &y);

	if (path == NULL)
		return FALSE;

	if (gc != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	gtk_tree_selection_select_path(GTK_TREE_SELECTION(
			gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkchat->list))), path);

	gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, path);
	gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 1, &who, -1);

	if (*who == '@') who++;
	if (*who == '%') who++;
	if (*who == '+') who++;

	if (event->button == 1 && event->type == GDK_2BUTTON_PRESS) {
		GaimConversation *c;

		if ((c = gaim_find_conversation(who)) == NULL)
			c = gaim_conversation_new(GAIM_CONV_IM, account, who);
		else
			gaim_conversation_set_account(c, account);
	}
	else if (event->button == 3 && event->type == GDK_BUTTON_PRESS) {
		static GtkWidget *menu = NULL;
		GtkWidget *button;

		/*
		 * If a menu already exists, destroy it before creating a new one,
		 * thus freeing-up the memory it occupied.
		 */

		if (menu)
			gtk_widget_destroy(menu);

		menu = gtk_menu_new();

		button = gtk_menu_item_new_with_label(_("IM"));
		g_signal_connect(G_OBJECT(button), "activate",
						 G_CALLBACK(menu_im_cb), conv);
		g_object_set_data(G_OBJECT(button), "user_data", who);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), button);
		gtk_widget_show(button);

		if (gaim_chat_is_user_ignored(GAIM_CHAT(conv), who))
			button = gtk_menu_item_new_with_label(_("Un-Ignore"));
		else
			button = gtk_menu_item_new_with_label(_("Ignore"));

		g_signal_connect(G_OBJECT(button), "activate",
						 G_CALLBACK(ignore_cb), conv);
		g_object_set_data(G_OBJECT(button), "user_data", who);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), button);
		gtk_widget_show(button);

		if (gc && prpl_info->get_info) {
			button = gtk_menu_item_new_with_label(_("Info"));
			g_signal_connect(G_OBJECT(button), "activate",
							 G_CALLBACK(menu_info_cb), conv);
			g_object_set_data(G_OBJECT(button), "user_data", who);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), button);
			gtk_widget_show(button);
		}

		if (gc && prpl_info->get_cb_away) {
			button = gtk_menu_item_new_with_label(_("Get Away Msg"));
			g_signal_connect(G_OBJECT(button), "activate",
							 G_CALLBACK(menu_away_cb), conv);
			g_object_set_data(G_OBJECT(button), "user_data", who);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), button);
			gtk_widget_show(button);
		}

		/* Added by Jonas <jonas@birme.se> */
		if (gc) {
			if (gaim_find_buddy(gc->account, who))
				button = gtk_menu_item_new_with_label(_("Remove"));
			else
				button = gtk_menu_item_new_with_label(_("Add"));

			g_signal_connect(G_OBJECT(button), "activate",
							 G_CALLBACK(menu_add_cb), conv);

			g_object_set_data(G_OBJECT(button), "user_data", who);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), button);
			gtk_widget_show(button);
		}
		/* End Jonas */

		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
					   event->button, event->time);
	}

	return TRUE;
}

static void
do_invite(GtkWidget *w, int resp, InviteBuddyInfo *info)
{
	const char *buddy, *message;
	GaimGtkConversation *gtkconv;

	gtkconv = GAIM_GTK_CONVERSATION(info->conv);

	if (resp == GTK_RESPONSE_OK) {
		buddy   = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(info->entry)->entry));
		message = gtk_entry_get_text(GTK_ENTRY(info->message));

		if (!g_ascii_strcasecmp(buddy, "")) {
			g_free(info);

			return;
		}

		serv_chat_invite(gaim_conversation_get_gc(info->conv),
						 gaim_chat_get_id(GAIM_CHAT(info->conv)),
						 message, buddy);
	}

	gtk_widget_destroy(invite_dialog);
	invite_dialog = NULL;

	g_free(info);
}

static void
invite_cb(GtkWidget *widget, GaimConversation *conv)
{
	InviteBuddyInfo *info = NULL;

	if (invite_dialog == NULL) {
		GaimConnection *gc;
		GaimWindow *win;
		GaimGtkWindow *gtkwin;
		GtkWidget *label;
		GtkWidget *vbox, *hbox;
		GtkWidget *table;
		GtkWidget *img;

		img = gtk_image_new_from_stock(GAIM_STOCK_DIALOG_QUESTION, GTK_ICON_SIZE_DIALOG);

		info = g_new0(InviteBuddyInfo, 1);
		info->conv = conv;

		gc     = gaim_conversation_get_gc(conv);
		win    = gaim_conversation_get_window(conv);
		gtkwin = GAIM_GTK_WINDOW(win);

		/* Create the new dialog. */
		invite_dialog = gtk_dialog_new_with_buttons(
			_("Gaim - Invite Buddy Into Chat Room"),
			GTK_WINDOW(gtkwin->window),
			GTK_DIALOG_MODAL, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);

		gtk_dialog_set_default_response(GTK_DIALOG(invite_dialog),
										GTK_RESPONSE_OK);
		gtk_container_set_border_width(GTK_CONTAINER(invite_dialog), 6);
		gtk_window_set_resizable(GTK_WINDOW(invite_dialog), FALSE);
		gtk_dialog_set_has_separator(GTK_DIALOG(invite_dialog), FALSE);

		/* Setup the outside spacing. */
		vbox = GTK_DIALOG(invite_dialog)->vbox;

		gtk_box_set_spacing(GTK_BOX(vbox), 12);
		gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);

		/* Setup the inner hbox and put the dialog's icon in it. */
		hbox = gtk_hbox_new(FALSE, 12);
		gtk_container_add(GTK_CONTAINER(vbox), hbox);
		gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
		gtk_misc_set_alignment(GTK_MISC(img), 0, 0);

		/* Setup the right vbox. */
		vbox = gtk_vbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER(hbox), vbox);

		/* Put our happy label in it. */
		label = gtk_label_new(_("Please enter the name of the user you wish "
								"to invite, along with an optional invite "
								"message."));
		gtk_widget_set_size_request(label, 350, -1);
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

		/* hbox for the table, and to give it some spacing on the left. */
		hbox = gtk_hbox_new(FALSE, 6);
		gtk_container_add(GTK_CONTAINER(vbox), hbox);

		/* Setup the table we're going to use to lay stuff out. */
		table = gtk_table_new(2, 2, FALSE);
		gtk_table_set_row_spacings(GTK_TABLE(table), 6);
		gtk_table_set_col_spacings(GTK_TABLE(table), 6);
		gtk_container_set_border_width(GTK_CONTAINER(table), 12);
		gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);

		/* Now the Buddy label */
		label = gtk_label_new(NULL);
		gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), _("_Buddy:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);

		/* Now the Buddy drop-down entry field. */
		info->entry = gtk_combo_new();
		gtk_combo_set_case_sensitive(GTK_COMBO(info->entry), FALSE);
		gtk_entry_set_activates_default(
				GTK_ENTRY(GTK_COMBO(info->entry)->entry), TRUE);

		gtk_table_attach_defaults(GTK_TABLE(table), info->entry, 1, 2, 0, 1);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), info->entry);

		/* Fill in the names. */
		gtk_combo_set_popdown_strings(GTK_COMBO(info->entry),
									  generate_invite_user_names(gc));


		/* Now the label for "Message" */
		label = gtk_label_new(NULL);
		gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), _("_Message:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);


		/* And finally, the Message entry field. */
		info->message = gtk_entry_new();
		gtk_entry_set_activates_default(GTK_ENTRY(info->message), TRUE);

		gtk_table_attach_defaults(GTK_TABLE(table), info->message, 1, 2, 1, 2);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), info->message);

		/* Connect the signals. */
		g_signal_connect(G_OBJECT(invite_dialog), "response",
						 G_CALLBACK(do_invite), info);
	}

	gtk_widget_show_all(invite_dialog);

	if (info != NULL)
		gtk_widget_grab_focus(GTK_COMBO(info->entry)->entry);
}

static gboolean
entry_key_pressed_cb_2(GtkWidget *entry, GdkEventKey *event, gpointer data)
{
	GaimWindow *win;
	GaimConversation *conv;
	GaimGtkConversation *gtkconv;
	GaimGtkWindow *gtkwin;

	conv    = (GaimConversation *)data;
	gtkconv = GAIM_GTK_CONVERSATION(conv);
	win     = gaim_conversation_get_window(conv);
	gtkwin  = GAIM_GTK_WINDOW(win);

	if (event->keyval == GDK_Escape) {
		if (gaim_prefs_get_bool("/gaim/gtk/conversations/escape_closes")) {
			g_signal_stop_emission_by_name(G_OBJECT(entry), "key_press_event");
			gaim_conversation_destroy(conv);
		}
	}
	else if (event->keyval == GDK_Page_Up) {
		g_signal_stop_emission_by_name(G_OBJECT(entry), "key_press_event");

		if (!(event->state & GDK_CONTROL_MASK))
			gtk_imhtml_page_up(GTK_IMHTML(gtkconv->imhtml));
	}
	else if (event->keyval == GDK_Page_Down) {
		g_signal_stop_emission_by_name(G_OBJECT(entry), "key_press_event");

		if (!(event->state & GDK_CONTROL_MASK))
			gtk_imhtml_page_down(GTK_IMHTML(gtkconv->imhtml));
	}
	else if (event->keyval == GDK_F2 &&
			 gaim_prefs_get_bool("/gaim/gtk/conversations/f2_toggles_timestamps")) {
		gtk_imhtml_show_comments(GTK_IMHTML(gtkconv->imhtml),
								 !GTK_IMHTML(gtkconv->imhtml)->comments);
	}
	else if (event->keyval == GDK_Return || event->keyval == GDK_KP_Enter) {
		if ((event->state & GDK_CONTROL_MASK) &&
			gaim_prefs_get_bool("/gaim/gtk/conversations/ctrl_enter_sends")) {

			send_cb(NULL, conv);
			g_signal_stop_emission_by_name(G_OBJECT(entry), "key_press_event");

			return TRUE;
		}
		else if (!(event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) &&
				 gaim_prefs_get_bool("/gaim/gtk/conversations/enter_sends")) {

			send_cb(NULL, conv);
			g_signal_stop_emission_by_name(G_OBJECT(entry), "key_press_event");

			return TRUE;
		}

		return FALSE;
	}
	else if ((event->state & GDK_CONTROL_MASK) && (event->keyval == 'm')) {
		g_signal_stop_emission_by_name(G_OBJECT(entry), "key_press_event");
		gtk_text_buffer_insert_at_cursor(gtkconv->entry_buffer, "\n", 1);
	}
	else if (event->state & GDK_CONTROL_MASK) {
		switch (event->keyval) {
			case GDK_Up:
				if (!conv->send_history)
					break;

				if (!conv->send_history->prev) {
					GtkTextIter start, end;

					if (conv->send_history->data)
						g_free(conv->send_history->data);

					gtk_text_buffer_get_start_iter(gtkconv->entry_buffer,
												   &start);
					gtk_text_buffer_get_end_iter(gtkconv->entry_buffer, &end);

					conv->send_history->data =
						gtk_text_buffer_get_text(gtkconv->entry_buffer,
												 &start, &end, FALSE);
				}

				if (conv->send_history->next &&
					conv->send_history->next->data) {

					conv->send_history = conv->send_history->next;
					gtk_text_buffer_set_text(gtkconv->entry_buffer,
											 conv->send_history->data, -1);
				}

				break;

			case GDK_Down:
				if (!conv->send_history)
					break;

				if (conv->send_history->prev) {
					conv->send_history = conv->send_history->prev;

					if (conv->send_history->data)
						gtk_text_buffer_set_text(gtkconv->entry_buffer,
												 conv->send_history->data, -1);
				}

				break;
		}

		if (gaim_prefs_get_bool("/gaim/gtk/conversations/html_shortcuts")) {
			switch (event->keyval) {
				case 'i':
				case 'I':
					set_toggle(gtkconv->toolbar.italic,
						!gtk_toggle_button_get_active(
							GTK_TOGGLE_BUTTON(gtkconv->toolbar.italic)));

					g_signal_stop_emission_by_name(G_OBJECT(entry),
												 "key_press_event");
					break;

				case 'u':  /* ctrl-u is GDK_Clear, which clears the line. */
				case 'U':
					set_toggle(gtkconv->toolbar.underline,
						!gtk_toggle_button_get_active(
							GTK_TOGGLE_BUTTON(gtkconv->toolbar.underline)));

					g_signal_stop_emission_by_name(G_OBJECT(entry),
												 "key_press_event");
					break;

				case 'b':  /* ctrl-b is GDK_Left, which moves backwards. */
				case 'B':
					set_toggle(gtkconv->toolbar.bold,
						!gtk_toggle_button_get_active(
							GTK_TOGGLE_BUTTON(gtkconv->toolbar.bold)));

					g_signal_stop_emission_by_name(G_OBJECT(entry),
												 "key_press_event");
					break;
					
				case '-':
					do_small(NULL, gtkconv);
					
					g_signal_stop_emission_by_name(G_OBJECT(entry),
												 "key_press_event");
					break;

				case '=':
				case '+':
					do_big(NULL, gtkconv);

					g_signal_stop_emission_by_name(G_OBJECT(entry),
												 "key_press_event");
					break;

				case '0':
					set_toggle(gtkconv->toolbar.normal_size,
						!gtk_toggle_button_get_active(
							GTK_TOGGLE_BUTTON(gtkconv->toolbar.normal_size)));

					g_signal_stop_emission_by_name(G_OBJECT(entry),
												 "key_press_event");
					break;

				case 'f':
				case 'F':
					set_toggle(gtkconv->toolbar.font,
						!gtk_toggle_button_get_active(
							GTK_TOGGLE_BUTTON(gtkconv->toolbar.font)));

					g_signal_stop_emission_by_name(G_OBJECT(entry),
												 "key_press_event");
					break;
			}
		}

		if (gaim_prefs_get_bool("/gaim/gtk/conversations/smiley_shortcuts")) {
			char buf[7];

			*buf = '\0';

			switch (event->keyval) {
				case '1': strcpy(buf, ":-)");  break;
				case '2': strcpy(buf, ":-(");  break;
				case '3': strcpy(buf, ";-)");  break;
				case '4': strcpy(buf, ":-P");  break;
				case '5': strcpy(buf, "=-O");  break;
				case '6': strcpy(buf, ":-*");  break;
				case '7': strcpy(buf, ">:o");  break;
				case '8': strcpy(buf, "8-)");  break;
				case '!': strcpy(buf, ":-$");  break;
				case '@': strcpy(buf, ":-!");  break;
				case '#': strcpy(buf, ":-[");  break;
				case '$': strcpy(buf, "O:-)"); break;
				case '%': strcpy(buf, ":-/");  break;
				case '^': strcpy(buf, ":'(");  break;
				case '&': strcpy(buf, ":-X");  break;
				case '*': strcpy(buf, ":-D");  break;
				default: break;
			}

			if (*buf) {
				gtk_text_buffer_insert_at_cursor(gtkconv->entry_buffer,
												 buf, -1);
				g_signal_stop_emission_by_name(G_OBJECT(entry), "key_press_event");
			}
		}

		if (event->keyval == 'l') {
			gtk_imhtml_clear(GTK_IMHTML(gtkconv->imhtml));
			g_string_free(conv->history, TRUE);
			conv->history = g_string_new("");
		}
		else if (event->keyval == 'w' &&
				 gaim_prefs_get_bool("/gaim/gtk/conversations/ctrl_w_closes")) {

			g_signal_stop_emission_by_name(G_OBJECT(entry), "key_press_event");
			gaim_conversation_destroy(conv);
			return TRUE;
		}
		else if (event->keyval == 'n') {
			g_signal_stop_emission_by_name(G_OBJECT(entry), "key_press_event");

			show_im_dialog();
		}
		else if (event->keyval == 'z') {
			g_signal_stop_emission_by_name(G_OBJECT(entry), "key_press_event");

			gtk_window_iconify(GTK_WINDOW(gtkwin->window));
		}
		else if (event->keyval == '[') {
			gaim_window_switch_conversation(win,
				gaim_conversation_get_index(conv) - 1);

			g_signal_stop_emission_by_name(G_OBJECT(entry), "key_press_event");
		}
		else if (event->keyval == ']') {
			gaim_window_switch_conversation(win,
				gaim_conversation_get_index(conv) + 1);

			g_signal_stop_emission_by_name(G_OBJECT(entry), "key_press_event");
		}
		else if (event->keyval == GDK_Tab) {
			move_next_tab(conv);

			g_signal_stop_emission_by_name(G_OBJECT(entry), "key_press_event");

			return TRUE;
		}
	}
	else if (event->keyval == GDK_Tab &&
			 gaim_conversation_get_type(conv) == GAIM_CONV_CHAT &&
			 gaim_prefs_get_bool("/gaim/gtk/conversations/chat/tab_completion")) {

		tab_complete(conv);

		g_signal_stop_emission_by_name(G_OBJECT(entry), "key_press_event");

		return TRUE;
	}
	else if ((event->state & GDK_MOD1_MASK) &&
			 event->keyval > '0' && event->keyval <= '9') {
		
		gaim_window_switch_conversation(win, event->keyval - '1');

		g_signal_stop_emission_by_name(G_OBJECT(entry), "key_press_event");
	}

	return FALSE;
}

/*
 * NOTE:
 *   This guy just kills a single right click from being propagated any 
 *   further.  I  have no idea *why* we need this, but we do ...  It 
 *   prevents right clicks on the GtkTextView in a convo dialog from
 *   going all the way down to the notebook.  I suspect a bug in 
 *   GtkTextView, but I'm not ready to point any fingers yet.
 */
static gboolean
entry_stop_rclick_cb(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	if (event->button == 3 && event->type == GDK_BUTTON_PRESS) {
		/* Right single click */
		g_signal_stop_emission_by_name(G_OBJECT(widget), "button_press_event");

		return TRUE;
	}

	return FALSE;
}

static void
menu_conv_sel_send_cb(GObject *m, gpointer data)
{
	GaimWindow *win = g_object_get_data(m, "user_data");
	GaimAccount *account = g_object_get_data(m, "gaim_account");
	GaimConversation *conv;

	conv = gaim_window_get_active_conversation(win);

	gaim_conversation_set_account(conv, account);
}

static void
insert_text_cb(GtkTextBuffer *textbuffer, GtkTextIter *position,
			   gchar *new_text, gint new_text_length, gpointer user_data)
{
	GaimConversation *conv = (GaimConversation *)user_data;

	g_return_if_fail(conv != NULL);

	if (!gaim_prefs_get_bool("/core/conversations/im/send_typing"))
		return;

	got_typing_keypress(conv, (gtk_text_iter_is_start(position) &&
							   gtk_text_iter_is_end(position)));
}

static void
delete_text_cb(GtkTextBuffer *textbuffer, GtkTextIter *start_pos,
			   GtkTextIter *end_pos, gpointer user_data)
{
	GaimConversation *conv = (GaimConversation *)user_data;
	GaimIm *im;

	g_return_if_fail(conv != NULL);

	if (!gaim_prefs_get_bool("/core/conversations/im/send_typing"))
		return;

	im = GAIM_IM(conv);

	if (gtk_text_iter_is_start(start_pos) && gtk_text_iter_is_end(end_pos)) {

		/* We deleted all the text, so turn off typing. */
		if (gaim_im_get_type_again_timeout(im))
			gaim_im_stop_type_again_timeout(im);

		/* XXX The (char *) should go away! Somebody add consts to stuff! */
		serv_send_typing(gaim_conversation_get_gc(conv),
						 (char *)gaim_conversation_get_name(conv),
						 NOT_TYPING);
	}
	else {
		/* We're deleting, but not all of it, so it counts as typing. */
		got_typing_keypress(conv, FALSE);
	}
}

static void
notebook_init_grab(GaimGtkWindow *gtkwin, GtkWidget *widget)
{
	static GdkCursor *cursor = NULL;

	gtkwin->in_drag = TRUE;

	if (gtkwin->drag_leave_signal) {
		g_signal_handler_disconnect(G_OBJECT(widget),
									gtkwin->drag_leave_signal);

		gtkwin->drag_leave_signal = 0;
	}

	if (cursor == NULL)
		cursor = gdk_cursor_new(GDK_FLEUR);

	/* Grab the pointer */
	gtk_grab_add(gtkwin->notebook);
#ifndef _WIN32
	/* Currently for win32 GTK+ (as of 2.2.1), gdk_pointer_is_grabbed will
	   always be true after a button press. */
	if (!gdk_pointer_is_grabbed())
#endif
		gdk_pointer_grab(gtkwin->notebook->window, FALSE,
						 GDK_BUTTON1_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
						 NULL, cursor, GDK_CURRENT_TIME);
}

static gboolean
notebook_motion_cb(GtkWidget *widget, GdkEventButton *e, GaimWindow *win)
{
	GaimGtkWindow *gtkwin;

	gtkwin = GAIM_GTK_WINDOW(win);

	/*
	 * Make sure the user moved the mouse far enough for the
	 * drag to be initiated.
	 */
	if (gtkwin->in_predrag) {
		if (e->x_root <  gtkwin->drag_min_x ||
			e->x_root >= gtkwin->drag_max_x ||
			e->y_root <  gtkwin->drag_min_y ||
			e->y_root >= gtkwin->drag_max_y) {

			gtkwin->in_predrag = FALSE;
			notebook_init_grab(gtkwin, widget);
		}
	}
	else { /* Otherwise, draw the arrows. */
		GaimWindow *dest_win;
		GaimGtkWindow *dest_gtkwin;
		GtkNotebook *dest_notebook;
		GtkWidget *tab, *last_vis_tab = NULL;
		gint nb_x, nb_y, page_num, i, last_vis_tab_loc = -1;
		gint arrow1_x, arrow1_y, arrow2_x, arrow2_y;
		gboolean horiz_tabs = FALSE, tab_found = FALSE;
		GList *l;

		/* Get the window that the cursor is over. */
		dest_win = gaim_gtkwin_get_at_xy(e->x_root, e->y_root);

		if (dest_win == NULL) {
			dnd_hints_hide_all();

			return TRUE;
		}

		dest_gtkwin = GAIM_GTK_WINDOW(dest_win);

		dest_notebook = GTK_NOTEBOOK(dest_gtkwin->notebook);

		gdk_window_get_origin(GTK_WIDGET(dest_notebook)->window, &nb_x, &nb_y);

		arrow1_x = arrow2_x = nb_x;
		arrow1_y = arrow2_y = nb_y;

		page_num = gaim_gtkconv_get_dest_tab_at_xy(dest_win,
												   e->x_root, e->y_root);

		if (gtk_notebook_get_tab_pos(dest_notebook) == GTK_POS_TOP ||
			gtk_notebook_get_tab_pos(dest_notebook) == GTK_POS_BOTTOM) {

			horiz_tabs = TRUE;
		}

		/* Find out where to put the arrows. */
		for (l = gaim_window_get_conversations(dest_win), i = 0;
			 l != NULL;
			 l = l->next, i++) {

			GaimConversation *conv = l->data;

			tab = GAIM_GTK_CONVERSATION(conv)->tabby;

			/*
			 * If this is the correct tab, record the positions
			 * for the arrows.
			 */
			if (i == page_num) {
				if (horiz_tabs) {
					arrow1_x = arrow2_x = nb_x + tab->allocation.x;
					arrow1_y = nb_y + tab->allocation.y;
					arrow2_y = nb_y + tab->allocation.y +
					                  tab->allocation.height;
				}
				else {
					arrow1_x = nb_x + tab->allocation.x;
					arrow2_x = nb_x + tab->allocation.x +
					                  tab->allocation.width;
					arrow1_y = arrow2_y = nb_y + tab->allocation.y;
				}

				tab_found = TRUE;
				break;
			}
			else { /* Keep track of the right-most tab that we see. */
				if (horiz_tabs && tab->allocation.x > last_vis_tab_loc) {
					last_vis_tab     = tab;
					last_vis_tab_loc = tab->allocation.x;
				}
				else if (!horiz_tabs && tab->allocation.y > last_vis_tab_loc) {
					last_vis_tab     = tab;
					last_vis_tab_loc = tab->allocation.y;
				}
			}
		}

		/*
		 * If we didn't find the tab, then we'll just place the
		 * arrows to the right/bottom of the last visible tab.
		 */
		if (!tab_found && last_vis_tab) {
			if (horiz_tabs) {
				arrow1_x = arrow2_x = nb_x + last_vis_tab->allocation.x +
				                             last_vis_tab->allocation.width;
				arrow1_y = nb_y + last_vis_tab->allocation.y;
				arrow2_y = nb_y + last_vis_tab->allocation.y +
				                  last_vis_tab->allocation.height;
			}
			else {
				arrow1_x = nb_x + last_vis_tab->allocation.x;
				arrow2_x = nb_x + last_vis_tab->allocation.x +
				                  last_vis_tab->allocation.width;
				arrow1_y = arrow2_y = nb_y + last_vis_tab->allocation.y +
				                             last_vis_tab->allocation.height;
			}
		}

		if (horiz_tabs) {
			dnd_hints_show(HINT_ARROW_DOWN, arrow1_x, arrow1_y);
			dnd_hints_show(HINT_ARROW_UP,   arrow2_x, arrow2_y);
		}
		else {
			dnd_hints_show(HINT_ARROW_RIGHT, arrow1_x, arrow1_y);
			dnd_hints_show(HINT_ARROW_LEFT,  arrow2_x, arrow2_y);
		}
	}

	return TRUE;
}

static gboolean
notebook_leave_cb(GtkWidget *widget, GdkEventCrossing *e, GaimWindow *win)
{
	GaimGtkWindow *gtkwin;

	gtkwin = GAIM_GTK_WINDOW(win);

	if (gtkwin->in_drag)
		return FALSE;

	if (e->x_root <  gtkwin->drag_min_x ||
		e->x_root >= gtkwin->drag_max_x ||
		e->y_root <  gtkwin->drag_min_y ||
		e->y_root >= gtkwin->drag_max_y) {

		gtkwin->in_predrag = FALSE;
		notebook_init_grab(gtkwin, widget);
	}

	return TRUE;
}

/*
 * THANK YOU GALEON!
 */
static gboolean
notebook_press_cb(GtkWidget *widget, GdkEventButton *e, GaimWindow *win)
{
	GaimGtkWindow *gtkwin;
	gint nb_x, nb_y, x_rel, y_rel;
	GList *l;
	int tab_clicked;

	if (e->button != 1 || e->type != GDK_BUTTON_PRESS)
		return FALSE;

	gtkwin = GAIM_GTK_WINDOW(win);

	if (gtkwin->in_drag) {
		gaim_debug(GAIM_DEBUG_WARNING, "gtkconv",
				   "Already in the middle of a window drag at tab_press_cb\n");
		return TRUE;
	}

	/* 
	 * Make sure a tab was actually clicked. The arrow buttons
	 * mess things up.
	 */
	tab_clicked = gaim_gtkconv_get_tab_at_xy(win, e->x_root, e->y_root);

	if (tab_clicked == -1)
		return FALSE;

	/*
	 * Get the relative position of the press event, with regards to
	 * the position of the notebook.
	 */
	gdk_window_get_origin(gtkwin->notebook->window, &nb_x, &nb_y);

	x_rel = e->x_root - nb_x;
	y_rel = e->y_root - nb_y;

	/* Reset the min/max x/y */
	gtkwin->drag_min_x = 0;
	gtkwin->drag_min_y = 0;
	gtkwin->drag_max_x = 0;
	gtkwin->drag_max_y = 0;

	/* Find out which tab was dragged. */
	for (l = gaim_window_get_conversations(win); l != NULL; l = l->next) {
		GaimConversation *conv = l->data;
		GtkWidget *tab = GAIM_GTK_CONVERSATION(conv)->tabby;

		if (!GTK_WIDGET_VISIBLE(tab))
			continue;

		if (tab->allocation.x > x_rel || tab->allocation.y > y_rel)
			break;

		/* Save the borders of the tab. */
		gtkwin->drag_min_x = tab->allocation.x      + nb_x;
		gtkwin->drag_min_y = tab->allocation.y      + nb_y;
		gtkwin->drag_max_x = tab->allocation.width  + gtkwin->drag_min_x;
		gtkwin->drag_max_y = tab->allocation.height + gtkwin->drag_min_y;
	}

	/* Make sure the click occurred in the tab. */
	if (e->x_root <  gtkwin->drag_min_x ||
		e->x_root >= gtkwin->drag_max_x ||
		e->y_root <  gtkwin->drag_min_y ||
		e->y_root >= gtkwin->drag_max_y) {

		return FALSE;
	}

	gtkwin->in_predrag = TRUE;

	/* Connect the new motion signals. */
	gtkwin->drag_motion_signal =
		g_signal_connect(G_OBJECT(widget), "motion_notify_event",
						 G_CALLBACK(notebook_motion_cb), win);

	gtkwin->drag_leave_signal =
		g_signal_connect(G_OBJECT(widget), "leave_notify_event",
						 G_CALLBACK(notebook_leave_cb), win);

	return FALSE;
}

static gboolean
notebook_release_cb(GtkWidget *widget, GdkEventButton *e, GaimWindow *win)
{
	GaimWindow *dest_win;
	GaimGtkWindow *gtkwin;
	GaimGtkWindow *dest_gtkwin;
	GaimConversation *conv;
	GtkNotebook *dest_notebook;
	gint dest_page_num;

	/*
	 * Don't check to make sure that the event's window matches the
	 * widget's, because we may be getting an event passed on from the
	 * close button.
	 */
	if (e->button != 1 && e->type != GDK_BUTTON_RELEASE)
		return FALSE;

	if (gdk_pointer_is_grabbed()) {
		gdk_pointer_ungrab(GDK_CURRENT_TIME);
		gtk_grab_remove(widget);
	}

	gtkwin = GAIM_GTK_WINDOW(win);

	if (!gtkwin->in_predrag && !gtkwin->in_drag)
		return FALSE;

	/* Disconnect the motion signal. */
	if (gtkwin->drag_motion_signal) {
		g_signal_handler_disconnect(G_OBJECT(widget),
									gtkwin->drag_motion_signal);

		gtkwin->drag_motion_signal = 0;
	}

	/*
	 * If we're in a pre-drag, we'll also need to disconnect the leave
	 * signal.
	 */
	if (gtkwin->in_predrag) {
		gtkwin->in_predrag = FALSE;

		if (gtkwin->drag_leave_signal) {
			g_signal_handler_disconnect(G_OBJECT(widget),
										gtkwin->drag_leave_signal);

			gtkwin->drag_leave_signal = 0;
		}
	}

	/* If we're not in drag...        */
	/* We're perfectly normal people! */
	if (!gtkwin->in_drag)
		return FALSE;

	gtkwin->in_drag = FALSE;

	dnd_hints_hide_all();

	dest_win = gaim_gtkwin_get_at_xy(e->x_root, e->y_root);

	conv = gaim_window_get_active_conversation(win);

	if (dest_win == NULL) {
		if (gaim_window_get_conversation_count(win) < 2)
			return FALSE;

		if (gaim_window_get_conversation_count(win) > 1) {
			/* Make a new window to stick this to. */
			GaimWindow *new_win;
			GaimGtkWindow *new_gtkwin;
			GaimGtkConversation *gtkconv;
			gint win_width, win_height;

			gtkconv = GAIM_GTK_CONVERSATION(conv);

			new_win = gaim_window_new();

			gaim_window_add_conversation(new_win,
					gaim_window_remove_conversation(win,
							gaim_conversation_get_index(conv)));

			new_gtkwin = GAIM_GTK_WINDOW(new_win);

			gtk_window_get_size(GTK_WINDOW(new_gtkwin->window),
								&win_width, &win_height);

			gtk_window_move(GTK_WINDOW(new_gtkwin->window),
							e->x_root - (win_width  / 2),
							e->y_root - (win_height / 2));

			gaim_window_show(new_win);
		}

		return TRUE;
	}

	dest_gtkwin = GAIM_GTK_WINDOW(dest_win);

	/* Get the destination notebook. */
	dest_notebook = GTK_NOTEBOOK(gtkwin->notebook);

	/* Get the destination page number. */
	dest_page_num = gaim_gtkconv_get_dest_tab_at_xy(dest_win,
													e->x_root, e->y_root);

	if (win == dest_win) {
		gaim_window_move_conversation(win,
			gaim_conversation_get_index(conv), dest_page_num);
	}
	else {
		size_t pos;

		gaim_window_remove_conversation(win,
			gaim_conversation_get_index(conv));

		pos = gaim_window_add_conversation(dest_win, conv);

		gaim_window_move_conversation(dest_win, pos, dest_page_num);

		gaim_window_switch_conversation(dest_win, dest_page_num);
	}

	gtk_widget_grab_focus(GAIM_GTK_CONVERSATION(conv)->entry);

	return TRUE;
}

static void
switch_conv_cb(GtkNotebook *notebook, GtkWidget *page, gint page_num,
				gpointer user_data)
{
	GaimPluginProtocolInfo *prpl_info = NULL;
	GaimWindow *win;
	GaimConversation *conv;
	GaimGtkConversation *gtkconv;
	GaimGtkWindow *gtkwin;
	GaimConnection *gc;

	win = (GaimWindow *)user_data;

	conv = gaim_window_get_conversation_at(win, page_num);

	g_return_if_fail(conv != NULL);

	gc      = gaim_conversation_get_gc(conv);
	gtkwin  = GAIM_GTK_WINDOW(win);
	gtkconv = GAIM_GTK_CONVERSATION(conv);

	gaim_conversation_set_unseen(conv, GAIM_UNSEEN_NONE);

	if (gc != NULL) {
		gtk_widget_set_sensitive(gtkwin->menu.insert_link, TRUE);
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);
	}

	/* Update the menubar */
	if (gaim_conversation_get_type(conv) == GAIM_CONV_IM) {
		gtk_widget_set_sensitive(gtkwin->menu.view_log, TRUE);
		gtk_widget_set_sensitive(gtkwin->menu.insert_image,
			(gc && prpl_info->options & OPT_PROTO_IM_IMAGE));

		if (gtkwin->menu.send_as != NULL)
			g_timeout_add(0, (GSourceFunc)update_send_as_selection, win);
	}
	else {
		gtk_widget_set_sensitive(gtkwin->menu.view_log, FALSE);
		gtk_widget_set_sensitive(gtkwin->menu.insert_image, FALSE);

		if (gtkwin->menu.send_as != NULL)
			gtk_widget_hide(gtkwin->menu.send_as);
	}

	update_typing_icon(conv);

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtkwin->menu.logging),
								   gaim_conversation_is_logging(conv));

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtkwin->menu.sounds),
								   gtkconv->make_sound);

	gtk_widget_grab_focus(gtkconv->entry);

	gtk_window_set_title(GTK_WINDOW(gtkwin->window),
						 gtk_label_get_text(GTK_LABEL(gtkconv->tab_label)));
}

/**************************************************************************
 * Utility functions
 **************************************************************************/
static void
do_bold(GtkWidget *bold, GaimGtkConversation *gtkconv)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(bold)))
		gaim_gtk_surround(gtkconv, "<B>", "</B>");
	else
		gaim_gtk_advance_past(gtkconv, "<B>", "</B>");

	gtk_widget_grab_focus(gtkconv->entry);
}

static void
do_italic(GtkWidget *italic, GaimGtkConversation *gtkconv)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(italic)))
		gaim_gtk_surround(gtkconv, "<I>", "</I>");
	else
		gaim_gtk_advance_past(gtkconv, "<I>", "</I>");

	gtk_widget_grab_focus(gtkconv->entry);
}

static void
do_underline(GtkWidget *underline, GaimGtkConversation *gtkconv)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(underline)))
		gaim_gtk_surround(gtkconv, "<U>", "</U>");
	else
		gaim_gtk_advance_past(gtkconv, "<U>", "</U>");

	gtk_widget_grab_focus(gtkconv->entry);
}

static void
do_small(GtkWidget *small, GaimGtkConversation *gtkconv)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(small)))
		gaim_gtk_surround(gtkconv, "<FONT SIZE=\"1\">", "</FONT>");
	else
		gaim_gtk_advance_past(gtkconv, "<FONT SIZE=\"1\">", "</FONT>");

	gtk_widget_grab_focus(gtkconv->entry);
}

static void
do_normal(GtkWidget *normal, GaimGtkConversation *gtkconv)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(normal)))
		gaim_gtk_surround(gtkconv, "<FONT SIZE=\"3\">", "</FONT>");
	else
		gaim_gtk_advance_past(gtkconv, "<FONT SIZE=\"3\">", "</FONT>");

	gtk_widget_grab_focus(gtkconv->entry);
}

static void
do_big(GtkWidget *large, GaimGtkConversation *gtkconv)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(large)))
		gaim_gtk_surround(gtkconv, "<FONT SIZE=\"5\">", "</FONT>");
	else
		gaim_gtk_advance_past(gtkconv, "<FONT SIZE=\"5\">", "</FONT>");

	gtk_widget_grab_focus(gtkconv->entry);
}

static void
toggle_font(GtkWidget *font, GaimConversation *conv)
{
	GaimGtkConversation *gtkconv;

	gtkconv = GAIM_GTK_CONVERSATION(conv);

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(font)))
		show_font_dialog(conv, font);
	else if (gtkconv->dialogs.font != NULL)
		cancel_font(font, conv);
	else
		gaim_gtk_advance_past(gtkconv, "<FONT FACE>", "</FONT>");
}

static void
toggle_fg_color(GtkWidget *color, GaimConversation *conv)
{
	GaimGtkConversation *gtkconv;

	gtkconv = GAIM_GTK_CONVERSATION(conv);

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(color)))
		show_fgcolor_dialog(conv, color);
	else if (gtkconv->dialogs.fg_color != NULL)
		cancel_fgcolor(color, conv);
	else
		gaim_gtk_advance_past(gtkconv, "<FONT COLOR>", "</FONT>");
}

static void
toggle_bg_color(GtkWidget *color, GaimConversation *conv)
{
	GaimGtkConversation *gtkconv;

	gtkconv = GAIM_GTK_CONVERSATION(conv);

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(color)))
		show_bgcolor_dialog(conv, color);
	else if (gtkconv->dialogs.bg_color != NULL)
		cancel_bgcolor(color, conv);
	else
		gaim_gtk_advance_past(gtkconv, "<BODY BGCOLOR>", "</BODY>");
}

static void
check_everything(GtkTextBuffer *buffer)
{
	GaimConversation *conv;
	GaimGtkConversation *gtkconv;

	conv = (GaimConversation *)g_object_get_data(G_OBJECT(buffer),
														 "user_data");

	g_return_if_fail(conv != NULL);

	gtkconv = GAIM_GTK_CONVERSATION(conv);

	/* CONV TODO */
}

static void
set_toggle(GtkWidget *tb, gboolean active)
{
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tb), active);
}

static void
got_typing_keypress(GaimConversation *conv, gboolean first)
{
	GaimIm *im;

	/*
	 * We know we got something, so we at least have to make sure we don't
	 * send TYPED any time soon.
	 */

	im = GAIM_IM(conv);

	if (gaim_im_get_type_again_timeout(im))
		gaim_im_stop_type_again_timeout(im);

	gaim_im_start_type_again_timeout(im);

	if (first || (gaim_im_get_type_again(im) != 0 &&
				  time(NULL) > gaim_im_get_type_again(im))) {

		int timeout = serv_send_typing(gaim_conversation_get_gc(conv),
									   (char *)gaim_conversation_get_name(conv),
									   TYPING);

		if (timeout)
			gaim_im_set_type_again(im, time(NULL) + timeout);
		else
			gaim_im_set_type_again(im, 0);
	}
}

static void
update_typing_icon(GaimConversation *conv)
{
	GaimGtkWindow *gtkwin;
	GaimIm *im = NULL;
	GaimGtkConversation *gtkconv = GAIM_GTK_CONVERSATION(conv);

	gtkwin = GAIM_GTK_WINDOW(gaim_conversation_get_window(conv));

	if(gaim_conversation_get_type(conv) == GAIM_CONV_IM)
		im = GAIM_IM(conv);

	if(gtkwin->menu.typing_icon) {
		gtk_widget_destroy(gtkwin->menu.typing_icon);
		gtkwin->menu.typing_icon = NULL;
	}
	if(im && gaim_im_get_typing_state(im) == TYPING) {
		gtkwin->menu.typing_icon = gtk_image_menu_item_new();
		gtk_image_menu_item_set_image(
				GTK_IMAGE_MENU_ITEM(gtkwin->menu.typing_icon),
				gtk_image_new_from_stock(GAIM_STOCK_TYPING,
					GTK_ICON_SIZE_MENU));
		gtk_tooltips_set_tip(gtkconv->tooltips, gtkwin->menu.typing_icon,
				_("User is typing..."), NULL);
	} else if(im && gaim_im_get_typing_state(im) == TYPED) {
		gtkwin->menu.typing_icon = gtk_image_menu_item_new();
		gtk_image_menu_item_set_image(
				GTK_IMAGE_MENU_ITEM(gtkwin->menu.typing_icon),
				gtk_image_new_from_stock(GAIM_STOCK_TYPED,
					GTK_ICON_SIZE_MENU));
		gtk_tooltips_set_tip(gtkconv->tooltips, gtkwin->menu.typing_icon,
				_("User has typed something and paused"), NULL);
	}

	if(gtkwin->menu.typing_icon) {
		gtk_menu_item_set_right_justified(
				GTK_MENU_ITEM(gtkwin->menu.typing_icon), TRUE);
		gtk_widget_show_all(gtkwin->menu.typing_icon);
		gtk_menu_shell_append(GTK_MENU_SHELL(gtkwin->menu.menubar),
				gtkwin->menu.typing_icon);
	}
}

static gboolean
update_send_as_selection(GaimWindow *win)
{
	GaimAccount *account;
	GaimConversation *conv;
	GaimGtkWindow *gtkwin;
	GtkWidget *menu;
	GList *child;

	g_return_val_if_fail(g_list_find(gaim_get_windows(), win) != NULL, FALSE);

	conv = gaim_window_get_active_conversation(win);

	g_return_val_if_fail(conv != NULL, FALSE);

	account = gaim_conversation_get_account(conv);
	gtkwin  = GAIM_GTK_WINDOW(win);

	g_return_val_if_fail(account != NULL, FALSE);

	if (gtkwin->menu.send_as == NULL)
		return FALSE;

	gtk_widget_show(gtkwin->menu.send_as);

	menu = gtk_menu_item_get_submenu(
		GTK_MENU_ITEM(gtkwin->menu.send_as));

	for (child = gtk_container_get_children(GTK_CONTAINER(menu));
		 child != NULL;
		 child = child->next) {

		GtkWidget *item = child->data;
		GaimAccount *item_account = g_object_get_data(G_OBJECT(item),
				"gaim_account");

		if (account == item_account) {
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
			break;
		}
	}
	return FALSE;
}

static void
generate_send_as_items(GaimWindow *win, GaimConversation *deleted_conv)
{
	GaimGtkWindow *gtkwin;
	GtkWidget *menu;
	GtkWidget *menuitem;
	GList *gcs;
	GList *convs;
	GSList *group = NULL;
	gboolean first_offline = TRUE;
	gboolean found_online = FALSE;
	GtkSizeGroup *sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	gtkwin = GAIM_GTK_WINDOW(win);

	if (gtkwin->menu.send_as != NULL)
		gtk_widget_destroy(gtkwin->menu.send_as);

	/* See if we have > 1 connection active. */
	if (g_list_length(gaim_connections_get_all()) < 2) {
		/* Now make sure we don't have any Offline entries. */
		gboolean found_offline = FALSE;

		for (convs = gaim_get_conversations();
			 convs != NULL;
			 convs = convs->next) {

			GaimConversation *conv;
			GaimAccount *account;

			conv = (GaimConversation *)convs->data;
			account = gaim_conversation_get_account(conv);

			if (account->gc == NULL) {
				found_offline = TRUE;
				break;
			}
		}

		if (!found_offline) {
			gtkwin->menu.send_as = NULL;
			return;
		}
	}

	/* Build the Send As menu */
	gtkwin->menu.send_as = gtk_menu_item_new_with_mnemonic(_("_Send As"));
	gtk_widget_show(gtkwin->menu.send_as);

	menu = gtk_menu_new();

	gtk_menu_shell_append(GTK_MENU_SHELL(gtkwin->menu.menubar),
						  gtkwin->menu.send_as);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(gtkwin->menu.send_as), menu);

	gtk_widget_show(menu);

	/* Fill it with entries. */
	for (gcs = gaim_connections_get_all(); gcs != NULL; gcs = gcs->next) {

		GaimConnection *gc;
		GaimAccount *account;
		GtkWidget *box;
		GtkWidget *label;
		GtkWidget *image;
		GdkPixbuf *pixbuf, *scale;

		found_online = TRUE;

		gc = (GaimConnection *)gcs->data;

		/* Create a pixmap for the protocol icon. */
		pixbuf = create_prpl_icon(gc->account);
		scale = gdk_pixbuf_scale_simple(pixbuf, 16, 16, GDK_INTERP_BILINEAR);

		/* Now convert it to GtkImage */
		if (pixbuf == NULL)
			image = gtk_image_new();
		else
			image = gtk_image_new_from_pixbuf(scale);

		gtk_size_group_add_widget(sg, image);

		g_object_unref(G_OBJECT(scale));
		g_object_unref(G_OBJECT(pixbuf));

		account = gaim_connection_get_account(gc);

		/* Make our menu item */
		menuitem = gtk_radio_menu_item_new_with_label(group,
				gaim_account_get_username(account));
		group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menuitem));

		/* Do some evil, see some evil, speak some evil. */
		box = gtk_hbox_new(FALSE, 0);

		label = gtk_bin_get_child(GTK_BIN(menuitem));
		g_object_ref(label);
		gtk_container_remove(GTK_CONTAINER(menuitem), label);

		gtk_box_pack_start(GTK_BOX(box), image, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 4);

		g_object_unref(label);

		gtk_container_add(GTK_CONTAINER(menuitem), box);

		gtk_widget_show(label);
		gtk_widget_show(image);
		gtk_widget_show(box);

		/* Set our data and callbacks. */
		g_object_set_data(G_OBJECT(menuitem), "user_data", win);
		g_object_set_data(G_OBJECT(menuitem), "gaim_account", gc->account);

		g_signal_connect(G_OBJECT(menuitem), "activate",
						 G_CALLBACK(menu_conv_sel_send_cb), NULL);

		gtk_widget_show(menuitem);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	}

	/*
	 * Fill it with any accounts that still has an open (yet disabled) window
	 * (signed off accounts with a window open).
	 */
	for (convs = gaim_get_conversations();
		 convs != NULL;
		 convs = convs->next) {

		GaimConversation *conv;
		GaimAccount *account;
		GtkWidget *box;
		GtkWidget *label;
		GtkWidget *image;
		GdkPixbuf *pixbuf, *scale;

		conv = (GaimConversation *)convs->data;

		if (conv == deleted_conv)
			continue;

		account = gaim_conversation_get_account(conv);


		if (account && (account->gc == NULL)) {
			if (first_offline && found_online) {
				menuitem = gtk_separator_menu_item_new();
				gtk_widget_show(menuitem);
				gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

				first_offline = FALSE;
			}

			/* Create a pixmap for the protocol icon. */
			pixbuf = create_prpl_icon(account);
			scale = gdk_pixbuf_scale_simple(pixbuf, 16, 16,
											GDK_INTERP_BILINEAR);

			/* Now convert it to GtkImage */
			if (pixbuf == NULL)
				image = gtk_image_new();
			else
				image = gtk_image_new_from_pixbuf(scale);

			gtk_size_group_add_widget(sg, image);

			if (scale  != NULL) g_object_unref(scale);
			if (pixbuf != NULL) g_object_unref(pixbuf);

			/* Make our menu item */
			menuitem = gtk_radio_menu_item_new_with_label(group,
														  account->username);
			group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menuitem));

			/* Do some evil, see some evil, speak some evil. */
			box = gtk_hbox_new(FALSE, 0);

			label = gtk_bin_get_child(GTK_BIN(menuitem));
			g_object_ref(label);
			gtk_container_remove(GTK_CONTAINER(menuitem), label);

			gtk_box_pack_start(GTK_BOX(box), image, FALSE, FALSE, 0);
			gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 4);

			g_object_unref(label);

			gtk_container_add(GTK_CONTAINER(menuitem), box);

			gtk_widget_show(label);
			gtk_widget_show(image);
			gtk_widget_show(box);

			gtk_widget_set_sensitive(menuitem, FALSE);
			g_object_set_data(G_OBJECT(menuitem), "user_data", win);
			g_object_set_data(G_OBJECT(menuitem), "gaim_account", account);

			g_signal_connect(G_OBJECT(menuitem), "activate",
					G_CALLBACK(menu_conv_sel_send_cb), NULL);

			gtk_widget_show(menuitem);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		}
	}

	g_object_unref(sg);

	gtk_widget_show(gtkwin->menu.send_as);
	update_send_as_selection(win);
}

static GList *
generate_invite_user_names(GaimConnection *gc)
{
	GaimBlistNode *gnode,*bnode;
	struct group *g;
	struct buddy *buddy;
	static GList *tmp = NULL;

	if (tmp)
		g_list_free(tmp);

	tmp = g_list_append(NULL, "");

	if (gc != NULL) {
		for(gnode = gaim_get_blist()->root; gnode; gnode = gnode->next) {
			if(!GAIM_BLIST_NODE_IS_GROUP(gnode))
				continue;
			g = (struct group *)gnode;
			for(bnode = gnode->child; bnode; bnode = bnode->next) {
				if(!GAIM_BLIST_NODE_IS_BUDDY(bnode))
					continue;
				buddy = (struct buddy *)bnode;

				if (buddy->account == gc->account && GAIM_BUDDY_IS_ONLINE(buddy))
					tmp = g_list_append(tmp, buddy->name);
			}
		}
	}

	return tmp;
}

static void
add_chat_buddy_common(GaimConversation *conv, const char *name, int pos)
{
	GaimGtkConversation *gtkconv;
	GaimGtkChatPane *gtkchat;
	GaimChat *chat;
	GtkTreeIter iter;
	GtkListStore *ls;

	chat    = GAIM_CHAT(conv);
	gtkconv = GAIM_GTK_CONVERSATION(conv);
	gtkchat = gtkconv->u.chat;

	ls = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list)));

	gtk_list_store_append(ls, &iter);
	gtk_list_store_set(ls, &iter, 0,
					   (gaim_chat_is_user_ignored(chat, name) ? "X" : " "),
					   1, name, -1);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(ls), 1,
										 GTK_SORT_ASCENDING);
}

static void
tab_complete(GaimConversation *conv)
{
	GaimGtkConversation *gtkconv;
	GaimChat *chat;
	GtkTextIter cursor, word_start, start_buffer;
	int start;
	int most_matched = -1;
	char *entered, *partial = NULL;
	char *text;
	GList *matches = NULL;
	GList *nicks = NULL;

	gtkconv = GAIM_GTK_CONVERSATION(conv);
	chat    = GAIM_CHAT(conv);

	gtk_text_buffer_get_start_iter(gtkconv->entry_buffer, &start_buffer);
	gtk_text_buffer_get_iter_at_mark(gtkconv->entry_buffer, &cursor,
			gtk_text_buffer_get_insert(gtkconv->entry_buffer));

	word_start = cursor;

	/* if there's nothing there just return */
	if (!gtk_text_iter_compare(&cursor, &start_buffer))
		return;
	
	text = gtk_text_buffer_get_text(gtkconv->entry_buffer, &start_buffer,
									&cursor, FALSE);

	/* if we're at the end of ": " we need to move back 2 spaces */
	start = strlen(text) - 1;

	if (strlen(text) >= 2 && !strncmp(&text[start-1], ": ", 2))
		gtk_text_iter_backward_chars(&word_start, 2);

	/* find the start of the word that we're tabbing */
	while (start >= 0 && text[start] != ' ') {
		gtk_text_iter_backward_char(&word_start);
		start--;
	}

	g_free(text);

	entered = gtk_text_buffer_get_text(gtkconv->entry_buffer, &word_start,
									   &cursor, FALSE);

	if (gaim_prefs_get_bool("/gaim/gtk/conversations/chat/old_tab_complete")) {
		if (strlen(entered) >= 2 &&
			!strncmp(": ", entered + strlen(entered) - 2, 2)) {

			entered[strlen(entered) - 2] = 0;
		}
	}

	if (!strlen(entered)) {
		g_free(entered);
		return;
	}

	for (nicks = gaim_chat_get_users(chat);
		 nicks != NULL;
		 nicks = nicks->next) {

		char *nick = nicks->data;
		/* this checks to see if the current nick could be a completion */
		if (g_ascii_strncasecmp(nick, entered, strlen(entered))) {
			if (*nick != '+' && *nick != '@' && *nick != '%')
				continue;

			if (g_ascii_strncasecmp(nick + 1, entered, strlen(entered))) {
				if (nick[0] != '@' || nick[1] != '+')
					continue;

				if (g_ascii_strncasecmp(nick + 2, entered, strlen(entered)))
					continue;
				else
					nick += 2;
			}
			else
				nick++;
		}

		/* if we're here, it's a possible completion */

		/* if we're doing old-style, just fill in the completion */
		if (gaim_prefs_get_bool("/gaim/gtk/conversations/chat/old_tab_complete")) {
			gtk_text_buffer_delete(gtkconv->entry_buffer,
								   &word_start, &cursor);

			if (strlen(nick) == strlen(entered)) {
				nicks = (nicks->next
						 ? nicks->next
						 : gaim_chat_get_users(chat));

				nick = nicks->data;

				if (*nick == '@') nick++;
				if (*nick == '%') nick++;
				if (*nick == '+') nick++;
			}

			gtk_text_buffer_get_start_iter(gtkconv->entry_buffer,
										   &start_buffer);
			gtk_text_buffer_get_iter_at_mark(gtkconv->entry_buffer, &cursor,
					gtk_text_buffer_get_insert(gtkconv->entry_buffer));

			if (!gtk_text_iter_compare(&cursor, &start_buffer)) {
				char *tmp = g_strdup_printf("%s: ", nick);
				gtk_text_buffer_insert_at_cursor(gtkconv->entry_buffer,
												 tmp, -1);
				g_free(tmp);
			}
			else
				gtk_text_buffer_insert_at_cursor(gtkconv->entry_buffer,
												 nick, -1);

			g_free(entered);

			return;
		}

		/* we're only here if we're doing new style */
		if (most_matched == -1) {
			/*
			 * this will only get called once, since from now
			 * on most_matched is >= 0
			 */
			most_matched = strlen(nick);
			partial = g_strdup(nick);
		}
		else if (most_matched) {
			while (g_ascii_strncasecmp(nick, partial, most_matched))
				most_matched--;

			partial[most_matched] = 0;
		}

		matches = g_list_append(matches, nick);
	}

	/* we're only here if we're doing new style */

	/* if there weren't any matches, return */
	if (!matches) {
		/* if matches isn't set partials won't be either */
		g_free(entered);
		return;
	}

	gtk_text_buffer_delete(gtkconv->entry_buffer, &word_start, &cursor);

	if (!matches->next) {
		/* there was only one match. fill it in. */
		gtk_text_buffer_get_start_iter(gtkconv->entry_buffer, &start_buffer);
		gtk_text_buffer_get_iter_at_mark(gtkconv->entry_buffer, &cursor,
				gtk_text_buffer_get_insert(gtkconv->entry_buffer));

		if (!gtk_text_iter_compare(&cursor, &start_buffer)) {
			char *tmp = g_strdup_printf("%s: ", (char *)matches->data);
			gtk_text_buffer_insert_at_cursor(gtkconv->entry_buffer, tmp, -1);
			g_free(tmp);
		}
		else
			gtk_text_buffer_insert_at_cursor(gtkconv->entry_buffer,
											 matches->data, -1);

		matches = g_list_remove(matches, matches->data);
	}
	else {
		/*
		 * there were lots of matches, fill in as much as possible
		 * and display all of them
		 */
		char *addthis = g_malloc0(1);

		while (matches) {
			char *tmp = addthis;
			addthis = g_strconcat(tmp, matches->data, " ", NULL);
			g_free(tmp);
			matches = g_list_remove(matches, matches->data);
		}

		gaim_conversation_write(conv, NULL, addthis, -1, WFLAG_NOLOG,
								time(NULL));
		gtk_text_buffer_insert_at_cursor(gtkconv->entry_buffer, partial, -1);
		g_free(addthis);
	}

	g_free(entered);
	g_free(partial);
}

static gboolean
meify(char *message, size_t len)
{
	/*
	 * Read /me-ify: If the message (post-HTML) starts with /me,
	 * remove the "/me " part of it (including that space) and return TRUE.
	 */
	char *c;
	gboolean inside_html = 0;

	/* Umm.. this would be very bad if this happens. */
	g_return_val_if_fail(message != NULL, FALSE);

	if (len == -1)
		len = strlen(message);

	for (c = message; *c != '\0'; c++, len--) {
		if (inside_html) {
			if (*c == '>')
				inside_html = FALSE;
		}
		else {
			if (*c == '<')
				inside_html = TRUE;
			else
				break;
		}
	}

	if (*c != '\0' && !g_ascii_strncasecmp(c, "/me ", 4)) {
		memmove(c, c + 4, len - 3);

		return TRUE;
	}

	return FALSE;
}

static GtkItemFactoryEntry menu_items[] =
{
	/* Conversation menu */
	{ N_("/_Conversation"), NULL, NULL, 0, "<Branch>" },
	{ N_("/Conversation/_Save As..."), NULL, menu_save_as_cb, 0,
	  "<StockItem>", GTK_STOCK_SAVE_AS },
	{ N_("/Conversation/View _Log..."), NULL, menu_view_log_cb, 0, NULL },
	{ "/Conversation/sep1", NULL, NULL, 0, "<Separator>" },
	{ N_("/Conversation/Insert _URL..."), NULL, menu_insert_link_cb, 0,
	  "<StockItem>", GAIM_STOCK_LINK },
	{ N_("/Conversation/Insert _Image..."), NULL, menu_insert_image_cb, 0,
	  "<StockItem>", GAIM_STOCK_IMAGE },
	{ "/Conversation/sep2", NULL, NULL, 0, "<Separator>" },
	{ N_("/Conversation/_Close"), NULL, menu_close_conv_cb, 0,
	  "<StockItem>", GTK_STOCK_CLOSE },

	/* Options */
	{ N_("/_Options"), NULL, NULL, 0, "<Branch>" },
	{ N_("/Options/Enable _Logging"), NULL, menu_logging_cb, 0, "<CheckItem>" },
	{ N_("/Options/Enable _Sounds"), NULL, menu_sounds_cb, 0, "<CheckItem>" },
};

static const int menu_item_count = 
	sizeof(menu_items) / sizeof(*menu_items);

static char *
item_factory_translate_func (const char *path, gpointer func_data)
{
	return _(path);
}

static GtkWidget *
setup_menubar(GaimWindow *win)
{
	GaimGtkWindow *gtkwin;
	GtkAccelGroup *accel_group;
	gtkwin = GAIM_GTK_WINDOW(win);

        accel_group = gtk_accel_group_new ();
        gtk_window_add_accel_group (GTK_WINDOW (gtkwin->window), accel_group);
        g_object_unref (accel_group);

	gtkwin->menu.item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR,
			"<main>", accel_group);

	gtk_item_factory_set_translate_func (gtkwin->menu.item_factory,
					     item_factory_translate_func,
					     NULL, NULL);

	gtk_item_factory_create_items(gtkwin->menu.item_factory, menu_item_count,
								  menu_items, win);

	gtkwin->menu.menubar = gtk_item_factory_get_widget(gtkwin->menu.item_factory,
			"<main>");
	gtkwin->menu.view_log = gtk_item_factory_get_widget(gtkwin->menu.item_factory,
			N_("/Conversation/View Log..."));
	gtkwin->menu.insert_link = gtk_item_factory_get_widget(gtkwin->menu.item_factory,
			N_("/Conversation/Insert URL..."));
	gtkwin->menu.insert_image = gtk_item_factory_get_widget(gtkwin->menu.item_factory,
			N_("/Conversation/Insert Image..."));
	gtkwin->menu.logging = gtk_item_factory_get_widget(gtkwin->menu.item_factory,
			N_("/Options/Enable Logging"));
	gtkwin->menu.sounds = gtk_item_factory_get_widget(gtkwin->menu.item_factory,
			N_("/Options/Enable Sounds"));

	generate_send_as_items(win, NULL);

	gtk_widget_show(gtkwin->menu.menubar);

	return gtkwin->menu.menubar;
}

static void
setup_im_buttons(GaimConversation *conv, GtkWidget *parent)
{
	GaimConnection *gc;
	GaimGtkConversation *gtkconv;
	GaimGtkImPane *gtkim;
	GaimConversationType type = GAIM_CONV_IM;

	gtkconv = GAIM_GTK_CONVERSATION(conv);
	gtkim   = gtkconv->u.im;
	gc      = gaim_conversation_get_gc(conv);

	/* From right to left... */

	/* Send button */
	gtkconv->send = gaim_gtk_change_text(_("Send"), gtkconv->send,
										 GAIM_STOCK_SEND, type);
	gtk_tooltips_set_tip(gtkconv->tooltips, gtkconv->send, _("Send"), NULL);

	gtk_box_pack_end(GTK_BOX(parent), gtkconv->send, FALSE, FALSE, 0);

	/* Separator */
	if (gtkim->sep2 != NULL)
		gtk_widget_destroy(gtkim->sep2);

	gtkim->sep2 = gtk_vseparator_new();
	gtk_box_pack_end(GTK_BOX(parent), gtkim->sep2, FALSE, TRUE, 0);
	gtk_widget_show(gtkim->sep2);

	/* Now, um, just kind of all over the place. Huh? */

	/* Add button */
	if (gaim_find_buddy(gaim_conversation_get_account(conv),
						gaim_conversation_get_name(conv)) == NULL) {

		gtkim->add = gaim_gtk_change_text(_("Add"), gtkim->add,
										  GTK_STOCK_ADD, type);
		gtk_tooltips_set_tip(gtkconv->tooltips, gtkim->add,
			_("Add the user to your buddy list"), NULL);
	}
	else {
		gtkim->add = gaim_gtk_change_text(_("Remove"), gtkim->add,
										  GTK_STOCK_REMOVE, type);
		gtk_tooltips_set_tip(gtkconv->tooltips, gtkim->add,
			_("Remove the user from your buddy list"), NULL);
	}

	gtk_box_pack_start(GTK_BOX(parent), gtkim->add,
					   FALSE, FALSE, 0);

	/* Warn button */
	gtkim->warn = gaim_gtk_change_text(_("Warn"), gtkim->warn,
									   GAIM_STOCK_WARN, type);
	gtk_box_pack_start(GTK_BOX(parent), gtkim->warn, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(gtkconv->tooltips, gtkim->warn,
						 _("Warn the user"), NULL);

	/* Info button */
	gtkconv->info = gaim_gtk_change_text(_("Info"), gtkconv->info,
										 GAIM_STOCK_INFO, type);
	gtk_box_pack_start(GTK_BOX(parent), gtkconv->info, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(gtkconv->tooltips, gtkconv->info,
						 _("Get the user's information"), NULL);

	/* Block button */
	gtkim->block = gaim_gtk_change_text(_("Block"), gtkim->block,
										GAIM_STOCK_BLOCK, type);
	gtk_box_pack_start(GTK_BOX(parent), gtkim->block, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(gtkconv->tooltips, gtkim->block,
						 _("Block the user"), NULL);

	gtk_button_set_relief(GTK_BUTTON(gtkconv->info), GTK_RELIEF_NONE);
	gtk_button_set_relief(GTK_BUTTON(gtkim->add),    GTK_RELIEF_NONE);
	gtk_button_set_relief(GTK_BUTTON(gtkim->warn),   GTK_RELIEF_NONE);
	gtk_button_set_relief(GTK_BUTTON(gtkconv->send), GTK_RELIEF_NONE);
	gtk_button_set_relief(GTK_BUTTON(gtkim->block),  GTK_RELIEF_NONE);

	gtk_size_group_add_widget(gtkconv->sg, gtkconv->info);
	gtk_size_group_add_widget(gtkconv->sg, gtkim->add);
	gtk_size_group_add_widget(gtkconv->sg, gtkim->warn);
	gtk_size_group_add_widget(gtkconv->sg, gtkconv->send);
	gtk_size_group_add_widget(gtkconv->sg, gtkim->block);

	gtk_box_reorder_child(GTK_BOX(parent), gtkim->warn,   1);
	gtk_box_reorder_child(GTK_BOX(parent), gtkim->block,  2);
	gtk_box_reorder_child(GTK_BOX(parent), gtkim->add,    3);
	gtk_box_reorder_child(GTK_BOX(parent), gtkconv->info, 4);

	gaim_gtkconv_update_buttons_by_protocol(conv);

	g_signal_connect(G_OBJECT(gtkconv->send), "clicked",
					 G_CALLBACK(send_cb), conv);
	g_signal_connect(G_OBJECT(gtkconv->info), "clicked",
					 G_CALLBACK(info_cb), conv);
	g_signal_connect(G_OBJECT(gtkim->warn), "clicked",
					 G_CALLBACK(warn_cb), conv);
	g_signal_connect(G_OBJECT(gtkim->block), "clicked",
					 G_CALLBACK(block_cb), conv);
}

static void
setup_chat_buttons(GaimConversation *conv, GtkWidget *parent)
{
	GaimConnection *gc;
	GaimGtkConversation *gtkconv;
	GaimGtkChatPane *gtkchat;
	GaimGtkWindow *gtkwin;
	GtkWidget *sep;

	gtkconv = GAIM_GTK_CONVERSATION(conv);
	gtkchat = gtkconv->u.chat;
	gtkwin  = GAIM_GTK_WINDOW(gaim_conversation_get_window(conv));
	gc      = gaim_conversation_get_gc(conv);

	/* Send button */
	gtkconv->send = gaim_gtk_change_text(_("Send"), gtkconv->send,
										 GAIM_STOCK_SEND, GAIM_CONV_CHAT);
	gtk_tooltips_set_tip(gtkconv->tooltips, gtkconv->send, _("Send"), NULL);

	gtk_box_pack_end(GTK_BOX(parent), gtkconv->send, FALSE, FALSE, 0);

	/* Separator */
	sep = gtk_vseparator_new();
	gtk_box_pack_end(GTK_BOX(parent), sep, FALSE, TRUE, 0);
	gtk_widget_show(sep);

	/* Invite */
	gtkchat->invite = gaim_gtk_change_text(_("Invite"), gtkchat->invite,
										   GAIM_STOCK_INVITE, GAIM_CONV_CHAT);
	gtk_tooltips_set_tip(gtkconv->tooltips, gtkchat->invite,
						 _("Invite a user"), NULL);
	gtk_box_pack_end(GTK_BOX(parent), gtkchat->invite, FALSE, FALSE, 0);

	/* Set the relief on these. */
	gtk_button_set_relief(GTK_BUTTON(gtkchat->invite), GTK_RELIEF_NONE);
	gtk_button_set_relief(GTK_BUTTON(gtkconv->send),   GTK_RELIEF_NONE);

	/* Callbacks */
	g_signal_connect(G_OBJECT(gtkconv->send), "clicked",
					 G_CALLBACK(send_cb), conv);
	g_signal_connect(G_OBJECT(gtkchat->invite), "clicked",
					 G_CALLBACK(invite_cb), conv);
}

static GtkWidget *
build_conv_toolbar(GaimConversation *conv)
{
	GaimGtkConversation *gtkconv;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *button;
	GtkWidget *sep;
	GtkSizeGroup *sg;

	gtkconv = GAIM_GTK_CONVERSATION(conv);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_BOTH);

	vbox = gtk_vbox_new(FALSE, 0);
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	/* Bold */
	button = gaim_pixbuf_toolbar_button_from_stock(GTK_STOCK_BOLD);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(gtkconv->tooltips, button, _("Bold"), NULL);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(do_bold), gtkconv);

	gtkconv->toolbar.bold = button;

	/* Italic */
	button = gaim_pixbuf_toolbar_button_from_stock(GTK_STOCK_ITALIC);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(gtkconv->tooltips, button, _("Italic"), NULL);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(do_italic), gtkconv);

	gtkconv->toolbar.italic = button;

	/* Underline */
	button = gaim_pixbuf_toolbar_button_from_stock(GTK_STOCK_UNDERLINE);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(gtkconv->tooltips, button, _("Underline"), NULL);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(do_underline), gtkconv);

	gtkconv->toolbar.underline = button;

	/* Sep */
	sep = gtk_vseparator_new();
	gtk_box_pack_start(GTK_BOX(hbox), sep, FALSE, FALSE, 0);

	/* Increase font size */
	button = gaim_pixbuf_toolbar_button_from_stock(GAIM_STOCK_TEXT_BIGGER);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(gtkconv->tooltips, button,
						 _("Larger font size"), NULL);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(do_big), gtkconv);

	gtkconv->toolbar.larger_size = button;

	/* Normal font size */
	button = gaim_pixbuf_toolbar_button_from_stock(GAIM_STOCK_TEXT_NORMAL);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(gtkconv->tooltips, button,
						 _("Normal font size"), NULL);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(do_normal), gtkconv);

	gtkconv->toolbar.normal_size = button;

	/* Decrease font size */
	button = gaim_pixbuf_toolbar_button_from_stock(GAIM_STOCK_TEXT_SMALLER);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(gtkconv->tooltips, button,
						 _("Smaller font size"), NULL);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(do_small), gtkconv);

	gtkconv->toolbar.smaller_size = button;

	/* Sep */
	sep = gtk_vseparator_new();
	gtk_box_pack_start(GTK_BOX(hbox), sep, FALSE, FALSE, 0);

	/* Font Face */

	button = gaim_pixbuf_toolbar_button_from_stock(GTK_STOCK_SELECT_FONT);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(gtkconv->tooltips, button,
			_("Font Face"), NULL);

	g_signal_connect(G_OBJECT(button), "clicked",
			G_CALLBACK(toggle_font), conv);

	gtkconv->toolbar.font = button;

	/* Foreground Color */
	button = gaim_pixbuf_toolbar_button_from_stock(GAIM_STOCK_FGCOLOR);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(gtkconv->tooltips, button,
						 _("Foreground font color"), NULL);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(toggle_fg_color), conv);

	gtkconv->toolbar.fgcolor = button;

	/* Background Color */
	button = gaim_pixbuf_toolbar_button_from_stock(GAIM_STOCK_BGCOLOR);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(gtkconv->tooltips, button,
						 _("Background color"), NULL);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(toggle_bg_color), conv);

	gtkconv->toolbar.bgcolor = button;

	/* Sep */
	sep = gtk_vseparator_new();
	gtk_box_pack_start(GTK_BOX(hbox), sep, FALSE, FALSE, 0);

	/* Insert IM Image */
	button = gaim_pixbuf_toolbar_button_from_stock(GAIM_STOCK_IMAGE);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(gtkconv->tooltips, button, _("Insert image"), NULL);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(insert_image_cb), conv);

	gtkconv->toolbar.image = button;

	/* Insert Link */
	button = gaim_pixbuf_toolbar_button_from_stock(GAIM_STOCK_LINK);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(gtkconv->tooltips, button, _("Insert link"), NULL);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(insert_link_cb), conv);

	gtkconv->toolbar.link = button;

	/* Insert Smiley */
	button = gaim_pixbuf_toolbar_button_from_stock(GAIM_STOCK_SMILEY);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(gtkconv->tooltips, button, _("Insert smiley"), NULL);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(insert_smiley_cb), conv);

	gtkconv->toolbar.smiley = button;


	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);

	gtk_widget_show_all(vbox);

	return vbox;
}

static GtkWidget *
setup_chat_pane(GaimConversation *conv)
{
	GaimPluginProtocolInfo *prpl_info = NULL;
	GaimGtkConversation *gtkconv;
	GaimGtkChatPane *gtkchat;
	GaimConnection *gc;
	GtkWidget *vpaned, *hpaned;
	GtkWidget *vbox, *hbox;
	GtkWidget *lbox, *bbox;
	GtkWidget *label;
	GtkWidget *sw2;
	GtkWidget *list;
	GtkWidget *button;
	GtkWidget *frame;
	GtkListStore *ls;
	GtkCellRenderer *rend;
	GtkTreeViewColumn *col;

	gtkconv = GAIM_GTK_CONVERSATION(conv);
	gtkchat = gtkconv->u.chat;
	gc      = gaim_conversation_get_gc(conv);

	/* Setup the outer pane. */
	vpaned = gtk_vpaned_new();
	gtk_widget_show(vpaned);

	/* Setup the top part of the pane. */
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_paned_pack1(GTK_PANED(vpaned), vbox, TRUE, FALSE);
	gtk_widget_show(vbox);

	if (gc != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if (prpl_info->options & OPT_PROTO_CHAT_TOPIC)
	{
		hbox = gtk_hbox_new(FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
		gtk_widget_show(hbox);

		label = gtk_label_new(_("Topic:"));
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
		gtk_widget_show(label);

		gtkchat->topic_text = gtk_entry_new();
		gtk_editable_set_editable(GTK_EDITABLE(gtkchat->topic_text), FALSE);
		gtk_box_pack_start(GTK_BOX(hbox), gtkchat->topic_text, TRUE, TRUE, 5);
		gtk_widget_show(gtkchat->topic_text);
	}

	/* Setup the horizontal pane. */
	hpaned = gtk_hpaned_new();
	gtk_box_pack_start(GTK_BOX(vbox), hpaned, TRUE, TRUE, 5);
	gtk_widget_show(hpaned);

	/* Setup the scrolled window to put gtkimhtml in. */
	gtkconv->sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(gtkconv->sw),
								   GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(gtkconv->sw),
										GTK_SHADOW_IN);
	gtk_paned_pack1(GTK_PANED(hpaned), gtkconv->sw, TRUE, TRUE);

	gtk_widget_set_size_request(gtkconv->sw,
			gaim_prefs_get_int("/gaim/gtk/conversations/chat/default_width"),
			gaim_prefs_get_int("/gaim/gtk/conversations/chat/default_height"));

	gtk_widget_show(gtkconv->sw);

	/* Setup gtkihmtml. */
	gtkconv->imhtml = gtk_imhtml_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(gtkconv->sw), gtkconv->imhtml);

	gtk_imhtml_show_comments(GTK_IMHTML(gtkconv->imhtml),
			gaim_prefs_get_bool("/gaim/gtk/conversations/show_timestamps"));

	g_signal_connect_after(G_OBJECT(gtkconv->imhtml), "button_press_event",
						   G_CALLBACK(entry_stop_rclick_cb), NULL);

	gaim_setup_imhtml(gtkconv->imhtml);

	gtk_widget_show(gtkconv->imhtml);

	/* Build the right pane. */
	lbox = gtk_vbox_new(FALSE, 5);
	gtk_paned_pack2(GTK_PANED(hpaned), lbox, FALSE, TRUE);
	gtk_widget_show(lbox);

	/* Setup the label telling how many people are in the room. */
	gtkchat->count = gtk_label_new(_("0 people in room"));
	gtk_box_pack_start(GTK_BOX(lbox), gtkchat->count, FALSE, FALSE, 0);
	gtk_widget_show(gtkchat->count);

	/* Setup the list of users. */
	sw2 = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw2),
								   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(lbox), sw2, TRUE, TRUE, 0);
	gtk_widget_show(sw2);

	ls = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(ls), 1,
										 GTK_SORT_ASCENDING);

	list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(ls));

	rend = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(NULL, rend,
												   "text", 0, NULL);
	gtk_tree_view_column_set_clickable(GTK_TREE_VIEW_COLUMN(col), TRUE);

	g_signal_connect(G_OBJECT(list), "button_press_event",
					 G_CALLBACK(right_click_chat_cb), conv);

	gtk_tree_view_append_column(GTK_TREE_VIEW(list), col);

	col = gtk_tree_view_column_new_with_attributes(NULL, rend,
												   "text", 1, NULL);
	gtk_tree_view_column_set_clickable(GTK_TREE_VIEW_COLUMN(col), TRUE);

#if 0
	g_signal_connect(G_OBJECT(list), "button_press_event",
					 G_CALLBACK(right_click_chat), conv);
#endif

	gtk_tree_view_append_column(GTK_TREE_VIEW(list), col);

	gtk_widget_set_size_request(list, 150, -1);

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(list), FALSE);
	gtk_widget_show(list);

	gtkchat->list = list;

	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw2), list);

	/* Setup the user list toolbar. */
	bbox = gtk_hbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(lbox), bbox, FALSE, FALSE, 0);
	gtk_widget_show(bbox);

	/* IM */
	button = gaim_pixbuf_button_from_stock(NULL, GTK_STOCK_REDO,
										   GAIM_BUTTON_VERTICAL);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(gtkconv->tooltips, button, _("IM the user"), NULL);
	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(im_cb), conv);

	gtk_widget_show(button);

	/* Ignore */
	button = gaim_pixbuf_button_from_stock(NULL, GAIM_STOCK_IGNORE,
										   GAIM_BUTTON_VERTICAL);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(gtkconv->tooltips, button,
						 _("Ignore the user"), NULL);
	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(ignore_cb), conv);
	gtk_widget_show(button);

	/* Info */
	button = gaim_pixbuf_button_from_stock(NULL, GAIM_STOCK_INFO,
										   GAIM_BUTTON_VERTICAL);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(gtkconv->tooltips, button,
						 _("Get the user's information"), NULL);
	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(info_cb), conv);

	gtk_widget_show(button);

	gtkconv->info = button;

	/* Build the toolbar. */
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_paned_pack2(GTK_PANED(vpaned), vbox, FALSE, FALSE);
	gtk_widget_show(vbox);

	gtkconv->toolbar.toolbar = build_conv_toolbar(conv);
	gtk_box_pack_start(GTK_BOX(vbox), gtkconv->toolbar.toolbar,
					   FALSE, FALSE, 0);

	/* Setup the entry widget. */
	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
	gtk_widget_show(frame);

	gtkconv->entry_buffer = gtk_text_buffer_new(NULL);
	g_object_set_data(G_OBJECT(gtkconv->entry_buffer), "user_data", conv);
	gtkconv->entry = gtk_text_view_new_with_buffer(gtkconv->entry_buffer);

	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(gtkconv->entry), GTK_WRAP_WORD_CHAR);
	gtk_widget_set_size_request(gtkconv->entry, -1,
			MAX(gaim_prefs_get_int("/gaim/gtk/conversations/chat/entry_height"),
				25));

	/* Connect the signal handlers. */
	g_signal_connect_swapped(G_OBJECT(gtkconv->entry), "key_press_event",
							 G_CALLBACK(entry_key_pressed_cb_1),
							 gtkconv->entry_buffer);
	g_signal_connect_after(G_OBJECT(gtkconv->entry), "button_press_event",
						   G_CALLBACK(entry_stop_rclick_cb), NULL);
	g_signal_connect(G_OBJECT(gtkconv->entry), "key_press_event",
					 G_CALLBACK(entry_key_pressed_cb_2), conv);

#ifdef USE_GTKSPELL
	if (gaim_prefs_get_bool("/gaim/gtk/conversations/spellcheck"))
		gtkspell_new_attach(GTK_TEXT_VIEW(gtkconv->entry), NULL, NULL);
#endif

	gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(gtkconv->entry));
	gtk_widget_show(gtkconv->entry);

	/* Setup the bottom button box. */
	gtkconv->bbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), gtkconv->bbox, FALSE, FALSE, 0);
	gtk_widget_show(gtkconv->bbox);

	setup_chat_buttons(conv, gtkconv->bbox);

	return vpaned;
}

static GtkWidget *
setup_im_pane(GaimConversation *conv)
{
	GaimGtkConversation *gtkconv;
	GaimGtkImPane *gtkim;
	GtkWidget *paned;
	GtkWidget *vbox;
	GtkWidget *vbox2;
	GtkWidget *frame;

	gtkconv = GAIM_GTK_CONVERSATION(conv);
	gtkim   = gtkconv->u.im;

	/* Setup the outer pane. */
	paned = gtk_vpaned_new();
	gtk_widget_show(paned);

	/* Setup the top part of the pane. */
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_paned_pack1(GTK_PANED(paned), vbox, TRUE, TRUE);
	gtk_widget_show(vbox);

	/* Setup the gtkimhtml widget. */
	gtkconv->sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(gtkconv->sw),
								   GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(gtkconv->sw),
										GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(vbox), gtkconv->sw, TRUE, TRUE, 0);

	gtk_widget_set_size_request(gtkconv->sw,
			gaim_prefs_get_int("/gaim/gtk/conversations/im/default_width"),
			gaim_prefs_get_int("/gaim/gtk/conversations/im/default_height"));
	gtk_widget_show(gtkconv->sw);

	gtkconv->imhtml = gtk_imhtml_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(gtkconv->sw), gtkconv->imhtml);

	g_signal_connect_after(G_OBJECT(gtkconv->imhtml), "button_press_event",
						   G_CALLBACK(entry_stop_rclick_cb), NULL);

	gtk_imhtml_show_comments(GTK_IMHTML(gtkconv->imhtml),
			gaim_prefs_get_bool("/gaim/gtk/conversations/show_timestamps"));

	gaim_setup_imhtml(gtkconv->imhtml);

	gtk_widget_show(gtkconv->imhtml);

	vbox2 = gtk_vbox_new(FALSE, 5);
	gtk_paned_pack2(GTK_PANED(paned), vbox2, FALSE, FALSE);
	gtk_widget_show(vbox2);

	/* Build the toolbar. */
	gtkconv->toolbar.toolbar = build_conv_toolbar(conv);
	gtk_box_pack_start(GTK_BOX(vbox2), gtkconv->toolbar.toolbar,
					   FALSE, FALSE, 0);

	/* Setup the entry widget. */
	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(vbox2), frame, TRUE, TRUE, 0);
	gtk_widget_show(frame);

	gtkconv->entry_buffer = gtk_text_buffer_new(NULL);
	g_object_set_data(G_OBJECT(gtkconv->entry_buffer), "user_data", conv);
	gtkconv->entry = gtk_text_view_new_with_buffer(gtkconv->entry_buffer);

	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(gtkconv->entry), GTK_WRAP_WORD_CHAR);
	gtk_widget_set_size_request(gtkconv->entry, -1,
			MAX(gaim_prefs_get_int("/gaim/gtk/conversations/im/entry_height"),
				25));

	/* Connect the signal handlers. */
	g_signal_connect_swapped(G_OBJECT(gtkconv->entry), "key_press_event",
							 G_CALLBACK(entry_key_pressed_cb_1),
							 gtkconv->entry_buffer);
	g_signal_connect(G_OBJECT(gtkconv->entry), "key_press_event",
					 G_CALLBACK(entry_key_pressed_cb_2), conv);
	g_signal_connect_after(G_OBJECT(gtkconv->entry), "button_press_event",
						   G_CALLBACK(entry_stop_rclick_cb), NULL);

	g_signal_connect(G_OBJECT(gtkconv->entry_buffer), "insert_text",
					 G_CALLBACK(insert_text_cb), conv);
	g_signal_connect(G_OBJECT(gtkconv->entry_buffer), "delete_range",
					 G_CALLBACK(delete_text_cb), conv);

#ifdef USE_GTKSPELL
	if (gaim_prefs_get_bool("/gaim/gtk/conversations/spellcheck"))
		gtkspell_new_attach(GTK_TEXT_VIEW(gtkconv->entry), NULL, NULL);
#endif

	gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(gtkconv->entry));
	gtk_widget_show(gtkconv->entry);

	gtkconv->bbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox2), gtkconv->bbox, FALSE, FALSE, 0);
	gtk_widget_show(gtkconv->bbox);

	setup_im_buttons(conv, gtkconv->bbox);

	return paned;
}

static void
move_next_tab(GaimConversation *conv)
{
	GaimConversation *next_conv = NULL;
	GaimWindow *win;
	GList *l;
	int index, i;

	win   = gaim_conversation_get_window(conv);
	index = gaim_conversation_get_index(conv);

	/* First check the tabs after this position. */
	for (l = g_list_nth(gaim_window_get_conversations(win), index);
		 l != NULL;
		 l = l->next) {

		next_conv = (GaimConversation *)l->data;

		if (gaim_conversation_get_unseen(next_conv) > 0)
			break;

		next_conv = NULL;
	}

	if (next_conv == NULL) {

		/* Now check before this position. */
		for (l = gaim_window_get_conversations(win), i = 0;
			 l != NULL && i < index;
			 l = l->next) {

			next_conv = (GaimConversation *)l->data;

			if (gaim_conversation_get_unseen(next_conv) > 0)
				break;

			next_conv = NULL;
		}

		if (next_conv == NULL) {
			/* Okay, just grab the next conversation tab. */
			if (index == gaim_window_get_conversation_count(win) - 1)
				next_conv = gaim_window_get_conversation_at(win, 0);
			else
				next_conv = gaim_window_get_conversation_at(win, index + 1);
		}
	}

	if (next_conv != NULL && next_conv != conv) {
		gaim_window_switch_conversation(win,
			gaim_conversation_get_index(next_conv));
	}
}

static void
conv_dnd_recv(GtkWidget *widget, GdkDragContext *dc, guint x, guint y,
			  GtkSelectionData *sd, guint info, guint t,
			  GaimConversation *conv)
{
	GaimWindow *win = conv->window;
	GaimConversation *c;

	if (sd->target == gdk_atom_intern("GAIM_BLIST_NODE", FALSE)) {
		GaimBlistNode *n = NULL;
		memcpy(&n, sd->data, sizeof(n));

		if (!GAIM_BLIST_NODE_IS_BUDDY(n))
			return;

		c = gaim_conversation_new(GAIM_CONV_IM,
								  ((struct buddy *)n)->account,
								  ((struct buddy *)n)->name);

		gaim_window_add_conversation(win, c);
	}
}

/**************************************************************************
 * GTK+ window ops
 **************************************************************************/
static GaimConversationUiOps *
gaim_gtk_get_conversation_ui_ops(void)
{
	return gaim_get_gtk_conversation_ui_ops();
}

static void
gaim_gtk_new_window(GaimWindow *win)
{
	GaimGtkWindow *gtkwin;
	GtkPositionType pos;
	GtkWidget *testidea;
	GtkWidget *menubar;

	gtkwin = g_malloc0(sizeof(GaimGtkWindow));

	win->ui_data = gtkwin;

	/* Create the window. */
	gtkwin->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_role(GTK_WINDOW(gtkwin->window), "conversation");
	gtk_window_set_resizable(GTK_WINDOW(gtkwin->window), TRUE);
	gtk_container_set_border_width(GTK_CONTAINER(gtkwin->window), 0);
	GTK_WINDOW(gtkwin->window)->allow_shrink = TRUE;
	gtk_widget_realize(gtkwin->window);

	g_signal_connect(G_OBJECT(gtkwin->window), "delete_event",
					 G_CALLBACK(close_win_cb), win);

	/* Create the notebook. */
	gtkwin->notebook = gtk_notebook_new();

	pos = gaim_prefs_get_int("/gaim/gtk/conversations/tab_side");

#if 0
	gtk_notebook_set_tab_hborder(GTK_NOTEBOOK(gtkwin->notebook), 0);
	gtk_notebook_set_tab_vborder(GTK_NOTEBOOK(gtkwin->notebook), 0);
#endif
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(gtkwin->notebook), pos);
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(gtkwin->notebook), TRUE);
	gtk_notebook_popup_enable(GTK_NOTEBOOK(gtkwin->notebook));
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(gtkwin->notebook), FALSE);

	gtk_widget_show(gtkwin->notebook);

	g_signal_connect_after(G_OBJECT(gtkwin->notebook), "switch_page",
						   G_CALLBACK(switch_conv_cb), win);

	/* Setup the tab drag and drop signals. */
	gtk_widget_add_events(gtkwin->notebook,
				GDK_BUTTON1_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK);
	g_signal_connect(G_OBJECT(gtkwin->notebook), "button_press_event",
					 G_CALLBACK(notebook_press_cb), win);
	g_signal_connect(G_OBJECT(gtkwin->notebook), "button_release_event",
					 G_CALLBACK(notebook_release_cb), win);

	testidea = gtk_vbox_new(FALSE, 0);

	/* Setup the menubar. */
	menubar = setup_menubar(win);
	gtk_box_pack_start(GTK_BOX(testidea), menubar, FALSE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(testidea), gtkwin->notebook, TRUE, TRUE, 0);

	gtk_container_add(GTK_CONTAINER(gtkwin->window), testidea);

	gtk_widget_show(testidea);
}

static void
gaim_gtk_destroy_window(GaimWindow *win)
{
	GaimGtkWindow *gtkwin = GAIM_GTK_WINDOW(win);

	gtk_widget_destroy(gtkwin->window);

	g_object_unref(G_OBJECT(gtkwin->menu.item_factory));

	g_free(gtkwin);
	win->ui_data = NULL;
}

static void
gaim_gtk_show(GaimWindow *win)
{
	GaimGtkWindow *gtkwin = GAIM_GTK_WINDOW(win);

	gtk_widget_show(gtkwin->window);
}

static void
gaim_gtk_hide(GaimWindow *win)
{
	GaimGtkWindow *gtkwin = GAIM_GTK_WINDOW(win);

	gtk_widget_hide(gtkwin->window);
}

static void
gaim_gtk_raise(GaimWindow *win)
{
	GaimGtkWindow *gtkwin = GAIM_GTK_WINDOW(win);

	gtk_widget_show(gtkwin->window);
	gtk_window_deiconify(GTK_WINDOW(gtkwin->window));
	gdk_window_raise(gtkwin->window->window);
}

static void
gaim_gtk_flash(GaimWindow *win)
{
#ifdef _WIN32
	GaimGtkWindow *gtkwin = GAIM_GTK_WINDOW(win);

	wgaim_im_blink(gtkwin->window);
#endif
}

static void
gaim_gtk_switch_conversation(GaimWindow *win, unsigned int index)
{
	GaimGtkWindow *gtkwin;

	gtkwin = GAIM_GTK_WINDOW(win);

	gtk_notebook_set_current_page(GTK_NOTEBOOK(gtkwin->notebook), index);
}

static const GtkTargetEntry te[] =
{
	{"text/plain", 0, 0},
	{"text/uri-list", 0, 1},
	{"GAIM_BLIST_NODE", 0, 2},
	{"STRING", 0, 3}
};

static void
gaim_gtk_add_conversation(GaimWindow *win, GaimConversation *conv)
{
	GaimGtkWindow *gtkwin;
	GaimGtkConversation *gtkconv, *focus_gtkconv;
	GaimConversation *focus_conv;
	GtkWidget *pane = NULL;
	GtkWidget *tab_cont;
	GtkWidget *tabby;
	gboolean new_ui;
	GaimConversationType conv_type;
	const char *name;

	name      = gaim_conversation_get_name(conv);
	conv_type = gaim_conversation_get_type(conv);
	gtkwin    = GAIM_GTK_WINDOW(win);

	if (conv->ui_data != NULL) {
		gtkconv = (GaimGtkConversation *)conv->ui_data;

		tab_cont = gtkconv->tab_cont;

		new_ui = FALSE;
	}
	else {
		gtkconv = g_malloc0(sizeof(GaimGtkConversation));
		conv->ui_data = gtkconv;

		/* Setup some initial variables. */
		gtkconv->sg       = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
		gtkconv->tooltips = gtk_tooltips_new();

		/* Setup the foreground and background colors */
		gaim_gtkconv_update_font_colors(conv);

		/* Setup the font face */
		gaim_gtkconv_update_font_face(conv);

		if (conv_type == GAIM_CONV_CHAT) {
			gtkconv->u.chat = g_malloc0(sizeof(GaimGtkChatPane));

			pane = setup_chat_pane(conv);
		}
		else if (conv_type == GAIM_CONV_IM) {
			gtkconv->u.im = g_malloc0(sizeof(GaimGtkImPane));
			gtkconv->u.im->a_virgin = TRUE;

			pane = setup_im_pane(conv);
		}

		if (pane == NULL) {
			if      (conv_type == GAIM_CONV_CHAT) g_free(gtkconv->u.chat);
			else if (conv_type == GAIM_CONV_IM)   g_free(gtkconv->u.im);

			g_free(gtkconv);
			conv->ui_data = NULL;

			return;
		}

		/* Setup drag-and-drop */
		gtk_drag_dest_set(pane,
						  GTK_DEST_DEFAULT_MOTION |
						  GTK_DEST_DEFAULT_DROP,
						  te, sizeof(te) / sizeof(GtkTargetEntry),
						  GDK_ACTION_COPY);
		gtk_drag_dest_set(gtkconv->imhtml,
						  GTK_DEST_DEFAULT_MOTION |
				                  GTK_DEST_DEFAULT_DROP,
						  te, sizeof(te) / sizeof(GtkTargetEntry),
						  GDK_ACTION_DEFAULT | GDK_ACTION_COPY | GDK_ACTION_MOVE);
		gtk_drag_dest_set(gtkconv->entry,
						  GTK_DEST_DEFAULT_MOTION |
						  GTK_DEST_DEFAULT_DROP,
						  te, sizeof(te) / sizeof(GtkTargetEntry),
						  GDK_ACTION_COPY);

		g_signal_connect(G_OBJECT(pane), "drag_data_received",
						 G_CALLBACK(conv_dnd_recv), conv);
		g_signal_connect(G_OBJECT(gtkconv->imhtml), "drag_data_received",
						 G_CALLBACK(conv_dnd_recv), conv);
#if 0
		g_signal_connect(G_OBJECT(gtkconv->entry), "drag_data_received",
						 G_CALLBACK(conv_dnd_recv), conv);
#endif

		/*
		 * Write the New Conversation log string.
		 *
		 * This should probably be elsewhere, but then, logging should
		 * be moved out in some way, either via plugin or via a new API.
		 */
		if (gaim_conversation_is_logging(conv) &&
			conv_type != GAIM_CONV_MISC) {

			FILE *fd;
			char filename[256];

			g_snprintf(filename, sizeof(filename), "%s%s", name,
					   (conv_type == GAIM_CONV_CHAT ? ".chat" : ""));

			fd = open_log_file(filename, (conv_type == GAIM_CONV_CHAT));

			if (fd) {
				if (!gaim_prefs_get_bool("/gaim/gtk/logging/strip_html"))
					fprintf(fd,
							"<HR><BR><H3 Align=Center> "
							"---- New Conversation @ %s ----</H3><BR>\n",
							full_date());
				else
					fprintf(fd, "---- New Conversation @ %s ----\n",
							full_date());

				fclose(fd);
			}
		}

		/* Setup the container for the tab. */
		gtkconv->tab_cont = tab_cont = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(tab_cont), 5);
		gtk_container_add(GTK_CONTAINER(tab_cont), pane);
		gtk_widget_show(pane);

		new_ui = TRUE;
		
		gtkconv->make_sound = TRUE;
	}

	g_signal_connect_swapped(G_OBJECT(pane), "focus",
							 G_CALLBACK(gtk_widget_grab_focus), gtkconv->entry);

	gtkconv->tabby = tabby = gtk_hbox_new(FALSE, 5);

	/* Close button. */
	gtkconv->close = gtk_button_new();
	gtk_widget_set_size_request(GTK_WIDGET(gtkconv->close), 16, 16);
	gtk_container_add(GTK_CONTAINER(gtkconv->close),
			  gtk_image_new_from_stock(GTK_STOCK_CLOSE,
			  GTK_ICON_SIZE_MENU));
	gtk_button_set_relief(GTK_BUTTON(gtkconv->close), GTK_RELIEF_NONE);
	gtk_tooltips_set_tip(gtkconv->tooltips, gtkconv->close,
						 _("Close conversation"), NULL);

	g_signal_connect(G_OBJECT(gtkconv->close), "clicked",
					 G_CALLBACK(close_conv_cb), conv);

	/* Tab label. */
	gtkconv->tab_label = gtk_label_new(gaim_conversation_get_title(conv));
#if 0
	gtk_misc_set_alignment(GTK_MISC(gtkconv->tab_label), 0.00, 0.5);
	gtk_misc_set_padding(GTK_MISC(gtkconv->tab_label), 4, 0);
#endif


	/* Pack it all together. */
	gtk_box_pack_start(GTK_BOX(tabby), gtkconv->tab_label, TRUE, TRUE, 0);
	gtk_widget_show(gtkconv->tab_label);
	gtk_box_pack_start(GTK_BOX(tabby), gtkconv->close, FALSE, FALSE, 0);

	if (gaim_prefs_get_bool("/gaim/gtk/conversations/close_on_tabs"))
		gtk_widget_show_all(gtkconv->close);

	gtk_widget_show(tabby);


	/* Add this pane to the conversations notebook. */
	gtk_notebook_append_page(GTK_NOTEBOOK(gtkwin->notebook), tab_cont, tabby);
	gtk_notebook_set_menu_label_text(GTK_NOTEBOOK(gtkwin->notebook), tab_cont,
									 gaim_conversation_get_title(conv));

	gtk_widget_show(tab_cont);

	if (gaim_window_get_conversation_count(win) == 1) {
		/* Er, bug in notebooks? Switch to the page manually. */
		gtk_notebook_set_current_page(GTK_NOTEBOOK(gtkwin->notebook), 0);

		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(gtkwin->notebook),
				gaim_prefs_get_bool("/gaim/gtk/conversations/tabs"));
	}
	else
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(gtkwin->notebook), TRUE);

	focus_conv = g_list_nth_data(gaim_window_get_conversations(win),
			gtk_notebook_get_current_page(GTK_NOTEBOOK(gtkwin->notebook)));
	focus_gtkconv = GAIM_GTK_CONVERSATION(focus_conv);
	gtk_widget_grab_focus(focus_gtkconv->entry);

	gaim_gtkconv_update_buddy_icon(conv);

	if (!new_ui)
		g_object_unref(gtkconv->tab_cont);

	if (gaim_window_get_conversation_count(win) == 1)
		g_timeout_add(0, (GSourceFunc)update_send_as_selection, win);

}

static void
gaim_gtk_remove_conversation(GaimWindow *win, GaimConversation *conv)
{
	GaimGtkWindow *gtkwin;
	GaimGtkConversation *gtkconv;
	unsigned int index;
	GaimConversationType conv_type;

	conv_type = gaim_conversation_get_type(conv);
	index = gaim_conversation_get_index(conv);

	gtkwin  = GAIM_GTK_WINDOW(win);
	gtkconv = GAIM_GTK_CONVERSATION(conv);

	g_object_ref(gtkconv->tab_cont);
	gtk_object_sink(GTK_OBJECT(gtkconv->tab_cont));

	gtk_notebook_remove_page(GTK_NOTEBOOK(gtkwin->notebook), index);

	/* go back to tabless if need be */
	if (gaim_window_get_conversation_count(win) <= 2) {
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(gtkwin->notebook),
				gaim_prefs_get_bool("/gaim/gtk/conversations/tabs"));
	}


	/* If this window is setup with an inactive gc, regenerate the menu. */
	if (conv_type == GAIM_CONV_IM &&
		gaim_conversation_get_gc(conv) == NULL) {

		generate_send_as_items(win, conv);
	}
}

static void
gaim_gtk_move_conversation(GaimWindow *win, GaimConversation *conv,
						   unsigned int new_index)
{
	GaimGtkWindow *gtkwin;
	GaimGtkConversation *gtkconv;

	gtkwin  = GAIM_GTK_WINDOW(win);
	gtkconv = GAIM_GTK_CONVERSATION(conv);

	if (new_index > gaim_conversation_get_index(conv))
		new_index--;

	gtk_notebook_reorder_child(GTK_NOTEBOOK(gtkwin->notebook),
							   gtkconv->tab_cont, new_index);
}

static int
gaim_gtk_get_active_index(const GaimWindow *win)
{
	GaimGtkWindow *gtkwin;
	int index;

	gtkwin = GAIM_GTK_WINDOW(win);

	index = gtk_notebook_get_current_page(GTK_NOTEBOOK(gtkwin->notebook));

	/*
	 * A fix, because the first conversation may be active, but not
	 * appear in the notebook just yet. -- ChipX86
	 */
	return (index == -1 ? 0 : index);
}

static GaimWindowUiOps window_ui_ops =
{
	gaim_gtk_get_conversation_ui_ops,
	gaim_gtk_new_window,
	gaim_gtk_destroy_window,
	gaim_gtk_show,
	gaim_gtk_hide,
	gaim_gtk_raise,
	gaim_gtk_flash,
	gaim_gtk_switch_conversation,
	gaim_gtk_add_conversation,
	gaim_gtk_remove_conversation,
	gaim_gtk_move_conversation,
	gaim_gtk_get_active_index
};

static void
update_convo_add_button(GaimConversation *conv)
{
	GaimPluginProtocolInfo *prpl_info = NULL;
	GaimGtkConversation *gtkconv;
	GaimConnection *gc;
	GaimConversationType type;
	GtkWidget *parent;

	type    = gaim_conversation_get_type(conv);
	gc      = gaim_conversation_get_gc(conv);
	gtkconv = GAIM_GTK_CONVERSATION(conv);
	parent  = gtk_widget_get_parent(gtkconv->u.im->add);

	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if (gaim_find_buddy(gc->account, gaim_conversation_get_name(conv))) {
		gtkconv->u.im->add =
			gaim_gtk_change_text(_("Remove"), gtkconv->u.im->add,
								 GTK_STOCK_REMOVE, type);
		gtk_tooltips_set_tip(gtkconv->tooltips, gtkconv->u.im->add,
			_("Remove the user from your buddy list"), NULL);

		gtk_widget_set_sensitive(gtkconv->u.im->add,
			(gc != NULL && prpl_info->remove_buddy != NULL));
	} else {
		gtkconv->u.im->add =
			gaim_gtk_change_text(_("Add"), gtkconv->u.im->add,
								 GTK_STOCK_ADD, type);
		gtk_tooltips_set_tip(gtkconv->tooltips, gtkconv->u.im->add,
			_("Add the user to your buddy list"), NULL);

		gtk_widget_set_sensitive(gtkconv->u.im->add,
			(gc != NULL && prpl_info->add_buddy != NULL));
	}

	g_signal_connect(G_OBJECT(gtkconv->u.im->add), "clicked",
					 G_CALLBACK(add_cb), conv);

	gtk_box_pack_start(GTK_BOX(parent), gtkconv->u.im->add,
					   FALSE, FALSE, 0);
	gtk_box_reorder_child(GTK_BOX(parent), gtkconv->u.im->add, 3);
	gtk_button_set_relief(GTK_BUTTON(gtkconv->u.im->add), GTK_RELIEF_NONE);
	gtk_size_group_add_widget(gtkconv->sg, gtkconv->u.im->add);
}

GaimWindowUiOps *
gaim_get_gtk_window_ui_ops(void)
{
	return &window_ui_ops;
}

/**************************************************************************
 * Conversation UI operations
 **************************************************************************/
static void
gaim_gtkconv_destroy(GaimConversation *conv)
{
	GaimGtkConversation *gtkconv = GAIM_GTK_CONVERSATION(conv);

	if (gtkconv->dialogs.fg_color != NULL)
		gtk_widget_destroy(gtkconv->dialogs.fg_color);

	if (gtkconv->dialogs.bg_color != NULL)
		gtk_widget_destroy(gtkconv->dialogs.bg_color);

	if (gtkconv->dialogs.font != NULL)
		gtk_widget_destroy(gtkconv->dialogs.font);

	if (gtkconv->dialogs.smiley != NULL)
		gtk_widget_destroy(gtkconv->dialogs.smiley);

	if (gtkconv->dialogs.link != NULL)
		gtk_widget_destroy(gtkconv->dialogs.link);

	if (gtkconv->dialogs.log != NULL)
		gtk_widget_destroy(gtkconv->dialogs.log);

	gtk_widget_destroy(gtkconv->tab_cont);
	g_object_unref(gtkconv->tab_cont);

	if (gaim_conversation_get_type(conv) == GAIM_CONV_IM) {
		if (gtkconv->u.im->icon_timer != 0)
			g_source_remove(gtkconv->u.im->icon_timer);

		if (gtkconv->u.im->save_icon != NULL)
			gtk_widget_destroy(gtkconv->u.im->save_icon);

		if (gtkconv->u.im->anim != NULL)
			g_object_unref(G_OBJECT(gtkconv->u.im->anim));

		g_free(gtkconv->u.im);
	}
	else if (gaim_conversation_get_type(conv) == GAIM_CONV_CHAT) {
		g_free(gtkconv->u.chat);
	}

	gtk_object_sink(GTK_OBJECT(gtkconv->tooltips));

	g_free(gtkconv);
}

static void
gaim_gtkconv_write_im(GaimConversation *conv, const char *who,
					  const char *message, size_t len, int flags,
					  time_t mtime)
{
	GaimGtkConversation *gtkconv;

	gtkconv = GAIM_GTK_CONVERSATION(conv);

	if (!(flags & WFLAG_NOLOG) &&
		gaim_prefs_get_bool("/gaim/gtk/conversations/im/raise_on_events")) {

		gaim_window_raise(gaim_conversation_get_window(conv));
	}

	/* Play a sound, if specified in prefs. */
	if (gtkconv->make_sound) {
		if (flags & WFLAG_RECV) {
			if (gtkconv->u.im->a_virgin &&
				gaim_prefs_get_bool("/gaim/gtk/sound/first_im_recv")) {

				gaim_sound_play_event(GAIM_SOUND_FIRST_RECEIVE);
			}
			else
				gaim_sound_play_event(GAIM_SOUND_RECEIVE);
		}
		else {
			gaim_sound_play_event(GAIM_SOUND_SEND);
		}
	}

	gtkconv->u.im->a_virgin = FALSE;

	gaim_conversation_write(conv, who, message, len, flags, mtime);
}

static void
gaim_gtkconv_write_chat(GaimConversation *conv, const char *who,
						const char *message, int flags, time_t mtime)
{
	GaimGtkConversation *gtkconv;

	gtkconv = GAIM_GTK_CONVERSATION(conv);

	/* Play a sound, if specified in prefs. */
	if (gtkconv->make_sound) {
		if (!(flags & WFLAG_WHISPER) && (flags & WFLAG_SEND))
			gaim_sound_play_event(GAIM_SOUND_CHAT_YOU_SAY);
		else if (flags & WFLAG_RECV) {
			if ((flags & WFLAG_NICK) &&
				gaim_prefs_get_bool("/gaim/gtk/sound/nick_said")) {

				gaim_sound_play_event(GAIM_SOUND_CHAT_NICK);
			}
			else
				gaim_sound_play_event(GAIM_SOUND_CHAT_SAY);
		}
	}

	if (gaim_prefs_get_bool("/gaim/gtk/conversations/chat/color_nicks"))
		flags |= WFLAG_COLORIZE;

	/* Raise the window, if specified in prefs. */
	if (!(flags & WFLAG_NOLOG) &&
		gaim_prefs_get_bool("/gaim/gtk/conversations/chat/raise_on_events")) {

		gaim_window_raise(gaim_conversation_get_window(conv));
	}

	gaim_conversation_write(conv, who, message, -1, flags, mtime);
}

static void
gaim_gtkconv_write_conv(GaimConversation *conv, const char *who,
						const char *message, size_t length, int flags,
						time_t mtime)
{
	GaimGtkConversation *gtkconv;
	GaimWindow *win;
	GaimConnection *gc;
	int gtk_font_options = 0;
	GString *log_str;
	FILE *fd;
	char buf[BUF_LONG];
	char buf2[BUF_LONG];
	char mdate[64];
	char color[10];
	char *str;
	char *with_font_tag;
	char *sml_attrib = NULL;

	if(length == -1)
		length = strlen(message) + 1;

	gtkconv = GAIM_GTK_CONVERSATION(conv);
	gc = gaim_conversation_get_gc(conv);

	win = gaim_conversation_get_window(conv);

	if (!(flags & WFLAG_NOLOG) &&
		((gaim_conversation_get_type(conv) == GAIM_CONV_CHAT &&
		  gaim_prefs_get_bool("/gaim/gtk/conversations/chat/raise_on_events")) ||
		 (gaim_conversation_get_type(conv) == GAIM_CONV_IM &&
		  (gaim_prefs_get_bool("/gaim/gtk/conversations/im/raise_on_events") ||
		   gaim_prefs_get_bool("/gaim/gtk/conversations/im/hide_on_send"))))) {

		gaim_window_show(win);
	}


	if(time(NULL) > mtime + 20*60) /* show date if older than 20 minutes */
		strftime(mdate, sizeof(mdate), "%Y-%m-%d %H:%M:%S", localtime(&mtime));
	else
		strftime(mdate, sizeof(mdate), "%H:%M:%S", localtime(&mtime));

	if(gc)
		sml_attrib = g_strdup_printf("sml=\"%s\"", gc->prpl->info->name);

	gtk_font_options ^= GTK_IMHTML_NO_COMMENTS;

	if (gaim_prefs_get_bool("/gaim/gtk/conversations/ignore_colors"))
		gtk_font_options ^= GTK_IMHTML_NO_COLOURS;

	if (gaim_prefs_get_bool("/gaim/gtk/conversations/ignore_fonts"))
		gtk_font_options ^= GTK_IMHTML_NO_FONTS;

	if (gaim_prefs_get_bool("/gaim/gtk/conversations/ignore_font_sizes"))
		gtk_font_options ^= GTK_IMHTML_NO_SIZES;

	if (!gaim_prefs_get_bool("/gaim/gtk/logging/strip_html"))
		gtk_font_options ^= GTK_IMHTML_RETURN_LOG;

	if (GAIM_PLUGIN_PROTOCOL_INFO(conv->account->gc->prpl)->options &
		OPT_PROTO_USE_POINTSIZE) {

		gtk_font_options ^= GTK_IMHTML_USE_POINTSIZE;
	}

	if (flags & WFLAG_SYSTEM) {
		if (gaim_prefs_get_bool("/gaim/gtk/conversations/show_timestamps"))
			g_snprintf(buf, BUF_LONG, "(%s) <B>%s</B>",
					   mdate, message);
		else
			g_snprintf(buf, BUF_LONG, "<B>%s</B>", message);

		g_snprintf(buf2, sizeof(buf2),
				   "<!--(%s) --><B>%s</B><BR>",
				   mdate, message);

		gtk_imhtml_append_text(GTK_IMHTML(gtkconv->imhtml), buf2, -1, 0);

		if (gaim_prefs_get_bool("/gaim/gtk/logging/strip_html")) {
			char *t1 = strip_html(buf);

			conv->history = g_string_append(conv->history, t1);
			conv->history = g_string_append(conv->history, "\n");

			g_free(t1);
		}
		else {
			conv->history = g_string_append(conv->history, buf);
			conv->history = g_string_append(conv->history, "<BR>\n");
		}

		if (!(flags & WFLAG_NOLOG) && gaim_conversation_is_logging(conv)) {

			char *t1;
			char nm[256];

			if (gaim_prefs_get_bool("/gaim/gtk/logging/strip_html"))
				t1 = strip_html(buf);
			else
				t1 = buf;

			if (gaim_conversation_get_type(conv) == GAIM_CONV_CHAT)
				g_snprintf(nm, sizeof(nm), "%s.chat",
						   gaim_conversation_get_name(conv));
			else
				strncpy(nm, gaim_conversation_get_name(conv), sizeof(nm));

			fd = open_log_file(nm,
				(gaim_conversation_get_type(conv) == GAIM_CONV_CHAT));

			if (fd) {
				if (gaim_prefs_get_bool("/gaim/gtk/logging/strip_html"))
					fprintf(fd, "%s\n", t1);
				else
					fprintf(fd, "%s<BR>\n", t1);

				fclose(fd);
			}

			if (gaim_prefs_get_bool("/gaim/gtk/logging/strip_html"))
				g_free(t1);
		}
	}
	else if (flags & WFLAG_NOLOG) {
		g_snprintf(buf, BUF_LONG,
				   "<B><FONT COLOR=\"#777777\">%s</FONT></B><BR>",
				   message);

		gtk_imhtml_append_text(GTK_IMHTML(gtkconv->imhtml), buf, -1, 0);
	}
	else {
		char *new_message = g_memdup(message, length);
		
		if (flags & WFLAG_WHISPER) {
			str = g_malloc(1024);

			/* If we're whispering, it's not an autoresponse. */
			if (meify(new_message, length)) {
				g_snprintf(str, 1024, "***%s", who);
				strcpy(color, "#6C2585");
			}
			else {
				g_snprintf(str, 1024, "*%s*:", who);
				strcpy(color, "#00FF00");
			}
		}
		else {
			if (meify(new_message, length)) {
				str = g_malloc(1024);

				if (flags & WFLAG_AUTO)
					g_snprintf(str, 1024, "%s ***%s", AUTO_RESPONSE, who);
				else
					g_snprintf(str, 1024, "***%s", who);

				if (flags & WFLAG_NICK)
					strcpy(color, "#AF7F00");
				else
					strcpy(color, "#062585");
			}
			else {
				str = g_malloc(1024);

				if (flags & WFLAG_AUTO)
					g_snprintf(str, 1024, "%s %s", who, AUTO_RESPONSE);
				else
					g_snprintf(str, 1024, "%s:", who);

				if (flags & WFLAG_NICK)
					strcpy(color, "#AF7F00");
				else if (flags & WFLAG_RECV) {
					if (flags & WFLAG_COLORIZE) {
						const char *u;
						int m = 0;

						for (u = who; *u != '\0'; u++)
							m += *u;

						m = m % NUM_NICK_COLORS;

						strcpy(color, nick_colors[m]);
					}
					else
						strcpy(color, "#A82F2F");
				}
				else if (flags & WFLAG_SEND)
					strcpy(color, "#16569E");
			}
		}

		if (gaim_prefs_get_bool("/gaim/gtk/conversations/show_timestamps"))
			g_snprintf(buf, BUF_LONG,
					   "<FONT COLOR=\"%s\" %s>(%s) "
					   "<B>%s</B></FONT> ", color,
					   sml_attrib ? sml_attrib : "", mdate, str);
		else
			g_snprintf(buf, BUF_LONG,
					   "<FONT COLOR=\"%s\" %s><B>%s</B></FONT> ", color,
					   sml_attrib ? sml_attrib : "", str);

		g_snprintf(buf2, BUF_LONG,
				   "<FONT COLOR=\"%s\" %s><!--(%s) -->"
				   "<B>%s</B></FONT> ",
				   color, sml_attrib ? sml_attrib : "", mdate, str);

		g_free(str);

		gtk_imhtml_append_text(GTK_IMHTML(gtkconv->imhtml), buf2, -1, 0);

		if(gc){
			char *pre = g_strdup_printf("<font %s>", sml_attrib ? sml_attrib : "");
			char *post = "</font>";
			int pre_len = strlen(pre);
			int post_len = strlen(post);

			with_font_tag = g_malloc(length + pre_len + post_len + 1);

			strcpy(with_font_tag, pre);
			memcpy(with_font_tag + pre_len, new_message, length);
			strcpy(with_font_tag + pre_len + length, post);

			length += pre_len + post_len;
			g_free(pre);
		}
		else
			with_font_tag = g_memdup(new_message, length);

		log_str = gtk_imhtml_append_text(GTK_IMHTML(gtkconv->imhtml),
										 with_font_tag, length, gtk_font_options);

		gtk_imhtml_append_text(GTK_IMHTML(gtkconv->imhtml), "<BR>", -1, 0);

		/* XXX This needs to be updated for the new length argument. */
		if (gaim_prefs_get_bool("/gaim/gtk/logging/strip_html")) {
			char *t1, *t2;

			t1 = strip_html(buf);
			t2 = strip_html(new_message);

			conv->history = g_string_append(conv->history, t1);
			conv->history = g_string_append(conv->history, t2);
			conv->history = g_string_append(conv->history, "\n");

			g_free(t1);
			g_free(t2);
		}
		else {
			char *t1, *t2;

			t1 = html_logize(buf);
			t2 = html_logize(new_message);

			conv->history = g_string_append(conv->history, t1);
			conv->history = g_string_append(conv->history, t2);
			conv->history = g_string_append(conv->history, "\n");
			conv->history = g_string_append(conv->history, log_str->str);
			conv->history = g_string_append(conv->history, "<BR>\n");

			g_free(t1);
			g_free(t2);
		}

		/* XXX This needs to be updated for the new length argument. */
		if (gaim_conversation_is_logging(conv)) {
			char *t1, *t2;
			char nm[256];

			if (gaim_conversation_get_type(conv) == GAIM_CONV_CHAT)
				g_snprintf(nm, sizeof(nm), "%s.chat",
						   gaim_conversation_get_name(conv));
			else
				strncpy(nm, gaim_conversation_get_name(conv), sizeof(nm));

			if (gaim_prefs_get_bool("/gaim/gtk/logging/strip_html")) {
				t1 = strip_html(buf);
				t2 = strip_html(with_font_tag);
			}
			else {
				t1 = html_logize(buf);
				t2 = html_logize(with_font_tag);
			}

			fd = open_log_file(nm,
				(gaim_conversation_get_type(conv) == GAIM_CONV_CHAT));

			if (fd) {
				if (gaim_prefs_get_bool("/gaim/gtk/logging/strip_html"))
					fprintf(fd, "%s%s\n", t1, t2);
				else {
					fprintf(fd, "%s%s%s<BR>\n", t1, t2, log_str->str);
					g_string_free(log_str, TRUE);
				}

				fclose(fd);
			}

			g_free(t1);
			g_free(t2);
		}

		g_free(with_font_tag);
		g_free(new_message);
	}
	if(sml_attrib)
		g_free(sml_attrib);
}

static void
gaim_gtkconv_chat_add_user(GaimConversation *conv, const char *user)
{
	GaimChat *chat;
	GaimGtkConversation *gtkconv;
	GaimGtkChatPane *gtkchat;
	char tmp[BUF_LONG];
	int num_users;
	int pos;

	chat    = GAIM_CHAT(conv);
	gtkconv = GAIM_GTK_CONVERSATION(conv);
	gtkchat = gtkconv->u.chat;

	num_users = g_list_length(gaim_chat_get_users(chat));

	g_snprintf(tmp, sizeof(tmp),
			   ngettext("%d person in room", "%d people in room",
						num_users),
			   num_users);

	gtk_label_set_text(GTK_LABEL(gtkchat->count), tmp);

	if (gtkconv->make_sound)
		gaim_sound_play_event(GAIM_SOUND_CHAT_JOIN);

	pos = g_list_index(gaim_chat_get_users(chat), user);

	add_chat_buddy_common(conv, user, pos);
}

static void
gaim_gtkconv_chat_rename_user(GaimConversation *conv, const char *old_name,
							  const char *new_name)
{
	GaimChat *chat;
	GaimGtkConversation *gtkconv;
	GaimGtkChatPane *gtkchat;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GList *names;
	int pos;
	int f = 1;

	chat    = GAIM_CHAT(conv);
	gtkconv = GAIM_GTK_CONVERSATION(conv);
	gtkchat = gtkconv->u.chat;

	for (names = gaim_chat_get_users(chat);
		 names != NULL;
		 names = names->next) {

		char *u = (char *)names->data;

		if (!gaim_utf8_strcasecmp(u, old_name)) {
			model = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));

			if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter))
				break;

			while (f != 0) {
				char *val;

				gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 1, &val, -1);

				if (!gaim_utf8_strcasecmp(old_name, val)) {
					gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
					break;
				}

				f = gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter);

				g_free(val);
			}
			
			break;
		}
	}

	if (!names)
		return;

	pos = g_list_index(gaim_chat_get_users(chat), new_name);

	add_chat_buddy_common(conv, new_name, pos);
}

static void
gaim_gtkconv_chat_remove_user(GaimConversation *conv, const char *user)
{
	GaimChat *chat;
	GaimGtkConversation *gtkconv;
	GaimGtkChatPane *gtkchat;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GList *names;
	char tmp[BUF_LONG];
	int num_users;
	int f = 1;

	chat    = GAIM_CHAT(conv);
	gtkconv = GAIM_GTK_CONVERSATION(conv);
	gtkchat = gtkconv->u.chat;

	num_users = g_list_length(gaim_chat_get_users(chat)) - 1;

	for (names = gaim_chat_get_users(chat);
		 names != NULL;
		 names = names->next) {

		char *u = (char *)names->data;

		if (!gaim_utf8_strcasecmp(u, user)) {
			model = gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));

			if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &iter))
				break;

			while (f != 0) {
				char *val;

				gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, 1, &val, -1);

				if (!gaim_utf8_strcasecmp(user, val))
					gtk_list_store_remove(GTK_LIST_STORE(model), &iter);

				f = gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &iter);

				g_free(val);
			}

			break;
		}
	}

	if (names == NULL)
		return;

	g_snprintf(tmp, sizeof(tmp),
			   ngettext("%d person in room", "%d people in room",
						num_users), num_users);

	gtk_label_set_text(GTK_LABEL(gtkchat->count), tmp);

	if (gtkconv->make_sound)
		gaim_sound_play_event(GAIM_SOUND_CHAT_LEAVE);
}

static void
gaim_gtkconv_set_title(GaimConversation *conv, const char *title)
{
	GaimGtkConversation *gtkconv;
	GaimWindow *win;
	GaimGtkWindow *gtkwin;

	win = gaim_conversation_get_window(conv);
	gtkwin = GAIM_GTK_WINDOW(win);
	gtkconv = GAIM_GTK_CONVERSATION(conv);

	gtk_label_set_text(GTK_LABEL(gtkconv->tab_label), title);

	if(conv == gaim_window_get_active_conversation(win))
		gtk_window_set_title(GTK_WINDOW(gtkwin->window), title);
}

static void
gaim_gtkconv_updated(GaimConversation *conv, GaimConvUpdateType type)
{
	GaimWindow *win;
	GaimGtkWindow *gtkwin;
	GaimGtkConversation *gtkconv;
	GaimGtkChatPane *gtkchat;
	GaimChat *chat;

	win     = gaim_conversation_get_window(conv);
	gtkwin = GAIM_GTK_WINDOW(win);
	gtkconv = GAIM_GTK_CONVERSATION(conv);

	if (type == GAIM_CONV_UPDATE_ACCOUNT) {
		gaim_conversation_autoset_title(conv);
		gaim_gtkconv_update_buddy_icon(conv);
		gaim_gtkconv_update_buttons_by_protocol(conv);

		g_timeout_add(0, (GSourceFunc)update_send_as_selection, win);

		smiley_themeize(gtkconv->imhtml);
	}
	else if (type == GAIM_CONV_UPDATE_TYPING ||
			 type == GAIM_CONV_UPDATE_UNSEEN) {
		GtkStyle *style;
		GaimIm *im = NULL;


		if (gaim_conversation_get_type(conv) == GAIM_CONV_IM)
			im = GAIM_IM(conv);

		style = gtk_style_new();

		if (!GTK_WIDGET_REALIZED(gtkconv->tab_label))
			gtk_widget_realize(gtkconv->tab_label);

		pango_font_description_free(style->font_desc);
		style->font_desc = pango_font_description_copy(
				gtk_widget_get_style(gtkconv->tab_label)->font_desc);

		if (im != NULL && gaim_im_get_typing_state(im) == TYPING) {
			style->fg[GTK_STATE_NORMAL].red   = 0x4646;
			style->fg[GTK_STATE_NORMAL].green = 0xA0A0;
			style->fg[GTK_STATE_NORMAL].blue  = 0x4646;
			style->fg[GTK_STATE_ACTIVE] = style->fg[GTK_STATE_NORMAL];
		}
		else if (im != NULL && gaim_im_get_typing_state(im) == TYPED) {
			style->fg[GTK_STATE_NORMAL].red   = 0xD1D1;
			style->fg[GTK_STATE_NORMAL].green = 0x9494;
			style->fg[GTK_STATE_NORMAL].blue  = 0x0C0C;
			style->fg[GTK_STATE_ACTIVE] = style->fg[GTK_STATE_NORMAL];
		}
		else if (gaim_conversation_get_unseen(conv) == GAIM_UNSEEN_NICK) {
			style->fg[GTK_STATE_ACTIVE].red   = 0x3131;
			style->fg[GTK_STATE_ACTIVE].green = 0x4E4E;
			style->fg[GTK_STATE_ACTIVE].blue  = 0x6C6C;
			style->fg[GTK_STATE_NORMAL] = style->fg[GTK_STATE_ACTIVE];
		}
		else if (gaim_conversation_get_unseen(conv) == GAIM_UNSEEN_TEXT) {
			style->fg[GTK_STATE_ACTIVE].red   = 0xDFDF;
			style->fg[GTK_STATE_ACTIVE].green = 0x4242;
			style->fg[GTK_STATE_ACTIVE].blue  = 0x1E1E;
			style->fg[GTK_STATE_NORMAL] = style->fg[GTK_STATE_ACTIVE];
		}

		gtk_widget_set_style(gtkconv->tab_label, style);
		g_object_unref(G_OBJECT(style));

		if(conv == gaim_window_get_active_conversation(win)) {
			update_typing_icon(conv);
		}

	}
	else if (type == GAIM_CONV_UPDATE_TOPIC) {
		chat = GAIM_CHAT(conv);
		gtkchat = gtkconv->u.chat;

		gtk_entry_set_text(GTK_ENTRY(gtkchat->topic_text),
						   gaim_chat_get_topic(chat));
	}
	else if (type == GAIM_CONV_ACCOUNT_ONLINE ||
			 type == GAIM_CONV_ACCOUNT_OFFLINE) {

		generate_send_as_items(win, NULL);
	}
	else if(type == GAIM_CONV_UPDATE_ADD ||
			type == GAIM_CONV_UPDATE_REMOVE) {

		update_convo_add_button(conv);
	}
}

static GaimConversationUiOps conversation_ui_ops =
{
	gaim_gtkconv_destroy,            /* destroy_conversation */
	gaim_gtkconv_write_chat,         /* write_chat           */
	gaim_gtkconv_write_im,           /* write_im             */
	gaim_gtkconv_write_conv,         /* write_conv           */
	gaim_gtkconv_chat_add_user,      /* chat_add_user        */
	gaim_gtkconv_chat_rename_user,   /* chat_rename_user     */
	gaim_gtkconv_chat_remove_user,   /* chat_remove_user     */
	gaim_gtkconv_set_title,          /* set_title            */
	NULL,                            /* update_progress      */
	gaim_gtkconv_updated             /* updated              */
};

GaimConversationUiOps *
gaim_get_gtk_conversation_ui_ops(void)
{
	return &conversation_ui_ops;
}

/**************************************************************************
 * Public conversation utility functions
 **************************************************************************/
static void
remove_icon(GaimGtkConversation *gtkconv)
{
	g_return_if_fail(gtkconv != NULL);

	if (gtkconv->u.im->icon != NULL)
		gtk_container_remove(GTK_CONTAINER(gtkconv->bbox),
							 gtkconv->u.im->icon->parent->parent);

	if (gtkconv->u.im->anim != NULL)
		g_object_unref(G_OBJECT(gtkconv->u.im->anim));

	if (gtkconv->u.im->icon_timer != 0)
		g_source_remove(gtkconv->u.im->icon_timer);

	if (gtkconv->u.im->iter != NULL)
		g_object_unref(G_OBJECT(gtkconv->u.im->iter));

	gtkconv->u.im->icon_timer = 0;
	gtkconv->u.im->icon = NULL;
	gtkconv->u.im->anim = NULL;
	gtkconv->u.im->iter = NULL;
}

static gboolean
redraw_icon(gpointer data)
{
	GaimConversation *conv = (GaimConversation *)data;
	GaimGtkConversation *gtkconv;

	GdkPixbuf *buf;
	GdkPixbuf *scale;
	GdkPixmap *pm;
	GdkBitmap *bm;
	gint delay;

	if (!g_list_find(gaim_get_ims(), conv)) {
		gaim_debug(GAIM_DEBUG_WARNING, "gtkconv",
				   "Conversation not found in redraw_icon. I think this "
				   "is a bug.\n");
		return FALSE;
	}

	gtkconv = GAIM_GTK_CONVERSATION(conv);

	gdk_pixbuf_animation_iter_advance(gtkconv->u.im->iter, NULL);
	buf = gdk_pixbuf_animation_iter_get_pixbuf(gtkconv->u.im->iter);

	scale = gdk_pixbuf_scale_simple(buf,
		MAX(gdk_pixbuf_get_width(buf) * SCALE(gtkconv->u.im->anim) /
		    gdk_pixbuf_animation_get_width(gtkconv->u.im->anim), 1),
		MAX(gdk_pixbuf_get_height(buf) * SCALE(gtkconv->u.im->anim) /
		    gdk_pixbuf_animation_get_height(gtkconv->u.im->anim), 1),
		GDK_INTERP_NEAREST);

	gdk_pixbuf_render_pixmap_and_mask(scale, &pm, &bm, 100);
	g_object_unref(G_OBJECT(scale));
	gtk_image_set_from_pixmap(GTK_IMAGE(gtkconv->u.im->icon), pm, bm);
	g_object_unref(G_OBJECT(pm));
	gtk_widget_queue_draw(gtkconv->u.im->icon);

	if (bm)
		g_object_unref(G_OBJECT(bm));

	delay = gdk_pixbuf_animation_iter_get_delay_time(gtkconv->u.im->iter) / 10;

	gtkconv->u.im->icon_timer = g_timeout_add(delay * 10, redraw_icon, conv);

	return FALSE;
}

static void
start_anim(GtkObject *obj, GaimConversation *conv)
{
	GaimGtkConversation *gtkconv;
	int delay;

	if (!GAIM_IS_GTK_CONVERSATION(conv))
		return;

	gtkconv = GAIM_GTK_CONVERSATION(conv);

	delay = gdk_pixbuf_animation_iter_get_delay_time(gtkconv->u.im->iter) / 10;

	if (gtkconv->u.im->anim)
	    gtkconv->u.im->icon_timer = g_timeout_add(delay * 10, redraw_icon,
												  conv);
}

static void
stop_anim(GtkObject *obj, GaimConversation *conv)
{
	GaimGtkConversation *gtkconv;

	if (!GAIM_IS_GTK_CONVERSATION(conv))
		return;

	gtkconv = GAIM_GTK_CONVERSATION(conv);

	if (gtkconv->u.im->icon_timer != 0)
		g_source_remove(gtkconv->u.im->icon_timer);

	gtkconv->u.im->icon_timer = 0;
}

static gboolean
icon_menu(GtkObject *obj, GdkEventButton *e, GaimConversation *conv)
{
	GaimGtkConversation *gtkconv;
	static GtkWidget *menu = NULL;
	GtkWidget *button;

	if (e->button != 3 || e->type != GDK_BUTTON_PRESS)
		return FALSE;

	gtkconv = GAIM_GTK_CONVERSATION(conv);

	/*
	 * If a menu already exists, destroy it before creating a new one,
	 * thus freeing-up the memory it occupied.
	 */
	if (menu != NULL)
		gtk_widget_destroy(menu);

	menu = gtk_menu_new();

	if (gtkconv->u.im->icon_timer) {
		button = gtk_menu_item_new_with_label(_("Disable Animation"));
		g_signal_connect(G_OBJECT(button), "activate",
						 G_CALLBACK(stop_anim), conv);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), button);
		gtk_widget_show(button);
	}
	else if (gtkconv->u.im->anim &&
			 !(gdk_pixbuf_animation_is_static_image(gtkconv->u.im->anim))) 
	{
		button = gtk_menu_item_new_with_label(_("Enable Animation"));
		g_signal_connect(G_OBJECT(button), "activate",
						 G_CALLBACK(start_anim), conv);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), button);
		gtk_widget_show(button);
	}

	button = gtk_menu_item_new_with_label(_("Hide Icon"));
	g_signal_connect_swapped(G_OBJECT(button), "activate",
							 G_CALLBACK(remove_icon), gtkconv);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), button);
	gtk_widget_show(button);

	button = gtk_menu_item_new_with_label(_("Save Icon As..."));
	g_signal_connect(G_OBJECT(button), "activate",
					 G_CALLBACK(gaim_gtk_save_icon_dialog), conv);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), button);
	gtk_widget_show(button);

	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, e->button, e->time);

	return TRUE;
}

void
gaim_gtkconv_update_buddy_icon(GaimConversation *conv)
{
	GaimGtkConversation *gtkconv;

	char filename[256];
	FILE *file;
	GError *err = NULL;
	gboolean animate = TRUE;

	struct buddy *buddy;

	void *data;
	int len, delay;

	GdkPixbuf *buf;

	GtkWidget *event;
	GtkWidget *frame;
	GdkPixbuf *scale;
	GdkPixmap *pm;
	GdkBitmap *bm;
	int sf = 0;

	g_return_if_fail(conv != NULL);
	g_return_if_fail(GAIM_IS_GTK_CONVERSATION(conv));
	g_return_if_fail(gaim_conversation_get_type(conv) == GAIM_CONV_IM);

	gtkconv = GAIM_GTK_CONVERSATION(conv);

	if (gtkconv->u.im->icon_timer == 0 && gtkconv->u.im->icon != NULL)
		animate = FALSE;

	remove_icon(gtkconv);

	if (!gaim_prefs_get_bool("/gaim/gtk/conversations/im/show_buddy_icons"))
		return;

	if (gaim_conversation_get_gc(conv) == NULL)
		return;

	if(gtkconv->u.im->anim)
		g_object_unref(G_OBJECT(gtkconv->u.im->anim));

	if((buddy = gaim_find_buddy(gaim_conversation_get_account(conv),
					gaim_conversation_get_name(conv))) != NULL) {
		char *file = gaim_buddy_get_setting(buddy, "buddy_icon");
		if(file) {
			gtkconv->u.im->anim = gdk_pixbuf_animation_new_from_file(file, &err);
			g_free(file);
		}
	} else {
		data = get_icon_data(gaim_conversation_get_gc(conv),
				normalize(gaim_conversation_get_name(conv)),
				&len);

		if (!data)
			return;

		/* this is such an evil hack, i don't know why i'm even considering it.
		 * we'll do it differently when gdk-pixbuf-loader isn't leaky anymore. */
		g_snprintf(filename, sizeof(filename),
				"%s" G_DIR_SEPARATOR_S "gaimicon-%s.%d",
				g_get_tmp_dir(), gaim_conversation_get_name(conv), getpid());

		if (!(file = fopen(filename, "wb")))
			return;

		fwrite(data, 1, len, file);
		fclose(file);

		gtkconv->u.im->anim = gdk_pixbuf_animation_new_from_file(filename, &err);
		/* make sure we remove the file as soon as possible */
		unlink(filename);
	}

	if (err) {
		gaim_debug(GAIM_DEBUG_ERROR, "gtkconv",
				   "Buddy icon error: %s\n", err->message);
		g_error_free(err);
	}


	if (!gtkconv->u.im->anim)
		return;

	if(gtkconv->u.im->iter)
		g_object_unref(G_OBJECT(gtkconv->u.im->iter));

	if (gdk_pixbuf_animation_is_static_image(gtkconv->u.im->anim)) {
		gtkconv->u.im->iter = NULL;
		delay = 0;
		buf = gdk_pixbuf_animation_get_static_image(gtkconv->u.im->anim);
	} else {
		gtkconv->u.im->iter =
			gdk_pixbuf_animation_get_iter(gtkconv->u.im->anim, NULL);
		buf = gdk_pixbuf_animation_iter_get_pixbuf(gtkconv->u.im->iter);
		delay = gdk_pixbuf_animation_iter_get_delay_time(gtkconv->u.im->iter);
		delay = delay / 10;
	}

	sf = SCALE(gtkconv->u.im->anim);
	scale = gdk_pixbuf_scale_simple(buf,
				MAX(gdk_pixbuf_get_width(buf) * sf /
				    gdk_pixbuf_animation_get_width(gtkconv->u.im->anim), 1),
				MAX(gdk_pixbuf_get_height(buf) * sf /
				    gdk_pixbuf_animation_get_height(gtkconv->u.im->anim), 1),
				GDK_INTERP_NEAREST);

	if (delay)
		gtkconv->u.im->icon_timer = g_timeout_add(delay * 10, redraw_icon,
												  conv);

	gdk_pixbuf_render_pixmap_and_mask(scale, &pm, &bm, 100);
	g_object_unref(G_OBJECT(scale));

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame),
							  (bm ? GTK_SHADOW_NONE : GTK_SHADOW_IN));
	gtk_box_pack_start(GTK_BOX(gtkconv->bbox), frame, FALSE, FALSE, 5);
	gtk_box_reorder_child(GTK_BOX(gtkconv->bbox), frame, 0);
	gtk_widget_show(frame);

	event = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(frame), event);
	g_signal_connect(G_OBJECT(event), "button-press-event",
					 G_CALLBACK(icon_menu), conv);
	gtk_widget_show(event);

	gtkconv->u.im->icon = gtk_image_new_from_pixmap(pm, bm);
	gtk_widget_set_size_request(gtkconv->u.im->icon, sf, sf);
	gtk_container_add(GTK_CONTAINER(event), gtkconv->u.im->icon);
	gtk_widget_show(gtkconv->u.im->icon);

	if (!animate ||
		!gaim_prefs_get_bool("/gaim/gtk/conversations/im/animate_buddy_icons")) {
		stop_anim(NULL, conv);
	}

	g_object_unref(G_OBJECT(pm));

	if (bm)
		g_object_unref(G_OBJECT(bm));
}

void
gaim_gtkconv_update_font_buttons(void)
{
	GList *l;
	GaimConversation *conv;
	GaimGtkConversation *gtkconv;

	for (l = gaim_get_ims(); l != NULL; l = l->next) {
		conv = (GaimConversation *)l->data;

		if (!GAIM_IS_GTK_CONVERSATION(conv))
			continue;

		gtkconv = GAIM_GTK_CONVERSATION(conv);

		if (gtkconv->toolbar.bold != NULL)
			gtk_widget_set_sensitive(gtkconv->toolbar.bold,
					!gaim_prefs_get_bool("/gaim/gtk/conversations/send_bold"));

		if (gtkconv->toolbar.italic != NULL)
			gtk_widget_set_sensitive(gtkconv->toolbar.italic,
					!gaim_prefs_get_bool("/gaim/gtk/conversations/send_italic"));

		if (gtkconv->toolbar.underline != NULL)
			gtk_widget_set_sensitive(gtkconv->toolbar.underline,
					!gaim_prefs_get_bool("/gaim/gtk/conversations/send_underline"));
	}
}

void
gaim_gtkconv_update_font_colors(GaimConversation *conv)
{
	GaimGtkConversation *gtkconv;
	
	if (!GAIM_IS_GTK_CONVERSATION(conv))
		return;
	
	gtkconv = GAIM_GTK_CONVERSATION(conv);

	gdk_color_parse(gaim_prefs_get_string("/gaim/gtk/conversations/fgcolor"),
					&gtkconv->fg_color);

	gdk_color_parse(gaim_prefs_get_string("/gaim/gtk/conversations/bgcolor"),
					&gtkconv->bg_color);
}

void
gaim_gtkconv_update_font_face(GaimConversation *conv)
{
	GaimGtkConversation *gtkconv;
	
	if (!GAIM_IS_GTK_CONVERSATION(conv))
		return;
	
	gtkconv = GAIM_GTK_CONVERSATION(conv);

	strncpy(gtkconv->fontface, fontface, 128);
}

void
gaim_gtkconv_update_buttons_by_protocol(GaimConversation *conv)
{
	GaimPluginProtocolInfo *prpl_info = NULL;
	GaimWindow *win;
	GaimGtkWindow *gtkwin = NULL;
	GaimGtkConversation *gtkconv;
	GaimConnection *gc;

	if (!GAIM_IS_GTK_CONVERSATION(conv))
		return;

	gc      = gaim_conversation_get_gc(conv);
	win     = gaim_conversation_get_window(conv);
	gtkconv = GAIM_GTK_CONVERSATION(conv);

	if (win != NULL)
		gtkwin = GAIM_GTK_WINDOW(win);

	if (gc == NULL) {
		gtk_widget_set_sensitive(gtkconv->send, FALSE);

		if (win != NULL && gaim_window_get_active_conversation(win) == conv) {
			gtk_widget_set_sensitive(gtkwin->menu.insert_link, FALSE);
		}
	}
	else {
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

		gtk_widget_set_sensitive(gtkconv->send, TRUE);

		if (win != NULL)
			gtk_widget_set_sensitive(gtkwin->menu.insert_link, TRUE);
	}

	if (gaim_conversation_get_type(conv) == GAIM_CONV_IM) {
		if (gc == NULL) {
			gtk_widget_set_sensitive(gtkconv->info,        FALSE);
			gtk_widget_set_sensitive(gtkconv->u.im->warn,  FALSE);
			gtk_widget_set_sensitive(gtkconv->u.im->block, FALSE);
			gtk_widget_set_sensitive(gtkconv->u.im->add,   FALSE);

			if (win != NULL &&
				gaim_window_get_active_conversation(win) == conv) {

				gtk_widget_set_sensitive(gtkwin->menu.insert_image, FALSE);
			}

			return;
		}

		gtk_widget_set_sensitive(gtkconv->info,
								 (prpl_info->get_info != NULL));

		gtk_widget_set_sensitive(gtkconv->toolbar.image,
								 (prpl_info->options & OPT_PROTO_IM_IMAGE));

		if (win != NULL && gaim_window_get_active_conversation(win) == conv) {
			gtk_widget_set_sensitive(gtkwin->menu.insert_image,
									 (prpl_info->options & OPT_PROTO_IM_IMAGE));
		}

		gtk_widget_set_sensitive(gtkconv->u.im->warn,
								 (prpl_info->warn != NULL));

		gtk_widget_set_sensitive(gtkconv->u.im->block,
								 (prpl_info->add_permit != NULL));

		update_convo_add_button(conv);
	}
	else if (gaim_conversation_get_type(conv) == GAIM_CONV_CHAT) {
		if (gc == NULL) {
			if (gtkconv->u.chat->whisper != NULL)
				gtk_widget_set_sensitive(gtkconv->u.chat->whisper, FALSE);

			gtk_widget_set_sensitive(gtkconv->u.chat->invite,  FALSE);

			return;
		}

		gtk_widget_set_sensitive(gtkconv->send, (prpl_info->chat_send != NULL));

		gtk_widget_set_sensitive(gtkconv->toolbar.image, FALSE);
		/* gtk_widget_set_sensitive(gtkwin->menu.insert_image, FALSE); */

		if (gtkconv->u.chat->whisper != NULL)
			gtk_widget_set_sensitive(gtkconv->u.chat->whisper,
									 (prpl_info->chat_whisper != NULL));

		gtk_widget_set_sensitive(gtkconv->u.chat->invite,
								 (prpl_info->chat_invite != NULL));
	}
}

GaimWindow *
gaim_gtkwin_get_at_xy(int x, int y)
{
	GaimWindow *win = NULL;
	GaimGtkWindow *gtkwin;
	GdkWindow *gdkwin;
	GList *l;

	gdkwin = gdk_window_at_pointer(&x, &y);

	if (gdkwin)
		gdkwin = gdk_window_get_toplevel(gdkwin);

	for (l = gaim_get_windows(); l != NULL; l = l->next) {
		win = (GaimWindow *)l->data;

		if (!GAIM_IS_GTK_WINDOW(win))
			continue;

		gtkwin = GAIM_GTK_WINDOW(win);

		if (gdkwin == gtkwin->window->window)
			return win;
	}

	return NULL;
}

int
gaim_gtkconv_get_tab_at_xy(GaimWindow *win, int x, int y)
{
	GaimGtkWindow *gtkwin;
	GList *l;
	gint nb_x, nb_y, x_rel, y_rel;
	GtkNotebook *notebook;
	GtkWidget *tab;
	gint i, page_num = 0;
	gboolean first_visible = TRUE;

	if (!GAIM_IS_GTK_WINDOW(win))
		return -1;

	gtkwin = GAIM_GTK_WINDOW(win);
	notebook = GTK_NOTEBOOK(gtkwin->notebook);

	gdk_window_get_origin(gtkwin->notebook->window, &nb_x, &nb_y);
	x_rel = x - nb_x;
	y_rel = y - nb_y;

	for (l = gaim_window_get_conversations(win), i = 0;
		 l != NULL;
		 l = l->next, i++) {

		GaimConversation *conv = l->data;
		tab = GAIM_GTK_CONVERSATION(conv)->tab_label;

		if (!GTK_WIDGET_MAPPED(tab))
			continue;

		if (first_visible) {
			first_visible = FALSE;

			if (x_rel < tab->allocation.x) x_rel = tab->allocation.x;
			if (y_rel < tab->allocation.y) y_rel = tab->allocation.y;
		}

		if (gtk_notebook_get_tab_pos(notebook) == GTK_POS_TOP ||
			gtk_notebook_get_tab_pos(notebook) == GTK_POS_BOTTOM) {

			if (tab->allocation.x <= x_rel) {
				if (tab->allocation.x + tab->allocation.width <= x_rel)
					page_num = i + 1;
				else
					page_num = i;
			}
			else
				break;
		}
		else {
			if (tab->allocation.y <= y_rel) {
				if (tab->allocation.y + tab->allocation.height <= y_rel)
					page_num = i + 1;
				else
					page_num = i;
			}
			else
				break;
		}
	}

	if (i == gaim_window_get_conversation_count(win) + 1)
		return -1;

	return page_num;
}

int
gaim_gtkconv_get_dest_tab_at_xy(GaimWindow *win, int x, int y)
{
	GaimGtkWindow *gtkwin;
	GList *l;
	gint nb_x, nb_y, x_rel, y_rel;
	GtkNotebook *notebook;
	GtkWidget *tab;
	gint i, page_num = 0;

	if (!GAIM_IS_GTK_WINDOW(win))
		return -1;

	gtkwin   = GAIM_GTK_WINDOW(win);
	notebook = GTK_NOTEBOOK(gtkwin->notebook);

	gdk_window_get_origin(gtkwin->notebook->window, &nb_x, &nb_y);
	x_rel = x - nb_x;
	y_rel = y - nb_y;

	for (l = gaim_window_get_conversations(win), i = 0;
		 l != NULL;
		 l = l->next, i++) {

		GaimConversation *conv = l->data;
		tab = GAIM_GTK_CONVERSATION(conv)->tab_label;

		if (!GTK_WIDGET_MAPPED(tab))
			continue;

		if (gtk_notebook_get_tab_pos(notebook) == GTK_POS_TOP ||
			gtk_notebook_get_tab_pos(notebook) == GTK_POS_BOTTOM) {

			if (tab->allocation.x <= x_rel) {
				if (tab->allocation.x + (tab->allocation.width / 2) <= x_rel)
					page_num = i + 1;
				else
					page_num = i;
			}
			else
				break;
		}
		else {
			if (tab->allocation.y <= y_rel) {
				if (tab->allocation.y + (tab->allocation.height / 2) <= y_rel)
					page_num = i + 1;
				else
					page_num = i;
			}
			else
				break;
		}
	}

	if (i == gaim_window_get_conversation_count(win) + 1)
		return -1;

	return page_num;
}

static void
close_on_tabs_pref_cb(const char *name, GaimPrefType type, gpointer value,
					  gpointer data)
{
	GList *l;
	GaimConversation *conv;
	GaimGtkConversation *gtkconv;

	for (l = gaim_get_conversations(); l != NULL; l = l->next) {
		conv = (GaimConversation *)l->data;

		if (!GAIM_IS_GTK_CONVERSATION(conv))
			continue;

		gtkconv = GAIM_GTK_CONVERSATION(conv);

		if (value)
			gtk_widget_show(gtkconv->close);
		else
			gtk_widget_hide(gtkconv->close);
	}
}

static void
show_timestamps_pref_cb(const char *name, GaimPrefType type, gpointer value,
						gpointer data)
{
	GList *l;
	GaimConversation *conv;
	GaimGtkConversation *gtkconv;

	for (l = gaim_get_conversations(); l != NULL; l = l->next) {
		conv = (GaimConversation *)l->data;

		if (!GAIM_IS_GTK_CONVERSATION(conv))
			continue;

		gtkconv = GAIM_GTK_CONVERSATION(conv);

		gtk_imhtml_show_comments(GTK_IMHTML(gtkconv->imhtml), (gboolean)value);
	}
}

static void
spellcheck_pref_cb(const char *name, GaimPrefType type, gpointer value,
				   gpointer data)
{
#ifdef USE_GTKSPELL
	GList *cl;
	GaimConversation *conv;
	GaimGtkConversation *gtkconv;
	GtkSpell *spell;

	for (cl = gaim_get_conversations(); cl != NULL; cl = cl->next) {
		
		conv = (GaimConversation *)cl->data;

		if (!GAIM_IS_GTK_CONVERSATION(conv))
			continue;

		gtkconv = GAIM_GTK_CONVERSATION(conv);

		if (value)
			gtkspell_new_attach(GTK_TEXT_VIEW(gtkconv->entry), NULL, NULL);
		else {
			spell = gtkspell_get_from_text_view(GTK_TEXT_VIEW(gtkconv->entry));
			gtkspell_detach(spell);
		}
	}
#endif
}

static void
show_smileys_pref_cb(const char *name, GaimPrefType type, gpointer value,
					 gpointer data)
{
	GList *cl;
	GaimConversation *conv;
	GaimGtkConversation *gtkconv;

	for (cl = gaim_get_conversations(); cl != NULL; cl = cl->next) {
		conv = (GaimConversation *)cl->data;

		if (!GAIM_IS_GTK_CONVERSATION(conv))
			continue;

		gtkconv = GAIM_GTK_CONVERSATION(conv);

		gtk_imhtml_show_smileys(GTK_IMHTML(gtkconv->imhtml), (gboolean)value);
	}
}

static void
tab_side_pref_cb(const char *name, GaimPrefType type, gpointer value,
				 gpointer data)
{
	GList *l;
	GtkPositionType pos;
	GaimWindow *win;
	GaimGtkWindow *gtkwin;

	pos = GPOINTER_TO_INT(value);

	for (l = gaim_get_windows(); l != NULL; l = l->next) {
		win = (GaimWindow *)l->data;

		if (!GAIM_IS_GTK_WINDOW(win))
			continue;

		gtkwin = GAIM_GTK_WINDOW(win);

		gtk_notebook_set_tab_pos(GTK_NOTEBOOK(gtkwin->notebook), pos);
	}
}

static void
im_button_type_pref_cb(const char *name, GaimPrefType type,
					   gpointer value, gpointer data)
{
	GList *l;
	GaimConversation *conv;
	GaimGtkConversation *gtkconv;

	for (l = gaim_get_ims(); l != NULL; l = l->next) {
		conv = (GaimConversation *)l->data;
		gtkconv = GAIM_GTK_CONVERSATION(conv);

		setup_im_buttons(conv, gtk_widget_get_parent(gtkconv->send));
	}
}

static void
animate_buddy_icons_pref_cb(const char *name, GaimPrefType type,
							gpointer value, gpointer data)
{
	GList *l;

	if (!gaim_prefs_get_bool("/gaim/gtk/conversations/im/show_buddy_icons"))
		return;

	if (value) {
		for (l = gaim_get_ims(); l != NULL; l = l->next)
			start_anim(NULL, (GaimConversation *)l->data);
	}
	else {
		for (l = gaim_get_ims(); l != NULL; l = l->next)
			stop_anim(NULL, (GaimConversation *)l->data);
	}
}

static void
show_buddy_icons_pref_cb(const char *name, GaimPrefType type, gpointer value,
						 gpointer data)
{
	gaim_conversation_foreach(gaim_gtkconv_update_buddy_icon);
}

static void
chat_button_type_pref_cb(const char *name, GaimPrefType type, gpointer value,
						 gpointer data)
{
	GList *l;
	GaimConnection *g;
	GtkWidget *parent;
	GaimConversationType conv_type = GAIM_CONV_CHAT;
	GSList *bcs;
	GaimConversation *conv;
	GaimGtkConversation *gtkconv;
	GaimGtkWindow *gtkwin;

	for (l = gaim_connections_get_all(); l != NULL; l = l->next) {

		g = (GaimConnection *)l->data;

		for (bcs = g->buddy_chats; bcs != NULL; bcs = bcs->next) {
			conv = (GaimConversation *)bcs->data;

			if (gaim_conversation_get_type(conv) != GAIM_CONV_CHAT)
				continue;
			
			if (!GAIM_IS_GTK_CONVERSATION(conv))
				continue;

			gtkconv = GAIM_GTK_CONVERSATION(conv);
			gtkwin  = GAIM_GTK_WINDOW(gaim_conversation_get_window(conv));
			parent  = gtk_widget_get_parent(gtkconv->send);

			gtkconv->send =
				gaim_gtk_change_text(_("Send"),
									 gtkconv->send, GAIM_STOCK_SEND, conv_type);
			gtkconv->u.chat->invite =
				gaim_gtk_change_text(_("Invite"),
									 gtkconv->u.chat->invite,
									 GAIM_STOCK_INVITE, conv_type);

			gtk_box_pack_end(GTK_BOX(parent), gtkconv->send, FALSE, FALSE,
							 conv_type);
			gtk_box_pack_end(GTK_BOX(parent), gtkconv->u.chat->invite,
							 FALSE, FALSE, 0);

			gtk_box_reorder_child(GTK_BOX(parent), gtkconv->send, 0);

			g_signal_connect(G_OBJECT(gtkconv->send), "clicked",
							 G_CALLBACK(send_cb), conv);
			g_signal_connect(G_OBJECT(gtkconv->u.chat->invite), "clicked",
							 G_CALLBACK(invite_cb), conv);

			gtk_button_set_relief(GTK_BUTTON(gtkconv->send),
								  GTK_RELIEF_NONE);
			gtk_button_set_relief(GTK_BUTTON(gtkconv->u.chat->invite),
								  GTK_RELIEF_NONE);

			gaim_gtkconv_update_buttons_by_protocol(conv);
		}
	}
}

void
gaim_gtk_conversation_init(void)
{
	/* Conversations */
	gaim_prefs_add_none("/gaim/gtk/conversations");
	gaim_prefs_add_bool("/gaim/gtk/conversations/close_on_tabs", TRUE);
	gaim_prefs_add_bool("/gaim/gtk/conversations/ctrl_enter_sends", FALSE);
	gaim_prefs_add_bool("/gaim/gtk/conversations/enter_sends", TRUE);
	gaim_prefs_add_bool("/gaim/gtk/conversations/escape_closes", FALSE);
	gaim_prefs_add_bool("/gaim/gtk/conversations/ctrl_w_closes", FALSE);
	gaim_prefs_add_bool("/gaim/gtk/conversations/f2_toggles_timestamps", TRUE);
	gaim_prefs_add_bool("/gaim/gtk/conversations/send_bold", FALSE);
	gaim_prefs_add_bool("/gaim/gtk/conversations/send_italic", FALSE);
	gaim_prefs_add_bool("/gaim/gtk/conversations/send_strikethrough", FALSE);
	gaim_prefs_add_bool("/gaim/gtk/conversations/send_underline", FALSE);
	gaim_prefs_add_bool("/gaim/gtk/conversations/show_smileys", TRUE);
	gaim_prefs_add_bool("/gaim/gtk/conversations/show_timestamps", TRUE);
	gaim_prefs_add_bool("/gaim/gtk/conversations/show_urls_as_links", TRUE);
	gaim_prefs_add_bool("/gaim/gtk/conversations/spellcheck", TRUE);
	gaim_prefs_add_bool("/gaim/gtk/conversations/ignore_colors", FALSE);
	gaim_prefs_add_bool("/gaim/gtk/conversations/ignore_fonts", FALSE);
	gaim_prefs_add_bool("/gaim/gtk/conversations/ignore_font_sizes", FALSE);
	gaim_prefs_add_bool("/gaim/gtk/conversations/use_custom_bgcolor", FALSE);
	gaim_prefs_add_bool("/gaim/gtk/conversations/use_custom_fgcolor", FALSE);
	gaim_prefs_add_bool("/gaim/gtk/conversations/use_custom_font", FALSE);
	gaim_prefs_add_bool("/gaim/gtk/conversations/use_custom_size", FALSE);
	gaim_prefs_add_bool("/gaim/gtk/conversations/html_shortcuts", FALSE);
	gaim_prefs_add_bool("/gaim/gtk/conversations/smiley_shortcuts", FALSE);
	gaim_prefs_add_string("/gaim/gtk/conversations/bgcolor", "#FFFFFF");
	gaim_prefs_add_string("/gaim/gtk/conversations/fgcolor", "#000000");
	gaim_prefs_add_string("/gaim/gtk/conversations/font_face", "");
	gaim_prefs_add_int("/gaim/gtk/conversations/font_size", 3);
	gaim_prefs_add_bool("/gaim/gtk/conversations/tabs", TRUE);
	gaim_prefs_add_int("/gaim/gtk/conversations/tab_side", GTK_POS_TOP);
	gaim_prefs_add_string("/gaim/gtk/conversations/placement", "");

	/* Conversations -> Chat */
	gaim_prefs_add_none("/gaim/gtk/conversations/chat");
	gaim_prefs_add_int("/gaim/gtk/conversations/chat/button_type",
					   GAIM_BUTTON_TEXT_IMAGE);
	gaim_prefs_add_bool("/gaim/gtk/conversations/chat/color_nicks", TRUE);
	gaim_prefs_add_bool("/gaim/gtk/conversations/chat/old_tab_complete", FALSE);
	gaim_prefs_add_bool("/gaim/gtk/conversations/chat/raise_on_events", FALSE);
	gaim_prefs_add_bool("/gaim/gtk/conversations/chat/tab_completion", TRUE);
	gaim_prefs_add_int("/gaim/gtk/conversations/chat/default_width", 410);
	gaim_prefs_add_int("/gaim/gtk/conversations/chat/default_height", 160);
	gaim_prefs_add_int("/gaim/gtk/conversations/chat/entry_height", 50);

	/* Conversations -> IM */
	gaim_prefs_add_none("/gaim/gtk/conversations/im");
	gaim_prefs_add_int("/gaim/gtk/conversations/im/button_type",
					   GAIM_BUTTON_TEXT_IMAGE);
	gaim_prefs_add_bool("/gaim/gtk/conversations/im/animate_buddy_icons", TRUE);
	gaim_prefs_add_bool("/gaim/gtk/conversations/im/hide_on_send", FALSE);
	gaim_prefs_add_bool("/gaim/gtk/conversations/im/raise_on_events", FALSE);
	gaim_prefs_add_bool("/gaim/gtk/conversations/im/show_buddy_icons", TRUE);
	gaim_prefs_add_int("/gaim/gtk/conversations/im/default_width", 410);
	gaim_prefs_add_int("/gaim/gtk/conversations/im/default_height", 160);
	gaim_prefs_add_int("/gaim/gtk/conversations/im/entry_height", 50);

	/* Connect callbacks. */
	gaim_prefs_connect_callback("/gaim/gtk/conversations/close_on_tabs",
								close_on_tabs_pref_cb, NULL);
	gaim_prefs_connect_callback("/gaim/gtk/conversations/show_smileys",
								show_smileys_pref_cb, NULL);
	gaim_prefs_connect_callback("/gaim/gtk/conversations/show_timestamps",
								show_timestamps_pref_cb, NULL);
	gaim_prefs_connect_callback("/gaim/gtk/conversations/spellcheck",
								spellcheck_pref_cb, NULL);
	gaim_prefs_connect_callback("/gaim/gtk/conversations/tab_side",
								tab_side_pref_cb, NULL);

	
	/* IM callbacks */
	gaim_prefs_connect_callback("/gaim/gtk/conversations/im/button_type",
								im_button_type_pref_cb, NULL);
	gaim_prefs_connect_callback("/gaim/gtk/conversations/im/animate_buddy_icons",
								animate_buddy_icons_pref_cb, NULL);
	gaim_prefs_connect_callback("/gaim/gtk/conversations/im/show_buddy_icons",
								show_buddy_icons_pref_cb, NULL);


	/* Chat callbacks */
	gaim_prefs_connect_callback("/gaim/gtk/conversations/chat/button_type",
								chat_button_type_pref_cb, NULL);
}

