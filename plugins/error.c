#define GAIM_PLUGINS
#include "gaim.h"

#include <stdlib.h>
#include <time.h>

char *gaim_plugin_error(int);

char *gaim_plugin_init(GModule *handle) {
	int error;

	/* so here, we load any callbacks, do the normal stuff */

	srand(time(NULL));
	error = rand() % 3;
	error -= 2;
	/* there's a 1 in 3 chance there *won't* be an error :) */
	return gaim_plugin_error(error);
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
	case -1:
		return "MY BAD";
	case -2:
		return "Internal plugin error: exiting.";
	default:
		return NULL;
	}
}

struct gaim_plugin_description desc; 
struct gaim_plugin_description *gaim_plugin_desc() {
	desc.api_version = PLUGIN_API_VERSION;
	desc.name = g_strdup("Error Tester");
	desc.version = g_strdup(VERSION);
	desc.description = g_strdup("A plugin that causes error messages.");
	desc.authors = g_strdup("Eric Warmehoven &lt;eric@warmenhoven.org>");
	desc.url = g_strdup(WEBSITE);
	return &desc;
}

char *name() {
	return "Error Tester " VERSION ;
}

char *description() {
	return "A nice little program that causes error messages";
}
