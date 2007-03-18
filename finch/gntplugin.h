/**
 * @file gntplugin.h GNT Plugins API
 * @ingroup gntui
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
#ifndef _GNT_PLUGIN_H
#define _GNT_PLUGIN_H

#include <gnt.h>

#include <plugin.h>

#include <string.h>

#include "gntgaim.h"

/**********************************************************************
 * @name GNT Plugins API
 **********************************************************************/
/*@{*/

typedef GntWidget* (*FinchPluginFrame) ();

/* Guess where these came from */
#define GAIM_GNT_PLUGIN_TYPE GAIM_GNT_UI

/**
 * Decide whether a plugin is a GNT-plugin.
 */
#define GAIM_IS_GNT_PLUGIN(plugin) \
	((plugin)->info != NULL && (plugin)->info->ui_info != NULL && \
	 !strcmp((plugin)->info->ui_requirement, GAIM_GNT_PLUGIN_TYPE))

/**
 * Get the ui-info from GNT-plugins.
 */
#define GAIM_GNT_PLUGIN_UI_INFO(plugin) \
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
