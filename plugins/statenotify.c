#include "internal.h"

#include "blist.h"
#include "conversation.h"
#include "debug.h"

static void
write_status(GaimConnection *gc, char *who, const char *message)
{
	GaimConversation *conv;
	struct buddy *b;
	char buf[256];

	conv = gaim_find_conversation_with_account(who, gc->account);

	if (conv == NULL)
		return;

	if ((b = gaim_find_buddy(gc->account, who)) != NULL)
		who = gaim_get_buddy_alias(b);

	g_snprintf(buf, sizeof(buf), "%s %s", who, message);

	gaim_conversation_write(conv, NULL, buf, -1, WFLAG_SYSTEM, time(NULL));
}

static void
buddy_away_cb(GaimConnection *gc, char *who, void *data)
{
	write_status(gc, who, "has gone away.");
}

static void
buddy_unaway_cb(GaimConnection *gc, char *who, void *data)
{
	write_status(gc, who, "is no longer away.");
}

static void
buddy_idle_cb(GaimConnection *gc, char *who, void *data)
{
	write_status(gc, who, "has become idle.");
}

static void
buddy_unidle_cb(GaimConnection *gc, char *who, void *data)
{
	write_status(gc, who, "is no longer idle.");
}

static gboolean
plugin_load(GaimPlugin *plugin)
{
	gaim_signal_connect(plugin, event_buddy_away,   buddy_away_cb,   NULL);
	gaim_signal_connect(plugin, event_buddy_back,   buddy_unaway_cb, NULL);
	gaim_signal_connect(plugin, event_buddy_idle,   buddy_idle_cb,   NULL);
	gaim_signal_connect(plugin, event_buddy_unidle, buddy_unidle_cb, NULL);

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

	NULL,                                             /**< id             */
	N_("Buddy State Notification"),                   /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Notifies in a conversation window when a buddy goes or returns from "
	   "away or idle."),
	                                                  /**  description    */
	N_("Notifies in a conversation window when a buddy goes or returns from "
	   "away or idle."),
	"Christian Hammond <chipx86@gnupdate.org>",       /**< author         */
	WEBSITE,                                          /**< homepage       */

	plugin_load,                                      /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL                                              /**< extra_info     */
};

static void
__init_plugin(GaimPlugin *plugin)
{
}

GAIM_INIT_PLUGIN(statenotify, __init_plugin, info);
