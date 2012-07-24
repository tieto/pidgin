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

#if !GTK_CHECK_VERSION(3,0,0)

#define gdk_x11_window_get_xid GDK_WINDOW_XWINDOW

#if !GTK_CHECK_VERSION(2,24,0)

#define gdk_x11_set_sm_client_id gdk_set_sm_client_id
#define gdk_window_get_display gdk_drawable_get_display
#define GtkComboBoxText GtkComboBox
#define GTK_COMBO_BOX_TEXT GTK_COMBO_BOX
#define gtk_combo_box_text_new gtk_combo_box_new_text
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

#if !GTK_CHECK_VERSION(2,18,0)

#define gtk_widget_get_state GTK_WIDGET_STATE
#define gtk_widget_is_drawable GTK_WIDGET_DRAWABLE
#define gtk_widget_get_visible GTK_WIDGET_VISIBLE
#define gtk_widget_has_focus GTK_WIDGET_HAS_FOCUS
#define gtk_widget_is_sensitive GTK_WIDGET_IS_SENSITIVE
#define gtk_widget_get_has_window(x) !GTK_WIDGET_NO_WINDOW(x)
#define gtk_widget_set_has_window(x,y) do { \
	if (!y) \
		GTK_WIDGET_SET_FLAGS(x, GTK_NO_WINDOW); \
	else \
		GTK_WIDGET_UNSET_FLAGS(x, GTK_NO_WINDOW); \
} while(0)
#define gtk_widget_get_allocation(x,y) *(y) = (x)->allocation
#define gtk_widget_set_allocation(x,y) (x)->allocation = *(y)
#define gtk_widget_set_window(x,y) ((x)->window = (y))
#define gtk_widget_set_can_default(w,y) do { \
	if (y) \
		GTK_WIDGET_SET_FLAGS((w), GTK_CAN_DEFAULT); \
	else \
		GTK_WIDGET_UNSET_FLAGS((w), GTK_CAN_DEFAULT); \
} while (0)
#define gtk_widget_set_can_focus(x,y) do {\
	if (y) \
		GTK_WIDGET_SET_FLAGS(x, GTK_CAN_FOCUS); \
	else \
		GTK_WIDGET_UNSET_FLAGS(x, GTK_CAN_FOCUS); \
} while(0)
#define gtk_cell_renderer_set_padding(x,y,z) do { \
	(x)->xpad = (y); \
	(x)->ypad = (z); \
} while (0)
#define gtk_cell_renderer_get_padding(x,y,z) do { \
	*(y) = (x)->xpad; \
	*(z) = (x)->ypad; \
} while (0)
#define gtk_cell_renderer_get_alignment(x,y,z) do { \
	*(y) = (x)->xalign; \
	*(z) = (x)->yalign; \
} while (0)
	
#if !GTK_CHECK_VERSION(2,16,0)

#define gtk_status_icon_set_tooltip_text gtk_status_icon_set_tooltip

#if !GTK_CHECK_VERSION(2,14,0)

#define gtk_widget_get_window(x) (x)->window
#define gtk_widget_set_style(x,y) (x)->style = (y)
#define gtk_selection_data_get_data(x) (x)->data
#define gtk_selection_data_get_length(x) (x)->length
#define gtk_selection_data_get_format(x) (x)->format
#define gtk_selection_data_get_target(x) (x)->target
#define gtk_dialog_get_content_area(x) GTK_DIALOG(x)->vbox
#define gtk_dialog_get_action_area(x) GTK_DIALOG(x)->action_area
#define gtk_adjustment_get_page_size(x) (x)->page_size
#define gtk_adjustment_get_lower(x) (x)->lower
#define gtk_adjustment_get_upper(x) (x)->upper
#define gtk_font_selection_get_size_entry (x)->size_entry
#define gtk_font_selection_get_family_list (x)->family_list
#define gtk_font_selection_dialog_get_ok_button(x) (x)->ok_button
#define gtk_font_selection_dialog_get_cancel_button(x) (x)->cancel_button
#define gtk_color_selection_dialog_get_color_selection(x) (x)->colorsel
#define gtk_menu_item_get_accel_path(x) (x)->accel_path

#if !GTK_CHECK_VERSION(2,12,0)

#ifdef GTK_TOOLTIPS_VAR
#define gtk_widget_set_tooltip_text(w, t) gtk_tooltips_set_tip(GTK_TOOLTIPS_VAR, (w), (t), NULL)
#else
#define gtk_widget_set_tooltip_text(w, t) gtk_tooltips_set_tip(tooltips, (w), (t), NULL)
#endif

#endif /* 2.12.0 */

#endif /* 2.14.0 */

#endif /* 2.16.0 */

#endif /* 2.18.0 */

#endif /* 2.20.0 */

#endif /* 2.22.0 */

#endif /* 2.24.0 */

#endif /* 3.0.0 */

#endif /* _PIDGINGTK3COMPAT_H_ */

