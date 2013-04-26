/**
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

#include "config.h"

#ifdef USE_PYTHON
#include <Python.h>
#endif

/* Python.h may define _GNU_SOURCE and _XOPEN_SOURCE_EXTENDED, so protect
 * these checks with #ifndef/!defined() */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#if !defined _XOPEN_SOURCE_EXTENDED && (defined(__APPLE__) || defined(__unix__)) && !defined(__FreeBSD__)
#define _XOPEN_SOURCE_EXTENDED
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <ctype.h>
#include <gmodule.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "gntinternal.h"
#undef GNT_LOG_DOMAIN
#define GNT_LOG_DOMAIN "WM"

#include "gntwm.h"
#include "gntstyle.h"
#include "gntmarshal.h"
#include "gnt.h"
#include "gntbox.h"
#include "gntbutton.h"
#include "gntentry.h"
#include "gntfilesel.h"
#include "gntlabel.h"
#include "gntmenu.h"
#include "gnttextview.h"
#include "gnttree.h"
#include "gntutils.h"
#include "gntwindow.h"

#define IDLE_CHECK_INTERVAL 5 /* 5 seconds */

enum
{
	SIG_NEW_WIN,
	SIG_DECORATE_WIN,
	SIG_CLOSE_WIN,
	SIG_CONFIRM_RESIZE,
	SIG_RESIZED,
	SIG_CONFIRM_MOVE,
	SIG_MOVED,
	SIG_UPDATE_WIN,
	SIG_GIVE_FOCUS,
	SIG_KEY_PRESS,
	SIG_MOUSE_CLICK,
	SIG_TERMINAL_REFRESH,
	SIGS
};

static guint signals[SIGS] = { 0 };
static void gnt_wm_new_window_real(GntWM *wm, GntWidget *widget);
static void gnt_wm_win_resized(GntWM *wm, GntNode *node);
static void gnt_wm_win_moved(GntWM *wm, GntNode *node);
static void gnt_wm_give_focus(GntWM *wm, GntWidget *widget);
static void update_window_in_list(GntWM *wm, GntWidget *wid);
static void shift_window(GntWM *wm, GntWidget *widget, int dir);
static gboolean workspace_next(GntBindable *wm, GList *n);
static gboolean workspace_prev(GntBindable *wm, GList *n);

#ifndef NO_WIDECHAR
static int widestringwidth(wchar_t *wide);
#endif

static void ensure_normal_mode(GntWM *wm);
static gboolean write_already(gpointer data);
static int write_timeout;
static time_t last_active_time;
static gboolean idle_update;
static GList *act = NULL; /* list of WS with unseen activitiy */
static gboolean ignore_keys = FALSE;
#ifdef USE_PYTHON
static gboolean started_python = FALSE;
#endif

static GList *
g_list_bring_to_front(GList *list, gpointer data)
{
	list = g_list_remove(list, data);
	list = g_list_prepend(list, data);
	return list;
}

static void
free_node(gpointer data)
{
	GntNode *node = data;
	hide_panel(node->panel);
	del_panel(node->panel);
	g_free(node);
}

void
gnt_wm_copy_win(GntWidget *widget, GntNode *node)
{
	WINDOW *src, *dst;
	if (!node)
		return;
	src = widget->window;
	dst = node->window;
	copywin(src, dst, node->scroll, 0, 0, 0, getmaxy(dst) - 1, getmaxx(dst) - 1, 0);

	/* Update the hardware cursor */
	if (GNT_IS_WINDOW(widget) || GNT_IS_BOX(widget)) {
		GntWidget *active = GNT_BOX(widget)->active;
		if (active) {
			int curx = active->priv.x + getcurx(active->window);
			int cury = active->priv.y + getcury(active->window);
			if (wmove(node->window, cury - widget->priv.y, curx - widget->priv.x) != OK)
				wmove(node->window, 0, 0);
		}
	}
}

/**
 * The following is a workaround for a bug in most versions of ncursesw.
 * Read about it in: http://article.gmane.org/gmane.comp.lib.ncurses.bugs/2751
 *
 * In short, if a panel hides one cell of a multi-cell character, then the rest
 * of the characters in that line get screwed. The workaround here is to erase
 * any such character preemptively.
 *
 * Caveat: If a wide character is erased, and the panel above it is moved enough
 * to expose the entire character, it is not always redrawn.
 */
static void
work_around_for_ncurses_bug(void)
{
#ifndef NO_WIDECHAR
	PANEL *panel = NULL;
	while ((panel = panel_below(panel)) != NULL) {
		int sx, ex, sy, ey, w, y;
		cchar_t ch;
		PANEL *below = panel;

		sx = panel->win->_begx;
		ex = panel->win->_maxx + sx;
		sy = panel->win->_begy;
		ey = panel->win->_maxy + sy;

		while ((below = panel_below(below)) != NULL) {
			if (sy > below->win->_begy + below->win->_maxy ||
					ey < below->win->_begy)
				continue;
			if (sx > below->win->_begx + below->win->_maxx ||
					ex < below->win->_begx)
				continue;
			for (y = MAX(sy, below->win->_begy); y <= MIN(ey, below->win->_begy + below->win->_maxy); y++) {
				if (mvwin_wch(below->win, y - below->win->_begy, sx - 1 - below->win->_begx, &ch) != OK)
					goto right;
				w = widestringwidth(ch.chars);
				if (w > 1 && (ch.attr & 1)) {
					ch.chars[0] = ' ';
					ch.attr &= ~ A_CHARTEXT;
					mvwadd_wch(below->win, y - below->win->_begy, sx - 1 - below->win->_begx, &ch);
					touchline(below->win, y - below->win->_begy, 1);
				}
right:
				if (mvwin_wch(below->win, y - below->win->_begy, ex + 1 - below->win->_begx, &ch) != OK)
					continue;
				w = widestringwidth(ch.chars);
				if (w > 1 && !(ch.attr & 1)) {
					ch.chars[0] = ' ';
					ch.attr &= ~ A_CHARTEXT;
					mvwadd_wch(below->win, y - below->win->_begy, ex + 1 - below->win->_begx, &ch);
					touchline(below->win, y - below->win->_begy, 1);
				}
			}
		}
	}
#endif
}

static void
update_act_msg(void)
{
	GntWidget *label;
	GList *iter;
	static GntWidget *message = NULL;
	GString *text = g_string_new("act: ");
	if (message)
		gnt_widget_destroy(message);
	if (!act)
		return;
	for (iter = act; iter; iter = iter->next) {
		GntWS *ws = iter->data;
		g_string_append_printf(text, "%s, ", gnt_ws_get_name(ws));
	}
	g_string_erase(text, text->len - 2, 2);
	message = gnt_vbox_new(FALSE);
	label = gnt_label_new_with_format(text->str, GNT_TEXT_FLAG_BOLD | GNT_TEXT_FLAG_HIGHLIGHT);
	GNT_WIDGET_UNSET_FLAGS(GNT_BOX(message), GNT_WIDGET_CAN_TAKE_FOCUS);
	GNT_WIDGET_SET_FLAGS(GNT_BOX(message), GNT_WIDGET_TRANSIENT);
	gnt_box_add_widget(GNT_BOX(message), label);
	gnt_widget_set_name(message, "wm-message");
	gnt_widget_set_position(message, 0, 0);
	gnt_widget_draw(message);
	g_string_free(text, TRUE);
}

static gboolean
update_screen(GntWM *wm)
{
	if (wm->mode == GNT_KP_MODE_WAIT_ON_CHILD)
		return TRUE;

	if (wm->menu) {
		GntMenu *top = wm->menu;
		while (top) {
			GntNode *node = g_hash_table_lookup(wm->nodes, top);
			if (node)
				top_panel(node->panel);
			top = top->submenu;
		}
	}
	work_around_for_ncurses_bug();
	update_panels();
	doupdate();
	return TRUE;
}

static gboolean
sanitize_position(GntWidget *widget, int *x, int *y, gboolean m)
{
	int X_MAX = getmaxx(stdscr);
	int Y_MAX = getmaxy(stdscr) - 1;
	int w, h;
	int nx, ny;
	gboolean changed = FALSE;
	GntWindowFlags flags = GNT_IS_WINDOW(widget) ?
			gnt_window_get_maximize(GNT_WINDOW(widget)) : 0;

	gnt_widget_get_size(widget, &w, &h);
	if (x) {
		if (m && (flags & GNT_WINDOW_MAXIMIZE_X) && *x != 0) {
			*x = 0;
			changed = TRUE;
		} else if (*x + w > X_MAX) {
			nx = MAX(0, X_MAX - w);
			if (nx != *x) {
				*x = nx;
				changed = TRUE;
			}
		}
	}
	if (y) {
		if (m && (flags & GNT_WINDOW_MAXIMIZE_Y) && *y != 0) {
			*y = 0;
			changed = TRUE;
		} else if (*y + h > Y_MAX) {
			ny = MAX(0, Y_MAX - h);
			if (ny != *y) {
				*y = ny;
				changed = TRUE;
			}
		}
	}
	return changed;
}

static void
refresh_node(GntWidget *widget, GntNode *node, gpointer m)
{
	int x, y, w, h;
	int nw, nh;

	int X_MAX = getmaxx(stdscr);
	int Y_MAX = getmaxy(stdscr) - 1;

	GntWindowFlags flags = 0;

	if (m && GNT_IS_WINDOW(widget)) {
		flags = gnt_window_get_maximize(GNT_WINDOW(widget));
	}

	gnt_widget_get_position(widget, &x, &y);
	gnt_widget_get_size(widget, &w, &h);

	if (sanitize_position(widget, &x, &y, !!m))
		gnt_screen_move_widget(widget, x, y);

	if (flags & GNT_WINDOW_MAXIMIZE_X)
		nw = X_MAX;
	else
		nw = MIN(w, X_MAX);

	if (flags & GNT_WINDOW_MAXIMIZE_Y)
		nh = Y_MAX;
	else
		nh = MIN(h, Y_MAX);

	if (nw != w || nh != h)
		gnt_screen_resize_widget(widget, nw, nh);
}

static void
read_window_positions(GntWM *wm)
{
	GKeyFile *gfile = g_key_file_new();
	char *filename = g_build_filename(g_get_home_dir(), ".gntpositions", NULL);
	GError *error = NULL;
	char **keys;
	gsize nk;

	if (!g_key_file_load_from_file(gfile, filename, G_KEY_FILE_NONE, &error)) {
		gnt_warning("%s", error->message);
		g_error_free(error);
		g_free(filename);
		return;
	}

	keys = g_key_file_get_keys(gfile, "positions", &nk, &error);
	if (error) {
		gnt_warning("%s", error->message);
		g_error_free(error);
		error = NULL;
	} else {
		while (nk--) {
			char *title = keys[nk];
			gsize l;
			char **coords = g_key_file_get_string_list(gfile, "positions", title, &l, NULL);
			if (l == 2) {
				int x = atoi(coords[0]);
				int y = atoi(coords[1]);
				GntPosition *p = g_new0(GntPosition, 1);
				p->x = x;
				p->y = y;
				g_hash_table_replace(wm->positions, g_strdup(title + 1), p);
			} else {
				gnt_warning("Invalid number of arguments (%" G_GSIZE_FORMAT
						") for positioning a window.", l);
			}
			g_strfreev(coords);
		}
		g_strfreev(keys);
	}

	g_free(filename);
	g_key_file_free(gfile);
}

static gboolean check_idle(gpointer n)
{
	if (idle_update) {
		time(&last_active_time);
		idle_update = FALSE;
	}
	return TRUE;
}

static void
gnt_wm_init(GTypeInstance *instance, gpointer class)
{
	GntWM *wm = GNT_WM(instance);
	wm->workspaces = NULL;
	wm->name_places = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	wm->title_places = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	gnt_style_read_workspaces(wm);
	if (wm->workspaces == NULL) {
		wm->cws = gnt_ws_new("default");
		gnt_wm_add_workspace(wm, wm->cws);
	} else {
		wm->cws = wm->workspaces->data;
	}
	wm->event_stack = FALSE;
	wm->tagged = NULL;
	wm->windows = NULL;
	wm->actions = NULL;
	wm->nodes = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, free_node);
	wm->positions = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	if (gnt_style_get_bool(GNT_STYLE_REMPOS, TRUE))
		read_window_positions(wm);
	g_timeout_add_seconds(IDLE_CHECK_INTERVAL, check_idle, NULL);
	time(&last_active_time);
	gnt_wm_switch_workspace(wm, 0);
}

static void
switch_window(GntWM *wm, int direction, gboolean urgent)
{
	GntWidget *w = NULL, *wid = NULL;
	int pos, orgpos;

	if (wm->_list.window || wm->menu)
		return;

	if (!wm->cws->ordered || !wm->cws->ordered->next)
		return;

	if (wm->mode != GNT_KP_MODE_NORMAL) {
		ensure_normal_mode(wm);
	}

	w = wm->cws->ordered->data;
	orgpos = pos = g_list_index(wm->cws->list, w);

	do {
		pos += direction;

		if (pos < 0) {
			wid = g_list_last(wm->cws->list)->data;
			pos = g_list_length(wm->cws->list) - 1;
		} else if (pos >= g_list_length(wm->cws->list)) {
			wid = wm->cws->list->data;
			pos = 0;
		} else
			wid = g_list_nth_data(wm->cws->list, pos);
	} while (urgent && !GNT_WIDGET_IS_FLAG_SET(wid, GNT_WIDGET_URGENT) && pos != orgpos);

	gnt_wm_raise_window(wm, wid);
}

static gboolean
window_next(GntBindable *bindable, GList *null)
{
	GntWM *wm = GNT_WM(bindable);
	switch_window(wm, 1, FALSE);
	return TRUE;
}

static gboolean
window_prev(GntBindable *bindable, GList *null)
{
	GntWM *wm = GNT_WM(bindable);
	switch_window(wm, -1, FALSE);
	return TRUE;
}

static gboolean
switch_window_n(GntBindable *bind, GList *list)
{
	GntWM *wm = GNT_WM(bind);
	GList *l;
	int n;

	if (!wm->cws->ordered)
		return TRUE;

	if (list)
		n = GPOINTER_TO_INT(list->data);
	else
		n = 0;

	if ((l = g_list_nth(wm->cws->list, n)) != NULL)
	{
		gnt_wm_raise_window(wm, l->data);
	}

	return TRUE;
}

static gboolean
window_scroll_up(GntBindable *bindable, GList *null)
{
	GntWM *wm = GNT_WM(bindable);
	GntWidget *window;
	GntNode *node;

	if (!wm->cws->ordered)
		return TRUE;

	window = wm->cws->ordered->data;
	node = g_hash_table_lookup(wm->nodes, window);
	if (!node)
		return TRUE;

	if (node->scroll) {
		node->scroll--;
		gnt_wm_copy_win(window, node);
		update_screen(wm);
	}
	return TRUE;
}

static gboolean
window_scroll_down(GntBindable *bindable, GList *null)
{
	GntWM *wm = GNT_WM(bindable);
	GntWidget *window;
	GntNode *node;
	int w, h;

	if (!wm->cws->ordered)
		return TRUE;

	window = wm->cws->ordered->data;
	node = g_hash_table_lookup(wm->nodes, window);
	if (!node)
		return TRUE;

	gnt_widget_get_size(window, &w, &h);
	if (h - node->scroll > getmaxy(node->window)) {
		node->scroll++;
		gnt_wm_copy_win(window, node);
		update_screen(wm);
	}
	return TRUE;
}

static gboolean
window_close(GntBindable *bindable, GList *null)
{
	GntWM *wm = GNT_WM(bindable);

	if (wm->_list.window)
		return TRUE;

	if (wm->cws->ordered) {
		gnt_widget_destroy(wm->cws->ordered->data);
		ensure_normal_mode(wm);
	}

	return TRUE;
}

static void
destroy__list(GntWidget *widget, GntWM *wm)
{
	wm->_list.window = NULL;
	wm->_list.tree = NULL;
	wm->windows = NULL;
	wm->actions = NULL;
	update_screen(wm);
}

static void
setup__list(GntWM *wm)
{
	GntWidget *tree, *win;
	ensure_normal_mode(wm);
	win = wm->_list.window = gnt_box_new(FALSE, FALSE);
	gnt_box_set_toplevel(GNT_BOX(win), TRUE);
	gnt_box_set_pad(GNT_BOX(win), 0);
	GNT_WIDGET_SET_FLAGS(win, GNT_WIDGET_TRANSIENT);

	tree = wm->_list.tree = gnt_tree_new();
	gnt_box_add_widget(GNT_BOX(win), tree);

	g_signal_connect(G_OBJECT(win), "destroy", G_CALLBACK(destroy__list), wm);
}

static void
window_list_activate(GntTree *tree, GntWM *wm)
{
	GntBindable *sel = gnt_tree_get_selection_data(GNT_TREE(tree));

	gnt_widget_destroy(wm->_list.window);

	if (!sel)
		return;

	if (GNT_IS_WS(sel)) {
		gnt_wm_switch_workspace(wm, g_list_index(wm->workspaces, sel));
	} else {
		gnt_wm_raise_window(wm, GNT_WIDGET(sel));
	}
}

static void
populate_window_list(GntWM *wm, gboolean workspace)
{
	GList *iter;
	GntTree *tree = GNT_TREE(wm->windows->tree);
	if (!workspace) {
		for (iter = wm->cws->list; iter; iter = iter->next) {
			GntBox *box = GNT_BOX(iter->data);

			gnt_tree_add_row_last(tree, box,
					gnt_tree_create_row(tree, box->title), NULL);
			update_window_in_list(wm, GNT_WIDGET(box));
		}
	} else {
		GList *ws = wm->workspaces;
		for (; ws; ws = ws->next) {
			gnt_tree_add_row_last(tree, ws->data,
					gnt_tree_create_row(tree, gnt_ws_get_name(GNT_WS(ws->data))), NULL);
			for (iter = GNT_WS(ws->data)->list; iter; iter = iter->next) {
				GntBox *box = GNT_BOX(iter->data);

				gnt_tree_add_row_last(tree, box,
						gnt_tree_create_row(tree, box->title), ws->data);
				update_window_in_list(wm, GNT_WIDGET(box));
			}
		}
	}
}

static gboolean
window_list_key_pressed(GntWidget *widget, const char *text, GntWM *wm)
{
	if (text[1] == 0 && wm->cws->ordered) {
		GntBindable *sel = gnt_tree_get_selection_data(GNT_TREE(widget));
		switch (text[0]) {
			case '-':
			case ',':
				if (GNT_IS_WS(sel)) {
					/* reorder the workspace. */
				} else
					shift_window(wm, GNT_WIDGET(sel), -1);
				break;
			case '=':
			case '.':
				if (GNT_IS_WS(sel)) {
					/* reorder the workspace. */
				} else
					shift_window(wm, GNT_WIDGET(sel), 1);
				break;
			default:
				return FALSE;
		}
		gnt_tree_remove_all(GNT_TREE(widget));
		populate_window_list(wm, GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "workspace")));
		gnt_tree_set_selected(GNT_TREE(widget), sel);
		return TRUE;
	}
	return FALSE;
}

static void
list_of_windows(GntWM *wm, gboolean workspace)
{
	GntWidget *tree, *win;
	setup__list(wm);
	wm->windows = &wm->_list;

	win = wm->windows->window;
	tree = wm->windows->tree;

	gnt_box_set_title(GNT_BOX(win), workspace ? "Workspace List" : "Window List");

	populate_window_list(wm, workspace);

	if (wm->cws->ordered)
		gnt_tree_set_selected(GNT_TREE(tree), wm->cws->ordered->data);
	else if (workspace)
		gnt_tree_set_selected(GNT_TREE(tree), wm->cws);

	g_signal_connect(G_OBJECT(tree), "activate", G_CALLBACK(window_list_activate), wm);
	g_signal_connect(G_OBJECT(tree), "key_pressed", G_CALLBACK(window_list_key_pressed), wm);
	g_object_set_data(G_OBJECT(tree), "workspace", GINT_TO_POINTER(workspace));

	gnt_tree_set_col_width(GNT_TREE(tree), 0, getmaxx(stdscr) / 3);
	gnt_widget_set_size(tree, 0, getmaxy(stdscr) / 2);
	gnt_widget_set_position(win, getmaxx(stdscr) / 3, getmaxy(stdscr) / 4);

	gnt_widget_show(win);
}

static gboolean
window_list(GntBindable *bindable, GList *null)
{
	GntWM *wm = GNT_WM(bindable);

	if (wm->_list.window || wm->menu)
		return TRUE;

	if (!wm->cws->ordered)
		return TRUE;

	list_of_windows(wm, FALSE);

	return TRUE;
}

static void
dump_file_save(GntFileSel *fs, const char *path, const char *f, gpointer n)
{
	FILE *file;
	int x, y;
	chtype old = 0, now = 0;
	struct {
		char ascii;
		char *unicode;
	} unis[] = {
		{'q', "&#x2500;"},
		{'t', "&#x251c;"},
		{'u', "&#x2524;"},
		{'x', "&#x2502;"},
		{'-', "&#x2191;"},
		{'.', "&#x2193;"},
		{'l', "&#x250c;"},
		{'k', "&#x2510;"},
		{'m', "&#x2514;"},
		{'j', "&#x2518;"},
		{'a', "&#x2592;"},
		{'n', "&#x253c;"},
		{'w', "&#x252c;"},
		{'v', "&#x2534;"},
		{'\0', NULL}
	};

	gnt_widget_destroy(GNT_WIDGET(fs));

	if ((file = g_fopen(path, "w+")) == NULL) {
		return;
	}

	fprintf(file, "<head>\n  <meta http-equiv='Content-Type' content='text/html; charset=utf-8' />\n</head>\n<body>\n");
	fprintf(file, "<pre>");
	for (y = 0; y < getmaxy(stdscr); y++) {
		for (x = 0; x < getmaxx(stdscr); x++) {
			char ch[2] = {0, 0}, *print;
#ifdef NO_WIDECHAR
			now = mvwinch(curscr, y, x);
			ch[0] = now & A_CHARTEXT;
			now ^= ch[0];
#else
			cchar_t wch;
			char unicode[12];
			mvwin_wch(curscr, y, x, &wch);
			now = wch.attr;
			ch[0] = (char)(wch.chars[0] & 0xff);
#endif

#define CHECK(attr, start, end) \
			do \
			{  \
				if (now & attr)  \
				{  \
					if (!(old & attr))  \
						fprintf(file, "%s", start);  \
				}  \
				else if (old & attr)  \
				{  \
					fprintf(file, "%s", end);  \
				}  \
			} while (0)

			CHECK(A_BOLD, "<b>", "</b>");
			CHECK(A_UNDERLINE, "<u>", "</u>");
			CHECK(A_BLINK, "<blink>", "</blink>");

			if ((now & A_COLOR) != (old & A_COLOR) ||
				(now & A_REVERSE) != (old & A_REVERSE))
			{
				short fgp, bgp, r, g, b;
				struct
				{
					int r, g, b;
				} fg, bg;

				if (pair_content(PAIR_NUMBER(now & A_COLOR), &fgp, &bgp) != OK) {
					fgp = -1;
					bgp = -1;
				}
				if (fgp == -1)
					fgp = COLOR_BLACK;
				if (bgp == -1)
					bgp = COLOR_WHITE;
				if (now & A_REVERSE)
				{
					short tmp = fgp;
					fgp = bgp;
					bgp = tmp;
				}
				if (color_content(fgp, &r, &g, &b) != OK) {
					r = g = b = 0;
				}
				fg.r = r; fg.b = b; fg.g = g;
				if (color_content(bgp, &r, &g, &b) != OK) {
					r = g = b = 255;
				}
				bg.r = r; bg.b = b; bg.g = g;
#define ADJUST(x) (x = x * 255 / 1000)
				ADJUST(fg.r);
				ADJUST(fg.g);
				ADJUST(fg.b);
				ADJUST(bg.r);
				ADJUST(bg.b);
				ADJUST(bg.g);

				if (x) fprintf(file, "</span>");
				fprintf(file, "<span style=\"background:#%02x%02x%02x;color:#%02x%02x%02x\">",
						bg.r, bg.g, bg.b, fg.r, fg.g, fg.b);
			}
			print = ch;
#ifndef NO_WIDECHAR
			if (wch.chars[0] > 255) {
				snprintf(unicode, sizeof(unicode), "&#x%x;", (unsigned int)wch.chars[0]);
				print = unicode;
			}
#endif
			if (now & A_ALTCHARSET)
			{
				int u;
				for (u = 0; unis[u].ascii; u++) {
					if (ch[0] == unis[u].ascii) {
						print = unis[u].unicode;
						break;
					}
				}
				if (!unis[u].ascii)
					print = " ";
			}
			if (ch[0] == '&')
				fprintf(file, "&amp;");
			else if (ch[0] == '<')
				fprintf(file, "&lt;");
			else if (ch[0] == '>')
				fprintf(file, "&gt;");
			else
				fprintf(file, "%s", print);
			old = now;
		}
		fprintf(file, "</span>\n");
		old = 0;
	}
	fprintf(file, "</pre>\n</body>");
	fclose(file);
}

static void
dump_file_cancel(GntWidget *w, GntFileSel *fs)
{
	gnt_widget_destroy(GNT_WIDGET(fs));
}

static gboolean
dump_screen(GntBindable *b, GList *null)
{
	GntWidget *window = gnt_file_sel_new();
	GntFileSel *sel = GNT_FILE_SEL(window);

	g_object_set(G_OBJECT(window), "vertical", TRUE, NULL);
	gnt_box_add_widget(GNT_BOX(window), gnt_label_new("Please enter the filename to save the screenshot."));
	gnt_box_set_title(GNT_BOX(window), "Save Screenshot...");

	gnt_file_sel_set_suggested_filename(sel, "dump.html");
	g_signal_connect(G_OBJECT(sel), "file_selected", G_CALLBACK(dump_file_save), NULL);
	g_signal_connect(G_OBJECT(sel->cancel), "activate", G_CALLBACK(dump_file_cancel), sel);
	gnt_widget_show(window);
	return TRUE;
}

static void
shift_window(GntWM *wm, GntWidget *widget, int dir)
{
	GList *all = wm->cws->list;
	GList *list = g_list_find(all, widget);
	int length, pos;
	if (!list)
		return;

	length = g_list_length(all);
	pos = g_list_position(all, list);

	pos += dir;
	if (dir > 0)
		pos++;

	if (pos < 0)
		pos = length;
	else if (pos > length)
		pos = 0;

	all = g_list_insert(all, widget, pos);
	all = g_list_delete_link(all, list);
	wm->cws->list = all;
	gnt_ws_draw_taskbar(wm->cws, FALSE);
	if (wm->cws->ordered) {
		GntWidget *w = wm->cws->ordered->data;
		GntNode *node = g_hash_table_lookup(wm->nodes, w);
		top_panel(node->panel);
		update_panels();
		doupdate();
	}
}

static gboolean
shift_left(GntBindable *bindable, GList *null)
{
	GntWM *wm = GNT_WM(bindable);
	if (wm->_list.window)
		return TRUE;

	if(!wm->cws->ordered)
		return FALSE;

	shift_window(wm, wm->cws->ordered->data, -1);
	return TRUE;
}

static gboolean
shift_right(GntBindable *bindable, GList *null)
{
	GntWM *wm = GNT_WM(bindable);

	if (wm->_list.window)
		return TRUE;

	if(!wm->cws->ordered)
		return FALSE;

	shift_window(wm, wm->cws->ordered->data, 1);
	return TRUE;
}

static void
action_list_activate(GntTree *tree, GntWM *wm)
{
	GntAction *action = gnt_tree_get_selection_data(tree);
	action->callback();
	gnt_widget_destroy(wm->_list.window);
}

static int
compare_action(gconstpointer p1, gconstpointer p2)
{
	const GntAction *a1 = p1;
	const GntAction *a2 = p2;

	return g_utf8_collate(a1->label, a2->label);
}

static gboolean
list_actions(GntBindable *bindable, GList *null)
{
	GntWidget *tree, *win;
	GList *iter;
	GntWM *wm = GNT_WM(bindable);
	int n;
	if (wm->_list.window || wm->menu)
		return TRUE;

	if (wm->acts == NULL)
		return TRUE;

	setup__list(wm);
	wm->actions = &wm->_list;

	win = wm->actions->window;
	tree = wm->actions->tree;

	gnt_box_set_title(GNT_BOX(win), "Actions");
	GNT_WIDGET_SET_FLAGS(tree, GNT_WIDGET_NO_BORDER);
	/* XXX: Do we really want this? */
	gnt_tree_set_compare_func(GNT_TREE(tree), compare_action);

	for (iter = wm->acts; iter; iter = iter->next) {
		GntAction *action = iter->data;
		gnt_tree_add_row_last(GNT_TREE(tree), action,
				gnt_tree_create_row(GNT_TREE(tree), action->label), NULL);
	}
	g_signal_connect(G_OBJECT(tree), "activate", G_CALLBACK(action_list_activate), wm);
	n = g_list_length(wm->acts);
	gnt_widget_set_size(tree, 0, n);
	gnt_widget_set_position(win, 0, getmaxy(stdscr) - 3 - n);

	gnt_widget_show(win);
	return TRUE;
}

#ifndef NO_WIDECHAR
static int
widestringwidth(wchar_t *wide)
{
	int len, ret;
	char *string;

	len = wcstombs(NULL, wide, 0) + 1;
	string = g_new0(char, len);
	wcstombs(string, wide, len);
	ret = string ? gnt_util_onscreen_width(string, NULL) : 1;
	g_free(string);
	return ret;
}
#endif

/* Returns the onscreen width of the character at the position */
static int
reverse_char(WINDOW *d, int y, int x, gboolean set)
{
#define DECIDE(ch) (set ? ((ch) | A_REVERSE) : ((ch) & ~A_REVERSE))

#ifdef NO_WIDECHAR
	chtype ch;
	ch = mvwinch(d, y, x);
	mvwaddch(d, y, x, DECIDE(ch));
	return 1;
#else
	cchar_t ch;
	int wc = 1;
	if (mvwin_wch(d, y, x, &ch) == OK) {
		wc = widestringwidth(ch.chars);
		ch.attr = DECIDE(ch.attr);
		ch.attr &= WA_ATTRIBUTES;   /* XXX: This is a workaround for a bug */
		mvwadd_wch(d, y, x, &ch);
	}

	return wc;
#endif
}

static void
window_reverse(GntWidget *win, gboolean set, GntWM *wm)
{
	int i;
	int w, h;
	WINDOW *d;

	if (GNT_WIDGET_IS_FLAG_SET(win, GNT_WIDGET_NO_BORDER))
		return;

	d = win->window;
	gnt_widget_get_size(win, &w, &h);

	if (gnt_widget_has_shadow(win)) {
		--w;
		--h;
	}

	/* the top and bottom */
	for (i = 0; i < w; i += reverse_char(d, 0, i, set));
	for (i = 0; i < w; i += reverse_char(d, h-1, i, set));

	/* the left and right */
	for (i = 0; i < h; i += reverse_char(d, i, 0, set));
	for (i = 0; i < h; i += reverse_char(d, i, w-1, set));

	gnt_wm_copy_win(win, g_hash_table_lookup(wm->nodes, win));
	update_screen(wm);
}

static void
ensure_normal_mode(GntWM *wm)
{
	if (wm->mode != GNT_KP_MODE_NORMAL) {
		if (wm->cws->ordered)
			window_reverse(wm->cws->ordered->data, FALSE, wm);
		wm->mode = GNT_KP_MODE_NORMAL;
	}
}

static gboolean
start_move(GntBindable *bindable, GList *null)
{
	GntWM *wm = GNT_WM(bindable);
	if (wm->_list.window || wm->menu)
		return TRUE;
	if (!wm->cws->ordered)
		return TRUE;

	wm->mode = GNT_KP_MODE_MOVE;
	window_reverse(GNT_WIDGET(wm->cws->ordered->data), TRUE, wm);

	return TRUE;
}

static gboolean
start_resize(GntBindable *bindable, GList *null)
{
	GntWM *wm = GNT_WM(bindable);
	if (wm->_list.window || wm->menu)
		return TRUE;
	if (!wm->cws->ordered)
		return TRUE;

	wm->mode = GNT_KP_MODE_RESIZE;
	window_reverse(GNT_WIDGET(wm->cws->ordered->data), TRUE, wm);

	return TRUE;
}

static gboolean
wm_quit(GntBindable *bindable, GList *list)
{
	GntWM *wm = GNT_WM(bindable);
	if (write_timeout)
		write_already(wm);
	g_main_loop_quit(wm->loop);
	return TRUE;
}

static gboolean
return_true(GntWM *wm, GntWidget *w, int *a, int *b)
{
	return TRUE;
}

static gboolean
refresh_screen(GntBindable *bindable, GList *null)
{
	GntWM *wm = GNT_WM(bindable);
	GList *iter;

	endwin();
	refresh();

	g_hash_table_foreach(wm->nodes, (GHFunc)refresh_node, GINT_TO_POINTER(TRUE));
	g_signal_emit(wm, signals[SIG_TERMINAL_REFRESH], 0);

	for (iter = g_list_last(wm->cws->ordered); iter; iter = iter->prev) {
		GntWidget *w = iter->data;
		GntNode *node = g_hash_table_lookup(wm->nodes, w);
		top_panel(node->panel);
	}

	gnt_ws_draw_taskbar(wm->cws, TRUE);
	update_screen(wm);
	curs_set(0);   /* endwin resets the cursor to normal */

	return TRUE;
}

static gboolean
toggle_clipboard(GntBindable *bindable, GList *n)
{
	static GntWidget *clip;
	gchar *text;
	if (clip) {
		gnt_widget_destroy(clip);
		clip = NULL;
		return TRUE;
	}
	text = gnt_get_clipboard_string();
	clip = gnt_hwindow_new(FALSE);
	GNT_WIDGET_SET_FLAGS(clip, GNT_WIDGET_TRANSIENT);
	GNT_WIDGET_SET_FLAGS(clip, GNT_WIDGET_NO_BORDER);
	gnt_box_set_pad(GNT_BOX(clip), 0);
	gnt_box_add_widget(GNT_BOX(clip), gnt_label_new(" "));
	gnt_box_add_widget(GNT_BOX(clip), gnt_label_new(text));
	gnt_box_add_widget(GNT_BOX(clip), gnt_label_new(" "));
	gnt_widget_set_position(clip, 0, 0);
	gnt_widget_draw(clip);
	g_free(text);
	return TRUE;
}

static void remove_tag(gpointer wid, gpointer wim)
{
	GntWM *wm = GNT_WM(wim);
	GntWidget *w = GNT_WIDGET(wid);
	wm->tagged = g_list_remove(wm->tagged, w);
	mvwhline(w->window, 0, 1, ACS_HLINE | gnt_color_pair(GNT_COLOR_NORMAL), 3);
	gnt_widget_draw(w);
}

static gboolean
tag_widget(GntBindable *b, GList *params)
{
	GntWM *wm = GNT_WM(b);
	GntWidget *widget;

	if (!wm->cws->ordered)
		return FALSE;
	widget = wm->cws->ordered->data;

	if (g_list_find(wm->tagged, widget)) {
		remove_tag(widget, wm);
		return TRUE;
	}

	wm->tagged = g_list_prepend(wm->tagged, widget);
	wbkgdset(widget->window, ' ' | gnt_color_pair(GNT_COLOR_HIGHLIGHT));
	mvwprintw(widget->window, 0, 1, "[T]");
	gnt_widget_draw(widget);
	return TRUE;
}

static void
widget_move_ws(gpointer wid, gpointer w)
{
	GntWM *wm = GNT_WM(w);
	gnt_wm_widget_move_workspace(wm, wm->cws, GNT_WIDGET(wid));
}

static gboolean
place_tagged(GntBindable *b, GList *params)
{
	GntWM *wm = GNT_WM(b);
	g_list_foreach(wm->tagged, widget_move_ws, wm);
	g_list_foreach(wm->tagged, remove_tag, wm);
	g_list_free(wm->tagged);
	wm->tagged = NULL;
	return TRUE;
}

static gboolean
workspace_list(GntBindable *b, GList *params)
{
	GntWM *wm = GNT_WM(b);

	if (wm->_list.window || wm->menu)
		return TRUE;

	list_of_windows(wm, TRUE);

	return TRUE;
}

static gboolean
workspace_new(GntBindable *bindable, GList *null)
{
	GntWM *wm = GNT_WM(bindable);
	GntWS *ws = gnt_ws_new(NULL);
	gnt_wm_add_workspace(wm, ws);
	gnt_wm_switch_workspace(wm, g_list_index(wm->workspaces, ws));
	return TRUE;
}

static gboolean
ignore_keys_start(GntBindable *bindable, GList *n)
{
	GntWM *wm = GNT_WM(bindable);

	if(!wm->menu && !wm->_list.window && wm->mode == GNT_KP_MODE_NORMAL){
		ignore_keys = TRUE;
		return TRUE;
	}
	return FALSE;
}

static gboolean
ignore_keys_end(GntBindable *bindable, GList *n)
{
	if (ignore_keys) {
		ignore_keys = FALSE;
		return TRUE;
	}
	return FALSE;
}

static gboolean
window_next_urgent(GntBindable *bindable, GList *n)
{
	GntWM *wm = GNT_WM(bindable);
	switch_window(wm, 1, TRUE);
	return TRUE;
}

static gboolean
window_prev_urgent(GntBindable *bindable, GList *n)
{
	GntWM *wm = GNT_WM(bindable);
	switch_window(wm, -1, TRUE);
	return TRUE;
}

#ifdef USE_PYTHON
static void
python_script_selected(GntFileSel *fs, const char *path, const char *f, gpointer n)
{
	char *dir = g_path_get_dirname(path);
	FILE *file = fopen(path, "r");
	PyObject *pp = PySys_GetObject("path"), *dirobj = PyString_FromString(dir);

	PyList_Insert(pp, 0, dirobj);
	Py_DECREF(dirobj);
	PyRun_SimpleFile(file, path);
	fclose(file);

	if (PyErr_Occurred()) {
		PyErr_Print();
	}
	g_free(dir);

	gnt_widget_destroy(GNT_WIDGET(fs));
}

static gboolean
run_python(GntBindable *bindable, GList *n)
{
	GntWidget *window = gnt_file_sel_new();
	GntFileSel *sel = GNT_FILE_SEL(window);

	g_object_set(G_OBJECT(window), "vertical", TRUE, NULL);
	gnt_box_add_widget(GNT_BOX(window), gnt_label_new("Please select the python script you want to run."));
	gnt_box_set_title(GNT_BOX(window), "Select Python Script...");

	g_signal_connect(G_OBJECT(sel), "file_selected", G_CALLBACK(python_script_selected), NULL);
	g_signal_connect_swapped(G_OBJECT(sel->cancel), "activate", G_CALLBACK(gnt_widget_destroy), sel);
	gnt_widget_show(window);
	return TRUE;
}
#endif  /* USE_PYTHON */

static gboolean
help_for_bindable(GntWM *wm, GntBindable *bindable)
{
	gboolean ret = TRUE;
	GntBindableClass *klass = GNT_BINDABLE_GET_CLASS(bindable);

	if (klass->help_window) {
		gnt_wm_raise_window(wm, GNT_WIDGET(klass->help_window));
	} else {
		ret =  gnt_bindable_build_help_window(bindable);
	}
	return ret;
}

static gboolean
help_for_wm(GntBindable *bindable, GList *null)
{
	return help_for_bindable(GNT_WM(bindable),bindable);
}

static gboolean
help_for_window(GntBindable *bindable, GList *null)
{
	GntWM *wm = GNT_WM(bindable);
	GntWidget *widget;

	if(!wm->cws->ordered)
		return FALSE;

	widget = wm->cws->ordered->data;

	return help_for_bindable(wm,GNT_BINDABLE(widget));
}

static gboolean
help_for_widget(GntBindable *bindable, GList *null)
{
	GntWM *wm = GNT_WM(bindable);
	GntWidget *widget;

	if (!wm->cws->ordered)
		return TRUE;

	widget = wm->cws->ordered->data;
	if (!GNT_IS_BOX(widget))
		return TRUE;

	return help_for_bindable(wm, GNT_BINDABLE(GNT_BOX(widget)->active));
}

static void
accumulate_windows(gpointer window, gpointer node, gpointer p)
{
	GList *list = *(GList**)p;
	list = g_list_prepend(list, window);
	*(GList**)p = list;
}

static void
gnt_wm_destroy(GObject *obj)
{
	GntWM *wm = GNT_WM(obj);
	GList *list = NULL;
	g_hash_table_foreach(wm->nodes, accumulate_windows, &list);
	g_list_foreach(list, (GFunc)gnt_widget_destroy, NULL);
	g_list_free(list);
	g_hash_table_destroy(wm->nodes);
	wm->nodes = NULL;

	while (wm->workspaces) {
		g_object_unref(wm->workspaces->data);
		wm->workspaces = g_list_delete_link(wm->workspaces, wm->workspaces);
	}
#ifdef USE_PYTHON
	if (started_python) {
		Py_Finalize();
		started_python = FALSE;
	}
#endif
}

static void
gnt_wm_class_init(GntWMClass *klass)
{
	int i;
	GObjectClass *gclass = G_OBJECT_CLASS(klass);
	char key[32];

	gclass->dispose = gnt_wm_destroy;

	klass->new_window = gnt_wm_new_window_real;
	klass->decorate_window = NULL;
	klass->close_window = NULL;
	klass->window_resize_confirm = return_true;
	klass->window_resized = gnt_wm_win_resized;
	klass->window_move_confirm = return_true;
	klass->window_moved = gnt_wm_win_moved;
	klass->window_update = NULL;
	klass->key_pressed  = NULL;
	klass->mouse_clicked = NULL;
	klass->give_focus = gnt_wm_give_focus;

	signals[SIG_NEW_WIN] =
		g_signal_new("new_win",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET(GntWMClass, new_window),
					 NULL, NULL,
					 g_cclosure_marshal_VOID__POINTER,
					 G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals[SIG_DECORATE_WIN] =
		g_signal_new("decorate_win",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET(GntWMClass, decorate_window),
					 NULL, NULL,
					 g_cclosure_marshal_VOID__POINTER,
					 G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals[SIG_CLOSE_WIN] =
		g_signal_new("close_win",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET(GntWMClass, close_window),
					 NULL, NULL,
					 g_cclosure_marshal_VOID__POINTER,
					 G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals[SIG_CONFIRM_RESIZE] =
		g_signal_new("confirm_resize",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET(GntWMClass, window_resize_confirm),
					 gnt_boolean_handled_accumulator, NULL,
					 gnt_closure_marshal_BOOLEAN__POINTER_POINTER_POINTER,
					 G_TYPE_BOOLEAN, 3, G_TYPE_POINTER, G_TYPE_POINTER, G_TYPE_POINTER);

	signals[SIG_CONFIRM_MOVE] =
		g_signal_new("confirm_move",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET(GntWMClass, window_move_confirm),
					 gnt_boolean_handled_accumulator, NULL,
					 gnt_closure_marshal_BOOLEAN__POINTER_POINTER_POINTER,
					 G_TYPE_BOOLEAN, 3, G_TYPE_POINTER, G_TYPE_POINTER, G_TYPE_POINTER);

	signals[SIG_RESIZED] =
		g_signal_new("window_resized",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET(GntWMClass, window_resized),
					 NULL, NULL,
					 g_cclosure_marshal_VOID__POINTER,
					 G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals[SIG_MOVED] =
		g_signal_new("window_moved",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET(GntWMClass, window_moved),
					 NULL, NULL,
					 g_cclosure_marshal_VOID__POINTER,
					 G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals[SIG_UPDATE_WIN] =
		g_signal_new("window_update",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET(GntWMClass, window_update),
					 NULL, NULL,
					 g_cclosure_marshal_VOID__POINTER,
					 G_TYPE_NONE, 1, G_TYPE_POINTER);

	signals[SIG_GIVE_FOCUS] =
		g_signal_new("give_focus",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET(GntWMClass, give_focus),
					 NULL, NULL,
					 g_cclosure_marshal_VOID__POINTER,
					 G_TYPE_NONE, 1, G_TYPE_POINTER);

	signals[SIG_MOUSE_CLICK] =
		g_signal_new("mouse_clicked",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET(GntWMClass, mouse_clicked),
					 gnt_boolean_handled_accumulator, NULL,
					 gnt_closure_marshal_BOOLEAN__INT_INT_INT_POINTER,
					 G_TYPE_BOOLEAN, 4, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_POINTER);

	signals[SIG_TERMINAL_REFRESH] =
		g_signal_new("terminal-refresh",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET(GntWMClass, terminal_refresh),
					 NULL, NULL,
					 g_cclosure_marshal_VOID__VOID,
					 G_TYPE_NONE, 0);

	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "window-next", window_next,
				"\033" "n", NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "window-prev", window_prev,
				"\033" "p", NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "window-close", window_close,
				"\033" "c", NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "window-list", window_list,
				"\033" "w", NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "dump-screen", dump_screen,
				"\033" "D", NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "shift-left", shift_left,
				"\033" ",", NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "shift-right", shift_right,
				"\033" ".", NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "action-list", list_actions,
				"\033" "a", NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "start-move", start_move,
				"\033" "m", NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "start-resize", start_resize,
				"\033" "r", NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "wm-quit", wm_quit,
				"\033" "q", NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "refresh-screen", refresh_screen,
				"\033" "l", NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "switch-window-n", switch_window_n,
				NULL, NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "window-scroll-down", window_scroll_down,
				"\033" GNT_KEY_CTRL_J, NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "window-scroll-up", window_scroll_up,
				"\033" GNT_KEY_CTRL_K, NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "help-for-widget", help_for_widget,
				"\033" "/", NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "workspace-new", workspace_new,
				GNT_KEY_F9, NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "workspace-next", workspace_next,
				"\033" ">", NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "workspace-prev", workspace_prev,
				"\033" "<", NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "window-tag", tag_widget,
				"\033" "t", NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "place-tagged", place_tagged,
				"\033" "T", NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "workspace-list", workspace_list,
				"\033" "s", NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "toggle-clipboard", toggle_clipboard,
				"\033" "C", NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "help-for-wm", help_for_wm,
				"\033" "\\", NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "help-for-window", help_for_window,
				"\033" "|", NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "ignore-keys-start", ignore_keys_start,
				NULL, NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "ignore-keys-end", ignore_keys_end,
				"\033" GNT_KEY_CTRL_G, NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "window-next-urgent", window_next_urgent,
				"\033" "\t", NULL);
	snprintf(key, sizeof(key), "\033%s", GNT_KEY_BACK_TAB);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "window-prev-urgent", window_prev_urgent,
				key[1] ? key : NULL, NULL);
#ifdef USE_PYTHON
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "run-python", run_python,
				GNT_KEY_F3, NULL);
	if (!Py_IsInitialized()) {
		Py_SetProgramName("gnt");
		Py_Initialize();
		started_python = TRUE;
	}
#endif

	gnt_style_read_actions(G_OBJECT_CLASS_TYPE(klass), GNT_BINDABLE_CLASS(klass));

	/* Make sure Alt+x are detected properly. */
	for (i = '0'; i <= '9'; i++) {
		char str[] = "\033X";
		str[1] = i;
		gnt_keys_add_combination(str);
	}

	GNTDEBUG;
}

/******************************************************************************
 * GntWM API
 *****************************************************************************/
GType
gnt_wm_get_gtype(void)
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(GntWMClass),
			NULL,					/* base_init		*/
			NULL,					/* base_finalize	*/
			(GClassInitFunc)gnt_wm_class_init,
			NULL,
			NULL,					/* class_data		*/
			sizeof(GntWM),
			0,						/* n_preallocs		*/
			gnt_wm_init,			/* instance_init	*/
			NULL					/* value_table		*/
		};

		type = g_type_register_static(GNT_TYPE_BINDABLE,
									  "GntWM",
									  &info, 0);
	}

	return type;
}

void
gnt_wm_add_workspace(GntWM *wm, GntWS *ws)
{
	wm->workspaces = g_list_append(wm->workspaces, ws);
}

gboolean
gnt_wm_switch_workspace(GntWM *wm, gint n)
{
	GntWS *s = g_list_nth_data(wm->workspaces, n);
	if (!s)
		return FALSE;

	if (wm->_list.window) {
		gnt_widget_destroy(wm->_list.window);
	}
	ensure_normal_mode(wm);
	gnt_ws_hide(wm->cws, wm->nodes);
	wm->cws = s;
	gnt_ws_show(wm->cws, wm->nodes);

	gnt_ws_draw_taskbar(wm->cws, TRUE);
	update_screen(wm);
	if (wm->cws->ordered) {
		gnt_wm_raise_window(wm, wm->cws->ordered->data);
	}

	if (act && g_list_find(act, wm->cws)) {
		act = g_list_remove(act, wm->cws);
		update_act_msg();
	}
	return TRUE;
}

gboolean
gnt_wm_switch_workspace_prev(GntWM *wm)
{
	int n = g_list_index(wm->workspaces, wm->cws);
	return gnt_wm_switch_workspace(wm, --n);
}

gboolean
gnt_wm_switch_workspace_next(GntWM *wm)
{
	int n = g_list_index(wm->workspaces, wm->cws);
	return gnt_wm_switch_workspace(wm, ++n);
}

static gboolean
workspace_next(GntBindable *wm, GList *n)
{
	return gnt_wm_switch_workspace_next(GNT_WM(wm));
}

static gboolean
workspace_prev(GntBindable *wm, GList *n)
{
	return gnt_wm_switch_workspace_prev(GNT_WM(wm));
}

void
gnt_wm_widget_move_workspace(GntWM *wm, GntWS *neww, GntWidget *widget)
{
	GntWS *oldw = gnt_wm_widget_find_workspace(wm, widget);
	GntNode *node;
	if (!oldw || oldw == neww)
		return;
	node = g_hash_table_lookup(wm->nodes, widget);
	if (node && node->ws == neww)
		return;

	if (node)
		node->ws = neww;

	gnt_ws_remove_widget(oldw, widget);
	gnt_ws_add_widget(neww, widget);
	if (neww == wm->cws) {
		gnt_ws_widget_show(widget, wm->nodes);
	} else {
		gnt_ws_widget_hide(widget, wm->nodes);
	}
}

static gint widget_in_workspace(gconstpointer workspace, gconstpointer wid)
{
	GntWS *s = (GntWS *)workspace;
	if (s->list && g_list_find(s->list, wid))
		return 0;
	return 1;
}

GntWS *gnt_wm_widget_find_workspace(GntWM *wm, GntWidget *widget)
{
	GList *l = g_list_find_custom(wm->workspaces, widget, widget_in_workspace);
	if (l)
		return l->data;
	return NULL;
}

static void free_workspaces(gpointer data, gpointer n)
{
	GntWS *s = data;
	g_free(s->name);
}

void gnt_wm_set_workspaces(GntWM *wm, GList *workspaces)
{
	g_list_foreach(wm->workspaces, free_workspaces, NULL);
	wm->workspaces = workspaces;
	gnt_wm_switch_workspace(wm, 0);
}

static void
update_window_in_list(GntWM *wm, GntWidget *wid)
{
	GntTextFormatFlags flag = 0;

	if (wm->windows == NULL)
		return;

	if (wm->cws->ordered && wid == wm->cws->ordered->data)
		flag |= GNT_TEXT_FLAG_DIM;
	else if (GNT_WIDGET_IS_FLAG_SET(wid, GNT_WIDGET_URGENT))
		flag |= GNT_TEXT_FLAG_BOLD;

	gnt_tree_set_row_flags(GNT_TREE(wm->windows->tree), wid, flag);
}

static gboolean
match_title(gpointer title, gpointer n, gpointer wid_title)
{
	/* XXX: do any regex magic here. */
	if (g_strrstr((gchar *)wid_title, (gchar *)title))
		return TRUE;
	return FALSE;
}

static GntWS *
new_widget_find_workspace(GntWM *wm, GntWidget *widget)
{
	GntWS *ret = NULL;
	const gchar *name, *title;
	title = GNT_BOX(widget)->title;
	if (title)
		ret = g_hash_table_find(wm->title_places, match_title, (gpointer)title);
	if (ret)
		return ret;
	name = gnt_widget_get_name(widget);
	if (name)
		ret = g_hash_table_find(wm->name_places, match_title, (gpointer)name);
	return ret ? ret : wm->cws;
}

static void
gnt_wm_new_window_real(GntWM *wm, GntWidget *widget)
{
	GntNode *node;
	gboolean transient = FALSE;

	if (widget->window == NULL)
		return;

	node = g_new0(GntNode, 1);
	node->me = widget;
	node->scroll = 0;

	g_hash_table_replace(wm->nodes, widget, node);

	refresh_node(widget, node, GINT_TO_POINTER(TRUE));

	transient = !!GNT_WIDGET_IS_FLAG_SET(node->me, GNT_WIDGET_TRANSIENT);

#if 1
	{
		int x, y, w, h, maxx, maxy;
		gboolean shadow = TRUE;

		if (!gnt_widget_has_shadow(widget))
			shadow = FALSE;
		x = widget->priv.x;
		y = widget->priv.y;
		w = widget->priv.width + shadow;
		h = widget->priv.height + shadow;

		maxx = getmaxx(stdscr);
		maxy = getmaxy(stdscr) - 1;              /* room for the taskbar */

		x = MAX(0, x);
		y = MAX(0, y);
		if (x + w >= maxx)
			x = MAX(0, maxx - w);
		if (y + h >= maxy)
			y = MAX(0, maxy - h);

		w = MIN(w, maxx);
		h = MIN(h, maxy);
		node->window = newwin(h, w, y, x);
		gnt_wm_copy_win(widget, node);
	}
#endif

	node->panel = new_panel(node->window);
	set_panel_userptr(node->panel, node);

	if (!transient) {
		GntWS *ws = wm->cws;
		if (node->me != wm->_list.window) {
			if (GNT_IS_BOX(widget)) {
				ws = new_widget_find_workspace(wm, widget);
			}
			node->ws = ws;
			ws->list = g_list_append(ws->list, widget);
			ws->ordered = g_list_append(ws->ordered, widget);
		}

		if (wm->event_stack || node->me == wm->_list.window ||
				node->me == ws->ordered->data) {
			gnt_wm_raise_window(wm, node->me);
		} else {
			bottom_panel(node->panel);     /* New windows should not grab focus */
			gnt_widget_set_focus(node->me, FALSE);
			gnt_widget_set_urgent(node->me);
			if (wm->cws != ws)
				gnt_ws_widget_hide(widget, wm->nodes);
		}
	}
}

void gnt_wm_new_window(GntWM *wm, GntWidget *widget)
{
	while (widget->parent)
		widget = widget->parent;

	if (GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_INVISIBLE) ||
			g_hash_table_lookup(wm->nodes, widget)) {
		update_screen(wm);
		return;
	}

	if (GNT_IS_BOX(widget)) {
		const char *title = GNT_BOX(widget)->title;
		GntPosition *p = NULL;
		if (title && (p = g_hash_table_lookup(wm->positions, title)) != NULL) {
			sanitize_position(widget, &p->x, &p->y, TRUE);
			gnt_widget_set_position(widget, p->x, p->y);
			mvwin(widget->window, p->y, p->x);
		}
	}

	g_signal_emit(wm, signals[SIG_NEW_WIN], 0, widget);
	g_signal_emit(wm, signals[SIG_DECORATE_WIN], 0, widget);

	if (wm->windows && !GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_TRANSIENT)) {
		if ((GNT_IS_BOX(widget) && GNT_BOX(widget)->title) && wm->_list.window != widget
				&& GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_CAN_TAKE_FOCUS)) {
			gnt_tree_add_row_last(GNT_TREE(wm->windows->tree), widget,
					gnt_tree_create_row(GNT_TREE(wm->windows->tree), GNT_BOX(widget)->title),
					g_object_get_data(G_OBJECT(wm->windows->tree), "workspace") ? wm->cws : NULL);
			update_window_in_list(wm, widget);
		}
	}

	gnt_ws_draw_taskbar(wm->cws, FALSE);
	update_screen(wm);
}

void gnt_wm_window_decorate(GntWM *wm, GntWidget *widget)
{
	g_signal_emit(wm, signals[SIG_DECORATE_WIN], 0, widget);
}

void gnt_wm_window_close(GntWM *wm, GntWidget *widget)
{
	GntWS *s;
	int pos;
	gboolean transient = !!GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_TRANSIENT);

	s = gnt_wm_widget_find_workspace(wm, widget);

	if (g_hash_table_lookup(wm->nodes, widget) == NULL)
		return;

	g_signal_emit(wm, signals[SIG_CLOSE_WIN], 0, widget);
	g_hash_table_remove(wm->nodes, widget);

	if (wm->windows) {
		gnt_tree_remove(GNT_TREE(wm->windows->tree), widget);
	}

	if (s) {
		pos = g_list_index(s->list, widget);

		if (pos != -1) {
			s->list = g_list_remove(s->list, widget);
			s->ordered = g_list_remove(s->ordered, widget);

			if (s->ordered && wm->cws == s)
				gnt_wm_raise_window(wm, s->ordered->data);
		}
	} else if (transient && wm->cws && wm->cws->ordered) {
		gnt_wm_update_window(wm, wm->cws->ordered->data);
	}

	gnt_ws_draw_taskbar(wm->cws, FALSE);
	update_screen(wm);
}

time_t gnt_wm_get_idle_time()
{
	return time(NULL) - last_active_time;
}

gboolean gnt_wm_process_input(GntWM *wm, const char *keys)
{
	gboolean ret = FALSE;

	keys = gnt_bindable_remap_keys(GNT_BINDABLE(wm), keys);

	idle_update = TRUE;
	if(ignore_keys){
		if(keys && !strcmp(keys, "\033" GNT_KEY_CTRL_G)){
			if(gnt_bindable_perform_action_key(GNT_BINDABLE(wm), keys)){
				return TRUE;
			}
		}
		return wm->cws->ordered ? gnt_widget_key_pressed(GNT_WIDGET(wm->cws->ordered->data), keys) : FALSE;
	}

	if (gnt_bindable_perform_action_key(GNT_BINDABLE(wm), keys)) {
		return TRUE;
	}

	/* Do some manual checking */
	if (wm->cws->ordered && wm->mode != GNT_KP_MODE_NORMAL) {
		int xmin = 0, ymin = 0, xmax = getmaxx(stdscr), ymax = getmaxy(stdscr) - 1;
		int x, y, w, h;
		GntWidget *widget = GNT_WIDGET(wm->cws->ordered->data);
		int ox, oy, ow, oh;

		gnt_widget_get_position(widget, &x, &y);
		gnt_widget_get_size(widget, &w, &h);
		ox = x;	oy = y;
		ow = w;	oh = h;

		if (wm->mode == GNT_KP_MODE_MOVE) {
			if (strcmp(keys, GNT_KEY_LEFT) == 0) {
				if (x > xmin)
					x--;
			} else if (strcmp(keys, GNT_KEY_RIGHT) == 0) {
				if (x + w < xmax)
					x++;
			} else if (strcmp(keys, GNT_KEY_UP) == 0) {
				if (y > ymin)
					y--;
			} else if (strcmp(keys, GNT_KEY_DOWN) == 0) {
				if (y + h < ymax)
					y++;
			}
			if (ox != x || oy != y) {
				gnt_screen_move_widget(widget, x, y);
				window_reverse(widget, TRUE, wm);
				return TRUE;
			}
		} else if (wm->mode == GNT_KP_MODE_RESIZE) {
			if (strcmp(keys, GNT_KEY_LEFT) == 0) {
				w--;
			} else if (strcmp(keys, GNT_KEY_RIGHT) == 0) {
				if (x + w < xmax)
					w++;
			} else if (strcmp(keys, GNT_KEY_UP) == 0) {
				h--;
			} else if (strcmp(keys, GNT_KEY_DOWN) == 0) {
				if (y + h < ymax)
					h++;
			}
			if (oh != h || ow != w) {
				gnt_screen_resize_widget(widget, w, h);
				window_reverse(widget, TRUE, wm);
				return TRUE;
			}
		}
		if (strcmp(keys, "\r") == 0 || strcmp(keys, "\033") == 0) {
			window_reverse(widget, FALSE, wm);
			wm->mode = GNT_KP_MODE_NORMAL;
		}
		return TRUE;
	}

	/* Escape to close the window-list or action-list window */
	if (strcmp(keys, "\033") == 0) {
		if (wm->_list.window) {
			gnt_widget_destroy(wm->_list.window);
			return TRUE;
		}
	} else if (keys[0] == '\033' && isdigit(keys[1]) && keys[2] == '\0') {
		/* Alt+x for quick switch */
		int n = *(keys + 1) - '0';
		GList *list = NULL;

		if (n == 0)
			n = 10;

		list = g_list_append(list, GINT_TO_POINTER(n - 1));
		switch_window_n(GNT_BINDABLE(wm), list);
		g_list_free(list);
		return TRUE;
	}

	if (wm->menu)
		ret = gnt_widget_key_pressed(GNT_WIDGET(wm->menu), keys);
	else if (wm->_list.window)
		ret = gnt_widget_key_pressed(wm->_list.window, keys);
	else if (wm->cws->ordered) {
		GntWidget *win = wm->cws->ordered->data;
		if (GNT_IS_WINDOW(win)) {
			GntMenu *menu = GNT_WINDOW(win)->menu;
			if (menu) {
				const char *id = gnt_window_get_accel_item(GNT_WINDOW(win), keys);
				if (id) {
					GntMenuItem *item = gnt_menu_get_item(menu, id);
					if (item)
						ret = gnt_menuitem_activate(item);
				}
			}
		}
		if (!ret)
			ret = gnt_widget_key_pressed(win, keys);
	}
	return ret;
}

static void
gnt_wm_win_resized(GntWM *wm, GntNode *node)
{
	/*refresh_node(node->me, node, NULL);*/
}

static void
gnt_wm_win_moved(GntWM *wm, GntNode *node)
{
	refresh_node(node->me, node, NULL);
}

void gnt_wm_resize_window(GntWM *wm, GntWidget *widget, int width, int height)
{
	gboolean ret = TRUE;
	GntNode *node;
	int maxx, maxy;

	while (widget->parent)
		widget = widget->parent;
	node = g_hash_table_lookup(wm->nodes, widget);
	if (!node)
		return;

	g_signal_emit(wm, signals[SIG_CONFIRM_RESIZE], 0, widget, &width, &height, &ret);
	if (!ret)
		return;    /* resize is not permitted */
	hide_panel(node->panel);
	gnt_widget_set_size(widget, width, height);
	gnt_widget_draw(widget);

	maxx = getmaxx(stdscr);
	maxy = getmaxy(stdscr) - 1;
	height = MIN(height, maxy);
	width = MIN(width, maxx);
	wresize(node->window, height, width);
	replace_panel(node->panel, node->window);

	g_signal_emit(wm, signals[SIG_RESIZED], 0, node);

	show_panel(node->panel);
	update_screen(wm);
}

static void
write_gdi(gpointer key, gpointer value, gpointer data)
{
	GntPosition *p = value;
	fprintf(data, ".%s = %d;%d\n", (char *)key, p->x, p->y);
}

static gboolean
write_already(gpointer data)
{
	GntWM *wm = data;
	FILE *file;
	char *filename;

	filename = g_build_filename(g_get_home_dir(), ".gntpositions", NULL);

	file = fopen(filename, "wb");
	if (file == NULL) {
		gnt_warning("error opening file (%s) to save positions", filename);
	} else {
		fprintf(file, "[positions]\n");
		g_hash_table_foreach(wm->positions, write_gdi, file);
		fclose(file);
	}

	g_free(filename);
	g_source_remove(write_timeout);
	write_timeout = 0;
	return FALSE;
}

static void
write_positions_to_file(GntWM *wm)
{
	if (write_timeout) {
		g_source_remove(write_timeout);
	}
	write_timeout = g_timeout_add_seconds(10, write_already, wm);
}

void gnt_wm_move_window(GntWM *wm, GntWidget *widget, int x, int y)
{
	gboolean ret = TRUE;
	GntNode *node;

	while (widget->parent)
		widget = widget->parent;
	node = g_hash_table_lookup(wm->nodes, widget);
	if (!node)
		return;

	g_signal_emit(wm, signals[SIG_CONFIRM_MOVE], 0, widget, &x, &y, &ret);
	if (!ret)
		return;    /* resize is not permitted */

	gnt_widget_set_position(widget, x, y);
	move_panel(node->panel, y, x);

	g_signal_emit(wm, signals[SIG_MOVED], 0, node);
	if (gnt_style_get_bool(GNT_STYLE_REMPOS, TRUE) && GNT_IS_BOX(widget) &&
		!GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_TRANSIENT)) {
		const char *title = GNT_BOX(widget)->title;
		if (title) {
			GntPosition *p = g_new0(GntPosition, 1);
			GntWidget *wid = node->me;
			p->x = wid->priv.x;
			p->y = wid->priv.y;
			g_hash_table_replace(wm->positions, g_strdup(title), p);
			write_positions_to_file(wm);
		}
	}

	update_screen(wm);
}

static void
gnt_wm_give_focus(GntWM *wm, GntWidget *widget)
{
	GntNode *node = g_hash_table_lookup(wm->nodes, widget);

	if (!node)
		return;

	if (widget != wm->_list.window && !GNT_IS_MENU(widget) &&
				wm->cws->ordered->data != widget) {
		GntWidget *w = wm->cws->ordered->data;
		wm->cws->ordered = g_list_bring_to_front(wm->cws->ordered, widget);
		gnt_widget_set_focus(w, FALSE);
	}

	gnt_widget_set_focus(widget, TRUE);
	GNT_WIDGET_UNSET_FLAGS(widget, GNT_WIDGET_URGENT);
	gnt_widget_draw(widget);
	top_panel(node->panel);

	if (wm->_list.window) {
		GntNode *nd = g_hash_table_lookup(wm->nodes, wm->_list.window);
		top_panel(nd->panel);
	}
	gnt_ws_draw_taskbar(wm->cws, FALSE);
	update_screen(wm);
}

void gnt_wm_update_window(GntWM *wm, GntWidget *widget)
{
	GntNode *node = NULL;
	GntWS *ws;

	while (widget->parent)
		widget = widget->parent;
	if (!GNT_IS_MENU(widget)) {
		if (!GNT_IS_BOX(widget))
			return;
		gnt_box_sync_children(GNT_BOX(widget));
	}

	ws = gnt_wm_widget_find_workspace(wm, widget);
	node = g_hash_table_lookup(wm->nodes, widget);
	if (node == NULL) {
		gnt_wm_new_window(wm, widget);
	} else
		g_signal_emit(wm, signals[SIG_UPDATE_WIN], 0, node);

	if (ws == wm->cws || GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_TRANSIENT)) {
		gnt_wm_copy_win(widget, node);
		gnt_ws_draw_taskbar(wm->cws, FALSE);
		update_screen(wm);
	} else if (ws && ws != wm->cws && GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_URGENT)) {
		if (!act || (act && !g_list_find(act, ws)))
			act = g_list_prepend(act, ws);
		update_act_msg();
	}
}

gboolean gnt_wm_process_click(GntWM *wm, GntMouseEvent event, int x, int y, GntWidget *widget)
{
	gboolean ret = TRUE;
	idle_update = TRUE;
	g_signal_emit(wm, signals[SIG_MOUSE_CLICK], 0, event, x, y, widget, &ret);
	return ret;
}

void gnt_wm_raise_window(GntWM *wm, GntWidget *widget)
{
	GntWS *ws = gnt_wm_widget_find_workspace(wm, widget);
	if (wm->cws != ws)
		gnt_wm_switch_workspace(wm, g_list_index(wm->workspaces, ws));
	if (widget != wm->cws->ordered->data) {
		GntWidget *wid = wm->cws->ordered->data;
		wm->cws->ordered = g_list_bring_to_front(wm->cws->ordered, widget);
		gnt_widget_set_focus(wid, FALSE);
		gnt_widget_draw(wid);
	}
	gnt_widget_set_focus(widget, TRUE);
	gnt_widget_draw(widget);
	g_signal_emit(wm, signals[SIG_GIVE_FOCUS], 0, widget);
}

void gnt_wm_set_event_stack(GntWM *wm, gboolean set)
{
	wm->event_stack = set;
}


