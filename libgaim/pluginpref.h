/**
 * @file pluginpref.h Plugin Preferences API
 * @ingroup core
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
#ifndef _GAIM_PLUGINPREF_H_
#define _GAIM_PLUGINPREF_H_

typedef struct _GaimPluginPrefFrame		GaimPluginPrefFrame;
typedef struct _GaimPluginPref			GaimPluginPref;

typedef enum {
	GAIM_PLUGIN_PREF_NONE,
	GAIM_PLUGIN_PREF_CHOICE,
	GAIM_PLUGIN_PREF_INFO,   /**< no-value label */
	GAIM_PLUGIN_PREF_STRING_FORMAT
} GaimPluginPrefType;

#include <glib.h>
#include "prefs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************/
/** @name Plugin Preference API                                           */
/**************************************************************************/
/*@{*/

/**
 * Create a new plugin preference frame
 *
 * @return a new GaimPluginPrefFrame
 */
GaimPluginPrefFrame *gaim_plugin_pref_frame_new(void);

/**
 * Destroy a plugin preference frame
 *
 * @param frame The plugin frame to destroy
 */
void gaim_plugin_pref_frame_destroy(GaimPluginPrefFrame *frame);

/**
 * Adds a plugin preference to a plugin preference frame
 *
 * @param frame The plugin frame to add the preference to
 * @param pref  The preference to add to the frame
 */
void gaim_plugin_pref_frame_add(GaimPluginPrefFrame *frame, GaimPluginPref *pref);

/**
 * Get the plugin preferences from a plugin preference frame
 *
 * @param frame The plugin frame to get the plugin preferences from
 * @return a GList of plugin preferences
 */
GList *gaim_plugin_pref_frame_get_prefs(GaimPluginPrefFrame *frame);

/**
 * Create a new plugin preference
 *
 * @return a new GaimPluginPref
 */
GaimPluginPref *gaim_plugin_pref_new(void);

/**
 * Create a new plugin preference with name
 *
 * @param name The name of the pref
 * @return a new GaimPluginPref
 */
GaimPluginPref *gaim_plugin_pref_new_with_name(const char *name);

/**
 * Create a new plugin preference with label
 *
 * @param label The label to be displayed
 * @return a new GaimPluginPref
 */
GaimPluginPref *gaim_plugin_pref_new_with_label(const char *label);

/**
 * Create a new plugin preference with name and label
 *
 * @param name  The name of the pref
 * @param label The label to be displayed
 * @return a new GaimPluginPref
 */
GaimPluginPref *gaim_plugin_pref_new_with_name_and_label(const char *name, const char *label);

/**
 * Destroy a plugin preference
 *
 * @param pref The preference to destroy
 */
void gaim_plugin_pref_destroy(GaimPluginPref *pref);

/**
 * Set a plugin pref name
 *
 * @param pref The plugin pref
 * @param name The name of the pref
 */
void gaim_plugin_pref_set_name(GaimPluginPref *pref, const char *name);

/**
 * Get a plugin pref name
 *
 * @param pref The plugin pref
 * @return The name of the pref
 */
const char *gaim_plugin_pref_get_name(GaimPluginPref *pref);

/**
 * Set a plugin pref label
 *
 * @param pref  The plugin pref
 * @param label The label for the plugin pref
 */
void gaim_plugin_pref_set_label(GaimPluginPref *pref, const char *label);

/**
 * Get a plugin pref label
 *
 * @param pref The plugin pref
 * @return The label for the plugin pref
 */
const char *gaim_plugin_pref_get_label(GaimPluginPref *pref);

/**
 * Set the bounds for an integer pref
 *
 * @param pref The plugin pref
 * @param min  The min value
 * @param max  The max value
 */
void gaim_plugin_pref_set_bounds(GaimPluginPref *pref, int min, int max);

/**
 * Get the bounds for an integer pref
 *
 * @param pref The plugin pref
 * @param min  The min value
 * @param max  The max value
 */
void gaim_plugin_pref_get_bounds(GaimPluginPref *pref, int *min, int *max);

/**
 * Set the type of a plugin pref
 *
 * @param pref The plugin pref
 * @param type The type
 */
void gaim_plugin_pref_set_type(GaimPluginPref *pref, GaimPluginPrefType type);

/**
 * Get the type of a plugin pref
 *
 * @param pref The plugin pref
 * @return The type
 */
GaimPluginPrefType gaim_plugin_pref_get_type(GaimPluginPref *pref);

/**
 * Set the choices for a choices plugin pref
 *
 * @param pref  The plugin pref
 * @param label The label for the choice
 * @param choice  A gpointer of the choice
 */
void gaim_plugin_pref_add_choice(GaimPluginPref *pref, const char *label, gpointer choice);

/**
 * Get the choices for a choices plugin pref
 *
 * @param pref The plugin pref
 * @return GList of the choices 
 */
GList *gaim_plugin_pref_get_choices(GaimPluginPref *pref);

/**
 * Set the max length for a string plugin pref
 *
 * @param pref       The plugin pref
 * @param max_length The max length of the string
 */
void gaim_plugin_pref_set_max_length(GaimPluginPref *pref, unsigned int max_length);

/**
 * Get the max length for a string plugin pref
 *
 * @param pref The plugin pref
 * @return the max length
 */
unsigned int gaim_plugin_pref_get_max_length(GaimPluginPref *pref);

/**
 * Sets the masking of a string plugin pref
 *
 * @param pref The plugin pref
 * @param mask The value to set
 */
void gaim_plugin_pref_set_masked(GaimPluginPref *pref, gboolean mask);

/**
 * Gets the masking of a string plugin pref
 *
 * @param pref The plugin pref
 * @return The masking
 */
gboolean gaim_plugin_pref_get_masked(GaimPluginPref *pref);

/**
 * Sets the format type for a formattable-string plugin pref. You need to set the
 * pref type to GAIM_PLUGIN_PREF_STRING_FORMAT first before setting the format.
 *
 * @param pref	 The plugin pref
 * @param format The format of the string
 */
void gaim_plugin_pref_set_format_type(GaimPluginPref *pref, GaimStringFormatType format);

/**
 * Gets the format type of the formattable-string plugin pref.
 *
 * @param pref The plugin pref
 * @return The format of the pref
 */
GaimStringFormatType gaim_plugin_pref_get_format_type(GaimPluginPref *pref);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* _GAIM_PLUGINPREF_H_ */
