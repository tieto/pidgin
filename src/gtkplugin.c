/**
 * @file plugins.h Conversation API
 * @ingroup core
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
#include "gtkplugin.h"
#include "debug.h"
#include "prefs.h"

#include <string.h>

GtkWidget *
gaim_gtk_plugin_get_config_frame(GaimPlugin *plugin)
{
	GaimGtkPluginUiInfo *ui_info;

	g_return_val_if_fail(plugin != NULL, NULL);
	g_return_val_if_fail(GAIM_IS_GTK_PLUGIN(plugin), NULL);

	if (plugin->info->ui_info == NULL)
		return NULL;

	ui_info = GAIM_GTK_PLUGIN_UI_INFO(plugin);

	if (ui_info->get_config_frame == NULL)
		return NULL;

	return ui_info->get_config_frame(plugin);
}

void
gaim_gtk_plugins_save(void)
{
	GList *pl;
	GList *files = NULL;
	GaimPlugin *p;

	for (pl = gaim_plugins_get_loaded(); pl != NULL; pl = pl->next) {
		p = pl->data;

		if (p->info->type != GAIM_PLUGIN_PROTOCOL &&
			p->info->type != GAIM_PLUGIN_LOADER) {

			files = g_list_append(files, p->path);

			gaim_debug(GAIM_DEBUG_INFO, "gtkplugin",
					   "Adding %s to save list.\n", p->path);
		}
	}

	gaim_prefs_set_string_list("/gaim/gtk/plugins/loaded", files);
	g_list_free(files);
}
