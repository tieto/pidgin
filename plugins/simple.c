#define GAIM_PLUGINS

#include <stdio.h>
#include "gaim.h"

static GModule *handle = NULL;

char *gaim_plugin_init(GModule *h) {
	printf("plugin loaded.\n");
	handle = h;
	return NULL;
}

void gaim_plugin_remove() {
	printf("plugin unloaded.\n");
	handle = NULL;
}

void gaim_plugin_config() {
	printf("configuring plugin.\n");
}

struct gaim_plugin_description desc; 
struct gaim_plugin_description *gaim_plugin_desc() {
	desc.api_version = PLUGIN_API_VERSION;
	desc.name = g_strdup("Simple Plugin");
	desc.version = g_strdup("1.0");
	desc.description = g_strdup("Tests to see that most things are working.");
	desc.authors = g_strdup("Eric Warmehoven &lt;eric@warmenhoven.org>");
	desc.url = g_strdup(WEBSITE);
	return &desc;
}

char *name() {
	return "Simple Plugin Version 1.0";
}

char *description() {
	return "Tests to see that most things are working.";
}
