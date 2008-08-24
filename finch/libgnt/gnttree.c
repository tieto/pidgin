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

#include "gntmarshal.h"
#include "gntstyle.h"
#include "gnttree.h"
#include "gntutils.h"

#include <string.h>
#include <ctype.h>

#define SEARCH_TIMEOUT 4000   /* 4 secs */
#define SEARCHING(tree)  (tree->priv->search && tree->priv->search->len > 0)

#define COLUMN_INVISIBLE(tree, index)  (tree->columns[index].flags & GNT_TREE_COLUMN_INVISIBLE)
#define BINARY_DATA(tree, index)       (tree->columns[index].flags & GNT_TREE_COLUMN_BINARY_DATA)
#define RIGHT_ALIGNED(tree, index)       (tree->columns[index].flags & GNT_TREE_COLUMN_RIGHT_ALIGNED)

enum
{
	PROP_0,
	PROP_COLUMNS,
	PROP_EXPANDER,
};

enum
{
	SIG_SELECTION_CHANGED,
	SIG_SCROLLED,
	SIG_TOGGLED,
	SIG_COLLAPSED,
	SIGS,
};

struct _GntTreePriv
{
	GString *search;
	int search_timeout;
	int search_column;
	gboolean (*search_func)(GntTree *tree, gpointer key, const char *search, const char *current);

	GCompareFunc compare;
	int lastvisible;
	int expander_level;
};

#define	TAB_SIZE 3

/* XXX: Make this one into a GObject?
 * 		 ... Probably not */
struct _GntTreeRow
{
	void *key;
	void *data;		/* XXX: unused */

	gboolean collapsed;
	gboolean choice;            /* Is this a choice-box?
	                               If choice is true, then child will be NULL */
	gboolean isselected;
	GntTextFormatFlags flags;
	int color;

	GntTreeRow *parent;
	GntTreeRow *child;
	GntTreeRow *next;
	GntTreeRow *prev;

	GList *columns;
	GntTree *tree;
};

struct _GntTreeCol
{
	char *text;
	gboolean isbinary;
	int span;       /* How many columns does it span? */
};

static void tree_selection_changed(GntTree *, GntTreeRow *, GntTreeRow *);
static void _gnt_tree_init_internals(GntTree *tree, int col);

static GntWidgetClass *parent_class = NULL;
static guint signals[SIGS] = { 0 };

static void
readjust_columns(GntTree *tree)
{
	int i, col, total;
	int width;
#define WIDTH(i) (tree->columns[i].width_ratio ? tree->columns[i].width_ratio : tree->columns[i].width)
	gnt_widget_get_size(GNT_WIDGET(tree), &width, NULL);
	if (!GNT_WIDGET_IS_FLAG_SET(GNT_WIDGET(tree), GNT_WIDGET_NO_BORDER))
		width -= 2;
	width -= 1;  /* Exclude the scrollbar from the calculation */
	for (i = 0, total = 0; i < tree->ncol ; i++) {
		if (tree->columns[i].flags & GNT_TREE_COLUMN_INVISIBLE)
			continue;
		if (tree->columns[i].flags & GNT_TREE_COLUMN_FIXED_SIZE)
			width -= WIDTH(i) + (tree->priv->lastvisible != i);
		else
			total += WIDTH(i) + (tree->priv->lastvisible != i);
	}

	if (total == 0)
		return;

	for (i = 0; i < tree->ncol; i++) {
		if (tree->columns[i].flags & GNT_TREE_COLUMN_INVISIBLE)
			continue;
		if (tree->columns[i].flags & GNT_TREE_COLUMN_FIXED_SIZE)
			col = WIDTH(i);
		else
			col = (WIDTH(i) * width) / total;
		gnt_tree_set_col_width(GNT_TREE(tree), i, col);
	}
}

/* Move the item at position old to position new */
static GList *
g_list_reposition_child(GList *list, int old, int new)
{
	gpointer item = g_list_nth_data(list, old);
	list = g_list_remove(list, item);
	if (old < new)
		new--;   /* because the positions would have shifted after removing the item */
	list = g_list_insert(list, item, new);
	return list;
}

static GntTreeRow *
_get_next(GntTreeRow *row, gboolean godeep)
{
	if (row == NULL)
		return NULL;
	if (godeep && row->child)
		return row->child;
	if (row->next)
		return row->next;
	return _get_next(row->parent, FALSE);
}

static gboolean
row_matches_search(GntTreeRow *row)
{
	GntTree *t = row->tree;
	if (t->priv->search && t->priv->search->len > 0) {
		GntTreeCol *col = (col = g_list_nth_data(row->columns, t->priv->search_column)) ? col : row->columns->data;
		char *one, *two, *z;
		if (t->priv->search_func)
			return t->priv->search_func(t, row->key, t->priv->search->str, col->text);
		one = g_utf8_casefold(col->text, -1);
		two = g_utf8_casefold(t->priv->search->str, -1);
		z = strstr(one, two);
		g_free(one);
		g_free(two);
		if (z == NULL)
			return FALSE;
	}
	return TRUE;
}

static GntTreeRow *
get_next(GntTreeRow *row)
{
	if (row == NULL)
		return NULL;
	while ((row = _get_next(row, !row->collapsed)) != NULL) {
		if (row_matches_search(row))
			break;
	}
	return row;
}

/* Returns the n-th next row. If it doesn't exist, returns NULL */
static GntTreeRow *
get_next_n(GntTreeRow *row, int n)
{
	while (row && n--)
		row = get_next(row);
	return row;
}

/* Returns the n-th next row. If it doesn't exist, then the last non-NULL node */
static GntTreeRow *
get_next_n_opt(GntTreeRow *row, int n, int *pos)
{
	GntTreeRow *next = row;
	int r = 0;

	if (row == NULL)
		return NULL;

	while (row && n--)
	{
		row = get_next(row);
		if (row)
		{
			next = row;
			r++;
		}
	}

	if (pos)
		*pos = r;

	return next;
}

static GntTreeRow *
get_last_child(GntTreeRow *row)
{
	if (row == NULL)
		return NULL;
	if (!row->collapsed && row->child)
		row = row->child;
	else
		return row;

	while(row->next)
		row = row->next;
	return get_last_child(row);
}

static GntTreeRow *
get_prev(GntTreeRow *row)
{
	if (row == NULL)
		return NULL;
	while (row) {
		if (row->prev)
			row = get_last_child(row->prev);
		else
			row = row->parent;
		if (!row || row_matches_search(row))
			break;
	}
	return row;
}

static GntTreeRow *
get_prev_n(GntTreeRow *row, int n)
{
	while (row && n--)
		row = get_prev(row);
	return row;
}

/* Distance of row from the root */
/* XXX: This is uber-inefficient */
static int
get_root_distance(GntTreeRow *row)
{
	if (row == NULL)
		return -1;
	return get_root_distance(get_prev(row)) + 1;
}

/* Returns the distance between a and b. 
 * If a is 'above' b, then the distance is positive */
static int
get_distance(GntTreeRow *a, GntTreeRow *b)
{
	/* First get the distance from a to the root.
	 * Then the distance from b to the root.
	 * Subtract.
	 * It's not that good, but it works. */
	int ha = get_root_distance(a);
	int hb = get_root_distance(b);

	return (hb - ha);
}

static int
find_depth(GntTreeRow *row)
{
	int dep = -1;

	while (row)
	{
		dep++;
		row = row->parent;
	}

	return dep;
}

static char *
update_row_text(GntTree *tree, GntTreeRow *row)
{
	GString *string = g_string_new(NULL);
	GList *iter;
	int i;
	gboolean notfirst = FALSE;

	for (i = 0, iter = row->columns; i < tree->ncol && iter; i++, iter = iter->next)
	{
		GntTreeCol *col = iter->data;
		const char *text;
		int len;
		int fl = 0;
		gboolean cut = FALSE;
		int width;
		const char *display;

		if (COLUMN_INVISIBLE(tree, i))
			continue;

		if (BINARY_DATA(tree, i))
			display = "";
		else
			display = col->text;

		len = gnt_util_onscreen_width(display, NULL);

		width = tree->columns[i].width;

		if (i == 0)
		{
			if (row->choice)
			{
				g_string_append_printf(string, "[%c] ",
						row->isselected ? 'X' : ' ');
				fl = 4;
			}
			else if (find_depth(row) < tree->priv->expander_level && row->child)
			{
				if (row->collapsed)
				{
					string = g_string_append(string, "+ ");
				}
				else
				{
					string = g_string_append(string, "- ");
				}
				fl = 2;
			}
			else
			{
				fl = TAB_SIZE * find_depth(row);
				g_string_append_printf(string, "%*s", fl, "");
			}
			len += fl;
		} else if (notfirst && tree->show_separator)
			g_string_append_c(string, '|');
		else
			g_string_append_c(string, ' ');

		notfirst = TRUE;

		if (len > width) {
			len = MAX(1, width - 1);
			cut = TRUE;
		}

		if (RIGHT_ALIGNED(tree, i) && len < tree->columns[i].width) {
			g_string_append_printf(string, "%*s", width - len - cut, "");
		}

		text = gnt_util_onscreen_width_to_pointer(display, len - fl, NULL);
		string = g_string_append_len(string, display, text - display);
		if (cut && width > 1) { /* ellipsis */
			if (gnt_ascii_only())
				g_string_append_c(string, '~');
			else
				string = g_string_append(string, "\342\200\246");
			len++;
		}

		if (!RIGHT_ALIGNED(tree, i) && len < tree->columns[i].width && iter->next)
			g_string_append_printf(string, "%*s", width - len, "");
	}
	return g_string_free(string, FALSE);
}

#define NEXT_X x += tree->columns[i].width + (i > 0 ? 1 : 0)

static void
tree_mark_columns(GntTree *tree, int pos, int y, chtype type)
{
	GntWidget *widget = GNT_WIDGET(tree);
	int i;
	int x = pos;
	gboolean notfirst = FALSE;

	for (i = 0; i < tree->ncol - 1; i++)
	{
		if (!COLUMN_INVISIBLE(tree, i)) {
			notfirst = TRUE;
			NEXT_X;
		}
		if (!COLUMN_INVISIBLE(tree, i+1) && notfirst)
			mvwaddch(widget->window, y, x, type);
	}
}

static void
redraw_tree(GntTree *tree)
{
	int start, i;
	GntWidget *widget = GNT_WIDGET(tree);
	GntTreeRow *row;
	int pos, up, down = 0;
	int rows, scrcol;

	if (!GNT_WIDGET_IS_FLAG_SET(GNT_WIDGET(tree), GNT_WIDGET_MAPPED))
		return;

	if (GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_NO_BORDER))
		pos = 0;
	else
		pos = 1;

	if (tree->top == NULL)
		tree->top = tree->root;
	if (tree->current == NULL) {
		tree->current = tree->root;
		tree_selection_changed(tree, NULL, tree->current);
	}

	wbkgd(widget->window, gnt_color_pair(GNT_COLOR_NORMAL));

	start = 0;
	if (tree->show_title)
	{
		int i;
		int x = pos;

		mvwhline(widget->window, pos + 1, pos, ACS_HLINE | gnt_color_pair(GNT_COLOR_NORMAL),
				widget->priv.width - pos - 1);
		mvwhline(widget->window, pos, pos, ' ' | gnt_color_pair(GNT_COLOR_NORMAL),
				widget->priv.width - pos - 1);

		for (i = 0; i < tree->ncol; i++)
		{
			if (COLUMN_INVISIBLE(tree, i)) {
				continue;
			}
			mvwaddnstr(widget->window, pos, x + (x != pos), tree->columns[i].title, tree->columns[i].width);
			NEXT_X;
		}
		if (pos)
		{
			tree_mark_columns(tree, pos, 0,
					(tree->show_separator ? ACS_TTEE : ACS_HLINE) | gnt_color_pair(GNT_COLOR_NORMAL));
			tree_mark_columns(tree, pos, widget->priv.height - pos,
					(tree->show_separator ? ACS_BTEE : ACS_HLINE) | gnt_color_pair(GNT_COLOR_NORMAL));
		}
		tree_mark_columns(tree, pos, pos + 1,
			(tree->show_separator ? ACS_PLUS : ACS_HLINE) | gnt_color_pair(GNT_COLOR_NORMAL));
		tree_mark_columns(tree, pos, pos,
			(tree->show_separator ? ACS_VLINE : ' ') | gnt_color_pair(GNT_COLOR_NORMAL));
		start = 2;
	}

	rows = widget->priv.height - pos * 2 - start - 1;
	tree->bottom = get_next_n_opt(tree->top, rows, &down);
	if (down < rows)
	{
		tree->top = get_prev_n(tree->bottom, rows);
		if (tree->top == NULL)
			tree->top = tree->root;
	}

	up = get_distance(tree->top, tree->current);
	if (up < 0)
		tree->top = tree->current;
	else if (up >= widget->priv.height - pos)
		tree->top = get_prev_n(tree->current, rows);

	if (tree->top && !row_matches_search(tree->top))
		tree->top = get_next(tree->top);
	row = tree->top;
	scrcol = widget->priv.width - 1 - 2 * pos;  /* exclude the borders and the scrollbar */
	for (i = start + pos; row && i < widget->priv.height - pos;
				i++, row = get_next(row))
	{
		char *str;
		int wr;

		GntTextFormatFlags flags = row->flags;
		int attr = 0;

		if (!row_matches_search(row))
			continue;
		str = update_row_text(tree, row);

		if ((wr = gnt_util_onscreen_width(str, NULL)) > scrcol)
		{
			char *s = (char*)gnt_util_onscreen_width_to_pointer(str, scrcol, &wr);
			*s = '\0';
		}

		if (flags & GNT_TEXT_FLAG_BOLD)
			attr |= A_BOLD;
		if (flags & GNT_TEXT_FLAG_UNDERLINE)
			attr |= A_UNDERLINE;
		if (flags & GNT_TEXT_FLAG_BLINK)
			attr |= A_BLINK;

		if (row == tree->current)
		{
			if (gnt_widget_has_focus(widget))
				attr |= gnt_color_pair(GNT_COLOR_HIGHLIGHT);
			else
				attr |= gnt_color_pair(GNT_COLOR_HIGHLIGHT_D);
		}
		else
		{
			if (flags & GNT_TEXT_FLAG_DIM)
				if (row->color)
					attr |= (A_DIM | gnt_color_pair(row->color));
				else
					attr |= (A_DIM | gnt_color_pair(GNT_COLOR_DISABLED));
			else if (flags & GNT_TEXT_FLAG_HIGHLIGHT)
				attr |= (A_DIM | gnt_color_pair(GNT_COLOR_HIGHLIGHT));
			else if (row->color)
				attr |= gnt_color_pair(row->color);
			else
				attr |= gnt_color_pair(GNT_COLOR_NORMAL);
		}

		wbkgdset(widget->window, '\0' | attr);
		mvwaddstr(widget->window, i, pos, str);
		whline(widget->window, ' ', scrcol - wr);
		tree->bottom = row;
		g_free(str);
		tree_mark_columns(tree, pos, i,
			(tree->show_separator ? ACS_VLINE : ' ') | attr);
	}

	wbkgdset(widget->window, '\0' | gnt_color_pair(GNT_COLOR_NORMAL));
	while (i < widget->priv.height - pos)
	{
		mvwhline(widget->window, i, pos, ' ',
				widget->priv.width - pos * 2 - 1);
		tree_mark_columns(tree, pos, i,
			(tree->show_separator ? ACS_VLINE : ' '));
		i++;
	}

	scrcol = widget->priv.width - pos - 1;  /* position of the scrollbar */
	rows--;
	if (rows > 0)
	{
		int total = 0;
		int showing, position;

		get_next_n_opt(tree->root, g_list_length(tree->list), &total);
		showing = rows * rows / MAX(total, 1) + 1;
		showing = MIN(rows, showing);

		total -= rows;
		up = get_distance(tree->root, tree->top);
		down = total - up;

		position = (rows - showing) * up / MAX(1, up + down);
		position = MAX((tree->top != tree->root), position);

		if (showing + position > rows)
			position = rows - showing;

		if (showing + position == rows  && row)
			position = MAX(0, rows - 1 - showing);
		else if (showing + position < rows && !row)
			position = rows - showing;

		position += pos + start + 1;

		mvwvline(widget->window, pos + start + 1, scrcol,
				' ' | gnt_color_pair(GNT_COLOR_NORMAL), rows);
		mvwvline(widget->window, position, scrcol,
				ACS_CKBOARD | gnt_color_pair(GNT_COLOR_HIGHLIGHT_D), showing);
	}

	mvwaddch(widget->window, start + pos, scrcol,
			((tree->top != tree->root) ?  ACS_UARROW : ' ') |
				gnt_color_pair(GNT_COLOR_HIGHLIGHT_D));

	mvwaddch(widget->window, widget->priv.height - pos - 1, scrcol,
			(row ?  ACS_DARROW : ' ') | gnt_color_pair(GNT_COLOR_HIGHLIGHT_D));

	/* If there's a search-text, show it in the bottom of the tree */
	if (tree->priv->search && tree->priv->search->len > 0) {
		const char *str = gnt_util_onscreen_width_to_pointer(tree->priv->search->str, scrcol - 1, NULL);
		wbkgdset(widget->window, '\0' | gnt_color_pair(GNT_COLOR_HIGHLIGHT_D));
		mvwaddnstr(widget->window, widget->priv.height - pos - 1, pos,
				tree->priv->search->str, str - tree->priv->search->str);
	}

	gnt_widget_queue_update(widget);
}

static void
gnt_tree_draw(GntWidget *widget)
{
	GntTree *tree = GNT_TREE(widget);

	redraw_tree(tree);
	
	GNTDEBUG;
}

static void
gnt_tree_size_request(GntWidget *widget)
{
	if (widget->priv.height == 0)
		widget->priv.height = 10;	/* XXX: Why?! */
	if (widget->priv.width == 0)
	{
		GntTree *tree = GNT_TREE(widget);
		int i, width = 0;
		width = 1 + 2 * (!GNT_WIDGET_IS_FLAG_SET(GNT_WIDGET(tree), GNT_WIDGET_NO_BORDER));
		for (i = 0; i < tree->ncol; i++)
			if (!COLUMN_INVISIBLE(tree, i)) {
				width = width + tree->columns[i].width;
				if (tree->priv->lastvisible != i)
					width++;
			}
		widget->priv.width = width;
	}
}

static void
gnt_tree_map(GntWidget *widget)
{
	GntTree *tree = GNT_TREE(widget);
	if (widget->priv.width == 0 || widget->priv.height == 0)
	{
		gnt_widget_size_request(widget);
	}
	tree->top = tree->root;
	tree->current = tree->root;
	GNTDEBUG;
}

static void
tree_selection_changed(GntTree *tree, GntTreeRow *old, GntTreeRow *current)
{
	g_signal_emit(tree, signals[SIG_SELECTION_CHANGED], 0, old ? old->key : NULL,
				current ? current->key : NULL);
}

static gboolean
action_down(GntBindable *bind, GList *null)
{
	int dist;
	GntTree *tree = GNT_TREE(bind);
	GntTreeRow *old = tree->current;
	GntTreeRow *row = get_next(tree->current);
	if (row == NULL)
		return FALSE;
	tree->current = row;
	if ((dist = get_distance(tree->current, tree->bottom)) < 0)
		gnt_tree_scroll(tree, -dist);
	else
		redraw_tree(tree);
	if (old != tree->current)
		tree_selection_changed(tree, old, tree->current);
	return TRUE;
}

static gboolean
action_move_parent(GntBindable *bind, GList *null)
{
	GntTree *tree = GNT_TREE(bind);
	GntTreeRow *row = tree->current;
	int dist;

	if (!row || !row->parent || SEARCHING(tree))
		return FALSE;

	tree->current = row->parent;
	if ((dist = get_distance(tree->current, tree->top)) > 0)
		gnt_tree_scroll(tree, -dist);
	else
		redraw_tree(tree);
	tree_selection_changed(tree, row, tree->current);
	return TRUE;
}

static gboolean
action_up(GntBindable *bind, GList *list)
{
	int dist;
	GntTree *tree = GNT_TREE(bind);
	GntTreeRow *old = tree->current;
	GntTreeRow *row = get_prev(tree->current);
	if (!row)
		return FALSE;
	tree->current = row;
	if ((dist = get_distance(tree->current, tree->top)) > 0)
		gnt_tree_scroll(tree, -dist);
	else
		redraw_tree(tree);
	if (old != tree->current)
		tree_selection_changed(tree, old, tree->current);

	return TRUE;
}

static gboolean
action_page_down(GntBindable *bind, GList *null)
{
	GntTree *tree = GNT_TREE(bind);
	GntTreeRow *old = tree->current;
	GntTreeRow *row = get_next(tree->bottom);
	if (row)
	{
		int dist = get_distance(tree->top, tree->current);
		tree->top = tree->bottom;
		tree->current = get_next_n_opt(tree->top, dist, NULL);
		redraw_tree(tree);
	}
	else if (tree->current != tree->bottom)
	{
		tree->current = tree->bottom;
		redraw_tree(tree);
	}

	if (old != tree->current)
		tree_selection_changed(tree, old, tree->current);
	return TRUE;
}

static gboolean
action_page_up(GntBindable *bind, GList *null)
{
	GntWidget *widget = GNT_WIDGET(bind);
	GntTree *tree = GNT_TREE(bind);
	GntTreeRow *row;
	GntTreeRow *old = tree->current;

	if (tree->top != tree->root)
	{
		int dist = get_distance(tree->top, tree->current);
		row = get_prev_n(tree->top, widget->priv.height - 1 -
			tree->show_title * 2 - 2 * (GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_NO_BORDER) == 0));
		if (row == NULL)
			row = tree->root;
		tree->top = row;
		tree->current = get_next_n_opt(tree->top, dist, NULL);
		redraw_tree(tree);
	}
	else if (tree->current != tree->top)
	{
		tree->current = tree->top;
		redraw_tree(tree);
	}
	if (old != tree->current)
		tree_selection_changed(tree, old, tree->current);
	return TRUE;
}

static void
end_search(GntTree *tree)
{
	if (tree->priv->search) {
		g_source_remove(tree->priv->search_timeout);
		g_string_free(tree->priv->search, TRUE);
		tree->priv->search = NULL;
		tree->priv->search_timeout = 0;
		GNT_WIDGET_UNSET_FLAGS(GNT_WIDGET(tree), GNT_WIDGET_DISABLE_ACTIONS);
	}
}

static gboolean
search_timeout(gpointer data)
{
	GntTree *tree = data;

	end_search(tree);
	redraw_tree(tree);

	return FALSE;
}

static gboolean
gnt_tree_key_pressed(GntWidget *widget, const char *text)
{
	GntTree *tree = GNT_TREE(widget);
	GntTreeRow *old = tree->current;

	if (text[0] == '\r' || text[0] == '\n') {
		end_search(tree);
		gnt_widget_activate(widget);
	} else if (tree->priv->search) {
		gboolean changed = TRUE;
		if (isalnum(*text)) {
			tree->priv->search = g_string_append_c(tree->priv->search, *text);
		} else if (g_utf8_collate(text, GNT_KEY_BACKSPACE) == 0) {
			if (tree->priv->search->len)
				tree->priv->search->str[--tree->priv->search->len] = '\0';
		} else
			changed = FALSE;
		if (changed) {
			redraw_tree(tree);
		} else {
			gnt_bindable_perform_action_key(GNT_BINDABLE(tree), text);
		}
		g_source_remove(tree->priv->search_timeout);
		tree->priv->search_timeout = g_timeout_add(SEARCH_TIMEOUT, search_timeout, tree);
		return TRUE;
	} else if (text[0] == ' ' && text[1] == 0) {
		/* Space pressed */
		GntTreeRow *row = tree->current;
		if (row && row->child)
		{
			row->collapsed = !row->collapsed;
			redraw_tree(tree);
			g_signal_emit(tree, signals[SIG_COLLAPSED], 0, row->key, row->collapsed);
		}
		else if (row && row->choice)
		{
			row->isselected = !row->isselected;
			g_signal_emit(tree, signals[SIG_TOGGLED], 0, row->key);
			redraw_tree(tree);
		}
	} else {
		return FALSE;
	}

	if (old != tree->current)
	{
		tree_selection_changed(tree, old, tree->current);
	}

	return TRUE;
}

static void
gnt_tree_free_columns(GntTree *tree)
{
	int i;
	for (i = 0; i < tree->ncol; i++) {
		g_free(tree->columns[i].title);
	}
	g_free(tree->columns);
}

static void
gnt_tree_destroy(GntWidget *widget)
{
	GntTree *tree = GNT_TREE(widget);

	end_search(tree);
	if (tree->hash)
		g_hash_table_destroy(tree->hash);
	g_list_free(tree->list);
	gnt_tree_free_columns(tree);
	g_free(tree->priv);
}

static gboolean
gnt_tree_clicked(GntWidget *widget, GntMouseEvent event, int x, int y)
{
	GntTree *tree = GNT_TREE(widget);
	GntTreeRow *old = tree->current;
	if (event == GNT_MOUSE_SCROLL_UP) {
		action_up(GNT_BINDABLE(widget), NULL);
	} else if (event == GNT_MOUSE_SCROLL_DOWN) {
		action_down(GNT_BINDABLE(widget), NULL);
	} else if (event == GNT_LEFT_MOUSE_DOWN) {
		GntTreeRow *row;
		GntTree *tree = GNT_TREE(widget);
		int pos = 1;
		if (GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_NO_BORDER))
			pos = 0;
		if (tree->show_title)
			pos += 2;
		pos = y - widget->priv.y - pos;
		row = get_next_n(tree->top, pos);
		if (row && tree->current != row) {
			GntTreeRow *old = tree->current;
			tree->current = row;
			redraw_tree(tree);
			tree_selection_changed(tree, old, tree->current);
		} else if (row && row == tree->current) {
			if (row->choice) {
				row->isselected = !row->isselected;
				g_signal_emit(tree, signals[SIG_TOGGLED], 0, row->key);
				redraw_tree(tree);
			} else {
				gnt_widget_activate(widget);
			}
		}
	} else {
		return FALSE;
	}
	if (old != tree->current) {
		tree_selection_changed(tree, old, tree->current);
	}
	return TRUE;
}

static void
gnt_tree_size_changed(GntWidget *widget, int w, int h)
{
	GntTree *tree = GNT_TREE(widget);
	if (widget->priv.width <= 0)
		return;

	readjust_columns(tree);
}

static gboolean
start_search(GntBindable *bindable, GList *list)
{
	GntTree *tree = GNT_TREE(bindable);
	if (tree->priv->search)
		return FALSE;
	GNT_WIDGET_SET_FLAGS(GNT_WIDGET(tree), GNT_WIDGET_DISABLE_ACTIONS);
	tree->priv->search = g_string_new(NULL);
	tree->priv->search_timeout = g_timeout_add(SEARCH_TIMEOUT, search_timeout, tree);
	return TRUE;
}

static gboolean
end_search_action(GntBindable *bindable, GList *list)
{
	GntTree *tree = GNT_TREE(bindable);
	if (tree->priv->search == NULL)
		return FALSE;
	GNT_WIDGET_UNSET_FLAGS(GNT_WIDGET(tree), GNT_WIDGET_DISABLE_ACTIONS);
	end_search(tree);
	redraw_tree(tree);
	return TRUE;
}

static void
gnt_tree_set_property(GObject *obj, guint prop_id, const GValue *value,
		GParamSpec *spec)
{
	GntTree *tree = GNT_TREE(obj);
	switch (prop_id) {
		case PROP_COLUMNS:
			_gnt_tree_init_internals(tree, g_value_get_int(value));
			break;
		case PROP_EXPANDER:
			if (tree->priv->expander_level == g_value_get_int(value))
				break;
			tree->priv->expander_level = g_value_get_int(value);
			g_object_notify(obj, "expander-level");
		default:
			break;
	}
}

static void
gnt_tree_get_property(GObject *obj, guint prop_id, GValue *value,
		GParamSpec *spec)
{
	GntTree *tree = GNT_TREE(obj);
	switch (prop_id) {
		case PROP_COLUMNS:
			g_value_set_int(value, tree->ncol);
			break;
		case PROP_EXPANDER:
			g_value_set_int(value, tree->priv->expander_level);
			break;
		default:
			break;
	}
}

static void
gnt_tree_class_init(GntTreeClass *klass)
{
	GntBindableClass *bindable = GNT_BINDABLE_CLASS(klass);
	GObjectClass *gclass = G_OBJECT_CLASS(klass);

	parent_class = GNT_WIDGET_CLASS(klass);
	parent_class->destroy = gnt_tree_destroy;
	parent_class->draw = gnt_tree_draw;
	parent_class->map = gnt_tree_map;
	parent_class->size_request = gnt_tree_size_request;
	parent_class->key_pressed = gnt_tree_key_pressed;
	parent_class->clicked = gnt_tree_clicked;
	parent_class->size_changed = gnt_tree_size_changed;

	gclass->set_property = gnt_tree_set_property;
	gclass->get_property = gnt_tree_get_property;
	g_object_class_install_property(gclass,
			PROP_COLUMNS,
			g_param_spec_int("columns", "Columns",
				"Number of columns in the tree.",
				1, G_MAXINT, 1,
				G_PARAM_READWRITE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
			)
		);
	g_object_class_install_property(gclass,
			PROP_EXPANDER,
			g_param_spec_int("expander-level", "Expander level",
				"Number of levels to show expander in the tree.",
				0, G_MAXINT, 1,
				G_PARAM_READWRITE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
			)
		);

	signals[SIG_SELECTION_CHANGED] = 
		g_signal_new("selection-changed",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET(GntTreeClass, selection_changed),
					 NULL, NULL,
					 gnt_closure_marshal_VOID__POINTER_POINTER,
					 G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER);
	signals[SIG_SCROLLED] = 
		g_signal_new("scrolled",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 0,
					 NULL, NULL,
					 g_cclosure_marshal_VOID__INT,
					 G_TYPE_NONE, 1, G_TYPE_INT);
	signals[SIG_TOGGLED] = 
		g_signal_new("toggled",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET(GntTreeClass, toggled),
					 NULL, NULL,
					 g_cclosure_marshal_VOID__POINTER,
					 G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals[SIG_COLLAPSED] = 
		g_signal_new("collapse-toggled",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 0,
					 NULL, NULL,
					 gnt_closure_marshal_VOID__POINTER_BOOLEAN,
					 G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_BOOLEAN);

	gnt_bindable_class_register_action(bindable, "move-up", action_up,
				GNT_KEY_UP, NULL);
	gnt_bindable_register_binding(bindable, "move-up", GNT_KEY_CTRL_P, NULL);
	gnt_bindable_class_register_action(bindable, "move-down", action_down,
				GNT_KEY_DOWN, NULL);
	gnt_bindable_register_binding(bindable, "move-down", GNT_KEY_CTRL_N, NULL);
	gnt_bindable_class_register_action(bindable, "move-parent", action_move_parent,
				GNT_KEY_BACKSPACE, NULL);
	gnt_bindable_class_register_action(bindable, "page-up", action_page_up,
				GNT_KEY_PGUP, NULL);
	gnt_bindable_class_register_action(bindable, "page-down", action_page_down,
				GNT_KEY_PGDOWN, NULL);
	gnt_bindable_class_register_action(bindable, "start-search", start_search,
				"/", NULL);
	gnt_bindable_class_register_action(bindable, "end-search", end_search_action,
				"\033", NULL);

	gnt_style_read_actions(G_OBJECT_CLASS_TYPE(klass), bindable);
	GNTDEBUG;
}

static void
gnt_tree_init(GTypeInstance *instance, gpointer class)
{
	GntWidget *widget = GNT_WIDGET(instance);
	GntTree *tree = GNT_TREE(widget);
	tree->show_separator = TRUE;
	tree->priv = g_new0(GntTreePriv, 1);
	GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_GROW_X | GNT_WIDGET_GROW_Y |
			GNT_WIDGET_CAN_TAKE_FOCUS | GNT_WIDGET_NO_SHADOW);
	gnt_widget_set_take_focus(widget, TRUE);
	widget->priv.minw = 4;
	widget->priv.minh = 1;
	GNTDEBUG;
}

/******************************************************************************
 * GntTree API
 *****************************************************************************/
GType
gnt_tree_get_gtype(void)
{
	static GType type = 0;

	if(type == 0)
	{
		static const GTypeInfo info = {
			sizeof(GntTreeClass),
			NULL,					/* base_init		*/
			NULL,					/* base_finalize	*/
			(GClassInitFunc)gnt_tree_class_init,
			NULL,					/* class_finalize	*/
			NULL,					/* class_data		*/
			sizeof(GntTree),
			0,						/* n_preallocs		*/
			gnt_tree_init,			/* instance_init	*/
			NULL					/* value_table		*/
		};

		type = g_type_register_static(GNT_TYPE_WIDGET,
									  "GntTree",
									  &info, 0);
	}

	return type;
}

static void
free_tree_col(gpointer data)
{
	GntTreeCol *col = data;
	if (!col->isbinary)
		g_free(col->text);
	g_free(col);
}

static void
free_tree_row(gpointer data)
{
	GntTreeRow *row = data;

	if (!row)
		return;

	g_list_foreach(row->columns, (GFunc)free_tree_col, NULL);
	g_list_free(row->columns);
	g_free(row);
}

GntWidget *gnt_tree_new()
{
	return gnt_tree_new_with_columns(1);
}

void gnt_tree_set_visible_rows(GntTree *tree, int rows)
{
	GntWidget *widget = GNT_WIDGET(tree);
	widget->priv.height = rows;
	if (!GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_NO_BORDER))
		widget->priv.height += 2;
}

int gnt_tree_get_visible_rows(GntTree *tree)
{
	GntWidget *widget = GNT_WIDGET(tree);
	int ret = widget->priv.height;
	if (!GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_NO_BORDER))
		ret -= 2;
	return ret;
}

GList *gnt_tree_get_rows(GntTree *tree)
{
	return tree->list;
}

void gnt_tree_scroll(GntTree *tree, int count)
{
	GntTreeRow *row;

	if (count < 0)
	{
		if (get_root_distance(tree->top) == 0)
			return;
		row = get_prev_n(tree->top, -count);
		if (row == NULL)
			row = tree->root;
		tree->top = row;
	}
	else
	{
		get_next_n_opt(tree->bottom, count, &count);
		tree->top = get_next_n(tree->top, count);
	}

	redraw_tree(tree);
	g_signal_emit(tree, signals[SIG_SCROLLED], 0, count);
}

static gpointer
find_position(GntTree *tree, gpointer key, gpointer parent)
{
	GntTreeRow *row;

	if (tree->priv->compare == NULL)
		return NULL;

	if (parent == NULL)
		row = tree->root;
	else
		row = g_hash_table_lookup(tree->hash, parent);

	if (!row)
		return NULL;

	if (parent)
		row = row->child;

	while (row)
	{
		if (tree->priv->compare(key, row->key) < 0)
			return (row->prev ? row->prev->key : NULL);
		if (row->next)
			row = row->next;
		else
			return row->key;
	}
	return NULL;
}

void gnt_tree_sort_row(GntTree *tree, gpointer key)
{
	GntTreeRow *row, *q, *s;
	int current, newp;

	if (!tree->priv->compare)
		return;

	row = g_hash_table_lookup(tree->hash, key);
	g_return_if_fail(row != NULL);

	current = g_list_index(tree->list, key);

	if (row->parent)
		s = row->parent->child;
	else
		s = tree->root;

	q = NULL;
	while (s) {
		if (tree->priv->compare(row->key, s->key) < 0)
			break;
		q = s;
		s = s->next;
	}

	/* Move row between q and s */
	if (row == q || row == s)
		return;

	if (q == NULL) {
		/* row becomes the first child of its parent */
		row->prev->next = row->next;  /* row->prev cannot be NULL at this point */
		if (row->next)
			row->next->prev = row->prev;
		if (row->parent)
			row->parent->child = row;
		else
			tree->root = row;
		row->next = s;
		s->prev = row;  /* s cannot be NULL */
		row->prev = NULL;
		newp = g_list_index(tree->list, s) - 1;
	} else {
		if (row->prev) {
			row->prev->next = row->next;
		} else {
			/* row was the first child of its parent */
			if (row->parent)
				row->parent->child = row->next;
			else
				tree->top = row->next;
		}

		if (row->next)
			row->next->prev = row->prev;

		q->next = row;
		row->prev = q;
		if (s)
			s->prev = row;
		row->next = s;
		newp = g_list_index(tree->list, q) + 1;
	}
	tree->list = g_list_reposition_child(tree->list, current, newp);

	redraw_tree(tree);
}

GntTreeRow *gnt_tree_add_row_after(GntTree *tree, void *key, GntTreeRow *row, void *parent, void *bigbro)
{
	GntTreeRow *pr = NULL;

	row->tree = tree;
	row->key = key;
	row->data = NULL;
	g_hash_table_replace(tree->hash, key, row);

	if (bigbro == NULL && tree->priv->compare)
	{
		bigbro = find_position(tree, key, parent);
	}

	if (tree->root == NULL)
	{
		tree->root = row;
		tree->list = g_list_prepend(tree->list, key);
	}
	else
	{
		int position = 0;

		if (bigbro)
		{
			pr = g_hash_table_lookup(tree->hash, bigbro);
			if (pr)
			{
				if (pr->next)	pr->next->prev = row;
				row->next = pr->next;
				row->prev = pr;
				pr->next = row;
				row->parent = pr->parent;

				position = g_list_index(tree->list, bigbro);
			}
		}

		if (pr == NULL && parent)	
		{
			pr = g_hash_table_lookup(tree->hash, parent);
			if (pr)
			{
				if (pr->child)	pr->child->prev = row;
				row->next = pr->child;
				pr->child = row;
				row->parent = pr;

				position = g_list_index(tree->list, parent);
			}
		}

		if (pr == NULL)
		{
			GntTreeRow *r = tree->root;
			row->next = r;
			if (r) r->prev = row;
			if (tree->current == tree->root)
				tree->current = row;
			tree->root = row;
			tree->list = g_list_prepend(tree->list, key);
		}
		else
		{
			tree->list = g_list_insert(tree->list, key, position + 1);
		}
	}
	redraw_tree(tree);

	return row;
}

GntTreeRow *gnt_tree_add_row_last(GntTree *tree, void *key, GntTreeRow *row, void *parent)
{
	GntTreeRow *pr = NULL, *br = NULL;

	if (parent)
		pr = g_hash_table_lookup(tree->hash, parent);

	if (pr)
		br = pr->child;
	else
		br = tree->root;

	if (br)
	{
		while (br->next)
			br = br->next;
	}

	return gnt_tree_add_row_after(tree, key, row, parent, br ? br->key : NULL);
}

gpointer gnt_tree_get_selection_data(GntTree *tree)
{
	if (tree->current)
		return tree->current->key;	/* XXX: perhaps we should just get rid of 'data' */
	return NULL;
}

char *gnt_tree_get_selection_text(GntTree *tree)
{
	if (tree->current)
		return update_row_text(tree, tree->current);
	return NULL;
}

GList *gnt_tree_get_row_text_list(GntTree *tree, gpointer key)
{
	GList *list = NULL, *iter;
	GntTreeRow *row = key ? g_hash_table_lookup(tree->hash, key) : tree->current;
	int i;

	if (!row)
		return NULL;

	for (i = 0, iter = row->columns; i < tree->ncol && iter;
			i++, iter = iter->next)
	{
		GntTreeCol *col = iter->data;
		list = g_list_append(list, BINARY_DATA(tree, i) ? col->text : g_strdup(col->text));
	}

	return list;
}

GList *gnt_tree_get_selection_text_list(GntTree *tree)
{
	return gnt_tree_get_row_text_list(tree, NULL);
}

void gnt_tree_remove(GntTree *tree, gpointer key)
{
	GntTreeRow *row = g_hash_table_lookup(tree->hash, key);
	static int depth = 0; /* Only redraw after all child nodes are removed */
	if (row)
	{
		gboolean redraw = FALSE;

		if (row->child) {
			depth++;
			while (row->child) {
				gnt_tree_remove(tree, row->child->key);
			}
			depth--;
		}

		if (get_distance(tree->top, row) >= 0 && get_distance(row, tree->bottom) >= 0)
			redraw = TRUE;

		/* Update root/top/current/bottom if necessary */
		if (tree->root == row)
			tree->root = get_next(row);
		if (tree->top == row)
		{
			if (tree->top != tree->root)
				tree->top = get_prev(row);
			else
				tree->top = get_next(row);
		}
		if (tree->current == row)
		{
			if (tree->current != tree->root)
				tree->current = get_prev(row);
			else
				tree->current = get_next(row);
			tree_selection_changed(tree, row, tree->current);
		}
		if (tree->bottom == row)
		{
			tree->bottom = get_prev(row);
		}

		/* Fix the links */
		if (row->next)
			row->next->prev = row->prev;
		if (row->parent && row->parent->child == row)
			row->parent->child = row->next;
		if (row->prev)
			row->prev->next = row->next;

		g_hash_table_remove(tree->hash, key);
		tree->list = g_list_remove(tree->list, key);

		if (redraw && depth == 0)
		{
			redraw_tree(tree);
		}
	}
}

static gboolean
return_true(gpointer key, gpointer data, gpointer null)
{
	return TRUE;
}

void gnt_tree_remove_all(GntTree *tree)
{
	tree->root = NULL;
	g_hash_table_foreach_remove(tree->hash, (GHRFunc)return_true, tree);
	g_list_free(tree->list);
	tree->list = NULL;
	tree->current = tree->top = tree->bottom = NULL;
}

int gnt_tree_get_selection_visible_line(GntTree *tree)
{
	return get_distance(tree->top, tree->current) +
			!!(GNT_WIDGET_IS_FLAG_SET(GNT_WIDGET(tree), GNT_WIDGET_NO_BORDER));
}

void gnt_tree_change_text(GntTree *tree, gpointer key, int colno, const char *text)
{
	GntTreeRow *row;
	GntTreeCol *col;

	g_return_if_fail(colno < tree->ncol);
	
	row = g_hash_table_lookup(tree->hash, key);
	if (row)
	{
		col = g_list_nth_data(row->columns, colno);
		if (BINARY_DATA(tree, colno)) {
			col->text = (gpointer)text;
		} else {
			g_free(col->text);
			col->text = g_strdup(text ? text : "");
		}

		if (GNT_WIDGET_IS_FLAG_SET(GNT_WIDGET(tree), GNT_WIDGET_MAPPED) &&
			get_distance(tree->top, row) >= 0 && get_distance(row, tree->bottom) >= 0)
			redraw_tree(tree);
	}
}

GntTreeRow *gnt_tree_add_choice(GntTree *tree, void *key, GntTreeRow *row, void *parent, void *bigbro)
{
	GntTreeRow *r;
	r = g_hash_table_lookup(tree->hash, key);
	g_return_val_if_fail(!r || !r->choice, NULL);

	if (bigbro == NULL) {
		if (tree->priv->compare)
			bigbro = find_position(tree, key, parent);
		else {
			r = g_hash_table_lookup(tree->hash, parent);
			if (!r)
				r = tree->root;
			else
				r = r->child;
			if (r) {
				while (r->next)
					r = r->next;
				bigbro = r->key;
			} 
		}
	}
	row = gnt_tree_add_row_after(tree, key, row, parent, bigbro);
	row->choice = TRUE;

	return row;
}

void gnt_tree_set_choice(GntTree *tree, void *key, gboolean set)
{
	GntTreeRow *row = g_hash_table_lookup(tree->hash, key);

	if (!row)
		return;
	g_return_if_fail(row->choice);

	row->isselected = set;
	redraw_tree(tree);
}

gboolean gnt_tree_get_choice(GntTree *tree, void *key)
{
	GntTreeRow *row = g_hash_table_lookup(tree->hash, key);

	if (!row)
		return FALSE;
	g_return_val_if_fail(row->choice, FALSE);

	return row->isselected;
}

void gnt_tree_set_row_flags(GntTree *tree, void *key, GntTextFormatFlags flags)
{
	GntTreeRow *row = g_hash_table_lookup(tree->hash, key);
	if (!row || row->flags == flags)
		return;

	row->flags = flags;
	redraw_tree(tree);	/* XXX: It shouldn't be necessary to redraw the whole darned tree */
}

void gnt_tree_set_row_color(GntTree *tree, void *key, int color)
{
	GntTreeRow *row = g_hash_table_lookup(tree->hash, key);
	if (!row || row->color == color)
		return;

	row->color = color;
	redraw_tree(tree);
}

void gnt_tree_set_selected(GntTree *tree , void *key)
{
	int dist;
	GntTreeRow *row = g_hash_table_lookup(tree->hash, key);
	if (!row || row == tree->current)
		return;

	if (tree->top == NULL)
		tree->top = row;
	if (tree->bottom == NULL)
		tree->bottom = row;

	tree->current = row;
	if ((dist = get_distance(tree->current, tree->bottom)) < 0)
		gnt_tree_scroll(tree, -dist);
	else if ((dist = get_distance(tree->current, tree->top)) > 0)
		gnt_tree_scroll(tree, -dist);
	else
		redraw_tree(tree);
	tree_selection_changed(tree, row, tree->current);
}

static void _gnt_tree_init_internals(GntTree *tree, int col)
{
	gnt_tree_free_columns(tree);

	tree->ncol = col;
	tree->hash = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, free_tree_row);
	tree->columns = g_new0(struct _GntTreeColInfo, col);
	tree->priv->lastvisible = col - 1;
	while (col--)
	{
		tree->columns[col].width = 15;
	}
	tree->list = NULL;
	tree->show_title = FALSE;
	g_object_notify(G_OBJECT(tree), "columns");
}

GntWidget *gnt_tree_new_with_columns(int col)
{
	GntWidget *widget = g_object_new(GNT_TYPE_TREE,
			"columns", col,
			"expander-level", 1,
			NULL);

	return widget;
}

GntTreeRow *gnt_tree_create_row_from_list(GntTree *tree, GList *list)
{
	GList *iter;
	int i;
	GntTreeRow *row = g_new0(GntTreeRow, 1);

	for (i = 0, iter = list; i < tree->ncol && iter; iter = iter->next, i++)
	{
		GntTreeCol *col = g_new0(GntTreeCol, 1);
		col->span = 1;
		if (BINARY_DATA(tree, i)) {
			col->text = iter->data;
			col->isbinary = TRUE;
		} else {
			col->text = g_strdup(iter->data ? iter->data : "");
			col->isbinary = FALSE;
		}

		row->columns = g_list_append(row->columns, col);
	}

	return row;
}

GntTreeRow *gnt_tree_create_row(GntTree *tree, ...)
{
	int i;
	va_list args;
	GList *list = NULL;
	GntTreeRow *row;

	va_start(args, tree);
	for (i = 0; i < tree->ncol; i++)
	{
		list = g_list_append(list, va_arg(args, char *));
	}
	va_end(args);

	row = gnt_tree_create_row_from_list(tree, list);
	g_list_free(list);

	return row;
}

void gnt_tree_set_col_width(GntTree *tree, int col, int width)
{
	g_return_if_fail(col < tree->ncol);

	tree->columns[col].width = width;
	if (tree->columns[col].width_ratio == 0)
		tree->columns[col].width_ratio = width;
}

void gnt_tree_set_column_title(GntTree *tree, int index, const char *title)
{
	g_free(tree->columns[index].title);
	tree->columns[index].title = g_strdup(title);
}

void gnt_tree_set_column_titles(GntTree *tree, ...)
{
	int i;
	va_list args;

	va_start(args, tree);
	for (i = 0; i < tree->ncol; i++)
	{
		const char *title = va_arg(args, const char *);
		tree->columns[i].title = g_strdup(title);
	}
	va_end(args);
}

void gnt_tree_set_show_title(GntTree *tree, gboolean set)
{
	tree->show_title = set;
	GNT_WIDGET(tree)->priv.minh = (set ? 6 : 4);
}

void gnt_tree_set_compare_func(GntTree *tree, GCompareFunc func)
{
	tree->priv->compare = func;
}

void gnt_tree_set_expanded(GntTree *tree, void *key, gboolean expanded)
{
	GntTreeRow *row = g_hash_table_lookup(tree->hash, key);
	if (row) {
		row->collapsed = !expanded;
		if (GNT_WIDGET(tree)->window)
			gnt_widget_draw(GNT_WIDGET(tree));
		g_signal_emit(tree, signals[SIG_COLLAPSED], 0, key, row->collapsed);
	}
}

void gnt_tree_set_show_separator(GntTree *tree, gboolean set)
{
	tree->show_separator = set;
}

void gnt_tree_adjust_columns(GntTree *tree)
{
	GntTreeRow *row = tree->root;
	int *widths, i, twidth;

	widths = g_new0(int, tree->ncol);
	while (row) {
		GList *iter;
		for (i = 0, iter = row->columns; iter; iter = iter->next, i++) {
			GntTreeCol *col = iter->data;
			int w = gnt_util_onscreen_width(col->text, NULL);
			if (i == 0 && row->choice)
				w += 4;
			if (i == 0) {
				w += find_depth(row) * TAB_SIZE;
			}
			if (widths[i] < w)
				widths[i] = w;
		}
		row = get_next(row);
	}

	twidth = 1 + 2 * (!GNT_WIDGET_IS_FLAG_SET(GNT_WIDGET(tree), GNT_WIDGET_NO_BORDER));
	for (i = 0; i < tree->ncol; i++) {
		if (tree->columns[i].flags & GNT_TREE_COLUMN_FIXED_SIZE)
			widths[i] = tree->columns[i].width;
		gnt_tree_set_col_width(tree, i, widths[i]);
		if (!COLUMN_INVISIBLE(tree, i)) {
			twidth = twidth + widths[i];
			if (tree->priv->lastvisible != i)
				twidth += 1;
		}
	}
	g_free(widths);

	gnt_widget_set_size(GNT_WIDGET(tree), twidth, -1);
}

void gnt_tree_set_hash_fns(GntTree *tree, gpointer hash, gpointer eq, gpointer kd)
{
	g_hash_table_foreach_remove(tree->hash, return_true, NULL);
	g_hash_table_destroy(tree->hash);
	tree->hash = g_hash_table_new_full(hash, eq, kd, free_tree_row);
}

static void
set_column_flag(GntTree *tree, int col, GntTreeColumnFlag flag, gboolean set)
{
	if (set)
		tree->columns[col].flags |= flag;
	else
		tree->columns[col].flags &= ~flag;
}

void gnt_tree_set_column_visible(GntTree *tree, int col, gboolean vis)
{
	g_return_if_fail(col < tree->ncol);
	set_column_flag(tree, col, GNT_TREE_COLUMN_INVISIBLE, !vis);
	if (vis) {
		/* the column is visible */
		if (tree->priv->lastvisible < col)
			tree->priv->lastvisible = col;
	} else {
		if (tree->priv->lastvisible == col)
			while (tree->priv->lastvisible) {
				tree->priv->lastvisible--;
				if (!COLUMN_INVISIBLE(tree, tree->priv->lastvisible))
					break;
			}
	}
	if (GNT_WIDGET_IS_FLAG_SET(GNT_WIDGET(tree), GNT_WIDGET_MAPPED))
		readjust_columns(tree);
}

void gnt_tree_set_column_resizable(GntTree *tree, int col, gboolean res)
{
	g_return_if_fail(col < tree->ncol);
	set_column_flag(tree, col, GNT_TREE_COLUMN_FIXED_SIZE, !res);
}

void gnt_tree_set_column_is_binary(GntTree *tree, int col, gboolean bin)
{
	g_return_if_fail(col < tree->ncol);
	set_column_flag(tree, col, GNT_TREE_COLUMN_FIXED_SIZE, bin);
}

void gnt_tree_set_column_is_right_aligned(GntTree *tree, int col, gboolean right)
{
	g_return_if_fail(col < tree->ncol);
	set_column_flag(tree, col, GNT_TREE_COLUMN_RIGHT_ALIGNED, right);
}

void gnt_tree_set_column_width_ratio(GntTree *tree, int cols[])
{
	int i;
	for (i = 0; i < tree->ncol && cols[i]; i++) {
		tree->columns[i].width_ratio = cols[i];
	}
}

void gnt_tree_set_search_column(GntTree *tree, int col)
{
	g_return_if_fail(col < tree->ncol);
	g_return_if_fail(!BINARY_DATA(tree, col));
	tree->priv->search_column = col;
}

gboolean gnt_tree_is_searching(GntTree *tree)
{
	return (tree->priv->search != NULL);
}

void gnt_tree_set_search_function(GntTree *tree,
		gboolean (*func)(GntTree *tree, gpointer key, const char *search, const char *current))
{
	tree->priv->search_func = func;
}

gpointer gnt_tree_get_parent_key(GntTree *tree, gpointer key)
{
	GntTreeRow *row = g_hash_table_lookup(tree->hash, key);
	return (row && row->parent) ? row->parent->key : NULL;
}

