#include "gnt.h"
#include "gntbox.h"
#include "gntkeys.h"
#include "gntcolors.h"

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <string.h>

static GList *focus_list;
static int max_x;
static int max_y;

typedef struct
{
	GntWidget *me;
	GList *below;		/* List of widgets below me */
	GList *above;		/* List of widgets above me */
} GntNode;

static GHashTable *nodes;

static void free_node(gpointer data);
static void draw_taskbar();

void gnt_screen_take_focus(GntWidget *widget)
{
	GntWidget *w = NULL;
	if (focus_list)
		w = focus_list->data;
	focus_list = g_list_prepend(focus_list, widget);
	gnt_widget_set_focus(widget, TRUE);
	if (w)
		gnt_widget_set_focus(w, FALSE);
	draw_taskbar();
}

void gnt_screen_remove_widget(GntWidget *widget)
{
	focus_list = g_list_remove(focus_list, widget);
	if (focus_list)
	{
		gnt_widget_set_focus(focus_list->data, TRUE);
		gnt_widget_draw(focus_list->data);
	}
	draw_taskbar();
}

static void
bring_on_top(GntWidget *widget)
{
	GntNode *node = g_hash_table_lookup(nodes, widget);
	GList *iter;

	if (!node)
		return;

	for (iter = node->above; iter;)
	{
		GntNode *n = iter->data;
		iter = iter->next;
		n->below = g_list_remove(n->below, node);
		n->above = g_list_prepend(n->above, node);

		node->above = g_list_remove(node->above, n);
		node->below = g_list_prepend(node->below, n);
	}
}

static void
draw_taskbar()
{
	static WINDOW *taskbar = NULL;
	GList *iter;
	int n, width;
	int i;

	if (taskbar == NULL)
	{
		taskbar = newwin(1, getmaxx(stdscr), getmaxy(stdscr) - 1, 0);
	}

	werase(taskbar);

	n = g_list_length(g_list_first(focus_list));
	if (n)
		width = getmaxx(stdscr) / n;

	for (i = 0, iter = g_list_first(focus_list); iter; iter = iter->next, i++)
	{
		GntWidget *w = iter->data;
		int color;

		if (w == focus_list->data)
		{
			/* This is the current window in focus */
			color = GNT_COLOR_TITLE;
			GNT_WIDGET_UNSET_FLAGS(w, GNT_WIDGET_URGENT);
		}
		else if (GNT_WIDGET_IS_FLAG_SET(w, GNT_WIDGET_URGENT))
		{
			/* This is a window with the URGENT hint set */
			color = GNT_COLOR_TITLE_D;
		}
		else
		{
			color = GNT_COLOR_NORMAL;
		}
		wbkgdset(taskbar, '\0' | COLOR_PAIR(color));
		mvwhline(taskbar, 0, width * i, ' ' | COLOR_PAIR(color), width);
		mvwprintw(taskbar, 0, width * i, "%s", GNT_BOX(w)->title);
	}

	wrefresh(taskbar);
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
		ret = gnt_widget_key_pressed(focus_list->data, buffer);
	}

	if (buffer[0] == 27)
	{
		/* Some special key has been pressed */
		if (strcmp(buffer+1, GNT_KEY_POPUP) == 0)
		{}
		else if (strcmp(buffer + 1, "c") == 0)
		{
			/* Alt + c was pressed. I am going to use it to close a window. */
			if (focus_list)
			{
				gnt_widget_destroy(focus_list->data);
				gnt_screen_remove_widget(focus_list->data);
			}
		}
		else if (strcmp(buffer + 1, "q") == 0)
		{
			/* I am going to use Alt + q to quit. */
			endwin();
			exit(1);
		}
		else if (strcmp(buffer + 1, "n") == 0)
		{
			/* Alt + n to go to the next window */
			GntWidget *w = NULL;
			if (focus_list)
				w = focus_list->data;

			if (focus_list && focus_list->next)
				focus_list = focus_list->next;
			else
				focus_list = g_list_first(focus_list);
			if (focus_list)
			{
				gnt_widget_set_focus(focus_list->data, TRUE);
				bring_on_top(focus_list->data);
				gnt_widget_draw(focus_list->data);
			}

			if (w && w != focus_list->data)
				gnt_widget_set_focus(w, FALSE);
		}
	}

	draw_taskbar();
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
	if (node == NULL)	/* Yay! Nothing to do. */
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
					w->priv.y - widget->priv.y + top,
					w->priv.x - widget->priv.x + left,
					w->priv.y - widget->priv.y + bottom - 1,
					w->priv.x - widget->priv.x + right - 1, FALSE);
		n->above = g_list_remove(n->above, node);
	}

	for (iter = node->above; iter; iter = iter->next)
	{
		GntNode *n = iter->data;
		n->below = g_list_remove(n->below, node);
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
	
	gnt_box_sync_children(GNT_BOX(widget));
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
					w->priv.y - widget->priv.y + top,
					w->priv.x - widget->priv.x + left,
					w->priv.y - widget->priv.y + bottom - 1,
					w->priv.x - widget->priv.x + right - 1, FALSE);
		}
	}

	wrefresh(win);
	delwin(win);
}

gboolean gnt_widget_has_focus(GntWidget *widget)
{
	GntWidget *w;
	if (!widget)
		return FALSE;

	w = widget;

	while (widget->parent)
	{
		fprintf(stderr, "%p %p\n", widget, widget->parent);
		widget = widget->parent;
	}
	fprintf(stderr, "%p %p\n", widget, widget->parent);

	if (focus_list && focus_list->data == widget &&
			(!GNT_WIDGET_IS_FLAG_SET(w, GNT_WIDGET_CAN_TAKE_FOCUS) ||
			GNT_WIDGET_IS_FLAG_SET(w, GNT_WIDGET_HAS_FOCUS)))
		return TRUE;

	return FALSE;
}

void gnt_widget_set_urgent(GntWidget *widget)
{
	while (widget->parent)
		widget = widget->parent;

	if (focus_list && focus_list->data == widget)
		return;

	GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_URGENT);
	draw_taskbar();
}

