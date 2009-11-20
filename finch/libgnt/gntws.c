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

#include <gmodule.h>

#include "gntinternal.h"
#include "gntbox.h"
#include "gntwidget.h"
#include "gntwindow.h"
#include "gntwm.h"
#include "gntws.h"

static void
widget_hide(gpointer data, gpointer nodes)
{
	GntWidget *widget = GNT_WIDGET(data);
	GntNode *node = g_hash_table_lookup(nodes, widget);
	if (GNT_IS_WINDOW(widget))
		gnt_window_workspace_hiding(GNT_WINDOW(widget));
	if (node)
		hide_panel(node->panel);
}

static void
widget_show(gpointer data, gpointer nodes)
{
	GntNode *node = g_hash_table_lookup(nodes, data);
	GNT_WIDGET_UNSET_FLAGS(GNT_WIDGET(data), GNT_WIDGET_INVISIBLE);
	if (node) {
		show_panel(node->panel);
		gnt_wm_copy_win(GNT_WIDGET(data), node);
	}
}

void
gnt_ws_draw_taskbar(GntWS *ws, gboolean reposition)
{
	static WINDOW *taskbar = NULL;
	GList *iter;
	int n, width = 0;
	int i;

	if (gnt_is_refugee())
		return;

	if (taskbar == NULL) {
		taskbar = newwin(1, getmaxx(stdscr), getmaxy(stdscr) - 1, 0);
	} else if (reposition) {
		int Y_MAX = getmaxy(stdscr) - 1;
		mvwin(taskbar, Y_MAX, 0);
	}

	wbkgdset(taskbar, '\0' | gnt_color_pair(GNT_COLOR_NORMAL));
	werase(taskbar);

	n = g_list_length(ws->list);
	if (n)
		width = getmaxx(stdscr) / n;

	for (i = 0, iter = ws->list; iter; iter = iter->next, i++) {
		GntWidget *w = iter->data;
		int color;
		const char *title;

		if (w == ws->ordered->data) {
			/* This is the current window in focus */
			color = GNT_COLOR_TITLE;
		} else if (GNT_WIDGET_IS_FLAG_SET(w, GNT_WIDGET_URGENT)) {
			/* This is a window with the URGENT hint set */
			color = GNT_COLOR_URGENT;
		} else {
			color = GNT_COLOR_NORMAL;
		}
		wbkgdset(taskbar, '\0' | gnt_color_pair(color));
		if (iter->next)
			mvwhline(taskbar, 0, width * i, ' ' | gnt_color_pair(color), width);
		else
			mvwhline(taskbar, 0, width * i, ' ' | gnt_color_pair(color), getmaxx(stdscr) - width * i);
		title = GNT_BOX(w)->title;
		mvwprintw(taskbar, 0, width * i, "%s", title ? C_(title) : "<gnt>");
		if (i)
			mvwaddch(taskbar, 0, width *i - 1, ACS_VLINE | A_STANDOUT | gnt_color_pair(GNT_COLOR_NORMAL));
	}
	wrefresh(taskbar);
}

static void
gnt_ws_init(GTypeInstance *instance, gpointer class)
{
	GntWS *ws = GNT_WS(instance);
	ws->list = NULL;
	ws->ordered = NULL;
	ws->name = NULL;
}

void gnt_ws_add_widget(GntWS *ws, GntWidget* wid)
{
	GntWidget *oldfocus;
	oldfocus = ws->ordered ? ws->ordered->data : NULL;
	ws->list = g_list_append(ws->list, wid);
	ws->ordered = g_list_prepend(ws->ordered, wid);
	if (oldfocus)
		gnt_widget_set_focus(oldfocus, FALSE);
}

void gnt_ws_remove_widget(GntWS *ws, GntWidget* wid)
{
	ws->list = g_list_remove(ws->list, wid);
	ws->ordered = g_list_remove(ws->ordered, wid);
}

void
gnt_ws_set_name(GntWS *ws, const gchar *name)
{
	g_free(ws->name);
	ws->name = g_strdup(name);
}

void
gnt_ws_hide(GntWS *ws, GHashTable *nodes)
{
	g_list_foreach(ws->ordered, widget_hide, nodes);
}

void gnt_ws_widget_hide(GntWidget *widget, GHashTable *nodes)
{
	widget_hide(widget, nodes);
}

void gnt_ws_widget_show(GntWidget *widget, GHashTable *nodes)
{
	widget_show(widget, nodes);
}

void
gnt_ws_show(GntWS *ws, GHashTable *nodes)
{
	GList *l;
	for (l = g_list_last(ws->ordered); l; l = g_list_previous(l))
		widget_show(l->data, nodes);
}

GType
gnt_ws_get_gtype(void)
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(GntWSClass),
			NULL,					/* base_init		*/
			NULL,					/* base_finalize	*/
			NULL,
			/*(GClassInitFunc)gnt_ws_class_init,*/
			NULL,
			NULL,					/* class_data		*/
			sizeof(GntWS),
			0,						/* n_preallocs		*/
			gnt_ws_init,			/* instance_init	*/
			NULL					/* value_table		*/
		};

		type = g_type_register_static(GNT_TYPE_BINDABLE,
									  "GntWS",
									  &info, 0);
	}

	return type;
}

GntWS *gnt_ws_new(const char *name)
{
	GntWS *ws = GNT_WS(g_object_new(GNT_TYPE_WS, NULL));
	ws->name = g_strdup(name ? name : "(noname)");
	return ws;
}

const char * gnt_ws_get_name(GntWS *ws)
{
	return ws->name;
}

