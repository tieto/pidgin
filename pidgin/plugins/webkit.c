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

static PidginPluginInfo *
plugin_query(GError **error)
{
	const gchar * const authors[] = {
		"Elliott Sales de Andrade <qulogic@pidgin.im>",
		NULL
	};

	return pidgin_plugin_info_new(
		"id",           "gtkwebkit-inspect",
		"name",         N_("WebKit Development"),
		"version",      DISPLAY_VERSION,
		"category",     N_("Testing"),
		"summary",      N_("Enables WebKit Inspector."),
		"description",  N_("Enables WebKit's built-in inspector. This may be "
		                   "viewed by right-clicking a WebKit widget and "
		                   "selecting 'Inspect Element'."),
		"authors",      authors,
		"website",      PURPLE_WEBSITE,
		"abi-version",  PURPLE_ABI_VERSION,
		NULL
	);
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/webview");
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/webview/inspector_enabled", FALSE);

	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/webview/inspector_enabled", TRUE);

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/webview/inspector_enabled", FALSE);

	return TRUE;
}

PURPLE_PLUGIN_INIT(webkit-devel, plugin_query, plugin_load, plugin_unload);
