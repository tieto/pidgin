/* 
 * System tray icon (aka docklet) plugin for Gaim
 * 
 * Copyright (C) 2002-3 Robert McQueen <robot101@debian.org>
 * Copyright (C) 2003 Herman Bloggs <hermanator12002@yahoo.com>
 * Inspired by a similar plugin by:
 *  John (J5) Palmieri <johnp@martianrock.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

/* todo (in order of importance):
    - unify the queue so we can have a global away without the dialog
    - handle and update tooltips to show your current accounts/queued messages?
    - show a count of queued messages in the unified queue
    - dernyi's account status menu in the right click
    - optional pop up notices when GNOME2's system-tray-applet supports it */

#include "internal.h"

#include "core.h"
#include "debug.h"
#include "prefs.h"
#include "sound.h"

#include "gtkaccount.h"
#include "gtkblist.h"
#include "gtkft.h"
#include "gtkplugin.h"
#include "gtkprefs.h"
#include "gtksound.h"
#include "gtkutils.h"
#include "stock.h"
#include "docklet.h"

#include "gaim.h"
#include "ui.h"

/* globals */

GaimPlugin *handle = NULL;
static struct docklet_ui_ops *ui_ops = NULL;
static enum docklet_status status = offline;
static enum docklet_status icon = offline;
#ifdef _WIN32
__declspec(dllimport) GSList *unread_message_queue;
__declspec(dllimport) GSList *away_messages;
__declspec(dllimport) struct away_message *awaymessage;
__declspec(dllimport) GSList *message_queue;
#endif

/* private functions */

static void
docklet_toggle_mute(GtkWidget *toggle, void *data)
{
	gaim_gtk_sound_set_mute(GTK_CHECK_MENU_ITEM(toggle)->active);
}

static void
docklet_set_bool(GtkWidget *widget, const char *key)
{
	gaim_prefs_set_bool(key, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)));
}

static void
docklet_auto_login()
{
	gaim_accounts_auto_login(GAIM_GTK_UI);
}

#ifdef _WIN32
/* This is a workaround for a bug in windows GTK+.. Clicking outside of the
   menu does not get rid of it, so instead we get rid of it as soon as the
   pointer leaves the menu. */
static gboolean
docklet_menu_leave(GtkWidget *menu, GdkEventCrossing *event, void *data)
{
	if(event->detail == GDK_NOTIFY_ANCESTOR) {
		gaim_debug(GAIM_DEBUG_INFO, "tray icon", "menu leave-notify-event\n");
		gtk_menu_popdown(GTK_MENU(menu));
	}
	return FALSE;
}
#endif

static void docklet_menu() {
	static GtkWidget *menu = NULL;
	GtkWidget *entry;

	if (menu) {
		gtk_widget_destroy(menu);
	}

	menu = gtk_menu_new();

	switch (status) {
		case offline:
		case offline_connecting:
			gaim_new_item_from_stock(menu, _("Auto-login"), GAIM_STOCK_SIGN_ON, G_CALLBACK(docklet_auto_login), NULL, 0, 0, NULL);
			break;
		default:
			gaim_new_item_from_stock(menu, _("New Message.."), GAIM_STOCK_IM, G_CALLBACK(show_im_dialog), NULL, 0, 0, NULL);
			gaim_new_item_from_stock(menu, _("Join A Chat..."), GAIM_STOCK_CHAT, G_CALLBACK(join_chat), NULL, 0, 0, NULL);
			break;
	}

	switch (status) {
		case offline:
		case offline_connecting:
			break;
		case online:
		case online_connecting:
		case online_pending: {
			GtkWidget *docklet_awaymenu;
			GSList *awy = NULL;
			struct away_message *a = NULL;

			docklet_awaymenu = gtk_menu_new();
			awy = away_messages;

			while (awy) {
				a = (struct away_message *)awy->data;

				entry = gtk_menu_item_new_with_label(a->name);
				g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(do_away_message), a);
				gtk_menu_shell_append(GTK_MENU_SHELL(docklet_awaymenu), entry);

				awy = g_slist_next(awy);
			}

			if (away_messages)
				gaim_separator(docklet_awaymenu);

			entry = gtk_menu_item_new_with_label(_("New..."));
			g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(create_away_mess), NULL);
			gtk_menu_shell_append(GTK_MENU_SHELL(docklet_awaymenu), entry);

			entry = gtk_menu_item_new_with_label(_("Away"));
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(entry), docklet_awaymenu);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), entry);
			} break;
		case away:
		case away_pending:
			entry = gtk_menu_item_new_with_label(_("Back"));
			g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(do_im_back), NULL);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), entry);
			break;
	}

	gaim_separator(menu);

	entry = gtk_check_menu_item_new_with_label(_("Mute Sounds"));
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(entry), gaim_gtk_sound_get_mute());
	g_signal_connect(G_OBJECT(entry), "toggled", G_CALLBACK(docklet_toggle_mute), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), entry);

	gaim_new_item_from_stock(menu, _("File Transfers"), GAIM_STOCK_FILE_TRANSFER, G_CALLBACK(gaim_show_xfer_dialog), NULL, 0, 0, NULL);
	gaim_new_item_from_stock(menu, _("Accounts"), GAIM_STOCK_ACCOUNTS, G_CALLBACK(gaim_gtk_accounts_window_show), NULL, 0, 0, NULL);
	gaim_new_item_from_stock(menu, _("Preferences"), GTK_STOCK_PREFERENCES, G_CALLBACK(gaim_gtk_prefs_show), NULL, 0, 0, NULL);

	gaim_separator(menu);

	switch (status) {
		case offline:
		case offline_connecting:
			break;
		default:
			gaim_new_item_from_stock(menu, _("Signoff"), GTK_STOCK_CLOSE, G_CALLBACK(gaim_connections_disconnect_all), NULL, 0, 0, 0);
			break;
	}

	gaim_new_item_from_stock(menu, _("Quit"), GTK_STOCK_QUIT, G_CALLBACK(gaim_core_quit), NULL, 0, 0, 0);

#ifdef _WIN32
	g_signal_connect(menu, "leave-notify-event", G_CALLBACK(docklet_menu_leave), NULL);
#endif
	gtk_widget_show_all(menu);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time());
}

static gboolean
docklet_blink_icon()
{
	if (status == online_pending) {
		if (status == icon) {
			/* last icon was the right one... let's change it */
			icon = online;
		} else {
			/* last icon was the wrong one, change it back */
			icon = online_pending;
		}
	} else if (status == away_pending) {
		if (status == icon) {
			/* last icon was the right one... let's change it */
			icon = away;
		} else {
			/* last icon was the wrong one, change it back */
			icon = away_pending;
		}
	} else {
		/* no messages, stop blinking */
		return FALSE;
	}

	if (ui_ops->update_icon)
		ui_ops->update_icon(icon);

	return TRUE; /* keep blinking */
}

static gboolean
docklet_update_status()
{
	enum docklet_status oldstatus;

	oldstatus = status;

	if (gaim_connections_get_all()) {
		if (unread_message_queue) {
			status = online_pending;
		} else if (awaymessage) {
			if (message_queue) {
				status = away_pending;
			} else {
				status = away;
			}
		} else if (gaim_connections_get_connecting()) {
			status = online_connecting;
		} else {
			status = online;
		}
	} else {
		if (gaim_connections_get_connecting()) {
			status = offline_connecting;
		} else {
			status = offline;
		}
	}

	/* update the icon if we changed status */
	if (status != oldstatus) {
		icon = status;
		if (ui_ops->update_icon)
			ui_ops->update_icon(icon);

		/* and schedule the blinker function if messages are pending */
		if (status == online_pending || status == away_pending) {
			g_timeout_add(500, docklet_blink_icon, NULL);
		}
	}

	return FALSE; /* for when we're called by the glib idle handler */
}

void
docklet_flush_queue()
{
	if (unread_message_queue) {
		purge_away_queue(&unread_message_queue);
	}
}

void
docklet_remove_callbacks()
{
	gaim_debug(GAIM_DEBUG_INFO, "tray icon", "removing callbacks");

	while (g_source_remove_by_user_data(&handle)) {
		gaim_debug(GAIM_DEBUG_INFO, NULL, ".");
	}

	gaim_debug(GAIM_DEBUG_INFO, NULL, "\n");
}

/* public code */

void
docklet_clicked(int button_type)
{
	switch (button_type) {
		case 1:
			if (unread_message_queue) {
				docklet_flush_queue();
				g_idle_add(docklet_update_status, &handle);
			} else {
				gaim_gtk_blist_docklet_toggle();
			}
			break;
		case 2:
			switch (status) {
				case offline:
				case offline_connecting:
					docklet_auto_login();
					break;
				default:
					break;
			}
			break;
		case 3:
			docklet_menu();
			break;
	}
}

void
docklet_embedded()
{
	gaim_gtk_blist_docklet_add();

	docklet_update_status();
	if (ui_ops->update_icon)
		ui_ops->update_icon(icon);
}

void
docklet_remove(gboolean visible)
{
	if (visible)
		gaim_gtk_blist_docklet_remove();

	docklet_flush_queue();
}

void
docklet_set_ui_ops(struct docklet_ui_ops *ops)
{
	ui_ops = ops;
}

/* callbacks */

static void
gaim_signon(GaimConnection *gc, void *data)
{
	docklet_update_status();
}

static void
gaim_signoff(GaimConnection *gc, void *data)
{
	/* do this when idle so that if the prpl was connecting
	   and was cancelled, we register that connecting_count
	   has returned to 0 */
	/* no longer necessary because Chip decided that us plugins
	   didn't need to know if an account was connecting or not
	   g_idle_add(docklet_update_status, &docklet); */
	docklet_update_status();
}

static void
gaim_connecting(GaimAccount *account, void *data)
{
	docklet_update_status();
}

static void
gaim_away(GaimConnection *gc, char *state, char *message, void *data)
{
	/* we only support global away. this is the way it is, ok? */
	docklet_update_status();
}

static void
gaim_im_recv(GaimConnection *gc, char **who, char **what, void *data)
{
	/* if message queuing while away is enabled, this event could be the first
	   message so we need to see if the status (and hence icon) needs changing.
	   do this when idle so that all message processing is completed, queuing
	   etc, before we run. */
	g_idle_add(docklet_update_status, &handle);
}

/* static void gaim_buddy_signon(GaimConnection *gc, char *who, void *data) {
}

static void gaim_buddy_signoff(GaimConnection *gc, char *who, void *data) {
}

static void gaim_buddy_away(GaimConnection *gc, char *who, void *data) {
}

static void gaim_buddy_back(GaimConnection *gc, char *who, void *data) {
}

static void gaim_new_conversation(char *who, void *data) {
} */

/* plugin glue */

#define DOCKLET_PLUGIN_ID "gtk-docklet"

static gboolean
plugin_load(GaimPlugin *plugin)
{
	gaim_debug(GAIM_DEBUG_INFO, "tray icon", "plugin loaded\n");

	handle = plugin;

	docklet_ui_init();
	if (ui_ops->create)
		ui_ops->create();

	gaim_signal_connect(plugin, event_signon, gaim_signon, NULL);
	gaim_signal_connect(plugin, event_signoff, gaim_signoff, NULL);
	gaim_signal_connect(plugin, event_connecting, gaim_connecting, NULL);
	gaim_signal_connect(plugin, event_away, gaim_away, NULL);
	gaim_signal_connect(plugin, event_im_recv, gaim_im_recv, NULL);
/*	gaim_signal_connect(plugin, event_buddy_signon, gaim_buddy_signon, NULL);
	gaim_signal_connect(plugin, event_buddy_signoff, gaim_buddy_signoff, NULL);
	gaim_signal_connect(plugin, event_buddy_away, gaim_buddy_away, NULL);
	gaim_signal_connect(plugin, event_buddy_back, gaim_buddy_back, NULL);
	gaim_signal_connect(plugin, event_new_conversation, gaim_new_conversation, NULL); */

	gaim_prefs_add_none("/plugins/gtk/docklet");
	gaim_prefs_add_bool("/plugins/gtk/docklet/queue_messages", FALSE);

	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	if (ui_ops->destroy)
		ui_ops->destroy();

	/* XXX: do this while gaim has no other way to toggle the global mute */
	gaim_gtk_sound_set_mute(FALSE);
	docklet_remove_callbacks();

	gaim_debug(GAIM_DEBUG_INFO, "tray icon", "plugin unloaded\n");

	return TRUE;
}

static GtkWidget *
plugin_config_frame(GaimPlugin *plugin)
{
	GtkWidget *frame;
	GtkWidget *vbox, *hbox;
	GtkWidget *toggle;
	static const char *qmpref = "/plugins/gtk/docklet/queue_messages";

	frame = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width(GTK_CONTAINER(frame), 12);

	vbox = gaim_gtk_make_frame(frame, _("Tray Icon Configuration"));
	hbox = gtk_hbox_new(FALSE, 18);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	toggle = gtk_check_button_new_with_mnemonic(_("_Hide new messages until tray icon is clicked"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), gaim_prefs_get_bool(qmpref));
	g_signal_connect(G_OBJECT(toggle), "clicked", G_CALLBACK(docklet_set_bool), (void *)qmpref);
	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);

	gtk_widget_show_all(frame);
	return frame;
}

static GaimGtkPluginUiInfo ui_info =
{
	plugin_config_frame
};

static GaimPluginInfo info =
{
	2,                                                /**< api_version    */
	GAIM_PLUGIN_STANDARD,                             /**< type           */
	GAIM_GTK_PLUGIN_TYPE,                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	DOCKLET_PLUGIN_ID,                                /**< id             */
	N_("System Tray Icon"),                           /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Displays an icon for Gaim in the system tray."),
	                                                  /**  description    */
	N_("Displays a system tray icon (in GNOME, KDE or Windows for example) "
	   "to show the current status of Gaim, allow fast access to commonly "
	   "used functions, and to toggle display of the buddy list or login "
	   "window. Also allows messages to be queued until the icon is "
	   "clicked, similar to ICQ."),
	"Robert McQueen <robot101@debian.org>",           /**< author         */
	WEBSITE,                                          /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	&ui_info,                                         /**< ui_info        */
	NULL                                              /**< extra_info     */
};

static void
plugin_init(GaimPlugin *plugin)
{
}

GAIM_INIT_PLUGIN(docklet, plugin_init, info)
