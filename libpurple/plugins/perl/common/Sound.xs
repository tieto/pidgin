#include "module.h"

MODULE = Purple::Sound  PACKAGE = Purple::Sound  PREFIX = purple_sound_
PROTOTYPES: ENABLE

Purple::Sound::UiOps
purple_sound_get_ui_ops()

void
purple_sound_init()

void
purple_sound_play_event(event, account)
	Purple::SoundEventID event
	Purple::Account account

void
purple_sound_play_file(filename, account)
	const char *filename
	Purple::Account account

void
purple_sound_set_ui_ops(ops)
	Purple::Sound::UiOps ops

void
purple_sound_uninit()
