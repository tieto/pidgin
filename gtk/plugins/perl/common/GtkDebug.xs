#include "gtkmodule.h"

MODULE = Gaim::Gtk::Debug  PACKAGE = Gaim::Gtk::Debug  PREFIX = gaim_gtk_debug_
PROTOTYPES: ENABLE

void *
gaim_gtk_debug_get_handle()

MODULE = Gaim::Gtk::Debug  PACKAGE = Gaim::Gtk::Debug::Window  PREFIX = gaim_gtk_debug_window_
PROTOTYPES: ENABLE

void
gaim_gtk_debug_window_show()

void
gaim_gtk_debug_window_hide()
