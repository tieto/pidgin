#include "gntbox.h"
#include "gntwm.h"

#include "gntblist.h"

#include <string.h>

static GntWM *gwm;

static PANEL *
s_new_window(GntWidget *win)
{
	int x, y, w, h;
	int maxx, maxy;
	const char *name;

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
	} else if (name && strcmp(name, "conversation-window") == 0) {
		/* Put the conversation windows to the far-right */
		x = maxx - w;
		y = 0;
		gnt_widget_set_position(win, x, y);
		mvwin(win->window, y, x);
		gnt_widget_draw(win);
	} else if (!GNT_WIDGET_IS_FLAG_SET(win, GNT_WIDGET_TRANSIENT)) {
		/* In the middle of the screen */
		x = (maxx - w) / 2;
		y = (maxy - h) / 2;

		gnt_widget_set_position(win, x, y);
		mvwin(win->window, y, x);
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

static const char*
s_key_pressed(const char *key)
{
	/* Alt+b to toggle the buddylist */
	if (key[0] == 27 && key[1] == 'b' && key[2] == '\0') {
		GntWidget *w = find_widget("buddylist");
		if (w == NULL) {
			gg_blist_show();
			w = find_widget("buddylist");
			gwm->give_focus(w);
		} else {
			gnt_widget_destroy(w);
		}
		return NULL;
	}
	return key;
}

void gntwm_init(GntWM *wm)
{
	gwm = wm;
	wm->new_window = s_new_window;
	wm->key_pressed = s_key_pressed;
}

