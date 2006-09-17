#include "gtkmodule.h"

/* This can't work at the moment since I don't have a typemap for Gtk::Widget.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.
Gtk::Widget
gaim_gtk_plugin_get_config_frame(plugin)
	Gaim::Plugin plugin
*/

MODULE = Gaim::GtkUI::Plugin  PACKAGE = Gaim::GtkUI::Plugin  PREFIX = gaim_gtk_plugin_
PROTOTYPES: ENABLE

MODULE = Gaim::GtkUI::Plugin  PACKAGE = Gaim::GtkUI::Plugins  PREFIX = gaim_gtk_plugins_
PROTOTYPES: ENABLE

void
gaim_gtk_plugins_save()

MODULE = Gaim::GtkUI::Plugin  PACKAGE = Gaim::GtkUI::Plugin::Dialog  PREFIX = gaim_gtk_plugin_dialog_
PROTOTYPES: ENABLE

void
gaim_gtk_plugin_dialog_show()
