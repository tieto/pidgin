/*
 * System tray icon (aka docklet) plugin for Purple
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02111-1301, USA.
 */

#include "internal.h"
#include "pidgin.h"
#include "debug.h"
#include "prefs.h"
#include "pidginstock.h"

#include "gtkdialogs.h"

#include "eggtrayicon.h"
#include "gtkdocklet.h"

#define SHORT_EMBED_TIMEOUT 5000
#define LONG_EMBED_TIMEOUT 15000

/* globals */
static EggTrayIcon *docklet = NULL;
static GtkWidget *image = NULL;
static GtkTooltips *tooltips = NULL;
static GdkPixbuf *blank_icon = NULL;
static int embed_timeout = 0;
static int docklet_height = 0;

/* protos */
static void docklet_x11_create(gboolean);

static gboolean
docklet_x11_recreate_cb(gpointer data)
{
	docklet_x11_create(TRUE);

	return FALSE; /* for when we're called by the glib idle handler */
}

static void
docklet_x11_embedded_cb(GtkWidget *widget, void *data)
{
	purple_debug(PURPLE_DEBUG_INFO, "docklet", "embedded\n");
	
	g_source_remove(embed_timeout);
	embed_timeout = 0;
	pidgin_docklet_embedded();
	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/docklet/x11/embedded", FALSE);
}

static void
docklet_x11_destroyed_cb(GtkWidget *widget, void *data)
{
	purple_debug(PURPLE_DEBUG_INFO, "docklet", "destroyed\n");

	pidgin_docklet_remove();

	g_object_unref(G_OBJECT(docklet));
	docklet = NULL;

	g_idle_add(docklet_x11_recreate_cb, NULL);
}

static gboolean
docklet_x11_clicked_cb(GtkWidget *button, GdkEventButton *event, void *data)
{
	if (event->type != GDK_BUTTON_RELEASE)
		return FALSE;

	pidgin_docklet_clicked(event->button);
	return TRUE;
}

static void
docklet_x11_update_icon(PurpleStatusPrimitive status, gboolean connecting, gboolean pending)
{
	const gchar *icon_name = NULL;

	g_return_if_fail(image != NULL);

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

	if(icon_name) {
		int icon_size;
		if (docklet_height < 22)
			icon_size = gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL);
		else if (docklet_height < 32)
			icon_size = gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_SMALL);
		else if (docklet_height < 48)
			icon_size = gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_MEDIUM);
		else
			icon_size = gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_LARGE);

		gtk_image_set_from_stock(GTK_IMAGE(image), icon_name, icon_size);
	}
}

static void
docklet_x11_resize_icon(GtkWidget *widget)
{
	if (docklet_height == MIN(widget->allocation.height, widget->allocation.width))
		return;
	docklet_height = MIN(widget->allocation.height, widget->allocation.width);
	pidgin_docklet_update_icon();
}

static void
docklet_x11_blank_icon(void)
{
	if (!blank_icon) {
		GtkIconSize size = GTK_ICON_SIZE_LARGE_TOOLBAR;
		gint width, height;
		g_object_get(G_OBJECT(image), "icon-size", &size, NULL);
		gtk_icon_size_lookup(size, &width, &height);
		blank_icon = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, width, height);
		gdk_pixbuf_fill(blank_icon, 0);
	}

	gtk_image_set_from_pixbuf(GTK_IMAGE(image), blank_icon);
}

static void
docklet_x11_set_tooltip(gchar *tooltip)
{
	if (!tooltips)
		tooltips = gtk_tooltips_new();

	/* image->parent is a GtkEventBox */
	if (tooltip) {
		gtk_tooltips_enable(tooltips);
		gtk_tooltips_set_tip(tooltips, image->parent, tooltip, NULL);
	} else {
		gtk_tooltips_set_tip(tooltips, image->parent, "", NULL);
		gtk_tooltips_disable(tooltips);
	}
}

#if GTK_CHECK_VERSION(2,2,0)
static void
docklet_x11_position_menu(GtkMenu *menu, int *x, int *y, gboolean *push_in,
						  gpointer user_data)
{
	GtkWidget *widget = GTK_WIDGET(docklet);
	GtkRequisition req;
	gint menu_xpos, menu_ypos;

	gtk_widget_size_request(GTK_WIDGET(menu), &req);
	gdk_window_get_origin(widget->window, &menu_xpos, &menu_ypos);

	menu_xpos += widget->allocation.x;
	menu_ypos += widget->allocation.y;

	if (menu_ypos > gdk_screen_get_height(gtk_widget_get_screen(widget)) / 2)
		menu_ypos -= req.height;
	else
		menu_ypos += widget->allocation.height;

	*x = menu_xpos;
	*y = menu_ypos;

	*push_in = TRUE;
}
#endif

static void
docklet_x11_destroy(void)
{
	g_return_if_fail(docklet != NULL);

	if (embed_timeout)
		g_source_remove(embed_timeout);
	
	pidgin_docklet_remove();
	
	g_signal_handlers_disconnect_by_func(G_OBJECT(docklet), G_CALLBACK(docklet_x11_destroyed_cb), NULL);
	gtk_widget_destroy(GTK_WIDGET(docklet));

	g_object_unref(G_OBJECT(docklet));
	docklet = NULL;

	if (blank_icon)
		g_object_unref(G_OBJECT(blank_icon));
	blank_icon = NULL;

	image = NULL;

	purple_debug(PURPLE_DEBUG_INFO, "docklet", "destroyed\n");
}

static gboolean
docklet_x11_embed_timeout_cb(gpointer data)
{
	/* The docklet was not embedded within the timeout.
	 * Remove it as a visibility manager, but leave the plugin
	 * loaded so that it can embed automatically if/when a notification
	 * area becomes available.
	 */
	purple_debug_info("docklet", "failed to embed within timeout\n");
	pidgin_docklet_remove();
	
	return FALSE;
}

static void
docklet_x11_create(gboolean recreate)
{
	GtkWidget *box;

	if (docklet) {
		/* if this is being called when a tray icon exists, it's because
		   something messed up. try destroying it before we proceed,
		   although docklet_refcount may be all hosed. hopefully won't happen. */
		purple_debug(PURPLE_DEBUG_WARNING, "docklet", "trying to create icon but it already exists?\n");
		docklet_x11_destroy();
	}

	docklet = egg_tray_icon_new(PIDGIN_NAME);
	box = gtk_event_box_new();
	image = gtk_image_new();

	g_signal_connect(G_OBJECT(docklet), "embedded", G_CALLBACK(docklet_x11_embedded_cb), NULL);
	g_signal_connect(G_OBJECT(docklet), "destroy", G_CALLBACK(docklet_x11_destroyed_cb), NULL);
	g_signal_connect(G_OBJECT(docklet), "size-allocate", G_CALLBACK(docklet_x11_resize_icon), NULL);
	g_signal_connect(G_OBJECT(box), "button-release-event", G_CALLBACK(docklet_x11_clicked_cb), NULL);
	gtk_container_add(GTK_CONTAINER(box), image);
	gtk_container_add(GTK_CONTAINER(docklet), box);

	if (!gtk_check_version(2,4,0))
		g_object_set(G_OBJECT(box), "visible-window", FALSE, NULL);

	gtk_widget_show_all(GTK_WIDGET(docklet));

	/* ref the docklet before we bandy it about the place */
	g_object_ref(G_OBJECT(docklet));

	/* This is a hack to avoid a race condition between the docklet getting
	 * embedded in the notification area and the gtkblist restoring its
	 * previous visibility state.  If the docklet does not get embedded within
	 * the timeout, it will be removed as a visibility manager until it does
	 * get embedded.  Ideally, we would only call docklet_embedded() when the
	 * icon was actually embedded. This only happens when the docklet is first
	 * created, not when being recreated.
	 *
	 * The x11 docklet tracks whether it successfully embedded in a pref and
	 * allows for a longer timeout period if it successfully embedded the last
	 * time it was run. This should hopefully solve problems with the buddy
	 * list not properly starting hidden when Pidgin is started on login.
	 */
	if(!recreate) {
		pidgin_docklet_embedded();
		if(purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/docklet/x11/embedded")) {
			embed_timeout = g_timeout_add(LONG_EMBED_TIMEOUT, docklet_x11_embed_timeout_cb, NULL);
		} else {
			embed_timeout = g_timeout_add(SHORT_EMBED_TIMEOUT, docklet_x11_embed_timeout_cb, NULL);
		}
	}

	purple_debug(PURPLE_DEBUG_INFO, "docklet", "created\n");
}

static void
docklet_x11_create_ui_op(void)
{
	docklet_x11_create(FALSE);
}

static struct docklet_ui_ops ui_ops =
{
	docklet_x11_create_ui_op,
	docklet_x11_destroy,
	docklet_x11_update_icon,
	docklet_x11_blank_icon,
	docklet_x11_set_tooltip,
#if GTK_CHECK_VERSION(2,2,0)
	docklet_x11_position_menu
#else
	NULL
#endif
};

void
docklet_ui_init()
{
	pidgin_docklet_set_ui_ops(&ui_ops);
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/docklet/x11");
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/docklet/x11/embedded", FALSE);
}
