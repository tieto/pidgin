/**
 * @file gtkpluginpref.h GTK+ Plugin Preferences
 * @ingroup gtkui
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#ifndef _GAIM_GTKPLUGINPREF_H_
#define _GAIM_GTKPLUGINPREF_H_

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "pluginpref.h"

#include "gtkgaim.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Creates a Gtk Preference frame for a GaimPluginPrefFrame
 *
 * @param frame GaimPluginPrefFrame
 * @return The gtk preference frame
 */
GtkWidget *gaim_gtk_plugin_pref_create_frame(GaimPluginPrefFrame *frame);

#ifdef __cplusplus
}
#endif

#endif /* _GAIM_GTKPLUGINPREF_H_ */
