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
    - check removing the icon factory actually frees the icons
    - unify the queue so we can have a global away without the dialog
    - handle and update tooltips to show your current accounts/queued messages?
    - show a count of queued messages in the unified queue
    - dernyi's account status menu in the right click
    - optional pop up notices when GNOME2's system-tray-applet supports it */

/* includes */
#include <gtk/gtk.h>
#include "gaim.h"
#include "eggtrayicon.h"

#ifndef GAIM_PLUGINS
#define GAIM_PLUGINS
#endif

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
void gaim_plugin_remove();

/* globals */
static EggTrayIcon *docklet = NULL;
static GtkWidget *image = NULL;
static GtkIconFactory *icon_factory = NULL;
static enum docklet_status status;
static enum docklet_status icon;

static void docklet_toggle_mute(GtkWidget *toggle, void *data) {
	mute_sounds = GTK_CHECK_MENU_ITEM(toggle)->active;
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
				gtk_menu_append(GTK_MENU(docklet_awaymenu), entry);

				awy = g_slist_next(awy);
			}

			if (away_messages)
				gaim_separator(docklet_awaymenu);

			entry = gtk_menu_item_new_with_label(_("New..."));
			g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(create_away_mess), NULL);
			gtk_menu_append(GTK_MENU(docklet_awaymenu), entry);

			entry = gtk_menu_item_new_with_label(_("Away"));
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(entry), docklet_awaymenu);
			gtk_menu_append(GTK_MENU(menu), entry);
			} break;
		case away:
		case away_pending:
			entry = gtk_menu_item_new_with_label(_("Back"));
			g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(do_im_back), NULL);
			gtk_menu_append(GTK_MENU(menu), entry);
			break;
	}

	switch (status) {
		case offline:
		case offline_connecting:
			entry = gtk_menu_item_new_with_label(_("Auto-login"));
			g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(auto_login), NULL);
			gtk_menu_append(GTK_MENU(menu), entry);
			break;
		default:
			entry = gtk_menu_item_new_with_label(_("Signoff"));
			g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(signoff_all), NULL);
			gtk_menu_append(GTK_MENU(menu), entry);
			break;
	}

	gaim_separator(menu);

	entry = gtk_check_menu_item_new_with_label(_("Mute Sounds"));
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(entry), mute_sounds);
	g_signal_connect(G_OBJECT(entry), "toggled", G_CALLBACK(docklet_toggle_mute), NULL);
	gtk_menu_append(GTK_MENU(menu), entry);

	gaim_new_item_from_pixbuf(menu, _("Accounts..."), "accounts-menu.png", G_CALLBACK(account_editor), NULL, 0, 0, 0);
	gaim_new_item_from_stock(menu, _("Preferences..."), GTK_STOCK_PREFERENCES, G_CALLBACK(show_prefs), NULL, 0, 0, 0);

	gaim_separator(menu);

	gaim_new_item_from_pixbuf(menu, _("About Gaim..."), "about_menu.png", G_CALLBACK(show_about), NULL, 0, 0, 0);
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
				docklet_toggle();
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
			icon_name = "gaim-docklet-offline";
			break;
		case offline_connecting:
		case online_connecting:
			icon_name = "gaim-docklet-connect";
			break;
		case online:
			icon_name = "gaim-docklet-online";
			break;
		case online_pending:
			icon_name = "gaim-docklet-msgunread";
			break;
		case away:
			icon_name = "gaim-docklet-away";
			break;
		case away_pending:
			icon_name = "gaim-docklet-msgpend";
			break;
	}

	gtk_image_set_from_stock(GTK_IMAGE(image), icon_name, GTK_ICON_SIZE_LARGE_TOOLBAR);

	debug_printf("Tray Icon: updated icon to '%s'\n", icon_name);
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
	debug_printf("Tray Icon: embedded\n");
	docklet_add();
}

static void docklet_remove_callbacks() {
	debug_printf("Tray Icon: removing callbacks");

	while (g_source_remove_by_user_data(&docklet)) {
		debug_printf(".");
	}

	debug_printf("\n");
}

static void docklet_destroyed(GtkWidget *widget, void *data) {
	debug_printf("Tray Icon: destroyed\n");

	docklet_remove();

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
		debug_printf("Tray Icon: trying to create icon but it already exists?\n");
		gaim_plugin_remove();
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

	debug_printf("Tray Icon: created\n");

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

static void gaim_connecting(struct aim_user *user, void *data) {
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

static void docklet_register_icon(const char *name, char *fn) {
	gchar *filename;

	filename = g_build_filename(DATADIR, "pixmaps", "gaim", fn, NULL);
	gtk_icon_factory_add(icon_factory, name,
		gtk_icon_set_new_from_pixbuf(gdk_pixbuf_new_from_file(filename, NULL)));
	g_free(filename);
}

static void docklet_register_icon_factory() {
	icon_factory = gtk_icon_factory_new();
	
	docklet_register_icon("gaim-docklet-offline", "offline.png");
	docklet_register_icon("gaim-docklet-connect", "connect.png");
	docklet_register_icon("gaim-docklet-online", "online.png");
	docklet_register_icon("gaim-docklet-msgunread", "msgunread.png");
	docklet_register_icon("gaim-docklet-away", "away.png");
	docklet_register_icon("gaim-docklet-msgpend", "msgpend.png");

	gtk_icon_factory_add_default(icon_factory);
}

static void docklet_unregister_icon_factory() {
	gtk_icon_factory_remove_default(icon_factory);
}

char *gaim_plugin_init(GModule *handle) {
	docklet_register_icon_factory();

	docklet_create(NULL);

	gaim_signal_connect(handle, event_signon, gaim_signon, NULL);
	gaim_signal_connect(handle, event_signoff, gaim_signoff, NULL);
	gaim_signal_connect(handle, event_connecting, gaim_connecting, NULL);
	gaim_signal_connect(handle, event_away, gaim_away, NULL);
	gaim_signal_connect(handle, event_im_recv, gaim_im_recv, NULL);
/*	gaim_signal_connect(handle, event_buddy_signon, gaim_buddy_signon, NULL);
	gaim_signal_connect(handle, event_buddy_signoff, gaim_buddy_signoff, NULL);
	gaim_signal_connect(handle, event_buddy_away, gaim_buddy_away, NULL);
	gaim_signal_connect(handle, event_buddy_back, gaim_buddy_back, NULL);
	gaim_signal_connect(handle, event_new_conversation, gaim_new_conversation, NULL); */

	return NULL;
}

void gaim_plugin_remove() {
	if (GTK_WIDGET_VISIBLE(docklet)) {
		docklet_remove();
	}

	docklet_flush_queue();

	docklet_remove_callbacks();

	g_signal_handlers_disconnect_by_func(G_OBJECT(docklet), G_CALLBACK(docklet_destroyed), NULL);
	gtk_widget_destroy(GTK_WIDGET(docklet));

	g_object_unref(G_OBJECT(docklet));
	docklet = NULL;
	
	docklet_unregister_icon_factory();

	debug_printf("Tray Icon: removed\n");
}

GtkWidget *gaim_plugin_config_gtk() {
	GtkWidget *frame;
	GtkWidget *vbox, *hbox;
	GtkWidget *toggle;

	frame = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width(GTK_CONTAINER(frame), 12);

	vbox = make_frame(frame, _("Tray Icon Configuration"));
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

struct gaim_plugin_description desc; 
struct gaim_plugin_description *gaim_plugin_desc() {
	desc.api_version = PLUGIN_API_VERSION;
	desc.name = g_strdup(_("Tray Icon"));
	desc.version = g_strdup(VERSION);
	desc.description = g_strdup(_("Interacts with a System Tray applet (in GNOME or KDE, for example) to display the current status of Gaim, allow fast access to commonly used functions, and to toggle display of the buddy list or login window. Also allows messages to be queued until the icon is clicked, similar to ICQ (although the icon doesn't flash yet =)."));
	desc.authors = g_strdup(_("Robert McQueen &lt;robot101@debian.org>"));
	desc.url = g_strdup(WEBSITE);
	return &desc;
}

char *name() {
	return _("System Tray Icon");
}

char *description() {
	return _("Interacts with a System Tray applet (in GNOME or KDE, for example) to display the current status of Gaim, allow fast access to commonly used functions, and to toggle display of the buddy list or login window. Also allows messages to be queued until the icon is clicked, similar to ICQ (although the icon doesn't flash yet =).");
}
