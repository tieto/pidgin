/*
 * PluginPref Example Plugin
 *
 * Copyright (C) 2004, Gary Kramlich <amc_grim@users.sf.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifndef GAIM_PLUGINS
# define GAIM_PLUGINS
#endif

#include "internal.h"

#include "plugin.h"
#include "pluginpref.h"
#include "prefs.h"
#include "version.h"

static GaimPluginPrefFrame *
get_plugin_pref_frame(GaimPlugin *plugin) {
	GaimPluginPrefFrame *frame;
	GaimPluginPref *ppref;

	frame = gaim_plugin_pref_frame_new();

	ppref = gaim_plugin_pref_new_with_label("boolean");
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_name_and_label(
									"/plugins/core/pluginpref_example/bool",
									"boolean pref");
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_label("integer");
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_name_and_label(
									"/plugins/core/pluginpref_example/int",
									"integer pref");
	gaim_plugin_pref_set_bounds(ppref, 0, 255);
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_name_and_label(
									"/plugins/core/pluginpref_example/int_choice",
									"integer choice");
	gaim_plugin_pref_set_type(ppref, GAIM_PLUGIN_PREF_CHOICE);
	gaim_plugin_pref_add_choice(ppref, "One", GINT_TO_POINTER(1));
	gaim_plugin_pref_add_choice(ppref, "Two", GINT_TO_POINTER(2));
	gaim_plugin_pref_add_choice(ppref, "Four", GINT_TO_POINTER(4));
	gaim_plugin_pref_add_choice(ppref, "Eight", GINT_TO_POINTER(8));
	gaim_plugin_pref_add_choice(ppref, "Sixteen", GINT_TO_POINTER(16));
	gaim_plugin_pref_add_choice(ppref, "Thirty Two", GINT_TO_POINTER(32));
	gaim_plugin_pref_add_choice(ppref, "Sixty Four", GINT_TO_POINTER(64));
	gaim_plugin_pref_add_choice(ppref, "One Hundred Twenty Eight", GINT_TO_POINTER(128));
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_label("string");
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_name_and_label(
								"/plugins/core/pluginpref_example/string",
								"string pref");
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_name_and_label(
								"/plugins/core/pluginpref_example/masked_string",
								"masked string");
	gaim_plugin_pref_set_masked(ppref, TRUE);
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_name_and_label(
							"/plugins/core/pluginpref_example/max_string",
							"string pref\n(max length of 16)");
	gaim_plugin_pref_set_max_length(ppref, 16);
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_name_and_label(
							"/plugins/core/pluginpref_example/string_choice",
							"string choice");
	gaim_plugin_pref_set_type(ppref, GAIM_PLUGIN_PREF_CHOICE);
	gaim_plugin_pref_add_choice(ppref, "red", "red");
	gaim_plugin_pref_add_choice(ppref, "orange", "orange");
	gaim_plugin_pref_add_choice(ppref, "yellow", "yellow");
	gaim_plugin_pref_add_choice(ppref, "green", "green");
	gaim_plugin_pref_add_choice(ppref, "blue", "blue");
	gaim_plugin_pref_add_choice(ppref, "purple", "purple");
	gaim_plugin_pref_frame_add(frame, ppref);

	return frame;
}

static GaimPluginUiInfo prefs_info = {
	get_plugin_pref_frame,
	0,   /* page_num (Reserved) */
	NULL /* frame (Reserved) */
};

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	"core-pluginpref_example",                     /**< id             */
	"Pluginpref Example",                           /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	"An example of how to use pluginprefs",
	                                                  /**  description    */
	"An example of how to use pluginprefs",
	"Gary Kramlich <amc_grim@users.sf.net>",      /**< author         */
	GAIM_WEBSITE,                                     /**< homepage       */

	NULL,                                             /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL,                                             /**< extra_info     */
	&prefs_info,                                      /**< prefs_info     */
	NULL
};

static void
init_plugin(GaimPlugin *plugin)
{
	gaim_prefs_add_none("/plugins/core/pluginpref_example");
	gaim_prefs_add_bool("/plugins/core/pluginpref_example/bool", TRUE);
	gaim_prefs_add_int("/plugins/core/pluginpref_example/int", 0);
	gaim_prefs_add_int("/plugins/core/pluginpref_example/int_choice", 1);
	gaim_prefs_add_string("/plugins/core/pluginpref_example/string",
							"string");
	gaim_prefs_add_string("/plugins/core/pluginpref_example/max_string",
							"max length string");
	gaim_prefs_add_string("/plugins/core/pluginpref_example/masked_string", "masked");
	gaim_prefs_add_string("/plugins/core/pluginpref_example/string_choice", "red");
}

GAIM_INIT_PLUGIN(ppexample, init_plugin, info)
