#include "gtkmodule.h"

MODULE = Pidgin::Plugin  PACKAGE = Pidgin::Plugins  PREFIX = pidgin_plugins_
PROTOTYPES: ENABLE

void
pidgin_plugins_save()

MODULE = Pidgin::Plugin  PACKAGE = Pidgin::Plugin::Dialog  PREFIX = pidgin_plugin_dialog_
PROTOTYPES: ENABLE

void
pidgin_plugin_dialog_show()
