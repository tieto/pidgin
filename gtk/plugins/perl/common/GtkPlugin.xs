#include "gtkmodule.h"

MODULE = Gaim::GtkUI::Plugin  PACKAGE = Gaim::GtkUI::Plugins  PREFIX = gaim_gtk_plugins_
PROTOTYPES: ENABLE

void
gaim_gtk_plugins_save()

MODULE = Gaim::GtkUI::Plugin  PACKAGE = Gaim::GtkUI::Plugin::Dialog  PREFIX = gaim_gtk_plugin_dialog_
PROTOTYPES: ENABLE

void
gaim_gtk_plugin_dialog_show()
