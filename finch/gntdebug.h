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

#ifndef _GNT_DEBUG_H
#define _GNT_DEBUG_H
/**
 * SECTION:gntdebug
 * @section_id: finch-gntdebug
 * @short_description: <filename>gntdebug.h</filename>
 * @title: Debug API
 */

#include "debug.h"

G_BEGIN_DECLS

/**********************************************************************
 * GNT Debug API
 **********************************************************************/

#define FINCH_TYPE_DEBUG_UI (finch_debug_ui_get_type())
#if GLIB_CHECK_VERSION(2,44,0)
G_DECLARE_FINAL_TYPE(FinchDebugUi, finch_debug_ui, FINCH, DEBUG_UI, GObject)
#else
GType finch_debug_ui_get_type(void);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
typedef struct _FinchDebugUi FinchDebugUi;
typedef struct { GObjectClass parent_class; } FinchDebugUiClass;
static inline FinchDebugUi *
FINCH_DEBUG_UI(gpointer ptr)
{
	return G_TYPE_CHECK_INSTANCE_CAST(ptr, finch_debug_ui_get_type(), FinchDebugUi);
}
static inline gboolean
FINCH_IS_DEBUG_UI(gpointer ptr)
{
	return G_TYPE_CHECK_INSTANCE_TYPE(ptr, finch_debug_ui_get_type());
}
G_GNUC_END_IGNORE_DEPRECATIONS
#endif

/**
 * finch_debug_ui_new:
 *
 * Perform necessary initializations.
 */
FinchDebugUi *finch_debug_ui_new(void);

/**
 * finch_debug_window_show:
 *
 * Show the debug window.
 */
void finch_debug_window_show(void);

G_END_DECLS

#endif
