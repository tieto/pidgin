#include "internal.h"

#include "connection.h"
#include "debug.h"
#include "prpl.h"

#define AUTORECON_PLUGIN_ID "core-autorecon"

#define INITIAL 8000
#define MAXTIME 2048000

typedef struct {
	int delay;
	guint timeout;
} GaimAutoRecon;

static GHashTable *hash = NULL;

static gboolean do_signon(gpointer data) {
	GaimAccount *account = data;
	GaimAutoRecon *info;

	gaim_debug(GAIM_DEBUG_INFO, "autorecon", "do_signon called\n");
	g_return_val_if_fail(account != NULL, FALSE);
	info = g_hash_table_lookup(hash, account);

	if (g_list_index(gaim_accounts_get_all(), account) < 0)
		return FALSE;

	gaim_debug(GAIM_DEBUG_INFO, "autorecon", "calling gaim_account_connect\n");
	gaim_account_connect(account);
	gaim_debug(GAIM_DEBUG_INFO, "autorecon", "done calling gaim_account_connect\n");

	info->timeout = 0;

	return FALSE;
}

static void reconnect(GaimConnection *gc, void *m) {
	GaimAccount *account;
	GaimAutoRecon *info;

	g_return_if_fail(gc != NULL);
	account = gaim_connection_get_account(gc);
	info = g_hash_table_lookup(hash, account);

	if (!gc->wants_to_die) {
		if (info == NULL) {
			info = g_new0(GaimAutoRecon, 1);
			g_hash_table_insert(hash, account, info);
			info->delay = INITIAL;
		} else
			info->delay = MIN(2 * info->delay, MAXTIME);
		info->timeout = g_timeout_add(info->delay, do_signon, account);
	} else if (info != NULL) {
		g_hash_table_remove(hash, account);
		g_free(info);
	}
}

static void
free_auto_recon(gpointer data)
{
	GaimAutoRecon *info = data;

	if (info->timeout != 0)
		g_source_remove(info->timeout);

	g_free(info);
}

static gboolean
plugin_load(GaimPlugin *plugin)
{
	hash = g_hash_table_new_full(g_int_hash, g_int_equal, NULL, free_auto_recon);

	gaim_signal_connect(plugin, event_signoff, reconnect, NULL);

	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	gaim_signal_disconnect(plugin, event_signoff, reconnect);

	g_hash_table_destroy(hash);
	hash = NULL;

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
init_plugin(GaimPlugin *plugin)
{
}

GAIM_INIT_PLUGIN(autorecon, init_plugin, info)
