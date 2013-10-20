/*
 * Debug Example Plugin
 *
 * Copyright (C) 2007, John Bailey <rekkanoryo@cpw.pidgin.im>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02111-1301, USA.
 *
 */

#include "internal.h"

/* This file defines PURPLE_PLUGINS and includes all the libpurple headers */
#include <purple.h>

/* It's more convenient to type PLUGIN_ID all the time than it is to type
 * "core-debugexample", so define this convenience macro. */
#define PLUGIN_ID "core-debugexample"

/* Common practice in third-party plugins is to define convenience macros for
 * many of the fields of the plugin info struct, so we'll do that for the
 * purposes of demonstration. */
#define PLUGIN_AUTHORS { "John Bailey <rekkanoryo@cpw.pidgin.im>", NULL }

static PurplePluginInfo *
plugin_query(GError **error)
{
	const gchar * const authors[] = PLUGIN_AUTHORS;

	return purple_plugin_info_new(
		"id",           PLUGIN_ID,
		"name",         "Debug API Example",
		"version",      DISPLAY_VERSION,
		"category",     "Example",
		"summary",      "Debug API Example",
		"description",  "Debug API Example",
		"authors",      authors,
		"website",      "https://pidgin.im",
		"abi-version",  PURPLE_ABI_VERSION,
		NULL
	);
}

/* As we've covered before, this function is called when the plugin is loaded.
 * Here we're using it to show off the capabilities of the debug API and just
 * blindly returning TRUE to tell libpurple it's safe to continue loading. */
static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	/* Define these for convenience--we're just using them to show the
	 * similarities of the debug functions to the standard printf(). */
	gint i = 256;
	gfloat f = 512.1024;
	const gchar *s = "example string";

	/* Introductory message */
	purple_debug_info(PLUGIN_ID,
		"Called plugin_load.  Beginning debug demonstration\n");

	/* Show off the debug API a bit */
	purple_debug_misc(PLUGIN_ID,
		"MISC level debug message.  i = %d, f = %f, s = %s\n", i, f, s);

	purple_debug_info(PLUGIN_ID,
		"INFO level debug message.  i = %d, f = %f, s = %s\n", i, f, s);

	purple_debug_warning(PLUGIN_ID,
		"WARNING level debug message.  i = %d, f = %f, s = %s\n", i, f, s);

	purple_debug_error(PLUGIN_ID,
		"ERROR level debug message.  i = %d, f = %f, s = %s\n", i, f, s);

	purple_debug_fatal(PLUGIN_ID,
		"FATAL level debug message. i = %d, f = %f, s = %s\n", i, f, s);

	/* Now just return TRUE to tell libpurple to finish loading. */
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	return TRUE;
}

PURPLE_PLUGIN_INIT(debugexample, plugin_query, plugin_load, plugin_unload);
