/* pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#ifndef _PIDGINGTK3COMPAT_H_
#define _PIDGINGTK3COMPAT_H_

/* This file is internal to Pidgin. Do not use!
 * Also, any public API should not depend on this file.
 */

#if !GTK_CHECK_VERSION(3,2,0)

#define GTK_FONT_CHOOSER GTK_FONT_SELECTION_DIALOG
#define gtk_font_chooser_get_font gtk_font_selection_dialog_get_font_name
#define gtk_font_chooser_set_font gtk_font_selection_dialog_set_font_name

static inline GtkWidget * gtk_font_chooser_dialog_new(const gchar *title,
	GtkWindow *parent)
{
	return gtk_font_selection_dialog_new(title);
}

#if !GTK_CHECK_VERSION(3,0,0)

#define gdk_x11_window_get_xid GDK_WINDOW_XWINDOW
#define gtk_widget_get_preferred_size(x,y,z) gtk_widget_size_request(x,z)

#ifdef GDK_WINDOWING_X11
#define GDK_IS_X11_WINDOW(window) TRUE
#endif
#ifdef GDK_WINDOWING_WIN32
#define GDK_IS_WIN32_WINDOW(window) TRUE
#endif
#ifdef GDK_WINDOWING_QUARTZ
#define GDK_IS_QUARTZ_WINDOW(window) TRUE
#endif

#if !GTK_CHECK_VERSION(2,24,0)

#define gdk_x11_set_sm_client_id gdk_set_sm_client_id
#define gdk_window_get_display gdk_drawable_get_display
#define GtkComboBoxText GtkComboBox
#define GTK_COMBO_BOX_TEXT GTK_COMBO_BOX
#define gtk_combo_box_text_new gtk_combo_box_new_text
#define gtk_combo_box_text_new_with_entry gtk_combo_box_entry_new_text
#define gtk_combo_box_text_append_text gtk_combo_box_append_text
#define gtk_combo_box_text_get_active_text gtk_combo_box_get_active_text
#define gtk_combo_box_text_remove gtk_combo_box_remove_text

static inline gint gdk_window_get_width(GdkWindow *x)
{
	gint w;
	gdk_drawable_get_size(GDK_DRAWABLE(x), &w, NULL);
	return w;
}

static inline gint gdk_window_get_height(GdkWindow *x)
{
	gint h;
	gdk_drawable_get_size(GDK_DRAWABLE(x), NULL, &h);
	return h;
}

#if !GTK_CHECK_VERSION(2,22,0)

#define gdk_drag_context_get_actions(x) (x)->action
#define gdk_drag_context_get_suggested_action(x) (x)->suggested_action
#define gtk_text_view_get_vadjustment(x) (x)->vadjustment
#define gtk_font_selection_dialog_get_font_selection(x) (x)->fontsel

#if !GTK_CHECK_VERSION(2,20,0)

#define gtk_widget_get_mapped GTK_WIDGET_MAPPED
#define gtk_widget_set_mapped(x,y) do { \
	if (y) \
		GTK_WIDGET_SET_FLAGS(x, GTK_MAPPED); \
	else \
		GTK_WIDGET_UNSET_FLAGS(x, GTK_MAPPED); \
} while(0)
#define gtk_widget_get_realized GTK_WIDGET_REALIZED
#define gtk_widget_set_realized(x,y) do { \
	if (y) \
		GTK_WIDGET_SET_FLAGS(x, GTK_REALIZED); \
	else \
		GTK_WIDGET_UNSET_FLAGS(x, GTK_REALIZED); \
} while(0)

#endif /* 2.20.0 */

#endif /* 2.22.0 */

#endif /* 2.24.0 */

#endif /* 3.0.0 */

#endif /* 3.2.0 */

#endif /* _PIDGINGTK3COMPAT_H_ */

