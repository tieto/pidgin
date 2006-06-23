#include "gnttree.h"

enum
{
	SIGS = 1,
};

/* XXX: Make this one into a GObject?
 * 		 ... Probably not */
struct _GnTreeRow
{
	void *key;
	char *text;
	void *data;		/* XXX: unused */

	GntTreeRow *parent;
	GntTreeRow *child;
	GntTreeRow *next;
};

static GntWidgetClass *parent_class = NULL;
static guint signals[SIGS] = { 0 };

static void
redraw_tree(GntTree *tree)
{
	int start;
	GntWidget *widget = GNT_WIDGET(tree);
	GList *iter;
	int pos;

	if (GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_NO_BORDER))
		pos = 0;
	else
		pos = 1;

	wbkgd(tree->scroll, COLOR_PAIR(GNT_COLOR_NORMAL));

	for (start = tree->top, iter = g_list_nth(tree->list, tree->top);
				iter && start < tree->bottom; start++, iter = iter->next)
	{
		char str[2096];	/* XXX: This should be safe for any terminal */
		int wr;
		GntTreeRow *row = g_hash_table_lookup(tree->hash, iter->data);

		if ((wr = snprintf(str, widget->priv.width, "%s", row->text)) >= widget->priv.width)
		{
			/* XXX: ellipsize */
			str[widget->priv.width - 1] = 0;
		}
		else
		{
			while (wr < widget->priv.width - 1)
				str[wr++] = ' ';
			str[wr] = 0;
		}
		
		if (start == tree->current)
		{
			wbkgdset(tree->scroll, '\0' | COLOR_PAIR(GNT_COLOR_HIGHLIGHT));
			mvwprintw(tree->scroll, start - tree->top + pos, pos, str);
			wbkgdset(tree->scroll, '\0' | COLOR_PAIR(GNT_COLOR_NORMAL));
		}
		else
			mvwprintw(tree->scroll, start - tree->top + pos, pos, str);
	}

	while (start < tree->bottom)
	{
		wmove(tree->scroll, start - tree->top + pos, pos);
		wclrtoeol(tree->scroll);
		start++;
	}

	wrefresh(widget->window);
}

static void
gnt_tree_draw(GntWidget *widget)
{
	GntTree *tree = GNT_TREE(widget);

	/* For the Tree (or anything that scrolls), we will create a 'hidden' window
	 * of 'large enough' size. We never wrefresh that hidden-window, instead we
	 * just copy stuff from it into the visible window */

	if (tree->scroll == NULL)
	{
		tree->scroll = widget->window; /* newwin(SCROLL_HEIGHT, widget->priv.width, 0, 0); */
		scrollok(tree->scroll, TRUE);
		/*wsetscrreg(tree->scroll, 0, SCROLL_HEIGHT - 1);*/
		wsetscrreg(tree->scroll, 0, widget->priv.height - 1);

		tree->top = 0;
		tree->bottom = widget->priv.height -
				(GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_NO_BORDER) ? 0 : 2);
	}

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

static gboolean
gnt_tree_key_pressed(GntWidget *widget, const char *text)
{
	GntTree *tree = GNT_TREE(widget);
	if (text[0] == 27)
	{
		if (strcmp(text+1, GNT_KEY_DOWN) == 0 && tree->current < g_list_length(tree->list) - 1)
		{
			tree->current++;
			if (tree->current >= tree->bottom)
				gnt_tree_scroll(tree, 1 + tree->current - tree->bottom);
			else
				redraw_tree(tree);
		}
		else if (strcmp(text+1, GNT_KEY_UP) == 0 && tree->current > 0)
		{
			tree->current--;
			if (tree->current < tree->top)
				gnt_tree_scroll(tree, tree->current - tree->top);
			else
				redraw_tree(tree);
		}
	}
	else if (text[0] == '\r')
	{
		gnt_widget_activate(widget);
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
gnt_tree_class_init(GntWidgetClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = GNT_WIDGET_CLASS(klass);
	parent_class->destroy = gnt_tree_destroy;
	parent_class->draw = gnt_tree_draw;
	parent_class->map = gnt_tree_map;
	parent_class->size_request = gnt_tree_size_request;
	parent_class->key_pressed = gnt_tree_key_pressed;

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

GntWidget *gnt_tree_new()
{
	GntWidget *widget = g_object_new(GNT_TYPE_TREE, NULL);
	GntTree *tree = GNT_TREE(widget);

	tree->hash = g_hash_table_new(g_direct_hash, g_direct_equal);
	GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_NO_SHADOW);

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
		widget->priv.height -= 2;
}

void gnt_tree_scroll(GntTree *tree, int count)
{
	if (tree->top == 0 && count < 0)
		return;

	if (count > 0 && tree->bottom + count >= g_list_length(tree->list))
		count = g_list_length(tree->list) - tree->bottom;
	else if (count < 0 && tree->top + count < 0)
		count = -tree->top;

	tree->top += count;
	tree->bottom += count;

	redraw_tree(tree);
}

void gnt_tree_add_row_after(GntTree *tree, void *key, const char *text, void *parent, void *bigbro)
{
	GntTreeRow *row = g_new0(GntTreeRow, 1), *pr = NULL;

	row->key = key;
	row->text = g_strdup(text);
	row->data = NULL;

	g_hash_table_insert(tree->hash, key, row);

	if (tree->root == NULL)
	{
		tree->root = row;
		tree->list = g_list_prepend(tree->list, key);
	}
	else
	{
		int position;

		if (bigbro)
		{
			pr = g_hash_table_lookup(tree->hash, bigbro);
			if (pr)
			{
				row->next = pr->next;
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
				row->next = pr->child;
				pr->child = row;
				row->parent = pr;

				position = g_list_index(tree->list, parent);
			}
		}

		if (pr == NULL)
		{
			row->next = tree->root;
			tree->root = row;

			tree->list = g_list_prepend(tree->list, key);
		}
		else
		{
			tree->list = g_list_insert(tree->list, key, position + 1);
		}
	}

	if (GNT_WIDGET_IS_FLAG_SET(GNT_WIDGET(tree), GNT_WIDGET_MAPPED))
		redraw_tree(tree);
}

gpointer gnt_tree_get_selection_data(GntTree *tree)
{
	return g_list_nth(tree->list, tree->current);
}

int gnt_tree_get_selection_index(GntTree *tree)
{
	return tree->current;
}

void gnt_tree_remove(GntTree *tree, gpointer key)
{
	GntTreeRow *row = g_hash_table_lookup(tree->hash, key);
	if (row)
	{
		int len, pos;

		g_free(row->text);
		g_free(row);

		pos = g_list_index(tree->list, key);

		g_hash_table_remove(tree->hash, key);
		tree->list = g_list_remove(tree->list, key);

		if (pos >= tree->top && pos < tree->bottom)
		{
			redraw_tree(tree);
		}
	}
}

