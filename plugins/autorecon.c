#define GAIM_PLUGINS
#include "gaim.h"
#include "prpl.h"

#ifdef _WIN32
#include "win32dep.h"
#endif

#define INITIAL 8000
#define MAXTIME 1024000

static GHashTable *hash = NULL;

static guint tim = 0;

static gboolean do_signon(gpointer data) {
	struct aim_user *u = data;
	if (g_slist_index(aim_users, u) < 0)
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
			del = MAX(2 * del, MAXTIME);
		tim = g_timeout_add(del, do_signon, gc->user);
		g_hash_table_insert(hash, gc->user, (gpointer)del);
	} else {
		g_hash_table_remove(hash, gc->user);
	}
}

/*
 *  EXPORTED FUNCTIONS
 */

G_MODULE_EXPORT char *name() {
	return "Auto Reconnect";
}

G_MODULE_EXPORT char *description() {
	return "When you are kicked offline, this reconnects you.";
}

G_MODULE_EXPORT char *gaim_plugin_init(GModule *handle) {
	hash = g_hash_table_new(g_int_hash, g_int_equal);

	gaim_signal_connect(handle, event_signoff, reconnect, NULL);

	return NULL;
}

G_MODULE_EXPORT void gaim_plugin_remove() {
	if (tim)
		g_source_remove(tim);
	g_hash_table_destroy(hash);
	hash = NULL;
	tim = 0;
}
