#define _GNU_SOURCE
#if defined(__APPLE__)
#define _XOPEN_SOURCE_EXTENDED
#endif

#include "config.h"

#include <ctype.h>
#include <glib/gprintf.h>
#include <gmodule.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "gntwm.h"
#include "gntstyle.h"
#include "gntmarshal.h"
#include "gnt.h"
#include "gntbox.h"
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

static gboolean write_already(gpointer data);
static int write_timeout;
static time_t last_active_time;
static gboolean idle_update;
static GList *act = NULL; /* list of WS with unseen activitiy */

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
	int shadow;
	if (!node)
		return;
	src = widget->window;
	dst = node->window;
	shadow = gnt_widget_has_shadow(widget) ? 1 : 0;
	copywin(src, dst, node->scroll, 0, 0, 0, getmaxy(dst) - 1, getmaxx(dst) - 1, 0);
}

static void
update_act_msg()
{
	GntWidget *label;
	GList *iter;
	static GntWidget *message = NULL;
	GString *text = g_string_new("act: ");
	if (message)
		gnt_widget_destroy(message);
	if (g_list_length(act) == 0)
		return;
	for (iter = act; iter; iter = iter->next) {
		GntWS *ws = iter->data;
		g_string_sprintfa(text, "%s, ", gnt_ws_get_name(ws));
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
	if (wm->menu) {
		GntMenu *top = wm->menu;
		while (top) {
			GntNode *node = g_hash_table_lookup(wm->nodes, top);
			if (node)
				top_panel(node->panel);
			top = top->submenu;
		}
	}
	update_panels();
	doupdate();
	return TRUE;
}

static gboolean
sanitize_position(GntWidget *widget, int *x, int *y)
{
	int X_MAX = getmaxx(stdscr);
	int Y_MAX = getmaxy(stdscr) - 1;
	int w, h;
	int nx, ny;
	gboolean changed = FALSE;

	gnt_widget_get_size(widget, &w, &h);
	if (x) {
		if (*x + w > X_MAX) {
			nx = MAX(0, X_MAX - w);
			if (nx != *x) {
				*x = nx;
				changed = TRUE;
			}
		}
	}
	if (y) {
		if (*y + h > Y_MAX) {
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
refresh_node(GntWidget *widget, GntNode *node, gpointer null)
{
	int x, y, w, h;
	int nw, nh;

	int X_MAX = getmaxx(stdscr);
	int Y_MAX = getmaxy(stdscr) - 1;

	gnt_widget_get_position(widget, &x, &y);
	gnt_widget_get_size(widget, &w, &h);

	if (sanitize_position(widget, &x, &y))
		gnt_screen_move_widget(widget, x, y);

	nw = MIN(w, X_MAX);
	nh = MIN(h, Y_MAX);
	if (nw != w || nh != h)
		gnt_screen_resize_widget(widget, nw, nh);
}

static void
read_window_positions(GntWM *wm)
{
#if GLIB_CHECK_VERSION(2,6,0)
	GKeyFile *gfile = g_key_file_new();
	char *filename = g_build_filename(g_get_home_dir(), ".gntpositions", NULL);
	GError *error = NULL;
	char **keys;
	gsize nk;

	if (!g_key_file_load_from_file(gfile, filename, G_KEY_FILE_NONE, &error)) {
		g_printerr("GntWM: %s\n", error->message);
		g_error_free(error);
		g_free(filename);
		return;
	}

	keys = g_key_file_get_keys(gfile, "positions", &nk, &error);
	if (error) {
		g_printerr("GntWM: %s\n", error->message);
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
				g_printerr("GntWM: Invalid number of arguments for positioing a window.\n");
			}
			g_strfreev(coords);
		}
		g_strfreev(keys);
	}

	g_free(filename);
	g_key_file_free(gfile);
#endif
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
		wm->cws = g_object_new(GNT_TYPE_WS, NULL);
		gnt_ws_set_name(wm->cws, "default");
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
	g_timeout_add(IDLE_CHECK_INTERVAL * 1000, check_idle, NULL);
	time(&last_active_time);
	gnt_wm_switch_workspace(wm, 0);
}

static void
switch_window(GntWM *wm, int direction)
{
	GntWidget *w = NULL, *wid = NULL;
	int pos;

	if (wm->_list.window || wm->menu)
		return;

	if (!wm->cws->ordered || !wm->cws->ordered->next)
		return;

	w = wm->cws->ordered->data;
	pos = g_list_index(wm->cws->list, w);
	pos += direction;

	if (pos < 0)
		wid = g_list_last(wm->cws->list)->data;
	else if (pos >= g_list_length(wm->cws->list))
		wid = wm->cws->list->data;
	else if (pos >= 0)
		wid = g_list_nth_data(wm->cws->list, pos);

	wm->cws->ordered = g_list_bring_to_front(wm->cws->ordered, wid);

	gnt_wm_raise_window(wm, wm->cws->ordered->data);

	if (w != wid) {
		gnt_widget_set_focus(w, FALSE);
	}
}

static gboolean
window_next(GntBindable *bindable, GList *null)
{
	GntWM *wm = GNT_WM(bindable);
	switch_window(wm, 1);
	return TRUE;
}

static gboolean
window_prev(GntBindable *bindable, GList *null)
{
	GntWM *wm = GNT_WM(bindable);
	switch_window(wm, -1);
	return TRUE;
}

static gboolean
switch_window_n(GntBindable *bind, GList *list)
{
	GntWM *wm = GNT_WM(bind);
	GntWidget *w = NULL;
	GList *l;
	int n;

	if (!wm->cws->ordered)
		return TRUE;

	if (list)
		n = GPOINTER_TO_INT(list->data);
	else
		n = 0;

	w = wm->cws->ordered->data;

	if ((l = g_list_nth(wm->cws->list, n)) != NULL)
	{
		gnt_wm_raise_window(wm, l->data);
	}

	if (l && w != l->data)
	{
		gnt_widget_set_focus(w, FALSE);
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
	}

	return TRUE;
}

static gboolean
help_for_widget(GntBindable *bindable, GList *null)
{
	GntWM *wm = GNT_WM(bindable);
	GntWidget *widget, *tree, *win, *active;
	char *title;

	if (!wm->cws->ordered)
		return TRUE;

	widget = wm->cws->ordered->data;
	if (!GNT_IS_BOX(widget))
		return TRUE;
	active = GNT_BOX(widget)->active;

	tree = gnt_widget_bindings_view(active);
	win = gnt_window_new();
	title = g_strdup_printf("Bindings for %s", g_type_name(G_OBJECT_TYPE(active)));
	gnt_box_set_title(GNT_BOX(win), title);
	if (tree)
		gnt_box_add_widget(GNT_BOX(win), tree);
	else
		gnt_box_add_widget(GNT_BOX(win), gnt_label_new("This widget has no customizable bindings."));

	gnt_widget_show(win);

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
		GntNode *node = g_hash_table_lookup(wm->nodes, sel);
		if (node && node->ws != wm->cws)
			gnt_wm_switch_workspace(wm, g_list_index(wm->workspaces, node->ws));
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

static gboolean
dump_screen(GntBindable *bindable, GList *null)
{
	int x, y;
	chtype old = 0, now = 0;
	FILE *file = fopen("dump.html", "w");

	fprintf(file, "<pre>");
	for (y = 0; y < getmaxy(stdscr); y++) {
		for (x = 0; x < getmaxx(stdscr); x++) {
			char ch;
			now = mvwinch(curscr, y, x);
			ch = now & A_CHARTEXT;
			now ^= ch;

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
				int ret;
				short fgp, bgp, r, g, b;
				struct
				{
					int r, g, b;
				} fg, bg;

				ret = pair_content(PAIR_NUMBER(now & A_COLOR), &fgp, &bgp);
				if (fgp == -1)
					fgp = COLOR_BLACK;
				if (bgp == -1)
					bgp = COLOR_WHITE;
				if (now & A_REVERSE)
					fgp ^= bgp ^= fgp ^= bgp;  /* *wink* */
				ret = color_content(fgp, &r, &g, &b);
				fg.r = r; fg.b = b; fg.g = g;
				ret = color_content(bgp, &r, &g, &b);
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
			if (now & A_ALTCHARSET)
			{
				switch (ch)
				{
					case 'q':
						ch = '-'; break;
					case 't':
					case 'u':
					case 'x':
						ch = '|'; break;
					case 'v':
					case 'w':
					case 'l':
					case 'm':
					case 'k':
					case 'j':
					case 'n':
						ch = '+'; break;
					case '-':
						ch = '^'; break;
					case '.':
						ch = 'v'; break;
					case 'a':
						ch = '#'; break;
					default:
						ch = ' '; break;
				}
			}
			if (ch == '&')
				fprintf(file, "&amp;");
			else if (ch == '<')
				fprintf(file, "&lt;");
			else if (ch == '>')
				fprintf(file, "&gt;");
			else
				fprintf(file, "%c", ch);
			old = now;
		}
		fprintf(file, "</span>\n");
		old = 0;
	}
	fprintf(file, "</pre>");
	fclose(file);
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
}

static gboolean
shift_left(GntBindable *bindable, GList *null)
{
	GntWM *wm = GNT_WM(bindable);
	if (wm->_list.window)
		return TRUE;

	shift_window(wm, wm->cws->ordered->data, -1);
	return TRUE;
}

static gboolean
shift_right(GntBindable *bindable, GList *null)
{
	GntWM *wm = GNT_WM(bindable);
	if (wm->_list.window)
		return TRUE;

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
	gnt_widget_set_size(tree, 0, g_list_length(wm->acts));
	gnt_widget_set_position(win, 0, getmaxy(stdscr) - 3 - g_list_length(wm->acts));

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
	ret = gnt_util_onscreen_width(string, NULL);
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

	endwin();
	refresh();
	curs_set(0);   /* endwin resets the cursor to normal */

	g_hash_table_foreach(wm->nodes, (GHFunc)refresh_node, NULL);
	update_screen(wm);
	gnt_ws_draw_taskbar(wm->cws, TRUE);

	return FALSE;
}

static gboolean
toggle_clipboard(GntBindable *bindable, GList *n)
{
	static GntWidget *clip;
	gchar *text;
	int maxx, maxy;
	if (clip) {
		gnt_widget_destroy(clip);
		clip = NULL;
		return TRUE;
	}
	getmaxyx(stdscr, maxy, maxx);
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
	mvwhline(w->window, 0, 1, ACS_HLINE | COLOR_PAIR(GNT_COLOR_NORMAL), 3);
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
	wbkgdset(widget->window, ' ' | COLOR_PAIR(GNT_COLOR_HIGHLIGHT));
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

static void
gnt_wm_class_init(GntWMClass *klass)
{
	int i;

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

	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "window-next", window_next,
				"\033" "n", NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "window-prev", window_prev,
				"\033" "p", NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "window-close", window_close,
				"\033" "c", NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "window-list", window_list,
				"\033" "w", NULL);
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "dump-screen", dump_screen,
				"\033" "d", NULL);
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
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "toggle-clipboard",
				toggle_clipboard, "\033" "C", NULL);

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
	gnt_ws_hide(wm->cws, wm->nodes);
	wm->cws = s;
	gnt_ws_show(wm->cws, wm->nodes);

	gnt_ws_draw_taskbar(wm->cws, TRUE);
	update_screen(wm);
	if (wm->cws->ordered) {
		gnt_widget_set_focus(wm->cws->ordered->data, TRUE);
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
	/* maybe check for regex.h? */
	if (g_strrstr((gchar *)wid_title, (gchar *)title))
		return TRUE;
	return FALSE;
}

static GntWS *
new_widget_find_workspace(GntWM *wm, GntWidget *widget, gchar *wid_title)
{
	GntWS *ret;
	const gchar *name;
	ret = g_hash_table_find(wm->title_places, match_title, wid_title);
	if (ret)
		return ret;
	name = gnt_widget_get_name(widget);
	if (name)
		ret = g_hash_table_lookup(wm->name_places, name);
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

	refresh_node(widget, node, NULL);

	transient = !!GNT_WIDGET_IS_FLAG_SET(node->me, GNT_WIDGET_TRANSIENT);

#if 1
	{
		int x, y, w, h, maxx, maxy;
		gboolean shadow = TRUE;

		if (!gnt_widget_has_shadow(widget))
			shadow = FALSE;
		x = widget->priv.x;
		y = widget->priv.y;
		w = widget->priv.width;
		h = widget->priv.height;

		getmaxyx(stdscr, maxy, maxx);
		maxy -= 1;              /* room for the taskbar */
		maxy -= shadow;
		maxx -= shadow;

		x = MAX(0, x);
		y = MAX(0, y);
		if (x + w >= maxx)
			x = MAX(0, maxx - w);
		if (y + h >= maxy)
			y = MAX(0, maxy - h);

		w = MIN(w, maxx);
		h = MIN(h, maxy);
		node->window = newwin(h + shadow, w + shadow, y, x);
		gnt_wm_copy_win(widget, node);
	}
#endif

	node->panel = new_panel(node->window);
	set_panel_userptr(node->panel, node);

	if (!transient) {
		GntWS *ws = wm->cws;
		if (node->me != wm->_list.window) {
			GntWidget *w = NULL;

			if (GNT_IS_BOX(widget)) {
				char *title = GNT_BOX(widget)->title;
				ws = new_widget_find_workspace(wm, widget, title);
			}

			if (ws->ordered)
				w = ws->ordered->data;

			node->ws = ws;
			ws->list = g_list_append(ws->list, widget);

			if (wm->event_stack)
				ws->ordered = g_list_prepend(ws->ordered, widget);
			else
				ws->ordered = g_list_append(ws->ordered, widget);

			gnt_widget_set_focus(widget, TRUE);
			if (w)
				gnt_widget_set_focus(w, FALSE);
		}

		if (wm->event_stack || node->me == wm->_list.window) {
			if (wm->cws != ws)
				gnt_wm_switch_workspace(wm, g_list_index(wm->workspaces, ws));
			gnt_wm_raise_window(wm, node->me);
		} else {
			bottom_panel(node->panel);     /* New windows should not grab focus */
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
			sanitize_position(widget, &p->x, &p->y);
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

	update_screen(wm);
	gnt_ws_draw_taskbar(wm->cws, FALSE);
}

void gnt_wm_window_decorate(GntWM *wm, GntWidget *widget)
{
	g_signal_emit(wm, signals[SIG_DECORATE_WIN], 0, widget);
}

void gnt_wm_window_close(GntWM *wm, GntWidget *widget)
{
	GntWS *s;
	GntNode *node;
	int pos;

	s = gnt_wm_widget_find_workspace(wm, widget);

	if ((node = g_hash_table_lookup(wm->nodes, widget)) == NULL)
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
	}

	update_screen(wm);
	gnt_ws_draw_taskbar(wm->cws, FALSE);
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
	else if (wm->cws->ordered)
		ret = gnt_widget_key_pressed(GNT_WIDGET(wm->cws->ordered->data), keys);
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
	int shadow;
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

	shadow = gnt_widget_has_shadow(widget) ? 1 : 0;
	maxx = getmaxx(stdscr) - shadow;
	maxy = getmaxy(stdscr) - 1 - shadow;
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
		g_printerr("GntWM: error opening file to save positions\n");
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
	write_timeout = g_timeout_add(10000, write_already, wm);
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
	update_screen(wm);
	gnt_ws_draw_taskbar(wm->cws, FALSE);
}

void gnt_wm_update_window(GntWM *wm, GntWidget *widget)
{
	GntNode *node = NULL;
	GntWS *ws;

	while (widget->parent)
		widget = widget->parent;
	if (!GNT_IS_MENU(widget))
		gnt_box_sync_children(GNT_BOX(widget));

	ws = gnt_wm_widget_find_workspace(wm, widget);
	node = g_hash_table_lookup(wm->nodes, widget);
	if (node == NULL) {
		gnt_wm_new_window(wm, widget);
	} else
		g_signal_emit(wm, signals[SIG_UPDATE_WIN], 0, node);

	if (ws == wm->cws || GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_TRANSIENT)) {
		gnt_wm_copy_win(widget, node);
		update_screen(wm);
		gnt_ws_draw_taskbar(wm->cws, FALSE);
	} else if (ws != wm->cws && GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_URGENT)) {
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
	g_signal_emit(wm, signals[SIG_GIVE_FOCUS], 0, widget);
}

void gnt_wm_set_event_stack(GntWM *wm, gboolean set)
{
	wm->event_stack = set;
}

