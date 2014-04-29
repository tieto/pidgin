/*
 * Screen Capture - a plugin that allows taking screenshots and sending them
 * to your buddies as inline images.
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

#include <gdk/gdkkeysyms.h>

#include "debug.h"
#include "version.h"

#include "gtkconv.h"
#include "gtkplugin.h"
#include "gtkwebviewtoolbar.h"
#include "pidginstock.h"

#define SCRNCAP_SHOOTING_TIMEOUT 500

static gboolean is_shooting = FALSE;
static guint shooting_timeout = 0;
static GtkWindow *crop_window;

/******************************************************************************
 * libpidgin helper functions
 ******************************************************************************/

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

/******************************************************************************
 * GdkPixbuf helper functions
 ******************************************************************************/

static GdkPixbuf *
scrncap_perform_screenshot(void)
{
	GdkWindow *root;
	gint orig_x, orig_y, width, height;

	root = gdk_get_default_root_window();
	gdk_window_get_origin(root, &orig_x, &orig_y);
	gdk_drawable_get_size(root, &width, &height);

	/* for gtk3 should be gdk_pixbuf_get_from_window */
	return gdk_pixbuf_get_from_drawable(
		NULL, root, NULL,
		orig_x, orig_y, 0, 0, width, height);
}

static void
scrncap_pixbuf_darken(GdkPixbuf *pixbuf)
{
	guchar *pixels;
	int i, y, width, height, row_width, n_channels, rowstride, pad;

	pixels = gdk_pixbuf_get_pixels(pixbuf);
	width = gdk_pixbuf_get_width(pixbuf);
	height = gdk_pixbuf_get_height(pixbuf);
	n_channels = gdk_pixbuf_get_n_channels(pixbuf);
	rowstride = gdk_pixbuf_get_rowstride(pixbuf);

	row_width = width * n_channels;
	pad = rowstride - row_width;
	g_return_if_fail(pad >= 0);

	for (y = 0; y < height; y++) {
		for (i = 0; i < row_width; i++, pixels++)
			*pixels /= 2;
		pixels += pad;
	}
}

/******************************************************************************
 * Crop window
 ******************************************************************************/

static void
scrncap_crop_window_close(GtkWindow *window, gpointer _unused)
{
	GdkPixbuf *screenshot;

	g_return_if_fail(crop_window == window);

	screenshot = g_object_get_data(G_OBJECT(crop_window), "screenshot");

	is_shooting = FALSE;
	crop_window = NULL;
	g_object_unref(screenshot);
}

static gboolean
scrncap_crop_window_keypress(GtkWidget *window, GdkEventKey *event,
	gpointer _unused)
{
	guint key = event->keyval;

	if (key == GDK_Escape) {
		gtk_widget_destroy(window);
		return TRUE;
	}
	if (key == GDK_Return) {
		/* TODO: process */
		return TRUE;
	}

	return FALSE;
}

static gboolean
scrncap_crop_window_focusout(GtkWidget *window, GdkEventFocus *event,
	gpointer _unused)
{
	gtk_widget_destroy(window);
	return FALSE;
}

static gboolean
scrncap_do_screenshot_cb(gpointer _webview)
{
	PidginWebView *webview = PIDGIN_WEBVIEW(_webview);
	GdkPixbuf *screenshot, *screenshot_d;
	int width, height;
	GtkWidget *image;

	shooting_timeout = 0;

	(void)webview;

	screenshot = scrncap_perform_screenshot();
	g_return_val_if_fail(screenshot != NULL, G_SOURCE_REMOVE);
	width = gdk_pixbuf_get_width(screenshot);
	height = gdk_pixbuf_get_height(screenshot);

	crop_window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
	gtk_window_set_decorated(crop_window, FALSE);
	gtk_window_set_policy(crop_window, FALSE, FALSE, TRUE);
	gtk_window_set_resizable(crop_window, FALSE);
	gtk_widget_set_size_request(GTK_WIDGET(crop_window), width, height);
	gtk_window_fullscreen(crop_window);
	gtk_window_set_keep_above(crop_window, TRUE);

	g_signal_connect(G_OBJECT(crop_window), "destroy",
		G_CALLBACK(scrncap_crop_window_close), NULL);
	g_signal_connect(G_OBJECT(crop_window), "key-press-event",
		G_CALLBACK(scrncap_crop_window_keypress), NULL);
	g_signal_connect(G_OBJECT(crop_window), "focus-out-event",
		G_CALLBACK(scrncap_crop_window_focusout), NULL);
	g_object_set_data(G_OBJECT(crop_window), "screenshot", screenshot);

	screenshot_d = gdk_pixbuf_copy(screenshot);
	scrncap_pixbuf_darken(screenshot_d);
	image = gtk_image_new_from_pixbuf(screenshot_d);
	g_object_unref(screenshot_d);
	gtk_container_add(GTK_CONTAINER(crop_window), image);

	gtk_widget_show_all(GTK_WIDGET(crop_window));

	return G_SOURCE_REMOVE;
}

static void
scrncap_do_screenshot(GtkAction *action, PidginWebView *webview)
{
	if (is_shooting)
		return;
	is_shooting = TRUE;

	shooting_timeout = purple_timeout_add(SCRNCAP_SHOOTING_TIMEOUT,
		scrncap_do_screenshot_cb, webview);
}

/******************************************************************************
 * PidginConversation setup
 ******************************************************************************/

static void
scrncap_conversation_init(PidginConversation *gtkconv)
{
	PidginWebView *webview;
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

	webview = PIDGIN_WEBVIEW(gtkconv->entry);
	toolbar = PIDGIN_WEBVIEWTOOLBAR(pidgin_webview_get_toolbar(webview));
	g_return_if_fail(toolbar != NULL);
	wide_view = GTK_TOOLBAR(pidgin_webviewtoolbar_get_wide_view(toolbar));
	g_return_if_fail(wide_view != NULL);
	lean_view = GTK_TOOLBAR(pidgin_webviewtoolbar_get_lean_view(toolbar));
	g_return_if_fail(lean_view != NULL);

	action = gtk_action_new("InsertScreenshot", _("_Screenshot"),
		_("Insert screenshot"), PIDGIN_STOCK_TOOLBAR_INSERT_SCREENSHOT);
	gtk_action_set_is_important(action, TRUE);
	g_signal_connect(G_OBJECT(action), "activate",
		G_CALLBACK(scrncap_do_screenshot), webview);

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

/******************************************************************************
 * Plugin setup
 ******************************************************************************/

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

	if (shooting_timeout > 0)
		purple_timeout_remove(shooting_timeout);
	if (crop_window != NULL)
		gtk_widget_destroy(GTK_WIDGET(crop_window));

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
