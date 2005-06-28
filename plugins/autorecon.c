#include "internal.h"

#include "connection.h"
#include "debug.h"
#include "pluginpref.h"
#include "prpl.h"
#include "signals.h"
#include "version.h"


#define AUTORECON_PLUGIN_ID "core-autorecon"

#define INITIAL 8000
#define MAXTIME 2048000

typedef struct {
	int delay;
	guint timeout;
} GaimAutoRecon;

static GHashTable *hash = NULL;

static GSList *accountReconnecting = NULL;

#define AUTORECON_OPT  "/plugins/core/autorecon"
#define OPT_HIDE_CONNECTED           AUTORECON_OPT "/hide_connected_error"
#define OPT_HIDE_CONNECTING          AUTORECON_OPT "/hide_connecting_error"
#define OPT_RESTORE_STATE            AUTORECON_OPT "/restore_state"
#define OPT_HIDE_RECONNECTING_DIALOG AUTORECON_OPT "/hide_reconnecting_dialog"

/* storage of original (old_ops) and modified (new_ops) ui ops to allow us to
   intercept calls to report_disconnect */
static GaimConnectionUiOps *old_ops = NULL;
static GaimConnectionUiOps *new_ops = NULL;

static void
connect_progress(GaimConnection *gc, const char *text,
				 size_t step, size_t step_count)
{
	if (old_ops == NULL || old_ops->connect_progress == NULL) {
		/* there's nothing to call through to, so don't bother
		   checking prefs */
		return;
	} else if (gaim_prefs_get_bool(OPT_HIDE_RECONNECTING_DIALOG) && accountReconnecting &&
			g_slist_find(accountReconnecting, gc->account)) {
		/* this is a reconnecting, and we're hiding those */
		gaim_debug(GAIM_DEBUG_INFO, "autorecon",
			"hide connecting dialog while reconnecting\n");
		return;
	}

	old_ops->connect_progress(gc, text, step, step_count);
}

static void
connected(GaimConnection *gc)
{
	if (old_ops == NULL || old_ops->connected == NULL) {
		/* there's nothing to call through to, so don't bother
		   checking prefs */
		return;
	} else if (gaim_prefs_get_bool(OPT_HIDE_RECONNECTING_DIALOG) && accountReconnecting &&
			g_slist_find(accountReconnecting, gc->account)) {
		/* this is a reconnecting, and we're hiding those */
		gaim_debug(GAIM_DEBUG_INFO, "autorecon",
			"hide connecting dialog while reconnecting\n");
		return;
	}

	old_ops->connected(gc);
}

static void
disconnected(GaimConnection *gc)
{
	if (old_ops == NULL || old_ops->disconnected == NULL) {
		/* there's nothing to call through to, so don't bother
		   checking prefs */
		return;
	} else if (gaim_prefs_get_bool(OPT_HIDE_RECONNECTING_DIALOG) && accountReconnecting &&
			g_slist_find(accountReconnecting, gc->account)) {
		/* this is a reconnecting, and we're hiding those */
		gaim_debug(GAIM_DEBUG_INFO, "autorecon",
			"hide connecting dialog while reconnecting\n");
		return;
	}

	old_ops->disconnected(gc);
}

static void
notice(GaimConnection *gc, const char *text)
{
	if (old_ops == NULL || old_ops->notice == NULL) {
		/* there's nothing to call through to, so don't bother
		   checking prefs */
		return;
	} else if (gaim_prefs_get_bool(OPT_HIDE_RECONNECTING_DIALOG) && accountReconnecting &&
			g_slist_find(accountReconnecting, gc->account)) {
		/* this is a reconnecting, and we're hiding those */
		gaim_debug(GAIM_DEBUG_INFO, "autorecon",
			"hide connecting dialog while reconnecting\n");
	}

	old_ops->notice(gc, text);
}

static void
report_disconnect(GaimConnection *gc, const char *text)
{
	if (old_ops == NULL || old_ops->report_disconnect == NULL) {
		/* there's nothing to call through to, so don't bother
		   checking prefs */
		return;

	} else if (gc->state == GAIM_CONNECTED
			&& gaim_prefs_get_bool(OPT_HIDE_CONNECTED)) {
		/* this is a connected error, and we're hiding those */
		gaim_debug(GAIM_DEBUG_INFO, "autorecon",
				"hid disconnect error message (%s)\n", text);
		return;

	} else if (gc->state == GAIM_CONNECTING
			&& gaim_prefs_get_bool(OPT_HIDE_CONNECTING)) {
		/* this is a connecting error, and we're hiding those */
		gaim_debug(GAIM_DEBUG_INFO, "autorecon",
			"hid error message while connecting (%s)\n", text);
		return;
	}

	/* if we haven't returned by now, then let's pass to the real
	   function */
	old_ops->report_disconnect(gc, text);
}


static gboolean
do_signon(gpointer data)
{
	GaimAccount *account = data;
	GaimAutoRecon *info;

	gaim_debug(GAIM_DEBUG_INFO, "autorecon", "do_signon called\n");
	g_return_val_if_fail(account != NULL, FALSE);
	info = g_hash_table_lookup(hash, account);

	if (g_list_index(gaim_accounts_get_all(), account) < 0)
		return FALSE;

	if (info)
		info->timeout = 0;

	gaim_debug(GAIM_DEBUG_INFO, "autorecon", "calling gaim_account_connect\n");
	gaim_account_connect(account);
	gaim_debug(GAIM_DEBUG_INFO, "autorecon", "done calling gaim_account_connect\n");

	return FALSE;
}


static void
reconnect(GaimConnection *gc, void *m)
{
	GaimAccount *account;
	GaimAutoRecon *info;
	GSList* listAccount;

	g_return_if_fail(gc != NULL);
	account = gaim_connection_get_account(gc);
	info = g_hash_table_lookup(hash, account);
	if (accountReconnecting)
		listAccount = g_slist_find(accountReconnecting, account);
	else
		listAccount = NULL;

	if (!gc->wants_to_die) {
		if (info == NULL) {
			info = g_new0(GaimAutoRecon, 1);
			g_hash_table_insert(hash, account, info);
			info->delay = INITIAL;
		} else {
			info->delay = MIN(2 * info->delay, MAXTIME);
			if (info->timeout != 0)
				g_source_remove(info->timeout);
		}
		info->timeout = g_timeout_add(info->delay, do_signon, account);

		if (!listAccount)
			accountReconnecting = g_slist_prepend(accountReconnecting, account);
	} else if (info != NULL) {
		g_hash_table_remove(hash, account);

		if (listAccount)
			accountReconnecting = g_slist_delete_link(accountReconnecting, listAccount);
	}
}

static void
reconnected(GaimConnection *gc, void *m)
{
	GaimAccount *account;

	g_return_if_fail(gc != NULL);

	account = gaim_connection_get_account(gc);

	g_hash_table_remove(hash, account);

	if (accountReconnecting == NULL)
		return;

	accountReconnecting = g_slist_remove(accountReconnecting, account);
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
	gaim_debug_register_category("autorecon");

	/* this was the suggested way to override a single function of the
	real ui ops. However, there's a mild concern of having more than one
	bit of code making a new ui op call-through copy. If plugins A and B
	both override the ui ops (in that order), B thinks that the
	overridden ui ops A created was the original. If A unloads first,
	and swaps out and frees its overridden version, then B is calling
	through to a free'd ui op. There needs to be a way to "stack up"
	overridden ui ops or something... I have a good idea of how to write
	such a creature if someone wants it done.  - siege 2004-04-20 */

	/* get old ops, make a copy with a minor change */
	old_ops = gaim_connections_get_ui_ops();
	new_ops = (GaimConnectionUiOps *) g_memdup(old_ops,
			sizeof(GaimConnectionUiOps));
	new_ops->connect_progress = connect_progress;
	new_ops->connected = connected;
	new_ops->disconnected = disconnected;
	new_ops->notice = notice;
	new_ops->report_disconnect = report_disconnect;
	gaim_connections_set_ui_ops(new_ops);

	hash = g_hash_table_new_full(g_int_hash, g_int_equal, NULL,
			free_auto_recon);

	accountReconnecting = NULL;

	gaim_signal_connect(gaim_connections_get_handle(), "signed-off",
			plugin, GAIM_CALLBACK(reconnect), NULL);

	gaim_signal_connect(gaim_connections_get_handle(), "signed-on",
			plugin, GAIM_CALLBACK(reconnected), NULL);

	return TRUE;
}


static gboolean
plugin_unload(GaimPlugin *plugin)
{
	g_hash_table_destroy(hash);
	hash = NULL;

	if (accountReconnecting) {
		g_slist_free(accountReconnecting);
		accountReconnecting = NULL;
	}

	gaim_connections_set_ui_ops(old_ops);
	g_free(new_ops);
	old_ops = new_ops = NULL;

	gaim_debug_unregister_category("autorecon");

	return TRUE;
}


static
GaimPluginPrefFrame *get_plugin_pref_frame(GaimPlugin *plugin)
{
	GaimPluginPrefFrame *frame = gaim_plugin_pref_frame_new();
	GaimPluginPref *pref;

	pref = gaim_plugin_pref_new_with_label(_("Error Message Suppression"));
	gaim_plugin_pref_frame_add(frame, pref);

	pref = gaim_plugin_pref_new_with_name_and_label(OPT_HIDE_CONNECTED,
		_("Hide Disconnect Errors"));
	gaim_plugin_pref_frame_add(frame, pref);

	pref = gaim_plugin_pref_new_with_name_and_label(OPT_HIDE_CONNECTING,
		_("Hide Login Errors"));
	gaim_plugin_pref_frame_add(frame, pref);

	pref = gaim_plugin_pref_new_with_name_and_label(OPT_HIDE_RECONNECTING_DIALOG,
		_("Hide Reconnecting Dialog"));
	gaim_plugin_pref_frame_add(frame, pref);

	return frame;
}


static GaimPluginUiInfo pref_info = {
	get_plugin_pref_frame
};


static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
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
	GAIM_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL,                                             /**< extra_info     */
	&pref_info,                                       /**< prefs_info     */
	NULL
};


static void
init_plugin(GaimPlugin *plugin)
{
	gaim_prefs_add_none(AUTORECON_OPT);
	gaim_prefs_add_bool(OPT_HIDE_CONNECTED, FALSE);
	gaim_prefs_add_bool(OPT_HIDE_CONNECTING, FALSE);
	gaim_prefs_add_bool(OPT_HIDE_RECONNECTING_DIALOG, FALSE);
	gaim_prefs_remove(OPT_RESTORE_STATE);
}

GAIM_INIT_PLUGIN(autorecon, init_plugin, info)
