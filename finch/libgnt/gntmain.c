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

#define _GNU_SOURCE
#if (defined(__APPLE__) || defined(__unix__)) && !defined(__FreeBSD__) && !defined(__OpenBSD__)
#define _XOPEN_SOURCE_EXTENDED
#endif

#include "config.h"

#include <gmodule.h>

#include <sys/types.h>
#include <sys/wait.h>

#include "gntinternal.h"
#undef GNT_LOG_DOMAIN
#define GNT_LOG_DOMAIN "Main"

#include "gnt.h"
#include "gntbox.h"
#include "gntbutton.h"
#include "gntcolors.h"
#include "gntclipboard.h"
#include "gntkeys.h"
#include "gntlabel.h"
#include "gntmenu.h"
#include "gntstyle.h"
#include "gnttree.h"
#include "gntutils.h"
#include "gntwindow.h"
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

/**
 * Notes: Interesting functions to look at:
 * 	scr_dump, scr_init, scr_restore: for workspaces
 *
 * 	Need to wattrset for colors to use with PDCurses.
 */

static GIOChannel *channel = NULL;
static guint channel_read_callback = 0;
static guint channel_error_callback = 0;

static gboolean ascii_only;
static gboolean mouse_enabled;

static void setup_io(void);

static gboolean refresh_screen(void);

static GntWM *wm;
static GntClipboard *clipboard;

int gnt_need_conversation_to_locale;

#define HOLDING_ESCAPE  (escape_stuff.timer != 0)

static struct {
	int timer;
} escape_stuff;

static gboolean
escape_timeout(gpointer data)
{
	gnt_wm_process_input(wm, "\033");
	escape_stuff.timer = 0;
	return FALSE;
}

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

	if (!wm->cws->ordered || buffer[0] != 27)
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

	if (widget && gnt_wm_process_click(wm, event, x, y, widget))
		return TRUE;
	
	if (event == GNT_LEFT_MOUSE_DOWN && widget && widget != wm->_list.window &&
			!GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_TRANSIENT)) {
		if (widget != wm->cws->ordered->data) {
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
			int n = g_list_length(wm->cws->list);
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

	if (widget)
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
	gssize rd;
	char *k;
	char *cvrt = NULL;

	if (wm->mode == GNT_KP_MODE_WAIT_ON_CHILD)
		return FALSE;

	rd = read(STDIN_FILENO, keys + HOLDING_ESCAPE, sizeof(keys) - 1 - HOLDING_ESCAPE);
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

	rd += HOLDING_ESCAPE;
	if (HOLDING_ESCAPE) {
		keys[0] = '\033';
		g_source_remove(escape_stuff.timer);
		escape_stuff.timer = 0;
	}
	keys[rd] = 0;
	gnt_wm_set_event_stack(wm, TRUE);

	cvrt = g_locale_to_utf8(keys, rd, (gsize*)&rd, NULL, NULL);
	k = cvrt ? cvrt : keys;
	if (mouse_enabled && detect_mouse_action(k))
		goto end;

#if 0
	/* I am not sure what's happening here. If this actually does something,
	 * then this needs to go in gnt_keys_refine. */
	if (*k < 0) { /* Alt not sending ESC* */
		*(k + 1) = 128 - *k;
		*k = 27;
		*(k + 2) = 0;
		rd++;
	}
#endif

	while (rd) {
		char back;
		int p;

		if (k[0] == '\033' && rd == 1) {
			escape_stuff.timer = g_timeout_add(250, escape_timeout, NULL);
			break;
		}

		gnt_keys_refine(k);
		p = MAX(1, gnt_keys_find_combination(k));
		back = k[p];
		k[p] = '\0';
		gnt_wm_process_input(wm, k);     /* XXX: */
		k[p] = back;
		rd -= p;
		k += p;
	}
end:
	if (wm)
		gnt_wm_set_event_stack(wm, FALSE);
	g_free(cvrt);
	return TRUE;
}

static void
setup_io()
{
	int result;
	channel = g_io_channel_unix_new(STDIN_FILENO);
	g_io_channel_set_close_on_unref(channel, TRUE);

#if 0
	g_io_channel_set_encoding(channel, NULL, NULL);
	g_io_channel_set_buffered(channel, FALSE);
	g_io_channel_set_flags(channel, G_IO_FLAG_NONBLOCK, NULL );
#endif

	channel_read_callback = result = g_io_add_watch_full(channel,  G_PRIORITY_HIGH,
					(G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_PRI),
					io_invoke, NULL, NULL);

	channel_error_callback = g_io_add_watch_full(channel,  G_PRIORITY_HIGH,
					(G_IO_NVAL),
					io_invoke_error, GINT_TO_POINTER(result), NULL);

	g_io_channel_unref(channel);  /* Apparently this caused crashes for some people.
	                                 But irssi does this, so I am going to assume the
	                                 crashes were caused by some other stuff. */

	gnt_warning("setting up IO (%d)", channel_read_callback);
}

static gboolean
refresh_screen(void)
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
exit_confirmed(gpointer null)
{
	gnt_bindable_perform_action_named(GNT_BINDABLE(wm), "wm-quit", NULL);
}

static void
exit_win_close(GntWidget *w, GntWidget **win)
{
	*win = NULL;
}

static void
ask_before_exit(void)
{
	static GntWidget *win = NULL;
	GntWidget *bbox, *button;

	if (wm->menu) {
		do {
			gnt_widget_hide(GNT_WIDGET(wm->menu));
			if (wm->menu)
				wm->menu = wm->menu->parentmenu;
		} while (wm->menu);
	}

	if (win)
		goto raise;

	win = gnt_vwindow_new(FALSE);
	gnt_box_add_widget(GNT_BOX(win), gnt_label_new("Are you sure you want to quit?"));
	gnt_box_set_title(GNT_BOX(win), "Quit?");
	gnt_box_set_alignment(GNT_BOX(win), GNT_ALIGN_MID);
	g_signal_connect(G_OBJECT(win), "destroy", G_CALLBACK(exit_win_close), &win);

	bbox = gnt_hbox_new(FALSE);
	gnt_box_add_widget(GNT_BOX(win), bbox);

	button = gnt_button_new("Quit");
	g_signal_connect(G_OBJECT(button), "activate", G_CALLBACK(exit_confirmed), NULL);
	gnt_box_add_widget(GNT_BOX(bbox), button);

	button = gnt_button_new("Cancel");
	g_signal_connect_swapped(G_OBJECT(button), "activate", G_CALLBACK(gnt_widget_destroy), win);
	gnt_box_add_widget(GNT_BOX(bbox), button);

	gnt_widget_show(win);
raise:
	gnt_wm_raise_window(wm, win);
}

#ifdef SIGWINCH
static void (*org_winch_handler)(int);
#endif

static void
sighandler(int sig)
{
	switch (sig) {
#ifdef SIGWINCH
	case SIGWINCH:
		erase();
		g_idle_add((GSourceFunc)refresh_screen, NULL);
		if (org_winch_handler)
			org_winch_handler(sig);
		signal(SIGWINCH, sighandler);
		break;
#endif
	case SIGCHLD:
		clean_pid();
		signal(SIGCHLD, sighandler);
		break;
	case SIGINT:
		ask_before_exit();
		signal(SIGINT, sighandler);
		break;
	}
}

static void
init_wm(void)
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

#ifdef NO_WIDECHAR
	ascii_only = TRUE;
#else
	if (locale && (strstr(locale, "UTF") || strstr(locale, "utf"))) {
		ascii_only = FALSE;
	} else {
		ascii_only = TRUE;
		gnt_need_conversation_to_locale = TRUE;
	}
#endif

	initscr();
	typeahead(-1);
	noecho();
	curs_set(0);

	gnt_init_keys();
	gnt_init_styles();

	filename = g_build_filename(g_get_home_dir(), ".gntrc", NULL);
	gnt_style_read_configure_file(filename);
	g_free(filename);

	gnt_init_colors();

	wbkgdset(stdscr, '\0' | gnt_color_pair(GNT_COLOR_NORMAL));
	refresh();

#ifdef ALL_MOUSE_EVENTS
	if ((mouse_enabled = gnt_style_get_bool(GNT_STYLE_MOUSE, FALSE)))
		mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
#endif

	wbkgdset(stdscr, '\0' | gnt_color_pair(GNT_COLOR_NORMAL));
	werase(stdscr);
	wrefresh(stdscr);

#ifdef SIGWINCH
	org_winch_handler = signal(SIGWINCH, sighandler);
#endif
	signal(SIGCHLD, sighandler);
	signal(SIGINT, sighandler);
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

void gnt_window_present(GntWidget *window)
{
	if (wm->event_stack)
		gnt_wm_raise_window(wm, window);
	else
		gnt_widget_set_urgent(window);
}

void gnt_screen_occupy(GntWidget *widget)
{
	gnt_wm_new_window(wm, widget);
}

void gnt_screen_release(GntWidget *widget)
{
	if (wm)
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
	if (wm->cws->ordered && wm->cws->ordered->data == widget) {
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

	if (wm->cws->ordered && wm->cws->ordered->data == widget)
		return;

	GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_URGENT);

	gnt_wm_update_window(wm, widget);
}

void gnt_quit()
{
	/* Prevent io_invoke() from being called after wm is destroyed */
	g_source_remove(channel_error_callback);
	g_source_remove(channel_read_callback);

	channel_error_callback = 0;
	channel_read_callback = 0;

	g_object_unref(G_OBJECT(wm));
	wm = NULL;

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

void gnt_register_action(const char *label, void (*callback)(void))
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
	g_signal_connect(G_OBJECT(wm->menu), "destroy", G_CALLBACK(reset_menu), NULL);

	return TRUE;
}

void gnt_set_clipboard_string(const gchar *string)
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

#if GLIB_CHECK_VERSION(2,4,0)
typedef struct
{
	void (*callback)(int status, gpointer data);
	gpointer data;
} ChildProcess;

static void
reap_child(GPid pid, gint status, gpointer data)
{
	ChildProcess *cp = data;
	if (cp->callback) {
		cp->callback(status, cp->data);
	}
	g_free(cp);
	clean_pid();
	wm->mode = GNT_KP_MODE_NORMAL;
	endwin();
	setup_io();
	refresh();
	refresh_screen();
}
#endif

gboolean gnt_giveup_console(const char *wd, char **argv, char **envp,
		gint *stin, gint *stout, gint *sterr,
		void (*callback)(int status, gpointer data), gpointer data)
{
#if GLIB_CHECK_VERSION(2,4,0)
	GPid pid = 0;
	ChildProcess *cp = NULL;

	if (!g_spawn_async_with_pipes(wd, argv, envp,
			G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
			(GSpawnChildSetupFunc)endwin, NULL,
			&pid, stin, stout, sterr, NULL))
		return FALSE;

	cp = g_new0(ChildProcess, 1);
	cp->callback = callback;
	cp->data = data;
	g_source_remove(channel_read_callback);
	wm->mode = GNT_KP_MODE_WAIT_ON_CHILD;
	g_child_watch_add(pid, reap_child, cp);

	return TRUE;
#else
	return FALSE;
#endif
}

gboolean gnt_is_refugee()
{
#if GLIB_CHECK_VERSION(2,4,0)
	return (wm && wm->mode == GNT_KP_MODE_WAIT_ON_CHILD);
#else
	return FALSE;
#endif
}

const char *C_(const char *x)
{
	static char *c = NULL;
	if (gnt_need_conversation_to_locale) {
		GError *error = NULL;
		g_free(c);
		c = g_locale_from_utf8(x, -1, NULL, NULL, &error);
		if (c == NULL || error) {
			char *store = c;
			c = NULL;
			gnt_warning("Error: %s\n", error ? error->message : "(unknown)");
			g_error_free(error);
			error = NULL;
			g_free(c);
			c = store;
		}
		return c ? c : x;
	} else
		return x;
}

