/**
 * @file ssl.c Main SSL plugin
 *
 * purple
 *
 * Copyright (C) 2003 Christian Hammond <chipx86@gnupdate.org>
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
#include "internal.h"
#include "debug.h"
#include "plugins.h"
#include "sslconn.h"
#include "version.h"

#define SSL_PLUGIN_ID      "core-ssl"
#define SSL_PLUGIN_DOMAIN  (g_quark_from_static_string(SSL_PLUGIN_ID))

static PurplePlugin *ssl_plugin = NULL;

static gboolean
probe_ssl_plugins(PurplePlugin *my_plugin, GError **error)
{
	PurplePlugin *plugin;
	GList *plugins, *l;

	ssl_plugin = NULL;

	plugins = purple_plugins_find_all();

	for (l = plugins; l != NULL; l = l->next)
	{
		plugin = PURPLE_PLUGIN(l->data);

		if (plugin == my_plugin)
			continue;

		if (strncmp(purple_plugin_info_get_id(purple_plugin_get_info(plugin)),
				"ssl-", 4) == 0)
		{
			if (purple_plugin_is_loaded(plugin) || purple_plugin_load(plugin))
			{
				ssl_plugin = plugin;

				break;
			}
		}
	}

	g_list_free(plugins);

	if (ssl_plugin == NULL) {
		g_set_error(error, SSL_PLUGIN_DOMAIN, 0,
				"Could not find a plugin that implements SSL.");
		return FALSE;
	} else {
		return TRUE;
	}
}

static PurplePluginInfo *
plugin_query(GError **error)
{
	return purple_plugin_info_new(
		"id",           SSL_PLUGIN_ID,
		"name",         N_("SSL"),
		"version",      DISPLAY_VERSION,
		"category",     N_("SSL"),
		"summary",      N_("Provides a wrapper around SSL support libraries."),
		"description",  N_("Provides a wrapper around SSL support libraries."),
		"author",       "Christian Hammond <chipx86@gnupdate.org>",
		"website",      PURPLE_WEBSITE,
		"abi-version",  PURPLE_ABI_VERSION,
		"flags",        GPLUGIN_PLUGIN_INFO_FLAGS_INTERNAL,
		NULL
	);
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	return probe_ssl_plugins(plugin, error);
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	if (ssl_plugin != NULL &&
		g_list_find(purple_plugins_get_loaded(), ssl_plugin) != NULL)
	{
		purple_plugin_unload(ssl_plugin);
	}

	ssl_plugin = NULL;

	return TRUE;
}

PURPLE_PLUGIN_INIT(ssl, plugin_query, plugin_load, plugin_unload);
