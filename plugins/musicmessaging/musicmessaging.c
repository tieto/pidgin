#define GAIM_PLUGINS

#include <glib.h>

#include "notify.h"
#include "plugin.h"
#include "version.h"

static gboolean
plugin_load(GaimPlugin *plugin) {
    gaim_notify_message(plugin, GAIM_NOTIFY_MSG_INFO, "Hello World!",
                        "This is the Hello World! plugin :)", NULL, NULL, NULL);

    return TRUE;
}

static GaimPluginInfo info = {
    GAIM_PLUGIN_MAGIC,
    GAIM_MAJOR_VERSION,
    GAIM_MINOR_VERSION,
    GAIM_PLUGIN_STANDARD,
    NULL,
    0,
    NULL,
    GAIM_PRIORITY_DEFAULT,

    "core-hello_world",
    "Hello World!",
    NULL,

    "Hello World Plugin",
    "Hello World Plugin",
    NULL,
    NULL,

    plugin_load,
    NULL,
    NULL,

    NULL,
    NULL,
    NULL,
    NULL
};

static void
init_plugin(GaimPlugin *plugin) {
}

GAIM_INIT_PLUGIN(hello_world, init_plugin, info);
