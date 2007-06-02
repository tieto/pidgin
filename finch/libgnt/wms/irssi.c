/**
 * 1. Buddylist and conversation windows are borderless and full height of the screen.
 * 2. Conversation windows will have (full-screen-width - buddylist-width) width
 * 	- It's possible to auto-resize the conversation windows when the buddylist
 * 	  is closed/opened/resized to keep this always true. But resizing the textview
 * 	  in conversation window is rather costly, especially when there's a lot of text
 * 	  in the scrollback. So it's not done yet.
 * 3. All the other windows are always centered.
 */
#include <string.h>
#include <sys/types.h>

#include "gnt.h"
#include "gntbox.h"
#include "gntmenu.h"
#include "gntstyle.h"
#include "gntwm.h"
#include "gntwindow.h"
#include "gntlabel.h"

#define TYPE_IRSSI				(irssi_get_gtype())

typedef struct _Irssi
{
	GntWM inherit;
} Irssi;

typedef struct _IrssiClass
{
	GntWMClass inherit;
} IrssiClass;

GType irssi_get_gtype(void);
void gntwm_init(GntWM **wm);

static void (*org_new_window)(GntWM *wm, GntWidget *win);

/* This is changed whenever the buddylist is opened/closed or resized. */
static int buddylistwidth;

static gboolean
is_budddylist(GntWidget *win)
{
	const char *name = gnt_widget_get_name(win);
	if (name && strcmp(name, "buddylist") == 0)
		return TRUE;
	return FALSE;
}

static void
remove_border_set_position_size(GntWM *wm, GntWidget *win, int x, int y, int w, int h)
{
	gnt_box_set_toplevel(GNT_BOX(win), FALSE);
	GNT_WIDGET_SET_FLAGS(win, GNT_WIDGET_CAN_TAKE_FOCUS);

	gnt_widget_set_position(win, x, y);
	mvwin(win->window, y, x);
	gnt_widget_set_size(win, (w < 0) ? -1 : w + 2, h + 2);
}

static void
irssi_new_window(GntWM *wm, GntWidget *win)
{
	const char *name;

	name = gnt_widget_get_name(win);
	if (!name || strcmp(name, "conversation-window")) {
		if (!GNT_IS_MENU(win) && !GNT_WIDGET_IS_FLAG_SET(win, GNT_WIDGET_TRANSIENT)) {
			if ((!name || strcmp(name, "buddylist"))) {
				int w, h, x, y;
				gnt_widget_get_size(win, &w, &h);
				x = (getmaxx(stdscr) - w) / 2;
				y = (getmaxy(stdscr) - h) / 2;
				gnt_widget_set_position(win, x, y);
				mvwin(win->window, y, x);
			} else {
				remove_border_set_position_size(wm, win, 0, 0, -1, getmaxy(stdscr) - 1);
				gnt_widget_get_size(win, &buddylistwidth, NULL);
				mvwvline(stdscr, 0, buddylistwidth, ACS_VLINE | COLOR_PAIR(GNT_COLOR_NORMAL), getmaxy(stdscr) - 1);
				buddylistwidth++;
			}
		}
		org_new_window(wm, win);
		return;
	}

	/* The window we have here is a conversation window. */
	remove_border_set_position_size(wm, win, buddylistwidth, 0,
			getmaxx(stdscr) - buddylistwidth, getmaxy(stdscr) - 1);
	org_new_window(wm, win);
}

static void
irssi_window_resized(GntWM *wm, GntNode *node)
{
	if (!is_budddylist(node->me))
		return;

	if (buddylistwidth)
		mvwvline(stdscr, 0, buddylistwidth - 1,
				' ' | COLOR_PAIR(GNT_COLOR_NORMAL), getmaxy(stdscr) - 1);
	gnt_widget_get_size(node->me, &buddylistwidth, NULL);
	mvwvline(stdscr, 0, buddylistwidth, ACS_VLINE | COLOR_PAIR(GNT_COLOR_NORMAL), getmaxy(stdscr) - 1);
	buddylistwidth++;
}

static gboolean
irssi_close_window(GntWM *wm, GntWidget *win)
{
	if (is_budddylist(win))
		buddylistwidth = 0;
	return FALSE;
}

static void
irssi_class_init(IrssiClass *klass)
{
	GntWMClass *pclass = GNT_WM_CLASS(klass);

	org_new_window = pclass->new_window;

	pclass->new_window = irssi_new_window;
	pclass->window_resized = irssi_window_resized;
	pclass->close_window = irssi_close_window;

	gnt_style_read_actions(G_OBJECT_CLASS_TYPE(klass), GNT_BINDABLE_CLASS(klass));
	GNTDEBUG;
}

void gntwm_init(GntWM **wm)
{
	buddylistwidth = 0;
	*wm = g_object_new(TYPE_IRSSI, NULL);
}

GType irssi_get_gtype(void)
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(IrssiClass),
			NULL,           /* base_init		*/
			NULL,           /* base_finalize	*/
			(GClassInitFunc)irssi_class_init,
			NULL,
			NULL,           /* class_data		*/
			sizeof(Irssi),
			0,              /* n_preallocs		*/
			NULL,	        /* instance_init	*/
			NULL
		};

		type = g_type_register_static(GNT_TYPE_WM,
		                              "GntIrssi",
		                              &info, 0);
	}

	return type;
}

