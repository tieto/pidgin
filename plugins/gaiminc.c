#include <gtk/gtk.h>
#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include "gaim.h"

void gaim_plugin_init() {
	show_about(NULL, NULL);
	play_sound(RECEIVE);
}
