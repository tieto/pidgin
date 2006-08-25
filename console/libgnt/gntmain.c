#ifdef HAVE_NCURSESW_INC
#include <ncursesw/panel.h>
#else
#include <panel.h>
#endif

#include <gmodule.h>

#include "gnt.h"
#include "gntbox.h"
#include "gntcolors.h"
#include "gntkeys.h"
#include "gntstyle.h"
#include "gnttree.h"
#include "gntwm.h"

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>

/**
 * Notes: Interesting functions to look at:
 * 	scr_dump, scr_init, scr_restore: for workspaces
 *
 * 	Need to wattrset for colors to use with PDCurses.
 */

static int lock_focus_list;
static GList *focus_list;
static GList *ordered;

static int X_MIN;
static int X_MAX;
static int Y_MIN;
static int Y_MAX;

static gboolean ascii_only;
static gboolean mouse_enabled;

static GntWM wm;

static GMainLoop *loop;
static struct
{
	GntWidget *window;
	GntWidget *tree;
} window_list;

typedef struct
{
	GntWidget *me;

	PANEL *panel;
} GntNode;

typedef enum
{
	GNT_KP_MODE_NORMAL,
	GNT_KP_MODE_RESIZE,
	GNT_KP_MODE_MOVE,
	GNT_KP_MODE_MENU,
	GNT_KP_MODE_WINDOW_LIST
} GntKeyPressMode;

static GHashTable *nodes;

static void free_node(gpointer data);
static void draw_taskbar(gboolean reposition);
static void bring_on_top(GntWidget *widget);

static gboolean refresh_screen();

static GList *
g_list_bring_to_front(GList *list, gpointer data)
{
	list = g_list_remove(list, data);
	list = g_list_prepend(list, data);
	return list;
}

static gboolean
update_screen(gpointer null)
{
	update_panels();
	doupdate();
	return TRUE;
}

void gnt_screen_take_focus(GntWidget *widget)
{
	GntWidget *w = NULL;

	if (lock_focus_list)
		return;
	if (g_list_find(focus_list, widget))
		return;

	if (ordered)
		w = ordered->data;

	focus_list = g_list_append(focus_list, widget);

	ordered = g_list_append(ordered, widget);

	gnt_widget_set_focus(widget, TRUE);
	if (w)
		gnt_widget_set_focus(w, FALSE);
	draw_taskbar(FALSE);
}

void gnt_screen_remove_widget(GntWidget *widget)
{
	int pos = g_list_index(focus_list, widget);

	if (lock_focus_list)
		return;

	if (pos == -1)
		return;

	focus_list = g_list_remove(focus_list, widget);
	ordered = g_list_remove(ordered, widget);

	if (ordered)
	{
		bring_on_top(ordered->data);
	}
	draw_taskbar(FALSE);
}

static void
bring_on_top(GntWidget *widget)
{
	GntNode *node = g_hash_table_lookup(nodes, widget);

	if (!node)
		return;

	gnt_widget_set_focus(widget, TRUE);
	gnt_widget_draw(widget);
	top_panel(node->panel);

	if (window_list.window)
	{
		GntNode *nd = g_hash_table_lookup(nodes, window_list.window);
		top_panel(nd->panel);
	}
	update_screen(NULL);
	draw_taskbar(FALSE);
}

static void
update_window_in_list(GntWidget *wid)
{
	GntTextFormatFlags flag = 0;

	if (window_list.window == NULL)
		return;

	if (wid == ordered->data)
		flag |= GNT_TEXT_FLAG_DIM;
	else if (GNT_WIDGET_IS_FLAG_SET(wid, GNT_WIDGET_URGENT))
		flag |= GNT_TEXT_FLAG_BOLD;

	gnt_tree_set_row_flags(GNT_TREE(window_list.tree), wid, flag);
}

static void
draw_taskbar(gboolean reposition)
{
	static WINDOW *taskbar = NULL;
	GList *iter;
	int n, width = 0;
	int i;

	if (taskbar == NULL)
	{
		taskbar = newwin(1, getmaxx(stdscr), getmaxy(stdscr) - 1, 0);
	}
	else if (reposition)
	{
		mvwin(taskbar, Y_MAX, 0);
	}

	wbkgdset(taskbar, '\0' | COLOR_PAIR(GNT_COLOR_NORMAL));
	werase(taskbar);

	n = g_list_length(focus_list);
	if (n)
		width = getmaxx(stdscr) / n;

	for (i = 0, iter = focus_list; iter; iter = iter->next, i++)
	{
		GntWidget *w = iter->data;
		int color;
		const char *title;

		if (w == ordered->data)
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
		title = GNT_BOX(w)->title;
		mvwprintw(taskbar, 0, width * i, "%s", title ? title : "<gnt>");

		update_window_in_list(w);
	}

	wrefresh(taskbar);
}

static void
switch_window(int direction)
{
	GntWidget *w = NULL, *wid = NULL;
	int pos;

	if (!ordered || !ordered->next)
		return;

	w = ordered->data;
	pos = g_list_index(focus_list, w);
	pos += direction;

	if (pos < 0)
		wid = g_list_last(focus_list)->data;
	else if (pos >= g_list_length(focus_list))
		wid = focus_list->data;
	else if (pos >= 0)
		wid = g_list_nth_data(focus_list, pos);

	ordered = g_list_bring_to_front(ordered, wid);

	bring_on_top(ordered->data);

	if (w != wid)
	{
		gnt_widget_set_focus(w, FALSE);
	}
}

static void
switch_window_n(int n)
{
	GntWidget *w = NULL;
	GList *l;

	if (!ordered)
		return;
	
	w = ordered->data;

	if ((l = g_list_nth(focus_list, n)) != NULL)
	{
		ordered = g_list_bring_to_front(ordered, l->data);
		bring_on_top(ordered->data);
	}

	if (l && w != l->data)
	{
		gnt_widget_set_focus(w, FALSE);
	}
}

static void
window_list_activate(GntTree *tree, gpointer null)
{
	GntWidget *widget = gnt_tree_get_selection_data(GNT_TREE(tree));
	GntWidget *old = NULL;

	if (!ordered || !widget)
		return;

	old = ordered->data;
	ordered = g_list_bring_to_front(ordered, widget);
	bring_on_top(widget);

	if (old != widget)
	{
		gnt_widget_set_focus(old, FALSE);
	}
}

static void
show_window_list()
{
	GntWidget *tree, *win;
	GList *iter;

	if (window_list.window)
		return;

	win = window_list.window = gnt_box_new(FALSE, FALSE);
	gnt_box_set_toplevel(GNT_BOX(win), TRUE);
	gnt_box_set_title(GNT_BOX(win), "Window List");
	gnt_box_set_pad(GNT_BOX(win), 0);

	tree = window_list.tree = gnt_tree_new();

	for (iter = focus_list; iter; iter = iter->next)
	{
		GntBox *box = GNT_BOX(iter->data);

		gnt_tree_add_row_last(GNT_TREE(tree), box,
				gnt_tree_create_row(GNT_TREE(tree), box->title), NULL);
		update_window_in_list(GNT_WIDGET(box));
	}

	gnt_tree_set_selected(GNT_TREE(tree), ordered->data);
	gnt_box_add_widget(GNT_BOX(win), tree);

	gnt_tree_set_col_width(GNT_TREE(tree), 0, getmaxx(stdscr) / 3);
	gnt_widget_set_size(tree, 0, getmaxy(stdscr) / 2);
	gnt_widget_set_position(win, getmaxx(stdscr) / 3, getmaxy(stdscr) / 4);

	lock_focus_list = 1;
	gnt_widget_show(win);
	lock_focus_list = 0;

	g_signal_connect(G_OBJECT(tree), "activate", G_CALLBACK(window_list_activate), NULL);
}

static void
shift_window(GntWidget *widget, int dir)
{
	GList *all = focus_list;
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
	focus_list = all;
	draw_taskbar(FALSE);
}

static void
dump_screen()
{
	int x, y;
	chtype old = 0, now = 0;
	FILE *file = fopen("dump.html", "w");

	fprintf(file, "<pre>");
	for (y = 0; y < getmaxy(stdscr); y++)
	{
		for (x = 0; x < getmaxx(stdscr); x++)
		{
			char ch;
			now = mvwinch(newscr, y, x);
			ch = now & A_CHARTEXT;
			now ^= ch;

#define CHECK(attr, start, end) \
			do \
			{  \
				if (now & attr)  \
				{  \
					if (!(old & attr))  \
						fprintf(file, start);  \
				}  \
				else if (old & attr)  \
				{  \
					fprintf(file, end);  \
				}  \
			} while (0) 

			CHECK(A_BOLD, "<b>", "</b>");
			CHECK(A_UNDERLINE, "<u>", "</u>");
			CHECK(A_BLINK, "<blink>", "</blink>");

			if ((now & A_COLOR) != (old & A_COLOR))
			{
				int ret;
				short fgp, bgp, r, g, b;
				struct
				{
					int r, g, b;
				} fg, bg;

				ret = pair_content(PAIR_NUMBER(now & A_COLOR), &fgp, &bgp);
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
}

static void
refresh_node(GntWidget *widget, GntNode *node, gpointer null)
{
	int x, y, w, h;
	int nw, nh;

	gnt_widget_get_position(widget, &x, &y);
	gnt_widget_get_size(widget, &w, &h);

	if (x + w >= X_MAX)
		x = MAX(0, X_MAX - w);
	if (y + h >= Y_MAX)
		y = MAX(0, Y_MAX - h);
	gnt_screen_move_widget(widget, x, y);

	nw = MIN(w, X_MAX);
	nh = MIN(h, Y_MAX);
	if (nw != w || nh != h)
		gnt_screen_resize_widget(widget, nw, nh);
}

/**
 * Mouse support:
 *  - bring a window on top if you click on its taskbar
 *  - click on the top-bar of the active window and drag+drop to move a window
 *  - click on a window to bring it to focus
 *  wishlist:
 *   - have a little [X] on the windows, and clicking it will close that window.
 *   - allow scrolling in tree/textview on wheel-scroll event
 *   - click to activate button or select a row in tree
 *      - all these can be fulfilled by adding a "clicked" event for GntWidget
 *        which will send the (x,y) to the widget. (look at "key_pressed" for hints)
 */
static gboolean
detect_mouse_action(const char *buffer)
{
	int x, y;
	static enum {
		MOUSE_NONE,
		MOUSE_LEFT,
		MOUSE_RIGHT,
		MOUSE_MIDDLE
	} button = MOUSE_NONE;
	static GntWidget *remember = NULL;
	static int offset = 0;

	if (buffer[0] != 27)
		return FALSE;
	
	buffer++;
	if (strlen(buffer) < 5)
		return FALSE;

	x = buffer[3];
	y = buffer[4];
	if (x < 0)	x += 256;
	if (y < 0)	y += 256;
	x -= 33;
	y -= 33;

	if (strncmp(buffer, "[M ", 3) == 0) {
		/* left button down */
		/* Bring the window you clicked on to front */
		/* If you click on the topbar, then you can drag to move the window */
		GList *iter;
		for (iter = ordered; iter; iter = iter->next) {
			GntWidget *wid = iter->data;
			if (x >= wid->priv.x && x < wid->priv.x + wid->priv.width) {
				if (y >= wid->priv.y && y < wid->priv.y + wid->priv.height) {
					if (iter != ordered) {
						GntWidget *w = ordered->data;
						ordered = g_list_bring_to_front(ordered, iter->data);
						bring_on_top(ordered->data);
						gnt_widget_set_focus(w, FALSE);
					}
					if (y == wid->priv.y) {
						offset = x - wid->priv.x;
						remember = wid;
						button = MOUSE_LEFT;
					}
					break;
				}
			}
		}
	} else if (strncmp(buffer, "[M\"", 3) == 0) {
		/* right button down */
	} else if (strncmp(buffer, "[M!", 3) == 0) {
		/* middle button down */
	} else if (strncmp(buffer, "[M`", 3) == 0) {
		/* wheel up*/
	} else if (strncmp(buffer, "[Ma", 3) == 0) {
		/* wheel down */
	} else if (strncmp(buffer, "[M#", 3) == 0) {
		/* button up */
		if (button == MOUSE_NONE && y == getmaxy(stdscr) - 1) {
			int n = g_list_length(focus_list);
			if (n) {
				int width = getmaxx(stdscr) / n;
				switch_window_n(x / width);
			}
		} else if (button == MOUSE_LEFT && remember) {
			x -= offset;
			if (x < 0)	x = 0;
			if (y < 0)	y = 0;
			gnt_screen_move_widget(remember, x, y);
			refresh_node(remember, NULL, NULL);
		}
		button = MOUSE_NONE;
		remember = NULL;
		offset = 0;
	} else
		return FALSE;
	return FALSE; /* XXX: this should be TRUE */
}

static gboolean
io_invoke(GIOChannel *source, GIOCondition cond, gpointer null)
{
	char buffer[256];
	gboolean ret = FALSE;
	static GntKeyPressMode mode = GNT_KP_MODE_NORMAL;

	int rd = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
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

	if (buffer[0] == 27 && buffer[1] == 'd' && buffer[2] == 0)
	{
		/* This dumps the screen contents in an html file */
		dump_screen();
	}

	gnt_keys_refine(buffer);

	if (mouse_enabled && detect_mouse_action(buffer))
		return TRUE;

	if (mode == GNT_KP_MODE_NORMAL)
	{
		if (ordered)
		{
			ret = gnt_widget_key_pressed(ordered->data, buffer);
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
					if (ordered)
					{
						gnt_widget_destroy(ordered->data);
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
					/* Move a window */
					mode = GNT_KP_MODE_MOVE;
				}
				else if (strcmp(buffer + 1, "w") == 0 && focus_list)
				{
					/* Window list */
					mode = GNT_KP_MODE_WINDOW_LIST;
					show_window_list();
				}
				else if (strcmp(buffer + 1, "r") == 0 && focus_list)
				{
					/* Resize window */
					mode = GNT_KP_MODE_RESIZE;
				}
				else if (strcmp(buffer + 1, ",") == 0 && focus_list)
				{
					/* Re-order the list of windows */
					shift_window(ordered->data, -1);
				}
				else if (strcmp(buffer + 1, ".") == 0 && focus_list)
				{
					shift_window(ordered->data, 1);
				}
				else if (strcmp(buffer + 1, "l") == 0)
				{
					refresh_screen();
				}
				else if (strlen(buffer) == 2 && isdigit(*(buffer + 1)))
				{
					int n = *(buffer + 1) - '0';

					if (n == 0)
						n = 10;

					switch_window_n(n - 1);
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
			GntWidget *widget = GNT_WIDGET(ordered->data);

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
				gnt_widget_draw(widget);
			}

			if (changed)
			{
				gnt_screen_move_widget(widget, x, y);
			}
		}
		else if (*buffer == '\r')
		{
			mode = GNT_KP_MODE_NORMAL;
		}
	}
	else if (mode == GNT_KP_MODE_WINDOW_LIST && window_list.window)
	{
		gnt_widget_key_pressed(window_list.window, buffer);

		if (buffer[0] == '\r' || (buffer[0] == 27 && buffer[1] == 0))
		{
			mode = GNT_KP_MODE_NORMAL;
			lock_focus_list = 1;
			gnt_widget_destroy(window_list.window);
			window_list.window = NULL;
			window_list.tree = NULL;
			lock_focus_list = 0;
		}
	}
	else if (mode == GNT_KP_MODE_RESIZE)
	{
		if (buffer[0] == '\r' || (buffer[0] == 27 && buffer[1] == 0))
			mode = GNT_KP_MODE_NORMAL;
		else if (buffer[0] == 27)
		{
			GntWidget *widget = ordered->data;
			gboolean changed = FALSE;
			int width, height;

			gnt_widget_get_size(widget, &width, &height);

			if (strcmp(buffer + 1, GNT_KEY_DOWN) == 0)
			{
				if (widget->priv.y + height < Y_MAX)
				{
					height++;
					changed = TRUE;
				}
			}
			else if (strcmp(buffer + 1, GNT_KEY_UP) == 0)
			{
				height--;
				changed = TRUE;
			}
			else if (strcmp(buffer + 1, GNT_KEY_LEFT) == 0)
			{
				width--;
				changed = TRUE;
			}
			else if (strcmp(buffer + 1, GNT_KEY_RIGHT) == 0)
			{
				if (widget->priv.x + width < X_MAX)
				{
					width++;
					changed = TRUE;
				}
			}

			if (changed)
			{
				gnt_screen_resize_widget(widget, width, height);
			}
		}
	}

	return TRUE;
}

static gboolean
refresh_screen()
{
	endwin();
	refresh();

	X_MAX = getmaxx(stdscr);
	Y_MAX = getmaxy(stdscr) - 1;

	g_hash_table_foreach(nodes, (GHFunc)refresh_node, NULL);
	update_screen(NULL);
	draw_taskbar(TRUE);

	return FALSE;
}

#ifdef SIGWINCH
static void
sighandler(int sig)
{
	if (sig == SIGWINCH)
	{
		werase(stdscr);
		wrefresh(stdscr);

		g_idle_add(refresh_screen, NULL);
	}

	signal(SIGWINCH, sighandler);
}
#endif

static void
init_wm()
{
	const char *name = gnt_style_get(GNT_STYLE_WM);
	gpointer handle;
	
	if (!name || !*name)
		return;
	
	handle = g_module_open(name, G_MODULE_BIND_LAZY);
	if (handle) {
		gboolean (*init)(GntWM *);
		if (g_module_symbol(handle, "gntwm_init", &init)) {
			init(&wm);
		}
	}
}

void gnt_init()
{
	static GIOChannel *channel = NULL;
	char *filename;
	int result;
	const char *locale;

	if (channel)
		return;
	
	channel = g_io_channel_unix_new(STDIN_FILENO);

	g_io_channel_set_encoding(channel, NULL, NULL);
	g_io_channel_set_buffered(channel, FALSE);
#if 0
	g_io_channel_set_flags(channel, G_IO_FLAG_NONBLOCK, NULL );
#endif

	result = g_io_add_watch_full(channel,  G_PRIORITY_HIGH,
					(G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_PRI | G_IO_NVAL),
					io_invoke, NULL, NULL);
	
	locale = setlocale(LC_ALL, "");

#if 0
	g_io_channel_unref(channel);  /* Apparently this causes crash for some people */
#endif

	if (locale && (strstr(locale, "UTF") || strstr(locale, "utf")))
		ascii_only = FALSE;
	else
		ascii_only = TRUE;

	initscr();
	typeahead(-1);
	noecho();
	curs_set(0);

	gnt_init_styles();

	filename = g_build_filename(g_get_home_dir(), ".gntrc", NULL);
	gnt_style_read_configure_file(filename);
	g_free(filename);

	gnt_init_colors();
	X_MIN = 0;
	Y_MIN = 0;
	X_MAX = getmaxx(stdscr);
	Y_MAX = getmaxy(stdscr) - 1;

	nodes = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, free_node);

	wbkgdset(stdscr, '\0' | COLOR_PAIR(GNT_COLOR_NORMAL));
	refresh();

#ifdef ALL_MOUSE_EVENTS
	if ((mouse_enabled = gnt_style_get_bool(GNT_STYLE_MOUSE, FALSE)))
		mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
#endif

	wbkgdset(stdscr, '\0' | COLOR_PAIR(GNT_COLOR_NORMAL));
	werase(stdscr);
	wrefresh(stdscr);

#ifdef SIGWINCH
	signal(SIGWINCH, sighandler);
#endif

	g_type_init();

	init_wm();
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
	hide_panel(node->panel);
	del_panel(node->panel);
	g_free(node);
}

void gnt_screen_occupy(GntWidget *widget)
{
	GntNode *node;

	while (widget->parent)
		widget = widget->parent;
	
	if (g_hash_table_lookup(nodes, widget))
		return;		/* XXX: perhaps _update instead? */

	node = g_new0(GntNode, 1);
	node->me = widget;

	g_hash_table_replace(nodes, widget, node);

	refresh_node(widget, node, NULL);

	if (window_list.window)
	{
		if ((GNT_IS_BOX(widget) && GNT_BOX(widget)->title) && window_list.window != widget
				&& GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_CAN_TAKE_FOCUS))
		{
			gnt_tree_add_row_last(GNT_TREE(window_list.tree), widget,
					gnt_tree_create_row(GNT_TREE(window_list.tree), GNT_BOX(widget)->title),
					NULL);
			update_window_in_list(widget);
		}
	}

	update_screen(NULL);
}

void gnt_screen_release(GntWidget *widget)
{
	GntNode *node;

	gnt_screen_remove_widget(widget);
	node = g_hash_table_lookup(nodes, widget);

	if (node == NULL)	/* Yay! Nothing to do. */
		return;

	g_hash_table_remove(nodes, widget);

	if (window_list.window)
	{
		gnt_tree_remove(GNT_TREE(window_list.tree), widget);
	}

	update_screen(NULL);
}

void gnt_screen_update(GntWidget *widget)
{
	GntNode *node;
	
	while (widget->parent)
		widget = widget->parent;
	
	gnt_box_sync_children(GNT_BOX(widget));
	node = g_hash_table_lookup(nodes, widget);
	if (node && !node->panel)
	{
		if (wm.new_window)
			node->panel = wm.new_window(node->me);
		else
			node->panel = new_panel(node->me->window);
		if (!GNT_WIDGET_IS_FLAG_SET(node->me, GNT_WIDGET_TRANSIENT))
		{
			bottom_panel(node->panel);     /* New windows should not grab focus */
			gnt_widget_set_urgent(node->me);
		}
	}

	if (window_list.window)
	{
		GntNode *nd = g_hash_table_lookup(nodes, window_list.window);
		top_panel(nd->panel);
	}

	update_screen(NULL);
}

gboolean gnt_widget_has_focus(GntWidget *widget)
{
	GntWidget *w;
	if (!widget)
		return FALSE;

	w = widget;

	while (widget->parent)
		widget = widget->parent;

	if (widget == window_list.window)
		return TRUE;

	if (ordered && ordered->data == widget)
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

	if (ordered && ordered->data == widget)
		return;

	GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_URGENT);
	draw_taskbar(FALSE);
}

void gnt_quit()
{
	gnt_uninit_colors();
	gnt_uninit_styles();
	endwin();
}

gboolean gnt_ascii_only()
{
	return ascii_only;
}

void gnt_screen_resize_widget(GntWidget *widget, int width, int height)
{
	if (widget->parent == NULL)
	{
		GntNode *node = g_hash_table_lookup(nodes, widget);
		if (!node)
			return;

		hide_panel(node->panel);
		gnt_widget_set_size(widget, width, height);
		gnt_widget_draw(widget);
		replace_panel(node->panel, widget->window);
		show_panel(node->panel);
		update_screen(NULL);
	}
}

void gnt_screen_move_widget(GntWidget *widget, int x, int y)
{
	GntNode *node = g_hash_table_lookup(nodes, widget);
	gnt_widget_set_position(widget, x, y);
	move_panel(node->panel, y, x);
	update_screen(NULL);
}

