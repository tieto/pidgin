/*
 * System tray icon (aka docklet) plugin for Gaim
 *
 * Copyright (C) 2002 Robert McQueen <robot101@debian.org>
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
#include "internal.h"
#include "debug.h"
#include "stock.h"

#include "gaim.h"
#include "ui.h"

#include "eggtrayicon.h"
#include "docklet.h"


/* globals */
static EggTrayIcon *docklet = NULL;
static GtkWidget *image = NULL;

/* protos */
static void gaim_tray_remove_callbacks();
static void gaim_tray_create();

static void gaim_tray_embedded_cb(GtkWidget *widget, void *data) {
        docklet_embedded();
}

static gboolean gaim_tray_create_cb() {
        gaim_tray_create();
	return FALSE; /* for when we're called by the glib idle handler */
}

static void gaim_tray_clicked_cb(GtkWidget *button, GdkEventButton *event, void *data) {
	if (event->type != GDK_BUTTON_PRESS)
		return;
        docklet_clicked(event->button);
}

static void gaim_tray_destroyed_cb(GtkWidget *widget, void *data) {
	gaim_debug(GAIM_DEBUG_INFO, "docklet", "Tray Icon: destroyed\n");

	docklet_flush_queue();

	gaim_tray_remove_callbacks();

	g_object_unref(G_OBJECT(docklet));
	docklet = NULL;

	g_idle_add(gaim_tray_create_cb, &docklet);
}

static void gaim_tray_remove_callbacks() {
	gaim_debug(GAIM_DEBUG_INFO, "docklet", "Tray Icon: removing callbacks");

	while (g_source_remove_by_user_data(&docklet)) {
		gaim_debug(GAIM_DEBUG_INFO, NULL, ".");
	}

	gaim_debug(GAIM_DEBUG_INFO, NULL, "\n");
}


static void gaim_tray_update_icon(enum docklet_status icon) {
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

static void gaim_tray_create() {
	GtkWidget *box;

	if (docklet) {
		/* if this is being called when a tray icon exists, it's because
		   something messed up. try destroying it before we proceed,
		   although docklet_refcount may be all hosed. hopefully won't happen. */
		gaim_debug(GAIM_DEBUG_WARNING, "docklet",
				   "Tray Icon: trying to create icon but it already exists?\n");
		/*plugin_unload(NULL);*/
	}

	docklet = egg_tray_icon_new("Gaim");
	box = gtk_event_box_new();
	image = gtk_image_new();

	g_signal_connect(G_OBJECT(docklet), "embedded", G_CALLBACK(gaim_tray_embedded_cb), NULL);
	g_signal_connect(G_OBJECT(docklet), "destroy", G_CALLBACK(gaim_tray_destroyed_cb), NULL);
	g_signal_connect(G_OBJECT(box), "button-press-event", G_CALLBACK(gaim_tray_clicked_cb), NULL);

	gtk_container_add(GTK_CONTAINER(box), image);
	gtk_container_add(GTK_CONTAINER(docklet), box);
	gtk_widget_show_all(GTK_WIDGET(docklet));

	/* ref the docklet before we bandy it about the place */
	g_object_ref(G_OBJECT(docklet));

	gaim_debug(GAIM_DEBUG_INFO, "docklet", "Tray Icon: created\n");
}

static void gaim_tray_destroy() {
	gaim_tray_remove_callbacks();

	g_signal_handlers_disconnect_by_func(G_OBJECT(docklet), G_CALLBACK(gaim_tray_destroyed_cb), NULL);
	gtk_widget_destroy(GTK_WIDGET(docklet));

	g_object_unref(G_OBJECT(docklet));
	docklet = NULL;

	gaim_debug(GAIM_DEBUG_INFO, "docklet", "Tray Icon: removed\n");
}

static struct gaim_tray_ops tray_ops =
{
	gaim_tray_create,
	gaim_tray_destroy,
	gaim_tray_update_icon
};

/* Used by docklet's plugin load func */
void trayicon_init() {
	docklet_set_tray_ops(&tray_ops);
}
