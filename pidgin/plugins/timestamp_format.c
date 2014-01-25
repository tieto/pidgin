#include "internal.h"

#include "debug.h"
#include "log.h"
#include "plugin.h"
#include "util.h"
#include "version.h"

#include "gtkconv.h"
#include "gtkplugin.h"
#include "gtkimhtml.h"

#include <time.h>

static const char *format_12hour_hour(const struct tm *tm)
{
	static char hr[3];
	int hour = tm->tm_hour % 12;
	if (hour == 0)
		hour = 12;

	g_snprintf(hr, sizeof(hr), "%d", hour);
	return hr;
}

static PurplePluginPrefFrame *
get_plugin_pref_frame(PurplePlugin *plugin)
{
	PurplePluginPrefFrame *frame;
	PurplePluginPref *ppref;
	char *tmp;

	frame = purple_plugin_pref_frame_new();

	ppref = purple_plugin_pref_new_with_label(_("Timestamp Format Options"));
	purple_plugin_pref_frame_add(frame, ppref);

	tmp = g_strdup(_("_Force timestamp format:"));
	ppref = purple_plugin_pref_new_with_name_and_label(
			"/plugins/gtk/timestamp_format/force",
			tmp);
	purple_plugin_pref_set_type(ppref, PURPLE_PLUGIN_PREF_CHOICE);
	purple_plugin_pref_add_choice(ppref, _("Use system default"), "default");
	purple_plugin_pref_add_choice(ppref, _("12 hour time format"), "force12");
	purple_plugin_pref_add_choice(ppref, _("24 hour time format"), "force24");
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
                                 const char *force,
                                 const char *dates,
								 gboolean parens)
{
	struct tm *tm;

	g_return_val_if_fail(dates != NULL, NULL);

	tm = localtime(&t);

	if (show_date ||
	    !strcmp(dates, "always") ||
	    (conv != NULL && purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT && !strcmp(dates, "chats")))
	{
		if (g_str_equal(force, "force24"))
			return g_strdup_printf("%s%s%s", parens ? "(" : "", purple_utf8_strftime("%Y-%m-%d %H:%M:%S", tm), parens ? ")" : "");
		else if (g_str_equal(force, "force12")) {
			char *date = g_strdup_printf("%s", purple_utf8_strftime("%Y-%m-%d ", tm));
			char *remtime = g_strdup_printf("%s", purple_utf8_strftime(":%M:%S %p", tm));
			const char *hour = format_12hour_hour(tm);
			char *output;

			output = g_strdup_printf("%s%s%s%s%s",
			                         parens ? "(" : "", date,
									 hour, remtime, parens ? ")" : "");

			g_free(date);
			g_free(remtime);

			return output;
		} else
			return g_strdup_printf("%s%s%s", parens ? "(" : "", purple_date_format_long(tm), parens ? ")" : "");
	}

	if (g_str_equal(force, "force24"))
		return g_strdup_printf("%s%s%s", parens ? "(" : "", purple_utf8_strftime("%H:%M:%S", tm), parens ? ")" : "");
	else if (g_str_equal(force, "force12")) {
		const char *hour = format_12hour_hour(tm);
		char *remtime = g_strdup_printf("%s", purple_utf8_strftime(":%M:%S %p", tm));
		char *output = g_strdup_printf("%s%s%s%s", parens ? "(" : "", hour, remtime, parens ? ")" : "");

		g_free(remtime);

		return output;
	}

	return NULL;
}

static char *conversation_timestamp_cb(PurpleConversation *conv,
                                       time_t t, gboolean show_date, gpointer data)
{
	const char *force = purple_prefs_get_string(
				"/plugins/gtk/timestamp_format/force");
	const char *dates = purple_prefs_get_string(
				"/plugins/gtk/timestamp_format/use_dates/conversation");

	g_return_val_if_fail(conv != NULL, NULL);

	return timestamp_cb_common(conv, t, show_date, force, dates, TRUE);
}

static char *log_timestamp_cb(PurpleLog *log, time_t t, gboolean show_date, gpointer data)
{
	const char *force = purple_prefs_get_string(
				"/plugins/gtk/timestamp_format/force");
	const char *dates = purple_prefs_get_string(
				"/plugins/gtk/timestamp_format/use_dates/log");

	g_return_val_if_fail(log != NULL, NULL);

	return timestamp_cb_common(log->conv, t, show_date, force, dates, FALSE);
}

static void
menu_cb(GtkWidget *item, gpointer data)
{
	PurplePlugin *plugin = data;
	GtkWidget *frame = pidgin_plugin_get_config_frame(plugin), *dialog;
	if (!frame)
		return;

	dialog = gtk_dialog_new_with_buttons(PIDGIN_ALERT_TITLE, NULL,
			GTK_DIALOG_NO_SEPARATOR | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
			NULL);
	g_signal_connect_after(G_OBJECT(dialog), "response", G_CALLBACK(gtk_widget_destroy), dialog);
#if GTK_CHECK_VERSION(2,14,0)
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), frame);
#else
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), frame);
#endif
	gtk_window_set_role(GTK_WINDOW(dialog), "plugin_config");
	gtk_window_set_title(GTK_WINDOW(dialog), _(purple_plugin_get_name(plugin)));
	gtk_widget_show_all(dialog);
}

static gboolean
textview_emission_hook(GSignalInvocationHint *hint, guint n_params,
		const GValue *pvalues, gpointer data)
{
	GtkTextView *view = GTK_TEXT_VIEW(g_value_get_object(pvalues));
	GtkWidget *menu, *item;
	GtkTextBuffer *buffer;
	GtkTextIter cursor;
	int cx, cy, bx, by;

	if (!GTK_IS_IMHTML(view))
		return TRUE;

#if GTK_CHECK_VERSION(2,14,0)
	if (!gdk_window_get_pointer(gtk_widget_get_window(GTK_WIDGET(view)), &cx, &cy, NULL))
		return TRUE;
#else
	if (!gdk_window_get_pointer(GTK_WIDGET(view)->window, &cx, &cy, NULL))
		return TRUE;
#endif

	buffer = gtk_text_view_get_buffer(view);

	gtk_text_view_window_to_buffer_coords(view, GTK_TEXT_WINDOW_TEXT, cx, cy, &bx, &by);
	gtk_text_view_get_iter_at_location(view, &cursor, bx, by);
	if (!gtk_text_iter_has_tag(&cursor,
				gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(buffer), "comment")))
		return TRUE;

	menu = g_value_get_object(&pvalues[1]);

	item = gtk_menu_item_new_with_label(_("Timestamp Format Options"));
	gtk_widget_show_all(item);
	g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(menu_cb), data);
	gtk_menu_shell_insert(GTK_MENU_SHELL(menu), item, 0);

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_menu_shell_insert(GTK_MENU_SHELL(menu), item, 1);

	return TRUE;
}

static guint signal_id;
static gulong hook_id;

static gboolean
plugin_load(PurplePlugin *plugin)
{
	gpointer klass = NULL;

	purple_signal_connect(pidgin_conversations_get_handle(), "conversation-timestamp",
	                    plugin, PURPLE_CALLBACK(conversation_timestamp_cb), NULL);
	purple_signal_connect(purple_log_get_handle(), "log-timestamp",
	                    plugin, PURPLE_CALLBACK(log_timestamp_cb), NULL);

	klass = g_type_class_ref(GTK_TYPE_TEXT_VIEW);

	/* In 3.0.0, use purple_g_signal_connect_flags */
	g_signal_parse_name("populate_popup", GTK_TYPE_TEXT_VIEW, &signal_id, NULL, FALSE);
	hook_id = g_signal_add_emission_hook(signal_id, 0, textview_emission_hook,
			plugin, NULL);

	g_type_class_unref(klass);

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	g_signal_remove_emission_hook(signal_id, hook_id);
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

	if (!purple_prefs_exists("/plugins/gtk/timestamp_format/force") &&
	    purple_prefs_exists("/plugins/gtk/timestamp_format/force_24hr"))
	{
		if (purple_prefs_get_bool(
		   "/plugins/gtk/timestamp_format/force_24hr"))
			purple_prefs_add_string("/plugins/gtk/timestamp_format/force", "force24");
		else
			purple_prefs_add_string("/plugins/gtk/timestamp_format/force", "default");
	}
	else
		purple_prefs_add_string("/plugins/gtk/timestamp_format/force", "default");

	purple_prefs_add_none("/plugins/gtk/timestamp_format/use_dates");
	purple_prefs_add_string("/plugins/gtk/timestamp_format/use_dates/conversation", "automatic");
	purple_prefs_add_string("/plugins/gtk/timestamp_format/use_dates/log", "automatic");
}

PURPLE_INIT_PLUGIN(timestamp_format, init_plugin, info)
