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

/*
   I use a struct here, but the visible/invisible isn't yet supported
   in this plugin, so this is more for future implementation of those
   features
*/
typedef struct {
	const char *state;
	const char *message;
} GaimAwayState;

static GHashTable *hash = NULL;
static GHashTable *awayStates = NULL;


#define AUTORECON_OPT  "/plugins/core/autorecon"
#define OPT_HIDE_CONNECTED   AUTORECON_OPT "/hide_connected_error"
#define OPT_HIDE_CONNECTING  AUTORECON_OPT "/hide_connecting_error"
#define OPT_RESTORE_STATE    AUTORECON_OPT "/restore_state"


/* storage of original (old_ops) and modified (new_ops) ui ops to allow us to
   intercept calls to report_disconnect */
static GaimConnectionUiOps *old_ops = NULL;
static GaimConnectionUiOps *new_ops = NULL;


static void report_disconnect(GaimConnection *gc, const char *text) {

	if(old_ops == NULL || old_ops->report_disconnect == NULL) {
		/* there's nothing to call through to, so don't bother
		   checking prefs */
		return;

	} else if(gc->state == GAIM_CONNECTED
			&& gaim_prefs_get_bool(OPT_HIDE_CONNECTED)) {
		/* this is a connected error, and we're hiding those */
		gaim_debug(GAIM_DEBUG_INFO, "autorecon",
				"hid disconnect error message\n");
		return;

	} else if(gc->state == GAIM_CONNECTING
			&& gaim_prefs_get_bool(OPT_HIDE_CONNECTING)) {
		/* this is a connecting error, and we're hiding those */
		gaim_debug(GAIM_DEBUG_INFO, "autorecon",
			"hid error message while connecting\n");
		return;
	}

	/* if we haven't returned by now, then let's pass to the real
	   function */
	old_ops->report_disconnect(gc, text);
}


static gboolean do_signon(gpointer data) {
	GaimAccount *account = data;
	GaimAutoRecon *info;

	gaim_debug(GAIM_DEBUG_INFO, "autorecon", "do_signon called\n");
	g_return_val_if_fail(account != NULL, FALSE);
	info = g_hash_table_lookup(hash, account);

	if (g_list_index(gaim_accounts_get_all(), account) < 0)
		return FALSE;

	if(info)
		info->timeout = 0;

	gaim_debug(GAIM_DEBUG_INFO, "autorecon", "calling gaim_account_connect\n");
	gaim_account_connect(account);
	gaim_debug(GAIM_DEBUG_INFO, "autorecon", "done calling gaim_account_connect\n");

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
		} else {
			info->delay = MIN(2 * info->delay, MAXTIME);
			if (info->timeout != 0)
				g_source_remove(info->timeout);
		}
		info->timeout = g_timeout_add(info->delay, do_signon, account);
	} else if (info != NULL) {
		g_hash_table_remove(hash, account);
	}

	if (gc->wants_to_die) 
		g_hash_table_remove(awayStates, account);
}

static void save_state(GaimAccount *account, const char *state, const char *message) {
	/* Saves whether the account is back/away/visible/invisible */

	GaimAwayState *info;

	if (!strcmp(state,GAIM_AWAY_CUSTOM)) {
		info = g_new0(GaimAwayState, 1);
		info->state = state;
		info->message = message;

		g_hash_table_insert(awayStates, account, info);
	} else if(!strcmp(state,"Back")) 
		g_hash_table_remove(awayStates, account);
}

static void restore_state(GaimConnection *gc, void *m) {
	/* Restore the state to what it was before the disconnect */
	GaimAwayState *info;
	GaimAccount *account;

	g_return_if_fail(gc != NULL && gaim_prefs_get_bool(OPT_RESTORE_STATE));
	account = gaim_connection_get_account(gc);

	info = g_hash_table_lookup(awayStates, account);
	if (info)
		serv_set_away(gc, info->state, info->message);
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
	new_ops->report_disconnect = report_disconnect;
	gaim_connections_set_ui_ops(new_ops);

	hash = g_hash_table_new_full(g_int_hash, g_int_equal, NULL,
			free_auto_recon);

	awayStates = g_hash_table_new(g_int_hash, g_int_equal);

	gaim_signal_connect(gaim_connections_get_handle(), "signed-off",
			plugin, GAIM_CALLBACK(reconnect), NULL);

	gaim_signal_connect(gaim_connections_get_handle(), "signed-on",
			plugin, GAIM_CALLBACK(restore_state), NULL);

	gaim_signal_connect(gaim_accounts_get_handle(), "account-away",
			plugin, GAIM_CALLBACK(save_state), NULL);


	return TRUE;
}


static gboolean
plugin_unload(GaimPlugin *plugin)
{
	gaim_signal_disconnect(gaim_connections_get_handle(), "signed-off",
			plugin, GAIM_CALLBACK(reconnect));

	gaim_signal_disconnect(gaim_connections_get_handle(), "signed-on",
			plugin, GAIM_CALLBACK(restore_state));
	
	gaim_signal_disconnect(gaim_accounts_get_handle(), "account-away",
			plugin, GAIM_CALLBACK(save_state));

	g_hash_table_destroy(hash);
	hash = NULL;

	g_hash_table_destroy(awayStates);
	awayStates = NULL;

	gaim_connections_set_ui_ops(old_ops);
	g_free(new_ops);
	old_ops = new_ops = NULL;

	return TRUE;
}


static GaimPluginPrefFrame *get_plugin_pref_frame(GaimPlugin *plugin) {
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

	pref = gaim_plugin_pref_new_with_name_and_label(OPT_RESTORE_STATE,
		_("Restore Away State On Reconnect"));
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
	gaim_prefs_add_bool(OPT_RESTORE_STATE, TRUE);
}

GAIM_INIT_PLUGIN(autorecon, init_plugin, info)

