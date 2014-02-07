/* finch
 *
 * Finch is the legal property of its developers, whose names are too numerous
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
/**
 * SECTION:gntdebug
 * @section_id: finch-gntdebug
 * @short_description: <filename>gntdebug.h</filename>
 * @title: Debug API
 */

#ifndef _GNT_DEBUG_H
#define _GNT_DEBUG_H

#include "debug.h"

/**********************************************************************
 * GNT Debug API
 **********************************************************************/

/**
 * finch_debug_get_ui_ops:
 *
 * Get the ui-functions.
 *
 * Returns: The PurpleDebugUiOps structure populated with the appropriate functions.
 */
PurpleDebugUiOps *finch_debug_get_ui_ops(void);

/**
 * finch_debug_init:
 *
 * Perform necessary initializations.
 */
void finch_debug_init(void);

/**
 * finch_debug_uninit:
 *
 * Perform necessary uninitializations.
 */
void finch_debug_uninit(void);

/**
 * finch_debug_window_show:
 *
 * Show the debug window.
 */
void finch_debug_window_show(void);

#endif
