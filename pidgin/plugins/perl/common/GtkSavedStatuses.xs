#include "gtkmodule.h"

MODULE = Gaim::GtkUI::Status  PACKAGE = Gaim::GtkUI::Status  PREFIX = gaim_gtk_status_
PROTOTYPES: ENABLE

Gaim::Handle
gaim_gtk_status_get_handle()

MODULE = Gaim::GtkUI::Status  PACKAGE = Gaim::GtkUI::Status::Editor  PREFIX = gaim_gtk_status_editor_
PROTOTYPES: ENABLE

void
gaim_gtk_status_editor_show(edit, status)
	gboolean edit
	Gaim::SavedStatus status

MODULE = Gaim::GtkUI::Status  PACKAGE = Gaim::GtkUI::Status::Window  PREFIX = gaim_gtk_status_window_
PROTOTYPES: ENABLE

void
gaim_gtk_status_window_show()

void
gaim_gtk_status_window_hide()
