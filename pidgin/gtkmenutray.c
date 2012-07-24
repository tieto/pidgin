/*
 * Pidgin is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * under the terms of the GNU General Public License as published by
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#include "debug.h"

#include "gtkmenutray.h"

#include <gtk/gtk.h>

#include "gtk3compat.h"

/******************************************************************************
 * Enums
 *****************************************************************************/
enum {
	PROP_ZERO = 0,
	PROP_BOX
};

/******************************************************************************
 * Globals
 *****************************************************************************/
static GObjectClass *parent_class = NULL;
#if !GTK_CHECK_VERSION(2,12,0)
static GtkTooltips *tooltips = NULL;
#endif

/******************************************************************************
 * Internal Stuff
 *****************************************************************************/

/******************************************************************************
 * Item Stuff
 *****************************************************************************/
#if GTK_CHECK_VERSION(3,0,0)
static void
pidgin_menu_tray_select(GtkMenuItem *widget) {
	/* this may look like nothing, but it's really overriding the
	 * GtkMenuItem's select function so that it doesn't get highlighted like
	 * a normal menu item would.
	 */
}

static void
pidgin_menu_tray_deselect(GtkMenuItem *widget) {
	/* Probably not necessary, but I'd rather be safe than sorry.  We're
	 * overridding the select, so it makes sense to override deselect as well.
	 */
}
#else
static void
pidgin_menu_tray_select(GtkItem *widget) {
	/* this may look like nothing, but it's really overriding the
	 * GtkMenuItem's select function so that it doesn't get highlighted like
	 * a normal menu item would.
	 */
}
static void
pidgin_menu_tray_deselect(GtkItem *widget) {
	/* Probably not necessary, but I'd rather be safe than sorry.  We're
	 * overridding the select, so it makes sense to override deselect as well.
	 */
}
#endif

/******************************************************************************
 * Widget Stuff
 *****************************************************************************/
#if !GTK_CHECK_VERSION(2,12,0)
static void
tooltips_unref_cb(gpointer data, GObject *object, gboolean is_last_ref)
{
	if (is_last_ref) {
		g_object_unref(tooltips);
		tooltips = NULL;
	}
}
#endif

/******************************************************************************
 * Object Stuff
 *****************************************************************************/
static void
pidgin_menu_tray_get_property(GObject *obj, guint param_id, GValue *value,
								GParamSpec *pspec)
{
	PidginMenuTray *menu_tray = PIDGIN_MENU_TRAY(obj);

	switch(param_id) {
		case PROP_BOX:
			g_value_set_object(value, pidgin_menu_tray_get_box(menu_tray));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
pidgin_menu_tray_map(GtkWidget *widget)
{
	GTK_WIDGET_CLASS(parent_class)->map(widget);
	gtk_container_add(GTK_CONTAINER(widget),
			PIDGIN_MENU_TRAY(widget)->tray);
}

static void
pidgin_menu_tray_finalize(GObject *obj)
{
#if 0
	PidginMenuTray *tray = PIDGIN_MENU_TRAY(obj);

	/* This _might_ be leaking, but I have a sneaking suspicion that the widget is
	 * getting destroyed in GtkContainer's finalize function.  But if were are
	 * leaking here, be sure to figure out why this causes a crash.
	 *	-- Gary
	 */

	if(GTK_IS_WIDGET(tray->tray))
		gtk_widget_destroy(GTK_WIDGET(tray->tray));
#endif

	G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static void
pidgin_menu_tray_class_init(PidginMenuTrayClass *klass) {
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
#if GTK_CHECK_VERSION(3,0,0)
	GtkMenuItemClass *menu_item_class = GTK_MENU_ITEM_CLASS(klass);
#else
	GtkItemClass *menu_item_class = GTK_ITEM_CLASS(klass);
#endif
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
	GParamSpec *pspec;

	parent_class = g_type_class_peek_parent(klass);

	object_class->finalize = pidgin_menu_tray_finalize;
	object_class->get_property = pidgin_menu_tray_get_property;

	menu_item_class->select = pidgin_menu_tray_select;
	menu_item_class->deselect = pidgin_menu_tray_deselect;

	widget_class->map = pidgin_menu_tray_map;

	pspec = g_param_spec_object("box", "The box",
								"The box",
								GTK_TYPE_BOX,
								G_PARAM_READABLE);
	g_object_class_install_property(object_class, PROP_BOX, pspec);
}

static void
pidgin_menu_tray_init(PidginMenuTray *menu_tray) {
	GtkWidget *widget = GTK_WIDGET(menu_tray);
	GtkSettings *settings;
	gint height = -1;

	gtk_menu_item_set_right_justified(GTK_MENU_ITEM(menu_tray), TRUE);

	if(!GTK_IS_WIDGET(menu_tray->tray))
		menu_tray->tray = gtk_hbox_new(FALSE, 0);

	settings =
		gtk_settings_get_for_screen(gtk_widget_get_screen(widget));

	if(gtk_icon_size_lookup_for_settings(settings, GTK_ICON_SIZE_MENU,
										 NULL, &height))
	{
		gtk_widget_set_size_request(widget, -1, height);
	}

	gtk_widget_show(menu_tray->tray);
}

/******************************************************************************
 * API
 *****************************************************************************/
GType
pidgin_menu_tray_get_gtype(void) {
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PidginMenuTrayClass),
			NULL,
			NULL,
			(GClassInitFunc)pidgin_menu_tray_class_init,
			NULL,
			NULL,
			sizeof(PidginMenuTray),
			0,
			(GInstanceInitFunc)pidgin_menu_tray_init,
			NULL
		};

		type = g_type_register_static(GTK_TYPE_MENU_ITEM,
									  "PidginMenuTray",
									  &info, 0);
	}

	return type;
}

GtkWidget *
pidgin_menu_tray_new() {
	return g_object_new(PIDGIN_TYPE_MENU_TRAY, NULL);
}

GtkWidget *
pidgin_menu_tray_get_box(PidginMenuTray *menu_tray) {
	g_return_val_if_fail(PIDGIN_IS_MENU_TRAY(menu_tray), NULL);
	return menu_tray->tray;
}

static void
pidgin_menu_tray_add(PidginMenuTray *menu_tray, GtkWidget *widget,
					   const char *tooltip, gboolean prepend)
{
	g_return_if_fail(PIDGIN_IS_MENU_TRAY(menu_tray));
	g_return_if_fail(GTK_IS_WIDGET(widget));

	if (!gtk_widget_get_has_window(widget))
	{
		GtkWidget *event;

		event = gtk_event_box_new();
		gtk_container_add(GTK_CONTAINER(event), widget);
		gtk_widget_show(event);
		widget = event;
	}

	pidgin_menu_tray_set_tooltip(menu_tray, widget, tooltip);

	if (prepend)
		gtk_box_pack_start(GTK_BOX(menu_tray->tray), widget, FALSE, FALSE, 0);
	else
		gtk_box_pack_end(GTK_BOX(menu_tray->tray), widget, FALSE, FALSE, 0);
}

void
pidgin_menu_tray_append(PidginMenuTray *menu_tray, GtkWidget *widget, const char *tooltip)
{
	pidgin_menu_tray_add(menu_tray, widget, tooltip, FALSE);
}

void
pidgin_menu_tray_prepend(PidginMenuTray *menu_tray, GtkWidget *widget, const char *tooltip)
{
	pidgin_menu_tray_add(menu_tray, widget, tooltip, TRUE);
}

void
pidgin_menu_tray_set_tooltip(PidginMenuTray *menu_tray, GtkWidget *widget, const char *tooltip)
{
#if !GTK_CHECK_VERSION(2,12,0)
	gboolean notify_tooltips = FALSE;
	if (!tooltips) {
		tooltips = gtk_tooltips_new();
		notify_tooltips = TRUE;
	}
#endif

	/* Should we check whether widget is a child of menu_tray? */

	/*
	 * If the widget does not have its own window, then it
	 * must have automatically been added to an event box
	 * when it was added to the menu tray.  If this is the
	 * case, we want to set the tooltip on the widget's parent,
	 * not on the widget itself.
	 */
	if (!gtk_widget_get_has_window(widget))
		widget = gtk_widget_get_parent(widget);

#if GTK_CHECK_VERSION(2,12,0)
	gtk_widget_set_tooltip_text(widget, tooltip);
#else
	gtk_tooltips_set_tip(tooltips, widget, tooltip, NULL);

	if (notify_tooltips)
		g_object_add_toggle_ref(G_OBJECT(tooltips), tooltips_unref_cb, NULL);
#endif
}

