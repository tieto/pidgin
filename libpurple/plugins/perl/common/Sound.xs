#include "module.h"

MODULE = Gaim::Sound  PACKAGE = Gaim::Sound  PREFIX = gaim_sound_
PROTOTYPES: ENABLE

Gaim::Sound::UiOps
gaim_sound_get_ui_ops()

void
gaim_sound_init()

void
gaim_sound_play_event(event, account)
	Gaim::SoundEventID event
	Gaim::Account account

void
gaim_sound_play_file(filename, account)
	const char *filename
	Gaim::Account account

void
gaim_sound_set_ui_ops(ops)
	Gaim::Sound::UiOps ops

void
gaim_sound_uninit()
