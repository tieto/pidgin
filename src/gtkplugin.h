/**
 * @file gtkplugin.h GTK+ Plugin API
 * @ingroup gtkui
 *
 * gaim
 *
 * Copyright (C) 2002-2003, Christian Hammond <chipx86@gnupdate.org>
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
#ifndef _GAIM_GTK_PLUGIN_H_
#define _GAIM_GTK_PLUGIN_H_

#include <gtk/gtk.h>
#include "plugin.h"

typedef struct _GaimGtkPluginUiInfo GaimGtkPluginUiInfo;

/**
 * A GTK+ UI structure for plugins.
 */
struct _GaimGtkPluginUiInfo
{
	GtkWidget *(*get_config_frame)(GaimPlugin *plugin);

	void *iter;                                           /**< Reserved */
};

#define GAIM_GTK_PLUGIN_TYPE "gtk"

#define GAIM_IS_GTK_PLUGIN(plugin) \
	((plugin)->info != NULL && (plugin)->info->ui_info != NULL && \
	 !strcmp((plugin)->info->ui_requirement, GAIM_GTK_PLUGIN_TYPE))

#define GAIM_GTK_PLUGIN_UI_INFO(plugin) \
	((GaimGtkPluginUiInfo *)(plugin)->info->ui_info)

/**
 * Returns the configuration frame widget for a GTK+ plugin, if one
 * exists.
 *
 * @param plugin The plugin.
 *
 * @return The frame, if the plugin is a GTK+ plugin and provides a
 *         configuration frame.
 */
GtkWidget *gaim_gtk_plugin_get_config_frame(GaimPlugin *plugin);

/**
 * Saves all loaded plugins.
 */
void gaim_gtk_plugins_save(void);

#endif /* _GAIM_GTK_PLUGIN_H_ */
