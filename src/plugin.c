/*
 * gaim
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
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

/*
 * ----------------
 * The Plug-in plugin
 *
 * Plugin support is currently being maintained by Mike Saraf
 * msaraf@dwc.edu
 *
 * Well, I didn't see any work done on it for a while, so I'm going to try
 * my hand at it. - Eric warmenhoven@yahoo.com
 *
 * Mike is my roomate.  I can assure you that he's lazy :-P
 * -- Rob rob@marko.net
 *
 * Yeah, well now I'm re-writing a good portion of it! The perl stuff was
 * a hack. Tsk tsk! -- Christian <chipx86@gnupdate.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gaim.h"
#include "accountopt.h"
#include "prpl.h"
#include "event.h"
#include "notify.h"

#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include "win32dep.h"
#endif

#ifdef _WIN32
# define PLUGIN_EXT ".dll"
#else
# define PLUGIN_EXT ".so"
#endif

static GList *loaded_plugins = NULL;
static GList *plugins = NULL;
static GList *plugin_loaders = NULL;
static GList *protocol_plugins = NULL;

static size_t search_path_count = 0;
static char **search_paths = NULL;

static void (*probe_cb)(void *) = NULL;
static void *probe_cb_data = NULL;
static void (*load_cb)(GaimPlugin *, void *) = NULL;
static void *load_cb_data = NULL;
static void (*unload_cb)(GaimPlugin *, void *) = NULL;
static void *unload_cb_data = NULL;

#ifdef GAIM_PLUGINS
static int
is_so_file(const char *filename, const char *ext)
{
	int len, extlen;

	if (filename == NULL || *filename == '\0' || ext == NULL)
		return 0;

	extlen = strlen(ext);
	len = strlen(filename) - extlen;

	if (len < 0)
		return 0;

	return (!strncmp(filename + len, ext, extlen));
}

static gboolean
__loader_supports_file(GaimPlugin *loader, const char *filename)
{
	GList *l, *exts;
	GaimPlugin *plugin;

	for (l = plugin_loaders; l != NULL; l = l->next) {
		plugin = l->data;

		for (exts = GAIM_PLUGIN_LOADER_INFO(plugin)->exts;
			 exts != NULL;
			 exts = exts->next) {

			if (is_so_file(filename, (char *)exts->data))
				return TRUE;
		}
	}

	return FALSE;
}

static GaimPlugin *
__find_loader_for_plugin(const GaimPlugin *plugin)
{
	GaimPlugin *loader;
	GList *l;

	if (plugin->path == NULL)
		return NULL;

	for (l = gaim_plugins_get_loaded(); l != NULL; l = l->next) {
		loader = l->data;

		if (loader->info->type == GAIM_PLUGIN_LOADER &&
			__loader_supports_file(loader, plugin->path)) {

			return loader;
		}

		loader = NULL;
	}

	return NULL;
}

#endif /* GAIM_PLUGINS */

static gint
compare_prpl(GaimPlugin *a, GaimPlugin *b)
{
	/* neg if a before b, 0 if equal, pos if a after b */
	return ((GAIM_IS_PROTOCOL_PLUGIN(a)
			 ? GAIM_PLUGIN_PROTOCOL_INFO(a)->protocol : -1) -
			((GAIM_IS_PROTOCOL_PLUGIN(b)
			 ? GAIM_PLUGIN_PROTOCOL_INFO(b)->protocol : -1)));
}

GaimPlugin *
gaim_plugin_new(gboolean native, const char *path)
{
	GaimPlugin *plugin;

	plugin = g_new0(GaimPlugin, 1);

	plugin->native_plugin = native;
	plugin->path = (path == NULL ? NULL : g_strdup(path));

	return plugin;
}

GaimPlugin *
gaim_plugin_probe(const char *filename)
{
#ifdef GAIM_PLUGINS
	GaimPlugin *plugin = NULL;
	GaimPlugin *loader;
	gboolean (*gaim_init_plugin)(GaimPlugin *);

	g_return_val_if_fail(filename != NULL, NULL);

	plugin = gaim_plugins_find_with_filename(filename);

	if (plugin != NULL)
		return plugin;

	plugin = gaim_plugin_new(is_so_file(filename, PLUGIN_EXT), filename);

	if (plugin->native_plugin) {
		const char *error;
		plugin->handle = g_module_open(filename, 0);

		if (plugin->handle == NULL) {
			error = g_module_error();
			gaim_debug(GAIM_DEBUG_ERROR, "plugins", "%s is unloadable: %s\n",
					   plugin->path, error ? error : "Unknown error.");

			gaim_plugin_destroy(plugin);

			return NULL;
		}

		if (!g_module_symbol(plugin->handle, "gaim_init_plugin",
							 (gpointer *)&gaim_init_plugin)) {
			g_module_close(plugin->handle);
			plugin->handle = NULL;

			error = g_module_error();
			gaim_debug(GAIM_DEBUG_ERROR, "plugins", "%s is unloadable: %s\n",
					   plugin->path, error ? error : "Unknown error.");

			gaim_plugin_destroy(plugin);

			return NULL;
		}
	}
	else {
		loader = __find_loader_for_plugin(plugin);

		if (loader == NULL) {
			gaim_plugin_destroy(plugin);

			return NULL;
		}

		gaim_init_plugin = GAIM_PLUGIN_LOADER_INFO(loader)->probe;
	}

	plugin->error = NULL;

	if (!gaim_init_plugin(plugin) || plugin->info == NULL) {
		char buf[BUFSIZ];

		g_snprintf(buf, sizeof(buf),
				   _("The plugin %s did not return any valid plugin "
					 "information"),
				   plugin->path);

		gaim_notify_error(NULL, NULL,
						  _("Gaim was unable to load your plugin."), buf);

		gaim_plugin_destroy(plugin);

		return NULL;
	}

	return plugin;
#else
	return NULL;
#endif /* !GAIM_PLUGINS */
}

gboolean
gaim_plugin_load(GaimPlugin *plugin)
{
#ifdef GAIM_PLUGINS
	g_return_val_if_fail(plugin != NULL, FALSE);
	g_return_val_if_fail(plugin->error == NULL, FALSE);

	if (gaim_plugin_is_loaded(plugin))
		return TRUE;

	if (plugin->native_plugin) {
		if (plugin->info != NULL) {
			if (plugin->info->load != NULL)
				plugin->info->load(plugin);

			if (plugin->info->type == GAIM_PLUGIN_LOADER) {
				GaimPluginLoaderInfo *loader_info;

				loader_info = GAIM_PLUGIN_LOADER_INFO(plugin);

				if (loader_info->broadcast != NULL)
					gaim_signals_register_broadcast_func(loader_info->broadcast,
														 NULL);
			}
		}
	}
	else {
		GaimPlugin *loader;
		GaimPluginLoaderInfo *loader_info;

		loader = __find_loader_for_plugin(plugin);

		if (loader == NULL)
			return FALSE;

		loader_info = GAIM_PLUGIN_LOADER_INFO(loader);

		if (loader_info->load != NULL)
			loader_info->load(plugin);
	}

	loaded_plugins = g_list_append(loaded_plugins, plugin);

	plugin->loaded = TRUE;

	/* TODO */
	if (load_cb != NULL)
		load_cb(plugin, load_cb_data);

	return TRUE;

#else
	return TRUE;
#endif /* !GAIM_PLUGINS */
}

gboolean
gaim_plugin_unload(GaimPlugin *plugin)
{
#ifdef GAIM_PLUGINS
	g_return_val_if_fail(plugin != NULL, FALSE);

	loaded_plugins = g_list_remove(loaded_plugins, plugin);

	g_return_val_if_fail(gaim_plugin_is_loaded(plugin), FALSE);

	gaim_debug(GAIM_DEBUG_INFO, "plugins", "Unloading plugin %s\n",
			   plugin->info->name);

	/* cancel any pending dialogs the plugin has */
	gaim_request_close_with_handle(plugin);
	gaim_notify_close_with_handle(plugin);

	plugin->loaded = FALSE;

	if (plugin->native_plugin) {
		if (plugin->info->unload != NULL)
			plugin->info->unload(plugin);

		if (plugin->info->type == GAIM_PLUGIN_PROTOCOL) {
			GaimPluginProtocolInfo *prpl_info;
			GList *l;

			prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(plugin);

			for (l = prpl_info->user_splits; l != NULL; l = l->next)
				gaim_account_user_split_destroy(l->data);

			for (l = prpl_info->protocol_options; l != NULL; l = l->next)
				gaim_account_option_destroy(l->data);

			g_list_free(prpl_info->user_splits);
			g_list_free(prpl_info->protocol_options);
		}
		else if (plugin->info->type == GAIM_PLUGIN_LOADER) {
			GaimPluginLoaderInfo *loader_info;

			loader_info = GAIM_PLUGIN_LOADER_INFO(plugin);

			if (loader_info->broadcast != NULL)
				gaim_signals_unregister_broadcast_func(loader_info->broadcast);
		}
	}
	else {
		GaimPlugin *loader;
		GaimPluginLoaderInfo *loader_info;

		loader = __find_loader_for_plugin(plugin);

		if (loader == NULL)
			return FALSE;

		loader_info = GAIM_PLUGIN_LOADER_INFO(loader);

		if (loader_info->load != NULL)
			loader_info->unload(plugin);
	}

	gaim_signals_disconnect_by_handle(plugin);

	/* TODO */
	if (unload_cb != NULL)
		unload_cb(plugin, unload_cb_data);

	return TRUE;
#else
	return TRUE;
#endif /* GAIM_PLUGINS */
}

gboolean
gaim_plugin_reload(GaimPlugin *plugin)
{
#ifdef GAIM_PLUGINS
	g_return_val_if_fail(plugin != NULL, FALSE);
	g_return_val_if_fail(gaim_plugin_is_loaded(plugin), FALSE);

	if (!gaim_plugin_unload(plugin))
		return FALSE;

	if (!gaim_plugin_load(plugin))
		return FALSE;

	return TRUE;
#else
	return TRUE;
#endif /* !GAIM_PLUGINS */
}

void
gaim_plugin_destroy(GaimPlugin *plugin)
{
#ifdef GAIM_PLUGINS
	g_return_if_fail(plugin != NULL);

	if (gaim_plugin_is_loaded(plugin))
		gaim_plugin_unload(plugin);

	plugins = g_list_remove(plugins, plugin);

	if (plugin->info != NULL && plugin->info->dependencies != NULL)
		g_list_free(plugin->info->dependencies);

	if (plugin->native_plugin) {

		if (plugin->info != NULL && plugin->info->type == GAIM_PLUGIN_LOADER) {
			GList *exts, *l, *next_l;
			GaimPlugin *p2;

			for (exts = GAIM_PLUGIN_LOADER_INFO(plugin)->exts;
				 exts != NULL;
				 exts = exts->next) {

				for (l = gaim_plugins_get_all(); l != NULL; l = next_l) {
					next_l = l->next;

					p2 = l->data;

					if (p2->path != NULL && is_so_file(p2->path, exts->data))
						gaim_plugin_destroy(p2);
				}
			}

			g_list_free(GAIM_PLUGIN_LOADER_INFO(plugin)->exts);

			plugin_loaders = g_list_remove(plugin_loaders, plugin);
		}

		if (plugin->info != NULL && plugin->info->destroy != NULL)
			plugin->info->destroy(plugin);

		if (plugin->handle != NULL)
			g_module_close(plugin->handle);
	}
	else {
		GaimPlugin *loader;
		GaimPluginLoaderInfo *loader_info;

		loader = __find_loader_for_plugin(plugin);

		if (loader == NULL)
			return;

		loader_info = GAIM_PLUGIN_LOADER_INFO(loader);

		if (loader_info->destroy != NULL)
			loader_info->destroy(plugin);
	}

	if (plugin->path  != NULL) g_free(plugin->path);
	if (plugin->error != NULL) g_free(plugin->error);

	g_free(plugin);
#endif /* !GAIM_PLUGINS */
}

gboolean
gaim_plugin_is_loaded(const GaimPlugin *plugin)
{
	g_return_val_if_fail(plugin != NULL, FALSE);

	return plugin->loaded;
}

void
gaim_plugins_set_search_paths(size_t count, char **paths)
{
	size_t s;

	g_return_if_fail(count > 0);
	g_return_if_fail(paths != NULL);

	if (search_paths != NULL) {
		for (s = 0; s < search_path_count; s++)
			g_free(search_paths[s]);

		g_free(search_paths);
	}

	search_paths = g_new0(char *, count);

	for (s = 0; s < count; s++) {
		if (paths[s] == NULL)
			search_paths[s] = NULL;
		else
			search_paths[s] = g_strdup(paths[s]);
	}

	search_path_count = count;
}

void
gaim_plugins_unload_all(void)
{
#ifdef GAIM_PLUGINS

	while (loaded_plugins != NULL)
		gaim_plugin_unload(loaded_plugins->data);

#endif /* GAIM_PLUGINS */
}

void
gaim_plugins_destroy_all(void)
{
#ifdef GAIM_PLUGINS

	while (plugins != NULL)
		gaim_plugin_destroy(plugins->data);

#endif /* GAIM_PLUGINS */
}
void
gaim_plugins_probe(const char *ext)
{
#ifdef GAIM_PLUGINS
	GDir *dir;
	const gchar *file;
	gchar *path;
	GaimPlugin *plugin;
	size_t i;

	if (!g_module_supported())
		return;

	for (i = 0; i < search_path_count; i++) {
		if (search_paths[i] == NULL)
			continue;

		dir = g_dir_open(search_paths[i], 0, NULL);

		if (dir != NULL) {
			while ((file = g_dir_read_name(dir)) != NULL) {
				path = g_build_filename(search_paths[i], file, NULL);

				if (ext == NULL || is_so_file(file, ext))
					plugin = gaim_plugin_probe(path);

				g_free(path);
			}

			g_dir_close(dir);
		}
	}

	if (probe_cb != NULL)
		probe_cb(probe_cb_data);

#endif /* GAIM_PLUGINS */
}

gboolean
gaim_plugin_register(GaimPlugin *plugin)
{
	g_return_val_if_fail(plugin != NULL, FALSE);

	if (g_list_find(plugins, plugin))
		return TRUE;

	/* Special exception for loader plugins. We want them loaded NOW! */
	if (plugin->info->type == GAIM_PLUGIN_LOADER) {
		GList *exts;

		/* We'll just load this right now. */
		if (!gaim_plugin_load(plugin)) {

			gaim_plugin_destroy(plugin);

			return FALSE;
		}

		plugin_loaders = g_list_append(plugin_loaders, plugin);

		for (exts = GAIM_PLUGIN_LOADER_INFO(plugin)->exts;
			 exts != NULL;
			 exts = exts->next) {

			gaim_plugins_probe(exts->data);
		}
	}
	else if (plugin->info->type == GAIM_PLUGIN_PROTOCOL) {

		/* We'll just load this right now. */
		if (!gaim_plugin_load(plugin)) {

			gaim_plugin_destroy(plugin);

			return FALSE;
		}
	
		if (GAIM_PLUGIN_PROTOCOL_INFO(plugin)->protocol == GAIM_PROTO_ICQ ||
			gaim_find_prpl(GAIM_PLUGIN_PROTOCOL_INFO(plugin)->protocol)) {

			/* Nothing to see here--move along, move along */
			gaim_plugin_destroy(plugin);

			return FALSE;
		}

		protocol_plugins = g_list_insert_sorted(protocol_plugins, plugin,
												(GCompareFunc)compare_prpl);
	}

	plugins = g_list_append(plugins, plugin);

	return TRUE;
}

gboolean
gaim_plugins_enabled(void)
{
#ifdef GAIM_PLUGINS
	return TRUE;
#else
	return FALSE;
#endif
}

void
gaim_plugins_register_probe_notify_cb(void (*func)(void *), void *data)
{
	/* TODO */
	probe_cb = func;
	probe_cb_data = data;
}

void
gaim_plugins_unregister_probe_notify_cb(void (*func)(void *))
{
	/* TODO */
	probe_cb = NULL;
	probe_cb_data = NULL;
}

void
gaim_plugins_register_load_notify_cb(void (*func)(GaimPlugin *, void *),
									 void *data)
{
	/* TODO */
	load_cb = func;
	load_cb_data = data;
}

void
gaim_plugins_unregister_load_notify_cb(void (*func)(GaimPlugin *, void *))
{
	/* TODO */
	load_cb = NULL;
	load_cb_data = NULL;
}

void
gaim_plugins_register_unload_notify_cb(void (*func)(GaimPlugin *, void *),
									   void *data)
{
	/* TODO */
	unload_cb = func;
	unload_cb_data = data;
}

void
gaim_plugins_unregister_unload_notify_cb(void (*func)(GaimPlugin *, void *))
{
	/* TODO */
	unload_cb = NULL;
	unload_cb_data = NULL;
}

GaimPlugin *
gaim_plugins_find_with_name(const char *name)
{
	GaimPlugin *plugin;
	GList *l;

	for (l = plugins; l != NULL; l = l->next) {
		plugin = l->data;

		if (!strcmp(plugin->info->name, name))
			return plugin;
	}

	return NULL;
}

GaimPlugin *
gaim_plugins_find_with_filename(const char *filename)
{
	GaimPlugin *plugin;
	GList *l;

	for (l = plugins; l != NULL; l = l->next) {
		plugin = l->data;

		if (plugin->path != NULL && !strcmp(plugin->path, filename))
			return plugin;
	}

	return NULL;
}

GaimPlugin *
gaim_plugins_find_with_id(const char *id)
{
	GaimPlugin *plugin;
	GList *l;

	g_return_val_if_fail(id != NULL, NULL);

	for (l = plugins; l != NULL; l = l->next) {
		plugin = l->data;

		if (!strcmp(plugin->info->id, id))
			return plugin;
	}

	return NULL;
}

GList *
gaim_plugins_get_loaded(void)
{
	return loaded_plugins;
}

GList *
gaim_plugins_get_protocols(void)
{
	return protocol_plugins;
}

GList *
gaim_plugins_get_all(void)
{
	return plugins;
}

