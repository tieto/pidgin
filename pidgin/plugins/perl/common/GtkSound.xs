#include "gtkmodule.h"

MODULE = Pidgin::Sound  PACKAGE = Pidgin::Sound  PREFIX = pidgin_sound_
PROTOTYPES: ENABLE

const char *
pidgin_sound_get_event_option(event)
	Gaim::SoundEventID event

const char *
pidgin_sound_get_event_label(event)
	Gaim::SoundEventID event

Gaim::Handle
pidgin_sound_get_handle()
