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

/* TODO (in order of importance):
 * - unify the queue so we can have a global away without the dialog
 * - handle and update tooltips to show your current accounts/queued messages?
 * - show a count of queued messages in the unified queue
 * - dernyi's account status menu in the right click
 * - optional pop up notices when GNOME2's system-tray-applet supports it
 */

#include "internal.h"
#include "gtkgaim.h"

#include "core.h"
#include "debug.h"
#include "prefs.h"
#include "signals.h"
#include "sound.h"
#include "version.h"

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
#include "gtkdialogs.h"

/* globals */

GaimPlugin *handle = NULL;
static struct docklet_ui_ops *ui_ops = NULL;
static enum docklet_status status = offline;
gboolean online_account_supports_chat = FALSE;
#if 0 /* XXX CUI */
#ifdef _WIN32
__declspec(dllimport) GSList *unread_message_queue;
__declspec(dllimport) GSList *away_messages;
__declspec(dllimport) struct away_message *awaymessage;
__declspec(dllimport) GSList *message_queue;
#endif
#endif

/* private functions */

static void
docklet_toggle_mute(GtkWidget *toggle, void *data)
{
	gaim_prefs_set_bool("/gaim/gtk/sound/mute", GTK_CHECK_MENU_ITEM(toggle)->active);
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
#if 0 /* XXX NEW STATUS */
/* This is workaround for a bug in windows GTK+. Clicking outside of the
   parent menu (including on a submenu-item) close the whole menu before
   the "activate" event is thrown for the given submenu-item. Fixed by
   replacing "activate" by "button-release-event". */
static gboolean
docklet_menu_do_away_message(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	do_away_message(widget, user_data);
	return FALSE;
}

static gboolean
docklet_menu_create_away_mess(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	create_away_mess(widget, user_data);
	return FALSE;
}
#endif

/* This is a workaround for a bug in windows GTK+. Clicking outside of the
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
	GtkWidget *menuitem;

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
			gaim_new_item_from_stock(menu, _("New Message..."), GAIM_STOCK_IM, G_CALLBACK(gaim_gtkdialogs_im), NULL, 0, 0, NULL);
			menuitem = gaim_new_item_from_stock(menu, _("Join A Chat..."), GAIM_STOCK_CHAT, G_CALLBACK(gaim_gtk_blist_joinchat_show), NULL, 0, 0, NULL);
			gtk_widget_set_sensitive(menuitem, online_account_supports_chat);
			break;
	}

	switch (status) {
		case offline:
		case offline_connecting:
			break;
		case online:
		case online_connecting:
		case online_pending: {
#if 0 /* XXX NEW STATUS */
			GtkWidget *docklet_awaymenu;
			GSList *awy = NULL;
			struct away_message *a = NULL;

			docklet_awaymenu = gtk_menu_new();
			awy = away_messages;

			while (awy) {
				a = (struct away_message *)awy->data;

				entry = gtk_menu_item_new_with_label(a->name);
#ifdef _WIN32
				g_signal_connect(G_OBJECT(entry), "button-release-event", G_CALLBACK(docklet_menu_do_away_message), a);
#else
				g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(do_away_message), a);
#endif
				gtk_menu_shell_append(GTK_MENU_SHELL(docklet_awaymenu), entry);

				awy = g_slist_next(awy);
			}

			if (away_messages)
				gaim_separator(docklet_awaymenu);

			entry = gtk_menu_item_new_with_label(_("New..."));
#ifdef _WIN32
			g_signal_connect(G_OBJECT(entry), "button-release-event", G_CALLBACK(docklet_menu_create_away_mess), NULL);
#else
			g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(create_away_mess), NULL);
#endif
			gtk_menu_shell_append(GTK_MENU_SHELL(docklet_awaymenu), entry);

			entry = gtk_menu_item_new_with_label(_("Away"));
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(entry), docklet_awaymenu);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), entry);
#endif
			} break;
		case away:
		case away_pending:
#if 0 /* XXX NEW STATUS */
			entry = gtk_menu_item_new_with_label(_("Back"));
			g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(do_im_back), NULL);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), entry);
#endif
			break;
	}

	gaim_separator(menu);

	entry = gtk_check_menu_item_new_with_label(_("Mute Sounds"));
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(entry), gaim_prefs_get_bool("/gaim/gtk/sound/mute"));
	if (!strcmp(gaim_prefs_get_string("/gaim/gtk/sound/method"), "none"))
		gtk_widget_set_sensitive(GTK_WIDGET(entry), FALSE);
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

	gaim_new_item_from_stock(menu, _("Quit"), GTK_STOCK_QUIT, G_CALLBACK(gaim_core_quit), NULL, 0, 0, NULL);

#ifdef _WIN32
	g_signal_connect(menu, "leave-notify-event", G_CALLBACK(docklet_menu_leave), NULL);
#endif
	gtk_widget_show_all(menu);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
				   ui_ops->position_menu,
				   NULL, 0, gtk_get_current_event_time());
}

static gboolean
docklet_blink_icon()
{
	static gboolean blinked = FALSE;
	gboolean ret = FALSE; /* by default, don't keep blinking */

	blinked = !blinked;

	switch (status) {
		case online_pending:
		case away_pending:
			if (blinked) {
				if (ui_ops && ui_ops->blank_icon)
					ui_ops->blank_icon();
			} else {
				if (ui_ops && ui_ops->update_icon)
					ui_ops->update_icon(status);
			}
			ret = TRUE; /* keep blinking */
			break;
		case offline:
		case offline_connecting:
		case online:
		case online_connecting:
		case away:
			blinked = FALSE;
			break;
	}

	return ret;
}

static gboolean
docklet_update_status()
{
	GList *c;
	enum docklet_status oldstatus;

	oldstatus = status;

	if ((c = gaim_connections_get_all())) {
#if 0 /* XXX NEW STATUS */
		if (unread_message_queue) {
			status = online_pending;
		} else if (awaymessage) {
			if (message_queue) {
				status = away_pending;
			} else {
				status = away;
			}
		} else if (gaim_connections_get_connecting()) {
#else
		if (gaim_connections_get_connecting()) {
#endif
			status = online_connecting;
		} else {
			status = online;
		}
		/* Check if any online accounts support chats */
		while (c != NULL) {
			GaimConnection *gc = c->data;
			if (GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl)->chat_info != NULL) {
				online_account_supports_chat = TRUE;
				break;
			}
			c = c->next;
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
		if (ui_ops && ui_ops->update_icon)
			ui_ops->update_icon(status);

		/* and schedule the blinker function if messages are pending */
		if (status == online_pending || status == away_pending) {
			g_timeout_add(500, docklet_blink_icon, &handle);
		}
	}

	return FALSE; /* for when we're called by the glib idle handler */
}

#if 0 /* XXX CUI */
static void
docklet_flush_queue()
{
	if (unread_message_queue) {
		purge_away_queue(&unread_message_queue);
	}
}
#endif

static void
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
#if 0 /* XXX CUI */
			if (unread_message_queue) {
				docklet_flush_queue();
			} else {
#endif
				gaim_gtk_blist_docklet_toggle();
#if 0 /* XXX CUI */
			}
#endif
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
	if (ui_ops && ui_ops->update_icon)
		ui_ops->update_icon(status);
}

void
docklet_remove(gboolean visible)
{
	if (visible)
		gaim_gtk_blist_docklet_remove();

#if 0 /* XXX CUI */
	docklet_flush_queue();
#endif
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
gaim_away(GaimAccount *account, char *state, char *message, void *data)
{
	/* we only support global away. this is the way it is, ok? */
	docklet_update_status();
}

static gboolean
gaim_conv_im_recv(GaimAccount *account, char *sender, char *message, 
			 GaimConversation *conv, int flags, void *data)
{
	/* if message queuing while away is enabled, this event could be the first
	   message so we need to see if the status (and hence icon) needs changing.
	   do this when idle so that all message processing is completed, queuing
	   etc, before we run. */
	g_idle_add(docklet_update_status, &handle);

	return FALSE;
}

static void
gaim_new_conversation(GaimConversation *conv, void *data)
{
	/* queue a callback here so if the queue is being
	   flushed, we stop flashing. thanks javabsp. */
	g_idle_add(docklet_update_status, &handle);
}

/* We need this because the blist purge_away_queue cb won't be called before the
   plugin is unloaded, when quitting */
static void gaim_quit_cb() {
	gaim_debug(GAIM_DEBUG_INFO, "tray icon", "dealing with queued messages on exit\n");
#if 0 /* XXX CUI */
	docklet_flush_queue();
#endif
}


/* static void gaim_buddy_signon(GaimConnection *gc, char *who, void *data) {
}

static void gaim_buddy_signoff(GaimConnection *gc, char *who, void *data) {
}

static void gaim_buddy_away(GaimConnection *gc, char *who, void *data) {
}

static void gaim_buddy_back(GaimConnection *gc, char *who, void *data) {
} */

/* plugin glue */

#define DOCKLET_PLUGIN_ID "gtk-docklet"

static gboolean
plugin_load(GaimPlugin *plugin)
{
	void *conn_handle = gaim_connections_get_handle();
	void *conv_handle = gaim_conversations_get_handle();
	void *accounts_handle = gaim_accounts_get_handle();
	void *core_handle = gaim_get_core();

	gaim_debug(GAIM_DEBUG_INFO, "tray icon", "plugin loaded\n");

	handle = plugin;

	docklet_ui_init();
	if (ui_ops && ui_ops->create)
		ui_ops->create();

	gaim_signal_connect(conn_handle, "signed-on",
						plugin, GAIM_CALLBACK(gaim_signon), NULL);
	gaim_signal_connect(conn_handle, "signed-off",
						plugin, GAIM_CALLBACK(gaim_signoff), NULL);
	gaim_signal_connect(accounts_handle, "account-connecting",
						plugin, GAIM_CALLBACK(gaim_connecting), NULL);
	gaim_signal_connect(accounts_handle, "account-away",
						plugin, GAIM_CALLBACK(gaim_away), NULL);
	gaim_signal_connect(conv_handle, "received-im-msg",
						plugin, GAIM_CALLBACK(gaim_conv_im_recv), NULL);
	gaim_signal_connect(conv_handle, "conversation-created",
						plugin, GAIM_CALLBACK(gaim_new_conversation), NULL);

	gaim_signal_connect(core_handle, "quitting",
						plugin, GAIM_CALLBACK(gaim_quit_cb), NULL);
/*	gaim_signal_connect(plugin, event_buddy_signon, gaim_buddy_signon, NULL);
	gaim_signal_connect(plugin, event_buddy_signoff, gaim_buddy_signoff, NULL);
	gaim_signal_connect(plugin, event_buddy_away, gaim_buddy_away, NULL);
	gaim_signal_connect(plugin, event_buddy_back, gaim_buddy_back, NULL); */

	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	if (ui_ops && ui_ops->destroy)
		ui_ops->destroy();

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
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
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
	GAIM_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	&ui_info,                                         /**< ui_info        */
	NULL,                                             /**< extra_info     */
	NULL,
	NULL
};

static void
plugin_init(GaimPlugin *plugin)
{
	gaim_prefs_add_none("/plugins/gtk/docklet");
	gaim_prefs_add_bool("/plugins/gtk/docklet/queue_messages", FALSE);
}

GAIM_INIT_PLUGIN(docklet, plugin_init, info)
