/*
 * Integration with Unity's messaging menu and launcher
 * Copyright (C) 2013 Ankit Vani <a@nevitus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#include "internal.h"
#include "version.h"
#include "account.h"
#include "savedstatuses.h"

#include "gtkplugin.h"
#include "gtkconv.h"
#include "gtkutils.h"

#include <unity.h>
#include <messaging-menu.h>

#define UNITY_PLUGIN_ID "gtk-unity-integration"

static MessagingMenuApp *mmapp = NULL;
static UnityLauncherEntry *launcher = NULL;
static guint n_sources = 0;
static gint launcher_count;
static gint messaging_menu_text;
static gboolean alert_chat_nick = TRUE;

enum {
	LAUNCHER_COUNT_DISABLE,
	LAUNCHER_COUNT_MESSAGES,
	LAUNCHER_COUNT_SOURCES,
};

enum {
	MESSAGING_MENU_COUNT,
	MESSAGING_MENU_TIME,
};

static int attach_signals(PurpleConversation *conv);
static void detach_signals(PurpleConversation *conv);

static void
update_launcher()
{
	guint count = 0;
	GList *convs = NULL;
	g_return_if_fail(launcher != NULL);
	if (launcher_count == LAUNCHER_COUNT_DISABLE)
		return;

	if (launcher_count == LAUNCHER_COUNT_MESSAGES) {
		for (convs = purple_get_conversations(); convs != NULL; convs = convs->next) {
			PurpleConversation *conv = convs->data;
			count += GPOINTER_TO_INT(purple_conversation_get_data(conv,
			                         "unity-message-count"));
		}
	} else {
		count = n_sources;
	}

	if (launcher != NULL) {
		if (count > 0)
			unity_launcher_entry_set_count_visible(launcher, TRUE);
		else
			unity_launcher_entry_set_count_visible(launcher, FALSE);
		unity_launcher_entry_set_count(launcher, count);
	}
}

static gchar *
conversation_id(PurpleConversation *conv)
{
	PurpleConversationType conv_type = purple_conversation_get_type(conv);
	PurpleAccount *account = purple_conversation_get_account(conv);
	char type[2] = "0";
	type[0] += conv_type;

	return g_strconcat(type, ":",
	                   purple_conversation_get_name(conv), ":",
	                   purple_account_get_username(account), ":",
	                   purple_account_get_protocol_id(account), NULL);
}

static void
messaging_menu_add_conversation(PurpleConversation *conv, gint count)
{
	gchar *id;
	g_return_if_fail(count > 0);
	id = conversation_id(conv);

	/* GBytesIcon may be useful for messaging menu source icons using buddy
	   icon data for IMs */
	if (!messaging_menu_app_has_source(mmapp, id))
		messaging_menu_app_append_source(mmapp, id, NULL,
		                                 purple_conversation_get_title(conv));

	if (messaging_menu_text == MESSAGING_MENU_TIME)
		messaging_menu_app_set_source_time(mmapp, id, g_get_real_time());
	else if (messaging_menu_text == MESSAGING_MENU_COUNT)
		messaging_menu_app_set_source_count(mmapp, id, count);
	messaging_menu_app_draw_attention(mmapp, id);

	g_free(id);
}

static void
messaging_menu_remove_conversation(PurpleConversation *conv)
{
	gchar *id = conversation_id(conv);
	if (messaging_menu_app_has_source(mmapp, id))
		messaging_menu_app_remove_source(mmapp, id);
	g_free(id);
}

static void
refill_messaging_menu()
{
	GList *convs;

	for (convs = purple_get_conversations(); convs != NULL; convs = convs->next) {
		PurpleConversation *conv = convs->data;
		messaging_menu_add_conversation(conv,
			GPOINTER_TO_INT(purple_conversation_get_data(conv, "unity-message-count")));
	}
}

static int
alert(PurpleConversation *conv)
{
	gint count;
	PidginWindow *purplewin = NULL;
	if (conv == NULL || PIDGIN_CONVERSATION(conv) == NULL)
		return 0;

	purplewin = PIDGIN_CONVERSATION(conv)->win;

	if (!pidgin_conv_window_has_focus(purplewin) ||
		!pidgin_conv_window_is_active_conversation(conv))
	{
		count = GPOINTER_TO_INT(purple_conversation_get_data(conv,
		                        "unity-message-count"));
		if (!count++)
			++n_sources;

		purple_conversation_set_data(conv, "unity-message-count",
		                             GINT_TO_POINTER(count));
		messaging_menu_add_conversation(conv, count);
		update_launcher();
	}

	return 0;
}

static void
unalert(PurpleConversation *conv)
{
	if (GPOINTER_TO_INT(purple_conversation_get_data(conv, "unity-message-count")) > 0)
		--n_sources;
	purple_conversation_set_data(conv, "unity-message-count",
	                             GINT_TO_POINTER(0));
	messaging_menu_remove_conversation(conv);
	update_launcher();
}

static int
unalert_cb(GtkWidget *widget, gpointer data, PurpleConversation *conv)
{
	unalert(conv);
	return 0;
}

static gboolean
message_displayed_cb(PurpleAccount *account, const char *who, char *message,
                     PurpleConversation *conv, PurpleMessageFlags flags)
{
	if ((purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT &&
	     alert_chat_nick && !(flags & PURPLE_MESSAGE_NICK)))
		return FALSE;

	if ((flags & PURPLE_MESSAGE_RECV) && !(flags & PURPLE_MESSAGE_DELAYED))
		alert(conv);

	return FALSE;
}

static void
im_sent_im(PurpleAccount *account, const char *receiver, const char *message)
{
	PurpleConversation *conv = NULL;
	conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, receiver,
	                                             account);
	unalert(conv);
}

static void
chat_sent_im(PurpleAccount *account, const char *message, int id)
{
	PurpleConversation *conv = NULL;
	conv = purple_find_chat(purple_account_get_connection(account), id);
	unalert(conv);
}

static void
conv_created(PurpleConversation *conv)
{
	purple_conversation_set_data(conv, "unity-message-count",
	                             GINT_TO_POINTER(0));
	attach_signals(conv);
}

static void
deleting_conv(PurpleConversation *conv)
{
	detach_signals(conv);
	unalert(conv);
}

static void
message_source_activated(MessagingMenuApp *app, const gchar *id,
                         gpointer user_data)
{
	gchar **sections = g_strsplit(id, ":", 0);
	PurpleConversation *conv = NULL;
	PurpleAccount *account;
	PidginWindow *purplewin = NULL;
	PurpleConversationType conv_type;

	char *type     = sections[0];
	char *cname    = sections[1];
	char *aname    = sections[2];
	char *protocol = sections[3];

	conv_type = type[0] - '0';
	account = purple_accounts_find(aname, protocol);
	conv = purple_find_conversation_with_account(conv_type, cname, account);

	if (conv) {
		unalert(conv);
		purplewin = PIDGIN_CONVERSATION(conv)->win;
		pidgin_conv_window_switch_gtkconv(purplewin, PIDGIN_CONVERSATION(conv));
		gdk_window_focus(gtk_widget_get_window(purplewin->window), time(NULL));
	}
	g_strfreev (sections);
}

static PurpleSavedStatus *
create_transient_status(PurpleStatusPrimitive primitive, PurpleStatusType *status_type)
{
	PurpleSavedStatus *saved_status = purple_savedstatus_new(NULL, primitive);

	if(status_type != NULL) {
		GList *tmp, *active_accts = purple_accounts_get_all_active();
		for (tmp = active_accts; tmp != NULL; tmp = tmp->next) {
			purple_savedstatus_set_substatus(saved_status,
				(PurpleAccount*) tmp->data, status_type, NULL);
		}
		g_list_free(active_accts);
	}

	return saved_status;
}

static void
status_changed_cb(PurpleSavedStatus *saved_status)
{
	MessagingMenuStatus status = MESSAGING_MENU_STATUS_AVAILABLE;

	switch (purple_savedstatus_get_type(saved_status)) {
	case PURPLE_STATUS_AVAILABLE:
	case PURPLE_STATUS_MOOD:
	case PURPLE_STATUS_TUNE:
	case PURPLE_STATUS_UNSET:
		status = MESSAGING_MENU_STATUS_AVAILABLE;
		break;

	case PURPLE_STATUS_AWAY:
	case PURPLE_STATUS_EXTENDED_AWAY:
		status = MESSAGING_MENU_STATUS_AWAY;
		break;

	case PURPLE_STATUS_INVISIBLE:
		status = MESSAGING_MENU_STATUS_INVISIBLE;
		break;

	case PURPLE_STATUS_MOBILE:
	case PURPLE_STATUS_OFFLINE:
		status = MESSAGING_MENU_STATUS_OFFLINE;
		break;

	case PURPLE_STATUS_UNAVAILABLE:
		status = MESSAGING_MENU_STATUS_BUSY;
		break;

	default:
		g_assert_not_reached();
	}
	messaging_menu_app_set_status(mmapp, status);
}

static void
messaging_menu_status_changed(MessagingMenuApp *mmapp,
                              MessagingMenuStatus mm_status, gpointer user_data)
{
	PurpleSavedStatus *saved_status;
	PurpleStatusPrimitive primitive = PURPLE_STATUS_UNSET;

	switch (mm_status) {
	case MESSAGING_MENU_STATUS_AVAILABLE:
		primitive = PURPLE_STATUS_AVAILABLE;
		break;

	case MESSAGING_MENU_STATUS_AWAY:
		primitive = PURPLE_STATUS_AWAY;
		break;

	case MESSAGING_MENU_STATUS_BUSY:
		primitive = PURPLE_STATUS_UNAVAILABLE;
		break;

	case MESSAGING_MENU_STATUS_INVISIBLE:
		primitive = PURPLE_STATUS_INVISIBLE;
		break;

	case MESSAGING_MENU_STATUS_OFFLINE:
		primitive = PURPLE_STATUS_OFFLINE;
		break;

	default:
		g_assert_not_reached();
	}

	saved_status = purple_savedstatus_find_transient_by_type_and_message(primitive, NULL);
	if (saved_status == NULL)
		saved_status = create_transient_status(primitive, NULL);
	purple_savedstatus_activate(saved_status);
}

static void
alert_config_cb(GtkWidget *widget, gpointer data)
{
	gboolean on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
	purple_prefs_set_bool("/plugins/gtk/unity/alert_chat_nick", on);
	alert_chat_nick = on;
}

static void
launcher_config_cb(GtkWidget *widget, gpointer data)
{
	gint option = GPOINTER_TO_INT(data);
	if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
		return;

	purple_prefs_set_int("/plugins/gtk/unity/launcher_count", option);
	launcher_count = option;
	if (option == LAUNCHER_COUNT_DISABLE)
		unity_launcher_entry_set_count_visible(launcher, FALSE);
	else
		update_launcher();
}

static void
messaging_menu_config_cb(GtkWidget *widget, gpointer data)
{
	gint option = GPOINTER_TO_INT(data);
	if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
		return;

	purple_prefs_set_int("/plugins/gtk/unity/messaging_menu_text", option);
	messaging_menu_text = option;
	refill_messaging_menu();
}

static int
attach_signals(PurpleConversation *conv)
{
	PidginConversation *gtkconv = NULL;
	guint id;

	gtkconv = PIDGIN_CONVERSATION(conv);
	if (!gtkconv)
		return 0;

	id = g_signal_connect(G_OBJECT(gtkconv->entry), "focus-in-event",
	                      G_CALLBACK(unalert_cb), conv);
	purple_conversation_set_data(conv, "unity-entry-signal", GUINT_TO_POINTER(id));

	id = g_signal_connect(G_OBJECT(gtkconv->imhtml), "focus-in-event",
	                      G_CALLBACK(unalert_cb), conv);
	purple_conversation_set_data(conv, "unity-imhtml-signal", GUINT_TO_POINTER(id));

	return 0;
}

static void
detach_signals(PurpleConversation *conv)
{
	PidginConversation *gtkconv = NULL;
	guint id;
	gtkconv = PIDGIN_CONVERSATION(conv);
	if (!gtkconv)
		return;

	id = GPOINTER_TO_INT(purple_conversation_get_data(conv, "unity-imhtml-signal"));
	g_signal_handler_disconnect(gtkconv->imhtml, id);

	id = GPOINTER_TO_INT(purple_conversation_get_data(conv, "unity-entry-signal"));
	g_signal_handler_disconnect(gtkconv->entry, id);

	purple_conversation_set_data(conv, "unity-message-count",
	                             GINT_TO_POINTER(0));
}

static GtkWidget *
get_config_frame(PurplePlugin *plugin)
{
	GtkWidget *ret = NULL, *frame = NULL;
	GtkWidget *vbox = NULL, *toggle = NULL;

	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width(GTK_CONTAINER (ret), 12);

	/* Alerts */

	frame = pidgin_make_frame(ret, _("Chatroom alerts"));
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	toggle = gtk_check_button_new_with_mnemonic(_("Chatroom message alerts _only where someone says your username"));
	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle),
	                             purple_prefs_get_bool("/plugins/gtk/unity/alert_chat_nick"));
	g_signal_connect(G_OBJECT(toggle), "toggled",
	                 G_CALLBACK(alert_config_cb), NULL);

	/* Launcher integration */

	frame = pidgin_make_frame(ret, _("Launcher Icon"));
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	toggle = gtk_radio_button_new_with_mnemonic(NULL, _("_Disable launcher integration"));
	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle),
		purple_prefs_get_int("/plugins/gtk/unity/launcher_count") == LAUNCHER_COUNT_DISABLE);
	g_signal_connect(G_OBJECT(toggle), "toggled",
	                 G_CALLBACK(launcher_config_cb), GUINT_TO_POINTER(LAUNCHER_COUNT_DISABLE));

	toggle = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(toggle),
	                                                        _("Show number of unread _messages on launcher icon"));
	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle),
		purple_prefs_get_int("/plugins/gtk/unity/launcher_count") == LAUNCHER_COUNT_MESSAGES);
	g_signal_connect(G_OBJECT(toggle), "toggled",
	                 G_CALLBACK(launcher_config_cb), GUINT_TO_POINTER(LAUNCHER_COUNT_MESSAGES));

	toggle = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(toggle),
	                                                        _("Show number of unread co_nversations on launcher icon"));
	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle),
		purple_prefs_get_int("/plugins/gtk/unity/launcher_count") == LAUNCHER_COUNT_SOURCES);
	g_signal_connect(G_OBJECT(toggle), "toggled",
	                 G_CALLBACK(launcher_config_cb), GUINT_TO_POINTER(LAUNCHER_COUNT_SOURCES));

	/* Messaging menu integration */

	frame = pidgin_make_frame(ret, _("Messaging Menu"));
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	toggle = gtk_radio_button_new_with_mnemonic(NULL,
	                                                        _("Show number of _unread messages for conversations in messaging menu"));
	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle),
		purple_prefs_get_int("/plugins/gtk/unity/messaging_menu_text") == MESSAGING_MENU_COUNT);
	g_signal_connect(G_OBJECT(toggle), "toggled",
	                 G_CALLBACK(messaging_menu_config_cb), GUINT_TO_POINTER(MESSAGING_MENU_COUNT));

	toggle = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(toggle),
	                                                        _("Show _elapsed time for unread conversations in messaging menu"));
	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle),
		purple_prefs_get_int("/plugins/gtk/unity/messaging_menu_text") == MESSAGING_MENU_TIME);
	g_signal_connect(G_OBJECT(toggle), "toggled",
	                 G_CALLBACK(messaging_menu_config_cb), GUINT_TO_POINTER(MESSAGING_MENU_TIME));

	gtk_widget_show_all(ret);
	return ret;
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
	GList *convs = purple_get_conversations();
	PurpleSavedStatus *saved_status;
	void *conv_handle = purple_conversations_get_handle();
	void *gtk_conv_handle = pidgin_conversations_get_handle();
	void *savedstat_handle = purple_savedstatuses_get_handle();

	alert_chat_nick = purple_prefs_get_bool("/plugins/gtk/unity/alert_chat_nick");

	mmapp = messaging_menu_app_new("pidgin.desktop");
	g_object_ref(mmapp);
	messaging_menu_app_register(mmapp);
	messaging_menu_text = purple_prefs_get_int("/plugins/gtk/unity/messaging_menu_text");

	g_signal_connect(mmapp, "activate-source",
	                 G_CALLBACK(message_source_activated), NULL);
	g_signal_connect(mmapp, "status-changed",
	                 G_CALLBACK(messaging_menu_status_changed), NULL);

	saved_status = purple_savedstatus_get_current();
	status_changed_cb(saved_status);

	purple_signal_connect(savedstat_handle, "savedstatus-changed", plugin,
	                    PURPLE_CALLBACK(status_changed_cb), NULL);

	launcher = unity_launcher_entry_get_for_desktop_id("pidgin.desktop");
	g_object_ref(launcher);
	launcher_count = purple_prefs_get_int("/plugins/gtk/unity/launcher_count");

	purple_signal_connect(gtk_conv_handle, "displayed-im-msg", plugin,
	                    PURPLE_CALLBACK(message_displayed_cb), NULL);
	purple_signal_connect(gtk_conv_handle, "displayed-chat-msg", plugin,
	                    PURPLE_CALLBACK(message_displayed_cb), NULL);
	purple_signal_connect(conv_handle, "sent-im-msg", plugin,
	                    PURPLE_CALLBACK(im_sent_im), NULL);
	purple_signal_connect(conv_handle, "sent-chat-msg", plugin,
	                    PURPLE_CALLBACK(chat_sent_im), NULL);
	purple_signal_connect(conv_handle, "conversation-created", plugin,
	                    PURPLE_CALLBACK(conv_created), NULL);
	purple_signal_connect(conv_handle, "deleting-conversation", plugin,
	                    PURPLE_CALLBACK(deleting_conv), NULL);

	while (convs) {
		PurpleConversation *conv = (PurpleConversation *)convs->data;
		attach_signals(conv);
		convs = convs->next;
	}

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	GList *convs = purple_get_conversations();
	while (convs) {
		PurpleConversation *conv = (PurpleConversation *)convs->data;
		unalert(conv);
		detach_signals(conv);
		convs = convs->next;
	}

	unity_launcher_entry_set_count_visible(launcher, FALSE);
	messaging_menu_app_unregister(mmapp);

	g_object_unref(launcher);
	g_object_unref(mmapp);
	return TRUE;
}

static PidginPluginUiInfo ui_info =
{
	get_config_frame,
	0, /* page_num (Reserved) */

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

	UNITY_PLUGIN_ID,                                  /**< id             */
	N_("Unity Integration"),                          /**< name           */
	DISPLAY_VERSION,                                  /**< version        */
	                                                  /**  summary        */
	N_("Provides integration with Unity."),
	                                                  /**  description    */
	N_("Provides integration with Unity's messaging "
	   "menu and launcher."),
	                                                  /**< author         */
	"Ankit Vani <a@nevitus.org>",
	PURPLE_WEBSITE,                                   /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	&ui_info,                                         /**< ui_info        */
	NULL,                                             /**< extra_info     */
	NULL,
	NULL,

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
	purple_prefs_add_none("/plugins/gtk/unity");
	purple_prefs_add_int("/plugins/gtk/unity/launcher_count", LAUNCHER_COUNT_SOURCES);
	purple_prefs_add_int("/plugins/gtk/unity/messaging_menu_text", MESSAGING_MENU_COUNT);
	purple_prefs_add_bool("/plugins/gtk/unity/alert_chat_nick", TRUE);
}

PURPLE_INIT_PLUGIN(unity, init_plugin, info)
