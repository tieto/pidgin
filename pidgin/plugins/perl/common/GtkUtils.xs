#include "gtkmodule.h"

MODULE = Pidgin::Utils  PACKAGE = Pidgin::Utils  PREFIX = pidgin_
PROTOTYPES: ENABLE

gboolean
pidgin_save_accels(data)
	gpointer data

void
pidgin_load_accels()

gchar_own *
pidgin_convert_buddy_icon(plugin, path, size)
	Purple::Plugin plugin
	const char * path
	size_t *size
