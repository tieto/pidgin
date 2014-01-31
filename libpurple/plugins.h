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

#define PURPLE_PLUGINS_DOMAIN          (g_quark_from_static_string("plugins"))

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

typedef void (*PurplePluginActionCb)(PurplePluginAction *);
typedef GList *(*PurplePluginActionsCb)(PurplePlugin *);
typedef gchar *(*PurplePluginExtraCb)(PurplePlugin *);
typedef PurplePluginPrefFrame *(*PurplePluginPrefFrameCb)(PurplePlugin *);
typedef gpointer (*PurplePluginPrefRequestCb)(PurplePlugin *);

/**
 * Flags that can be used to treat plugins differently.
 */
typedef enum /*< flags >*/
{
    PURPLE_PLUGIN_INFO_FLAGS_INTERNAL  = 1 << 1, /**< Plugin is not shown in UI lists */
    PURPLE_PLUGIN_INFO_FLAGS_AUTO_LOAD = 1 << 2, /**< Auto-load the plugin */
} PurplePluginInfoFlags;

/**
 * Holds information about a plugin.
 */
struct _PurplePluginInfo {
#ifdef PURPLE_PLUGINS
	GPluginPluginInfo parent;
#else
	GObject parent;
#endif

	/** The UI data associated with the plugin. This is a convenience
	 *  field provided to the UIs -- it is not used by the libpurple core.
	 */
	gpointer ui_data;
};

/**
 * PurplePluginInfoClass:
 *
 * The base class for all #PurplePluginInfo's.
 */
struct _PurplePluginInfoClass {
#ifdef PURPLE_PLUGINS
	GPluginPluginInfoClass parent_class;
#else
	GObjectClass parent_class;
#endif

	/*< private >*/
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
	PurplePluginActionCb callback;
	PurplePlugin *plugin;
	gpointer user_data;
};

/**
 * Returns an ABI version to set in plugins using major and minor versions.
 *
 * Note: The lower six nibbles represent the ABI version for libpurple, the
 *       rest are required by GPlugin.
 */
#define PURPLE_PLUGIN_ABI_VERSION(major,minor) \
	(0x01000000 | ((major) << 16) | (minor))

/** Returns the major version from an ABI version */
#define PURPLE_PLUGIN_ABI_MAJOR_VERSION(abi) \
	((abi >> 16) & 0xff)

/** Returns the minor version from an ABI version */
#define PURPLE_PLUGIN_ABI_MINOR_VERSION(abi) \
	(abi & 0xffff)

/**
  * A convenience‎ macro that returns an ABI version using PURPLE_MAJOR_VERSION
  * and PURPLE_MINOR_VERSION
  */
#define PURPLE_ABI_VERSION PURPLE_PLUGIN_ABI_VERSION(PURPLE_MAJOR_VERSION, PURPLE_MINOR_VERSION)

/**
 * PURPLE_PLUGIN_INIT:
 *
 * Defines the plugin's entry points.
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
#else /* PURPLE_PLUGINS && !PURPLE_STATIC_PRPL */
#define PURPLE_PLUGIN_INIT(pluginname,pluginquery,pluginload,pluginunload) \
	G_MODULE_EXPORT GPluginPluginInfo *gplugin_query(GError **e); \
	G_MODULE_EXPORT GPluginPluginInfo *gplugin_query(GError **e) { \
		return GPLUGIN_PLUGIN_INFO(pluginquery(e)); \
	} \
	G_MODULE_EXPORT gboolean gplugin_load(GPluginNativePlugin *p, GError **e); \
	G_MODULE_EXPORT gboolean gplugin_load(GPluginNativePlugin *p, GError **e) { \
		return pluginload(PURPLE_PLUGIN(p), e); \
	} \
	G_MODULE_EXPORT gboolean gplugin_unload(GPluginNativePlugin *p, GError **e); \
	G_MODULE_EXPORT gboolean gplugin_unload(GPluginNativePlugin *p, GError **e) { \
		return pluginunload(PURPLE_PLUGIN(p), e); \
	}
#endif

/**
 * PURPLE_DEFINE_TYPE:
 * 
 * A convenience macro for type implementations, which defines a *_get_type()
 * function; and a *_register_type() function for use in your plugin's load
 * function. You must define an instance initialization function *_init()
 * and a class initialization function *_class_init() for the type.
 *
 * The type will be registered statically if used in a static protocol or if
 * plugins support is disabled.
 *
 * @TN:   The name of the new type, in Camel case.
 * @t_n:  The name of the new type, in lowercase, words separated by '_'.
 * @T_P:  The #GType of the parent type.
 */
#if !defined(PURPLE_PLUGINS) || defined(PURPLE_STATIC_PRPL)
#define PURPLE_DEFINE_TYPE(TN, t_n, T_P) \
	PURPLE_DEFINE_STATIC_TYPE(TN, t_n, T_P)
#else
#define PURPLE_DEFINE_TYPE(TN, t_n, T_P) \
	PURPLE_DEFINE_DYNAMIC_TYPE(TN, t_n, T_P)
#endif

/**
 * PURPLE_DEFINE_TYPE_EXTENDED:
 *
 * A more general version of PURPLE_DEFINE_TYPE() which allows you to
 * specify #GTypeFlags and custom code.
 *
 * @TN:     The name of the new type, in Camel case.
 * @t_n:    The name of the new type, in lowercase, words separated by '_'.
 * @T_P:    The #GType of the parent type.
 * @flags:  #GTypeFlags to register the type with.
 * @CODE:   Custom code that gets inserted in *_get_type().
 */
#if !defined(PURPLE_PLUGINS) || defined(PURPLE_STATIC_PRPL)
#define PURPLE_DEFINE_TYPE_EXTENDED \
	PURPLE_DEFINE_STATIC_TYPE_EXTENDED
#else
#define PURPLE_DEFINE_TYPE_EXTENDED \
	PURPLE_DEFINE_DYNAMIC_TYPE_EXTENDED
#endif

/**
 * PURPLE_IMPLEMENT_INTERFACE_STATIC:
 *
 * A convenience macro to ease static interface addition in the CODE section
 * of PURPLE_DEFINE_TYPE_EXTENDED(). You should use this macro if the
 * interface is a part of the libpurple core.
 *
 * @TYPE_IFACE:  The #GType of the interface to add.
 * @iface_init:  The interface init function.
 */
#define PURPLE_IMPLEMENT_INTERFACE_STATIC(TYPE_IFACE, iface_init) { \
	const GInterfaceInfo interface_info = { \
		(GInterfaceInitFunc) iface_init, NULL, NULL \
	}; \
	g_type_add_interface_static(type_id, TYPE_IFACE, &interface_info); \
}

/**
 * PURPLE_IMPLEMENT_INTERFACE:
 *
 * A convenience macro to ease interface addition in the CODE section
 * of PURPLE_DEFINE_TYPE_EXTENDED(). You should use this macro if the
 * interface lives in the plugin.
 *
 * @TYPE_IFACE:  The #GType of the interface to add.
 * @iface_init:  The interface init function.
 */
#if !defined(PURPLE_PLUGINS) || defined(PURPLE_STATIC_PRPL)
#define PURPLE_IMPLEMENT_INTERFACE(TYPE_IFACE, iface_init) \
	PURPLE_IMPLEMENT_INTERFACE_STATIC(TYPE_IFACE, iface_init)
#else
#define PURPLE_IMPLEMENT_INTERFACE(TYPE_IFACE, iface_init) \
	PURPLE_IMPLEMENT_INTERFACE_DYNAMIC(TYPE_IFACE, iface_init)
#endif

/** A convenience macro for dynamic type implementations. */
#define PURPLE_DEFINE_DYNAMIC_TYPE(TN, t_n, T_P) \
	PURPLE_DEFINE_DYNAMIC_TYPE_EXTENDED (TN, t_n, T_P, 0, {})

/** A more general version of PURPLE_DEFINE_DYNAMIC_TYPE(). */
#define PURPLE_DEFINE_DYNAMIC_TYPE_EXTENDED(TypeName, type_name, TYPE_PARENT, flags, CODE) \
static GType type_name##_type_id = 0; \
G_MODULE_EXPORT GType type_name##_get_type(void) { \
	return type_name##_type_id; \
} \
void type_name##_register_type(PurplePlugin *); \
void type_name##_register_type(PurplePlugin *plugin) { \
	GType type_id; \
	const GTypeInfo type_info = { \
		sizeof (TypeName##Class), \
		(GBaseInitFunc) NULL, \
		(GBaseFinalizeFunc) NULL, \
		(GClassInitFunc) type_name##_class_init, \
		(GClassFinalizeFunc) NULL, \
		NULL, \
		sizeof (TypeName), \
		0, \
		(GInstanceInitFunc) type_name##_init, \
		NULL \
	}; \
	type_id = purple_plugin_register_type(plugin, TYPE_PARENT, #TypeName, \
	                                      &type_info, (GTypeFlags) flags); \
	type_name##_type_id = type_id; \
	{ CODE ; } \
}

/** A convenience macro to ease dynamic interface addition. */
#define PURPLE_IMPLEMENT_INTERFACE_DYNAMIC(TYPE_IFACE, iface_init) { \
	const GInterfaceInfo interface_info = { \
		(GInterfaceInitFunc) iface_init, NULL, NULL \
	}; \
	purple_plugin_add_interface(plugin, type_id, TYPE_IFACE, &interface_info); \
}

/** A convenience macro for static type implementations. */
#define PURPLE_DEFINE_STATIC_TYPE(TN, t_n, T_P) \
	PURPLE_DEFINE_STATIC_TYPE_EXTENDED (TN, t_n, T_P, 0, {})

/** A more general version of PURPLE_DEFINE_STATIC_TYPE(). */
#define PURPLE_DEFINE_STATIC_TYPE_EXTENDED(TypeName, type_name, TYPE_PARENT, flags, CODE) \
static GType type_name##_type_id = 0; \
GType type_name##_get_type(void) { \
	if (G_UNLIKELY(type_name##_type_id == 0)) { \
		GType type_id; \
		const GTypeInfo type_info = { \
			sizeof (TypeName##Class), \
			(GBaseInitFunc) NULL, \
			(GBaseFinalizeFunc) NULL, \
			(GClassInitFunc) type_name##_class_init, \
			(GClassFinalizeFunc) NULL, \
			NULL, \
			sizeof (TypeName), \
			0, \
			(GInstanceInitFunc) type_name##_init, \
			NULL \
		}; \
		type_id = g_type_register_static(TYPE_PARENT, #TypeName, &type_info, \
		                                 (GTypeFlags) flags); \
		type_name##_type_id = type_id; \
		{ CODE ; } \
	} \
	return type_name##_type_id; \
} \
void type_name##_register_type(PurplePlugin *); \
void type_name##_register_type(PurplePlugin *plugin) { }

G_BEGIN_DECLS

/**************************************************************************/
/** @name Plugin API                                                      */
/**************************************************************************/
/*@{*/

/**
 * Attempts to load a plugin.
 *
 * @plugin: The plugin to load.
 * @error:  Return location for a #GError or %NULL. If provided, this
 *               will be set to the reason if the load fails.
 *
 * Returns: %TRUE if successful or already loaded, %FALSE otherwise.
 *
 * @see purple_plugin_unload()
 */
gboolean purple_plugin_load(PurplePlugin *plugin, GError **error);

/**
 * Unloads the specified plugin.
 *
 * @plugin: The plugin handle.
 * @error:  Return location for a #GError or %NULL. If provided, this
 *               will be set to the reason if the unload fails.
 *
 * Returns: %TRUE if successful or not loaded, %FALSE otherwise.
 *
 * @see purple_plugin_load()
 */
gboolean purple_plugin_unload(PurplePlugin *plugin, GError **error);

/**
 * Returns whether or not a plugin is currently loaded.
 *
 * @plugin: The plugin.
 *
 * Returns: %TRUE if loaded, or %FALSE otherwise.
 */
gboolean purple_plugin_is_loaded(const PurplePlugin *plugin);

/**
 * Returns a plugin's filename, along with the path.
 *
 * @info: The plugin.
 *
 * Returns: The plugin's filename.
 */
const gchar *purple_plugin_get_filename(const PurplePlugin *plugin);

/**
 * Returns a plugin's #PurplePluginInfo instance.
 *
 * @info: The plugin.
 *
 * Returns: The plugin's #PurplePluginInfo instance.
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
 * @plugin:  The plugin that is registering the type.
 * @parent:  Type from which this type will be derived.
 * @name:    Name of the new type.
 * @info:    Information to initialize and destroy a type's classes and
 *                instances.
 * @flags:   Bitwise combination of values that determines the nature
 *                (e.g. abstract or not) of the type.
 *
 * Returns: The new GType, or @c G_TYPE_INVALID if registration failed.
 */
GType purple_plugin_register_type(PurplePlugin *plugin, GType parent,
                                  const gchar *name, const GTypeInfo *info,
                                  GTypeFlags flags);

/**
 * Adds a dynamic interface type to an instantiable type.
 *
 * @plugin:          The plugin that is adding the interface type.
 * @instance_type:   The GType of the instantiable type.
 * @interface_type:  The GType of the interface type.
 * @interface_info:  Information used to manage the interface type.
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
 * @plugin: The plugin.
 *
 * Returns: %TRUE if the plugin is an internal plugin, %FALSE otherwise.
 */
gboolean purple_plugin_is_internal(const PurplePlugin *plugin);

/**
 * Returns a list of plugins that depend on a particular plugin.
 *
 * @plugin: The plugin whose dependent plugins are returned.
 *
 * Returns: (TODO const): The list of a plugins that depend on the specified plugin.
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
 * Valid property names are:                                                  \n
 * "id"               (string) The ID of the plugin.                          \n
 * "name"             (string) The translated name of the plugin.             \n
 * "version"          (string) Version of the plugin.                         \n
 * "category"         (string) Primary category of the plugin.                \n
 * "summary"          (string) Brief summary of the plugin.                   \n
 * "description"      (string) Full description of the plugin.                \n
 * "authors"          (const gchar * const *) A NULL-terminated list of plugin
 *                             authors. format: First Last <user@domain.com>  \n
 * "website"          (string) Website of the plugin.                         \n
 * "icon"             (string) Path to a plugin's icon.                       \n
 * "license-id"       (string) Short name of the plugin's license. This should
 *                        either be an identifier of the license from
 *                        http://dep.debian.net/deps/dep5/#license-specification
 *                        or "Other" for custom licenses.                     \n
 * "license-text"     (string) The text of the plugin's license, if unlisted on
 *                             DEP5.                                          \n
 * "license-url"      (string) The plugin's license URL, if unlisted on DEP5. \n
 * "dependencies"     (const gchar * const *) A NULL-terminated list of plugin
 *                             IDs required by the plugin.                    \n
 * "abi-version"      (guint32) The ABI version required by the plugin.       \n
 * "actions-cb"       (PurplePluginActionsCb) Callback that returns a list of
 *                             actions the plugin can perform.                \n
 * "extra-cb"         (PurplePluginExtraCb) Callback that returns a newly
 *                             allocated string denoting extra information
 *                             about a plugin.                                \n
 * "pref-frame-cb"    (PurplePluginPrefFrameCb) Callback that returns a
 *                             preferences frame for the plugin.              \n
 * "pref-request-cb"  (PurplePluginPrefRequestCb) Callback that returns a
 *                             preferences request handle for the plugin.     \n
 * "flags"            (PurplePluginInfoFlags) The flags for a plugin.         \n
 *
 * @first_property:  The first property name
 * @...:  The value of the first property, followed optionally by more
 *             name/value pairs, followed by %NULL
 *
 * Returns: A new #PurplePluginInfo instance.
 *
 * @see PURPLE_PLUGIN_ABI_VERSION
 * @see @ref plugin-ids
 */
PurplePluginInfo *purple_plugin_info_new(const char *first_property, ...)
                  G_GNUC_NULL_TERMINATED;

/**
 * Returns a plugin's ID.
 *
 * @info: The plugin's info instance.
 *
 * Returns: The plugin's ID.
 */
const gchar *purple_plugin_info_get_id(const PurplePluginInfo *info);

/**
 * Returns a plugin's translated name.
 *
 * @info: The plugin's info instance.
 *
 * Returns: The name of the plugin, or %NULL.
 */
const gchar *purple_plugin_info_get_name(const PurplePluginInfo *info);

/**
 * Returns a plugin's version.
 *
 * @info: The plugin's info instance.
 *
 * Returns: The version of the plugin, or %NULL.
 */
const gchar *purple_plugin_info_get_version(const PurplePluginInfo *info);

/**
 * Returns a plugin's primary category.
 *
 * @info: The plugin's info instance.
 *
 * Returns: The primary category of the plugin, or %NULL.
 */
const gchar *purple_plugin_info_get_category(const PurplePluginInfo *info);

/**
 * Returns a plugin's summary.
 *
 * @info: The plugin's info instance.
 *
 * Returns: The summary of the plugin, or %NULL.
 */
const gchar *purple_plugin_info_get_summary(const PurplePluginInfo *info);

/**
 * Returns a plugin's description.
 *
 * @info: The plugin's info instance.
 *
 * Returns: The description of the plugin, or %NULL.
 */
const gchar *purple_plugin_info_get_description(const PurplePluginInfo *info);

/**
 * Returns a NULL-terminated list of the plugin's authors.
 *
 * @info: The plugin's info instance.
 *
 * Returns: The authors of the plugin, or %NULL.
 */
const gchar * const *
purple_plugin_info_get_authors(const PurplePluginInfo *info);

/**
 * Returns a plugin's website.
 *
 * @info: The plugin's info instance.
 *
 * Returns: The website of the plugin, or %NULL.
 */
const gchar *purple_plugin_info_get_website(const PurplePluginInfo *info);

/**
 * Returns the path to a plugin's icon.
 *
 * @info: The plugin's info instance.
 *
 * Returns: The path to the plugin's icon, or %NULL.
 */
const gchar *purple_plugin_info_get_icon(const PurplePluginInfo *info);

/**
 * Returns a short name of the plugin's license.
 *
 * @info: The plugin's info instance.
 *
 * Returns: The license name of the plugin, or %NULL.
 */
const gchar *purple_plugin_info_get_license_id(const PurplePluginInfo *info);

/**
 * Returns the text of a plugin's license.
 *
 * @info: The plugin's info instance.
 *
 * Returns: The license text of the plugin, or %NULL.
 */
const gchar *purple_plugin_info_get_license_text(const PurplePluginInfo *info);

/**
 * Returns the URL of a plugin's license.
 *
 * @info: The plugin's info instance.
 *
 * Returns: The license URL of the plugin, or %NULL.
 */
const gchar *purple_plugin_info_get_license_url(const PurplePluginInfo *info);

/**
 * Returns a NULL-terminated list of IDs of plugins required by a plugin.
 *
 * @info: The plugin's info instance.
 *
 * Returns: The dependencies of the plugin, or %NULL.
 */
const gchar * const *
purple_plugin_info_get_dependencies(const PurplePluginInfo *info);

/**
 * Returns the required purple ABI version for a plugin.
 *
 * @info: The plugin's info instance.
 *
 * Returns: The required purple ABI version for the plugin.
 */
guint32 purple_plugin_info_get_abi_version(const PurplePluginInfo *info);

/**
 * Returns the callback that retrieves the list of actions a plugin can perform
 * at that moment.
 *
 * @info: The plugin info to get the callback from.
 *
 * Returns: The callback that returns a list of #PurplePluginAction
 *         instances corresponding to the actions a plugin can perform.
 */
PurplePluginActionsCb
purple_plugin_info_get_actions_cb(const PurplePluginInfo *info);

/**
 * Returns a callback that gives extra information about a plugin. You must
 * free the string returned by this callback.
 *
 * @info: The plugin info to get extra information from.
 *
 * Returns: The callback that returns extra information about a plugin.
 */
PurplePluginExtraCb
purple_plugin_info_get_extra_cb(const PurplePluginInfo *info);

/**
 * Returns the callback that retrieves the preferences frame for a plugin, set
 * via the "pref-frame-cb" property of the plugin info.
 *
 * @info: The plugin info to get the callback from.
 *
 * Returns: The callback that returns the preferences frame.
 */
PurplePluginPrefFrameCb
purple_plugin_info_get_pref_frame_cb(const PurplePluginInfo *info);

/**
 * Returns the callback that retrieves the preferences request handle for a
 * plugin, set via the "pref-request-cb" property of the plugin info.
 *
 * @info: The plugin info to get the callback from.
 *
 * Returns: The callback that returns the preferences request handle.
 */
PurplePluginPrefRequestCb
purple_plugin_info_get_pref_request_cb(const PurplePluginInfo *info);

/**
 * Returns the plugin's flags.
 *
 * @info: The plugin's info instance.
 *
 * Returns: The flags of the plugin.
 */
PurplePluginInfoFlags
purple_plugin_info_get_flags(const PurplePluginInfo *info);

/**
 * Returns an error in the plugin info that would prevent the plugin from being
 * loaded.
 *
 * @info: The plugin info.
 *
 * Returns: The plugin info error, or %NULL.
 */
const gchar *purple_plugin_info_get_error(const PurplePluginInfo *info);

/**
 * Set the UI data associated with a plugin.
 *
 * @info: The plugin's info instance.
 * @ui_data: A pointer to associate with this object.
 */
void purple_plugin_info_set_ui_data(PurplePluginInfo *info, gpointer ui_data);

/**
 * Returns the UI data associated with a plugin.
 *
 * @info: The plugin's info instance.
 *
 * Returns: The UI data associated with this plugin.  This is a
 *         convenience field provided to the UIs--it is not
 *         used by the libpurple core.
 */
gpointer purple_plugin_info_get_ui_data(const PurplePluginInfo *info);

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
 * list in the "actions-cb" callback for your plugin.
 *
 * @label:    The description of the action to show to the user.
 * @callback: The callback to call when the user selects this action.
 */
PurplePluginAction *purple_plugin_action_new(const char* label,
		PurplePluginActionCb callback);

/**
 * Frees a PurplePluginAction
 *
 * @action: The PurplePluginAction to free.
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
 * Returns: A list of all plugins. The list is owned by the caller, and must be
 *         g_list_free()d to avoid leaking the nodes.
 */
GList *purple_plugins_find_all(void);

/**
 * Returns a list of all loaded plugins.
 *
 * Returns: (TODO const): A list of all loaded plugins.
 */
GList *purple_plugins_get_loaded(void);

/**
 * Add a new directory to search for plugins
 *
 * @path: The new search path.
 */
void purple_plugins_add_search_path(const gchar *path);

/**
 * Forces a refresh of all plugins found in the search paths, and loads plugins
 * that are to be auto-loaded.
 *
 * @see purple_plugins_add_search_path()
 */
void purple_plugins_refresh(void);

/**
 * Finds a plugin with the specified plugin ID.
 *
 * @id: The plugin ID.
 *
 * Returns: The plugin if found, or %NULL if not found.
 */
PurplePlugin *purple_plugins_find_plugin(const gchar *id);

/**
 * Finds a plugin with the specified filename (filename with a path).
 *
 * @filename: The plugin filename.
 *
 * Returns: The plugin if found, or %NULL if not found.
 */
PurplePlugin *purple_plugins_find_by_filename(const char *filename);

/**
 * Saves the list of loaded plugins to the specified preference key.
 * Plugins that are set to auto-load are not saved.
 *
 * @key: The preference key to save the list of plugins to.
 */
void purple_plugins_save_loaded(const char *key);

/**
 * Attempts to load all the plugins in the specified preference key
 * that were loaded when purple last quit.
 *
 * @key: The preference key containing the list of plugins.
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
 * Returns: The plugin sybsystem handle.
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
