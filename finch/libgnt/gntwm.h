
#include "gntwidget.h"
#include "gntmenu.h"

#include <panel.h>

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
} GntNode;

typedef struct _GnttWM GntWM;

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

struct _GnttWM
{
	GntBindable inherit;

	GMainLoop *loop;

	GList *list;      /* List of windows ordered on their creation time */
	GList *ordered;   /* List of windows ordered on their focus */

	struct {
		GntWidget *window;
		GntWidget *tree;
	} _list,
		*windows,         /* Window-list window */
		*actions;         /* Action-list window */

	GHashTable *nodes;    /* GntWidget -> GntNode */

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
	/*const GList *(*window_list)();*/

	void (*res1)(void);
	void (*res2)(void);
	void (*res3)(void);
	void (*res4)(void);
};

G_BEGIN_DECLS

GType gnt_wm_get_gtype(void);

void gnt_wm_new_window(GntWM *wm, GntWidget *widget);

void gnt_wm_window_decorate(GntWM *wm, GntWidget *widget);

void gnt_wm_window_close(GntWM *wm, GntWidget *widget);

gboolean gnt_wm_process_input(GntWM *wm, const char *string);

gboolean gnt_wm_process_click(GntWM *wm, GntMouseEvent event, int x, int y, GntWidget *widget);

void gnt_wm_resize_window(GntWM *wm, GntWidget *widget, int width, int height);

void gnt_wm_move_window(GntWM *wm, GntWidget *widget, int x, int y);

void gnt_wm_update_window(GntWM *wm, GntWidget *widget);

void gnt_wm_raise_window(GntWM *wm, GntWidget *widget);

time_t gnt_wm_get_idle_time(void);

G_END_DECLS
