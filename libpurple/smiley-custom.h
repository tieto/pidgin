/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
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

#ifndef _PURPLE_SMILEY_CUSTOM_H_
#define _PURPLE_SMILEY_CUSTOM_H_
/**
 * SECTION:smiley-custom
 * @include:smiley-custom.h
 * @section_id: libpurple-smiley-custom
 * @short_description: a persistent storage for user-defined smileys
 * @title: Custom smileys storage
 *
 * A custom smiley is a non-standard (not defined by a particular protocol)
 * #PurpleSmiley, defined by user. Some protocols support sending such smileys
 * for other buddies, that do not have such image on their machine. Protocol
 * that supports this feature should set the flag
 * @PURPLE_CONNECTION_FLAG_ALLOW_CUSTOM_SMILEY of #PurpleConnectionFlags.
 */

#include "smiley.h"
#include "smiley-list.h"

/**
 * purple_smiley_custom_add:
 * @image: the smiley's image.
 * @shortcut: textual representation of a smiley.
 *
 * Adds a new smiley to the store. The @shortcut should be unique, but the
 * @image contents don't have to.
 *
 * Returns: a new #PurpleSmiley, or %NULL if error occured.
 */
PurpleSmiley *
purple_smiley_custom_add(PurpleStoredImage *image, const gchar *shortcut);

/**
 * purple_smiley_custom_remove:
 * @smiley: the smiley to be removed.
 *
 * Removes a @smiley from the store. If the @smiley file is unique (not used by
 * other smileys) it will be removed from a disk.
 */
void
purple_smiley_custom_remove(PurpleSmiley *smiley);

/**
 * purple_smiley_custom_get_list:
 *
 * Returns the whole list of user-configured custom smileys.
 *
 * Returns: a #PurpleSmileyList of custom smileys.
 */
PurpleSmileyList *
purple_smiley_custom_get_list(void);

/**
 * _purple_smiley_custom_init: (skip)
 *
 * Initializes the custom smileys storage subsystem.
 * Stability: Private
 */
void
_purple_smiley_custom_init(void);

/**
 * _purple_smiley_custom_uninit: (skip)
 *
 * Uninitializes the custom smileys storage subsystem.
 */
void
_purple_smiley_custom_uninit(void);

#endif /* _PURPLE_SMILEY_CUSTOM_H_ */
