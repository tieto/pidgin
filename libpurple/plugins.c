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
#include "plugins.h"

#define PURPLE_PLUGIN_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_PLUGIN, PurplePluginPrivate))

/** @copydoc _PurplePluginPrivate */
typedef struct _PurplePluginPrivate  PurplePluginPrivate;

/**************************************************************************
 * Private data
 **************************************************************************/
struct _PurplePluginPrivate {
	gboolean unloadable;
};

/* Plugin property enums */
enum
{
	PROP_0,
	PROP_UNLOADABLE,
	PROP_LAST
};

static GPluginPluginImplementationClass *parent_class;

/**************************************************************************
 * Plugin API
 **************************************************************************/


/**************************************************************************
 * GObject code
 **************************************************************************/
/* GObject Property names */
#define PROP_UNLOADABLE_S  "unloadable"

/* Set method for GObject properties */
static void
purple_plugin_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurplePlugin *plugin = PURPLE_PLUGIN(obj);

	switch (param_id) {
		case PROP_UNLOADABLE:
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Get method for GObject properties */
static void
purple_plugin_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PurplePlugin *plugin = PURPLE_PLUGIN(obj);

	switch (param_id) {
		case PROP_UNLOADABLE:
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* GObject initialization function */
static void
purple_plugin_init(GTypeInstance *instance, gpointer klass)
{
	PurplePluginPrivate *priv = PURPLE_PLUGIN_GET_PRIVATE(instance);

	priv->unloadable = TRUE;
}

/* GObject dispose function */
static void
purple_plugin_dispose(GObject *object)
{
	G_OBJECT_CLASS(parent_class)->dispose(object);
}

/* GObject finalize function */
static void
purple_plugin_finalize(GObject *object)
{
	G_OBJECT_CLASS(parent_class)->finalize(object);
}

/* Class initializer function */
static void purple_plugin_class_init(PurplePluginClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	g_type_class_add_private(klass, sizeof(PurplePluginPrivate));

	obj_class->dispose = purple_plugin_dispose;
	obj_class->finalize = purple_plugin_finalize;

	/* Setup properties */
	obj_class->get_property = purple_plugin_get_property;
	obj_class->set_property = purple_plugin_set_property;

	g_object_class_install_property(obj_class, PROP_UNLOADABLE,
			g_param_spec_boolean(PROP_UNLOADABLE_S, _("Unloadable"),
				_("Whether the plugin can be unloaded or not."), TRUE,
				G_PARAM_READWRITE)
			);
}

GType
purple_plugin_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.class_size = sizeof(PurplePluginClass),
			.class_init = (GClassInitFunc)purple_plugin_class_init,
			.instance_size = sizeof(PurplePlugin),
			.instance_init = (GInstanceInitFunc)purple_plugin_init,
		};

		type = g_type_register_static(GPLUGIN_TYPE_PLUGIN_IMPLEMENTATION,
		                              "PurplePlugin", &info, 0);
	}

	return type;
}

/**************************************************************************
 * Plugins Subsystem API
 **************************************************************************/
void *
purple_plugins_get_handle(void) {
	static int handle;

	return &handle;
}

void
purple_plugins_init(void) {
	void *handle = purple_plugins_get_handle();

	gplugin_init();
	gplugin_plugin_manager_append_path(LIBDIR);
	gplugin_plugin_manager_refresh();

	/* TODO GPlugin already has signals for these, these should be removed once
	        the new plugin API is properly established */
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

	purple_debug_info("plugins", "Unloading all plugins\n");
	purple_plugins_destroy_all();

	purple_signals_disconnect_by_handle(handle);
	purple_signals_unregister_by_instance(handle);

	gplugin_uninit();
}
