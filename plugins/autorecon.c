#define GAIM_PLUGINS
#include "gaim.h"
#include <gtk/gtk.h>

extern GtkWidget *imaway;

static int recon;
static int away_state;
static int forced_off = 0;
static struct away_message *last_away;

char *name() {
	return "Auto Reconnect";
}

char *description() {
	return "When AOL kicks you off, this auto-reconnects you.";
}

extern void dologin(GtkWidget *, GtkWidget *);

void do_signon() {
	if (!blist)
		dologin(NULL, NULL);
	if (!USE_OSCAR) {
		if (blist) {
			gtk_timeout_remove(recon);
			forced_off = 0;
			if (away_state)
				do_away_message(NULL, last_away);
			return;
		}
	} else {
		gtk_timeout_remove(recon);
		forced_off = 0;
		if (blist && away_state) do_away_message(NULL, last_away);
		return;
	}
}

void reconnect(void *m) {
	recon = gtk_timeout_add(8000, (GtkFunction)do_signon, NULL);
	forced_off = 1;
}

void away_toggle(void *m) {
	if ((int)m == 1) {
		last_away = awaymessage;
		away_state = 1;
	} else if (!forced_off)
		away_state = 0;
}

void gaim_plugin_init(void *handle) {
	if (imaway) {
		away_state = 1;
		last_away = awaymessage;
	} else
		away_state = 0;

	gaim_signal_connect(handle, event_away, away_toggle, (void *)1);
	gaim_signal_connect(handle, event_back, away_toggle, (void *)0);
	gaim_signal_connect(handle, event_signoff, reconnect, NULL);
}
