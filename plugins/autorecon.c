/* TODO: save state
 * I'm not at my computer right now, so I'm not going to bother
 * writing it, but if someone wants to before I get back (hint,
 * hint), go for it. Here's how to do it.
 *
 * First, add a global "state", which is either 'away' or not.
 *
 * In gaim_plugin_init, set state, and add two more signal
 * handlers: event_away and event_back, and if you can't figure
 * out what you're supposed to do for them, you shouldn't be
 * editing this plugin.
 *
 * In the reconnect function, if "state" was away, then reset
 * the away message. You may have to remember the away message
 * on your own; I haven't checked yet to see if there's a global
 * that remembers it that isn't erased on signoff/signon.
 *
 * Anyway, that should be it.
 */

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
