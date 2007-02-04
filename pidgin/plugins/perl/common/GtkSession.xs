#include "gtkmodule.h"

MODULE = Gaim::GtkUI::Session  PACKAGE = Gaim::GtkUI::Session  PREFIX = gaim_gtk_session_
PROTOTYPES: ENABLE

void
pidgin_session_init(argv0, previous_id, config_dir)
	gchar * argv0
	gchar * previous_id
	gchar * config_dir

void
pidgin_session_end()
