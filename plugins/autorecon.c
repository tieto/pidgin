#include "config.h"

#if 0
#ifndef GAIM_PLUGINS
#define GAIM_PLUGINS
#endif
#endif

#include "gaim.h"
#include "prpl.h"

#ifdef _WIN32
#include "win32dep.h"
#endif

#define AUTORECON_PLUGIN_ID "core-autorecon"

G_MODULE_IMPORT GSList *gaim_accounts;

#define INITIAL 8000
#define MAXTIME 2048000

static GHashTable *hash = NULL;

static guint tim = 0;

static gboolean do_signon(gpointer data) {
	struct gaim_account *account = data;
	debug_printf("do_signon called\n");

	if (g_slist_index(gaim_accounts, account) < 0)
		return FALSE;
	debug_printf("calling serv_login\n");
	serv_login(account);
	debug_printf("done calling serv_login\n");
	tim = 0;
	return FALSE;
}

static void reconnect(struct gaim_connection *gc, void *m) {
	if (!gc->wants_to_die) {
		int del;
		del = (int)g_hash_table_lookup(hash, gc->account);
		if (!del)
			del = INITIAL;
		else
			del = MIN(2 * del, MAXTIME);
		tim = g_timeout_add(del, do_signon, gc->account);
		g_hash_table_insert(hash, gc->account, (gpointer)del);
	} else {
		g_hash_table_remove(hash, gc->account);
	}
}

static gboolean
plugin_load(GaimPlugin *plugin)
{
	hash = g_hash_table_new(g_int_hash, g_int_equal);

	gaim_signal_connect(plugin, event_signoff, reconnect, NULL);

	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	if (tim)
		g_source_remove(tim);

	gaim_signal_disconnect(plugin, event_signoff, reconnect);

	g_hash_table_destroy(hash);

	hash = NULL;
	tim = 0;

	return TRUE;
}

static GaimPluginInfo info =
{
	2,                                                /**< api_version    */
	GAIM_PLUGIN_STANDARD,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	AUTORECON_PLUGIN_ID,                              /**< id             */
	N_("Auto-Reconnect"),                             /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("When you are kicked offline, this reconnects you."), 
	                                                  /**  description    */
	N_("When you are kicked offline, this reconnects you."), 
	"Eric Warmenhoven <eric@warmenhoven.org>",        /**< author         */
	WEBSITE,                                          /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL                                              /**< extra_info     */
};

static void
__init_plugin(GaimPlugin *plugin)
{
}

GAIM_INIT_PLUGIN(autorecon, __init_plugin, info);
