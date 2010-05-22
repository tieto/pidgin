/*
 * System tray icon (aka docklet) plugin for Purple
 *
 * Copyright (C) 2007 Anders Hasselqvist
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
#include "pidgin.h"
#include "debug.h"
#include "prefs.h"
#include "pidginstock.h"
#include "gtkdocklet.h"

/* globals */
GtkStatusIcon *docklet = NULL;

static void
docklet_gtk_status_activated_cb(GtkStatusIcon *status_icon, gpointer user_data)
{
	pidgin_docklet_clicked(1); 
}

static void
docklet_gtk_status_clicked_cb(GtkStatusIcon *status_icon, guint button, guint activate_time, gpointer user_data)
{
	pidgin_docklet_clicked(button); 
}

static void
docklet_gtk_status_update_icon(PurpleStatusPrimitive status, gboolean connecting, gboolean pending)
{
	const gchar *icon_name = NULL;

	switch (status) {
		case PURPLE_STATUS_OFFLINE:
			icon_name = PIDGIN_STOCK_TRAY_OFFLINE;
			break;
		case PURPLE_STATUS_AWAY:
			icon_name = PIDGIN_STOCK_TRAY_AWAY;
			break;
		case PURPLE_STATUS_UNAVAILABLE:
			icon_name = PIDGIN_STOCK_TRAY_BUSY;
			break;
		case PURPLE_STATUS_EXTENDED_AWAY:
			icon_name = PIDGIN_STOCK_TRAY_XA;
			break;
		case PURPLE_STATUS_INVISIBLE:
			icon_name = PIDGIN_STOCK_TRAY_INVISIBLE;
			break;
		default:
			icon_name = PIDGIN_STOCK_TRAY_AVAILABLE;
			break;
	}

	if (pending)
		icon_name = PIDGIN_STOCK_TRAY_PENDING;
	if (connecting)
		icon_name = PIDGIN_STOCK_TRAY_CONNECT;

	if (icon_name) {
		gtk_status_icon_set_from_icon_name(docklet, icon_name);
	}

	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/docklet/blink")) {
		gtk_status_icon_set_blinking(docklet, (pending && !connecting));
	} else if (gtk_status_icon_get_blinking(docklet)) {
		gtk_status_icon_set_blinking(docklet, FALSE);
	}
}

static void
docklet_gtk_status_set_tooltip(gchar *tooltip)
{
	if (tooltip) {
		gtk_status_icon_set_tooltip(docklet, tooltip);
	} else {
		gtk_status_icon_set_tooltip(docklet, NULL);
	}
}

static void
docklet_gtk_status_position_menu(GtkMenu *menu,
                                 int *x, int *y, gboolean *push_in,
                                 gpointer user_data)
{
	gtk_status_icon_position_menu(menu, x, y, push_in, docklet);
}

static void
docklet_gtk_status_destroy(void)
{
	g_return_if_fail(docklet != NULL);

	pidgin_docklet_remove();

	gtk_status_icon_set_visible(docklet, FALSE);
	g_object_unref(G_OBJECT(docklet));
	docklet = NULL;

	purple_debug_info("docklet", "GTK+ destroyed\n");
}

static void
docklet_gtk_status_create(gboolean recreate)
{
	if (docklet) {
		/* if this is being called when a tray icon exists, it's because
		   something messed up. try destroying it before we proceed,
		   although docklet_refcount may be all hosed. hopefully won't happen. */
		purple_debug_warning("docklet", "trying to create icon but it already exists?\n");
		docklet_gtk_status_destroy();
	}

	docklet = gtk_status_icon_new();
	g_return_if_fail(docklet != NULL);

	g_signal_connect(G_OBJECT(docklet), "activate", G_CALLBACK(docklet_gtk_status_activated_cb), NULL);
	g_signal_connect(G_OBJECT(docklet), "popup-menu", G_CALLBACK(docklet_gtk_status_clicked_cb), NULL);

	pidgin_docklet_embedded();
	gtk_status_icon_set_visible(docklet, TRUE);
	purple_debug_info("docklet", "GTK+ created\n");
}

static void
docklet_gtk_status_create_ui_op(void)
{
	docklet_gtk_status_create(FALSE);
}

static struct docklet_ui_ops ui_ops =
{
	docklet_gtk_status_create_ui_op,
	docklet_gtk_status_destroy,
	docklet_gtk_status_update_icon,
	NULL,
	docklet_gtk_status_set_tooltip,
	docklet_gtk_status_position_menu
};

void
docklet_ui_init(void)
{
	pidgin_docklet_set_ui_ops(&ui_ops);
	gtk_icon_theme_append_search_path(gtk_icon_theme_get_default(),
		DATADIR G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S "pidgin" G_DIR_SEPARATOR_S "tray");
}
