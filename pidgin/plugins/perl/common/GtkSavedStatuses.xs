#include "gtkmodule.h"

MODULE = Pidgin::Status  PACKAGE = Pidgin::Status  PREFIX = pidgin_status_
PROTOTYPES: ENABLE

Purple::Handle
pidgin_status_get_handle()

MODULE = Pidgin::Status  PACKAGE = Pidgin::Status::Editor  PREFIX = pidgin_status_editor_
PROTOTYPES: ENABLE

void
pidgin_status_editor_show(edit, status)
	gboolean edit
	Purple::SavedStatus status

MODULE = Pidgin::Status  PACKAGE = Pidgin::Status::Window  PREFIX = pidgin_status_window_
PROTOTYPES: ENABLE

void
pidgin_status_window_show()

void
pidgin_status_window_hide()
