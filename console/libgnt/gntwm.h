#ifdef HAVE_NCURSESW_INC
#include <ncursesw/panel.h>
#else
#include <panel.h>
#endif

#include "gntwidget.h"

typedef struct _GntWM GntWM;

struct _GntWM
{
	/* This should return a PANEL for the win */
	PANEL *(*new_window)(GntWidget *win);

	/* This is called when a window is being closed */
	gboolean (*close_window)(GntWidget *win);

	/* Can del_panel the old panel and return a new_panel.
	 * Otherwise, this should at least do a replace_panel. */
	PANEL *(*window_resized)(PANEL *pan, GntWidget *win);

	/* This should usually return NULL if the keys were processed by the WM.
	 * If not, the WM can simply return the original string, which will be
	 * processed by the default WM. The custom WM can also return a different
	 * static string for the default WM to process.
	 */
	const char *(*key_pressed)(const char *key);

	gboolean (*mouse_clicked)(GntMouseEvent event, int x, int y, GntWidget *widget);

	/* Whatever the WM wants to do when a window is given focus */
	void (*give_focus)(GntWidget *widget);

	/* If something needs to be uninited */
	void (*gntwm_uninit)();

	/* List of windows. Although the WM can keep a list of its own for the windows,
	 * it'd be better if there was a way to share between the 'core' and the WM.
	 */
	const GList *(*window_list)();
};

