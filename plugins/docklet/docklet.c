/* System tray docklet plugin for Gaim
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
    - don't crash when the plugin gets unloaded (it seems to crash after
       the plugin has gone, when gtk updates the button in the plugins
       dialog. backtrace is always useless. weird)
    - have a toggle on the menu to mute all Gaim sounds
    - handle and update tooltips to show your current accounts
    - connecting status support (needs more fruxing with the core)
    - dernyi's account status menu in the right click
    - store icons in gtk2 stock icon thing (needs doing for the whole prog)
    - pop up notices when GNOME2's system-tray-applet supports it, with a
       prefs dialog to choose what to alert for */

/* includes */
#define GAIM_PLUGINS
#include <gtk/gtk.h>
#include "gaim.h"
#include "eggtrayicon.h"

/* macros */
#define DOCKLET_WINDOW_ICONIFIED(x) (gdk_window_get_state(GTK_WIDGET(x)->window) & GDK_WINDOW_STATE_ICONIFIED)

/* types */
enum docklet_status {
	online,
	away,
	away_pending,
	connecting,
	offline
};

/* functions */
static void docklet_create();

/* globals */
static EggTrayIcon *docklet = NULL;
static GtkWidget *icon;
static enum docklet_status status;

static void docklet_embedded(GtkWidget *widget, void *data) {
	debug_printf("Docklet: embedded\n");
	docklet_add();
}

static void docklet_destroyed(GtkWidget *widget, void *data) {
	debug_printf("Docklet: destroyed\n");
	docklet_remove();
	docklet_create();
}

static void docklet_toggle() {
	/* this looks bad, but we need to use (un)hide_buddy_list to allow buddy.c to
	   correctly hide/iconify depending on the docklet refcount, and to reposition
	   the blist for us when we unhide. no such constraint for the login window
	   because nothing else needs a unified way to hide/iconify it. otherwise I'd
	   make a function and use it for both. */
	if (connections) {
		if (GTK_WIDGET_VISIBLE(blist)) {
			if (DOCKLET_WINDOW_ICONIFIED(blist)) {
				unhide_buddy_list();
			} else {
				hide_buddy_list();
			}
		} else {
			unhide_buddy_list();
		}
	} else {
		if (GTK_WIDGET_VISIBLE(mainwindow)) {
			if (DOCKLET_WINDOW_ICONIFIED(mainwindow)) {
				gtk_window_present(GTK_WINDOW(mainwindow));
			} else {
				gtk_widget_hide(mainwindow);
			}
		} else {
			gtk_window_present(GTK_WINDOW(mainwindow));
		}
	}
}

static void docklet_menu(GdkEventButton *event) {
	static GtkWidget *menu = NULL;
	GtkWidget *entry;

	if (menu) {
		gtk_widget_destroy(menu);
	}

	menu = gtk_menu_new();

	if (status == offline) {
		entry = gtk_menu_item_new_with_label(_("Auto-login"));
		g_signal_connect(GTK_WIDGET(entry), "activate", G_CALLBACK(auto_login), NULL);
		gtk_menu_append(GTK_MENU(menu), entry);
	} else {
		if (status == online) {
			GtkWidget *docklet_awaymenu;
			GSList *awy = NULL;
			struct away_message *a = NULL;

			docklet_awaymenu = gtk_menu_new();
			awy = away_messages;

			while (awy) {
				a = (struct away_message *)awy->data;

				entry = gtk_menu_item_new_with_label(a->name);
				g_signal_connect(GTK_WIDGET(entry), "activate", G_CALLBACK(do_away_message), a);
				gtk_menu_append(GTK_MENU(docklet_awaymenu), entry);

				awy = g_slist_next(awy);
			}

			entry = gtk_separator_menu_item_new();
			gtk_menu_append(GTK_MENU(docklet_awaymenu), entry);

			entry = gtk_menu_item_new_with_label(_("New..."));
			g_signal_connect(GTK_WIDGET(entry), "activate", G_CALLBACK(create_away_mess), NULL);
			gtk_menu_append(GTK_MENU(docklet_awaymenu), entry);

			entry = gtk_menu_item_new_with_label(_("Away"));
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(entry), docklet_awaymenu);
			gtk_menu_append(GTK_MENU(menu), entry);
		} else {
			entry = gtk_menu_item_new_with_label(_("Back"));
			g_signal_connect(GTK_WIDGET(entry), "activate", G_CALLBACK(do_im_back), NULL);
			gtk_menu_append(GTK_MENU(menu), entry);
		}

		entry = gtk_menu_item_new_with_label(_("Signoff"));
		g_signal_connect(GTK_WIDGET(entry), "activate", G_CALLBACK(signoff_all), NULL);
		gtk_menu_append(GTK_MENU(menu), entry);
	}

	entry = gtk_separator_menu_item_new();
	gtk_menu_append(GTK_MENU(menu), entry);

	entry = gtk_menu_item_new_with_label(_("Accounts"));
	g_signal_connect(GTK_WIDGET(entry), "activate", G_CALLBACK(account_editor), NULL);
	gtk_menu_append(GTK_MENU(menu), entry);

	entry = gtk_image_menu_item_new_from_stock(GTK_STOCK_PREFERENCES, NULL);
	g_signal_connect(GTK_WIDGET(entry), "activate", G_CALLBACK(show_prefs), NULL);
	gtk_menu_append(GTK_MENU(menu), entry);

	entry = gtk_menu_item_new_with_label(_("Plugins"));
	g_signal_connect(GTK_WIDGET(entry), "activate", G_CALLBACK(show_plugins), NULL);
	gtk_menu_append(GTK_MENU(menu), entry);

	entry = gtk_separator_menu_item_new();
	gtk_menu_append(GTK_MENU(menu), entry);

	entry = gtk_menu_item_new_with_label(_("About"));
	g_signal_connect(GTK_WIDGET(entry), "activate", G_CALLBACK(show_about), NULL);
	gtk_menu_append(GTK_MENU(menu), entry);

	entry = gtk_image_menu_item_new_from_stock(GTK_STOCK_QUIT, NULL);
	g_signal_connect(GTK_WIDGET(entry), "activate", G_CALLBACK(do_quit), NULL);
	gtk_menu_append(GTK_MENU(menu), entry);

	gtk_widget_show_all(menu);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);
}

static void docklet_clicked(GtkWidget *button, GdkEventButton *event, void *data) {
	switch (event->button) {
		case 1:
			docklet_toggle();
			break;
		case 2:
			break;
		case 3:
			docklet_menu(event);
			break;
	}
}

static void docklet_update_icon() {
	gchar *filename;
	GdkPixbuf *unscaled;

	switch (status) {
		case online:
			filename = g_build_filename(DATADIR, "pixmaps", "gaim", "online.png", NULL);
			break;
		case away:
			filename = g_build_filename(DATADIR, "pixmaps", "gaim", "away.png", NULL);
			break;
		case away_pending:
			filename = g_build_filename(DATADIR, "pixmaps", "gaim", "msgpend.png", NULL);
			break;
		case connecting:
			filename = g_build_filename(DATADIR, "pixmaps", "gaim", "connecting.png", NULL);
			break;
		case offline:
			filename = g_build_filename(DATADIR, "pixmaps", "gaim", "offline.png", NULL);
	}

	unscaled = gdk_pixbuf_new_from_file(filename, NULL);

	if (unscaled) {
		GdkPixbuf *scaled;

		scaled = gdk_pixbuf_scale_simple(unscaled, 24, 24, GDK_INTERP_BILINEAR);
		gtk_image_set_from_pixbuf(GTK_IMAGE(icon), scaled);
		g_object_unref(unscaled);
		g_object_unref(scaled);

		debug_printf("Docklet: updated icon to %s\n",filename);
	} else {
		debug_printf("Docklet: failed to load icon from %s\n",filename);
	}

	g_free(filename);
}

static void docklet_update_status() {
	enum docklet_status oldstatus;

	oldstatus = status;

	if (connections) {
		if (imaway) {
			if (message_queue) {
				status = away_pending;
			} else {
				status = away;
			}
		} else {
			status = online;
		}
	} else {
		status = offline;
	}

	if (status != oldstatus) {
		docklet_update_icon();
	}
}

static void docklet_create() {
	GtkWidget *box;

	/* is this necessary/wise? */
	if (docklet) {
		g_signal_handlers_disconnect_by_func(GTK_WIDGET(docklet), G_CALLBACK(docklet_destroyed), NULL);
		gtk_widget_destroy(GTK_WIDGET(docklet));
		debug_printf("Docklet: freed\n");
	}

	docklet = egg_tray_icon_new("Gaim");
	box = gtk_event_box_new();
	icon = gtk_image_new();

	g_signal_connect(GTK_WIDGET(docklet), "embedded", G_CALLBACK(docklet_embedded), NULL);
	g_signal_connect(GTK_WIDGET(docklet), "destroy", G_CALLBACK(docklet_destroyed), NULL);
	g_signal_connect(box, "button-press-event", G_CALLBACK(docklet_clicked), NULL);

	gtk_container_add(GTK_CONTAINER(box), icon);
	gtk_container_add(GTK_CONTAINER(docklet), box);
	gtk_widget_show_all(GTK_WIDGET(docklet));

	docklet_update_status();
	docklet_update_icon();

	debug_printf("Docklet: created\n");
}

static void gaim_signon(struct gaim_connection *gc, void *data) {
	docklet_update_status();
}

static void gaim_signoff(struct gaim_connection *gc, void *data) {
	docklet_update_status();
}

static void gaim_connecting(struct aim_user *user, void *data) {
	docklet_update_status();
}

static void gaim_away(struct gaim_connection *gc, char *state, char *message, void *data) {
	/* we only support global away. this is the way it is, ok? */
	docklet_update_status();
}

static void gaim_im_recv(struct gaim_connection *gc, char **who, char **what, void *data) {
	/* if message queuing while away is enabled, this event could be the first
	   message so we need to see if the status (and hence icon) needs changing */
	docklet_update_status();
}

static void gaim_buddy_signon(struct gaim_connection *gc, char *who, void *data) {
}

static void gaim_buddy_signoff(struct gaim_connection *gc, char *who, void *data) {
}

static void gaim_buddy_away(struct gaim_connection *gc, char *who, void *data) {
}

static void gaim_buddy_back(struct gaim_connection *gc, char *who, void *data) {
}

static void gaim_new_conversation(char *who, void *data) {
}

char *gaim_plugin_init(GModule *handle) {
	docklet_create();

	gaim_signal_connect(handle, event_signon, gaim_signon, NULL);
	gaim_signal_connect(handle, event_signoff, gaim_signoff, NULL);
	gaim_signal_connect(handle, event_connecting, gaim_connecting, NULL);
	gaim_signal_connect(handle, event_away, gaim_away, NULL);
	gaim_signal_connect(handle, event_im_recv, gaim_im_recv, NULL);
	gaim_signal_connect(handle, event_buddy_signon, gaim_buddy_signon, NULL);
	gaim_signal_connect(handle, event_buddy_signoff, gaim_buddy_signoff, NULL);
	gaim_signal_connect(handle, event_buddy_away, gaim_buddy_away, NULL);
	gaim_signal_connect(handle, event_buddy_back, gaim_buddy_back, NULL);
	gaim_signal_connect(handle, event_new_conversation, gaim_new_conversation, NULL);

	return NULL;
}

void gaim_plugin_remove() {
	if (GTK_WIDGET_VISIBLE(docklet)) {
		docklet_remove();
	}

	g_signal_handlers_disconnect_by_func(GTK_WIDGET(docklet), G_CALLBACK(docklet_destroyed), NULL);
	gtk_widget_destroy(GTK_WIDGET(docklet));

	debug_printf("Docklet: removed\n");
}

const char *name() {
	return _("System Tray Docklet");
}

const char *description() {
	return _("Interacts with a System Tray applet (in GNOME or KDE, for example) to display the current status of Gaim, allow fast access to commonly used functions, and to toggle display of the buddy list or login window.");
}
