#include "gtkmodule.h"

MODULE = Pidgin::Sound  PACKAGE = Pidgin::Sound  PREFIX = pidgin_sound_
PROTOTYPES: ENABLE

const char *
pidgin_sound_get_event_option(event)
	Purple::SoundEventID event

const char *
pidgin_sound_get_event_label(event)
	Purple::SoundEventID event

Purple::Handle
pidgin_sound_get_handle()
