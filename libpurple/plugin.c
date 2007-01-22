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
#include "core.h"
#include "dbus-maybe.h"
#include "debug.h"
#include "notify.h"
#include "prefs.h"
#include "prpl.h"
#include "request.h"
#include "signals.h"
#include "util.h"
#include "version.h"

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

static GList *search_paths     = NULL;
static GList *plugins          = NULL;
static GList *loaded_plugins   = NULL;
static GList *protocol_plugins = NULL;
#ifdef GAIM_PLUGINS
static GList *load_queue       = NULL;
static GList *plugin_loaders   = NULL;
#endif

/*
 * TODO: I think the intention was to allow multiple load and unload
 *       callback functions.  Perhaps using a GList instead of a
 *       pointer to a single function.
 */
static void (*probe_cb)(void *) = NULL;
static void *probe_cb_data = NULL;
static void (*load_cb)(GaimPlugin *, void *) = NULL;
static void *load_cb_data = NULL;
static void (*unload_cb)(GaimPlugin *, void *) = NULL;
static void *unload_cb_data = NULL;

#ifdef GAIM_PLUGINS

static gboolean
has_file_extension(const char *filename, const char *ext)
{
	int len, extlen;

	if (filename == NULL || *filename == '\0' || ext == NULL)
		return 0;

	extlen = strlen(ext);
	len = strlen(filename) - extlen;

	if (len < 0)
		return 0;

	return (strncmp(filename + len, ext, extlen) == 0);
}

static gboolean
is_native(const char *filename)
{
	const char *last_period;

	last_period = strrchr(filename, '.');
	if (last_period == NULL)
		return FALSE;

	return !(strcmp(last_period, ".dll") &
			 strcmp(last_period, ".sl") &
			 strcmp(last_period, ".so"));
}

static char *
gaim_plugin_get_basename(const char *filename)
{
	const char *basename;
	const char *last_period;

	basename = strrchr(filename, G_DIR_SEPARATOR);
	if (basename != NULL)
		basename++;
	else
		basename = filename;

	if (is_native(basename) &&
		((last_period = strrchr(basename, '.')) != NULL))
			return g_strndup(basename, (last_period - basename));

	return g_strdup(basename);
}

static gboolean
loader_supports_file(GaimPlugin *loader, const char *filename)
{
	GList *exts;

	for (exts = GAIM_PLUGIN_LOADER_INFO(loader)->exts; exts != NULL; exts = exts->next) {
		if (has_file_extension(filename, (char *)exts->data)) {
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

/**
 * Negative if a before b, 0 if equal, positive if a after b.
 */
static gint
compare_prpl(GaimPlugin *a, GaimPlugin *b)
{
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
	plugin->path = g_strdup(path);

	GAIM_DBUS_REGISTER_POINTER(plugin, GaimPlugin);

	return plugin;
}

GaimPlugin *
gaim_plugin_probe(const char *filename)
{
#ifdef GAIM_PLUGINS
	GaimPlugin *plugin = NULL;
	GaimPlugin *loader;
	gpointer unpunned;
	gchar *basename = NULL;
	gboolean (*gaim_init_plugin)(GaimPlugin *);

	gaim_debug_misc("plugins", "probing %s\n", filename);
	g_return_val_if_fail(filename != NULL, NULL);

	if (!g_file_test(filename, G_FILE_TEST_EXISTS))
		return NULL;

	/* If this plugin has already been probed then exit */
	basename = gaim_plugin_get_basename(filename);
	plugin = gaim_plugins_find_with_basename(basename);
	g_free(basename);
	if (plugin != NULL)
	{
		if (!strcmp(filename, plugin->path))
			return plugin;
		else if (!gaim_plugin_is_unloadable(plugin))
		{
			gaim_debug_info("plugins", "Not loading %s. "
							"Another plugin with the same name (%s) has already been loaded.\n",
							filename, plugin->path);
			return plugin;
		}
		else
		{
			/* The old plugin was a different file and it was unloadable.
			 * There's no guarantee that this new file with the same name
			 * will be loadable, but unless it fails in one of the silent
			 * ways and the first one didn't, it's not any worse.  The user
			 * will still see a greyed-out plugin, which is what we want. */
			gaim_plugin_destroy(plugin);
		}
	}

	plugin = gaim_plugin_new(has_file_extension(filename, G_MODULE_SUFFIX), filename);

	if (plugin->native_plugin) {
		const char *error;
#ifdef _WIN32
		/* Suppress error popups for failing to load plugins */
		UINT old_error_mode = SetErrorMode(SEM_FAILCRITICALERRORS);
#endif

		/*
		 * We pass G_MODULE_BIND_LOCAL here to prevent symbols from
		 * plugins being added to the global name space.
		 *
		 * G_MODULE_BIND_LOCAL was added in glib 2.3.3.
		 * TODO: I guess there's nothing we can do about that?
		 */
#if GLIB_CHECK_VERSION(2,3,3)
		plugin->handle = g_module_open(filename, G_MODULE_BIND_LOCAL);
#else
		plugin->handle = g_module_open(filename, 0);
#endif

		if (plugin->handle == NULL)
		{
			const char *error = g_module_error();
			if (error != NULL && gaim_str_has_prefix(error, filename))
			{
				error = error + strlen(filename);

				/* These are just so we don't crash.  If we
				 * got this far, they should always be true. */
				if (*error == ':')
					error++;
				if (*error == ' ')
					error++;
			}

			if (error == NULL || !*error)
			{
				plugin->error = g_strdup(_("Unknown error"));
				gaim_debug_error("plugins", "%s is not loadable: Unknown error\n",
						 plugin->path);
			}
			else
			{
				plugin->error = g_strdup(error);
				gaim_debug_error("plugins", "%s is not loadable: %s\n",
						 plugin->path, plugin->error);
			}
#if GLIB_CHECK_VERSION(2,3,3)
			plugin->handle = g_module_open(filename, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
#else
			plugin->handle = g_module_open(filename, G_MODULE_BIND_LAZY);
#endif

			if (plugin->handle == NULL)
			{
#ifdef _WIN32
				/* Restore the original error mode */
				SetErrorMode(old_error_mode);
#endif
				gaim_plugin_destroy(plugin);
				return NULL;
			}
			else
			{
				/* We were able to load the plugin with lazy symbol binding.
				 * This means we're missing some symbol.  Mark it as
				 * unloadable and keep going so we get the info to display
				 * to the user so they know to rebuild this plugin. */
				plugin->unloadable = TRUE;
			}
		}

		if (!g_module_symbol(plugin->handle, "gaim_init_plugin",
							 &unpunned))
		{
			gaim_debug_error("plugins", "%s is not usable because the "
							 "'gaim_init_plugin' symbol could not be "
							 "found.  Does the plugin call the "
							 "GAIM_INIT_PLUGIN() macro?\n", plugin->path);

			g_module_close(plugin->handle);
			error = g_module_error();
			if (error != NULL)
				gaim_debug_error("plugins", "Error closing module %s: %s\n",
								 plugin->path, error);
			plugin->handle = NULL;

#ifdef _WIN32
			/* Restore the original error mode */
			SetErrorMode(old_error_mode);
#endif
			gaim_plugin_destroy(plugin);
			return NULL;
		}
		gaim_init_plugin = unpunned;

#ifdef _WIN32
		/* Restore the original error mode */
		SetErrorMode(old_error_mode);
#endif
	}
	else {
		loader = find_loader_for_plugin(plugin);

		if (loader == NULL) {
			gaim_plugin_destroy(plugin);
			return NULL;
		}

		gaim_init_plugin = GAIM_PLUGIN_LOADER_INFO(loader)->probe;
	}

	if (!gaim_init_plugin(plugin) || plugin->info == NULL)
	{
		gaim_plugin_destroy(plugin);
		return NULL;
	}
	else if (plugin->info->ui_requirement &&
			strcmp(plugin->info->ui_requirement, gaim_core_get_ui()))
	{
		plugin->error = g_strdup_printf(_("You are using %s, but this plugin requires %s."),
					gaim_core_get_ui(), plugin->info->ui_requirement);
		gaim_debug_error("plugins", "%s is not loadable: The UI requirement is not met.\n", plugin->path);
		plugin->unloadable = TRUE;
		return plugin;
	}

	/* Really old plugins. */
	if (plugin->info->magic != GAIM_PLUGIN_MAGIC)
	{
		if (plugin->info->magic >= 2 && plugin->info->magic <= 4)
		{
			struct _GaimPluginInfo2
			{
				unsigned int api_version;
				GaimPluginType type;
				char *ui_requirement;
				unsigned long flags;
				GList *dependencies;
				GaimPluginPriority priority;

				char *id;
				char *name;
				char *version;
				char *summary;
				char *description;
				char *author;
				char *homepage;

				gboolean (*load)(GaimPlugin *plugin);
				gboolean (*unload)(GaimPlugin *plugin);
				void (*destroy)(GaimPlugin *plugin);

				void *ui_info;
				void *extra_info;
				GaimPluginUiInfo *prefs_info;
				GList *(*actions)(GaimPlugin *plugin, gpointer context);
			} *info2 = (struct _GaimPluginInfo2 *)plugin->info;

			/* This leaks... but only for ancient plugins, so deal with it. */
			plugin->info = g_new0(GaimPluginInfo, 1);

			/* We don't really need all these to display the plugin info, but
			 * I'm copying them all for good measure. */
			plugin->info->magic          = info2->api_version;
			plugin->info->type           = info2->type;
			plugin->info->ui_requirement = info2->ui_requirement;
			plugin->info->flags          = info2->flags;
			plugin->info->dependencies   = info2->dependencies;
			plugin->info->id             = info2->id;
			plugin->info->name           = info2->name;
			plugin->info->version        = info2->version;
			plugin->info->summary        = info2->summary;
			plugin->info->description    = info2->description;
			plugin->info->author         = info2->author;
			plugin->info->homepage       = info2->homepage;
			plugin->info->load           = info2->load;
			plugin->info->unload         = info2->unload;
			plugin->info->destroy        = info2->destroy;
			plugin->info->ui_info        = info2->ui_info;
			plugin->info->extra_info     = info2->extra_info;

			if (info2->api_version >= 3)
				plugin->info->prefs_info = info2->prefs_info;

			if (info2->api_version >= 4)
				plugin->info->actions    = info2->actions;


			plugin->error = g_strdup_printf(_("Plugin magic mismatch %d (need %d)"),
							 plugin->info->magic, GAIM_PLUGIN_MAGIC);
			gaim_debug_error("plugins", "%s is not loadable: Plugin magic mismatch %d (need %d)\n",
					  plugin->path, plugin->info->magic, GAIM_PLUGIN_MAGIC);
			plugin->unloadable = TRUE;
			return plugin;
		}

		gaim_debug_error("plugins", "%s is not loadable: Plugin magic mismatch %d (need %d)\n",
				 plugin->path, plugin->info->magic, GAIM_PLUGIN_MAGIC);
		gaim_plugin_destroy(plugin);
		return NULL;
	}

	if (plugin->info->major_version != GAIM_MAJOR_VERSION ||
			plugin->info->minor_version > GAIM_MINOR_VERSION)
	{
		plugin->error = g_strdup_printf(_("ABI version mismatch %d.%d.x (need %d.%d.x)"),
						 plugin->info->major_version, plugin->info->minor_version,
						 GAIM_MAJOR_VERSION, GAIM_MINOR_VERSION);
		gaim_debug_error("plugins", "%s is not loadable: ABI version mismatch %d.%d.x (need %d.%d.x)\n",
				 plugin->path, plugin->info->major_version, plugin->info->minor_version,
				 GAIM_MAJOR_VERSION, GAIM_MINOR_VERSION);
		plugin->unloadable = TRUE;
		return plugin;
	}

	if (plugin->info->type == GAIM_PLUGIN_PROTOCOL)
	{
		/* If plugin is a PRPL, make sure it implements the required functions */
		if ((GAIM_PLUGIN_PROTOCOL_INFO(plugin)->list_icon == NULL) ||
		    (GAIM_PLUGIN_PROTOCOL_INFO(plugin)->login == NULL) ||
		    (GAIM_PLUGIN_PROTOCOL_INFO(plugin)->close == NULL))
		{
			plugin->error = g_strdup(_("Plugin does not implement all required functions"));
			gaim_debug_error("plugins", "%s is not loadable: Plugin does not implement all required functions\n",
					 plugin->path);
			plugin->unloadable = TRUE;
			return plugin;
		}

		/* For debugging, let's warn about prpl prefs. */
		if (plugin->info->prefs_info != NULL)
		{
			gaim_debug_error("plugins", "%s has a prefs_info, but is a prpl. This is no longer supported.\n",
			                 plugin->path);
		}
	}

	return plugin;
#else
	return NULL;
#endif /* !GAIM_PLUGINS */
}

#ifdef GAIM_PLUGINS
static gint
compare_plugins(gconstpointer a, gconstpointer b)
{
	const GaimPlugin *plugina = a;
	const GaimPlugin *pluginb = b;

	return strcmp(plugina->info->name, pluginb->info->name);
}
#endif /* GAIM_PLUGINS */

gboolean
gaim_plugin_load(GaimPlugin *plugin)
{
#ifdef GAIM_PLUGINS
	GList *dep_list = NULL;
	GList *l;

	g_return_val_if_fail(plugin != NULL, FALSE);

	if (gaim_plugin_is_loaded(plugin))
		return TRUE;

	if (gaim_plugin_is_unloadable(plugin))
		return FALSE;

	g_return_val_if_fail(plugin->error == NULL, FALSE);

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
			char *tmp;

			tmp = g_strdup_printf(_("The required plugin %s was not found. "
			                        "Please install this plugin and try again."),
			                      dep_name);

			gaim_notify_error(NULL, NULL,
			                  _("Unable to load the plugin"), tmp);
			g_free(tmp);

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
				char *tmp;

				tmp = g_strdup_printf(_("The required plugin %s was unable to load."),
				                      plugin->info->name);

				gaim_notify_error(NULL, NULL,
				                 _("Unable to load your plugin."), tmp);
				g_free(tmp);

				g_list_free(dep_list);

				return FALSE;
			}
		}
	}

	/* Third pass: note that other plugins are dependencies of this plugin.
	 * This is done separately in case we had to bail out earlier. */
	for (l = dep_list; l != NULL; l = l->next)
	{
		GaimPlugin *dep_plugin = (GaimPlugin *)l->data;
		dep_plugin->dependent_plugins = g_list_prepend(dep_plugin->dependent_plugins, plugin->info->id);
	}

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

	loaded_plugins = g_list_insert_sorted(loaded_plugins, plugin, compare_plugins);

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
	GList *l;

	g_return_val_if_fail(plugin != NULL, FALSE);

	loaded_plugins = g_list_remove(loaded_plugins, plugin);
	if ((plugin->info != NULL) && GAIM_IS_PROTOCOL_PLUGIN(plugin))
		protocol_plugins = g_list_remove(protocol_plugins, plugin);

	g_return_val_if_fail(gaim_plugin_is_loaded(plugin), FALSE);

	gaim_debug_info("plugins", "Unloading plugin %s\n", plugin->info->name);

	/* cancel any pending dialogs the plugin has */
	gaim_request_close_with_handle(plugin);
	gaim_notify_close_with_handle(plugin);

	plugin->loaded = FALSE;

	/* Unload all plugins that depend on this plugin. */
	while ((l = plugin->dependent_plugins) != NULL)
	{
		const char * dep_name = (const char *)l->data;
		GaimPlugin *dep_plugin;

		dep_plugin = gaim_plugins_find_with_id(dep_name);

		if (dep_plugin != NULL && gaim_plugin_is_loaded(dep_plugin))
		{
			if (!gaim_plugin_unload(dep_plugin))
			{
				char *translated_name = g_strdup(_(dep_plugin->info->name));
				char *tmp;

				tmp = g_strdup_printf(_("The dependent plugin %s failed to unload."),
				                      translated_name);
				g_free(translated_name);

				gaim_notify_error(NULL, NULL,
				                  _("There were errors unloading the plugin."), tmp);
				g_free(tmp);
			}
		}
	}

	/* Remove this plugin from each dependency's dependent_plugins list. */
	for (l = plugin->info->dependencies; l != NULL; l = l->next)
	{
		const char *dep_name = (const char *)l->data;
		GaimPlugin *dependency;

		dependency = gaim_plugins_find_with_id(dep_name);

		dependency->dependent_plugins = g_list_remove(dependency->dependent_plugins, plugin->info->id);
	}

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

			if (prpl_info->user_splits != NULL) {
				g_list_free(prpl_info->user_splits);
				prpl_info->user_splits = NULL;
			}

			if (prpl_info->protocol_options != NULL) {
				g_list_free(prpl_info->protocol_options);
				prpl_info->protocol_options = NULL;
			}
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
			plugin->info->major_version != GAIM_MAJOR_VERSION)
	{
		if(plugin->handle)
			g_module_close(plugin->handle);

		g_free(plugin->path);
		g_free(plugin->error);

		GAIM_DBUS_UNREGISTER_POINTER(plugin);

		g_free(plugin);
		return;
	}

	if (plugin->info != NULL)
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
							has_file_extension(p2->path, exts->data))
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

	g_free(plugin->path);
	g_free(plugin->error);

	GAIM_DBUS_UNREGISTER_POINTER(plugin);

	g_free(plugin);
#endif /* !GAIM_PLUGINS */
}

gboolean
gaim_plugin_is_loaded(const GaimPlugin *plugin)
{
	g_return_val_if_fail(plugin != NULL, FALSE);

	return plugin->loaded;
}

gboolean
gaim_plugin_is_unloadable(const GaimPlugin *plugin)
{
	g_return_val_if_fail(plugin != NULL, FALSE);

	return plugin->unloadable;
}

const gchar *
gaim_plugin_get_id(const GaimPlugin *plugin) {
	g_return_val_if_fail(plugin, NULL);
	g_return_val_if_fail(plugin->info, NULL);

	return plugin->info->id;
}

const gchar *
gaim_plugin_get_name(const GaimPlugin *plugin) {
	g_return_val_if_fail(plugin, NULL);
	g_return_val_if_fail(plugin->info, NULL);

	return plugin->info->name;
}

const gchar *
gaim_plugin_get_version(const GaimPlugin *plugin) {
	g_return_val_if_fail(plugin, NULL);
	g_return_val_if_fail(plugin->info, NULL);

	return plugin->info->version;
}

const gchar *
gaim_plugin_get_summary(const GaimPlugin *plugin) {
	g_return_val_if_fail(plugin, NULL);
	g_return_val_if_fail(plugin->info, NULL);

	return plugin->info->summary;
}

const gchar *
gaim_plugin_get_description(const GaimPlugin *plugin) {
	g_return_val_if_fail(plugin, NULL);
	g_return_val_if_fail(plugin->info, NULL);

	return plugin->info->description;
}

const gchar *
gaim_plugin_get_author(const GaimPlugin *plugin) {
	g_return_val_if_fail(plugin, NULL);
	g_return_val_if_fail(plugin->info, NULL);

	return plugin->info->author;
}

const gchar *
gaim_plugin_get_homepage(const GaimPlugin *plugin) {
	g_return_val_if_fail(plugin, NULL);
	g_return_val_if_fail(plugin->info, NULL);

	return plugin->info->homepage;
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
void *
gaim_plugins_get_handle(void) {
	static int handle;

	return &handle;
}

void
gaim_plugins_init(void) {
	void *handle = gaim_plugins_get_handle();

	gaim_signal_register(handle, "plugin-load",
						 gaim_marshal_VOID__POINTER,
						 NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_PLUGIN));
	gaim_signal_register(handle, "plugin-unload",
						 gaim_marshal_VOID__POINTER,
						 NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_PLUGIN));
}

void
gaim_plugins_uninit(void) {
	gaim_signals_disconnect_by_handle(gaim_plugins_get_handle());
}

/**************************************************************************
 * Plugins API
 **************************************************************************/
void
gaim_plugins_add_search_path(const char *path)
{
	g_return_if_fail(path != NULL);

	if (g_list_find_custom(search_paths, path, (GCompareFunc)strcmp))
		return;

	search_paths = g_list_append(search_paths, strdup(path));
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
gaim_plugins_save_loaded(const char *key)
{
#ifdef GAIM_PLUGINS
	GList *pl;
	GList *files = NULL;
	GaimPlugin *p;

	for (pl = gaim_plugins_get_loaded(); pl != NULL; pl = pl->next) {
		p = pl->data;

		if (p->info->type != GAIM_PLUGIN_PROTOCOL &&
			p->info->type != GAIM_PLUGIN_LOADER) {
				files = g_list_append(files, p->path);
		}
	}

	gaim_prefs_set_path_list(key, files);
	g_list_free(files);
#endif
}

void
gaim_plugins_load_saved(const char *key)
{
#ifdef GAIM_PLUGINS
	GList *f, *files;

	g_return_if_fail(key != NULL);

	files = gaim_prefs_get_path_list(key);

	for (f = files; f; f = f->next)
	{
		char *filename;
		char *basename;
		GaimPlugin *plugin;

		if (f->data == NULL)
			continue;

		filename = f->data;

		/*
		 * We don't know if the filename uses Windows or Unix path
		 * separators (because people might be sharing a prefs.xml
		 * file across systems), so we find the last occurrence
		 * of either.
		 */
		basename = strrchr(filename, '/');
		if ((basename == NULL) || (basename < strrchr(filename, '\\')))
			basename = strrchr(filename, '\\');
		if (basename != NULL)
			basename++;

		/* Strip the extension */
		if (basename)
			basename = gaim_plugin_get_basename(filename);

		if ((plugin = gaim_plugins_find_with_filename(filename)) != NULL)
		{
			gaim_debug_info("plugins", "Loading saved plugin %s\n",
							plugin->path);
			gaim_plugin_load(plugin);
		}
		else if (basename && (plugin = gaim_plugins_find_with_basename(basename)) != NULL)
		{
			gaim_debug_info("plugins", "Loading saved plugin %s\n",
							plugin->path);
			gaim_plugin_load(plugin);
		}
		else
		{
			gaim_debug_error("plugins", "Unable to find saved plugin %s\n",
							 filename);
		}

		g_free(basename);

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
	GList *cur;
	const char *search_path;

	if (!g_module_supported())
		return;

	/* Probe plugins */
	for (cur = search_paths; cur != NULL; cur = cur->next)
	{
		search_path = cur->data;

		dir = g_dir_open(search_path, 0, NULL);

		if (dir != NULL)
		{
			while ((file = g_dir_read_name(dir)) != NULL)
			{
				path = g_build_filename(search_path, file, NULL);

				if (ext == NULL || has_file_extension(file, ext))
					plugin = gaim_plugin_probe(path);

				g_free(path);
			}

			g_dir_close(dir);
		}
	}

	/* See if we have any plugins waiting to load */
	while (load_queue != NULL)
	{
		plugin = (GaimPlugin *)load_queue->data;

		load_queue = g_list_remove(load_queue, plugin);

		if (plugin == NULL || plugin->info == NULL)
			continue;

		if (plugin->info->type == GAIM_PLUGIN_LOADER)
		{
			/* We'll just load this right now. */
			if (!gaim_plugin_load(plugin))
			{
				gaim_plugin_destroy(plugin);

				continue;
			}

			plugin_loaders = g_list_append(plugin_loaders, plugin);

			for (cur = GAIM_PLUGIN_LOADER_INFO(plugin)->exts;
				 cur != NULL;
				 cur = cur->next)
			{
				gaim_plugins_probe(cur->data);
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

			/* Make sure we don't load two PRPLs with the same name? */
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

	if (probe_cb != NULL)
		probe_cb(probe_cb_data);
#endif /* GAIM_PLUGINS */
}

gboolean
gaim_plugin_register(GaimPlugin *plugin)
{
	g_return_val_if_fail(plugin != NULL, FALSE);

	/* If this plugin has been registered already then exit */
	if (g_list_find(plugins, plugin))
		return TRUE;

	/* Ensure the plugin has the requisite information */
	if (plugin->info->type == GAIM_PLUGIN_LOADER)
	{
		GaimPluginLoaderInfo *loader_info;

		loader_info = GAIM_PLUGIN_LOADER_INFO(plugin);

		if (loader_info == NULL)
		{
			gaim_debug_error("plugins", "%s is not loadable, loader plugin missing loader_info\n",
							   plugin->path);
			return FALSE;
		}
	}
	else if (plugin->info->type == GAIM_PLUGIN_PROTOCOL)
	{
		GaimPluginProtocolInfo *prpl_info;

		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(plugin);

		if (prpl_info == NULL)
		{
			gaim_debug_error("plugins", "%s is not loadable, protocol plugin missing prpl_info\n",
							   plugin->path);
			return FALSE;
		}
	}

#ifdef GAIM_PLUGINS
	/* This plugin should be probed and maybe loaded--add it to the queue */
	load_queue = g_list_append(load_queue, plugin);
#else
	if (plugin->info != NULL)
	{
		if (plugin->info->type == GAIM_PLUGIN_PROTOCOL)
			protocol_plugins = g_list_insert_sorted(protocol_plugins, plugin,
													(GCompareFunc)compare_prpl);
		if (plugin->info->load != NULL)
			if (!plugin->info->load(plugin))
				return FALSE;
	}
#endif

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
#ifdef GAIM_PLUGINS
	GaimPlugin *plugin;
	GList *l;
	char *tmp;

	g_return_val_if_fail(basename != NULL, NULL);

	for (l = plugins; l != NULL; l = l->next)
	{
		plugin = (GaimPlugin *)l->data;

		if (plugin->path != NULL) {
			tmp = gaim_plugin_get_basename(plugin->path);
			if (!strcmp(tmp, basename))
			{
				g_free(tmp);
				return plugin;
			}
			g_free(tmp);
		}
	}

#endif /* GAIM_PLUGINS */

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
gaim_plugin_action_new(const char* label, void (*callback)(GaimPluginAction *))
{
	GaimPluginAction *act = g_new0(GaimPluginAction, 1);

	act->label = g_strdup(label);
	act->callback = callback;

	return act;
}

void
gaim_plugin_action_free(GaimPluginAction *action)
{
	g_return_if_fail(action != NULL);

	g_free(action->label);
	g_free(action);
}
