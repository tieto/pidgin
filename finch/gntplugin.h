/**
 * @file gntplugin.h GNT Plugins API
 * @ingroup finch
 */

/* finch
 *
 * Finch is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#ifndef _GNT_PLUGIN_H
#define _GNT_PLUGIN_H

#include <gnt.h>

#include <plugin.h>
#include <pluginpref.h>

#include <string.h>

#include "finch.h"

/**********************************************************************
 * @name GNT Plugins API
 **********************************************************************/
/*@{*/

typedef GntWidget* (*FinchPluginFrame) (void);

/* Guess where these came from */
#define FINCH_PLUGIN_TYPE FINCH_UI

/**
 * Decide whether a plugin is a GNT-plugin.
 */
#define PURPLE_IS_GNT_PLUGIN(plugin) \
	((plugin)->info != NULL && (plugin)->info->ui_info != NULL && \
	 !strcmp((plugin)->info->ui_requirement, FINCH_PLUGIN_TYPE))

/**
 * Get the ui-info from GNT-plugins.
 */
#define FINCH_PLUGIN_UI_INFO(plugin) \
	(FinchPluginFrame)((plugin)->info->ui_info)

/**
 * Show a list of plugins.
 */
void finch_plugins_show_all(void);

/**
 * Save the list of loaded plugins.
 */
void finch_plugins_save_loaded(void);

/*@}*/

#endif
