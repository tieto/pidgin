#include "gtkmodule.h"

/* This can't work at the moment since I don't have a typemap for Gtk::Widget.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.
Gtk::Widget
pidgin_menu_tray_new()

Gtk::Widget
pidgin_menu_tray_get_box(menu_tray)
	Pidgin::MenuTray menu_tray

void
pidgin_menu_tray_append(menu_tray, widget, tooltip)
	Pidgin::MenuTray menu_tray
	Gtk::Widget widget
	const char * tooltip

void
pidgin_menu_tray_prepend(menu_tray, widget, tooltip)
	Pidgin::MenuTray menu_tray
	Gtk::Widget widget
	const char * tooltip

void
pidgin_menu_tray_set_tooltip(menu_tray, widget, tooltip)
	Pidgin::MenuTray menu_tray
	Gtk::Widget widget
	const char * tooltip
*/

MODULE = Pidgin::MenuTray  PACKAGE = Pidgin::MenuTray  PREFIX = pidgin_menu_tray
PROTOTYPES: ENABLE
