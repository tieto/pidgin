/*
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
#include "internal.h"

#include "accountopt.h"
#include "debug.h"
#include "notify.h"
#include "prefs.h"
#include "prpl.h"
#include "request.h"
#include "signals.h"
#include "version.h"

#ifdef _WIN32
# define PLUGIN_EXT ".dll"
#else
#ifdef __hpux
# define PLUGIN_EXT ".sl"
#else
# define PLUGIN_EXT ".so"
#endif
#endif

typedef struct
{
	GHashTable *commands;
	size_t command_count;

} GaimPluginIpcInfo;

typedef struct
{
	GaimCallback func;
	GaimSignalMarshalFunc marshal;

	int num_params;
	GaimValue **params;
	GaimValue *ret_value;

} GaimPluginIpcCommand;

static GList *loaded_plugins   = NULL;
static GList *plugins          = NULL;
static GList *plugin_loaders   = NULL;
static GList *protocol_plugins = NULL;
static GList *load_queue       = NULL;

static size_t search_path_count = 0;
static char **search_paths = NULL;

static void (*probe_cb)(void *) = NULL;
static void *probe_cb_data = NULL;
static void (*load_cb)(GaimPlugin *, void *) = NULL;
static void *load_cb_data = NULL;
static void (*unload_cb)(GaimPlugin *, void *) = NULL;
static void *unload_cb_data = NULL;


void *
gaim_plugins_get_handle(void)
{
	static int handle;
	return &handle;
}


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
loader_supports_file(GaimPlugin *loader, const char *filename)
{
	GList *exts;

	for (exts = GAIM_PLUGIN_LOADER_INFO(loader)->exts; exts != NULL; exts = exts->next) {
		if (is_so_file(filename, (char *)exts->data)) {
			return TRUE;
		}
	}

	return FALSE;
}

static GaimPlugin *
find_loader_for_plugin(const GaimPlugin *plugin)
{
	GaimPlugin *loader;
	GList *l;

	if (plugin->path == NULL)
		return NULL;

	for (l = gaim_plugins_get_loaded(); l != NULL; l = l->next) {
		loader = l->data;

		if (loader->info->type == GAIM_PLUGIN_LOADER &&
			loader_supports_file(loader, plugin->path)) {

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
	if(GAIM_IS_PROTOCOL_PLUGIN(a)) {
		if(GAIM_IS_PROTOCOL_PLUGIN(b))
			return strcmp(a->info->name, b->info->name);
		else
			return -1;
	} else {
		if(GAIM_IS_PROTOCOL_PLUGIN(b))
			return 1;
		else
			return 0;
	}
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
	gpointer unpunned;
	gboolean (*gaim_init_plugin)(GaimPlugin *);

	gaim_debug_misc("plugins", "probing %s\n", filename);
	g_return_val_if_fail(filename != NULL, NULL);

	if (!g_file_test(filename, G_FILE_TEST_EXISTS))
		return NULL;

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
							 &unpunned)) {
			g_module_close(plugin->handle);
			plugin->handle = NULL;

			error = g_module_error();
			gaim_debug(GAIM_DEBUG_ERROR, "plugins", "%s is unloadable: %s\n",
					   plugin->path, error ? error : "Unknown error.");

			gaim_plugin_destroy(plugin);

			return NULL;
		}
		gaim_init_plugin = unpunned;
	}
	else {
		loader = find_loader_for_plugin(plugin);

		if (loader == NULL) {
			gaim_plugin_destroy(plugin);

			return NULL;
		}

		gaim_init_plugin = GAIM_PLUGIN_LOADER_INFO(loader)->probe;
	}

	plugin->error = NULL;

	if (!gaim_init_plugin(plugin) || plugin->info == NULL) {
		gaim_plugin_destroy(plugin);

		return NULL;
	}

	if (plugin->info->magic != GAIM_PLUGIN_MAGIC ||
			plugin->info->major_version != GAIM_MAJOR_VERSION ||
			plugin->info->minor_version > GAIM_MINOR_VERSION)
	{
		gaim_debug(GAIM_DEBUG_ERROR, "plugins", "%s is unloadable: API version mismatch %d.%d.x (need %d.%d.x)\n",
				   plugin->path, plugin->info->major_version, plugin->info->minor_version, GAIM_MAJOR_VERSION, GAIM_MINOR_VERSION);
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
	GList *dep_list = NULL;
	GList *l;

	g_return_val_if_fail(plugin != NULL, FALSE);
	g_return_val_if_fail(plugin->error == NULL, FALSE);

	if (gaim_plugin_is_loaded(plugin))
		return TRUE;

	/*
	 * Go through the list of the plugin's dependencies.
	 *
	 * First pass: Make sure all the plugins needed are probed.
	 */
	for (l = plugin->info->dependencies; l != NULL; l = l->next)
	{
		const char *dep_name = (const char *)l->data;
		GaimPlugin *dep_plugin;

		dep_plugin = gaim_plugins_find_with_id(dep_name);

		if (dep_plugin == NULL)
		{
			char buf[BUFSIZ];

			g_snprintf(buf, sizeof(buf),
					   _("The required plugin %s was not found. "
						 "Please install this plugin and try again."),
					   dep_name);

			gaim_notify_error(NULL, NULL,
							  _("Gaim was unable to load your plugin."),
							  buf);

			if (dep_list != NULL)
				g_list_free(dep_list);

			return FALSE;
		}

		dep_list = g_list_append(dep_list, dep_plugin);
	}

	/* Second pass: load all the required plugins. */
	for (l = dep_list; l != NULL; l = l->next)
	{
		GaimPlugin *dep_plugin = (GaimPlugin *)l->data;

		if (!gaim_plugin_is_loaded(dep_plugin))
		{
			if (!gaim_plugin_load(dep_plugin))
			{
				char buf[BUFSIZ];

				g_snprintf(buf, sizeof(buf),
						   _("The required plugin %s was unable to load."),
						   plugin->info->name);

				gaim_notify_error(NULL, NULL,
								  _("Gaim was unable to load your plugin."),
								  buf);

				if (dep_list != NULL)
					g_list_free(dep_list);

				return FALSE;
			}
		}
	}

	if (dep_list != NULL)
		g_list_free(dep_list);

	if (plugin->native_plugin)
	{
		if (plugin->info != NULL && plugin->info->load != NULL)
		{
			if (!plugin->info->load(plugin))
				return FALSE;
		}
	}
	else {
		GaimPlugin *loader;
		GaimPluginLoaderInfo *loader_info;

		loader = find_loader_for_plugin(plugin);

		if (loader == NULL)
			return FALSE;

		loader_info = GAIM_PLUGIN_LOADER_INFO(loader);

		if (loader_info->load != NULL)
		{
			if (!loader_info->load(plugin))
				return FALSE;
		}
	}

	loaded_plugins = g_list_append(loaded_plugins, plugin);

	plugin->loaded = TRUE;

	/* TODO */
	if (load_cb != NULL)
		load_cb(plugin, load_cb_data);

	gaim_signal_emit(gaim_plugins_get_handle(), "plugin-load", plugin);

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

			if (prpl_info->user_splits != NULL)
				g_list_free(prpl_info->user_splits);

			if (prpl_info->protocol_options != NULL)
				g_list_free(prpl_info->protocol_options);
		}
	}
	else {
		GaimPlugin *loader;
		GaimPluginLoaderInfo *loader_info;

		loader = find_loader_for_plugin(plugin);

		if (loader == NULL)
			return FALSE;

		loader_info = GAIM_PLUGIN_LOADER_INFO(loader);

		if (loader_info->unload != NULL)
			loader_info->unload(plugin);
	}

	gaim_signals_disconnect_by_handle(plugin);
	gaim_plugin_ipc_unregister_all(plugin);

	/* TODO */
	if (unload_cb != NULL)
		unload_cb(plugin, unload_cb_data);

	/* I suppose this is the right place to call this... */
	gaim_signal_emit(gaim_plugins_get_handle(), "plugin-unload", plugin);

	gaim_prefs_disconnect_by_handle(plugin);

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

	if (load_queue != NULL)
		load_queue = g_list_remove(load_queue, plugin);

	/* true, this may leak a little memory if there is a major version
	 * mismatch, but it's a lot better than trying to free something
	 * we shouldn't, and crashing while trying to load an old plugin */
	if(plugin->info == NULL || plugin->info->magic != GAIM_PLUGIN_MAGIC ||
			plugin->info->major_version != GAIM_MAJOR_VERSION) {
		if(plugin->handle)
			g_module_close(plugin->handle);
		if(plugin->path != NULL)
			g_free(plugin->path);
		g_free(plugin);
		return;
	}

	if (plugin->info != NULL && plugin->info->dependencies != NULL)
		g_list_free(plugin->info->dependencies);

	if (plugin->native_plugin)
	{
		if (plugin->info != NULL && plugin->info->type == GAIM_PLUGIN_LOADER)
		{
			GaimPluginLoaderInfo *loader_info;
			GList *exts, *l, *next_l;
			GaimPlugin *p2;

			loader_info = GAIM_PLUGIN_LOADER_INFO(plugin);

			if (loader_info != NULL && loader_info->exts != NULL)
			{
				for (exts = GAIM_PLUGIN_LOADER_INFO(plugin)->exts;
					 exts != NULL;
					 exts = exts->next) {

					for (l = gaim_plugins_get_all(); l != NULL; l = next_l)
					{
						next_l = l->next;

						p2 = l->data;

						if (p2->path != NULL &&
							is_so_file(p2->path, exts->data))
						{
							gaim_plugin_destroy(p2);
						}
					}
				}

				g_list_free(loader_info->exts);
			}

			plugin_loaders = g_list_remove(plugin_loaders, plugin);
		}

		if (plugin->info != NULL && plugin->info->destroy != NULL)
			plugin->info->destroy(plugin);

		if (plugin->handle != NULL)
			g_module_close(plugin->handle);
	}
	else
	{
		GaimPlugin *loader;
		GaimPluginLoaderInfo *loader_info;

		loader = find_loader_for_plugin(plugin);

		if (loader != NULL)
		{
			loader_info = GAIM_PLUGIN_LOADER_INFO(loader);

			if (loader_info->destroy != NULL)
				loader_info->destroy(plugin);
		}
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

/**************************************************************************
 * Plugin IPC
 **************************************************************************/
static void
destroy_ipc_info(void *data)
{
	GaimPluginIpcCommand *ipc_command = (GaimPluginIpcCommand *)data;
	int i;

	if (ipc_command->params != NULL)
	{
		for (i = 0; i < ipc_command->num_params; i++)
			gaim_value_destroy(ipc_command->params[i]);

		g_free(ipc_command->params);
	}

	if (ipc_command->ret_value != NULL)
		gaim_value_destroy(ipc_command->ret_value);

	g_free(ipc_command);
}

gboolean
gaim_plugin_ipc_register(GaimPlugin *plugin, const char *command,
						 GaimCallback func, GaimSignalMarshalFunc marshal,
						 GaimValue *ret_value, int num_params, ...)
{
	GaimPluginIpcInfo *ipc_info;
	GaimPluginIpcCommand *ipc_command;

	g_return_val_if_fail(plugin  != NULL, FALSE);
	g_return_val_if_fail(command != NULL, FALSE);
	g_return_val_if_fail(func    != NULL, FALSE);
	g_return_val_if_fail(marshal != NULL, FALSE);

	if (plugin->ipc_data == NULL)
	{
		ipc_info = plugin->ipc_data = g_new0(GaimPluginIpcInfo, 1);
		ipc_info->commands = g_hash_table_new_full(g_str_hash, g_str_equal,
												   g_free, destroy_ipc_info);
	}
	else
		ipc_info = (GaimPluginIpcInfo *)plugin->ipc_data;

	ipc_command = g_new0(GaimPluginIpcCommand, 1);
	ipc_command->func       = func;
	ipc_command->marshal    = marshal;
	ipc_command->num_params = num_params;
	ipc_command->ret_value  = ret_value;

	if (num_params > 0)
	{
		va_list args;
		int i;

		ipc_command->params = g_new0(GaimValue *, num_params);

		va_start(args, num_params);

		for (i = 0; i < num_params; i++)
			ipc_command->params[i] = va_arg(args, GaimValue *);

		va_end(args);
	}

	g_hash_table_replace(ipc_info->commands, g_strdup(command), ipc_command);

	ipc_info->command_count++;

	return TRUE;
}

void
gaim_plugin_ipc_unregister(GaimPlugin *plugin, const char *command)
{
	GaimPluginIpcInfo *ipc_info;

	g_return_if_fail(plugin  != NULL);
	g_return_if_fail(command != NULL);

	ipc_info = (GaimPluginIpcInfo *)plugin->ipc_data;

	if (ipc_info == NULL ||
		g_hash_table_lookup(ipc_info->commands, command) == NULL)
	{
		gaim_debug_error("plugins",
						 "IPC command '%s' was not registered for plugin %s\n",
						 command, plugin->info->name);
		return;
	}

	g_hash_table_remove(ipc_info->commands, command);

	ipc_info->command_count--;

	if (ipc_info->command_count == 0)
	{
		g_hash_table_destroy(ipc_info->commands);
		g_free(ipc_info);

		plugin->ipc_data = NULL;
	}
}

void
gaim_plugin_ipc_unregister_all(GaimPlugin *plugin)
{
	GaimPluginIpcInfo *ipc_info;

	g_return_if_fail(plugin != NULL);

	if (plugin->ipc_data == NULL)
		return; /* Silently ignore it. */

	ipc_info = (GaimPluginIpcInfo *)plugin->ipc_data;

	g_hash_table_destroy(ipc_info->commands);
	g_free(ipc_info);

	plugin->ipc_data = NULL;
}

gboolean
gaim_plugin_ipc_get_params(GaimPlugin *plugin, const char *command,
						   GaimValue **ret_value, int *num_params,
						   GaimValue ***params)
{
	GaimPluginIpcInfo *ipc_info;
	GaimPluginIpcCommand *ipc_command;

	g_return_val_if_fail(plugin  != NULL, FALSE);
	g_return_val_if_fail(command != NULL, FALSE);

	ipc_info = (GaimPluginIpcInfo *)plugin->ipc_data;

	if (ipc_info == NULL ||
		(ipc_command = g_hash_table_lookup(ipc_info->commands,
										   command)) == NULL)
	{
		gaim_debug_error("plugins",
						 "IPC command '%s' was not registered for plugin %s\n",
						 command, plugin->info->name);

		return FALSE;
	}

	if (num_params != NULL)
		*num_params = ipc_command->num_params;

	if (params != NULL)
		*params = ipc_command->params;

	if (ret_value != NULL)
		*ret_value = ipc_command->ret_value;

	return TRUE;
}

void *
gaim_plugin_ipc_call(GaimPlugin *plugin, const char *command,
					 gboolean *ok, ...)
{
	GaimPluginIpcInfo *ipc_info;
	GaimPluginIpcCommand *ipc_command;
	va_list args;
	void *ret_value;

	if (ok != NULL)
		*ok = FALSE;

	g_return_val_if_fail(plugin  != NULL, NULL);
	g_return_val_if_fail(command != NULL, NULL);

	ipc_info = (GaimPluginIpcInfo *)plugin->ipc_data;

	if (ipc_info == NULL ||
		(ipc_command = g_hash_table_lookup(ipc_info->commands,
										   command)) == NULL)
	{
		gaim_debug_error("plugins",
						 "IPC command '%s' was not registered for plugin %s\n",
						 command, plugin->info->name);

		return NULL;
	}

	va_start(args, ok);
	ipc_command->marshal(ipc_command->func, args, NULL, &ret_value);
	va_end(args);

	if (ok != NULL)
		*ok = TRUE;

	return ret_value;
}

/**************************************************************************
 * Plugins subsystem
 **************************************************************************/
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
gaim_plugins_load_saved(const char *key)
{
#ifdef GAIM_PLUGINS
	GList *f, *files;

	g_return_if_fail(key != NULL);

	files = gaim_prefs_get_string_list(key);

	for (f = files; f; f = f->next)
	{
		char *filename = g_path_get_basename(f->data);
		GaimPlugin *plugin = NULL;

		if (filename != NULL)
		{
			if ((plugin = gaim_plugins_find_with_basename(filename)) != NULL)
			{
				gaim_debug_info("plugins", "Loading saved plugin %s\n",
								filename);
				gaim_plugin_load(plugin);
			}
			else
			{
				gaim_debug_error("plugins", "Unable to find saved plugin %s\n",
								 filename);
			}

			g_free(filename);
		}

		g_free(f->data);
	}

	g_list_free(files);
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

	void *handle;

	if (!g_module_supported())
		return;

	handle = gaim_plugins_get_handle();

	gaim_debug_info("plugins", "registering plugin-load signal\n");
	gaim_signal_register(handle, "plugin-load", gaim_marshal_VOID__POINTER, NULL,
			1, gaim_value_new(GAIM_TYPE_SUBTYPE, GAIM_SUBTYPE_PLUGIN));

	gaim_debug_info("plugins", "registering plugin-unload signal\n");
	gaim_signal_register(handle, "plugin-unload", gaim_marshal_VOID__POINTER, NULL,
			1, gaim_value_new(GAIM_TYPE_SUBTYPE, GAIM_SUBTYPE_PLUGIN));


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

	/* See if we have any plugins waiting to load. */
	while (load_queue != NULL)
	{
		plugin = (GaimPlugin *)load_queue->data;

		load_queue = g_list_remove(load_queue, plugin);

		if (plugin == NULL || plugin->info == NULL)
			continue;

		if (plugin->info->type == GAIM_PLUGIN_LOADER)
		{
			GList *exts;

			/* We'll just load this right now. */
			if (!gaim_plugin_load(plugin))
			{
				gaim_plugin_destroy(plugin);

				continue;
			}

			plugin_loaders = g_list_append(plugin_loaders, plugin);

			for (exts = GAIM_PLUGIN_LOADER_INFO(plugin)->exts;
				 exts != NULL;
				 exts = exts->next)
			{
				gaim_plugins_probe(exts->data);
			}
		}
		else if (plugin->info->type == GAIM_PLUGIN_PROTOCOL)
		{
			/* We'll just load this right now. */
			if (!gaim_plugin_load(plugin))
			{
				gaim_plugin_destroy(plugin);

				continue;
			}

			if (gaim_find_prpl(plugin->info->id))
			{
				/* Nothing to see here--move along, move along */
				gaim_plugin_destroy(plugin);

				continue;
			}

			protocol_plugins = g_list_insert_sorted(protocol_plugins, plugin,
													(GCompareFunc)compare_prpl);
		}
	}

	if (load_queue != NULL)
	{
		g_list_free(load_queue);
		load_queue = NULL;
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

	if (plugin->info->type == GAIM_PLUGIN_LOADER)
	{
		GaimPluginLoaderInfo *loader_info;

		loader_info = GAIM_PLUGIN_LOADER_INFO(plugin);

		if (loader_info == NULL)
		{
			gaim_debug(GAIM_DEBUG_ERROR, "plugins", "%s is unloadable\n",
							   plugin->path);
			return FALSE;
		}

		load_queue = g_list_append(load_queue, plugin);
	}
	else if (plugin->info->type == GAIM_PLUGIN_PROTOCOL)
	{
		GaimPluginProtocolInfo *prpl_info;

		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(plugin);

		if (prpl_info == NULL)
		{
			gaim_debug(GAIM_DEBUG_ERROR, "plugins", "%s is unloadable\n",
							   plugin->path);
			return FALSE;
		}

		load_queue = g_list_append(load_queue, plugin);
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
gaim_plugins_find_with_basename(const char *basename)
{
	GaimPlugin *plugin;
	GList *l;

	g_return_val_if_fail(basename != NULL, NULL);

	for (l = plugins; l != NULL; l = l->next)
	{
		char *tmp;

		plugin = (GaimPlugin *)l->data;

		if (plugin->path != NULL) {
			tmp = g_path_get_basename(plugin->path);
			if (!strcmp(tmp, basename)) {
				g_free(tmp);
				return plugin;
			}
			g_free(tmp);
		}
	}

	return NULL;
}

GaimPlugin *
gaim_plugins_find_with_id(const char *id)
{
	GaimPlugin *plugin;
	GList *l;

	g_return_val_if_fail(id != NULL, NULL);

	for (l = plugins; l != NULL; l = l->next)
	{
		plugin = l->data;

		if (plugin->info->id != NULL && !strcmp(plugin->info->id, id))
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


GaimPluginAction *
gaim_plugin_action_new(char* label, void (*callback)(GaimPluginAction *))
{
	GaimPluginAction *act = g_new0(GaimPluginAction, 1);

	act->label = label;
	act->callback = callback;

	return act;
}

