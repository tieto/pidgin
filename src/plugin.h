/**
 * @file plugin.h Plugin API
 * @ingroup core
 *
 * gaim
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _GAIM_PLUGIN_H_
#define _GAIM_PLUGIN_H_
#include <gmodule.h>

typedef enum   _GaimPluginType GaimPluginType;     /**< GaimPluginType   */
typedef struct _GaimPlugin     GaimPlugin;         /**< GaimPlugin       */
typedef struct _GaimPluginInfo GaimPluginInfo;     /**< GaimPluginInfo   */
typedef struct _GaimPluginLoaderInfo GaimPluginLoaderInfo;

typedef int GaimPluginPriority; /**< Plugin priority. */

/* XXX */
#include "config.h"
#include "event.h"

/**
 * Plugin types.
 */
enum _GaimPluginType
{
	GAIM_PLUGIN_UNKNOWN  = -1,  /**< Unknown type.    */
	GAIM_PLUGIN_STANDARD = 0,   /**< Standard plugin. */
	GAIM_PLUGIN_LOADER,         /**< Loader plugin.   */
	GAIM_PLUGIN_PROTOCOL        /**< Protocol plugin. */
};

#define GAIM_PRIORITY_DEFAULT     0
#define GAIM_PRIORITY_HIGHEST  9999
#define GAIM_PRIORITY_LOWEST  -9999

#define GAIM_PLUGIN_ID_UNSET { 0, 0, 0, 0 }

/**
 * Detailed information about a plugin.
 *
 * This is used in the version 2.0 API and up.
 */
struct _GaimPluginInfo
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
};

/**
 * Extra information for loader plugins.
 */
struct _GaimPluginLoaderInfo
{
	GList *exts;

	gboolean (*probe)(GaimPlugin *plugin);
	gboolean (*load)(GaimPlugin *plugin);
	gboolean (*unload)(GaimPlugin *plugin);
	void     (*destroy)(GaimPlugin *plugin);

	GaimSignalBroadcastFunc broadcast;
};

/**
 * A plugin handle.
 */
struct _GaimPlugin
{
	gboolean native_plugin;                /**< Native C plugin.          */
	gboolean loaded;                       /**< The loaded state.         */
	void *handle;                          /**< The module handle.        */
	char *path;                            /**< The path to the plugin.   */
	GaimPluginInfo *info;                  /**< The plugin information.   */
	char *error;
	void *extra;                           /**< Plugin-specific data.     */
};

#define GAIM_PLUGIN_LOADER_INFO(plugin) \
	((GaimPluginLoaderInfo *)(plugin)->info->extra_info)

/**
 * Handles the initialization of modules.
 */
#ifndef GAIM_PLUGINS
# define GAIM_INIT_PLUGIN(pluginname, initfunc, plugininfo) \
	gboolean gaim_init_##pluginname##_plugin(void) { \
		GaimPlugin *plugin = gaim_plugin_new(TRUE, NULL); \
		plugin->info = &(plugininfo); \
		initfunc((plugin)); \
		return gaim_plugin_register(plugin); \
	}
#else /* GAIM_PLUGINS */
# define GAIM_INIT_PLUGIN(pluginname, initfunc, plugininfo) \
	G_MODULE_EXPORT gboolean gaim_init_plugin(GaimPlugin *plugin) { \
		plugin->info = &(plugininfo); \
		initfunc((plugin)); \
		return gaim_plugin_register(plugin); \
	}
#endif

/**************************************************************************/
/** @name Plugin namespace                                                */
/**************************************************************************/
/*@{*/

/**
 * Creates a new plugin structure.
 *
 * @param native Whether or not the plugin is native.
 * @param path   The path to the plugin, or @c NULL if statically compiled.
 *
 * @return A new GaimPlugin structure.
 */
GaimPlugin *gaim_plugin_new(gboolean native, const char *path);

/**
 * Probes a plugin, retrieving the information on it and adding it to the
 * list of available plugins.
 *
 * @param filename The plugin's filename.
 *
 * @return The plugin handle.
 *
 * @see gaim_plugin_load()
 * @see gaim_plugin_destroy()
 */
GaimPlugin *gaim_plugin_probe(const char *filename);

/**
 * Registers a plugin and prepares it for loading.
 *
 * This shouldn't be called by anything but the internal module code.
 *
 * @param plugin The plugin to register.
 */
gboolean gaim_plugin_register(GaimPlugin *plugin);

/**
 * Attempts to load a previously probed plugin.
 *
 * @param filename The plugin's filename.
 *
 * @return @c TRUE if successful, or @c FALSE otherwise.
 *
 * @see gaim_plugin_reload()
 * @see gaim_plugin_unload()
 */
gboolean gaim_plugin_load(GaimPlugin *plugin);

/**
 * Unloads the specified plugin.
 *
 * @param plugin The plugin handle.
 *
 * @return @c TRUE if successful, or @c FALSE otherwise.
 *
 * @see gaim_plugin_load()
 * @see gaim_plugin_reload()
 */
gboolean gaim_plugin_unload(GaimPlugin *plugin);

/**
 * Reloads a plugin.
 *
 * @param plugin The old plugin handle.
 * 
 * @return @c TRUE if successful, or @c FALSE otherwise.
 *
 * @see gaim_plugin_load()
 * @see gaim_plugin_unload()
 */
gboolean gaim_plugin_reload(GaimPlugin *plugin);

/**
 * Unloads a plugin and destroys the structure from memory.
 *
 * @param plugin The plugin handle.
 */
void gaim_plugin_destroy(GaimPlugin *plugin);

/**
 * Returns whether or not a plugin is currently loaded.
 *
 * @param plugin The plugin.
 *
 * @return TRUE if loaded, or FALSE otherwise.
 */
gboolean gaim_plugin_is_loaded(const GaimPlugin *plugin);

/*@}*/

/**************************************************************************/
/** @name Plugins namespace                                               */
/**************************************************************************/
/*@{*/

/**
 * Sets the search paths for plugins.
 *
 * @param count The number of search paths.
 * @param paths The search paths.
 */
void gaim_plugins_set_search_paths(size_t count, char **paths);

/**
 * Unloads all loaded plugins.
 */
void gaim_plugins_unload_all(void);


/**
 * Destroys all registered plugins.
 */
void gaim_plugins_destroy_all(void);

/**
 * Probes for plugins in the registered module paths.
 *
 * @param ext The extension type to probe for, or @c NULL for all.
 *
 * @see gaim_plugin_set_probe_path()
 */
void gaim_plugins_probe(const char *ext);

/**
 * Returns whether or not plugin support is enabled.
 *
 * @return TRUE if plugin support is enabled, or FALSE otherwise.
 */
gboolean gaim_plugins_enabled(void);

/**
 * Registers a function that will be called when probing is finished.
 *
 * @param func The callback function.
 * @param data Data to pass to the callback.
 */
void gaim_plugins_register_probe_notify_cb(void (*func)(void *), void *data);

/**
 * Unregisters a function that would be called when probing is finished.
 *
 * @param func The callback function.
 */
void gaim_plugins_unregister_probe_notify_cb(void (*func)(void *));

/**
 * Registers a function that will be called when a plugin is loaded.
 *
 * @param func The callback functino.
 * @param data Data to pass to the callback.
 */
void gaim_plugins_register_load_notify_cb(void (*func)(GaimPlugin *, void *),
										  void *data);

/**
 * Unregisters a function that would be called when a plugin is loaded.
 *
 * @param func The callback functino.
 */
void gaim_plugins_unregister_load_notify_cb(void (*func)(GaimPlugin *, void *));

/**
 * Registers a function that will be called when a plugin is unloaded.
 *
 * @param func The callback functino.
 * @param data Data to pass to the callback.
 */
void gaim_plugins_register_unload_notify_cb(void (*func)(GaimPlugin *, void *),
											void *data);

/**
 * Unregisters a function that would be called when a plugin is unloaded.
 *
 * @param func The callback functino.
 */
void gaim_plugins_unregister_unload_notify_cb(void (*func)(GaimPlugin *,
														   void *));

/**
 * Finds a plugin with the specified name.
 *
 * @param name The plugin name.
 *
 * @return The plugin if found, or @c NULL if not found.
 */
GaimPlugin *gaim_plugins_find_with_name(const char *name);

/**
 * Finds a plugin with the specified filename.
 *
 * @param filename The plugin filename.
 *
 * @return The plugin if found, or @c NULL if not found.
 */
GaimPlugin *gaim_plugins_find_with_filename(const char *filename);

/**
 * Finds a plugin with the specified plugin ID.
 *
 * @param id The plugin ID.
 *
 * @return The plugin if found, or @c NULL if not found.
 */
GaimPlugin *gaim_plugins_find_with_id(const char *id);

/**
 * Returns a list of all loaded plugins.
 *
 * @return A list of all plugins.
 */
GList *gaim_plugins_get_loaded(void);

/**
 * Returns a list of all plugins, whether loaded or not.
 *
 * @return A list of all plugins.
 */
GList *gaim_plugins_get_all(void);

/*@}*/

#endif /* _GAIM_PLUGIN_H_ */
