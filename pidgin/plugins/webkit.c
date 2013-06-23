/*
 * WebKit - Open the inspector on any WebKit views.
 * Copyright (C) 2011 Elliott Sales de Andrade <qulogic@pidgin.im>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301, USA.
 */

#include "internal.h"

#include "version.h"

#include "pidgin.h"

#include "gtkconv.h"
#include "gtkplugin.h"
#include "gtkwebview.h"

static WebKitWebView *
create_gtk_window_around_it(WebKitWebInspector *inspector,
                            WebKitWebView      *webview,
                            PidginConversation *gtkconv)
{
	GtkWidget *win;
	GtkWidget *view;
	char *title;

	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	title = g_strdup_printf(_("%s - Inspector"),
	                        gtk_label_get_text(GTK_LABEL(gtkconv->tab_label)));
	gtk_window_set_title(GTK_WINDOW(win), title);
	g_free(title);
	gtk_window_set_default_size(GTK_WINDOW(win), 600, 400);
	g_signal_connect_swapped(G_OBJECT(gtkconv->tab_cont), "destroy", G_CALLBACK(gtk_widget_destroy), win);

	view = webkit_web_view_new();
	gtk_container_add(GTK_CONTAINER(win), view);
	g_object_set_data(G_OBJECT(webview), "inspector-window", win);

	return WEBKIT_WEB_VIEW(view);
}

static gboolean
show_inspector_window(WebKitWebInspector *inspector,
                      GtkWidget          *webview)
{
	GtkWidget *win;

	win = g_object_get_data(G_OBJECT(webview), "inspector-window");

	gtk_widget_show_all(win);

	return TRUE;
}

static void
setup_inspector(PidginConversation *gtkconv)
{
	GtkWidget *webview = gtkconv->webview;
	WebKitWebSettings *settings;
	WebKitWebInspector *inspector;

	settings = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(webview));
	inspector = webkit_web_view_get_inspector(WEBKIT_WEB_VIEW(webview));

	g_object_set(G_OBJECT(settings), "enable-developer-extras", TRUE, NULL);

	g_signal_connect(G_OBJECT(inspector), "inspect-web-view",
	                 G_CALLBACK(create_gtk_window_around_it), gtkconv);
	g_signal_connect(G_OBJECT(inspector), "show-window",
	                 G_CALLBACK(show_inspector_window), webview);
}

static void
remove_inspector(PidginConversation *gtkconv)
{
	GtkWidget *webview = gtkconv->webview;
	GtkWidget *win;
	WebKitWebSettings *settings;

	win = g_object_get_data(G_OBJECT(webview), "inspector-window");
	gtk_widget_destroy(win);
	g_object_set_data(G_OBJECT(webview), "inspector-window", NULL);

	settings = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(webview));

	g_object_set(G_OBJECT(settings), "enable-developer-extras", FALSE, NULL);
}

static void
conversation_displayed_cb(PidginConversation *gtkconv)
{
	GtkWidget *inspect = NULL;

	inspect = g_object_get_data(G_OBJECT(gtkconv->webview),
	                            "inspector-window");
	if (inspect == NULL) {
		setup_inspector(gtkconv);
	}
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
	GList *convs = purple_conversations_get();
	void *gtk_conv_handle = pidgin_conversations_get_handle();

	purple_signal_connect(gtk_conv_handle, "conversation-displayed", plugin,
	                      PURPLE_CALLBACK(conversation_displayed_cb), NULL);

	while (convs) {
		PurpleConversation *conv = (PurpleConversation *)convs->data;

		/* Setup WebKit Inspector */
		if (PIDGIN_IS_PIDGIN_CONVERSATION(conv)) {
			setup_inspector(PIDGIN_CONVERSATION(conv));
		}

		convs = convs->next;
	}

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	GList *convs = purple_conversations_get();

	while (convs) {
		PurpleConversation *conv = (PurpleConversation *)convs->data;

		/* Remove WebKit Inspector */
		if (PIDGIN_IS_PIDGIN_CONVERSATION(conv)) {
			remove_inspector(PIDGIN_CONVERSATION(conv));
		}

		convs = convs->next;
	}

	return TRUE;
}

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,                           /**< major version */
	PURPLE_MINOR_VERSION,                           /**< minor version */
	PURPLE_PLUGIN_STANDARD,                         /**< type */
	PIDGIN_PLUGIN_TYPE,                             /**< ui_requirement */
	0,                                              /**< flags */
	NULL,                                           /**< dependencies */
	PURPLE_PRIORITY_DEFAULT,                        /**< priority */

	"gtkwebkit-inspect",                            /**< id */
	N_("WebKit Development"),                       /**< name */
	DISPLAY_VERSION,                                /**< version */
	N_("Enables WebKit Inspector."),                /**< summary */
	N_("Enables WebKit's built-in inspector in a "
	   "conversation window. This may be viewed "
	   "by right-clicking a WebKit widget and "
	   "selecting 'Inspect Element'."),             /**< description */
	"Elliott Sales de Andrade <qulogic@pidgin.im>", /**< author */
	PURPLE_WEBSITE,                                 /**< homepage */
	plugin_load,                                    /**< load */
	plugin_unload,                                  /**< unload */
	NULL,                                           /**< destroy */
	NULL,                                           /**< ui_info */
	NULL,                                           /**< extra_info */
	NULL,                                           /**< prefs_info */
	NULL,                                           /**< actions */

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
}

PURPLE_INIT_PLUGIN(webkit-devel, init_plugin, info)

