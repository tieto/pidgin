/**
 * @file gtkdebug.h GTK+ Debug API
 * @ingroup gtkui
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _GAIM_GTKDEBUG_H_
#define _GAIM_GTKDEBUG_H_

#include "debug.h"

/**
 * Initializes the GTK+ debug system.
 */
void gaim_gtk_debug_init(void);

/**
 * Uninitialized the GTK+ debug system.
 */
void gaim_gtk_debug_uninit(void);

/**
 * Get the handle for the GTK+ debug system.
 *
 * @return the handle to the debug system
 */
void *gaim_gtk_debug_get_handle(void);

/**
 * Shows the debug window.
 */
void gaim_gtk_debug_window_show(void);

/**
 * Hides the debug window.
 */
void gaim_gtk_debug_window_hide(void);

/**
 * Returns the UI operations structure for GTK+ debug output.
 *
 * @return The GTK+ UI debug operations structure.
 */
GaimDebugUiOps *gaim_gtk_debug_get_ui_ops(void);

#endif /* _GAIM_GTKDEBUG_H_ */
