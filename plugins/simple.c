#define GAIM_PLUGINS

#include <stdio.h>
#include "gaim.h"

static void *handle = NULL;

void gaim_plugin_init(void *h) {
	printf("plugin loaded.\n");
	handle = h;
}

void gaim_plugin_remove() {
	printf("plugin unloaded.\n");
	handle = NULL;
}

char *name() {
	return "Simple Plugin Version 1.0";
}

char *description() {
	return "Tests to see that most things are working.";
}
