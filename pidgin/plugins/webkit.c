/*
 * WebKit - Open the inspector on any WebKit views.
 * Copyright (C) 2011 Elliott Sales de Andrade <qulogic@pidgin.im>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301, USA.
 */

#include "internal.h"

#include "version.h"

#include "gtkplugin.h"

static gboolean
plugin_load(PurplePlugin *plugin)
{
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/webview");
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/webview/inspector_enabled", FALSE);

	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/webview/inspector_enabled", TRUE);

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/webview/inspector_enabled", FALSE);

	return TRUE;
}

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,                           /**< major version */
	PURPLE_MINOR_VERSION,                           /**< minor version */
	PURPLE_PLUGIN_STANDARD,                         /**< type */
	PIDGIN_PLUGIN_TYPE,                             /**< ui_requirement */
	0,                                              /**< flags */
	NULL,                                           /**< dependencies */
	PURPLE_PRIORITY_DEFAULT,                        /**< priority */

	"gtkwebkit-inspect",                            /**< id */
	N_("WebKit Development"),                       /**< name */
	DISPLAY_VERSION,                                /**< version */
	N_("Enables WebKit Inspector."),                /**< summary */
	N_("Enables WebKit's built-in inspector. This "
	   "may be viewed by right-clicking a WebKit "
	   "widget and selecting 'Inspect Element'."),  /**< description */
	"Elliott Sales de Andrade <qulogic@pidgin.im>", /**< author */
	PURPLE_WEBSITE,                                 /**< homepage */
	plugin_load,                                    /**< load */
	plugin_unload,                                  /**< unload */
	NULL,                                           /**< destroy */
	NULL,                                           /**< ui_info */
	NULL,                                           /**< extra_info */
	NULL,                                           /**< prefs_info */
	NULL,                                           /**< actions */

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
}

PURPLE_INIT_PLUGIN(webkit-devel, init_plugin, info)
