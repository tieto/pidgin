#include "internal.h"

#include "debug.h"
#include "log.h"
#include "plugin.h"
#include "util.h"
#include "version.h"

#include "gtkconv.h"
#include "gtkplugin.h"

#include <time.h>

static GaimPluginPrefFrame *
get_plugin_pref_frame(GaimPlugin *plugin)
{
	GaimPluginPrefFrame *frame;
	GaimPluginPref *ppref;

	frame = gaim_plugin_pref_frame_new();

	ppref = gaim_plugin_pref_new_with_label(_("Timestamp Format Options"));
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_name_and_label(
			"/plugins/gtk/timestamp_format/force_24hr",
			_("_Force (traditional " PIDGIN_NAME ") 24-hour time format"));
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_label(_("Show dates in..."));
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_name_and_label(
			"/plugins/gtk/timestamp_format/use_dates/conversation",
			_("Co_nversations:"));
        gaim_plugin_pref_set_type(ppref, GAIM_PLUGIN_PREF_CHOICE);
        gaim_plugin_pref_add_choice(ppref, _("For delayed messages"), "automatic");
        gaim_plugin_pref_add_choice(ppref, _("For delayed messages and in chats"), "chats");
        gaim_plugin_pref_add_choice(ppref, _("Always"), "always");
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_name_and_label(
			"/plugins/gtk/timestamp_format/use_dates/log",
			_("_Message Logs:"));
        gaim_plugin_pref_set_type(ppref, GAIM_PLUGIN_PREF_CHOICE);
        gaim_plugin_pref_add_choice(ppref, _("For delayed messages"), "automatic");
        gaim_plugin_pref_add_choice(ppref, _("For delayed messages and in chats"), "chats");
        gaim_plugin_pref_add_choice(ppref, _("Always"), "always");
	gaim_plugin_pref_frame_add(frame, ppref);

	return frame;
}

static char *timestamp_cb_common(GaimConversation *conv,
                                 time_t t,
                                 gboolean show_date,
                                 gboolean force,
                                 const char *dates)
{
	g_return_val_if_fail(conv != NULL, NULL);
	g_return_val_if_fail(dates != NULL, NULL);

	if (show_date ||
	    !strcmp(dates, "always") ||
	    (gaim_conversation_get_type(conv) == GAIM_CONV_TYPE_CHAT && !strcmp(dates, "chats")))
	{
		struct tm *tm = localtime(&t);
		if (force)
			return g_strdup(gaim_utf8_strftime("%Y-%m-%d %H:%M:%S", tm));
		else
			return g_strdup(gaim_date_format_long(tm));
	}

	if (force)
	{
		struct tm *tm = localtime(&t);
		return g_strdup(gaim_utf8_strftime("%H:%M:%S", tm));
	}

	return NULL;
}

static char *conversation_timestamp_cb(GaimConversation *conv,
                                       time_t t, gboolean show_date, gpointer data)
{
	gboolean force = gaim_prefs_get_bool(
				"/plugins/gtk/timestamp_format/force_24hr");
	const char *dates = gaim_prefs_get_string(
				"/plugins/gtk/timestamp_format/use_dates/conversation");

	g_return_val_if_fail(conv != NULL, NULL);

	return timestamp_cb_common(conv, t, show_date, force, dates);
}

static char *log_timestamp_cb(GaimLog *log, time_t t, gboolean show_date, gpointer data)
{
	gboolean force = gaim_prefs_get_bool(
				"/plugins/gtk/timestamp_format/force_24hr");
	const char *dates = gaim_prefs_get_string(
				"/plugins/gtk/timestamp_format/use_dates/log");

	g_return_val_if_fail(log != NULL, NULL);

	return timestamp_cb_common(log->conv, t, show_date, force, dates);
}

static gboolean
plugin_load(GaimPlugin *plugin)
{
	gaim_signal_connect(pidgin_conversations_get_handle(), "conversation-timestamp",
	                    plugin, GAIM_CALLBACK(conversation_timestamp_cb), NULL);
	gaim_signal_connect(gaim_log_get_handle(), "log-timestamp",
	                    plugin, GAIM_CALLBACK(log_timestamp_cb), NULL);
	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	return TRUE;
}

static GaimPluginUiInfo prefs_info = {
        get_plugin_pref_frame,
	0,   /* page num (Reserved) */
	NULL /* frame (Reserved) */
};

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,                             /**< type           */
	PIDGIN_PLUGIN_TYPE,                               /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	NULL,                                             /**< id             */
	N_("Message Timestamp Formats"),                  /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Customizes the message timestamp formats."),
	                                                  /**  description    */
	N_("This plugin allows the user to customize "
	   "conversation and logging message timestamp "
	   "formats."),
	"Richard Laager <rlaager@users.sf.net>",          /**< author         */
	GAIM_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL,                                             /**< extra_info     */
	&prefs_info,                                      /**< prefs_info     */
	NULL                                              /**< actions        */
};

static void
init_plugin(GaimPlugin *plugin)
{
	gaim_prefs_add_none("/plugins/gtk");
	gaim_prefs_add_none("/plugins/gtk/timestamp_format");

	gaim_prefs_add_bool("/plugins/gtk/timestamp_format/force_24hr", TRUE);

	gaim_prefs_add_none("/plugins/gtk/timestamp_format/use_dates");
	gaim_prefs_add_string("/plugins/gtk/timestamp_format/use_dates/conversation", "automatic");
	gaim_prefs_add_string("/plugins/gtk/timestamp_format/use_dates/log", "automatic");
}

GAIM_INIT_PLUGIN(timestamp_format, init_plugin, info)
