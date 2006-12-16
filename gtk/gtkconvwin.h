/**
 * @file gtkconvwin.h GTK+ Conversation Window API
 * @ingroup gtkui
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
#ifndef _GAIM_GTKCONVERSATION_WINDOW_H_
#define _GAIM_GTKCONVERSATION_WINDOW_H_

typedef struct _GaimGtkWindow       GaimGtkWindow;


/**************************************************************************
 * @name Structures
 **************************************************************************/
/*@{*/

/**
 * A GTK+ representation of a graphical window containing one or more
 * conversations.
 */
struct _GaimGtkWindow
{
	GtkWidget *window;           /**< The window.                      */
	GtkWidget *notebook;         /**< The notebook of conversations.   */
	GList *gtkconvs;

	struct
	{
		GtkWidget *menubar;

		GtkWidget *view_log;

		GtkWidget *send_file;
		GtkWidget *add_pounce;
		GtkWidget *get_info;
		GtkWidget *invite;

		GtkWidget *alias;
		GtkWidget *block;
		GtkWidget *add;
		GtkWidget *remove;

		GtkWidget *insert_link;
		GtkWidget *insert_image;

		GtkWidget *logging;
		GtkWidget *sounds;
		GtkWidget *show_formatting_toolbar;
		GtkWidget *show_timestamps;
		GtkWidget *show_icon;

		GtkWidget *send_to;

		GtkWidget *tray;

		GtkWidget *typing_icon;

		GtkItemFactory *item_factory;

	} menu;

	/* Tab dragging stuff. */
	gboolean in_drag;
	gboolean in_predrag;

	gint drag_tab;

	gint drag_min_x, drag_max_x, drag_min_y, drag_max_y;

	gint drag_motion_signal;
	gint drag_leave_signal;
};

/*@}*/

/**************************************************************************
 * @name GTK+ Conversation Window API
 **************************************************************************/
/*@{*/

GaimGtkWindow * gaim_gtk_conv_window_new(void);
void gaim_gtk_conv_window_destroy(GaimGtkWindow *win);
GList *gaim_gtk_conv_windows_get_list(void);
void gaim_gtk_conv_window_show(GaimGtkWindow *win);
void gaim_gtk_conv_window_hide(GaimGtkWindow *win);
void gaim_gtk_conv_window_raise(GaimGtkWindow *win);
void gaim_gtk_conv_window_switch_gtkconv(GaimGtkWindow *win, GaimGtkConversation *gtkconv);
void gaim_gtk_conv_window_add_gtkconv(GaimGtkWindow *win, GaimGtkConversation *gtkconv);
void gaim_gtk_conv_window_remove_gtkconv(GaimGtkWindow *win, GaimGtkConversation *gtkconv);
GaimGtkConversation *gaim_gtk_conv_window_get_gtkconv_at_index(const GaimGtkWindow *win, int index);
GaimGtkConversation *gaim_gtk_conv_window_get_active_gtkconv(const GaimGtkWindow *win);
GaimConversation *gaim_gtk_conv_window_get_active_conversation(const GaimGtkWindow *win);
gboolean gaim_gtk_conv_window_is_active_conversation(const GaimConversation *conv);
gboolean gaim_gtk_conv_window_has_focus(GaimGtkWindow *win);
GaimGtkWindow *gaim_gtk_conv_window_get_at_xy(int x, int y);
GList *gaim_gtk_conv_window_get_gtkconvs(GaimGtkWindow *win);
guint gaim_gtk_conv_window_get_gtkconv_count(GaimGtkWindow *win);

GaimGtkWindow *gaim_gtk_conv_window_first_with_type(GaimConversationType type);
GaimGtkWindow *gaim_gtk_conv_window_last_with_type(GaimConversationType type);

/*@}*/

/**************************************************************************
 * @name GTK+ Conversation Placement API
 **************************************************************************/
/*@{*/

typedef void (*GaimConvPlacementFunc)(GaimGtkConversation *);

GList *gaim_gtkconv_placement_get_options(void);
void gaim_gtkconv_placement_add_fnc(const char *id, const char *name, GaimConvPlacementFunc fnc);
void gaim_gtkconv_placement_remove_fnc(const char *id);
const char *gaim_gtkconv_placement_get_name(const char *id);
GaimConvPlacementFunc gaim_gtkconv_placement_get_fnc(const char *id);
void gaim_gtkconv_placement_set_current_func(GaimConvPlacementFunc func);
GaimConvPlacementFunc gaim_gtkconv_placement_get_current_func(void);
void gaim_gtkconv_placement_place(GaimGtkConversation *gtkconv);

/*@}*/

#endif /* _GAIM_GTKCONVERSATION_WINDOW_H_ */
