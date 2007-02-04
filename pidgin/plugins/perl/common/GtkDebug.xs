#include "gtkmodule.h"

MODULE = Pidgin::Debug  PACKAGE = Pidgin::Debug  PREFIX = pidgin_debug_
PROTOTYPES: ENABLE

Gaim::Handle
pidgin_debug_get_handle()

MODULE = Pidgin::Debug  PACKAGE = Pidgin::Debug::Window  PREFIX = pidgin_debug_window_
PROTOTYPES: ENABLE

void
pidgin_debug_window_show()

void
pidgin_debug_window_hide()
