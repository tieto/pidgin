#include "../config.h"
#include "gaim.h"

#include <gtk/gtk.h>

void *handle;

extern GtkWidget *imaway;
extern GtkWidget *blist;
extern GtkWidget *all_chats;
extern GtkWidget *all_convos;

#ifdef USE_APPLET
extern void applet_destroy_buddy();
#endif

void iconify_windows(struct gaim_connection *gc, char *state, char *message, void *data) {
	if (!imaway || !gc->away)
		return;
	gtk_window_iconify(GTK_WINDOW(imaway));
	hide_buddy_list();
	if (all_convos)
		gtk_window_iconify(GTK_WINDOW(all_convos));
	if (all_chats)
		gtk_window_iconify(GTK_WINDOW(all_chats));
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
