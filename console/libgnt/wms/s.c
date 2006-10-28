#include "gnt.h"
#include "gntbox.h"
#include "gntmenu.h"
#include "gntwm.h"

#include "gntblist.h"

#include <string.h>

static GntWM *gwm;

static void
envelope_buddylist(GntWidget *win)
{
	int w, h;
	gnt_widget_get_size(win, &w, &h);
	wresize(win->window, h, w + 1);
	mvwvline(win->window, 0, w, ACS_VLINE | COLOR_PAIR(GNT_COLOR_NORMAL), h);
	touchwin(win->window);
}

static void
envelope_normal_window(GntWidget *win)
{
	int w, h;

	if (GNT_WIDGET_IS_FLAG_SET(win, GNT_WIDGET_NO_BORDER))
		return;

	gnt_widget_get_size(win, &w, &h);
	wbkgdset(win->window, ' ' | COLOR_PAIR(GNT_COLOR_NORMAL));
	mvwprintw(win->window, 0, w - 4, "[X]");
}

static PANEL *
s_resize_window(PANEL *panel, GntWidget *win)
{
	const char *name;

	name = gnt_widget_get_name(win);
	if (name && strcmp(name, "buddylist") == 0) {
		envelope_buddylist(win);
	} else {
		envelope_normal_window(win);
	}
	replace_panel(panel, win->window);
	return panel;
}

static PANEL *
s_new_window(GntWidget *win)
{
	int x, y, w, h;
	int maxx, maxy;
	const char *name;

	if (GNT_IS_MENU(win))
		return new_panel(win->window);
	getmaxyx(stdscr, maxy, maxx);

	gnt_widget_get_position(win, &x, &y);
	gnt_widget_get_size(win, &w, &h);

	name = gnt_widget_get_name(win);

	if (name && strcmp(name, "buddylist") == 0) {
		/* The buddylist doesn't have no border nor nothing! */
		x = 0;
		y = 0;
		h = maxy - 1;

		gnt_box_set_toplevel(GNT_BOX(win), FALSE);
		GNT_WIDGET_SET_FLAGS(win, GNT_WIDGET_CAN_TAKE_FOCUS);
		gnt_box_readjust(GNT_BOX(win));

		gnt_widget_set_position(win, x, y);
		mvwin(win->window, y, x);

		gnt_widget_set_size(win, w, h);
		gnt_widget_draw(win);
		envelope_buddylist(win);
	} else if (name && strcmp(name, "conversation-window") == 0) {
		/* Put the conversation windows to the far-right */
		x = maxx - w;
		y = 0;
		gnt_widget_set_position(win, x, y);
		mvwin(win->window, y, x);
		gnt_widget_draw(win);
		envelope_normal_window(win);
	} else if (!GNT_WIDGET_IS_FLAG_SET(win, GNT_WIDGET_TRANSIENT)) {
		/* In the middle of the screen */
		x = (maxx - w) / 2;
		y = (maxy - h) / 2;

		gnt_widget_set_position(win, x, y);
		mvwin(win->window, y, x);
		envelope_normal_window(win);
	}

	return new_panel(win->window);
}

static GntWidget *
find_widget(const char *wname)
{
	const GList *iter = gwm->window_list();
	for (; iter; iter = iter->next) {
		GntWidget *widget = iter->data;
		const char *name = gnt_widget_get_name(widget);
		if (name && strcmp(name, wname) == 0) {
			return widget;
		}
	}
	return NULL;
}

static gboolean
give_the_darned_focus(gpointer w)
{
	gwm->give_focus(w);
	return FALSE;
}

static const char*
s_key_pressed(const char *key)
{
	/* Alt+b to toggle the buddylist */
	if (key[0] == 27 && key[1] == 'b' && key[2] == '\0') {
		GntWidget *w = find_widget("buddylist");
		if (w == NULL) {
			gg_blist_show();
			w = find_widget("buddylist");
			g_timeout_add(0, give_the_darned_focus, w);
		} else {
			gnt_widget_destroy(w);
		}
		return NULL;
	}
	return key;
}

static gboolean
s_mouse_clicked(GntMouseEvent event, int cx, int cy, GntWidget *widget)
{
	int x, y, w, h;

	if (!widget)
		return FALSE;       /* This might a place to bring up a context menu */
	
	if (event != GNT_LEFT_MOUSE_DOWN ||
			GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_NO_BORDER))
		return FALSE;       /* For now, just the left-button to close a window */
	
	gnt_widget_get_position(widget, &x, &y);
	gnt_widget_get_size(widget, &w, &h);

	if (cy == y && cx == x + w - 3) {
		gnt_widget_destroy(widget);
		return TRUE;
	}
	return FALSE;
}

static void
s_window_update(PANEL *panel, GntWidget *window)
{
	const char *name = gnt_widget_get_name(window);
	if (name && strcmp(name, "buddylist"))
		envelope_normal_window(window);
}

void gntwm_init(GntWM *wm);
void gntwm_init(GntWM *wm)
{
	gwm = wm;
	wm->new_window = s_new_window;
	wm->window_resized = s_resize_window;
	wm->key_pressed = s_key_pressed;
	wm->mouse_clicked = s_mouse_clicked;
	wm->window_update = s_window_update;
}

