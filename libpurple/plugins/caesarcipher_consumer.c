/*
 * An example plugin that demonstrates usage of a cipher object type
 * registered in another plugin.
 *
 * This plugin requires the caesarcipher plugin to be loaded to use the
 * CaesarCipher type.
 *
 * Copyright (C) 2013, Ankit Vani <a@nevitus.org>
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
 */

#include "internal.h"
#include "caesarcipher.h"

static PurplePluginInfo *
plugin_query(GError **error)
{
	const gchar * const authors[] = {
		"Ankit Vani <a@nevitus.org>",
		NULL
	};

	/* we need to ensure the object provider is loaded for its type to be
	 * registered in the type system. */
	const gchar * const dependencies[] = {
		"core-cipher_provider",
		NULL
	};

	return purple_plugin_info_new(
		"id",            "core-caesarcipher_consumer",
		"name",          N_("Caesar Cipher Consumer Example"),
		"version",       DISPLAY_VERSION,
		"category",      N_("Example"),
		"summary",       N_("An example plugin that demonstrates usage of a "
		                    "loaded cipher type."),
		"description",   N_("An example plugin that demonstrates usage of "
		                    "a cipher object type registered in another "
		                    "plugin."),
		"authors",       authors,
		"website",       PURPLE_WEBSITE,
		"abi-version",   PURPLE_ABI_VERSION,
		"dependencies",  dependencies,

		NULL
	);
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	return TRUE;
}

PURPLE_PLUGIN_INIT(caesarcipher_consumer, plugin_query, plugin_load, plugin_unload);
