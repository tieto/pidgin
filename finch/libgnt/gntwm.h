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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef GNTWM_H
#define GNTWM_H

#include "gntwidget.h"
#include "gntmenu.h"
#include "gntws.h"

#include <panel.h>
#include <time.h>

#define GNT_TYPE_WM				(gnt_wm_get_gtype())
#define GNT_WM(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_WM, GntWM))
#define GNT_WM_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_WM, GntWMClass))
#define GNT_IS_WM(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_WM))
#define GNT_IS_WM_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_WM))
#define GNT_WM_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_WM, GntWMClass))

typedef enum
{
	GNT_KP_MODE_NORMAL,
	GNT_KP_MODE_RESIZE,
	GNT_KP_MODE_MOVE,
} GntKeyPressMode;

typedef struct
{
	GntWidget *me;

	WINDOW *window;
	int scroll;
	PANEL *panel;
	GntWS *ws;
} GntNode;

typedef struct _GntWM GntWM;

typedef struct _GntPosition
{
	int x;
	int y;
} GntPosition;

/**
 * An application can register actions which will show up in a 'start-menu' like popup
 */
typedef struct _GntAction
{
	const char *label;
	void (*callback)();
} GntAction;

struct _GntWM
{
	GntBindable inherit;

	GMainLoop *loop;

	GList *workspaces;
	GList *tagged; /* tagged windows */
	GntWS *cws;

	struct {
		GntWidget *window;
		GntWidget *tree;
	} _list,
		*windows,         /* Window-list window */
		*actions;         /* Action-list window */

	GHashTable *nodes;    /* GntWidget -> GntNode */
	GHashTable *name_places;    /* window name -> ws*/
	GHashTable *title_places;    /* window title -> ws */

	GList *acts;          /* List of actions */

	/**
	 * There can be at most one menu at a time on the screen.
	 * If there is a menu being displayed, then all the keystrokes will be sent to
	 * the menu until it is closed, either when the user activates a menuitem, or
	 * presses Escape to cancel the menu.
	 */
	GntMenu *menu;        /* Currently active menu */

	/**
	 * 'event_stack' will be set to TRUE when a user-event, ie. a mouse-click
	 * or a key-press is being processed. This variable will be used to determine
	 * whether to give focus to a new window.
	 */
	gboolean event_stack;
	
	GntKeyPressMode mode;

	GHashTable *positions;

	void *res1;
	void *res2;
	void *res3;
	void *res4;
};

typedef struct _GntWMClass GntWMClass;

struct _GntWMClass
{
	GntBindableClass parent;

	/* This is called when a new window is shown */
	void (*new_window)(GntWM *wm, GntWidget *win);

	void (*decorate_window)(GntWM *wm, GntWidget *win);
	/* This is called when a window is being closed */
	gboolean (*close_window)(GntWM *wm, GntWidget *win);

	/* The WM may want to confirm a size for a window first */
	gboolean (*window_resize_confirm)(GntWM *wm, GntWidget *win, int *w, int *h);

	void (*window_resized)(GntWM *wm, GntNode *node);

	/* The WM may want to confirm the position of a window */
	gboolean (*window_move_confirm)(GntWM *wm, GntWidget *win, int *x, int *y);

	void (*window_moved)(GntWM *wm, GntNode *node);

	/* This gets called when:
	 * 	 - the title of the window changes
	 * 	 - the 'urgency' of the window changes
	 */
	void (*window_update)(GntWM *wm, GntNode *node);

	/* This should usually return NULL if the keys were processed by the WM.
	 * If not, the WM can simply return the original string, which will be
	 * processed by the default WM. The custom WM can also return a different
	 * static string for the default WM to process.
	 */
	gboolean (*key_pressed)(GntWM *wm, const char *key);

	gboolean (*mouse_clicked)(GntWM *wm, GntMouseEvent event, int x, int y, GntWidget *widget);

	/* Whatever the WM wants to do when a window is given focus */
	void (*give_focus)(GntWM *wm, GntWidget *widget);

	/* List of windows. Although the WM can keep a list of its own for the windows,
	 * it'd be better if there was a way to share between the 'core' and the WM.
	 */
	/*GList *(*window_list)();*/

	void (*res1)(void);
	void (*res2)(void);
	void (*res3)(void);
	void (*res4)(void);
};

G_BEGIN_DECLS

/**
 * 
 *
 * @return
 */
GType gnt_wm_get_gtype(void);

void gnt_wm_add_workspace(GntWM *wm, GntWS *ws);

gboolean gnt_wm_switch_workspace(GntWM *wm, gint n);
gboolean gnt_wm_switch_workspace_prev(GntWM *wm);
gboolean gnt_wm_switch_workspace_next(GntWM *wm);
void gnt_wm_widget_move_workspace(GntWM *wm, GntWS *neww, GntWidget *widget);
void gnt_wm_set_workspaces(GntWM *wm, GList *workspaces);
GntWS *gnt_wm_widget_find_workspace(GntWM *wm, GntWidget *widget);

/**
 * 
 * @param wm
 * @param widget
 */
void gnt_wm_new_window(GntWM *wm, GntWidget *widget);

/**
 * 
 * @param wm
 * @param widget
 */
void gnt_wm_window_decorate(GntWM *wm, GntWidget *widget);

/**
 * 
 * @param wm
 * @param widget
 */
void gnt_wm_window_close(GntWM *wm, GntWidget *widget);

/**
 * 
 * @param wm
 * @param string
 *
 * @return
 */
gboolean gnt_wm_process_input(GntWM *wm, const char *string);

/**
 * 
 * @param wm
 * @param event
 * @param x
 * @param y
 * @param widget
 *
 * @return
 */
gboolean gnt_wm_process_click(GntWM *wm, GntMouseEvent event, int x, int y, GntWidget *widget);

/**
 * 
 * @param wm
 * @param widget
 * @param width
 * @param height
 */
void gnt_wm_resize_window(GntWM *wm, GntWidget *widget, int width, int height);

/**
 * 
 * @param wm
 * @param widget
 * @param x
 * @param y
 */
void gnt_wm_move_window(GntWM *wm, GntWidget *widget, int x, int y);

/**
 * 
 * @param wm
 * @param widget
 */
void gnt_wm_update_window(GntWM *wm, GntWidget *widget);

/**
 * 
 * @param wm
 * @param widget
 */
void gnt_wm_raise_window(GntWM *wm, GntWidget *widget);

/**
 * 
 * @param wm
 * @param set
 */
void gnt_wm_set_event_stack(GntWM *wm, gboolean set);

void gnt_wm_copy_win(GntWidget *widget, GntNode *node);

/**
 * 
 *
 * @return
 */
time_t gnt_wm_get_idle_time(void);

G_END_DECLS
#endif
