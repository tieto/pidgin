#define GAIM_PLUGINS
#include "gaim.h"

#include <stdlib.h>
#include <time.h>

int gaim_plugin_init(void *handle) {
	int error;

	/* so here, we load any callbacks, do the normal stuff */

	srand(time(NULL));
	error = rand() % 3;
	/* there's a 1 in 3 chance there *won't* be an error :) */
	return error;
}

void gaim_plugin_remove() {
	/* this only gets called if we get loaded successfully, and then
	 * unloaded. */
}

char *gaim_plugin_error(int error) {
	/* by the time we've gotten here, all our callbacks are removed.
	 * we just have to deal with what the error was (as defined by us)
	 * and do any other clean-up stuff we need to do. */
	switch (error) {
	case 0:
		do_error_dialog("I'm calling the error myself", "MY BAD");
		return NULL;
	case 2:
		return "Internal plugin error: exiting.";
	}
	/* we should never get here */
	return NULL;
}

char *name() {
	return "Error Tester " VERSION ;
}

char *description() {
	return "A nice little program that causes error messages";
}
