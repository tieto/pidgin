#define GAIM_PLUGINS
#include "gaim.h"
#include "prpl.h"

#define INITIAL 8000
#define MAXTIME 1024000

static GHashTable *hash = NULL;

static guint tim = 0;

char *name() {
	return "Auto Reconnect";
}

char *description() {
	return "When you are kicked offline, this reconnects you.";
}

static gboolean do_signon(gpointer data) {
	struct aim_user *u = data;
	if (g_list_index(aim_users, u) < 0)
		return FALSE;
	serv_login(u);
	tim = 0;
	return FALSE;
}

static void reconnect(struct gaim_connection *gc, void *m) {
	if (!gc->wants_to_die) {
		int del;
		del = (int)g_hash_table_lookup(hash, gc->user);
		if (!del)
			del = INITIAL;
		else
			del = 2 * del;
		tim = g_timeout_add(del, do_signon, gc->user);
		g_hash_table_insert(hash, gc->user, (gpointer)del);
	} else {
		g_hash_table_remove(hash, gc->user);
	}
}

char *gaim_plugin_init(GModule *handle) {
	hash = g_hash_table_new(g_int_hash, g_int_equal);

	gaim_signal_connect(handle, event_signoff, reconnect, NULL);

	return NULL;
}

void gaim_plugin_remove() {
	if (tim)
		g_source_remove(tim);
	g_hash_table_destroy(hash);
	hash = NULL;
	tim = 0;
}
