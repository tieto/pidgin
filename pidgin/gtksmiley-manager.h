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

#ifndef _PIDGIN_SMILEY_MANAGER_H_
#define _PIDGIN_SMILEY_MANAGER_H_
/**
 * SECTION:gtksmiley-manager
 * @include:gtksmiley-manager.h
 * @section_id: pidgin-smiley-manager
 * @short_description: a UI for user-defined smileys management
 * @title: Custom smileys manager
 *
 * This module provides a GTK+ UI that allows the user adding and removing
 * custom smileys. See libpurple-smiley-custom section (TODO: how to link this
 * to libpurple's docs?).
 */

G_BEGIN_DECLS

/**
 * pidgin_smiley_manager_show:
 *
 * Creates and shows the smiley manager window, or requests focus for it,
 * if it's already opened.
 */
void
pidgin_smiley_manager_show(void);

/**
 * pidgin_smiley_manager_add:
 * @image: the image for a new smiley.
 * @shortcut: the textual representation, may be %NULL.
 *
 * Creates and shows the new dialog for adding a new custom smiley with
 * provided image.
 */
void
pidgin_smiley_manager_add(PurpleImage *image, const gchar *shortcut);

G_END_DECLS

#endif /* _PIDGIN_SMILEY_MANAGER_H_ */
