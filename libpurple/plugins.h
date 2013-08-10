/**
 * @file plugins.h Plugins API
 * @ingroup core
 * @see @ref plugin-signals
 * @see @ref plugin-ids
 * @see @ref plugin-i18n
 */

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
#ifndef _PURPLE_PLUGINS_H_
#define _PURPLE_PLUGINS_H_

#ifdef PURPLE_PLUGINS
#include <gplugin.h>
#include <gplugin-native.h>
#else
#include <glib.h>
#include <glib-object.h>
#endif

#include "version.h"

#ifdef PURPLE_PLUGINS

#define PURPLE_TYPE_PLUGIN             GPLUGIN_TYPE_PLUGIN
#define PURPLE_PLUGIN(obj)             GPLUGIN_PLUGIN(obj)
#define PURPLE_PLUGIN_CLASS(klass)     GPLUGIN_PLUGIN_CLASS(klass)
#define PURPLE_IS_PLUGIN(obj)          GPLUGIN_IS_PLUGIN(obj)
#define PURPLE_IS_PLUGIN_CLASS(klass)  GPLUGIN_IS_PLUGIN_CLASS(klass)
#define PURPLE_PLUGIN_GET_CLASS(obj)   GPLUGIN_PLUGIN_GET_CLASS(obj)

/**
 * Represents a plugin handle.
 * This type is an alias for GPluginPlugin.
 */
typedef GPluginPlugin PurplePlugin;

/**
 * The base class for all #PurplePlugin's.
 * This type is an alias for GPluginPluginClass.
 */
typedef GPluginPluginClass PurplePluginClass;

#else /* !defined(PURPLE_PLUGINS) */

#define PURPLE_TYPE_PLUGIN             G_TYPE_OBJECT
#define PURPLE_PLUGIN(obj)             G_OBJECT(obj)
#define PURPLE_PLUGIN_CLASS(klass)     G_OBJECT_CLASS(klass)
#define PURPLE_IS_PLUGIN(obj)          G_IS_OBJECT(obj)
#define PURPLE_IS_PLUGIN_CLASS(klass)  G_IS_OBJECT_CLASS(klass)
#define PURPLE_PLUGIN_GET_CLASS(obj)   G_OBJECT_GET_CLASS(obj)

#define GPLUGIN_PLUGIN_INFO_FLAGS_LOAD_ON_QUERY 0
#define GPLUGIN_PLUGIN_INFO_FLAGS_INTERNAL      0

typedef GObject PurplePlugin;
typedef GObjectClass PurplePluginClass;

#endif /* PURPLE_PLUGINS */

#define PURPLE_TYPE_PLUGIN_INFO             (purple_plugin_info_get_type())
#define PURPLE_PLUGIN_INFO(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_PLUGIN_INFO, PurplePluginInfo))
#define PURPLE_PLUGIN_INFO_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_PLUGIN_INFO, PurplePluginInfoClass))
#define PURPLE_IS_PLUGIN_INFO(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_PLUGIN_INFO))
#define PURPLE_IS_PLUGIN_INFO_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_PLUGIN_INFO))
#define PURPLE_PLUGIN_INFO_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_PLUGIN_INFO, PurplePluginInfoClass))

/** @copydoc _PurplePluginInfo */
typedef struct _PurplePluginInfo PurplePluginInfo;
/** @copydoc _PurplePluginInfoClass */
typedef struct _PurplePluginInfoClass PurplePluginInfoClass;

#define PURPLE_TYPE_PLUGIN_ACTION  (purple_plugin_action_get_type())

/** @copydoc _PurplePluginAction */
typedef struct _PurplePluginAction PurplePluginAction;

#include "pluginpref.h"

typedef void (*PurplePluginActionCallback)(PurplePluginAction *);
typedef GList *(*PurplePluginGetActionsCallback)(PurplePlugin *);
typedef PurplePluginPrefFrame *(*PurplePluginPrefFrameCallback)(PurplePlugin *);

/**
 * Holds information about a plugin.
 */
struct _PurplePluginInfo {
#ifdef PURPLE_PLUGINS
	/*< private >*/
	GPluginPluginInfo parent;
#else
	/*< private >*/
	GObject parent;
#endif
};

/**
 * PurplePluginInfoClass:
 *
 * The base class for all #PurplePluginInfo's.
 */
struct _PurplePluginInfoClass {
#ifdef PURPLE_PLUGINS
	/*< private >*/
	GPluginPluginInfoClass parent_class;
#else
	/*< private >*/
	GObjectClass parent_class;
#endif

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * Represents an action that the plugin can perform. This shows up in the Tools
 * menu, under a submenu with the name of the plugin.
 */
struct _PurplePluginAction {
	char *label;
	PurplePluginActionCallback callback;
	PurplePlugin *plugin;
	gpointer user_data;
};

/** Returns an ABI version to set in plugins using major and minor versions */
#define PURPLE_PLUGIN_ABI_VERSION(major,minor) ((major << 16) + minor)
/** Returns the major version from an ABI version */
#define PURPLE_PLUGIN_ABI_MAJOR_VERSION(abi)   (abi >> 16)
/** Returns the minor version from an ABI version */
#define PURPLE_PLUGIN_ABI_MINOR_VERSION(abi)   (abi & 0xFFFF)

/**
  * A convenience‎ macro that returns an ABI version using PURPLE_MAJOR_VERSION
  * and PURPLE_MINOR_VERSION
  */
#define PURPLE_ABI_VERSION PURPLE_PLUGIN_ABI_VERSION(PURPLE_MAJOR_VERSION,\
                                                     PURPLE_MINOR_VERSION)

/**
 * Handles the initialization of modules.
 */
#if !defined(PURPLE_PLUGINS) || defined(PURPLE_STATIC_PRPL)
#define PURPLE_PLUGIN_INIT(pluginname,pluginquery,pluginload,pluginunload) \
	PurplePluginInfo * pluginname##_plugin_query(void); \
	PurplePluginInfo * pluginname##_plugin_query(void) { \
		return pluginquery(NULL); \
	} \
	gboolean pluginname##_plugin_load(void); \
	gboolean pluginname##_plugin_load(void) { \
		GError *e = NULL; \
		gboolean loaded = pluginload(NULL, &e); \
		if (e) g_error_free(e); \
		return loaded; \
	} \
	gboolean pluginname##_plugin_unload(void); \
	gboolean pluginname##_plugin_unload(void) { \
		GError *e = NULL; \
		gboolean unloaded = pluginunload(NULL, &e); \
		if (e) g_error_free(e); \
		return unloaded; \
	}
#else /* PURPLE_PLUGINS  && !PURPLE_STATIC_PRPL */
#define PURPLE_PLUGIN_INIT(pluginname,pluginquery,pluginload,pluginunload) \
	G_MODULE_EXPORT GPluginPluginInfo *gplugin_plugin_query(GError **e); \
	G_MODULE_EXPORT GPluginPluginInfo *gplugin_plugin_query(GError **e) { \
		return GPLUGIN_PLUGIN_INFO(pluginquery(e)); \
	} \
	G_MODULE_EXPORT gboolean gplugin_plugin_load(GPluginNativePlugin *p, GError **e); \
	G_MODULE_EXPORT gboolean gplugin_plugin_load(GPluginNativePlugin *p, GError **e) { \
		return pluginload(PURPLE_PLUGIN(p), e); \
	} \
	G_MODULE_EXPORT gboolean gplugin_plugin_unload(GPluginNativePlugin *p, GError **e); \
	G_MODULE_EXPORT gboolean gplugin_plugin_unload(GPluginNativePlugin *p, GError **e) { \
		return pluginunload(PURPLE_PLUGIN(p), e); \
	}
#endif

G_BEGIN_DECLS

/**************************************************************************/
/** @name Plugin API                                                      */
/**************************************************************************/
/*@{*/

/**
 * Attempts to load a plugin.
 *
 * @param plugin The plugin to load.
 *
 * @return @c TRUE if successful, or @c FALSE otherwise.
 *
 * @see purple_plugin_unload()
 */
gboolean purple_plugin_load(PurplePlugin *plugin);

/**
 * Unloads the specified plugin.
 *
 * @param plugin The plugin handle.
 *
 * @return @c TRUE if successful, or @c FALSE otherwise.
 *
 * @see purple_plugin_load()
 */
gboolean purple_plugin_unload(PurplePlugin *plugin);

/**
 * Returns whether or not a plugin is currently loaded.
 *
 * @param plugin The plugin.
 *
 * @return @c TRUE if loaded, or @c FALSE otherwise.
 */
gboolean purple_plugin_is_loaded(const PurplePlugin *plugin);

/**
 * Returns a plugin's filename, along with the path.
 *
 * @param info The plugin.
 *
 * @return The plugin's filename.
 */
const gchar *purple_plugin_get_filename(const PurplePlugin *plugin);

/**
 * Returns a plugin's #PurplePluginInfo instance.
 *
 * @param info The plugin.
 *
 * @return The plugin's #PurplePluginInfo instance.
 */
PurplePluginInfo *purple_plugin_get_info(const PurplePlugin *plugin);

/**
 * Disable a plugin.
 *
 * This function adds the plugin to a list of plugins to "disable at the next
 * startup" by excluding said plugins from the list of plugins to save.  The
 * UI needs to call purple_plugins_save_loaded() after calling this for it
 * to have any effect.
 */
void purple_plugin_disable(PurplePlugin *plugin);

/**
 * Registers a new dynamic type.
 *
 * @param plugin  The plugin that is registering the type.
 * @param parent  Type from which this type will be derived.
 * @param name    Name of the new type.
 * @param info    Information to initialize and destroy a type's classes and
 *                instances.
 * @param flags   Bitwise combination of values that determines the nature
 *                (e.g. abstract or not) of the type.
 *
 * @return The new GType, or @c G_TYPE_INVALID if registration failed.
 */
GType purple_plugin_register_type(PurplePlugin *plugin, GType parent,
                                  const gchar *name, const GTypeInfo *info,
                                  GTypeFlags flags);

/**
 * Adds a dynamic interface type to an instantiable type.
 *
 * @param plugin          The plugin that is adding the interface type.
 * @param instance_type   The GType of the instantiable type.
 * @param interface_type  The GType of the interface type.
 * @param interface_info  Information used to manage the interface type.
 */
void purple_plugin_add_interface(PurplePlugin *plugin, GType instance_type,
                                 GType interface_type,
                                 const GInterfaceInfo *interface_info);

/**
 * Returns whether a plugin is an internal plugin. Internal plugins provide
 * required additional functionality to the libpurple core. These plugins must
 * not be shown in plugin lists. Examples of such plugins are in-tree protocol
 * plugins, loaders etc.
 *
 * @param plugin The plugin.
 *
 * @return @c TRUE if the plugin is an internal plugin, @c FALSE otherwise.
 */
gboolean purple_plugin_is_internal(const PurplePlugin *plugin);

/**
 * Returns whether or not a plugin is loadable.
 *
 * If this returns @c FALSE, the plugin is guaranteed to not
 * be loadable. However, a return value of @c TRUE does not
 * guarantee the plugin is loadable.
 * An error is set if the plugin is not loadable.
 *
 * @param plugin The plugin.
 *
 * @return @c TRUE if the plugin may be loadable, @c FALSE if the plugin is not
 *         loadable.
 *
 * @see purple_plugin_get_error()
 */
gboolean purple_plugin_is_loadable(const PurplePlugin *plugin);

/**
 * If a plugin is not loadable, this returns the reason.
 *
 * @param plugin The plugin.
 *
 * @return The reason why the plugin is not loadable.
 */
const gchar *purple_plugin_get_error(const PurplePlugin *plugin);

/**
 * Returns a list of plugins that depend on a particular plugin.
 *
 * @param plugin The plugin whose dependent plugins are returned.
 *
 * @constreturn The list of a plugins that depend on the specified plugin.
 */
GSList *purple_plugin_get_dependent_plugins(const PurplePlugin *plugin);

/*@}*/

/**************************************************************************/
/** @name PluginInfo API                                                  */
/**************************************************************************/
/*@{*/

/**
 * Returns the GType for the PurplePluginInfo object.
 */
GType purple_plugin_info_get_type(void);

/**
 * Creates a new #PurplePluginInfo instance to be returned from
 * gplugin_plugin_query() of a plugin, using the provided name/value pairs.
 *
 * All properties except "id" and "purple-abi" are optional.
 *
 * Valid property names are:                                                 \n
 * "id"                 (string) The ID of the plugin.                       \n
 * "name"               (string) The translated name of the plugin.          \n
 * "version"            (string) Version of the plugin.                      \n
 * "category"           (string) Primary category of the plugin.             \n
 * "summary"            (string) Brief summary of the plugin.                \n
 * "description"        (string) Full description of the plugin.             \n
 * "author"             (string) Name and e-mail address of the author.
 *                               format: First Last <user@domain.com>        \n
 * "website"            (string) Website of the plugin.                      \n
 * "icon"               (string) Path to a plugin's icon.                    \n
 * "license"            (string) Short name of the plugin's license. This
 *                               should either be an identifier of the
 *                               license from http://spdx.org/licenses/ or
 *                               "Other" for custom licenses.                \n
 * "license-text"       (string) The text of the plugin's license, if
 *                               unlisted on SPDX.                           \n
 * "license-url"        (string) The plugin's license URL, if unlisted on
 *                               SPDX.                                       \n
 * "purple-abi"         (guint32) The purple ABI version required by plugin. \n
 * "dependencies"       (GSList) List of plugin IDs required by the plugin.  \n
 * "get-actions"        (PurplePluginGetActionsCallback) Callback that
 *                               returns a list of actions the plugin can
 *                               perform.                                    \n
 * "preferences-frame"  (PurplePluginPrefFrameCallback) Callback that returns
 *                               a preferences frame for the plugin.
 *
 * Additionally, you can provide a "flags" property if the plugin is to be
 * distributed with libpurple, the value for which should be a bitwise
 * combination of:                                                           \n
 * GPLUGIN_PLUGIN_INFO_FLAGS_INTERNAL: Internal plugin, not shown in lists.  \n
 * GPLUGIN_PLUGIN_INFO_FLAGS_LOAD_ON_QUERY: Auto-load on query.              \n
 *
 * @param first_property  The first property name
 * @param ...  The value of the first property, followed optionally by more
 *             name/value pairs, followed by @c NULL
 *
 * @return A new #PurplePluginInfo instance.
 *
 * @see PURPLE_PLUGIN_ABI_VERSION
 * @see @ref plugin-ids
 */
PurplePluginInfo *purple_plugin_info_new(const char *first_property, ...);

/**
 * Returns a plugin's ID.
 *
 * @param info The plugin's info instance.
 *
 * @return The plugin's ID.
 */
const gchar *purple_plugin_info_get_id(const PurplePluginInfo *info);

/**
 * Returns a plugin's translated name.
 *
 * @param info The plugin's info instance.
 *
 * @return The name of the plugin, or @c NULL.
 */
const gchar *purple_plugin_info_get_name(const PurplePluginInfo *info);

/**
 * Returns a plugin's version.
 *
 * @param info The plugin's info instance.
 *
 * @return The version of the plugin, or @c NULL.
 */
const gchar *purple_plugin_info_get_version(const PurplePluginInfo *info);

/**
 * Returns a plugin's primary category.
 *
 * @param info The plugin's info instance.
 *
 * @return The primary category of the plugin, or @c NULL.
 */
const gchar *purple_plugin_info_get_category(const PurplePluginInfo *info);

/**
 * Returns a plugin's summary.
 *
 * @param info The plugin's info instance.
 *
 * @return The summary of the plugin, or @c NULL.
 */
const gchar *purple_plugin_info_get_summary(const PurplePluginInfo *info);

/**
 * Returns a plugin's description.
 *
 * @param info The plugin's info instance.
 *
 * @return The description of the plugin, or @c NULL.
 */
const gchar *purple_plugin_info_get_description(const PurplePluginInfo *info);

/**
 * Returns a plugin's author.
 *
 * @param info The plugin's info instance.
 *
 * @return The author of the plugin, or @c NULL.
 */
const gchar *purple_plugin_info_get_author(const PurplePluginInfo *info);

/**
 * Returns a plugin's website.
 *
 * @param info The plugin's info instance.
 *
 * @return The website of the plugin, or @c NULL.
 */
const gchar *purple_plugin_info_get_website(const PurplePluginInfo *info);

/**
 * Returns the path to a plugin's icon.
 *
 * @param info The plugin's info instance.
 *
 * @return The path to the plugin's icon, or @c NULL.
 */
const gchar *purple_plugin_info_get_icon(const PurplePluginInfo *info);

/**
 * Returns a short name of the plugin's license.
 *
 * @param info The plugin's info instance.
 *
 * @return The license name of the plugin, or @c NULL.
 */
const gchar *purple_plugin_info_get_license(const PurplePluginInfo *info);

/**
 * Returns the text of a plugin's license.
 *
 * @param info The plugin's info instance.
 *
 * @return The license text of the plugin, or @c NULL.
 */
const gchar *purple_plugin_info_get_license_text(const PurplePluginInfo *info);

/**
 * Returns the URL of a plugin's license.
 *
 * @param info The plugin's info instance.
 *
 * @return The license URL of the plugin, or @c NULL.
 */
const gchar *purple_plugin_info_get_license_url(const PurplePluginInfo *info);

/**
 * Returns the required purple ABI version for a plugin.
 *
 * @param info The plugin's info instance.
 *
 * @return The required purple ABI version for the plugin.
 */
guint32 purple_plugin_info_get_abi_version(const PurplePluginInfo *info);

/**
 * Returns a list of plugins that a particular plugin depends on.
 *
 * @param info The plugin's info instance.
 *
 * @constreturn The list of dependencies of a plugin.
 */
GSList *purple_plugin_info_get_dependencies(const PurplePluginInfo *info);

/**
 * Returns the callback that retrieves the list of actions a plugin can perform
 * at that moment.
 *
 * @param info The plugin info to get the callback from.
 *
 * @constreturn The callback that returns a list of #PurplePluginAction
 *              instances corresponding to the actions a plugin can perform.
 */
PurplePluginGetActionsCallback
purple_plugin_info_get_actions_callback(const PurplePluginInfo *info);

/**
 * Returns the callback that retrieves the preferences frame for a plugin.
 *
 * @param info The plugin info to get the callback from.
 *
 * @return The callback that returns the preferences frame.
 */
PurplePluginPrefFrameCallback
purple_plugin_info_get_pref_frame_callback(const PurplePluginInfo *info);

/*@}*/

/**************************************************************************/
/** @name PluginAction API                                                */
/**************************************************************************/
/*@{*/

/**
 * Returns the GType for the PurplePluginAction boxed structure.
 */
GType purple_plugin_action_get_type(void);

/**
 * Allocates and returns a new PurplePluginAction. Use this to add actions in a
 * list in the "get-actions" callback for your plugin.
 *
 * @param label    The description of the action to show to the user.
 * @param callback The callback to call when the user selects this action.
 */
PurplePluginAction *purple_plugin_action_new(const char* label,
		PurplePluginActionCallback callback);

/**
 * Frees a PurplePluginAction
 *
 * @param action The PurplePluginAction to free.
 */
void purple_plugin_action_free(PurplePluginAction *action);

/*@}*/

/**************************************************************************/
/** @name Plugins API                                                     */
/**************************************************************************/
/*@{*/

/**
 * Returns a list of all plugins, whether loaded or not.
 *
 * @return A list of all plugins. The list is owned by the caller, and must be
 *         g_list_free()d to avoid leaking the nodes.
 */
GList *purple_plugins_find_all(void);

/**
 * Returns a list of all loaded plugins.
 *
 * @constreturn A list of all loaded plugins.
 */
GList *purple_plugins_get_loaded(void);

/**
 * Add a new directory to search for plugins
 *
 * @param path The new search path.
 */
void purple_plugins_add_search_path(const gchar *path);

/**
 * Forces a refresh of all plugins found in the search paths, and loads plugins
 * that are to be loaded on query.
 *
 * @see purple_plugins_add_search_path()
 */
void purple_plugins_refresh(void);

/**
 * Finds a plugin with the specified plugin ID.
 *
 * @param id The plugin ID.
 *
 * @return The plugin if found, or @c NULL if not found.
 */
PurplePlugin *purple_plugins_find_plugin(const gchar *id);

/**
 * Finds a plugin with the specified filename (filename with a path).
 *
 * @param filename The plugin filename.
 *
 * @return The plugin if found, or @c NULL if not found.
 */
PurplePlugin *purple_plugins_find_by_filename(const char *filename);

/**
 * Saves the list of loaded plugins to the specified preference key.
 * Plugins that are set to load on query are not saved.
 *
 * @param key The preference key to save the list of plugins to.
 */
void purple_plugins_save_loaded(const char *key);

/**
 * Attempts to load all the plugins in the specified preference key
 * that were loaded when purple last quit.
 *
 * @param key The preference key containing the list of plugins.
 */
void purple_plugins_load_saved(const char *key);

/*@}*/

/**************************************************************************/
/** @name Plugins Subsystem API                                           */
/**************************************************************************/
/*@{*/

/**
 * Returns the plugin subsystem handle.
 *
 * @return The plugin sybsystem handle.
 */
void *purple_plugins_get_handle(void);

/**
 * Initializes the plugin subsystem
 */
void purple_plugins_init(void);

/**
 * Uninitializes the plugin subsystem
 */
void purple_plugins_uninit(void);

/*@}*/

G_END_DECLS

#endif /* _PURPLE_PLUGINS_H_ */
