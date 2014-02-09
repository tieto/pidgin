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

#ifndef _PIDGINDEBUG_H_
#define _PIDGINDEBUG_H_
/**
 * SECTION:gtkdebug
 * @section_id: pidgin-gtkdebug
 * @short_description: <filename>gtkdebug.h</filename>
 * @title: Debug API
 */

#include "debug.h"

G_BEGIN_DECLS

/**
 * pidgin_debug_init:
 *
 * Initializes the GTK+ debug system.
 */
void pidgin_debug_init(void);

/**
 * pidgin_debug_uninit:
 *
 * Uninitialized the GTK+ debug system.
 */
void pidgin_debug_uninit(void);

/**
 * pidgin_debug_get_handle:
 *
 * Get the handle for the GTK+ debug system.
 *
 * Returns: the handle to the debug system
 */
void *pidgin_debug_get_handle(void);

/**
 * pidgin_debug_window_show:
 *
 * Shows the debug window.
 */
void pidgin_debug_window_show(void);

/**
 * pidgin_debug_window_hide:
 *
 * Hides the debug window.
 */
void pidgin_debug_window_hide(void);

/**
 * pidgin_debug_get_ui_ops:
 *
 * Returns the UI operations structure for GTK+ debug output.
 *
 * Returns: The GTK+ UI debug operations structure.
 */
PurpleDebugUiOps *pidgin_debug_get_ui_ops(void);

G_END_DECLS

#endif /* _PIDGINDEBUG_H_ */
