/* pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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

#ifndef _PIDGINPLUGIN_H_
#define _PIDGINPLUGIN_H_
/**
 * SECTION:gtkplugin
 * @section_id: pidgin-gtkplugin
 * @short_description: <filename>gtkplugin.h</filename>
 * @title: Plugin API
 */

#include "pidgin.h"
#include "plugins.h"

#define PIDGIN_TYPE_PLUGIN_INFO             (pidgin_plugin_info_get_type())
#define PIDGIN_PLUGIN_INFO(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PIDGIN_TYPE_PLUGIN_INFO, PidginPluginInfo))
#define PIDGIN_PLUGIN_INFO_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PIDGIN_TYPE_PLUGIN_INFO, PidginPluginInfoClass))
#define PIDGIN_IS_PLUGIN_INFO(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PIDGIN_TYPE_PLUGIN_INFO))
#define PIDGIN_IS_PLUGIN_INFO_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PIDGIN_TYPE_PLUGIN_INFO))
#define PIDGIN_PLUGIN_INFO_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PIDGIN_TYPE_PLUGIN_INFO, PidginPluginInfoClass))

typedef struct _PidginPluginInfo PidginPluginInfo;
typedef struct _PidginPluginInfoClass PidginPluginInfoClass;

typedef GtkWidget *(*PidginPluginConfigFrameCb)(PurplePlugin *);

/**
 * PidginPluginInfo:
 *
 * Extends #PurplePluginInfo to hold UI information for pidgin.
 */
struct _PidginPluginInfo {
	PurplePluginInfo parent;
};

/**
 * PidginPluginInfoClass:
 *
 * The base class for all #PidginPluginInfo's.
 */
struct _PidginPluginInfoClass {
	PurplePluginInfoClass parent_class;

	/*< private >*/
	void (*_pidgin_reserved1)(void);
	void (*_pidgin_reserved2)(void);
	void (*_pidgin_reserved3)(void);
	void (*_pidgin_reserved4)(void);
};

G_BEGIN_DECLS

/**
 * pidgin_plugin_info_get_type:
 *
 * Returns: The #GType for the #PidginPluginInfo object.
 */
GType pidgin_plugin_info_get_type(void);

/**
 * pidgin_plugin_info_new:
 * @first_property:  The first property name
 * @...:  The value of the first property, followed optionally by more
 *             name/value pairs, followed by %NULL
 *
 * Creates a new #PidginPluginInfo instance to be returned from
 * #plugin_query of a pidgin plugin, using the provided name/value
 * pairs.
 *
 * See purple_plugin_info_new() for a list of available property names.
 * Additionally, you can provide the property
 * <literal>"gtk-config-frame-cb"</literal>, which should be a callback that
 * returns a #GtkWidget for the plugin's configuration
 * (see #PidginPluginConfigFrameCb).
 *
 * See purple_plugin_info_new().
 *
 * Returns: A new #PidginPluginInfo instance.
 */
PidginPluginInfo *pidgin_plugin_info_new(const char *first_property, ...)
                  G_GNUC_NULL_TERMINATED;

/**
 * pidgin_plugins_save:
 *
 * Saves all loaded plugins.
 */
void pidgin_plugins_save(void);

/**
 * pidgin_plugin_dialog_show:
 *
 * Shows the Plugins dialog
 */
void pidgin_plugin_dialog_show(void);

G_END_DECLS

#endif /* _PIDGINPLUGIN_H_ */
