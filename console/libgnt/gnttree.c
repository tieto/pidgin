#include "gnttree.h"
#include "gntutils.h"

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
	char *text;
	void *data;		/* XXX: unused */

	gboolean collapsed;
	gboolean choice;            /* Is this a choice-box?
	                               If choice is true, then child will be NULL */
	gboolean isselected;

	GntTreeRow *parent;
	GntTreeRow *child;
	GntTreeRow *next;
	GntTreeRow *prev;
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

static void
redraw_tree(GntTree *tree)
{
	int start;
	GntWidget *widget = GNT_WIDGET(tree);
	GntTreeRow *row;
	int pos;
	gboolean deep;

	if (GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_NO_BORDER))
		pos = 0;
	else
		pos = 1;

	if (tree->top == NULL)
		tree->top = tree->root;
	if (tree->current == NULL)
		tree->current = tree->root;

	wbkgd(widget->window, COLOR_PAIR(GNT_COLOR_NORMAL));

	deep = TRUE;
	row = tree->top;
	for (start = pos; row && start < widget->priv.height - pos;
				start++, row = get_next(row))
	{
		char str[2048];
		int wr;
		char format[16] = "";

		deep = TRUE;

		if (row->parent == NULL && row->child)
		{
			if (row->collapsed)
			{
				strcpy(format, "+ ");
				deep = FALSE;
			}
			else
				strcpy(format, "- ");
		}
		else if (row->choice)
		{
			g_snprintf(format, sizeof(format) - 1, "[%c] ", row->isselected ? 'X' : ' ');
		}

		if ((wr = g_snprintf(str, widget->priv.width, "%s%s", format, row->text)) >= widget->priv.width)
		{
			/* XXX: ellipsize */
			str[widget->priv.width - 1 - pos] = 0;
		}
		else
		{
			while (wr < widget->priv.width - 1 - pos)
				str[wr++] = ' ';
			str[wr] = 0;
		}
		
		if (row == tree->current)
		{
			if (gnt_widget_has_focus(widget))
				wbkgdset(widget->window, '\0' | COLOR_PAIR(GNT_COLOR_HIGHLIGHT));
			else
				wbkgdset(widget->window, '\0' | COLOR_PAIR(GNT_COLOR_HIGHLIGHT_D));
			mvwprintw(widget->window, start, pos, str);
			whline(widget->window, ' ', widget->priv.width - pos * 2 - g_utf8_strlen(str, -1));
			wbkgdset(widget->window, '\0' | COLOR_PAIR(GNT_COLOR_NORMAL));
		}
		else
		{
			mvwprintw(widget->window, start, pos, str);
			whline(widget->window, ' ', widget->priv.width - pos * 2 - g_utf8_strlen(str, -1));
		}
		tree->bottom = row;
	}

	while (start < widget->priv.height - pos)
	{
		mvwhline(widget->window, start, pos, ' ',
				widget->priv.width - pos * 2);
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
		widget->priv.width = 20;	/* YYY: 'cuz ... */
}

static void
gnt_tree_map(GntWidget *widget)
{
	if (widget->priv.width == 0 || widget->priv.height == 0)
		gnt_widget_size_request(widget);
	DEBUG;
}

static void
tree_selection_changed(GntTree *tree, GntTreeRow *old, GntTreeRow *current)
{
	g_signal_emit(tree, signals[SIG_SELECTION_CHANGED], 0, old, current);
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

	g_hash_table_destroy(tree->hash);
	g_list_free(tree->list);
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
free_tree_row(gpointer data)
{
	GntTreeRow *row = data;

	if (!row)
		return;

	g_free(row->text);
	g_free(row);
}

GntWidget *gnt_tree_new()
{
	GntWidget *widget = g_object_new(GNT_TYPE_TREE, NULL);
	GntTree *tree = GNT_TREE(widget);

	tree->hash = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, free_tree_row);
	GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_NO_SHADOW);
	gnt_widget_set_take_focus(widget, TRUE);

	return widget;
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

GntTreeRow *gnt_tree_add_row_after(GntTree *tree, void *key, const char *text, void *parent, void *bigbro)
{
	GntTreeRow *row = g_new0(GntTreeRow, 1), *pr = NULL;

	g_hash_table_replace(tree->hash, key, row);

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
			while (r->next)
				r = r->next;
			r->next = row;
			row->prev = r;

			tree->list = g_list_append(tree->list, key);
		}
		else
		{
			tree->list = g_list_insert(tree->list, key, position + 1);
		}
	}

	row->key = key;
	row->text = g_strdup_printf("%*s%s", TAB_SIZE * find_depth(row), "", text);
	row->data = NULL;

	if (GNT_WIDGET_IS_FLAG_SET(GNT_WIDGET(tree), GNT_WIDGET_MAPPED))
		redraw_tree(tree);

	return row;
}

gpointer gnt_tree_get_selection_data(GntTree *tree)
{
	if (tree->current)
		return tree->current->key;	/* XXX: perhaps we should just get rid of 'data' */
	return NULL;
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

int gnt_tree_get_selection_visible_line(GntTree *tree)
{
	return get_distance(tree->top, tree->current) +
			!!(GNT_WIDGET_IS_FLAG_SET(GNT_WIDGET(tree), GNT_WIDGET_NO_BORDER));
}

void gnt_tree_change_text(GntTree *tree, gpointer key, const char *text)
{
	GntTreeRow *row = g_hash_table_lookup(tree->hash, key);
	if (row)
	{
		g_free(row->text);
		row->text = g_strdup_printf("%*s%s", TAB_SIZE * find_depth(row), "", text);

		if (get_distance(tree->top, row) >= 0 && get_distance(row, tree->bottom) > 0)
			redraw_tree(tree);
	}
}

GntTreeRow *gnt_tree_add_choice(GntTree *tree, void *key, const char *text, void *parent, void *bigbro)
{
	GntTreeRow *row;

	row = g_hash_table_lookup(tree->hash, key);
	g_return_val_if_fail(!row || !row->choice, NULL);
		
	row = gnt_tree_add_row_after(tree, key, text, parent, bigbro);
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

