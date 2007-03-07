#define _GNU_SOURCE
#if defined(__APPLE__)
#define _XOPEN_SOURCE_EXTENDED
#endif

#include "config.h"

#include <gmodule.h>

#include "gnt.h"
#include "gntbox.h"
#include "gntcolors.h"
#include "gntclipboard.h"
#include "gntkeys.h"
#include "gntmenu.h"
#include "gntstyle.h"
#include "gnttree.h"
#include "gntutils.h"
#include "gntwm.h"

#include <panel.h>

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/wait.h>

/**
 * Notes: Interesting functions to look at:
 * 	scr_dump, scr_init, scr_restore: for workspaces
 *
 * 	Need to wattrset for colors to use with PDCurses.
 */

static GIOChannel *channel = NULL;

static gboolean ascii_only;
static gboolean mouse_enabled;

static void setup_io();

static gboolean refresh_screen();

GntWM *wm;
static GntClipboard *clipboard;

/**
 * Mouse support:
 *  - bring a window on top if you click on its taskbar
 *  - click on the top-bar of the active window and drag+drop to move a window
 *  - click on a window to bring it to focus
 *   - allow scrolling in tree/textview on wheel-scroll event
 *   - click to activate button or select a row in tree
 *  wishlist:
 *   - have a little [X] on the windows, and clicking it will close that window.
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
	GntMouseEvent event;
	GntWidget *widget = NULL;
	PANEL *p = NULL;

	if (!wm->ordered || buffer[0] != 27)
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

	while ((p = panel_below(p)) != NULL) {
		const GntNode *node = panel_userptr(p);
		GntWidget *wid;
		if (!node)
			continue;
		wid = node->me;
		if (x >= wid->priv.x && x < wid->priv.x + wid->priv.width) {
			if (y >= wid->priv.y && y < wid->priv.y + wid->priv.height) {
				widget = wid;
				break;
			}
		}
	}

	if (strncmp(buffer, "[M ", 3) == 0) {
		/* left button down */
		/* Bring the window you clicked on to front */
		/* If you click on the topbar, then you can drag to move the window */
		event = GNT_LEFT_MOUSE_DOWN;
	} else if (strncmp(buffer, "[M\"", 3) == 0) {
		/* right button down */
		event = GNT_RIGHT_MOUSE_DOWN;
	} else if (strncmp(buffer, "[M!", 3) == 0) {
		/* middle button down */
		event = GNT_MIDDLE_MOUSE_DOWN;
	} else if (strncmp(buffer, "[M`", 3) == 0) {
		/* wheel up*/
		event = GNT_MOUSE_SCROLL_UP;
	} else if (strncmp(buffer, "[Ma", 3) == 0) {
		/* wheel down */
		event = GNT_MOUSE_SCROLL_DOWN;
	} else if (strncmp(buffer, "[M#", 3) == 0) {
		/* button up */
		event = GNT_MOUSE_UP;
	} else
		return FALSE;
	
	if (gnt_wm_process_click(wm, event, x, y, widget))
		return TRUE;
	
	if (event == GNT_LEFT_MOUSE_DOWN && widget && widget != wm->_list.window &&
			!GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_TRANSIENT)) {
		if (widget != wm->ordered->data) {
			gnt_wm_raise_window(wm, widget);
		}
		if (y == widget->priv.y) {
			offset = x - widget->priv.x;
			remember = widget;
			button = MOUSE_LEFT;
		}
	} else if (event == GNT_MOUSE_UP) {
		if (button == MOUSE_NONE && y == getmaxy(stdscr) - 1) {
			/* Clicked on the taskbar */
			int n = g_list_length(wm->list);
			if (n) {
				int width = getmaxx(stdscr) / n;
				gnt_bindable_perform_action_named(GNT_BINDABLE(wm), "switch-window-n", x/width, NULL);
			}
		} else if (button == MOUSE_LEFT && remember) {
			x -= offset;
			if (x < 0)	x = 0;
			if (y < 0)	y = 0;
			gnt_screen_move_widget(remember, x, y);
		}
		button = MOUSE_NONE;
		remember = NULL;
		offset = 0;
	}

	gnt_widget_clicked(widget, event, x, y);
	return TRUE;
}

static gboolean
io_invoke_error(GIOChannel *source, GIOCondition cond, gpointer data)
{
	int id = GPOINTER_TO_INT(data);
	g_source_remove(id);
	g_io_channel_unref(source);

	channel = NULL;
	setup_io();
	return TRUE;
}

static gboolean
io_invoke(GIOChannel *source, GIOCondition cond, gpointer null)
{
	char keys[256];
	int rd = read(STDIN_FILENO, keys, sizeof(keys) - 1);
	int processed;
	char *k;
	if (rd < 0)
	{
		int ch = getch(); /* This should return ERR, but let's see what it really returns */
		endwin();
		printf("ERROR: %s\n", strerror(errno));
		printf("File descriptor is: %d\n\nGIOChannel is: %p\ngetch() = %d\n", STDIN_FILENO, source, ch);
		raise(SIGABRT);
	}
	else if (rd == 0)
	{
		endwin();
		printf("EOF\n");
		raise(SIGABRT);
	}

	keys[rd] = 0;
	gnt_keys_refine(keys);

	if (mouse_enabled && detect_mouse_action(keys))
		return TRUE;

	processed = 0;
	k = keys;
	while (rd) {
		char back;
		int p = MAX(1, gnt_keys_find_combination(k));
		back = k[p];
		k[p] = '\0';
		gnt_wm_process_input(wm, k);     /* XXX: */
		k[p] = back;
		rd -= p;
		k += p;
	}

	return TRUE;
}

static void
setup_io()
{
	int result;
	channel = g_io_channel_unix_new(STDIN_FILENO);

#if 0
	g_io_channel_set_encoding(channel, NULL, NULL);
	g_io_channel_set_buffered(channel, FALSE);
	g_io_channel_set_flags(channel, G_IO_FLAG_NONBLOCK, NULL );
#endif

	result = g_io_add_watch_full(channel,  G_PRIORITY_HIGH,
					(G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_PRI),
					io_invoke, NULL, NULL);
	
	g_io_add_watch_full(channel,  G_PRIORITY_HIGH,
					(G_IO_NVAL),
					io_invoke_error, GINT_TO_POINTER(result), NULL);
	
	g_io_channel_unref(channel);  /* Apparently this caused crashes for some people.
	                                 But irssi does this, so I am going to assume the
	                                 crashes were caused by some other stuff. */

	g_printerr("gntmain: setting up IO\n");
}

static gboolean
refresh_screen()
{
	gnt_bindable_perform_action_named(GNT_BINDABLE(wm), "refresh-screen", NULL);
	return FALSE;
}

/* Xerox */
static void
clean_pid(void)
{
	int status;
	pid_t pid;

	do {
		pid = waitpid(-1, &status, WNOHANG);
	} while (pid != 0 && pid != (pid_t)-1);

	if ((pid == (pid_t) - 1) && (errno != ECHILD)) {
		char errmsg[BUFSIZ];
		g_snprintf(errmsg, BUFSIZ, "Warning: waitpid() returned %d", pid);
		perror(errmsg);
	}
}

static void
sighandler(int sig)
{
	switch (sig) {
#ifdef SIGWINCH
	case SIGWINCH:
		werase(stdscr);
		wrefresh(stdscr);
		g_idle_add(refresh_screen, NULL);
		signal(SIGWINCH, sighandler);
		break;
#endif
	case SIGCHLD:
		clean_pid();
		signal(SIGCHLD, sighandler);
		break;
	}
}

static void
init_wm()
{
	const char *name = gnt_style_get(GNT_STYLE_WM);
	gpointer handle;
	
	if (name && *name) {
		handle = g_module_open(name, G_MODULE_BIND_LAZY);
		if (handle) {
			gboolean (*init)(GntWM **);
			if (g_module_symbol(handle, "gntwm_init", (gpointer)&init)) {
				init(&wm);
			}
		}
	}
	if (wm == NULL)
		wm = g_object_new(GNT_TYPE_WM, NULL);
}

void gnt_init()
{
	char *filename;
	const char *locale;

	if (channel)
		return;
	
	locale = setlocale(LC_ALL, "");

	setup_io();

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
	gnt_init_keys();

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
	signal(SIGCHLD, sighandler);
	signal(SIGPIPE, SIG_IGN);

	g_type_init();

	init_wm();

	clipboard = g_object_new(GNT_TYPE_CLIPBOARD, NULL);
}

void gnt_main()
{
	wm->loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(wm->loop);
}

/*********************************
 * Stuff for 'window management' *
 *********************************/

void gnt_screen_occupy(GntWidget *widget)
{
	gnt_wm_new_window(wm, widget);
}

void gnt_screen_release(GntWidget *widget)
{
	gnt_wm_window_close(wm, widget);
}

void gnt_screen_update(GntWidget *widget)
{
	gnt_wm_update_window(wm, widget);
}

gboolean gnt_widget_has_focus(GntWidget *widget)
{
	GntWidget *w;
	if (!widget)
		return FALSE;
	
	if (GNT_IS_MENU(widget))
		return TRUE;

	w = widget;

	while (widget->parent)
		widget = widget->parent;

	if (widget == wm->_list.window)
		return TRUE;
	if (wm->ordered && wm->ordered->data == widget) {
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

	if (wm->ordered && wm->ordered->data == widget)
		return;

	GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_URGENT);

	gnt_wm_update_window(wm, widget);
}

void gnt_quit()
{
	g_hash_table_destroy(wm->nodes); /* XXX: */
	update_panels();
	doupdate();
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
	gnt_wm_resize_window(wm, widget, width, height);
}

void gnt_screen_move_widget(GntWidget *widget, int x, int y)
{
	gnt_wm_move_window(wm, widget, x, y);
}

void gnt_screen_rename_widget(GntWidget *widget, const char *text)
{
	gnt_box_set_title(GNT_BOX(widget), text);
	gnt_widget_draw(widget);
	gnt_wm_update_window(wm, widget);
}

void gnt_register_action(const char *label, void (*callback)())
{
	GntAction *action = g_new0(GntAction, 1);
	action->label = g_strdup(label);
	action->callback = callback;

	wm->acts = g_list_append(wm->acts, action);
}

static void
reset_menu(GntWidget *widget, gpointer null)
{
	wm->menu = NULL;
}

gboolean gnt_screen_menu_show(gpointer newmenu)
{
	if (wm->menu) {
		/* For now, if a menu is being displayed, then another menu
		 * can NOT take over. */
		return FALSE;
	}

	wm->menu = newmenu;
	GNT_WIDGET_UNSET_FLAGS(GNT_WIDGET(wm->menu), GNT_WIDGET_INVISIBLE);
	gnt_widget_draw(GNT_WIDGET(wm->menu));

	g_signal_connect(G_OBJECT(wm->menu), "hide", G_CALLBACK(reset_menu), NULL);

	return TRUE;
}

void gnt_set_clipboard_string(gchar *string)
{
	gnt_clipboard_set_string(clipboard, string);
}

GntClipboard *gnt_get_clipboard()
{
	return clipboard;
}
gchar *gnt_get_clipboard_string()
{
	return gnt_clipboard_get_string(clipboard);
}
