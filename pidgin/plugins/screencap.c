/*
 * Screenshots as outgoing images plugin.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

/* TODO: disable, when prpl doesn't support inline images */
/* TODO: add "Insert screenshot" to the Conversation window menu */

#include "internal.h"

#include "version.h"

#include "gtkconv.h"
#include "gtkplugin.h"
#include "gtkwebviewtoolbar.h"
#include "pidginstock.h"

static inline void
scrncap_conv_set_data(PidginConversation *gtkconv, const gchar *key,
	gpointer value)
{
	g_return_if_fail(gtkconv != NULL);

	g_object_set_data(G_OBJECT(gtkconv->tab_cont), key, value);
}

static inline gpointer
scrncap_conv_get_data(PidginConversation *gtkconv, const gchar *key)
{
	g_return_val_if_fail(gtkconv != NULL, NULL);

	return g_object_get_data(G_OBJECT(gtkconv->tab_cont), key);
}

static void
scrncap_conversation_init(PidginConversation *gtkconv)
{
	PidginWebViewToolbar *toolbar;
	GtkAction *action;
	GtkToolItem *scrncap_btn_wide;
	GtkWidget *scrncap_btn_lean;
	gint pos = -1, i;
	GtkToolbar *wide_view, *lean_view;
	GtkMenu *wide_menu = NULL;
	GList *wide_children, *it;

	if (scrncap_conv_get_data(gtkconv, "scrncap-btn-wide") != NULL)
		return;

	toolbar = PIDGIN_WEBVIEWTOOLBAR(pidgin_webview_get_toolbar(
		PIDGIN_WEBVIEW(gtkconv->entry)));
	g_return_if_fail(toolbar != NULL);
	wide_view = GTK_TOOLBAR(pidgin_webviewtoolbar_get_wide_view(toolbar));
	g_return_if_fail(wide_view != NULL);
	lean_view = GTK_TOOLBAR(pidgin_webviewtoolbar_get_lean_view(toolbar));
	g_return_if_fail(lean_view != NULL);

	action = gtk_action_new("InsertScreenshot", _("_Screenshot"),
		_("Insert screenshot"), PIDGIN_STOCK_TOOLBAR_INSERT_SCREENSHOT);
	gtk_action_set_is_important(action, TRUE);
	/*g_signal_connect(G_OBJECT(action), "activate", actions[i].cb, toolbar);*/

	scrncap_btn_wide = GTK_TOOL_ITEM(gtk_action_create_tool_item(action));
	scrncap_conv_set_data(gtkconv, "scrncap-btn-wide", scrncap_btn_wide);
	for (i = 0; i < gtk_toolbar_get_n_items(wide_view); i++) {
		GtkToolItem *ref_item = gtk_toolbar_get_nth_item(wide_view, i);
		const gchar *action_name;

		action_name = g_object_get_data(G_OBJECT(ref_item),
			"action-name");
		if (g_strcmp0(action_name, "InsertImage") == 0) {
			pos = i + 1;
			break;
		}
	}
	gtk_toolbar_insert(wide_view, scrncap_btn_wide, pos);
	gtk_widget_show(GTK_WIDGET(scrncap_btn_wide));

	for (i = 0; i < gtk_toolbar_get_n_items(lean_view); i++) {
		GtkToolItem *ref_item = gtk_toolbar_get_nth_item(lean_view, i);
		const gchar *action_name;

		action_name = g_object_get_data(G_OBJECT(ref_item), "action-name");
		if (g_strcmp0(action_name, "insert") == 0) {
			wide_menu = g_object_get_data(G_OBJECT(ref_item), "menu");
			break;
		}
	}
	g_return_if_fail(wide_menu);

	pos = -1;
	wide_children = gtk_container_get_children(GTK_CONTAINER(wide_menu));
	for (it = wide_children, i = 0; it; it = g_list_next(it), i++) {
		GtkWidget *child = it->data;
		const gchar *action_name;

		action_name = g_object_get_data(G_OBJECT(child), "action-name");
		if (g_strcmp0(action_name, "InsertImage") == 0) {
			pos = i + 1;
			break;
		}
	}
	g_list_free(wide_children);
	if (pos < 0) {
		g_warn_if_fail(pos >= 0);
		pos = 0;
	}

	scrncap_btn_lean = gtk_action_create_menu_item(action);
	scrncap_conv_set_data(gtkconv, "scrncap-btn-lean", scrncap_btn_lean);
	gtk_menu_shell_insert(GTK_MENU_SHELL(wide_menu),
		GTK_WIDGET(scrncap_btn_lean), pos);
	gtk_widget_show(GTK_WIDGET(scrncap_btn_lean));
}

static void
scrncap_conversation_uninit(PidginConversation *gtkconv)
{
	GtkWidget *scrncap_btn_wide, *scrncap_btn_lean;

	scrncap_btn_wide = scrncap_conv_get_data(gtkconv, "scrncap-btn-wide");
	if (scrncap_btn_wide == NULL)
		return;

	scrncap_btn_lean = scrncap_conv_get_data(gtkconv, "scrncap-btn-lean");

	gtk_widget_destroy(scrncap_btn_wide);
	if (scrncap_btn_lean)
		gtk_widget_destroy(scrncap_btn_lean);

	scrncap_conv_set_data(gtkconv, "scrncap-btn-wide", NULL);
	scrncap_conv_set_data(gtkconv, "scrncap-btn-lean", NULL);
}

static gboolean
scrncap_plugin_load(PurplePlugin *plugin)
{
	GList *it;

	purple_signal_connect(pidgin_conversations_get_handle(),
		"conversation-displayed", plugin,
		PURPLE_CALLBACK(scrncap_conversation_init), NULL);

	it = purple_conversations_get_all();
	for (; it; it = g_list_next(it)) {
		PurpleConversation *conv = it->data;

		if (!PIDGIN_IS_PIDGIN_CONVERSATION(conv))
			continue;
		scrncap_conversation_init(PIDGIN_CONVERSATION(conv));
	}

	return TRUE;
}

static gboolean
scrncap_plugin_unload(PurplePlugin *plugin)
{
	GList *it;

	it = purple_conversations_get_all();
	for (; it; it = g_list_next(it)) {
		PurpleConversation *conv = it->data;

		if (!PIDGIN_IS_PIDGIN_CONVERSATION(conv))
			continue;
		scrncap_conversation_uninit(PIDGIN_CONVERSATION(conv));
	}

	return TRUE;
}

static PidginPluginUiInfo scrncap_ui_info =
{
	NULL, /* config */

	/* padding */
	NULL, NULL, NULL, NULL
};

static PurplePluginInfo scrncap_info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,
	PIDGIN_PLUGIN_TYPE,
	0,
	NULL,
	PURPLE_PRIORITY_DEFAULT,
	"gtk-screencap",
	N_("Screen Capture"),
	DISPLAY_VERSION,
	N_("Send screenshots to your buddies."),
	N_("Adds an option to send a screenshot as an inline image. "
		"It works only with protocols that supports inline images."),
	"Tomasz Wasilczyk <twasilczyk@pidgin.im>",
	PURPLE_WEBSITE,
	scrncap_plugin_load,
	scrncap_plugin_unload,
	NULL,
	&scrncap_ui_info,
	NULL,
	NULL,
	NULL,

	/* padding */
	NULL, NULL, NULL, NULL
};

static void
scrncap_init_plugin(PurplePlugin *plugin)
{
#if 0
	purple_prefs_add_none("/plugins");
	purple_prefs_add_none("/plugins/gtk");
	purple_prefs_add_none("/plugins/gtk/screencap");
#endif
}

PURPLE_INIT_PLUGIN(screencap, scrncap_init_plugin, scrncap_info)
