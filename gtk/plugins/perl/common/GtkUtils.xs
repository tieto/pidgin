#include "gtkmodule.h"

MODULE = Gaim::GtkUI::Utils  PACKAGE = Gaim::GtkUI::Utils  PREFIX = gaim_gtk_
PROTOTYPES: ENABLE

gboolean
gaim_gtk_save_accels(data)
	gpointer data

void
gaim_gtk_load_accels()

char *
gaim_gtk_convert_buddy_icon(plugin, path)
	Gaim::Plugin plugin
	const char * path
