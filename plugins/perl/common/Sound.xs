
#include "module.h"

/* TODO


*/

MODULE = Gaim::Sound  PACKAGE = Gaim::Sound  PREFIX = gaim_sound_
PROTOTYPES: ENABLE

Gaim::Sound::UiOps
gaim_sound_get_ui_ops()
 

void 
gaim_sound_init()
 

void 
gaim_sound_play_event(event)
	Gaim::SoundEventID event

void 
gaim_sound_play_file(filename)
	const char *filename

void 
gaim_sound_set_ui_ops(ops)
	Gaim::Sound::UiOps ops

void 
gaim_sound_uninit()
 

