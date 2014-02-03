/*
 * GNT - The GLib Ncurses Toolkit
 *
 * GNT is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This library is free software; you can redistribute it and/or modify
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
 * SECTION:gntws
 * @section_id: libgnt-gntws
 * @short_description: <filename>gntws.h</filename>
 * @title: Workspace API
 */

#ifndef GNTWS_H
#define GNTWS_H

#include "gntwidget.h"

#include <panel.h>

#define GNT_TYPE_WS				(gnt_ws_get_gtype())
#define GNT_WS(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_WS, GntWS))
#define GNT_IS_WS(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_WS))
#define GNT_IS_WS_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_WS))
#define GNT_WS_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_WS, GntWSClass))

typedef struct _GntWS GntWS;

struct _GntWS
{
	GntBindable inherit;
	char *name;
	GList *list;
	GList *ordered;
	gpointer ui_data;

	void *res1;
	void *res2;
	void *res3;
	void *res4;
};

typedef struct _GntWSClass GntWSClass;

struct _GntWSClass
{
	GntBindableClass parent;

	void (*draw_taskbar)(GntWS *ws, gboolean );

	void (*res1)(void);
	void (*res2)(void);
	void (*res3)(void);
	void (*res4)(void);
};

G_BEGIN_DECLS

/**
 * gnt_ws_get_gtype:
 *
 * Returns: The GType for GntWS.
 *
 * Since: 2.0.0 (gnt), 2.1.0 (pidgin)
 */
GType gnt_ws_get_gtype(void);

/**
 * gnt_ws_new:
 * @name:  The desired name of the workspace, or %NULL.
 *
 * Create a new workspace with the specified name.
 *
 * Returns: The newly created workspace.
 *
 * Since: 2.0.0 (gnt), 2.1.0 (pidgin)
 */
GntWS *gnt_ws_new(const char *name);

/**
 * gnt_ws_set_name:
 * @ws:    The workspace to rename.
 * @name:  The new name of the workspace.
 *
 * Set the name of a workspace.
 *
 * Since: 2.0.0 (gnt), 2.1.0 (pidgin)
 */
void gnt_ws_set_name(GntWS *ws, const gchar *name);

/**
 * gnt_ws_add_widget:
 * @ws:     The workspace.
 * @widget: The widget to add.
 *
 * Add a widget to a workspace.
 *
 * Since: 2.0.0 (gnt), 2.1.0 (pidgin)
 */
void gnt_ws_add_widget(GntWS *ws, GntWidget *widget);

/**
 * gnt_ws_remove_widget:
 * @ws:      The workspace
 * @widget:  The widget to remove from the workspace.
 *
 * Remove a widget from a workspace.
 *
 * Since: 2.0.0 (gnt), 2.1.0 (pidgin)
 */
void gnt_ws_remove_widget(GntWS *ws, GntWidget *widget);

/**
 * gnt_ws_widget_hide:
 * @widget:  The widget to hide.
 * @nodes:   A hashtable containing information about the widgets.
 *
 * Hide a widget in a workspace.
 *
 * Since: 2.0.0 (gnt), 2.1.0 (pidgin)
 */
void gnt_ws_widget_hide(GntWidget *widget, GHashTable *nodes);

/**
 * gnt_ws_widget_show:
 * @widget:   The widget to show.
 * @nodes:   A hashtable containing information about the widgets.
 *
 * Show a widget in a workspace.
 *
 * Since: 2.0.0 (gnt), 2.1.0 (pidgin)
 */
void gnt_ws_widget_show(GntWidget *widget, GHashTable *nodes);

/**
 * gnt_ws_draw_taskbar:
 * @ws:         The workspace.
 * @reposition: Whether the workspace should reposition the taskbar.
 *
 * Draw the taskbar in a workspace.
 *
 * Since: 2.0.0 (gnt), 2.1.0 (pidgin)
 */
void gnt_ws_draw_taskbar(GntWS *ws, gboolean reposition);

/**
 * gnt_ws_hide:
 * @ws:      The workspace to hide.
 * @table:   A hashtable containing information about the widgets.
 *
 * Hide a workspace.
 *
 * Since: 2.0.0 (gnt), 2.1.0 (pidgin)
 */
void gnt_ws_hide(GntWS *ws, GHashTable *table);

/**
 * gnt_ws_show:
 * @ws:      The workspace to hide.
 * @table:   A hashtable containing information about the widgets.
 *
 * Show a workspace.
 *
 * Since: 2.0.0 (gnt), 2.1.0 (pidgin)
 */
void gnt_ws_show(GntWS *ws, GHashTable *table);

/**
 * gnt_ws_get_name:
 * @ws:   The workspace.
 *
 * Get the name of a workspace.
 *
 * Returns:  The name of the workspace (can be %NULL).
 *
 * Since: 2.0.0 (gnt), 2.1.0 (pidgin)
 */
const char * gnt_ws_get_name(GntWS *ws);

#endif
