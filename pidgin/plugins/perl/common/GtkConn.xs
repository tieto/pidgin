#include "gtkmodule.h"

MODULE = Pidgin::Connection  PACKAGE = Pidgin::Connection  PREFIX = pidgin_connection_
PROTOTYPES: ENABLE

Purple::Handle
pidgin_connection_get_handle()
