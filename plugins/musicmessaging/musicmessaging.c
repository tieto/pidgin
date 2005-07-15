#include "internal.h"
#include "gtkgaim.h"

#include "gtkconv.h"
#include "gtkplugin.h"
#include "gtkutils.h"

#include "notify.h"
#include "version.h"

#define MUSICMESSAGIN_PLUGIN_ID "gtk-hazure-musicmessaging"

static gboolean
plugin_load(GaimPlugin *plugin) {
    gaim_notify_message(plugin, GAIM_NOTIFY_MSG_INFO, "Welcome",
                        "Welcome to music messaging.", NULL, NULL, NULL);

    return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin) {
	gaim_notify_message(plugin, GAIM_NOTIFY_MSG_INFO, "Unloaded",
						"The MM plugin has been unloaded.", NULL, NULL, NULL);
	return TRUE;
}

static gboolean
plugin_destroyed(GaimPlugin *plugin) {
	gaim_notify_message(plugin, GAIM_NOTIFY_MSG_INFO, "Destroyed",
						"The MM plugin has been destroyed.", NULL, NULL, NULL);
	return TRUE;
}

static GtkWidget *
get_config_frame(GaimPlugin *plugin)
{
	GtkWidget *ret;
	GtkWidget *vbox;
	
	/* Outside container */
	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width(GTK_CONTAINER(ret), 10);

	/* Configuration frame */
	vbox = gaim_gtk_make_frame(ret, _("Music Messaging Configuration"));
	
	gtk_widget_show_all(ret);

	return ret;
}

static GaimGtkPluginUiInfo ui_info =
{
	get_config_frame
};

static GaimPluginInfo info = {
    GAIM_PLUGIN_MAGIC,
    GAIM_MAJOR_VERSION,
    GAIM_MINOR_VERSION,
    GAIM_PLUGIN_STANDARD,
    GAIM_GTK_PLUGIN_TYPE,
    0,
    NULL,
    GAIM_PRIORITY_DEFAULT,

    MUSICMESSAGIN_PLUGIN_ID,
    "Music Messaging",
    VERSION,
    "Music Messaging Plugin for collabrative composition.",
    "The Music Messaging Plugin allows a number of users to simultaniously work on a piece of music by editting a common score in real-time.",
    "Christian Muise <christian.muise@gmail.com>",
    GAIM_WEBSITE,
    plugin_load,
    plugin_unload,
    plugin_destroyed,
    &ui_info,
    NULL,
    NULL,
    NULL
};

static void
init_plugin(GaimPlugin *plugin) {
}

GAIM_INIT_PLUGIN(musicmessaging, init_plugin, info);
