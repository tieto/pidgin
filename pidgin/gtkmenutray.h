/**
 * @file gtkmenutray.h GTK+ Tray menu item
 * @ingroup pidgin
 */

/* Pidgin is the legal property of its developers, whose names are too numerous
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
#ifndef PIDGIN_MENU_TRAY_H
#define PIDGIN_MENU_TRAY_H

#include <gtk/gtkhbox.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtktooltips.h>

#define PIDGIN_TYPE_MENU_TRAY				(pidgin_menu_tray_get_gtype())
#define PIDGIN_MENU_TRAY(obj)				(GTK_CHECK_CAST((obj), PIDGIN_TYPE_MENU_TRAY, PidginMenuTray))
#define PIDGIN_MENU_TRAY_CLASS(klass)		(GTK_CHECK_CLASS_CAST((klass), PIDGIN_TYPE_MENU_TRAY, PidginMenuTrayClass))
#define PIDGIN_IS_MENU_TRAY(obj)			(GTK_CHECK_TYPE((obj), PIDGIN_TYPE_MENU_TRAY))
#define PIDGIN_IS_MENU_TRAY_CLASS(klass)	(GTK_CHECK_CLASS_TYPE((klass), PIDGIN_TYPE_MENU_TRAY))
#define PIDGIN_MENU_TRAY_GET_CLASS(obj)	(GTK_CHECK_GET_CLASS((obj), PIDGIN_TYPE_MENU_TRAY, PidginMenuTrayClass))

typedef struct _PidginMenuTray				PidginMenuTray;
typedef struct _PidginMenuTrayClass		PidginMenuTrayClass;

/** A PidginMenuTray */
struct _PidginMenuTray {
	GtkMenuItem gparent;					/**< The parent instance */
	GtkWidget *tray;						/**< The tray */
	GtkTooltips *tooltips;					/**< Tooltips */
};

/** A PidginMenuTrayClass */
struct _PidginMenuTrayClass {
	GtkMenuItemClass gparent;				/**< The parent class */
};

G_BEGIN_DECLS

/**
 * Registers the PidginMenuTray class if necessary and returns the
 * type ID assigned to it.
 *
 * @return The PidginMenuTray type ID
 */
GType pidgin_menu_tray_get_gtype(void);

/**
 * Creates a new PidginMenuTray
 *
 * @return A new PidginMenuTray
 */
GtkWidget *pidgin_menu_tray_new(void);

/**
 * Gets the box for the PidginMenuTray
 *
 * @param menu_tray The PidginMenuTray
 *
 * @return The box that this menu tray is using
 */
GtkWidget *pidgin_menu_tray_get_box(PidginMenuTray *menu_tray);

/**
 * Appends a widget into the tray
 *
 * @param menu_tray The tray
 * @param widget    The widget
 * @param tooltip   The tooltip for this widget (widget requires its own X-window)
 */
void pidgin_menu_tray_append(PidginMenuTray *menu_tray, GtkWidget *widget, const char *tooltip);

/**
 * Prepends a widget into the tray
 *
 * @param menu_tray The tray
 * @param widget    The widget
 * @param tooltip   The tooltip for this widget (widget requires its own X-window)
 */
void pidgin_menu_tray_prepend(PidginMenuTray *menu_tray, GtkWidget *widget, const char *tooltip);

/**
 * Set the tooltip for a widget
 *
 * @param menu_tray The tray
 * @param widget    The widget
 * @param tooltip   The tooltip to set for the widget (widget requires its own X-window)
 */
void pidgin_menu_tray_set_tooltip(PidginMenuTray *menu_tray, GtkWidget *widget, const char *tooltip);

G_END_DECLS

#endif /* PIDGIN_MENU_TRAY_H */
