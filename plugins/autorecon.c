#define GAIM_PLUGINS
#include "gaim.h"
#include <gtk/gtk.h>

extern GtkWidget *imaway;

static int away_state;
static int forced_off = 0;
static struct away_message *last_away = NULL;
GSList *reconnects = NULL;
GSList *recontim = NULL;

char *name() {
	return "Auto Reconnect";
}

char *description() {
	return "When AOL kicks you off, this auto-reconnects you.";
}

static void now_online(struct gaim_connection *gc, void *m) {
	gint place;
	guint recon;
	if (!g_slist_find(reconnects, gc->user))
		return;
	place = g_slist_index(reconnects, gc->user);
	recon = (guint)g_slist_nth(recontim, place);
	reconnects = g_slist_remove(reconnects, gc->user);
	recontim = g_slist_remove(recontim, (void *)recon);
	if (away_state) do_away_message(NULL, last_away);
}

static void do_signon(struct aim_user *u) {
	gint place;
	guint recon;
	place = g_slist_index(reconnects, u);
	recon = (guint)g_slist_nth(recontim, place);
	gtk_timeout_remove(recon);
	forced_off = 0;
	serv_login(u);
}

static void reconnect(struct gaim_connection *gc, void *m) {
	guint recon;
	if (g_slist_find(reconnects, gc->user))
		return;
	recon = gtk_timeout_add(8000, (GtkFunction)do_signon, gc->user);
	reconnects = g_slist_append(reconnects, gc->user);
	recontim = g_slist_append(recontim, (void *)recon);
	forced_off = 1;
}

static void away_toggle(void *m) {
	if ((int)m == 1) {
		last_away = awaymessage;
		away_state = 1;
	} else if (!forced_off)
		away_state = 0;
}

char *gaim_plugin_init(GModule *handle) {
	if (imaway) {
		away_state = 1;
		last_away = awaymessage;
	} else
		away_state = 0;

	gaim_signal_connect(handle, event_away, away_toggle, (void *)1);
	gaim_signal_connect(handle, event_back, away_toggle, (void *)0);
	gaim_signal_connect(handle, event_signoff, reconnect, NULL);
	gaim_signal_connect(handle, event_signon, now_online, NULL);

	return NULL;
}
