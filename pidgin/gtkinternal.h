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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#ifndef _PIDGIN_INTERNAL_H_
#define _PIDGIN_INTERNAL_H_
/*
 * SECTION:gtkinternal
 * @section_id: pidgin-gtkinternal
 * @short_description: <filename>gtkinternal.h</filename>
 * @title: Internal Definitions and Includes
 */

G_BEGIN_DECLS

static inline void
_pidgin_widget_set_accessible_name(GtkWidget *widget, const gchar *name);

PurpleImage *
_pidgin_e2ee_stock_icon_get(const gchar *stock_name);

G_END_DECLS

static inline void
_pidgin_widget_set_accessible_name(GtkWidget *widget, const gchar *name)
{
	atk_object_set_name(gtk_widget_get_accessible(widget), name);
}

#endif /* _PIDGIN_INTERNAL_H_ */
