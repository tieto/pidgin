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

#if !GTK_CHECK_VERSION(3,22,0)

static inline void
gtk_menu_popup_at_pointer(GtkMenu *menu, const GdkEvent *trigger_event)
{
	const GdkEventButton *event = (const GdkEventButton *)trigger_event;
	gtk_menu_popup(menu, NULL, NULL, NULL, NULL,
	               event ? event->button : 0, gdk_event_get_time(event));
}

#if !GTK_CHECK_VERSION(3,16,0)

static inline void
gtk_label_set_xalign(GtkLabel *label, gfloat xalign)
{
	g_object_set(label, "xalign", xalign, NULL);
}

static inline void
gtk_label_set_yalign(GtkLabel *label, gfloat yalign)
{
	g_object_set(label, "yalign", yalign, NULL);
}

#endif /* 3.16.0 */

#endif /* 3.22.0 */

#endif /* _PIDGINGTK3COMPAT_H_ */

