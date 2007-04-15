/**
 * @file gtkmenutray.h GTK+ Tray menu item
 * @ingroup gtkui
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef GAIM_GTK_MENU_TRAY_H
#define GAIM_GTK_MENU_TRAY_H

#include <gtk/gtkhbox.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtktooltips.h>

#define GAIM_GTK_TYPE_MENU_TRAY				(gaim_gtk_menu_tray_get_gtype())
#define GAIM_GTK_MENU_TRAY(obj)				(GTK_CHECK_CAST((obj), GAIM_GTK_TYPE_MENU_TRAY, GaimGtkMenuTray))
#define GAIM_GTK_MENU_TRAY_CLASS(klass)		(GTK_CHECK_CLASS_CAST((klass), GAIM_GTK_TYPE_MENU_TRAY, GaimGtkMenuTrayClass))
#define GAIM_GTK_IS_MENU_TRAY(obj)			(GTK_CHECK_TYPE((obj), GAIM_GTK_TYPE_MENU_TRAY))
#define GAIM_GTK_IS_MENU_TRAY_CLASS(klass)	(GTK_CHECK_CLASS_TYPE((klass), GAIM_GTK_TYPE_MENU_TRAY))
#define GAIM_GTK_MENU_TRAY_GET_CLASS(obj)	(GTK_CHECK_GET_CLASS((obj), GAIM_GTK_TYPE_MENU_TRAY, GaimGtkMenuTrayClass))

typedef struct _GaimGtkMenuTray				GaimGtkMenuTray;
typedef struct _GaimGtkMenuTrayClass		GaimGtkMenuTrayClass;

/** A GaimGtkMenuTray */
struct _GaimGtkMenuTray {
	GtkMenuItem gparent;					/**< The parent instance */
	GtkWidget *tray;						/**< The tray */
	GtkTooltips *tooltips;					/**< Tooltips */
};

/** A GaimGtkMenuTrayClass */
struct _GaimGtkMenuTrayClass {
	GtkMenuItemClass gparent;				/**< The parent class */
};

G_BEGIN_DECLS

/**
 * Registers the GaimGtkMenuTray class if necessary and returns the
 * type ID assigned to it.
 *
 * @return The GaimGtkMenuTray type ID
 */
GType gaim_gtk_menu_tray_get_gtype(void);

/**
 * Creates a new GaimGtkMenuTray
 *
 * @return A new GaimGtkMenuTray
 */
GtkWidget *gaim_gtk_menu_tray_new(void);

/**
 * Gets the box for the GaimGtkMenuTray
 *
 * @param menu_tray The GaimGtkMenuTray
 *
 * @return The box that this menu tray is using
 */
GtkWidget *gaim_gtk_menu_tray_get_box(GaimGtkMenuTray *menu_tray);

/**
 * Appends a widget into the tray
 *
 * @param menu_tray The tray
 * @param widget    The widget
 * @param tooltip   The tooltip for this widget (widget requires its own X-window)
 */
void gaim_gtk_menu_tray_append(GaimGtkMenuTray *menu_tray, GtkWidget *widget, const char *tooltip);

/**
 * Prepends a widget into the tray
 *
 * @param menu_tray The tray
 * @param widget    The widget
 * @param tooltip   The tooltip for this widget (widget requires its own X-window)
 */
void gaim_gtk_menu_tray_prepend(GaimGtkMenuTray *menu_tray, GtkWidget *widget, const char *tooltip);

/**
 * Set the tooltip for a widget
 *
 * @param menu_tray The tray
 * @param widget    The widget
 * @param tooltip   The tooltip to set for the widget (widget requires its own X-window)
 */
void gaim_gtk_menu_tray_set_tooltip(GaimGtkMenuTray *menu_tray, GtkWidget *widget, const char *tooltip);

G_END_DECLS

#endif /* GAIM_GTK_MENU_TRAY_H */
