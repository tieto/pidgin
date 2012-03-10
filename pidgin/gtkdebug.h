/**
 * @file gtkdebug.h GTK+ Debug API
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
#ifndef _PIDGINDEBUG_H_
#define _PIDGINDEBUG_H_

#include "debug.h"

G_BEGIN_DECLS

/**
 * Initializes the GTK+ debug system.
 */
void pidgin_debug_init(void);

/**
 * Uninitialized the GTK+ debug system.
 */
void pidgin_debug_uninit(void);

/**
 * Get the handle for the GTK+ debug system.
 *
 * @return the handle to the debug system
 */
void *pidgin_debug_get_handle(void);

/**
 * Shows the debug window.
 */
void pidgin_debug_window_show(void);

/**
 * Hides the debug window.
 */
void pidgin_debug_window_hide(void);

/**
 * Returns the UI operations structure for GTK+ debug output.
 *
 * @return The GTK+ UI debug operations structure.
 */
PurpleDebugUiOps *pidgin_debug_get_ui_ops(void);

G_END_DECLS

#endif /* _PIDGINDEBUG_H_ */
