#define GAIM_PLUGINS
#include "gaim.h"
#include <gtk/gtk.h>

static int recon;

char *name() {
	return "Auto Reconnect";
}

char *description() {
	return "When AOL kicks you off, this auto-reconnects you.";
}

extern void dologin(GtkWidget *, GtkWidget *);

void do_signon() {
	dologin(NULL, NULL);
	if (query_state() != STATE_OFFLINE) {
		gtk_timeout_remove(recon);
		return;
	}
}

void reconnect(void *m) {
	recon = gtk_timeout_add(2000, (GtkFunction)do_signon, NULL);
}

void gaim_plugin_init(void *handle) {
	gaim_signal_connect(handle, event_signoff, reconnect, NULL);
}
