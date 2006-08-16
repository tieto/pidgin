#include <gnt.h>

#include <plugin.h>

#include <string.h>

typedef GntWidget* (*GGPluginFrame) ();

/* Guess where these came from */
#define GAIM_GNT_PLUGIN_TYPE "gnt"

#define GAIM_IS_GNT_PLUGIN(plugin) \
	((plugin)->info != NULL && (plugin)->info->ui_info != NULL && \
	 !strcmp((plugin)->info->ui_requirement, GAIM_GNT_PLUGIN_TYPE))

#define GAIM_GNT_PLUGIN_UI_INFO(plugin) \
	(GGPluginFrame)((plugin)->info->ui_info)

void gg_plugins_show_all();

void gg_plugins_save_loaded();

