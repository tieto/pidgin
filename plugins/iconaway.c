#include "config.h"
#include "gaim.h"

#include <gdk/gdkx.h>
#include <X11/Xlib.h>

void *handle;

extern GtkWidget *imaway;
extern GtkWidget *blist;
extern GtkWidget *all_chats;
extern GtkWidget *all_convos;

#ifdef USE_APPLET
extern void applet_destroy_buddy();
#endif

void iconify_windows(struct gaim_connection *gc, char *state, char *message, void *data) {
	if (!imaway)
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
	if (all_convos)
		XIconifyWindow(GDK_DISPLAY(),
				GDK_WINDOW_XWINDOW(all_convos->window),
				((_XPrivDisplay)GDK_DISPLAY())->default_screen);
	if (all_chats)
		XIconifyWindow(GDK_DISPLAY(),
				GDK_WINDOW_XWINDOW(all_chats->window),
				((_XPrivDisplay)GDK_DISPLAY())->default_screen);
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
