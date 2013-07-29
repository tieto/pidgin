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

#define PURPLE_PLUGIN_INFO_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_PLUGIN_INFO, PurplePluginInfoPrivate))

/** @copydoc _PurplePluginInfoPrivate */
typedef struct _PurplePluginInfoPrivate  PurplePluginInfoPrivate;

/**************************************************************************
 * Private data
 **************************************************************************/
struct _PurplePluginInfoPrivate {
};

/* Plugin info property enums */
enum
{
	PROP_0,
	PROP_LAST
};

static GPluginPluginInfoClass *parent_class;

/**************************************************************************
 * PluginInfo API
 **************************************************************************/


/**************************************************************************
 * GObject code
 **************************************************************************/
/* GObject Property names */
#define PROP_S  ""

/* Set method for GObject properties */
static void
purple_plugin_info_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurplePluginInfo *plugin_info = PURPLE_PLUGIN_INFO(obj);

	switch (param_id) {
		case PROP_0: /* TODO remove */
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
	PurplePluginInfo *plugin = PURPLE_PLUGIN_INFO(obj);

	switch (param_id) {
		case PROP_0: /* TODO remove */
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
}

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
						 G_TYPE_NONE, 1, PURPLE_TYPE_PLUGIN_INFO);
	purple_signal_register(handle, "plugin-unload",
						 purple_marshal_VOID__POINTER,
						 G_TYPE_NONE, 1, PURPLE_TYPE_PLUGIN_INFO);
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
