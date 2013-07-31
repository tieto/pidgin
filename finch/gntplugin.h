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

#include <plugins.h>
#include <pluginpref.h>

#include <string.h>

#include "finch.h"

#define FINCH_TYPE_PLUGIN_INFO             (finch_plugin_info_get_type())
#define FINCH_PLUGIN_INFO(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), FINCH_TYPE_PLUGIN_INFO, FinchPluginInfo))
#define FINCH_PLUGIN_INFO_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), FINCH_TYPE_PLUGIN_INFO, FinchPluginInfoClass))
#define FINCH_IS_PLUGIN_INFO(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), FINCH_TYPE_PLUGIN_INFO))
#define FINCH_IS_PLUGIN_INFO_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), FINCH_TYPE_PLUGIN_INFO))
#define FINCH_PLUGIN_INFO_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), FINCH_TYPE_PLUGIN_INFO, FinchPluginInfoClass))

/** @copydoc _FinchPluginInfo */
typedef struct _FinchPluginInfo FinchPluginInfo;
/** @copydoc _FinchPluginInfoClass */
typedef struct _FinchPluginInfoClass FinchPluginInfoClass;

typedef GntWidget* (*FinchPluginFrame) (void);

/**
 * Extends #PurplePluginInfo to hold UI information for finch.
 */
struct _FinchPluginInfo {
	/*< private >*/
	PurplePluginInfo parent;
};

/**
 * FinchPluginInfoClass:
 *
 * The base class for all #FinchPluginInfo's.
 */
struct _FinchPluginInfoClass {
	/*< private >*/
	PurplePluginInfoClass parent_class;

	void (*_gnt_reserved1)(void);
	void (*_gnt_reserved2)(void);
	void (*_gnt_reserved3)(void);
	void (*_gnt_reserved4)(void);
};

/**********************************************************************
 * @name Plugin Info API
 **********************************************************************/
/*@{*/

/**
 * Returns the GType for the FinchPluginInfo object.
 */
GType finch_plugin_info_get_type(void);

/*@}*/

/**********************************************************************
 * @name GNT Plugins API
 **********************************************************************/
/*@{*/

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
