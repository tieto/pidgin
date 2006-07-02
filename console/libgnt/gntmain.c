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
static int X_MIN;
static int X_MAX;
static int Y_MIN;
static int Y_MAX;

static GMainLoop *loop;

typedef struct
{
	GntWidget *me;
	GList *below;		/* List of widgets below me */
	GList *above;		/* List of widgets above me */
} GntNode;

typedef enum
{
	GNT_KP_MODE_NORMAL,
	GNT_KP_MODE_RESIZE,
	GNT_KP_MODE_MOVE,
	GNT_KP_MODE_MENU,
} GntKeyPressMode;

static GHashTable *nodes;

static void free_node(gpointer data);
static void draw_taskbar();

void gnt_screen_take_focus(GntWidget *widget)
{
	GntWidget *w = NULL;

	if (focus_list)
		w = focus_list->data;

	/* XXX: ew */
	focus_list = g_list_first(focus_list);
	focus_list = g_list_append(focus_list, widget);
	focus_list = g_list_find(focus_list, widget);

	gnt_widget_set_focus(widget, TRUE);
	if (w)
		gnt_widget_set_focus(w, FALSE);
	draw_taskbar();
}

void gnt_screen_remove_widget(GntWidget *widget)
{
	int pos = g_list_index(g_list_first(focus_list), widget);
	GList *next;

	if (pos == -1)
		return;

	focus_list = g_list_first(focus_list);
	focus_list = g_list_remove(focus_list, widget);
	next = g_list_nth(focus_list, pos - 1);
	if (next)
		focus_list = next;

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

	wbkgdset(taskbar, '\0' | COLOR_PAIR(GNT_COLOR_NORMAL));
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

static void
switch_window(int direction)
{
	GntWidget *w = NULL;
	if (focus_list)
		w = focus_list->data;

	if (direction == 1)
	{
		if (focus_list && focus_list->next)
			focus_list = focus_list->next;
		else
			focus_list = g_list_first(focus_list);
	}
	else if (direction == -1)
	{
		if (focus_list && focus_list->prev)
			focus_list = focus_list->prev;
		else
			focus_list = g_list_last(focus_list);
	}
	
	if (focus_list)
	{
		gnt_widget_set_focus(focus_list->data, TRUE);
		bring_on_top(focus_list->data);
		gnt_widget_draw(focus_list->data);
	}

	if (w && (!focus_list || w != focus_list->data))
		gnt_widget_set_focus(w, FALSE);
}		

static gboolean
io_invoke(GIOChannel *source, GIOCondition cond, gpointer null)
{
	char buffer[256];
	static GntKeyPressMode mode = GNT_KP_MODE_NORMAL;
	gboolean ret = FALSE;

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

	if (mode == GNT_KP_MODE_NORMAL)
	{
		if (focus_list)
		{
			ret = gnt_widget_key_pressed(focus_list->data, buffer);
		}

		if (!ret)
		{
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
					}
				}
				else if (strcmp(buffer + 1, "q") == 0)
				{
					/* I am going to use Alt + q to quit. */
					g_main_loop_quit(loop);
				}
				else if (strcmp(buffer + 1, "n") == 0)
				{
					/* Alt + n to go to the next window */
					switch_window(1);
				}
				else if (strcmp(buffer + 1, "p") == 0)
				{
					/* Alt + p to go to the previous window */
					switch_window(-1);
				}
				else if (strcmp(buffer + 1, "m") == 0 && focus_list)
				{
					mode = GNT_KP_MODE_MOVE;
				}
			}
		}
	}
	else if (mode == GNT_KP_MODE_MOVE && focus_list)
	{
		if (buffer[0] == 27)
		{
			gboolean changed = FALSE;
			int x, y, w, h;
			GntWidget *widget = GNT_WIDGET(focus_list->data);

			gnt_widget_get_position(widget, &x, &y);
			gnt_widget_get_size(widget, &w, &h);

			if (strcmp(buffer + 1, GNT_KEY_LEFT) == 0)
			{
				if (x > X_MIN)
				{
					x--;
					changed = TRUE;
				}
			}
			else if (strcmp(buffer + 1, GNT_KEY_RIGHT) == 0)
			{
				if (x + w < X_MAX)
				{
					x++;
					changed = TRUE;
				}
			}
			else if (strcmp(buffer + 1, GNT_KEY_UP) == 0)
			{
				if (y > Y_MIN)
				{
					y--;
					changed = TRUE;
				}						
			}
			else if (strcmp(buffer + 1, GNT_KEY_DOWN) == 0)
			{
				if (y + h < Y_MAX)
				{
					y++;
					changed = TRUE;
				}
			}
			else if (buffer[1] == 0)
			{
				mode = GNT_KP_MODE_NORMAL;
				changed = TRUE;
			}

			if (changed)
			{
				gnt_widget_hide(widget);
				gnt_widget_set_position(widget, x, y);
				gnt_widget_show(widget);
			}
		}
		else if (*buffer == '\r')
		{
			mode = GNT_KP_MODE_NORMAL;
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

	X_MIN = 0;
	Y_MIN = 0;
	X_MAX = getmaxx(stdscr);
	Y_MAX = getmaxy(stdscr) - 1;

	nodes = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, free_node);

	wbkgdset(stdscr, '\0' | COLOR_PAIR(GNT_COLOR_NORMAL));
	noecho();
	refresh();
	mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
	wbkgdset(stdscr, '\0' | COLOR_PAIR(GNT_COLOR_NORMAL));
	werase(stdscr);
	wrefresh(stdscr);

	g_type_init();
}

void gnt_main()
{
	loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(loop);
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

	gnt_screen_remove_widget(widget);

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
		widget = widget->parent;
	}

	if (focus_list && focus_list->data == widget)
	{
		if (GNT_IS_BOX(widget) &&
				(GNT_BOX(widget)->active == w || widget == w))
			return TRUE;
	}

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

void gnt_quit()
{
	endwin();
}

