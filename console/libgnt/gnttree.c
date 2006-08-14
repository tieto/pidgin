#include "gnttree.h"
#include "gntmarshal.h"

#include <string.h>

enum
{
	SIG_SELECTION_CHANGED,
	SIG_SCROLLED,
	SIG_TOGGLED,
	SIGS,
};

#define	TAB_SIZE 3

/* XXX: Make this one into a GObject?
 * 		 ... Probably not */
struct _GnTreeRow
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
};

struct _GnTreeCol
{
	char *text;
	int span;       /* How many columns does it span? */
};

static GntWidgetClass *parent_class = NULL;
static guint signals[SIGS] = { 0 };

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

static GntTreeRow *
get_next(GntTreeRow *row)
{
	if (row == NULL)
		return NULL;
	return _get_next(row, !row->collapsed);
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
	if (row->child)
		row = get_last_child(row->child);
	return row;
}

static GntTreeRow *
get_prev(GntTreeRow *row)
{
	if (row == NULL)
		return NULL;
	if (row->prev)
		return get_last_child(row->prev);
	return row->parent;
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

	for (i = 0, iter = row->columns; i < tree->ncol && iter; i++, iter = iter->next)
	{
		GntTreeCol *col = iter->data;
		char *text;
		int len = g_utf8_strlen(col->text, -1);
		int fl = 0;

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
		else
			g_string_append_c(string, '|');

		if (len > tree->columns[i].width)
		{
			len = tree->columns[i].width;
		}

		text = g_utf8_offset_to_pointer(col->text, len - fl);
		string = g_string_append_len(string, col->text, text - col->text);
		if (len < tree->columns[i].width && iter->next)
			g_string_append_printf(string, "%*s", tree->columns[i].width - len, "");
	}
	return g_string_free(string, FALSE);
}

static void
tree_mark_columns(GntTree *tree, int pos, int y, chtype type)
{
	GntWidget *widget = GNT_WIDGET(tree);
	int i;
	int x = pos;

	for (i = 0; i < tree->ncol - 1; i++)
	{
		x += tree->columns[i].width;
		mvwaddch(widget->window, y, x + i, type);
	}
}

static void
redraw_tree(GntTree *tree)
{
	int start;
	GntWidget *widget = GNT_WIDGET(tree);
	GntTreeRow *row;
	int pos, up, down, nr;

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
		
		for (i = 0; i < tree->ncol; i++)
		{
			mvwprintw(widget->window, pos, x + i, tree->columns[i].title);
			x += tree->columns[i].width;
		}
		if (pos)
		{
			tree_mark_columns(tree, pos, 0, ACS_TTEE | COLOR_PAIR(GNT_COLOR_NORMAL));
			tree_mark_columns(tree, pos, widget->priv.height - pos,
					ACS_BTEE | COLOR_PAIR(GNT_COLOR_NORMAL));
		}
		tree_mark_columns(tree, pos, pos + 1, ACS_PLUS | COLOR_PAIR(GNT_COLOR_NORMAL));
		tree_mark_columns(tree, pos, pos, ACS_VLINE | COLOR_PAIR(GNT_COLOR_NORMAL));
		start = 2;
	}

	nr = widget->priv.height - pos * 2 - start - 1;
	tree->bottom = get_next_n_opt(tree->top, nr, &down);
	if (down < nr)
	{
		tree->top = get_prev_n(tree->bottom, nr);
		if (tree->top == NULL)
			tree->top = tree->root;
	}

	up = get_distance(tree->top, tree->current);
	if (up < 0)
		tree->top = tree->current;
	else if (up >= widget->priv.height - pos)
		tree->top = get_prev_n(tree->current, nr);

	mvwaddch(widget->window, start + pos,
			widget->priv.width - pos - 1,
			(tree->top != tree->root) ? 
			ACS_UARROW | COLOR_PAIR(GNT_COLOR_HIGHLIGHT_D) :
			' '| COLOR_PAIR(GNT_COLOR_NORMAL));

	row = tree->top;
	for (start = start + pos; row && start < widget->priv.height - pos;
				start++, row = get_next(row))
	{
		char *str;
		int wr;

		GntTextFormatFlags flags = row->flags;
		int attr = 0;

		str = update_row_text(tree, row);

		if ((wr = g_utf8_strlen(str, -1)) >= widget->priv.width - 1 - pos)
		{
			/* XXX: ellipsize */
			char *s = g_utf8_offset_to_pointer(str, widget->priv.width - 1 - pos);
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
		mvwprintw(widget->window, start, pos, str);
		whline(widget->window, ' ', widget->priv.width - pos * 2 - g_utf8_strlen(str, -1) - 1);
		tree->bottom = row;
		g_free(str);
		tree_mark_columns(tree, pos, start, ACS_VLINE | attr);
	}

	mvwaddch(widget->window, widget->priv.height - pos - 1,
			widget->priv.width - pos - 1,
			get_next(tree->bottom) ? 
			ACS_DARROW | COLOR_PAIR(GNT_COLOR_HIGHLIGHT_D) :
			' '| COLOR_PAIR(GNT_COLOR_NORMAL));

	wbkgdset(widget->window, '\0' | COLOR_PAIR(GNT_COLOR_NORMAL));
	while (start < widget->priv.height - pos)
	{
		mvwhline(widget->window, start, pos, ' ',
				widget->priv.width - pos * 2 - 1);
		tree_mark_columns(tree, pos, start, ACS_VLINE);
		start++;
	}

	gnt_widget_queue_update(widget);
}

static void
gnt_tree_draw(GntWidget *widget)
{
	GntTree *tree = GNT_TREE(widget);

	redraw_tree(tree);
	
	DEBUG;
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
			width += tree->columns[i].width;
		widget->priv.width = width + i;
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
	DEBUG;
}

static void
tree_selection_changed(GntTree *tree, GntTreeRow *old, GntTreeRow *current)
{
	g_signal_emit(tree, signals[SIG_SELECTION_CHANGED], 0, old->key, current->key);
}

static GntTreeRow *
get_nth_row(GntTree *tree, int n)
{
	gpointer key = g_list_nth_data(tree->list, n);
	return g_hash_table_lookup(tree->hash, key);
}

static gboolean
gnt_tree_key_pressed(GntWidget *widget, const char *text)
{
	GntTree *tree = GNT_TREE(widget);
	GntTreeRow *old = tree->current;
	GntTreeRow *row;

	if (text[0] == 27)
	{
		int dist;
		if (strcmp(text+1, GNT_KEY_DOWN) == 0 && (row = get_next(tree->current)) != NULL)
		{
			tree->current = row;
			if ((dist = get_distance(tree->current, tree->bottom)) < 0)
				gnt_tree_scroll(tree, -dist);
			else
				redraw_tree(tree);
		}
		else if (strcmp(text+1, GNT_KEY_UP) == 0 && (row = get_prev(tree->current)) != NULL)
		{
			tree->current = row;

			if ((dist = get_distance(tree->current, tree->top)) > 0)
				gnt_tree_scroll(tree, -dist);
			else
				redraw_tree(tree);
		}
	}
	else if (text[0] == '\r')
	{
		gnt_widget_activate(widget);
	}
	else if (text[0] == ' ' && text[1] == 0)
	{
		/* Space pressed */
		GntTreeRow *row = tree->current;
		if (row && row->child)
		{
			row->collapsed = !row->collapsed;
			redraw_tree(tree);
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

	g_hash_table_destroy(tree->hash);
	g_list_free(tree->list);

	for (i = 0; i < tree->ncol; i++)
	{
		g_free(tree->columns[i].title);
	}
	g_free(tree->columns);
}

static void
gnt_tree_class_init(GntTreeClass *klass)
{
	parent_class = GNT_WIDGET_CLASS(klass);
	parent_class->destroy = gnt_tree_destroy;
	parent_class->draw = gnt_tree_draw;
	parent_class->map = gnt_tree_map;
	parent_class->size_request = gnt_tree_size_request;
	parent_class->key_pressed = gnt_tree_key_pressed;

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
					 0,
					 NULL, NULL,
					 g_cclosure_marshal_VOID__POINTER,
					 G_TYPE_NONE, 1, G_TYPE_POINTER);

	DEBUG;
}

static void
gnt_tree_init(GTypeInstance *instance, gpointer class)
{
	GntWidget *widget = GNT_WIDGET(instance);
	GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_GROW_X | GNT_WIDGET_GROW_Y);
	widget->priv.minw = 4;
	widget->priv.minh = 3;
	DEBUG;
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

GntTreeRow *gnt_tree_add_row_after(GntTree *tree, void *key, GntTreeRow *row, void *parent, void *bigbro)
{
	GntTreeRow *pr = NULL;

	g_hash_table_replace(tree->hash, key, row);

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

/* XXX: Should this also remove all the children of the row being removed? */
void gnt_tree_remove(GntTree *tree, gpointer key)
{
	GntTreeRow *row = g_hash_table_lookup(tree->hash, key);
	if (row)
	{
		gboolean redraw = FALSE;

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
			if (tree->current == row)
				tree->current = tree->top;
		}
		else if (tree->current == row)
		{
			if (tree->current != tree->root)
				tree->current = get_prev(row);
			else
				tree->current = get_next(row);
		}
		else if (tree->bottom == row)
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

		if (redraw)
		{
			redraw_tree(tree);
		}
	}
}

void gnt_tree_remove_all(GntTree *tree)
{
	tree->root = NULL;
	g_hash_table_remove_all(tree->hash);
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

		if (get_distance(tree->top, row) >= 0 && get_distance(row, tree->bottom) > 0)
			redraw_tree(tree);
	}
}

GntTreeRow *gnt_tree_add_choice(GntTree *tree, void *key, GntTreeRow *row, void *parent, void *bigbro)
{
	GntTreeRow *r;
	r = g_hash_table_lookup(tree->hash, key);
	g_return_val_if_fail(!r || !r->choice, NULL);
		
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

GntWidget *gnt_tree_new_with_columns(int col)
{
	GntWidget *widget = g_object_new(GNT_TYPE_TREE, NULL);
	GntTree *tree = GNT_TREE(widget);

	tree->hash = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, free_tree_row);
	tree->ncol = col;
	tree->columns = g_new0(struct _GntTreeColInfo, col);
	while (col--)
	{
		tree->columns[col].width = 15;
	}
	tree->show_title = FALSE;
	
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
		col->text = g_strdup(iter->data);

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
		list = g_list_append(list, va_arg(args, const char *));
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
}

void gnt_tree_set_compare_func(GntTree *tree, GCompareFunc func)
{
	tree->compare = func;
}

