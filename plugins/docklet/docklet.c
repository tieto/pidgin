/* System tray icon (aka docklet) plugin for Gaim
 * Copyright (C) 2002 Robert McQueen <robot101@debian.org>
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

/* includes */
#include <gtk/gtk.h>
#include "gtkplugin.h"
#include "gaim.h"
#include "sound.h"
#include "eggtrayicon.h"
#include "gtkblist.h"

#define DOCKLET_PLUGIN_ID "gtk-docklet"

/* types */
enum docklet_status {
	offline,
	offline_connecting,
	online,
	online_connecting,
	online_pending,
	away,
	away_pending
};

/* functions */
static gboolean docklet_create();
static gboolean docklet_update_status();
static gboolean plugin_unload(GaimPlugin *plugin);

/* globals */
static EggTrayIcon *docklet = NULL;
static GtkWidget *image = NULL;
static enum docklet_status status;
static enum docklet_status icon;

static void docklet_toggle_mute(GtkWidget *toggle, void *data) {
	gaim_sound_set_mute(GTK_CHECK_MENU_ITEM(toggle)->active);
}

static void docklet_toggle_queue(GtkWidget *widget, void *data) {
	away_options ^= OPT_AWAY_QUEUE_UNREAD;
	save_prefs();
}

/* static void docklet_toggle_blist_show(GtkWidget *widget, void *data) {
	blist_options ^= OPT_BLIST_APP_BUDDY_SHOW;
	save_prefs();
} */

static void docklet_flush_queue() {
	if (unread_message_queue) {
		purge_away_queue(&unread_message_queue);
	}
}

static void docklet_menu(GdkEventButton *event) {
	static GtkWidget *menu = NULL;
	GtkWidget *entry;

	if (menu) {
		gtk_widget_destroy(menu);
	}

	menu = gtk_menu_new();

	switch (status) {
		case offline:
		case offline_connecting:
			gaim_new_item_from_stock(menu, _("Auto-login"), GAIM_STOCK_SIGN_ON, G_CALLBACK(auto_login), NULL, 0, 0, NULL);
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
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(entry), gaim_sound_get_mute());
	g_signal_connect(G_OBJECT(entry), "toggled", G_CALLBACK(docklet_toggle_mute), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), entry);

	gaim_new_item_from_stock(menu, _("File Transfers..."), GAIM_STOCK_FILE_TRANSFER, G_CALLBACK(gaim_show_xfer_dialog), NULL, 0, 0, NULL);
	gaim_new_item_from_stock(menu, _("Accounts..."), GAIM_STOCK_ACCOUNTS, G_CALLBACK(account_editor), NULL, 0, 0, NULL);
	gaim_new_item_from_stock(menu, _("Preferences..."), GTK_STOCK_PREFERENCES, G_CALLBACK(gaim_gtk_prefs_show), NULL, 0, 0, NULL);

	gaim_separator(menu);

	switch (status) {
		case offline:
		case offline_connecting:
			break;
		default:
			gaim_new_item_from_stock(menu, _("Signoff"), GTK_STOCK_CLOSE, G_CALLBACK(signoff_all), NULL, 0, 0, 0);
			break;
	}

	gaim_new_item_from_stock(menu, _("Quit"), GTK_STOCK_QUIT, G_CALLBACK(do_quit), NULL, 0, 0, 0);

	gtk_widget_show_all(menu);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);
}

static void docklet_clicked(GtkWidget *button, GdkEventButton *event, void *data) {
	if (event->type != GDK_BUTTON_PRESS)
		return;

	switch (event->button) {
		case 1:
			if (unread_message_queue) {
				docklet_flush_queue();
				docklet_update_status();
			} else {
				gaim_gtk_blist_docklet_toggle();
			}
			break;
		case 2:
			break;
		case 3:
			docklet_menu(event);
			break;
	}
}

static void docklet_update_icon() {
	const gchar *icon_name = NULL;

	switch (icon) {
		case offline:
			icon_name = GAIM_STOCK_ICON_OFFLINE;
			break;
		case offline_connecting:
		case online_connecting:
			icon_name = GAIM_STOCK_ICON_CONNECT;
			break;
		case online:
			icon_name = GAIM_STOCK_ICON_ONLINE;
			break;
		case online_pending:
			icon_name = GAIM_STOCK_ICON_ONLINE_MSG;
			break;
		case away:
			icon_name = GAIM_STOCK_ICON_AWAY;
			break;
		case away_pending:
			icon_name = GAIM_STOCK_ICON_AWAY_MSG;
			break;
	}

	gtk_image_set_from_stock(GTK_IMAGE(image), icon_name, GTK_ICON_SIZE_LARGE_TOOLBAR);
}

static gboolean docklet_blink_icon() {
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

	docklet_update_icon();

	return TRUE; /* keep blinking */
}

static gboolean docklet_update_status() {
	enum docklet_status oldstatus;

	oldstatus = status;

	if (connections) {
		if (unread_message_queue) {
			status = online_pending;
		} else if (awaymessage) {
			if (message_queue) {
				status = away_pending;
			} else {
				status = away;
			}
		} else if (connecting_count) {
			status = online_connecting;
		} else {
			status = online;
		}
	} else {
		if (connecting_count) {
			status = offline_connecting;
		} else {
			status = offline;
		}
	}

	/* update the icon if we changed status */
	if (status != oldstatus) {
		icon = status;
		docklet_update_icon();

		/* and schedule the blinker function if messages are pending */
		if (status == online_pending || status == away_pending) {
			g_timeout_add(500, docklet_blink_icon, &docklet);
		}
	}

	return FALSE; /* for when we're called by the glib idle handler */
}

static void docklet_embedded(GtkWidget *widget, void *data) {
	gaim_debug(GAIM_DEBUG_INFO, "docklet", "Tray Icon: embedded\n");
	gaim_gtk_blist_docklet_add();
}

static void docklet_remove_callbacks() {
	gaim_debug(GAIM_DEBUG_INFO, "docklet", "Tray Icon: removing callbacks");

	while (g_source_remove_by_user_data(&docklet)) {
		gaim_debug(GAIM_DEBUG_INFO, NULL, ".");
	}

	gaim_debug(GAIM_DEBUG_INFO, NULL, "\n");
}

static void docklet_destroyed(GtkWidget *widget, void *data) {
	gaim_debug(GAIM_DEBUG_INFO, "docklet", "Tray Icon: destroyed\n");

	gaim_gtk_blist_docklet_remove();

	docklet_flush_queue();

	docklet_remove_callbacks();

	g_object_unref(G_OBJECT(docklet));
	docklet = NULL;

	g_idle_add(docklet_create, &docklet);
}

static gboolean docklet_create() {
	GtkWidget *box;

	if (docklet) {
		/* if this is being called when a tray icon exists, it's because
		   something messed up. try destroying it before we proceed,
		   although docklet_refcount may be all hosed. hopefully won't happen. */
		gaim_debug(GAIM_DEBUG_WARNING, "docklet",
				   "Tray Icon: trying to create icon but it already exists?\n");
		plugin_unload(NULL);
	}

	docklet = egg_tray_icon_new("Gaim");
	box = gtk_event_box_new();
	image = gtk_image_new();

	g_signal_connect(G_OBJECT(docklet), "embedded", G_CALLBACK(docklet_embedded), NULL);
	g_signal_connect(G_OBJECT(docklet), "destroy", G_CALLBACK(docklet_destroyed), NULL);
	g_signal_connect(G_OBJECT(box), "button-press-event", G_CALLBACK(docklet_clicked), NULL);

	gtk_container_add(GTK_CONTAINER(box), image);
	gtk_container_add(GTK_CONTAINER(docklet), box);
	gtk_widget_show_all(GTK_WIDGET(docklet));

	/* ref the docklet before we bandy it about the place */
	g_object_ref(G_OBJECT(docklet));
	docklet_update_status();
	docklet_update_icon();

	gaim_debug(GAIM_DEBUG_INFO, "docklet", "Tray Icon: created\n");

	return FALSE; /* for when we're called by the glib idle handler */
}

static void gaim_signon(struct gaim_connection *gc, void *data) {
	docklet_update_status();
}

static void gaim_signoff(struct gaim_connection *gc, void *data) {
	/* do this when idle so that if the prpl was connecting
	   and was cancelled, we register that connecting_count
	   has returned to 0 */
	g_idle_add(docklet_update_status, &docklet);
}

static void gaim_connecting(struct gaim_account *account, void *data) {
	docklet_update_status();
}

static void gaim_away(struct gaim_connection *gc, char *state, char *message, void *data) {
	/* we only support global away. this is the way it is, ok? */
	docklet_update_status();
}

static void gaim_im_recv(struct gaim_connection *gc, char **who, char **what, void *data) {
	/* if message queuing while away is enabled, this event could be the first
	   message so we need to see if the status (and hence icon) needs changing.
	   do this when idle so that all message processing is completed, queuing
	   etc, before we run. */
	g_idle_add(docklet_update_status, &docklet);
}

/* static void gaim_buddy_signon(struct gaim_connection *gc, char *who, void *data) {
}

static void gaim_buddy_signoff(struct gaim_connection *gc, char *who, void *data) {
}

static void gaim_buddy_away(struct gaim_connection *gc, char *who, void *data) {
}

static void gaim_buddy_back(struct gaim_connection *gc, char *who, void *data) {
}

static void gaim_new_conversation(char *who, void *data) {
} */

static gboolean
plugin_load(GaimPlugin *plugin)
{
	docklet_create(NULL);

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

	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	if (GTK_WIDGET_VISIBLE(docklet)) {
		gaim_gtk_blist_docklet_remove();
	}

	docklet_flush_queue();

	docklet_remove_callbacks();

	g_signal_handlers_disconnect_by_func(G_OBJECT(docklet), G_CALLBACK(docklet_destroyed), NULL);
	gtk_widget_destroy(GTK_WIDGET(docklet));

	g_object_unref(G_OBJECT(docklet));
	docklet = NULL;

	/* do this while gaim has no other way to toggle the global mute */
	gaim_sound_set_mute(FALSE);

	gaim_debug(GAIM_DEBUG_INFO, "docklet", "Tray Icon: removed\n");

	return TRUE;
}

static GtkWidget *
get_config_frame(GaimPlugin *plugin)
{
	GtkWidget *frame;
	GtkWidget *vbox, *hbox;
	GtkWidget *toggle;

	frame = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width(GTK_CONTAINER(frame), 12);

	vbox = gaim_gtk_make_frame(frame, _("Tray Icon Configuration"));
	hbox = gtk_hbox_new(FALSE, 18);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

/*	toggle = gtk_check_button_new_with_mnemonic(_("_Automatically show buddy list on sign on"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), blist_options & OPT_BLIST_APP_BUDDY_SHOW);
	g_signal_connect(G_OBJECT(toggle), "clicked", G_CALLBACK(docklet_toggle_blist_show), NULL);
	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0); */

	toggle = gtk_check_button_new_with_mnemonic(_("_Hide new messages until tray icon is clicked"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), away_options & OPT_AWAY_QUEUE_UNREAD);
	g_signal_connect(G_OBJECT(toggle), "clicked", G_CALLBACK(docklet_toggle_queue), NULL);
	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);

	gtk_widget_show_all(frame);
	return frame;
}

static GaimGtkPluginUiInfo ui_info =
{
	get_config_frame
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
	N_("Interacts with a Notification Area applet (in GNOME or KDE, "
	   "for example) to display the current status of Gaim, allow fast "
	   "access to commonly used functions, and to toggle display of the "
	   "buddy list or login window. Also allows messages to be queued "
	   "until the icon is clicked, similar to ICQ."),
	"Robert McQueen <robot101@debian.org>",           /**< author         */
	WEBSITE,                                          /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	&ui_info,                                         /**< ui_info        */
	NULL                                              /**< extra_info     */
};

static void
__init_plugin(GaimPlugin *plugin)
{
}

GAIM_INIT_PLUGIN(docklet, __init_plugin, info);
