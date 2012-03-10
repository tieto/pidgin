/**
 * @file gtkpluginpref.h GTK+ Plugin Preferences
 * @ingroup pidgin
 */

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
 *
 */
#ifndef _PIDGINPLUGINPREF_H_
#define _PIDGINPLUGINPREF_H_

#include "pluginpref.h"

#include "pidgin.h"

G_BEGIN_DECLS

/**
 * Creates a Gtk Preference frame for a PurplePluginPrefFrame
 *
 * @param frame PurplePluginPrefFrame
 * @return The gtk preference frame
 */
GtkWidget *pidgin_plugin_pref_create_frame(PurplePluginPrefFrame *frame);

G_END_DECLS

#endif /* _PIDGINPLUGINPREF_H_ */
