/**
 * @file gtkplugin.h GTK+ Plugin API
 * @ingroup gtkui
 *
 * purple
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _PIDGINPLUGIN_H_
#define _PIDGINPLUGIN_H_

#include "pidgin.h"
#include "plugin.h"

typedef struct _PidginPluginUiInfo PidginPluginUiInfo;

/**
 * A GTK+ UI structure for plugins.
 */
struct _PidginPluginUiInfo
{
	GtkWidget *(*get_config_frame)(PurplePlugin *plugin);

	int page_num;                                         /**< Reserved */
};

#define PIDGIN_PLUGIN_TYPE PIDGIN_UI

#define PIDGIN_IS_PIDGIN_PLUGIN(plugin) \
	((plugin)->info != NULL && (plugin)->info->ui_info != NULL && \
	 !strcmp((plugin)->info->ui_requirement, PIDGIN_PLUGIN_TYPE))

#define PIDGIN_PLUGIN_UI_INFO(plugin) \
	((PidginPluginUiInfo *)(plugin)->info->ui_info)

/**
 * Returns the configuration frame widget for a GTK+ plugin, if one
 * exists.
 *
 * @param plugin The plugin.
 *
 * @return The frame, if the plugin is a GTK+ plugin and provides a
 *         configuration frame.
 */
GtkWidget *pidgin_plugin_get_config_frame(PurplePlugin *plugin);

/**
 * Saves all loaded plugins.
 */
void pidgin_plugins_save(void);

/**
 * Shows the Plugins dialog
 */
void pidgin_plugin_dialog_show(void);

#endif /* _PIDGINPLUGIN_H_ */
