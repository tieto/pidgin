#include "gnt.h"
#include "gntkeys.h"
#include "gntcolors.h"
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

static GList *focus_list;
static int max_x;
static int max_y;

static GHashTable *nodes;

static void free_node(gpointer data);

void gnt_screen_take_focus(GntWidget *widget)
{
	focus_list = g_list_prepend(focus_list, widget);
}

void gnt_screen_remove_widget(GntWidget *widget)
{
	focus_list = g_list_remove(focus_list, widget);
	if (focus_list)
		gnt_widget_draw(focus_list->data);
}

static gboolean
io_invoke(GIOChannel *source, GIOCondition cond, gpointer null)
{
	char buffer[256];

	int rd = read(0, buffer, sizeof(buffer) - 1);
	if (rd < 0)
	{
		endwin();
		printf("ERROR!\n");
		exit(1);
	}
	else if (rd == 0)
	{
		endwin();
		printf("EOF\n");
		exit(1);
	}

	buffer[rd] = 0;

	if (focus_list)
	{
		gboolean ret = FALSE;
		/*g_signal_emit_by_name(focus_list->data, "key_pressed", buffer, &ret);*/
		ret = gnt_widget_key_pressed(focus_list->data, buffer);
	}

	if (buffer[0] == 27)
	{
		/* Some special key has been pressed */
		if (strcmp(buffer+1, GNT_KEY_POPUP) == 0)
		{
			/*printf("popup\n");*/
		}
		else
		{
			/*printf("Unknown: %s\n", buffer+1);*/
		}
	}
	else
	{
		if (buffer[0] == 'q')
		{
			endwin();
			exit(1);
		}
		/*printf("%s\n", buffer);*/
	}
	refresh();

	return TRUE;
}

void gnt_init()
{
	GIOChannel *channel = g_io_channel_unix_new(0);

	g_io_channel_set_encoding(channel, NULL, NULL);
	g_io_channel_set_buffered(channel, FALSE);
	g_io_channel_set_flags(channel, G_IO_FLAG_NONBLOCK, NULL );

	int result = g_io_add_watch(channel, 
					(G_IO_IN | G_IO_HUP | G_IO_ERR),
					io_invoke, NULL);

	setlocale(LC_ALL, "");
	initscr();
	start_color();
	gnt_init_colors();

	max_x = getmaxx(stdscr);
	max_y = getmaxy(stdscr);

	nodes = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, free_node);

	wbkgdset(stdscr, '\0' | COLOR_PAIR(GNT_COLOR_NORMAL));
	noecho();
	refresh();
	mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
	g_type_init();
}

void gnt_main()
{
	GMainLoop *loop = g_main_new(FALSE);
	g_main_run(loop);
}

/*********************************
 * Stuff for 'window management' *
 *********************************/

typedef struct
{
	GntWidget *me;
	GList *below;		/* List of widgets below me */
	GList *above;		/* List of widgets above me */
} GntNode;

static void
free_node(gpointer data)
{
	GntNode *node = data;
	g_list_free(node->below);
	g_list_free(node->above);
	g_free(node);
}

static void
check_intersection(gpointer key, gpointer value, gpointer data)
{
	GntNode *n = value;
	GntNode *nu = data;

	if (value == NULL)
		return;
	if (n->me == nu->me)
		return;

	if (n->me->priv.x + n->me->priv.width < nu->me->priv.x)
		return;
	if (nu->me->priv.x + nu->me->priv.width < n->me->priv.x)
		return;

	if (n->me->priv.y + n->me->priv.height < nu->me->priv.y)
		return;
	if (nu->me->priv.y + nu->me->priv.height < n->me->priv.y)
		return;

	n->above = g_list_prepend(n->above, nu);
	nu->below = g_list_prepend(nu->below, n);
}

void gnt_screen_occupy(GntWidget *widget)
{
	GntNode *node;

	if (widget->parent)
	{
		while (widget->parent)
			widget = widget->parent;
	}
	
	if (g_hash_table_lookup(nodes, widget))
		return;		/* XXX: perhaps _update instead? */

	node = g_new0(GntNode, 1);
	node->me = widget;

	g_hash_table_foreach(nodes, check_intersection, node);
	g_hash_table_replace(nodes, widget, node);
}

void gnt_screen_release(GntWidget *widget)
{
	WINDOW *win;
	GList *iter;
	GntNode *node = g_hash_table_lookup(nodes, widget);
	if (node == NULL || node->below == NULL)	/* Yay! Nothing to do. */
		return;

	win = dupwin(widget->window);
	werase(win);

	/* XXX: This is not going to work.
	 *      It will be necessary to build a topology and go from there. */
	for (iter = node->below; iter; iter = iter->next)
	{
		GntNode *n = iter->data;
		GntWidget *w = n->me;
		int left, right, top, bottom;

		left = MAX(widget->priv.x, w->priv.x) - w->priv.x;
		right = MIN(widget->priv.x + widget->priv.width, w->priv.x + w->priv.width) - w->priv.x;
		
		top = MAX(widget->priv.y, w->priv.y) - w->priv.y;
		bottom = MIN(widget->priv.y + widget->priv.height, w->priv.y + w->priv.height) - w->priv.y;
		
		copywin(w->window, win, top, left,
						w->priv.y + top,
						w->priv.x + left,
						w->priv.y + bottom - top - 1,
						w->priv.x + right - left - 1, FALSE);
		n->above = g_list_remove(n->above, node);
	}

	wrefresh(win);
	delwin(win);

	g_hash_table_remove(nodes, widget);
}

void gnt_screen_update(GntWidget *widget)
{
	GList *iter;
	WINDOW *win;
	GntNode *node;
	
	if (widget->parent)
	{
		while (widget->parent)
			widget = widget->parent;
	}
	
	gnt_box_sync_children(widget);
	node = g_hash_table_lookup(nodes, widget);

	win = dupwin(widget->window);
	
	if (node && node->above)
	{
		/* XXX: Same here: need to build a topology first. */
		for (iter = node->above; iter; iter = iter->next)
		{
			GntNode *n = iter->data;
			GntWidget *w = n->me;
			int left, right, top, bottom;

			left = MAX(widget->priv.x, w->priv.x) - w->priv.x;
			right = MIN(widget->priv.x + widget->priv.width, w->priv.x + w->priv.width) - w->priv.x;
			
			top = MAX(widget->priv.y, w->priv.y) - w->priv.y;
			bottom = MIN(widget->priv.y + widget->priv.height, w->priv.y + w->priv.height) - w->priv.y;

			copywin(w->window, win, top, left,
					w->priv.y + top,
					w->priv.x + left,
					w->priv.y + bottom - top - 1,
					w->priv.x + right - left - 1, FALSE);
		}
	}

	wrefresh(win);
	delwin(win);
}

