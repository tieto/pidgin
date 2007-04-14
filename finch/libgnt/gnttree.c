#include "gntmarshal.h"
#include "gntstyle.h"
#include "gnttree.h"
#include "gntutils.h"

#include <string.h>
#include <ctype.h>

#define SEARCH_TIMEOUT 4000   /* 4 secs */

enum
{
	SIG_SELECTION_CHANGED,
	SIG_SCROLLED,
	SIG_TOGGLED,
	SIG_COLLAPSED,
	SIGS,
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
	int span;       /* How many columns does it span? */
};

static GntWidgetClass *parent_class = NULL;
static guint signals[SIGS] = { 0 };

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
	if (t->search && t->search->len > 0) {
		char *one = g_utf8_casefold(((GntTreeCol*)row->columns->data)->text, -1);
		char *two = g_utf8_casefold(t->search->str, -1);
		char *z = strstr(one, two);
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
	if (!row->collapsed && row->child)
		row = get_last_child(row->child);
	return row;
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
	int lastvisible = tree->ncol;

	while (lastvisible && tree->columns[lastvisible].invisible)
		lastvisible--;

	for (i = 0, iter = row->columns; i < tree->ncol && iter; i++, iter = iter->next)
	{
		GntTreeCol *col = iter->data;
		const char *text;
		int len = gnt_util_onscreen_width(col->text, NULL);
		int fl = 0;
		gboolean cut = FALSE;
		int width;

		if (tree->columns[i].invisible)
			continue;

		if (i == lastvisible)
			width = GNT_WIDGET(tree)->priv.width - gnt_util_onscreen_width(string->str, NULL);
		else
			width = tree->columns[i].width;

		if (i == 0)
		{
			if (row->choice)
			{
				g_string_append_printf(string, "[%c] ",
						row->isselected ? 'X' : ' ');
				fl = 4;
			}
			else if (row->parent == NULL && row->child)
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
		}
		else if (notfirst)
			g_string_append_c(string, '|');
		else
			g_string_append_c(string, ' ');

		notfirst = TRUE;

		if (len > width) {
			len = width - 1;
			cut = TRUE;
		}
		text = gnt_util_onscreen_width_to_pointer(col->text, len - fl, NULL);
		string = g_string_append_len(string, col->text, text - col->text);
		if (cut) { /* ellipsis */
			if (gnt_ascii_only())
				g_string_append_c(string, '~');
			else
				string = g_string_append(string, "\342\200\246");
			len++;
		}

		if (len < tree->columns[i].width && iter->next)
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
		if (!tree->columns[i].invisible) {
			notfirst = TRUE;
			NEXT_X;
		}
		if (!tree->columns[i+1].invisible && notfirst)
			mvwaddch(widget->window, y, x, type);
	}
}

static void
redraw_tree(GntTree *tree)
{
	int start, i;
	GntWidget *widget = GNT_WIDGET(tree);
	GntTreeRow *row;
	int pos, up, down;
	int rows, scrcol;

	if (!GNT_WIDGET_IS_FLAG_SET(GNT_WIDGET(tree), GNT_WIDGET_MAPPED))
		return;

	if (GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_NO_BORDER))
		pos = 0;
	else
		pos = 1;

	if (tree->top == NULL)
		tree->top = tree->root;
	if (tree->current == NULL)
		tree->current = tree->root;

	wbkgd(widget->window, COLOR_PAIR(GNT_COLOR_NORMAL));

	start = 0;
	if (tree->show_title)
	{
		int i;
		int x = pos;

		mvwhline(widget->window, pos + 1, pos, ACS_HLINE | COLOR_PAIR(GNT_COLOR_NORMAL),
				widget->priv.width - pos - 1);
		mvwhline(widget->window, pos, pos, ' ' | COLOR_PAIR(GNT_COLOR_NORMAL),
				widget->priv.width - pos - 1);

		for (i = 0; i < tree->ncol; i++)
		{
			if (tree->columns[i].invisible) {
				continue;
			}
			mvwaddstr(widget->window, pos, x + 1, tree->columns[i].title);
			NEXT_X;
		}
		if (pos)
		{
			tree_mark_columns(tree, pos, 0, ACS_TTEE | COLOR_PAIR(GNT_COLOR_NORMAL));
			tree_mark_columns(tree, pos, widget->priv.height - pos,
					ACS_BTEE | COLOR_PAIR(GNT_COLOR_NORMAL));
		}
		tree_mark_columns(tree, pos, pos + 1,
			(tree->show_separator ? ACS_PLUS : ACS_HLINE) | COLOR_PAIR(GNT_COLOR_NORMAL));
		tree_mark_columns(tree, pos, pos,
			(tree->show_separator ? ACS_VLINE : ' ') | COLOR_PAIR(GNT_COLOR_NORMAL));
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
				attr |= COLOR_PAIR(GNT_COLOR_HIGHLIGHT);
			else
				attr |= COLOR_PAIR(GNT_COLOR_HIGHLIGHT_D);
		}
		else
		{
			if (flags & GNT_TEXT_FLAG_DIM)
				attr |= (A_DIM | COLOR_PAIR(GNT_COLOR_DISABLED));
			else if (flags & GNT_TEXT_FLAG_HIGHLIGHT)
				attr |= (A_DIM | COLOR_PAIR(GNT_COLOR_HIGHLIGHT));
			else
				attr |= COLOR_PAIR(GNT_COLOR_NORMAL);
		}

		wbkgdset(widget->window, '\0' | attr);
		mvwaddstr(widget->window, i, pos, str);
		whline(widget->window, ' ', scrcol - wr);
		tree->bottom = row;
		g_free(str);
		tree_mark_columns(tree, pos, i,
			(tree->show_separator ? ACS_VLINE : ' ') | attr);
	}

	wbkgdset(widget->window, '\0' | COLOR_PAIR(GNT_COLOR_NORMAL));
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
		int total;
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
				' ' | COLOR_PAIR(GNT_COLOR_NORMAL), rows);
		mvwvline(widget->window, position, scrcol,
				ACS_CKBOARD | COLOR_PAIR(GNT_COLOR_HIGHLIGHT_D), showing);
	}

	mvwaddch(widget->window, start + pos, scrcol,
			((tree->top != tree->root) ?  ACS_UARROW : ' ') |
				COLOR_PAIR(GNT_COLOR_HIGHLIGHT_D));

	mvwaddch(widget->window, widget->priv.height - pos - 1, scrcol,
			(row ?  ACS_DARROW : ' ') | COLOR_PAIR(GNT_COLOR_HIGHLIGHT_D));

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
		for (i = 0; i < tree->ncol; i++)
			if (!tree->columns[i].invisible)
				width += tree->columns[i].width + 1;
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
	if (row->parent) {
		int dist;
		tree->current = row->parent;
		if ((dist = get_distance(tree->current, tree->top)) > 0)
			gnt_tree_scroll(tree, -dist);
		else
			redraw_tree(tree);
		tree_selection_changed(tree, row, tree->current);
	}
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
	if (tree->search) {
		g_source_remove(tree->search_timeout);
		g_string_free(tree->search, TRUE);
		tree->search = NULL;
		tree->search_timeout = 0;
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

	if (text[0] == '\r') {
		end_search(tree);
		gnt_widget_activate(widget);
	} else if (tree->search) {
		if (isalnum(*text)) {
			tree->search = g_string_append_c(tree->search, *text);
			redraw_tree(tree);
			g_source_remove(tree->search_timeout);
			tree->search_timeout = g_timeout_add(SEARCH_TIMEOUT, search_timeout, tree);
		}
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
	}

	if (old != tree->current)
	{
		tree_selection_changed(tree, old, tree->current);
		return TRUE;
	}

	return FALSE;
}

static void
gnt_tree_destroy(GntWidget *widget)
{
	GntTree *tree = GNT_TREE(widget);
	int i;

	end_search(tree);
	if (tree->hash)
		g_hash_table_destroy(tree->hash);
	g_list_free(tree->list);

	for (i = 0; i < tree->ncol; i++)
	{
		g_free(tree->columns[i].title);
	}
	g_free(tree->columns);
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
	int i;
	int n = 0;
	if (widget->priv.width <= 0)
		return;
	for (i = 0; i < tree->ncol; ++i)
		n += tree->columns[i].width;
	if (GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_NO_BORDER))
		tree->columns[tree->ncol - 1].width += widget->priv.width - n - 1 * tree->ncol;
	else
		tree->columns[tree->ncol - 1].width += widget->priv.width - n - 2 - 1 * tree->ncol;
}

static gboolean
start_search(GntBindable *bindable, GList *list)
{
	GntTree *tree = GNT_TREE(bindable);
	if (tree->search)
		return FALSE;
	tree->search = g_string_new(NULL);
	tree->search_timeout = g_timeout_add(SEARCH_TIMEOUT, search_timeout, tree);
	return TRUE;
}

static gboolean
end_search_action(GntBindable *bindable, GList *list)
{
	GntTree *tree = GNT_TREE(bindable);
	if (tree->search == NULL)
		return FALSE;
	end_search(tree);
	redraw_tree(tree);
	return TRUE;
}

static void
gnt_tree_class_init(GntTreeClass *klass)
{
	GntBindableClass *bindable = GNT_BINDABLE_CLASS(klass);
	parent_class = GNT_WIDGET_CLASS(klass);
	parent_class->destroy = gnt_tree_destroy;
	parent_class->draw = gnt_tree_draw;
	parent_class->map = gnt_tree_map;
	parent_class->size_request = gnt_tree_size_request;
	parent_class->key_pressed = gnt_tree_key_pressed;
	parent_class->clicked = gnt_tree_clicked;
	parent_class->size_changed = gnt_tree_size_changed;

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
	GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_GROW_X | GNT_WIDGET_GROW_Y);
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

const GList *gnt_tree_get_rows(GntTree *tree)
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

	if (tree->compare == NULL)
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
		if (tree->compare(key, row->key) < 0)
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

	if (!tree->compare)
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
		if (tree->compare(row->key, s->key) < 0)
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

	g_hash_table_replace(tree->hash, key, row);
	row->tree = tree;

	if (bigbro == NULL && tree->compare)
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

	row->key = key;
	row->data = NULL;

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

GList *gnt_tree_get_selection_text_list(GntTree *tree)
{
	GList *list = NULL, *iter;
	int i;

	if (!tree->current)
		return NULL;

	for (i = 0, iter = tree->current->columns; i < tree->ncol && iter;
			i++, iter = iter->next)
	{
		GntTreeCol *col = iter->data;
		list = g_list_append(list, g_strdup(col->text));
	}

	return list;
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
		g_free(col->text);
		col->text = g_strdup(text);

		if (get_distance(tree->top, row) >= 0 && get_distance(row, tree->bottom) >= 0)
			redraw_tree(tree);
	}
}

GntTreeRow *gnt_tree_add_choice(GntTree *tree, void *key, GntTreeRow *row, void *parent, void *bigbro)
{
	GntTreeRow *r;
	r = g_hash_table_lookup(tree->hash, key);
	g_return_val_if_fail(!r || !r->choice, NULL);

	if (bigbro == NULL) {
		if (tree->compare)
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

void gnt_tree_set_selected(GntTree *tree , void *key)
{
	int dist;
	GntTreeRow *row = g_hash_table_lookup(tree->hash, key);
	if (!row)
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
}

void _gnt_tree_init_internals(GntTree *tree, int col)
{
	tree->ncol = col;
	tree->hash = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, free_tree_row);
	tree->columns = g_new0(struct _GntTreeColInfo, col);
	while (col--)
	{
		tree->columns[col].width = 15;
	}
	tree->list = NULL;
	tree->show_title = FALSE;
}

GntWidget *gnt_tree_new_with_columns(int col)
{
	GntWidget *widget = g_object_new(GNT_TYPE_TREE, NULL);
	GntTree *tree = GNT_TREE(widget);

	_gnt_tree_init_internals(tree, col);
	
	GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_NO_SHADOW);
	gnt_widget_set_take_focus(widget, TRUE);

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
		col->text = g_strdup(iter->data ? iter->data : "");

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
	tree->compare = func;
}

void gnt_tree_set_expanded(GntTree *tree, void *key, gboolean expanded)
{
	GntTreeRow *row = g_hash_table_lookup(tree->hash, key);
	if (row) {
		row->collapsed = !expanded;
		if (GNT_WIDGET(tree)->window)
			gnt_widget_draw(GNT_WIDGET(tree));
	}
}

void gnt_tree_set_show_separator(GntTree *tree, gboolean set)
{
	tree->show_separator = set;
}

void gnt_tree_adjust_columns(GntTree *tree)
{
	GntTreeRow *row = tree->root;
	int *widths, i, twidth, height;

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
		gnt_tree_set_col_width(tree, i, widths[i]);
		if (!tree->columns[i].invisible)
			twidth += widths[i] + (tree->show_separator ? 1 : 0) + 1;
	}
	g_free(widths);

	gnt_widget_get_size(GNT_WIDGET(tree), NULL, &height);
	gnt_widget_set_size(GNT_WIDGET(tree), twidth, height);
}

void gnt_tree_set_hash_fns(GntTree *tree, gpointer hash, gpointer eq, gpointer kd)
{
	g_hash_table_foreach_remove(tree->hash, return_true, NULL);
	g_hash_table_destroy(tree->hash);
	tree->hash = g_hash_table_new_full(hash, eq, kd, free_tree_row);
}

void gnt_tree_set_column_visible(GntTree *tree, int col, gboolean vis)
{
	g_return_if_fail(col < tree->ncol);
	tree->columns[col].invisible = !vis;
}

