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
/*
 * SECTION:gtk3compat
 * @section_id: pidgin-gtk3compat
 * @short_description: <filename>gtk3compat.h</filename>
 * @title: GTK3 version-dependent definitions
 *
 * This file is internal to Pidgin. Do not use!
 * Also, any public API should not depend on this file.
 */

#include <gtk/gtk.h>
#include <math.h>

#if GTK_CHECK_VERSION(3,16,0)

static inline void
gtk_label_set_alignment(GtkLabel *label, gfloat xalign, gfloat yalign)
{
	gtk_label_set_xalign(label, xalign);
	gtk_label_set_yalign(label, yalign);
}

#else /* 3.16.0 */

static inline void
gtk_label_set_alignment(GtkLabel *label, gfloat xalign, gfloat yalign)
{
	gtk_misc_set_alignment(GTK_MISC(label), xalign, yalign);
}

#endif /* 3.16.0 */


#if !GTK_CHECK_VERSION(3,4,0)

#define gtk_color_chooser_dialog_new(title, parent) \
	gtk_color_selection_dialog_new(title)
#define GTK_COLOR_CHOOSER(widget) (GTK_WIDGET(widget))

static inline void
gtk_color_chooser_set_use_alpha(GtkWidget *widget, gboolean use_alpha)
{
	if (GTK_IS_COLOR_BUTTON(widget)) {
		gtk_color_button_set_use_alpha(GTK_COLOR_BUTTON(widget),
			use_alpha);
	}
}

static inline void
pidgin_color_chooser_set_rgb(GtkWidget *widget, const GdkColor *color)
{
	if (GTK_IS_COLOR_SELECTION_DIALOG(widget)) {
		GtkWidget *colorsel;

		colorsel = gtk_color_selection_dialog_get_color_selection(
			GTK_COLOR_SELECTION_DIALOG(widget));
		gtk_color_selection_set_current_color(
			GTK_COLOR_SELECTION(colorsel), color);
	} else
		gtk_color_button_set_color(GTK_COLOR_BUTTON(widget), color);
}

static inline void
pidgin_color_chooser_get_rgb(GtkWidget *widget, GdkColor *color)
{
	if (GTK_IS_COLOR_SELECTION_DIALOG(widget)) {
		GtkWidget *colorsel;

		colorsel = gtk_color_selection_dialog_get_color_selection(
			GTK_COLOR_SELECTION_DIALOG(widget));
		gtk_color_selection_get_current_color(
			GTK_COLOR_SELECTION(colorsel), color);
	} else
		gtk_color_button_get_color(GTK_COLOR_BUTTON(widget), color);
}

#else /* 3.4.0 */

static inline void
pidgin_color_chooser_set_rgb(GtkColorChooser *chooser, const GdkColor *rgb)
{
	GdkRGBA rgba;

	rgba.red = rgb->red / 65535.0;
	rgba.green = rgb->green / 65535.0;
	rgba.blue = rgb->blue / 65535.0;
	rgba.alpha = 1.0;

	gtk_color_chooser_set_rgba(chooser, &rgba);
}

static inline void
pidgin_color_chooser_get_rgb(GtkColorChooser *chooser, GdkColor *rgb)
{
	GdkRGBA rgba;

	gtk_color_chooser_get_rgba(chooser, &rgba);
	rgb->red = (int)round(rgba.red * 65535.0);
	rgb->green = (int)round(rgba.green * 65535.0);
	rgb->blue = (int)round(rgba.blue * 65535.0);
}

#endif /* 3.4.0 and gtk_color_chooser_ */


#if !GTK_CHECK_VERSION(3,2,0)

#define GTK_FONT_CHOOSER GTK_FONT_SELECTION_DIALOG
#define gtk_font_chooser_get_font gtk_font_selection_dialog_get_font_name
#define gtk_font_chooser_set_font gtk_font_selection_dialog_set_font_name

static inline GtkWidget * gtk_font_chooser_dialog_new(const gchar *title,
	GtkWindow *parent)
{
	return gtk_font_selection_dialog_new(title);
}

#endif /* 3.2.0 */

#endif /* _PIDGINGTK3COMPAT_H_ */

