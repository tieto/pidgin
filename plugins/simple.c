#include <stdio.h>

void gaim_plugin_init() {
	printf("plugin loaded.\n");
}

void gaim_plugin_remove() {
	printf("plugin unloaded.\n");
}

char *name() {
	return "Simple plugin";
}

char *description() {
	return "Tests to see that most things are working.";
}
