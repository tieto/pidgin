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

#include <gplugin.h>

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

typedef void (*PurplePluginActionCallback)(PurplePluginAction *);

#include "pluginpref.h"

/**
 * Holds information about a plugin.
 */
struct _PurplePluginInfo {
	/*< private >*/
	GPluginPluginInfo parent;
};

/**
 * PurplePluginInfoClass:
 *
 * The base class for all #PurplePluginInfo's.
 */
struct _PurplePluginInfoClass {
	/*< private >*/
	GPluginPluginInfoClass parent_class;

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
	GPluginPlugin *plugin;
};

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
gboolean purple_plugin_load(GPluginPlugin *plugin);

/**
 * Unloads the specified plugin.
 *
 * @param plugin The plugin handle.
 *
 * @return @c TRUE if successful, or @c FALSE otherwise.
 *
 * @see purple_plugin_load()
 */
gboolean purple_plugin_unload(GPluginPlugin *plugin);

/**
 * Returns whether or not a plugin is currently loaded.
 *
 * @param plugin The plugin.
 *
 * @return @c TRUE if loaded, or @c FALSE otherwise.
 */
gboolean purple_plugin_is_loaded(const GPluginPlugin *plugin);

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
 * Returns a list of actions that a plugin can perform.
 *
 * @param plugin_info The plugin info to get the actions from.
 *
 * @return A list of #PurplePluginAction instances corresponding to the actions
 *         a plugin can perform.
 *
 * @see purple_plugin_actions_add()
 */
GList *purple_plugin_info_get_actions(PurplePluginInfo *plugin_info);

/*@}*/

/**************************************************************************/
/** @name Plugin actions API                                              */
/**************************************************************************/
/*@{*/

/**
 * Returns the GType for the PurplePluginAction boxed structure.
 */
GType purple_plugin_action_get_type(void);

/**
 * Adds a new action to a plugin.
 *
 * @param plugin   The plugin to add the action to.
 * @param label    The description of the action to show to the user.
 * @param callback The callback to call when the user selects this action.
 */
void purple_plugin_actions_add(GPluginPlugin *plugin, const char* label,
                               PurplePluginActionCallback callback);

/*@}*/

/**************************************************************************/
/** @name Plugins API                                                     */
/**************************************************************************/
/*@{*/

/**
 * Returns a list of all plugins, whether loaded or not.
 *
 * @constreturn A list of all plugins.
 */
GList *purple_plugins_get_all(void);

/**
 * Returns a list of all loaded plugins.
 *
 * @constreturn A list of all loaded plugins.
 */
GList *purple_plugins_get_loaded(void);

/**
 * Saves the list of loaded plugins to the specified preference key
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

/**
 * Unloads all loaded plugins.
 */
void purple_plugins_unload_all(void);

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
