#include "gtkmodule.h"

MODULE = Gaim::Gtk::Sound  PACKAGE = Gaim::Gtk::Sound  PREFIX = gaim_gtk_sound_
PROTOTYPES: ENABLE

const char *
gaim_gtk_sound_get_event_option(event)
	Gaim::SoundEventID event

char *
gaim_gtk_sound_get_event_label(event)
	Gaim::SoundEventID event

void *
gaim_gtk_sound_get_handle()
