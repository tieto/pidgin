#define GAIM_PLUGINS
#include "gaim.h"
#include "prpl.h"
#include <gtk/gtk.h>

char *name() {
	return "Auto Reconnect";
}

char *description() {
	return "When you are kicked offline, this reconnects you.";
}

static gboolean do_signon(struct aim_user *u) {
	if (!g_list_index(aim_users, u))
		return FALSE;
	serv_login(u);
	return FALSE;
}

static void reconnect(struct gaim_connection *gc, void *m) {
	if (gc->wants_to_die)
		gtk_timeout_add(8000, (GtkFunction)do_signon, gc->user);
}

char *gaim_plugin_init(GModule *handle) {
	gaim_signal_connect(handle, event_signoff, reconnect, NULL);

	return NULL;
}
