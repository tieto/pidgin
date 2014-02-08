/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#ifndef _PURPLE_PLUGIN_H_
#define _PURPLE_PLUGIN_H_
/**
 * SECTION:plugin
 * @section_id: libpurple-plugin
 * @short_description: <filename>plugin.h</filename>
 * @title: Plugin API
 * @see_also: <link linkend="chapter-signals-plugin">Plugin signals</link>,
 *     <link linkend="chapter-plugin-ids">Plugin IDs</link>,
 *     <link linkend="chapter-plugin-i18n">Third Party Plugin Translation</link>
 */

#include <glib.h>
#include <gmodule.h>
#include "signals.h"

#define PURPLE_TYPE_PLUGIN  (purple_plugin_get_type())

typedef struct _PurplePlugin           PurplePlugin;
typedef struct _PurplePluginInfo       PurplePluginInfo;
typedef struct _PurplePluginUiInfo     PurplePluginUiInfo;
typedef struct _PurplePluginLoaderInfo PurplePluginLoaderInfo;
typedef struct _PurplePluginAction     PurplePluginAction;

/**
 * PurplePluginPriority:
 *
 * Plugin priority.
 */
typedef int PurplePluginPriority;

#include "pluginpref.h"

/**
 * PurplePluginType:
 * @PURPLE_PLUGIN_UNKNOWN:  Unknown type.
 * @PURPLE_PLUGIN_STANDARD: Standard plugin.
 * @PURPLE_PLUGIN_LOADER:   Loader plugin.
 * @PURPLE_PLUGIN_PROTOCOL: Protocol plugin.
 *
 * Plugin types.
 */
typedef enum
{
	PURPLE_PLUGIN_UNKNOWN  = -1,
	PURPLE_PLUGIN_STANDARD = 0,
	PURPLE_PLUGIN_LOADER,
	PURPLE_PLUGIN_PROTOCOL

} PurplePluginType;

#define PURPLE_PRIORITY_DEFAULT     0
#define PURPLE_PRIORITY_HIGHEST  9999
#define PURPLE_PRIORITY_LOWEST  -9999

#define PURPLE_PLUGIN_FLAG_INVISIBLE 0x01

#define PURPLE_PLUGIN_MAGIC 5 /* once we hit 6.0.0 I think we can remove this */

/**
 * PurplePluginInfo:
 * @load:       If a plugin defines a @load function, and it returns %FALSE,
 *              then the plugin will not be loaded.
 * @ui_info:    Used only by UI-specific plugins to build a preference screen
 *              with a custom UI.
 * @prefs_info: Used by any plugin to display preferences. If @ui_info has been
 *              specified, this will be ignored.
 * @actions:    This callback has a different use depending on whether this
 *              plugin type is #PURPLE_PLUGIN_STANDARD or
 *              #PURPLE_PLUGIN_PROTOCOL.
 *              <sbr/>If #PURPLE_PLUGIN_STANDARD then the list of actions will
 *              show up in the Tools menu, under a submenu with the name of the
 *              plugin. @context will be NULL.
 *              <sbr/>If PURPLE_PLUGIN_PROTOCOL then the list of actions will
 *              show up in the Accounts menu, under a submenu with the name of
 *              the account. @context will be set to the #PurpleConnection for
 *              that account. This callback will only be called for online
 *              accounts.
 *
 * Detailed information about a plugin.
 *
 * This is used in the version 2.0 API and up.
 */
struct _PurplePluginInfo
{
	unsigned int magic;
	unsigned int major_version;
	unsigned int minor_version;
	PurplePluginType type;
	char *ui_requirement;
	unsigned long flags;
	GList *dependencies;
	PurplePluginPriority priority;

	const char *id;
	const char *name;
	const char *version;
	const char *summary;
	const char *description;
	const char *author;
	const char *homepage;

	gboolean (*load)(PurplePlugin *plugin);
	gboolean (*unload)(PurplePlugin *plugin);
	void (*destroy)(PurplePlugin *plugin);

	void *ui_info;
	void *extra_info;
	PurplePluginUiInfo *prefs_info;

	GList *(*actions)(PurplePlugin *plugin, gpointer context);

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * PurplePluginLoaderInfo:
 *
 * Extra information for loader plugins.
 */
struct _PurplePluginLoaderInfo
{
	GList *exts;

	gboolean (*probe)(PurplePlugin *plugin);
	gboolean (*load)(PurplePlugin *plugin);
	gboolean (*unload)(PurplePlugin *plugin);
	void     (*destroy)(PurplePlugin *plugin);

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * PurplePlugin:
 * @native_plugin:     Native C plugin.
 * @loaded:            The loaded state.
 * @handle:            The module handle.
 * @path:              The path to the plugin.
 * @info:              The plugin information.
 * @ipc_data:          IPC data.
 * @extra:             Plugin-specific data.
 * @unloadable:        Unloadable
 * @dependent_plugins: Plugins depending on this
 * @ui_data:           The UI data.
 *
 * A plugin handle.
 */
struct _PurplePlugin
{
	gboolean native_plugin;
	gboolean loaded;
	void *handle;
	char *path;
	PurplePluginInfo *info;
	char *error;
	void *ipc_data;
	void *extra;
	gboolean unloadable;
	GList *dependent_plugins;
	gpointer ui_data;

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

#define PURPLE_PLUGIN_LOADER_INFO(plugin) \
	((PurplePluginLoaderInfo *)(plugin)->info->extra_info)

struct _PurplePluginUiInfo {
	PurplePluginPrefFrame *(*get_plugin_pref_frame)(PurplePlugin *plugin);
	gpointer (*get_plugin_pref_request)(PurplePlugin *plugin);

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

#define PURPLE_PLUGIN_HAS_PREF_FRAME(plugin) \
	((plugin)->info != NULL && (plugin)->info->prefs_info != NULL)

#define PURPLE_PLUGIN_UI_INFO(plugin) \
	((PurplePluginUiInfo*)(plugin)->info->prefs_info)


/**
 * PurplePluginAction:
 * @plugin: set to the owning plugin
 * @context: NULL for plugin actions menu, set to the PurpleConnection for
 *           account actions menu
 *
 * The structure used in the actions member of PurplePluginInfo
 */
struct _PurplePluginAction {
	char *label;
	void (*callback)(PurplePluginAction *);

	/** set to the owning plugin */
	PurplePlugin *plugin;

	/** NULL for plugin actions menu, set to the PurpleConnection for
	    account actions menu */
	gpointer context;

	gpointer user_data;
};

#define PURPLE_PLUGIN_HAS_ACTIONS(plugin) \
	((plugin)->info != NULL && (plugin)->info->actions != NULL)

#define PURPLE_PLUGIN_ACTIONS(plugin, context) \
	(PURPLE_PLUGIN_HAS_ACTIONS(plugin)? \
	(plugin)->info->actions(plugin, context): NULL)


/**
 * PURPLE_INIT_PLUGIN:
 *
 * Handles the initialization of modules.
 */
#if !defined(PURPLE_PLUGINS) || defined(PURPLE_STATIC_PRPL)
# define _FUNC_NAME(x) purple_init_##x##_plugin
# define PURPLE_INIT_PLUGIN(pluginname, initfunc, plugininfo) \
	gboolean _FUNC_NAME(pluginname)(void);\
	gboolean _FUNC_NAME(pluginname)(void) { \
		PurplePlugin *plugin = purple_plugin_new(TRUE, NULL); \
		plugin->info = &(plugininfo); \
		initfunc((plugin)); \
		purple_plugin_load((plugin)); \
		return purple_plugin_register(plugin); \
	}
#else /* PURPLE_PLUGINS  && !PURPLE_STATIC_PRPL */
# define PURPLE_INIT_PLUGIN(pluginname, initfunc, plugininfo) \
	G_MODULE_EXPORT gboolean purple_init_plugin(PurplePlugin *plugin); \
	G_MODULE_EXPORT gboolean purple_init_plugin(PurplePlugin *plugin) { \
		plugin->info = &(plugininfo); \
		initfunc((plugin)); \
		return purple_plugin_register(plugin); \
	}
#endif


G_BEGIN_DECLS

/**************************************************************************/
/* Plugin API                                                             */
/**************************************************************************/

/**
 * purple_plugin_get_type:
 *
 * Returns: The #GType for the #PurplePlugin boxed structure.
 */
/* TODO Boxing of PurplePlugin is a temporary solution to having a GType for
 *      plugins. This should rather be a GObject instead of a GBoxed.
 */
GType purple_plugin_get_type(void);

/**
 * purple_plugin_new:
 * @native: Whether or not the plugin is native.
 * @path:   The path to the plugin, or %NULL if statically compiled.
 *
 * Creates a new plugin structure.
 *
 * Returns: A new PurplePlugin structure.
 */
PurplePlugin *purple_plugin_new(gboolean native, const char *path);

/**
 * purple_plugin_probe:
 * @filename: The plugin's filename.
 *
 * Probes a plugin, retrieving the information on it and adding it to the
 * list of available plugins.
 *
 * See purple_plugin_load(), purple_plugin_destroy().
 *
 * Returns: The plugin handle.
 */
PurplePlugin *purple_plugin_probe(const char *filename);

/**
 * purple_plugin_register:
 * @plugin: The plugin to register.
 *
 * Registers a plugin and prepares it for loading.
 *
 * This shouldn't be called by anything but the internal module code.
 * Plugins should use the PURPLE_INIT_PLUGIN() macro to register themselves
 * with the core.
 *
 * Returns: %TRUE if the plugin was registered successfully.  Otherwise
 *         %FALSE is returned (this happens if the plugin does not contain
 *         the necessary information).
 */
gboolean purple_plugin_register(PurplePlugin *plugin);

/**
 * purple_plugin_load:
 * @plugin: The plugin to load.
 *
 * Attempts to load a previously probed plugin.
 *
 * See purple_plugin_reload(), purple_plugin_unload().
 *
 * Returns: %TRUE if successful, or %FALSE otherwise.
 */
gboolean purple_plugin_load(PurplePlugin *plugin);

/**
 * purple_plugin_unload:
 * @plugin: The plugin handle.
 *
 * Unloads the specified plugin.
 *
 * See purple_plugin_load(), purple_plugin_reload().
 *
 * Returns: %TRUE if successful, or %FALSE otherwise.
 */
gboolean purple_plugin_unload(PurplePlugin *plugin);

/**
 * purple_plugin_disable:
 *
 * Disable a plugin.
 *
 * This function adds the plugin to a list of plugins to "disable at the next
 * startup" by excluding said plugins from the list of plugins to save.  The
 * UI needs to call purple_plugins_save_loaded() after calling this for it
 * to have any effect.
 */
void purple_plugin_disable(PurplePlugin *plugin);

/**
 * purple_plugin_reload:
 * @plugin: The old plugin handle.
 *
 * Reloads a plugin.
 *
 * See purple_plugin_load(), purple_plugin_unload().
 *
 * Returns: %TRUE if successful, or %FALSE otherwise.
 */
gboolean purple_plugin_reload(PurplePlugin *plugin);

/**
 * purple_plugin_destroy:
 * @plugin: The plugin handle.
 *
 * Unloads a plugin and destroys the structure from memory.
 */
void purple_plugin_destroy(PurplePlugin *plugin);

/**
 * purple_plugin_is_loaded:
 * @plugin: The plugin.
 *
 * Returns whether or not a plugin is currently loaded.
 *
 * Returns: %TRUE if loaded, or %FALSE otherwise.
 */
gboolean purple_plugin_is_loaded(const PurplePlugin *plugin);

/**
 * purple_plugin_is_unloadable:
 * @plugin: The plugin.
 *
 * Returns whether or not a plugin is unloadable.
 *
 * If this returns %TRUE, the plugin is guaranteed to not
 * be loadable. However, a return value of %FALSE does not
 * guarantee the plugin is loadable.
 *
 * Returns: %TRUE if the plugin is known to be unloadable,\
 *         %FALSE otherwise
 */
gboolean purple_plugin_is_unloadable(const PurplePlugin *plugin);

/**
 * purple_plugin_get_id:
 * @plugin: The plugin.
 *
 * Returns a plugin's id.
 *
 * Returns: The plugin's id.
 */
const gchar *purple_plugin_get_id(const PurplePlugin *plugin);

/**
 * purple_plugin_get_name:
 * @plugin: The plugin.
 *
 * Returns a plugin's name.
 *
 * Returns: THe name of the plugin, or %NULL.
 */
const gchar *purple_plugin_get_name(const PurplePlugin *plugin);

/**
 * purple_plugin_get_version:
 * @plugin: The plugin.
 *
 * Returns a plugin's version.
 *
 * Returns: The plugin's version or %NULL.
 */
const gchar *purple_plugin_get_version(const PurplePlugin *plugin);

/**
 * purple_plugin_get_summary:
 * @plugin: The plugin.
 *
 * Returns a plugin's summary.
 *
 * Returns: The plugin's summary.
 */
const gchar *purple_plugin_get_summary(const PurplePlugin *plugin);

/**
 * purple_plugin_get_description:
 * @plugin: The plugin.
 *
 * Returns a plugin's description.
 *
 * Returns: The plugin's description.
 */
const gchar *purple_plugin_get_description(const PurplePlugin *plugin);

/**
 * purple_plugin_get_author:
 * @plugin: The plugin.
 *
 * Returns a plugin's author.
 *
 * Returns: The plugin's author.
 */
const gchar *purple_plugin_get_author(const PurplePlugin *plugin);

/**
 * purple_plugin_get_homepage:
 * @plugin: The plugin.
 *
 * Returns a plugin's homepage.
 *
 * Returns: The plugin's homepage.
 */
const gchar *purple_plugin_get_homepage(const PurplePlugin *plugin);

/**************************************************************************/
/* Plugin IPC API                                                         */
/**************************************************************************/

/**
 * purple_plugin_ipc_register:
 * @plugin:     The plugin to register the command with.
 * @command:    The name of the command.
 * @func:       The function to execute.
 * @marshal:    The marshalling function.
 * @ret_type:   The return type.
 * @num_params: The number of parameters.
 * @...:        The parameter types.
 *
 * Registers an IPC command in a plugin.
 *
 * Returns: TRUE if the function was registered successfully, or
 *         FALSE otherwise.
 */
gboolean purple_plugin_ipc_register(PurplePlugin *plugin, const char *command,
								  PurpleCallback func,
								  PurpleSignalMarshalFunc marshal,
								  GType ret_type, int num_params, ...);

/**
 * purple_plugin_ipc_unregister:
 * @plugin:  The plugin to unregister the command from.
 * @command: The name of the command.
 *
 * Unregisters an IPC command in a plugin.
 */
void purple_plugin_ipc_unregister(PurplePlugin *plugin, const char *command);

/**
 * purple_plugin_ipc_unregister_all:
 * @plugin: The plugin to unregister the commands from.
 *
 * Unregisters all IPC commands in a plugin.
 */
void purple_plugin_ipc_unregister_all(PurplePlugin *plugin);

/**
 * purple_plugin_ipc_get_types:
 * @plugin:      The plugin.
 * @command:     The name of the command.
 * @ret_type:    The returned return type.
 * @num_params:  The returned number of parameters.
 * @param_types: The returned list of parameter types.
 *
 * Returns a list of value types used for an IPC command.
 *
 * Returns: TRUE if the command was found, or FALSE otherwise.
 */
gboolean purple_plugin_ipc_get_types(PurplePlugin *plugin, const char *command,
									GType *ret_type, int *num_params,
									GType **param_types);

/**
 * purple_plugin_ipc_call:
 * @plugin:  The plugin to execute the command on.
 * @command: The name of the command.
 * @ok:      TRUE if the call was successful, or FALSE otherwise.
 * @...:     The parameters to pass.
 *
 * Executes an IPC command.
 *
 * Returns: The return value, which will be NULL if the command doesn't
 *         return a value.
 */
void *purple_plugin_ipc_call(PurplePlugin *plugin, const char *command,
						   gboolean *ok, ...);

/**************************************************************************/
/* Plugins API                                                            */
/**************************************************************************/

/**
 * purple_plugins_add_search_path:
 * @path: The new search path.
 *
 * Add a new directory to search for plugins
 */
void purple_plugins_add_search_path(const char *path);

/**
 * purple_plugins_get_search_paths:
 *
 * Returns a list of plugin search paths.
 *
 * Returns: (transfer none): A list of searched paths.
 */
GList *purple_plugins_get_search_paths(void);

/**
 * purple_plugins_unload_all:
 *
 * Unloads all loaded plugins.
 */
void purple_plugins_unload_all(void);

/**
 * purple_plugins_unload:
 *
 * Unloads all plugins of a specific type.
 */
void purple_plugins_unload(PurplePluginType type);

/**
 * purple_plugins_destroy_all:
 *
 * Destroys all registered plugins.
 */
void purple_plugins_destroy_all(void);

/**
 * purple_plugins_save_loaded:
 * @key: The preference key to save the list of plugins to.
 *
 * Saves the list of loaded plugins to the specified preference key
 */
void purple_plugins_save_loaded(const char *key);

/**
 * purple_plugins_load_saved:
 * @key: The preference key containing the list of plugins.
 *
 * Attempts to load all the plugins in the specified preference key
 * that were loaded when purple last quit.
 */
void purple_plugins_load_saved(const char *key);

/**
 * purple_plugins_probe:
 * @ext: The extension type to probe for, or %NULL for all.
 *
 * Probes for plugins in the registered module paths.
 *
 * See purple_plugins_add_search_path().
 */
void purple_plugins_probe(const char *ext);

/**
 * purple_plugins_enabled:
 *
 * Returns whether or not plugin support is enabled.
 *
 * Returns: TRUE if plugin support is enabled, or FALSE otherwise.
 */
gboolean purple_plugins_enabled(void);

/**
 * purple_plugins_find_with_name:
 * @name: The plugin name.
 *
 * Finds a plugin with the specified name.
 *
 * Returns: The plugin if found, or %NULL if not found.
 */
PurplePlugin *purple_plugins_find_with_name(const char *name);

/**
 * purple_plugins_find_with_filename:
 * @filename: The plugin filename.
 *
 * Finds a plugin with the specified filename (filename with a path).
 *
 * Returns: The plugin if found, or %NULL if not found.
 */
PurplePlugin *purple_plugins_find_with_filename(const char *filename);

/**
 * purple_plugins_find_with_basename:
 * @basename: The plugin basename.
 *
 * Finds a plugin with the specified basename (filename without a path).
 *
 * Returns: The plugin if found, or %NULL if not found.
 */
PurplePlugin *purple_plugins_find_with_basename(const char *basename);

/**
 * purple_plugins_find_with_id:
 * @id: The plugin ID.
 *
 * Finds a plugin with the specified plugin ID.
 *
 * Returns: The plugin if found, or %NULL if not found.
 */
PurplePlugin *purple_plugins_find_with_id(const char *id);

/**
 * purple_plugins_get_loaded:
 *
 * Returns a list of all loaded plugins.
 *
 * Returns: (transfer none): A list of all loaded plugins.
 */
GList *purple_plugins_get_loaded(void);

/**
 * purple_plugins_get_protocols:
 *
 * Returns a list of all valid protocol plugins.  A protocol
 * plugin is considered invalid if it does not contain the call
 * to the PURPLE_INIT_PLUGIN() macro, or if it was compiled
 * against an incompatable API version.
 *
 * Returns: (transfer none): A list of all protocol plugins.
 */
GList *purple_plugins_get_protocols(void);

/**
 * purple_plugins_get_all:
 *
 * Returns a list of all plugins, whether loaded or not.
 *
 * Returns: (transfer none): A list of all plugins.
 */
GList *purple_plugins_get_all(void);

/**************************************************************************/
/* Plugins SubSytem API                                                   */
/**************************************************************************/

/**
 * purple_plugins_get_handle:
 *
 * Returns the plugin subsystem handle.
 *
 * Returns: The plugin sybsystem handle.
 */
void *purple_plugins_get_handle(void);

/**
 * purple_plugins_init:
 *
 * Initializes the plugin subsystem
 */
void purple_plugins_init(void);

/**
 * purple_plugins_uninit:
 *
 * Uninitializes the plugin subsystem
 */
void purple_plugins_uninit(void);

/**
 * purple_plugin_action_new:
 * @label:    The description of the action to show to the user.
 * @callback: The callback to call when the user selects this action.
 *
 * Allocates and returns a new PurplePluginAction.
 */
PurplePluginAction *purple_plugin_action_new(const char* label, void (*callback)(PurplePluginAction *));

/**
 * purple_plugin_action_free:
 * @action: The PurplePluginAction to free.
 *
 * Frees a PurplePluginAction
 */
void purple_plugin_action_free(PurplePluginAction *action);

G_END_DECLS

#endif /* _PURPLE_PLUGIN_H_ */
