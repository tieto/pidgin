/**
 * @file gtkstyle.h GTK+ Style utility functions
 * @ingroup pidgin
 */

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
#ifndef _PIDGINSTYLE_H_
#define _PIDGINSTYLE_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

/*@{*/

/**
 * Returns TRUE if dark mode is enabled and foreground colours should be invertred
 *
 * @param style The GtkStyle in use, or NULL to use a cached version.
 *
 * @return @c TRUE if dark mode, @c FALSE otherwise
 */

gboolean pidgin_style_is_dark(GtkStyle *style);

/**
 * Lighten a color if dark mode is enabled.
 *
 * @param style The GtkStyle in use.
 *
 * @param color Color to be lightened. Transformed color will be written here.
 */

void pidgin_style_adjust_contrast(GtkStyle *style, GdkColor *color);

/*@}*/

G_END_DECLS

#endif /* _PIDGINSTYLE_H_ */
