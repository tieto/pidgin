/**
 * @file plugins.h Plugins API
 * @ingroup core
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
#ifndef _PURPLE_PLUGIN_H_
#define _PURPLE_PLUGIN_H_

#include <gplugin.h>

#define PURPLE_TYPE_PLUGIN             (purple_plugin_get_type())
#define PURPLE_PLUGIN(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_PLUGIN, PurplePlugin))
#define PURPLE_PLUGIN_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_PLUGIN, PurplePluginClass))
#define PURPLE_IS_PLUGIN(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_PLUGIN))
#define PURPLE_IS_PLUGIN_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_PLUGIN))
#define PURPLE_PLUGIN_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_PLUGIN, PurplePluginClass))

/** @copydoc _PurplePlugin */
typedef struct _PurplePlugin PurplePlugin;
/** @copydoc _PurplePluginClass */
typedef struct _PurplePluginClass PurplePluginClass;

#include "pluginpref.h"

/**
 * Represents a plugin that can be loaded/unloaded by libpurple.
 *
 * #PurplePlugin inherits #GPluginPluginImplementation, which holds the
 * low-level details about the plugin in a #GPluginPlugin instance.
 */
struct _PurplePlugin {
	/*< private >*/
	GPluginPluginImplementation parent;
};

/**
 * PurplePluginClass:
 *
 * The base class for all #PurplePlugin's.
 */
struct _PurplePluginClass {
	/*< private >*/
	GPluginPluginImplementationClass parent_class;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

/**************************************************************************/
/** @name Plugin API                                                      */
/**************************************************************************/
/*@{*/

/**
 * Returns the GType for the PurplePlugin object.
 */
GType purple_plugin_get_type(void);

/*@}*/

/**************************************************************************/
/** @name Plugins Subsystem API                                            */
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

#endif /* _PURPLE_PLUGIN_H_ */
