#include "gtkmodule.h"

MODULE = Pidgin::Connection  PACKAGE = Pidgin::Connection  PREFIX = pidgin_connection_
PROTOTYPES: ENABLE

Gaim::Handle
pidgin_connection_get_handle()
