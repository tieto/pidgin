#include "gtkmodule.h"

MODULE = Gaim::GtkUI::Debug  PACKAGE = Gaim::GtkUI::Debug  PREFIX = gaim_gtk_debug_
PROTOTYPES: ENABLE

Gaim::Handle
gaim_gtk_debug_get_handle()

MODULE = Gaim::GtkUI::Debug  PACKAGE = Gaim::GtkUI::Debug::Window  PREFIX = gaim_gtk_debug_window_
PROTOTYPES: ENABLE

void
gaim_gtk_debug_window_show()

void
gaim_gtk_debug_window_hide()
