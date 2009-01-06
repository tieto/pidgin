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
 * 1. Buddylist is aligned on the left.
 * 2. The rest of the screen is split into MxN grid for conversation windows.
 * 	- M = split-h in ~/.gntrc:[irssi]
 * 	- N = split-v in ~/.gntrc:[irssi]
 *	- Press alt-shift-k/j/l/h to move the selected window to the frame
 *	  above/below/left/right of the current frame.
 * 3. All the other windows are always centered.
 */
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "gnt.h"
#include "gntbox.h"
#include "gntmenu.h"
#include "gntstyle.h"
#include "gntwm.h"
#include "gntwindow.h"
#include "gntlabel.h"

#define TYPE_IRSSI				(irssi_get_gtype())

typedef struct _Irssi
{
	GntWM inherit;
	int vert;
	int horiz;

	/* This is changed whenever the buddylist is opened/closed or resized. */
	int buddylistwidth;
} Irssi;

typedef struct _IrssiClass
{
	GntWMClass inherit;
} IrssiClass;

GType irssi_get_gtype(void);
void gntwm_init(GntWM **wm);

static void (*org_new_window)(GntWM *wm, GntWidget *win);

static void
get_xywh_for_frame(Irssi *irssi, int hor, int vert, int *x, int *y, int *w, int *h)
{
	int width, height, rx, ry;

	width = (getmaxx(stdscr) - irssi->buddylistwidth) / irssi->horiz;
	height = (getmaxy(stdscr) - 1) / irssi->vert;

	if (width) {
		rx = irssi->buddylistwidth;
	} else {
		rx = 0;
		width = getmaxx(stdscr) / irssi->horiz;
	}
	if (hor)
		rx += hor * width;
	if (rx)
		rx++;

	ry = 0;
	if (vert)
		ry += vert * height + 1;

	if (x) *x = rx;
	if (y) *y = ry;
	if (w) {
		*w = (hor == irssi->horiz - 1) ? (getmaxx(stdscr) - rx) : (width - 1);
	}
	if (h) {
		*h = (vert == irssi->vert - 1) ? (getmaxy(stdscr) - 1 - ry) : (height - !!vert);
	}
}

static void
draw_line_separators(Irssi *irssi)
{
	int x, y;
	int width, height;

	wclear(stdscr);
	/* Draw the separator for the buddylist */
	if (irssi->buddylistwidth)
		mvwvline(stdscr, 0, irssi->buddylistwidth,
				ACS_VLINE | COLOR_PAIR(GNT_COLOR_NORMAL), getmaxy(stdscr) - 1);

	/* Now the separators for the conversation windows */
	width = (getmaxx(stdscr) - irssi->buddylistwidth) / irssi->horiz;
	height = (getmaxy(stdscr) - 1) / irssi->vert;
	for (x = 1; x < irssi->horiz; x++) {
		mvwvline(stdscr, 0, irssi->buddylistwidth + x * width,
				ACS_VLINE | COLOR_PAIR(GNT_COLOR_NORMAL), getmaxy(stdscr) - 1);
	}

	for (y = 1; y < irssi->vert; y++) {
		mvwhline(stdscr, y * height, irssi->buddylistwidth + 1, ACS_HLINE | COLOR_PAIR(GNT_COLOR_NORMAL),
				getmaxx(stdscr) - irssi->buddylistwidth);
		for (x = 1; x < irssi->horiz; x++) {
			mvwaddch(stdscr, y * height, x * width + irssi->buddylistwidth, ACS_PLUS | COLOR_PAIR(GNT_COLOR_NORMAL));
		}
		if (irssi->buddylistwidth)
			mvwaddch(stdscr, y * height, irssi->buddylistwidth, ACS_LTEE | COLOR_PAIR(GNT_COLOR_NORMAL));
	}
}

static gboolean
is_budddylist(GntWidget *win)
{
	const char *name = gnt_widget_get_name(win);
	if (name && strcmp(name, "buddylist") == 0)
		return TRUE;
	return FALSE;
}

static void
remove_border_set_position_size(GntWM *wm, GntWidget *win, int x, int y, int w, int h)
{
	gnt_box_set_toplevel(GNT_BOX(win), FALSE);
	GNT_WIDGET_SET_FLAGS(win, GNT_WIDGET_CAN_TAKE_FOCUS);

	gnt_widget_set_position(win, x, y);
	mvwin(win->window, y, x);
	gnt_widget_set_size(win, (w < 0) ? -1 : w + 2, h + 2);
}

static void
irssi_new_window(GntWM *wm, GntWidget *win)
{
	const char *name;
	int x, y, w, h;

	name = gnt_widget_get_name(win);
	if (!name || !strstr(name, "conversation-window")) {
		if (!GNT_IS_MENU(win) && !GNT_WIDGET_IS_FLAG_SET(win, GNT_WIDGET_TRANSIENT)) {
			if ((!name || strcmp(name, "buddylist"))) {
				gnt_widget_get_size(win, &w, &h);
				x = (getmaxx(stdscr) - w) / 2;
				y = (getmaxy(stdscr) - h) / 2;
				gnt_widget_set_position(win, x, y);
				mvwin(win->window, y, x);
			} else {
				gnt_window_set_maximize(GNT_WINDOW(win), GNT_WINDOW_MAXIMIZE_Y);
				remove_border_set_position_size(wm, win, 0, 0, -1, getmaxy(stdscr) - 1);
				gnt_widget_get_size(win, &((Irssi*)wm)->buddylistwidth, NULL);
				draw_line_separators((Irssi*)wm);
			}
		}
		org_new_window(wm, win);
		return;
	}

	/* The window we have here is a conversation window. */

	/* XXX: There should be some way to remember which frame a conversation window
	 * was in the last time. Perhaps save them in some ~/.gntpositionirssi or some
	 * such. */
	get_xywh_for_frame((Irssi*)wm, 0, 0, &x, &y, &w, &h);
	remove_border_set_position_size(wm, win, x, y, w, h);
	org_new_window(wm, win);
}

static void
irssi_window_resized(GntWM *wm, GntNode *node)
{
	if (!is_budddylist(node->me))
		return;

	gnt_widget_get_size(node->me, &((Irssi*)wm)->buddylistwidth, NULL);
	draw_line_separators((Irssi*)wm);
}

static gboolean
irssi_close_window(GntWM *wm, GntWidget *win)
{
	if (is_budddylist(win))
		((Irssi*)wm)->buddylistwidth = 0;
	return FALSE;
}

static gboolean
update_conv_window_title(GntNode *node)
{
	char title[256];
	snprintf(title, sizeof(title), "%d: %s",
			GPOINTER_TO_INT(g_object_get_data(G_OBJECT(node->me), "irssi-index")) + 1,
			GNT_BOX(node->me)->title);
	wbkgdset(node->window, '\0' | COLOR_PAIR(gnt_widget_has_focus(node->me) ? GNT_COLOR_TITLE : GNT_COLOR_TITLE_D));
	mvwaddstr(node->window, 0, 0, title);
	if (!gnt_is_refugee()) {
		update_panels();
		doupdate();
	}
	return FALSE;
}

static void
irssi_update_window(GntWM *wm, GntNode *node)
{
	GntWidget *win = node->me;
	const char *name = gnt_widget_get_name(win);
	if (!name || !GNT_IS_BOX(win) || !strstr(name, "conversation-window"))
		return;
	g_object_set_data(G_OBJECT(win), "irssi-index", GINT_TO_POINTER(g_list_index(wm->cws->list, win)));
	g_timeout_add(0, (GSourceFunc)update_conv_window_title, node);
}

static void
find_window_position(Irssi *irssi, GntWidget *win, int *h, int *v)
{
	int x, y;
	int width, height;

	gnt_widget_get_position(win, &x, &y);
	width = (getmaxx(stdscr) - irssi->buddylistwidth) / irssi->horiz;
	height = (getmaxy(stdscr) - 1) / irssi->vert;

	if (h)
		*h = width ? (x - irssi->buddylistwidth) / width : x / (getmaxx(stdscr) / irssi->horiz);
	if (v)
		*v = y / height;
}

static gboolean
move_direction(GntBindable *bindable, GList *list)
{
	GntWM *wm = GNT_WM(bindable);
	Irssi *irssi = (Irssi*)wm;
	int vert, hor;
	int x, y, w, h;
	GntWidget *win;

	if (wm->cws->ordered == NULL || is_budddylist(win = GNT_WIDGET(wm->cws->ordered->data)))
		return FALSE;

	find_window_position(irssi, win, &hor, &vert);

	switch (GPOINTER_TO_INT(list->data)) {
		case 'k':
			vert = MAX(0, vert - 1);
			break;
		case 'j':
			vert = MIN(vert + 1, irssi->vert - 1);
			break;
		case 'l':
			hor = MIN(hor + 1, irssi->horiz - 1);
			break;
		case 'h':
			hor = MAX(0, hor - 1);
			break;
	}
	get_xywh_for_frame(irssi, hor, vert, &x, &y, &w, &h);
	gnt_wm_move_window(wm, win, x, y);
	gnt_wm_resize_window(wm, win, w, h);
	return TRUE;
}

static void
refresh_window(GntWidget *widget, GntNode *node, Irssi *irssi)
{
	int vert, hor;
	int x, y, w, h;
	const char *name;

	if (!GNT_IS_WINDOW(widget))
		return;

	if (is_budddylist(widget)) {
		return;
	}

	name = gnt_widget_get_name(widget);
	if (name && strstr(name, "conversation-window")) {
		find_window_position(irssi, widget, &hor, &vert);
		get_xywh_for_frame(irssi, hor, vert, &x, &y, &w, &h);
		gnt_wm_move_window(GNT_WM(irssi), widget, x, y);
		gnt_wm_resize_window(GNT_WM(irssi), widget, w, h);
	}
}

static void
irssi_terminal_refresh(GntWM *wm)
{
	draw_line_separators((Irssi*)wm);
	g_hash_table_foreach(wm->nodes, (GHFunc)refresh_window, wm);
}

static void
irssi_class_init(IrssiClass *klass)
{
	GntWMClass *pclass = GNT_WM_CLASS(klass);

	org_new_window = pclass->new_window;

	pclass->new_window = irssi_new_window;
	pclass->window_resized = irssi_window_resized;
	pclass->close_window = irssi_close_window;
	pclass->window_update = irssi_update_window;
	pclass->terminal_refresh = irssi_terminal_refresh;

	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "move-up", move_direction,
			"\033" "K", GINT_TO_POINTER('k'), NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "move-down", move_direction,
			"\033" "J", GINT_TO_POINTER('j'), NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "move-right", move_direction,
			"\033" "L", GINT_TO_POINTER('l'), NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "move-left", move_direction,
			"\033" "H", GINT_TO_POINTER('h'), NULL);

	gnt_style_read_actions(G_OBJECT_CLASS_TYPE(klass), GNT_BINDABLE_CLASS(klass));
	GNTDEBUG;
}

void gntwm_init(GntWM **wm)
{
	char *style = NULL;
	Irssi *irssi;

	irssi = g_object_new(TYPE_IRSSI, NULL);
	*wm = GNT_WM(irssi);

	style = gnt_style_get_from_name("irssi", "split-v");
	irssi->vert = style ? atoi(style) : 1;
	g_free(style);

	style = gnt_style_get_from_name("irssi", "split-h");
	irssi->horiz = style ? atoi(style) : 1;
	g_free(style);

	irssi->vert = MAX(irssi->vert, 1);
	irssi->horiz = MAX(irssi->horiz, 1);

	irssi->buddylistwidth = 0;
}

GType irssi_get_gtype(void)
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(IrssiClass),
			NULL,           /* base_init		*/
			NULL,           /* base_finalize	*/
			(GClassInitFunc)irssi_class_init,
			NULL,
			NULL,           /* class_data		*/
			sizeof(Irssi),
			0,              /* n_preallocs		*/
			NULL,	        /* instance_init	*/
			NULL
		};

		type = g_type_register_static(GNT_TYPE_WM,
		                              "GntIrssi",
		                              &info, 0);
	}

	return type;
}

