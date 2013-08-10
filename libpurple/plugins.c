/*
 * purple
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
#include "internal.h"

#include "core.h"
#include "debug.h"
#include "plugins.h"

#define PURPLE_PLUGIN_INFO_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_PLUGIN_INFO, PurplePluginInfoPrivate))

/** @copydoc _PurplePluginInfoPrivate */
typedef struct _PurplePluginInfoPrivate  PurplePluginInfoPrivate;

/**************************************************************************
 * Plugin info private data
 **************************************************************************/
struct _PurplePluginInfoPrivate {
	guint32 purple_abi;    /**< ABI version of purple required by the plugin */
	char *ui_requirement;  /**< ID of UI that is required to load the plugin */
	gboolean loadable;     /**< Whether the plugin is loadable               */
	char *error;           /**< Why the plugin is not loadable               */

	/** Callback that returns a list of actions the plugin can perform */
	PurplePluginGetActionsCallback get_actions;

	/** Callback that returns a preferences frame for a plugin */
	PurplePluginPrefFrameCallback get_pref_frame;

	/** TRUE if a plugin has been unloaded at least once. Load-on-query
	 *  plugins that have been unloaded once will not be auto-loaded again. */
	gboolean unloaded;
};

enum
{
	PROP_0,
	PROP_PURPLE_ABI,
	PROP_UI_REQUIREMENT,
	PROP_GET_ACTIONS,
	PROP_PREFERENCES_FRAME,
	PROP_LAST
};

static GObjectClass *parent_class;

/**************************************************************************
 * Globals
 **************************************************************************/
#ifdef PURPLE_PLUGINS
static GList *loaded_plugins     = NULL;
static GList *plugins_to_disable = NULL;
#endif

/**************************************************************************
 * Plugin API
 **************************************************************************/
gboolean
purple_plugin_load(PurplePlugin *plugin)
{
#ifdef PURPLE_PLUGINS
	GError *error = NULL;

	g_return_val_if_fail(plugin != NULL, FALSE);

	if (purple_plugin_is_loaded(plugin))
		return TRUE;

	if (!purple_plugin_is_loadable(plugin)) {
		purple_debug_error("plugins", "Failed to load plugin %s: %s",
				purple_plugin_get_filename(plugin),
				purple_plugin_get_error(plugin));
		return FALSE;
	}

	if (!gplugin_plugin_manager_load_plugin(plugin, &error)) {
		PurplePluginInfoPrivate *priv =
				PURPLE_PLUGIN_INFO_GET_PRIVATE(purple_plugin_get_info(plugin));
		g_free(priv->error);
		priv->error = g_strdup(error->message);
		g_error_free(error);

		purple_debug_error("plugins", "Failed to load plugin %s: %s",
				purple_plugin_get_filename(plugin),
				purple_plugin_get_error(plugin));
		return FALSE;
	}

	loaded_plugins = g_list_append(loaded_plugins, plugin);

	purple_debug_info("plugins", "Loaded plugin %s\n",
	                  purple_plugin_get_filename(plugin));

	purple_signal_emit(purple_plugins_get_handle(), "plugin-load", plugin);

	return TRUE;

#else
	return TRUE;
#endif /* PURPLE_PLUGINS */
}

gboolean
purple_plugin_unload(PurplePlugin *plugin)
{
#ifdef PURPLE_PLUGINS
	GError *error = NULL;
	PurplePluginInfoPrivate *priv;

	g_return_val_if_fail(plugin != NULL, FALSE);
	g_return_val_if_fail(purple_plugin_is_loaded(plugin), FALSE);

	priv = PURPLE_PLUGIN_INFO_GET_PRIVATE(purple_plugin_get_info(plugin));

	g_return_val_if_fail(priv != NULL, FALSE);

	purple_debug_info("plugins", "Unloading plugin %s\n",
			purple_plugin_get_filename(plugin));

	if (!gplugin_plugin_manager_unload_plugin(plugin, &error)) {
		purple_debug_error("plugins", "Failed to unload plugin %s: %s",
				purple_plugin_get_filename(plugin), error->message);
		g_error_free(error);
		return FALSE;
	}

	/* cancel any pending dialogs the plugin has */
	purple_request_close_with_handle(plugin);
	purple_notify_close_with_handle(plugin);

	purple_signals_disconnect_by_handle(plugin);

	priv->unloaded = TRUE;

	loaded_plugins     = g_list_remove(loaded_plugins, plugin);
	plugins_to_disable = g_list_remove(plugins_to_disable, plugin);

	purple_signal_emit(purple_plugins_get_handle(), "plugin-unload", plugin);

	purple_prefs_disconnect_by_handle(plugin);

	return TRUE;

#else
	return TRUE;
#endif /* PURPLE_PLUGINS */
}

gboolean
purple_plugin_is_loaded(const PurplePlugin *plugin)
{
#ifdef PURPLE_PLUGINS
	g_return_val_if_fail(plugin != NULL, FALSE);

	return (gplugin_plugin_get_state(plugin) == GPLUGIN_PLUGIN_STATE_LOADED);

#else
	return FALSE;
#endif
}

const gchar *
purple_plugin_get_filename(const PurplePlugin *plugin)
{
#ifdef PURPLE_PLUGINS
	g_return_val_if_fail(plugin != NULL, NULL);

	return gplugin_plugin_get_filename(plugin);

#else
	return NULL;
#endif
}

PurplePluginInfo *
purple_plugin_get_info(const PurplePlugin *plugin)
{
#ifdef PURPLE_PLUGINS
	GPluginPluginInfo *info;

	g_return_val_if_fail(plugin != NULL, NULL);

	info = gplugin_plugin_get_info(plugin);
	g_object_unref(info);

	return PURPLE_PLUGIN_INFO(info);

#else
	return NULL;
#endif
}

void
purple_plugin_disable(PurplePlugin *plugin)
{
#ifdef PURPLE_PLUGINS
	g_return_if_fail(plugin != NULL);

	if (!g_list_find(plugins_to_disable, plugin))
		plugins_to_disable = g_list_prepend(plugins_to_disable, plugin);
#endif
}

GType
purple_plugin_register_type(PurplePlugin *plugin, GType parent,
                            const gchar *name, const GTypeInfo *info,
                            GTypeFlags flags)
{
#ifdef PURPLE_PLUGINS
	return gplugin_native_plugin_register_type(GPLUGIN_NATIVE_PLUGIN(plugin),
	                                           parent, name, info, flags);

#else
	return G_TYPE_INVALID;
#endif
}

void
purple_plugin_add_interface(PurplePlugin *plugin, GType instance_type,
                            GType interface_type,
                            const GInterfaceInfo *interface_info)
{
#ifdef PURPLE_PLUGINS
	gplugin_native_plugin_add_interface(GPLUGIN_NATIVE_PLUGIN(plugin),
	                                    instance_type, interface_type,
	                                    interface_info);
#endif
}

gboolean
purple_plugin_is_internal(const PurplePlugin *plugin)
{
#ifdef PURPLE_PLUGINS
	GPluginPluginInfo *info;

	g_return_val_if_fail(plugin != NULL, FALSE);

	info = GPLUGIN_PLUGIN_INFO(purple_plugin_get_info(plugin));

	g_return_val_if_fail(info != NULL, FALSE);

	return (gplugin_plugin_info_get_flags(info) &
	        GPLUGIN_PLUGIN_INFO_FLAGS_INTERNAL);

#else
	return FALSE;
#endif
}

gboolean
purple_plugin_is_loadable(const PurplePlugin *plugin)
{
	PurplePluginInfoPrivate *priv;

	g_return_val_if_fail(plugin != NULL, FALSE);

	priv = PURPLE_PLUGIN_INFO_GET_PRIVATE(purple_plugin_get_info(plugin));

	if (priv)
		return priv->loadable;
	else
		return FALSE;
}

const gchar *
purple_plugin_get_error(const PurplePlugin *plugin)
{
	PurplePluginInfoPrivate *priv;

	g_return_val_if_fail(plugin != NULL, NULL);

	priv = PURPLE_PLUGIN_INFO_GET_PRIVATE(purple_plugin_get_info(plugin));

	if (priv)
		return priv->error;
	else
		return _("This plugin does not return a PurplePluginInfo.");
}

GSList *
purple_plugin_get_dependent_plugins(const PurplePlugin *plugin)
{
#ifdef PURPLE_PLUGINS
#warning TODO: Implement this when GPlugin can return dependent plugins.
	return NULL;

#else
	return NULL;
#endif
}

/**************************************************************************
 * GObject code for PurplePluginInfo
 **************************************************************************/
/* GObject Property names */
#define PROP_PURPLE_ABI_S         "purple-abi"
#define PROP_UI_REQUIREMENT_S     "ui-requirement"
#define PROP_GET_ACTIONS_S        "get-actions"
#define PROP_PREFERENCES_FRAME_S  "preferences-frame"

/* Set method for GObject properties */
static void
purple_plugin_info_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurplePluginInfo *info = PURPLE_PLUGIN_INFO(obj);
	PurplePluginInfoPrivate *priv = PURPLE_PLUGIN_INFO_GET_PRIVATE(info);

	switch (param_id) {
		case PROP_PURPLE_ABI:
			priv->purple_abi = g_value_get_uint(value);
			break;
		case PROP_UI_REQUIREMENT:
			priv->ui_requirement = g_strdup(g_value_get_string(value));
			break;
		case PROP_GET_ACTIONS:
			priv->get_actions = g_value_get_pointer(value);
			break;
		case PROP_PREFERENCES_FRAME:
			priv->get_pref_frame = g_value_get_pointer(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Get method for GObject properties */
static void
purple_plugin_info_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PurplePluginInfo *info = PURPLE_PLUGIN_INFO(obj);

	switch (param_id) {
		case PROP_PURPLE_ABI:
			g_value_set_uint(value, purple_plugin_info_get_abi_version(info));
			break;
		case PROP_GET_ACTIONS:
			g_value_set_pointer(value,
					purple_plugin_info_get_actions_callback(info));
			break;
		case PROP_PREFERENCES_FRAME:
			g_value_set_pointer(value,
					purple_plugin_info_get_pref_frame_callback(info));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Called when done constructing */
static void
purple_plugin_info_constructed(GObject *object)
{
	PurplePluginInfo *info = PURPLE_PLUGIN_INFO(object);
	PurplePluginInfoPrivate *priv = PURPLE_PLUGIN_INFO_GET_PRIVATE(info);
	const char *id = purple_plugin_info_get_id(info);
	guint32 version;

	parent_class->constructed(object);

	priv->loadable = TRUE;

	if (id == NULL || *id == '\0')
	{
		/* GPlugin already logs a warning when a plugin has no ID */

		priv->error = g_strdup(_("This plugin has not defined an ID."));
		priv->loadable = FALSE;
	}

	if (priv->ui_requirement && !purple_strequal(priv->ui_requirement, purple_core_get_ui()))
	{
		priv->error = g_strdup_printf(_("You are using %s, but this plugin requires %s."),
				purple_core_get_ui(), priv->ui_requirement);
		purple_debug_error("plugins", "%s is not loadable: The UI requirement is not met. (%s)\n",
				id, priv->error);
		priv->loadable = FALSE;
	}

	version = purple_plugin_info_get_abi_version(info);
	if (PURPLE_PLUGIN_ABI_MAJOR_VERSION(version) != PURPLE_MAJOR_VERSION ||
		PURPLE_PLUGIN_ABI_MINOR_VERSION(version) > PURPLE_MINOR_VERSION)
	{
		priv->error = g_strdup_printf(_("libpurple version mismatch %d.%d.x (need %d.%d.x)"),
				PURPLE_PLUGIN_ABI_MAJOR_VERSION(version),
				PURPLE_PLUGIN_ABI_MINOR_VERSION(version),
				PURPLE_MAJOR_VERSION, PURPLE_MINOR_VERSION);
		purple_debug_error("plugins", "%s is not loadable: libpurple version mismatch %d.%d.x (need %d.%d.x)\n",
				id, PURPLE_PLUGIN_ABI_MAJOR_VERSION(version),
				PURPLE_PLUGIN_ABI_MINOR_VERSION(version),
				PURPLE_MAJOR_VERSION, PURPLE_MINOR_VERSION);
		priv->loadable = FALSE;
	}
}

/* GObject finalize function */
static void
purple_plugin_info_finalize(GObject *object)
{
	PurplePluginInfoPrivate *priv = PURPLE_PLUGIN_INFO_GET_PRIVATE(object);

	g_free(priv->ui_requirement);
	g_free(priv->error);

	parent_class->finalize(object);
}

/* Class initializer function */
static void purple_plugin_info_class_init(PurplePluginInfoClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	g_type_class_add_private(klass, sizeof(PurplePluginInfoPrivate));

	obj_class->constructed = purple_plugin_info_constructed;
	obj_class->finalize = purple_plugin_info_finalize;

	/* Setup properties */
	obj_class->get_property = purple_plugin_info_get_property;
	obj_class->set_property = purple_plugin_info_set_property;

	g_object_class_install_property(obj_class, PROP_PURPLE_ABI,
		g_param_spec_uint(PROP_PURPLE_ABI_S,
		                  "ABI version",
		                  "The libpurple ABI version required by the plugin",
		                  0, G_MAXUINT32, 0,
		                  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property(obj_class, PROP_UI_REQUIREMENT,
		g_param_spec_string(PROP_UI_REQUIREMENT_S,
		                  "UI Requirement",
		                  "ID of UI that is required by this plugin", NULL,
		                  G_PARAM_WRITABLE));

	g_object_class_install_property(obj_class, PROP_GET_ACTIONS,
		g_param_spec_pointer(PROP_GET_ACTIONS_S,
		                  "Plugin actions",
		                  "Callback that returns list of the plugin's actions",
		                  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property(obj_class, PROP_PREFERENCES_FRAME,
		g_param_spec_pointer(PROP_PREFERENCES_FRAME_S,
		                  "Preferences frame callback",
		                  "The callback that returns the preferences frame",
		                  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

/**************************************************************************
 * PluginInfo API
 **************************************************************************/
GType
purple_plugin_info_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.class_size = sizeof(PurplePluginInfoClass),
			.class_init = (GClassInitFunc)purple_plugin_info_class_init,
			.instance_size = sizeof(PurplePluginInfo),
		};

		type = g_type_register_static(
#ifdef PURPLE_PLUGINS
		                              GPLUGIN_TYPE_PLUGIN_INFO,
#else
		                              G_TYPE_OBJECT,
#endif
		                              "PurplePluginInfo", &info, 0);
	}

	return type;
}

PurplePluginInfo *
purple_plugin_info_new(const char *first_property, ...)
{
	GObject *info;
	va_list var_args;

	/* at least ID is required */
	if (!first_property)
		return NULL;

	va_start(var_args, first_property);
	info = g_object_new_valist(PURPLE_TYPE_PLUGIN_INFO, first_property,
	                           var_args);
	va_end(var_args);

	return PURPLE_PLUGIN_INFO(info);
}

const gchar *
purple_plugin_info_get_id(const PurplePluginInfo *info)
{
#ifdef PURPLE_PLUGINS
	g_return_val_if_fail(info != NULL, NULL);

	return gplugin_plugin_info_get_id(GPLUGIN_PLUGIN_INFO(info));

#else
	return NULL;
#endif
}

const gchar *
purple_plugin_info_get_name(const PurplePluginInfo *info)
{
#ifdef PURPLE_PLUGINS
	g_return_val_if_fail(info != NULL, NULL);

	return gplugin_plugin_info_get_name(GPLUGIN_PLUGIN_INFO(info));

#else
	return NULL;
#endif
}

const gchar *
purple_plugin_info_get_version(const PurplePluginInfo *info)
{
#ifdef PURPLE_PLUGINS
	g_return_val_if_fail(info != NULL, NULL);

	return gplugin_plugin_info_get_version(GPLUGIN_PLUGIN_INFO(info));

#else
	return NULL;
#endif
}

const gchar *
purple_plugin_info_get_category(const PurplePluginInfo *info)
{
#ifdef PURPLE_PLUGINS
	g_return_val_if_fail(info != NULL, NULL);

	return gplugin_plugin_info_get_category(GPLUGIN_PLUGIN_INFO(info));

#else
	return NULL;
#endif
}

const gchar *
purple_plugin_info_get_summary(const PurplePluginInfo *info)
{
#ifdef PURPLE_PLUGINS
	g_return_val_if_fail(info != NULL, NULL);

	return gplugin_plugin_info_get_summary(GPLUGIN_PLUGIN_INFO(info));

#else
	return NULL;
#endif
}

const gchar *
purple_plugin_info_get_description(const PurplePluginInfo *info)
{
#ifdef PURPLE_PLUGINS
	g_return_val_if_fail(info != NULL, NULL);

	return gplugin_plugin_info_get_description(GPLUGIN_PLUGIN_INFO(info));

#else
	return NULL;
#endif
}

const gchar *
purple_plugin_info_get_author(const PurplePluginInfo *info)
{
#ifdef PURPLE_PLUGINS
	g_return_val_if_fail(info != NULL, NULL);

	return gplugin_plugin_info_get_author(GPLUGIN_PLUGIN_INFO(info));

#else
	return NULL;
#endif
}

const gchar *
purple_plugin_info_get_website(const PurplePluginInfo *info)
{
#ifdef PURPLE_PLUGINS
	g_return_val_if_fail(info != NULL, NULL);

	return gplugin_plugin_info_get_website(GPLUGIN_PLUGIN_INFO(info));

#else
	return NULL;
#endif
}

const gchar *
purple_plugin_info_get_icon(const PurplePluginInfo *info)
{
#ifdef PURPLE_PLUGINS
	g_return_val_if_fail(info != NULL, NULL);

	return gplugin_plugin_info_get_icon(GPLUGIN_PLUGIN_INFO(info));

#else
	return NULL;
#endif
}

const gchar *
purple_plugin_info_get_license(const PurplePluginInfo *info)
{
#ifdef PURPLE_PLUGINS
	g_return_val_if_fail(info != NULL, NULL);

	return gplugin_plugin_info_get_license(GPLUGIN_PLUGIN_INFO(info));

#else
	return NULL;
#endif
}

const gchar *
purple_plugin_info_get_license_text(const PurplePluginInfo *info)
{
#ifdef PURPLE_PLUGINS
	g_return_val_if_fail(info != NULL, NULL);

	return gplugin_plugin_info_get_license_text(GPLUGIN_PLUGIN_INFO(info));

#else
	return NULL;
#endif
}

const gchar *
purple_plugin_info_get_license_url(const PurplePluginInfo *info)
{
#ifdef PURPLE_PLUGINS
	g_return_val_if_fail(info != NULL, NULL);

	return gplugin_plugin_info_get_license_url(GPLUGIN_PLUGIN_INFO(info));

#else
	return NULL;
#endif
}

guint32
purple_plugin_info_get_abi_version(const PurplePluginInfo *info)
{
	PurplePluginInfoPrivate *priv = PURPLE_PLUGIN_INFO_GET_PRIVATE(info);

	g_return_val_if_fail(priv != NULL, 0);

	return priv->purple_abi;
}

GSList *
purple_plugin_info_get_dependencies(const PurplePluginInfo *info)
{
#ifdef PURPLE_PLUGINS
	g_return_val_if_fail(info != NULL, NULL);

	return gplugin_plugin_info_get_dependencies(GPLUGIN_PLUGIN_INFO(info));

#else
	return NULL;
#endif
}

PurplePluginGetActionsCallback
purple_plugin_info_get_actions_callback(const PurplePluginInfo *info)
{
	PurplePluginInfoPrivate *priv = PURPLE_PLUGIN_INFO_GET_PRIVATE(info);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->get_actions;
}

PurplePluginPrefFrameCallback
purple_plugin_info_get_pref_frame_callback(const PurplePluginInfo *info)
{
	PurplePluginInfoPrivate *priv = PURPLE_PLUGIN_INFO_GET_PRIVATE(info);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->get_pref_frame;
}

/**************************************************************************
 * PluginAction API
 **************************************************************************/
PurplePluginAction *
purple_plugin_action_new(const char* label, PurplePluginActionCallback callback)
{
	PurplePluginAction *action;

	g_return_val_if_fail(label != NULL && callback != NULL, NULL);

	action = g_new0(PurplePluginAction, 1);

	action->label    = g_strdup(label);
	action->callback = callback;

	return action;
}

void
purple_plugin_action_free(PurplePluginAction *action)
{
	g_return_if_fail(action != NULL);

	g_free(action->label);
	g_free(action);
}

static PurplePluginAction *
purple_plugin_action_copy(PurplePluginAction *action)
{
	PurplePluginAction *action_copy;

	g_return_val_if_fail(action != NULL, NULL);

	action_copy = g_new(PurplePluginAction, 1);
	*action_copy = *action;

	action_copy->label    = g_strdup(action->label);

	return action_copy;
}

GType
purple_plugin_action_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		type = g_boxed_type_register_static("PurplePluginAction",
				(GBoxedCopyFunc)purple_plugin_action_copy,
				(GBoxedFreeFunc)purple_plugin_action_free);
	}

	return type;
}

/**************************************************************************
 * Plugins API
 **************************************************************************/
GList *
purple_plugins_find_all(void)
{
#ifdef PURPLE_PLUGINS
	GList *ret = NULL, *ids, *l;
	GSList *plugins, *ll;

	ids = gplugin_plugin_manager_list_plugins();

	for (l = ids; l; l = l->next) {
		plugins = gplugin_plugin_manager_find_plugins(l->data);
		for (ll = plugins; ll; ll = ll->next)
			ret = g_list_append(ret, ll->data);

		gplugin_plugin_manager_free_plugin_list(plugins);
	}
	g_list_free(ids);

	return ret;

#else
	return NULL;
#endif
}

GList *
purple_plugins_get_loaded(void)
{
#ifdef PURPLE_PLUGINS
	return loaded_plugins;
#else
	return NULL;
#endif
}

void
purple_plugins_add_search_path(const gchar *path)
{
#ifdef PURPLE_PLUGINS
	gplugin_plugin_manager_append_path(path);
#endif
}

void
purple_plugins_refresh(void)
{
#ifdef PURPLE_PLUGINS
	GList *plugins, *l;

	gplugin_plugin_manager_refresh();

	plugins = purple_plugins_find_all();
	for (l = plugins; l != NULL; l = l->next) {
		PurplePlugin *plugin = PURPLE_PLUGIN(l->data);
		GPluginPluginInfo *info;
		PurplePluginInfoPrivate *priv;

		if (purple_plugin_is_loaded(plugin))
			continue;

		info = GPLUGIN_PLUGIN_INFO(purple_plugin_get_info(plugin));
		if (!info)
			continue;

		priv = PURPLE_PLUGIN_INFO_GET_PRIVATE(info);

		if (!priv->unloaded && gplugin_plugin_info_get_flags(info) &
				GPLUGIN_PLUGIN_INFO_FLAGS_LOAD_ON_QUERY) {
			purple_debug_info("plugins", "Auto-loading plugin %s\n",
			                  purple_plugin_get_filename(plugin));
			purple_plugin_load(plugin);
		}
	}

	g_list_free(plugins);
#endif
}

PurplePlugin *
purple_plugins_find_plugin(const gchar *id)
{
#ifdef PURPLE_PLUGINS
	PurplePlugin *plugin;

	g_return_val_if_fail(id != NULL && *id != '\0', NULL);

	plugin = gplugin_plugin_manager_find_plugin(id);
	g_object_unref(plugin);

	return plugin;

#else
	return NULL;
#endif
}

PurplePlugin *
purple_plugins_find_by_filename(const char *filename)
{
	GList *plugins, *l;

	g_return_val_if_fail(filename != NULL && *filename != '\0', NULL);

	plugins = purple_plugins_find_all();

	for (l = plugins; l != NULL; l = l->next) {
		PurplePlugin *plugin = PURPLE_PLUGIN(l->data);

		if (purple_strequal(purple_plugin_get_filename(plugin), filename)) {
			g_list_free(plugins);
			return plugin;
		}
	}
	g_list_free(plugins);

	return NULL;
}

void
purple_plugins_save_loaded(const char *key)
{
#ifdef PURPLE_PLUGINS
	GList *pl;
	GList *files = NULL;

	g_return_if_fail(key != NULL && *key != '\0');

	for (pl = purple_plugins_get_loaded(); pl != NULL; pl = pl->next) {
		PurplePlugin *plugin = PURPLE_PLUGIN(pl->data);
		GPluginPluginInfo *info =
				GPLUGIN_PLUGIN_INFO(purple_plugin_get_info(plugin));

		if (!info)
			continue;

		if (gplugin_plugin_info_get_flags(info) &
				GPLUGIN_PLUGIN_INFO_FLAGS_LOAD_ON_QUERY)
			continue;

		if (!g_list_find(plugins_to_disable, plugin))
			files = g_list_append(files, (gchar *)purple_plugin_get_filename(plugin));
	}

	purple_prefs_set_path_list(key, files);
	g_list_free(files);
#endif
}

void
purple_plugins_load_saved(const char *key)
{
#ifdef PURPLE_PLUGINS
	GList *l, *files;

	g_return_if_fail(key != NULL && *key != '\0');

	files = purple_prefs_get_path_list(key);

	for (l = files; l; l = l->next)
	{
		char *file;
		PurplePlugin *plugin;

		if (l->data == NULL)
			continue;

		file = l->data;
		plugin = purple_plugins_find_by_filename(file);

		if (plugin) {
			purple_debug_info("plugins", "Loading saved plugin %s\n", file);
			purple_plugin_load(plugin);
		} else {
			purple_debug_error("plugins", "Unable to find saved plugin %s\n", file);
		}

		g_free(l->data);
	}

	g_list_free(files);
#endif /* PURPLE_PLUGINS */
}

/**************************************************************************
 * Plugins Subsystem API
 **************************************************************************/
void *
purple_plugins_get_handle(void)
{
	static int handle;

	return &handle;
}

void
purple_plugins_init(void)
{
	void *handle = purple_plugins_get_handle();

#ifdef PURPLE_PLUGINS
	gplugin_init();
	purple_plugins_add_search_path(LIBDIR);
	purple_plugins_refresh();
#endif

	purple_signal_register(handle, "plugin-load",
	                       purple_marshal_VOID__POINTER,
	                       G_TYPE_NONE, 1, PURPLE_TYPE_PLUGIN);
	purple_signal_register(handle, "plugin-unload",
	                       purple_marshal_VOID__POINTER,
	                       G_TYPE_NONE, 1, PURPLE_TYPE_PLUGIN);
}

void
purple_plugins_uninit(void) 
{
	void *handle = purple_plugins_get_handle();

#ifdef PURPLE_PLUGINS
	purple_debug_info("plugins", "Unloading all plugins\n");
	while (loaded_plugins != NULL)
		purple_plugin_unload(loaded_plugins->data);
#endif

	purple_signals_disconnect_by_handle(handle);
	purple_signals_unregister_by_instance(handle);

#ifdef PURPLE_PLUGINS
	gplugin_uninit();
#endif
}
