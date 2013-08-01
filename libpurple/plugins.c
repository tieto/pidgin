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
	GList *actions;  /**< Actions that the plugin can perform                >*/
	PurplePluginPrefFrameCallback get_pref_frame;  /**< Callback that returns a
	                                                    preferences frame for a
	                                                    plugin               >*/
};

enum
{
	PROP_0,
	PROP_PREF_FRAME_CALLBACK,
	PROP_LAST
};

static GPluginPluginInfoClass *parent_class;

/**************************************************************************
 * Globals
 **************************************************************************/
static GList *loaded_plugins     = NULL;
static GList *plugins_to_disable = NULL;

/**************************************************************************
 * Plugin API
 **************************************************************************/
gboolean
purple_plugin_load(GPluginPlugin *plugin)
{
	GError *error = NULL;

	g_return_val_if_fail(plugin != NULL, FALSE);

	if (purple_plugin_is_loaded(plugin))
		return TRUE;

	if (!gplugin_plugin_manager_load_plugin(plugin, &error)) {
		purple_debug_error("plugins", "Failed to load plugin %s: %s",
				gplugin_plugin_get_filename(plugin), error->message);
		g_error_free(error);
		return FALSE;
	}

	loaded_plugins = g_list_append(loaded_plugins, plugin);

	purple_signal_emit(purple_plugins_get_handle(), "plugin-load", plugin);

	return TRUE;
}

gboolean
purple_plugin_unload(GPluginPlugin *plugin)
{
	GError *error = NULL;

	g_return_val_if_fail(plugin != NULL, FALSE);
	g_return_val_if_fail(purple_plugin_is_loaded(plugin), FALSE);

	purple_debug_info("plugins", "Unloading plugin %s\n",
			gplugin_plugin_get_filename(plugin));

	if (!gplugin_plugin_manager_unload_plugin(plugin, &error)) {
		purple_debug_error("plugins", "Failed to unload plugin %s: %s",
				gplugin_plugin_get_filename(plugin), error->message);
		g_error_free(error);
		return FALSE;
	}

	/* cancel any pending dialogs the plugin has */
	purple_request_close_with_handle(plugin);
	purple_notify_close_with_handle(plugin);

	purple_signals_disconnect_by_handle(plugin);

	loaded_plugins     = g_list_remove(loaded_plugins, plugin);
	plugins_to_disable = g_list_remove(plugins_to_disable, plugin);

	purple_signal_emit(purple_plugins_get_handle(), "plugin-unload", plugin);

	purple_prefs_disconnect_by_handle(plugin);

	return TRUE;
}

gboolean
purple_plugin_is_loaded(const GPluginPlugin *plugin)
{
	g_return_val_if_fail(plugin != NULL, FALSE);

	return (gplugin_plugin_get_state(plugin) == GPLUGIN_PLUGIN_STATE_LOADED);
}

void
purple_plugin_add_action(GPluginPlugin *plugin, const char* label,
                         PurplePluginActionCallback callback)
{
	GPluginPluginInfo *plugin_info;
	PurplePluginInfoPrivate *priv;
	PurplePluginAction *action;

	g_return_if_fail(plugin != NULL);
	g_return_if_fail(label != NULL && callback != NULL);

	plugin_info = gplugin_plugin_get_info(plugin);
	priv = PURPLE_PLUGIN_INFO_GET_PRIVATE(plugin_info);

	action = g_new0(PurplePluginAction, 1);

	action->label    = g_strdup(label);
	action->callback = callback;
	action->plugin   = g_object_ref(plugin);

	priv->actions = g_list_append(priv->actions, action);

	g_object_unref(plugin_info);
}

void
purple_plugin_disable(GPluginPlugin *plugin)
{
	g_return_if_fail(plugin != NULL);

	if (!g_list_find(plugins_to_disable, plugin))
		plugins_to_disable = g_list_prepend(plugins_to_disable, plugin);
}

/**************************************************************************
 * GObject code for PurplePluginInfo
 **************************************************************************/
/* GObject Property names */
#define PROP_PREF_FRAME_CALLBACK_S  "pref-frame-callback"

/* Set method for GObject properties */
static void
purple_plugin_info_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurplePluginInfo *plugin_info = PURPLE_PLUGIN_INFO(obj);

	switch (param_id) {
		case PROP_PREF_FRAME_CALLBACK:
			purple_plugin_info_set_pref_frame_callback(plugin_info,
					g_value_get_pointer(value));
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
	PurplePluginInfo *plugin_info = PURPLE_PLUGIN_INFO(obj);

	switch (param_id) {
		case PROP_PREF_FRAME_CALLBACK:
			g_value_set_pointer(value,
					purple_plugin_info_get_pref_frame_callback(plugin_info));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* GObject initialization function */
static void
purple_plugin_info_init(GTypeInstance *instance, gpointer klass)
{
}

/* GObject dispose function */
static void
purple_plugin_info_dispose(GObject *object)
{
	G_OBJECT_CLASS(parent_class)->dispose(object);
}

/* GObject finalize function */
static void
purple_plugin_info_finalize(GObject *object)
{
	PurplePluginInfoPrivate *priv = PURPLE_PLUGIN_INFO_GET_PRIVATE(object);

	while (priv->actions) {
		g_boxed_free(PURPLE_TYPE_PLUGIN_ACTION, priv->actions->data);
		priv->actions = g_list_delete_link(priv->actions, priv->actions);
	}

	G_OBJECT_CLASS(parent_class)->finalize(object);
}

/* Class initializer function */
static void purple_plugin_info_class_init(PurplePluginInfoClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	g_type_class_add_private(klass, sizeof(PurplePluginInfoPrivate));

	obj_class->dispose = purple_plugin_info_dispose;
	obj_class->finalize = purple_plugin_info_finalize;

	/* Setup properties */
	obj_class->get_property = purple_plugin_info_get_property;
	obj_class->set_property = purple_plugin_info_set_property;

	g_object_class_install_property(obj_class, PROP_PREF_FRAME_CALLBACK,
		g_param_spec_pointer(PROP_PREF_FRAME_CALLBACK_S,
		                     "Preferences frame callback",
		                     "The callback that returns the preferences frame",
		                     G_PARAM_READWRITE));
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
			.instance_init = (GInstanceInitFunc)purple_plugin_info_init,
		};

		type = g_type_register_static(GPLUGIN_TYPE_PLUGIN_INFO,
		                              "PurplePluginInfo", &info, 0);
	}

	return type;
}

GList *
purple_plugin_info_get_actions(PurplePluginInfo *plugin_info)
{
	PurplePluginInfoPrivate *priv = PURPLE_PLUGIN_INFO_GET_PRIVATE(plugin_info);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->actions;
}

void
purple_plugin_info_set_pref_frame_callback(PurplePluginInfo *plugin_info,
		PurplePluginPrefFrameCallback callback)
{
	PurplePluginInfoPrivate *priv = PURPLE_PLUGIN_INFO_GET_PRIVATE(plugin_info);

	g_return_if_fail(priv != NULL);

	priv->get_pref_frame = callback;
}

PurplePluginPrefFrameCallback
purple_plugin_info_get_pref_frame_callback(PurplePluginInfo *plugin_info)
{
	PurplePluginInfoPrivate *priv = PURPLE_PLUGIN_INFO_GET_PRIVATE(plugin_info);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->get_pref_frame;
}

/**************************************************************************
 * PluginAction API
 **************************************************************************/
static void
purple_plugin_action_free(PurplePluginAction *action)
{
	g_return_if_fail(action != NULL);

	g_free(action->label);
	g_object_unref(action->plugin);
	g_free(action);
}

static PurplePluginAction *
purple_plugin_action_copy(PurplePluginAction *action)
{
	PurplePluginAction *action_copy;

	g_return_val_if_fail(action != NULL, NULL);

	action_copy = g_new(PurplePluginAction, 1);

	action_copy->label    = g_strdup(action->label);
	action_copy->callback = action->callback;
	action_copy->plugin   = g_object_ref(action->plugin);

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
purple_plugins_get_all(void)
{
	GList *ret = NULL, *ids, *l;
	GSList *plugins, *ll;

	ids = gplugin_plugin_manager_list_plugins();

	for (l = ids; l; l = l->next) {
		plugins = gplugin_plugin_manager_find_plugins(l->data);
		for (ll = plugins; ll; ll = ll->next)
			ret = g_list_append(ret, GPLUGIN_PLUGIN(ll->data));

		gplugin_plugin_manager_free_plugin_list(plugins);
	}
	g_list_free(ids);

	return ret;
}

GList *
purple_plugins_get_loaded(void)
{
	return loaded_plugins;
}

GPluginPlugin *
purple_plugins_find_by_filename(const char *filename)
{
	GList *l;

	for (l = purple_plugins_get_all(); l != NULL; l = l->next) {
		GPluginPlugin *plugin = GPLUGIN_PLUGIN(l->data);

		if (purple_strequal(gplugin_plugin_get_filename(plugin), filename))
			return plugin;
	}

	return NULL;
}

void
purple_plugins_save_loaded(const char *key)
{
	GList *pl;
	GList *files = NULL;

	for (pl = purple_plugins_get_loaded(); pl != NULL; pl = pl->next) {
		GPluginPlugin *plugin = GPLUGIN_PLUGIN(pl->data);
		if (!g_list_find(plugins_to_disable, plugin))
			files = g_list_append(files, (gchar *)gplugin_plugin_get_filename(plugin));
	}

	purple_prefs_set_string_list(key, files);
	g_list_free(files);
}

void
purple_plugins_load_saved(const char *key)
{
	GList *l, *files;

	g_return_if_fail(key != NULL);

	files = purple_prefs_get_string_list(key);

	for (l = files; l; l = l->next)
	{
		char *file;
		GPluginPlugin *plugin;

		if (l->data == NULL)
			continue;

		file = l->data;
		plugin = purple_plugins_find_by_filename(file);

		if (plugin) {
			purple_debug_info("plugins", "Loading saved plugin %s\n", file);
			purple_plugin_load(plugin);
			g_object_unref(plugin);
		} else {
			purple_debug_error("plugins", "Unable to find saved plugin %s\n", file);
		}

		g_free(l->data);
	}

	g_list_free(files);
}

void
purple_plugins_unload_all(void)
{
	while (loaded_plugins != NULL)
		purple_plugin_unload(loaded_plugins->data);
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

	gplugin_init();
	gplugin_set_plugin_info_type(PURPLE_TYPE_PLUGIN_INFO);
	gplugin_plugin_manager_append_path(LIBDIR);
	gplugin_plugin_manager_refresh();

	/* TODO GPlugin already has signals for these, these should be removed once
	        the new plugin API is properly established */
	purple_signal_register(handle, "plugin-load",
						 purple_marshal_VOID__POINTER,
						 G_TYPE_NONE, 1, GPLUGIN_TYPE_PLUGIN);
	purple_signal_register(handle, "plugin-unload",
						 purple_marshal_VOID__POINTER,
						 G_TYPE_NONE, 1, GPLUGIN_TYPE_PLUGIN);
}

void
purple_plugins_uninit(void) 
{
	void *handle = purple_plugins_get_handle();

	purple_debug_info("plugins", "Unloading all plugins\n");
	purple_plugins_unload_all();

	purple_signals_disconnect_by_handle(handle);
	purple_signals_unregister_by_instance(handle);

	gplugin_uninit();
}
