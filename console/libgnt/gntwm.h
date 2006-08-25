#ifdef HAVE_NCURSESW_INC
#include <ncursesw/panel.h>
#else
#include <panel.h>
#endif

#include "gntwidget.h"

typedef struct _GntWM GntWM;

struct _GntWM
{
	PANEL *(*new_window)(GntWidget *win);
	gboolean (*key_pressed)(const char *key);
	gboolean (*mouse_clicked)(void);    /* XXX: haven't decided yet */
	void (*gntwm_uninit)();
};

