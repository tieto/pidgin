#define GAIM_PLUGINS
#include "gaim.h"

#include <gdk/gdkx.h>
#include <X11/Xlib.h>

void *handle;

extern GtkWidget *imaway;
extern GtkWidget *blist;

#ifdef USE_APPLET
extern void applet_destroy_buddy();
#endif

void iconify_windows(struct gaim_connection *gc, char *state, char *message, void *data) {
	if (!gc->away)
		return;
	XIconifyWindow(GDK_DISPLAY(),
			GDK_WINDOW_XWINDOW(imaway->window),
			((_XPrivDisplay)GDK_DISPLAY())->default_screen);
#ifdef USE_APPLET
	applet_destroy_buddy();
#else
	XIconifyWindow(GDK_DISPLAY(),
			GDK_WINDOW_XWINDOW(blist->window),
			((_XPrivDisplay)GDK_DISPLAY())->default_screen);
#endif
}

char *gaim_plugin_init(GModule *h) {
	handle = h;

	gaim_signal_connect(handle, event_away, iconify_windows, NULL);

	return NULL;
}

char *name() {
	return "Iconify On Away";
}

char *description() {
	return "Iconifies the away box and the buddy list when you go away.";
}
