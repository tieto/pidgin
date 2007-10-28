#include "internal.h"

#include "debug.h"
#include "log.h"
#include "plugin.h"
#include "util.h"
#include "version.h"

#include "gtkconv.h"
#include "gtkplugin.h"

#include <time.h>

static PurplePluginPrefFrame *
get_plugin_pref_frame(PurplePlugin *plugin)
{
	PurplePluginPrefFrame *frame;
	PurplePluginPref *ppref;
	char *tmp;

	frame = purple_plugin_pref_frame_new();

	ppref = purple_plugin_pref_new_with_label(_("Timestamp Format Options"));
	purple_plugin_pref_frame_add(frame, ppref);

	tmp = g_strdup_printf(_("_Force 24-hour time format"));
	ppref = purple_plugin_pref_new_with_name_and_label(
			"/plugins/gtk/timestamp_format/force_24hr",
			tmp);
	purple_plugin_pref_frame_add(frame, ppref);
	g_free(tmp);

	ppref = purple_plugin_pref_new_with_label(_("Show dates in..."));
	purple_plugin_pref_frame_add(frame, ppref);

	ppref = purple_plugin_pref_new_with_name_and_label(
			"/plugins/gtk/timestamp_format/use_dates/conversation",
			_("Co_nversations:"));
        purple_plugin_pref_set_type(ppref, PURPLE_PLUGIN_PREF_CHOICE);
        purple_plugin_pref_add_choice(ppref, _("For delayed messages"), "automatic");
        purple_plugin_pref_add_choice(ppref, _("For delayed messages and in chats"), "chats");
        purple_plugin_pref_add_choice(ppref, _("Always"), "always");
	purple_plugin_pref_frame_add(frame, ppref);

	ppref = purple_plugin_pref_new_with_name_and_label(
			"/plugins/gtk/timestamp_format/use_dates/log",
			_("_Message Logs:"));
        purple_plugin_pref_set_type(ppref, PURPLE_PLUGIN_PREF_CHOICE);
        purple_plugin_pref_add_choice(ppref, _("For delayed messages"), "automatic");
        purple_plugin_pref_add_choice(ppref, _("For delayed messages and in chats"), "chats");
        purple_plugin_pref_add_choice(ppref, _("Always"), "always");
	purple_plugin_pref_frame_add(frame, ppref);

	return frame;
}

static char *timestamp_cb_common(PurpleConversation *conv,
                                 time_t t,
                                 gboolean show_date,
                                 gboolean force,
                                 const char *dates,
								 gboolean parens)
{
	g_return_val_if_fail(dates != NULL, NULL);

	if (show_date ||
	    !strcmp(dates, "always") ||
	    (conv != NULL && purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT && !strcmp(dates, "chats")))
	{
		struct tm *tm = localtime(&t);
		if (force)
			return g_strdup_printf("%s%s%s", parens ? "(" : "", purple_utf8_strftime("%Y-%m-%d %H:%M:%S", tm), parens ? ")" : "");
		else
			return g_strdup_printf("%s%s%s", parens ? "(" : "", purple_date_format_long(tm), parens ? ")" : "");
	}

	if (force)
	{
		struct tm *tm = localtime(&t);
		return g_strdup_printf("%s%s%s", parens ? "(" : "", purple_utf8_strftime("%H:%M:%S", tm), parens ? ")" : "");
	}

	return NULL;
}

static char *conversation_timestamp_cb(PurpleConversation *conv,
                                       time_t t, gboolean show_date, gpointer data)
{
	gboolean force = purple_prefs_get_bool(
				"/plugins/gtk/timestamp_format/force_24hr");
	const char *dates = purple_prefs_get_string(
				"/plugins/gtk/timestamp_format/use_dates/conversation");

	g_return_val_if_fail(conv != NULL, NULL);

	return timestamp_cb_common(conv, t, show_date, force, dates, TRUE);
}

static char *log_timestamp_cb(PurpleLog *log, time_t t, gboolean show_date, gpointer data)
{
	gboolean force = purple_prefs_get_bool(
				"/plugins/gtk/timestamp_format/force_24hr");
	const char *dates = purple_prefs_get_string(
				"/plugins/gtk/timestamp_format/use_dates/log");

	g_return_val_if_fail(log != NULL, NULL);

	return timestamp_cb_common(log->conv, t, show_date, force, dates, FALSE);
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
	purple_signal_connect(pidgin_conversations_get_handle(), "conversation-timestamp",
	                    plugin, PURPLE_CALLBACK(conversation_timestamp_cb), NULL);
	purple_signal_connect(purple_log_get_handle(), "log-timestamp",
	                    plugin, PURPLE_CALLBACK(log_timestamp_cb), NULL);
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	return TRUE;
}

static PurplePluginUiInfo prefs_info = {
        get_plugin_pref_frame,
	0,   /* page num (Reserved) */
	NULL,/* frame (Reserved) */

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,                           /**< type           */
	PIDGIN_PLUGIN_TYPE,                               /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,                          /**< priority       */

	"core-timestamp_format",                          /**< id             */
	N_("Message Timestamp Formats"),                  /**< name           */
	DISPLAY_VERSION,                                  /**< version        */
	                                                  /**  summary        */
	N_("Customizes the message timestamp formats."),
	                                                  /**  description    */
	N_("This plugin allows the user to customize "
	   "conversation and logging message timestamp "
	   "formats."),
	"Richard Laager <rlaager@pidgin.im>",             /**< author         */
	PURPLE_WEBSITE,                                   /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL,                                             /**< extra_info     */
	&prefs_info,                                      /**< prefs_info     */
	NULL,                                             /**< actions        */

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
	purple_prefs_add_none("/plugins/gtk");
	purple_prefs_add_none("/plugins/gtk/timestamp_format");

	purple_prefs_add_bool("/plugins/gtk/timestamp_format/force_24hr", TRUE);

	purple_prefs_add_none("/plugins/gtk/timestamp_format/use_dates");
	purple_prefs_add_string("/plugins/gtk/timestamp_format/use_dates/conversation", "automatic");
	purple_prefs_add_string("/plugins/gtk/timestamp_format/use_dates/log", "automatic");
}

PURPLE_INIT_PLUGIN(timestamp_format, init_plugin, info)
