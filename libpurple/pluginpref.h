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
 *
 */
/**
 * SECTION:pluginpref
 * @section_id: libpurple-pluginpref
 * @short_description: <filename>pluginpref.h</filename>
 * @title: Plugin Preferences Frontend
 */

#ifndef _PURPLE_PLUGINPREF_H_
#define _PURPLE_PLUGINPREF_H_

typedef struct _PurplePluginPrefFrame		PurplePluginPrefFrame;
typedef struct _PurplePluginPref			PurplePluginPref;

/**
 * PurpleStringFormatType:
 * @PURPLE_STRING_FORMAT_TYPE_NONE:      The string is plain text.
 * @PURPLE_STRING_FORMAT_TYPE_MULTILINE: The string can have newlines.
 * @PURPLE_STRING_FORMAT_TYPE_HTML:      The string can be in HTML.
 *
 * String format for preferences.
 */
typedef enum
{
	PURPLE_STRING_FORMAT_TYPE_NONE      = 0,
	PURPLE_STRING_FORMAT_TYPE_MULTILINE = 1 << 0,
	PURPLE_STRING_FORMAT_TYPE_HTML      = 1 << 1
} PurpleStringFormatType;

/**
 * PurplePluginPrefType:
 * @PURPLE_PLUGIN_PREF_INFO:          no-value label
 * @PURPLE_PLUGIN_PREF_STRING_FORMAT: The preference has a string value.
 */
typedef enum {
	PURPLE_PLUGIN_PREF_NONE,
	PURPLE_PLUGIN_PREF_CHOICE,
	PURPLE_PLUGIN_PREF_INFO,
	PURPLE_PLUGIN_PREF_STRING_FORMAT
} PurplePluginPrefType;

#include <glib.h>
#include "prefs.h"

G_BEGIN_DECLS

/**************************************************************************/
/* Plugin Preference API                                                  */
/**************************************************************************/

/**
 * purple_plugin_pref_frame_new:
 *
 * Create a new plugin preference frame
 *
 * Returns: a new PurplePluginPrefFrame
 */
PurplePluginPrefFrame *purple_plugin_pref_frame_new(void);

/**
 * purple_plugin_pref_frame_destroy:
 * @frame: The plugin frame to destroy
 *
 * Destroy a plugin preference frame
 */
void purple_plugin_pref_frame_destroy(PurplePluginPrefFrame *frame);

/**
 * purple_plugin_pref_frame_add:
 * @frame: The plugin frame to add the preference to
 * @pref:  The preference to add to the frame
 *
 * Adds a plugin preference to a plugin preference frame
 */
void purple_plugin_pref_frame_add(PurplePluginPrefFrame *frame, PurplePluginPref *pref);

/**
 * purple_plugin_pref_frame_get_prefs:
 * @frame: The plugin frame to get the plugin preferences from
 *
 * Get the plugin preferences from a plugin preference frame
 *
 * Returns: (transfer none): a GList of plugin preferences
 */
GList *purple_plugin_pref_frame_get_prefs(PurplePluginPrefFrame *frame);

/**
 * purple_plugin_pref_new:
 *
 * Create a new plugin preference
 *
 * Returns: a new PurplePluginPref
 */
PurplePluginPref *purple_plugin_pref_new(void);

/**
 * purple_plugin_pref_new_with_name:
 * @name: The name of the pref
 *
 * Create a new plugin preference with name
 *
 * Returns: a new PurplePluginPref
 */
PurplePluginPref *purple_plugin_pref_new_with_name(const char *name);

/**
 * purple_plugin_pref_new_with_label:
 * @label: The label to be displayed
 *
 * Create a new plugin preference with label
 *
 * Returns: a new PurplePluginPref
 */
PurplePluginPref *purple_plugin_pref_new_with_label(const char *label);

/**
 * purple_plugin_pref_new_with_name_and_label:
 * @name:  The name of the pref
 * @label: The label to be displayed
 *
 * Create a new plugin preference with name and label
 *
 * Returns: a new PurplePluginPref
 */
PurplePluginPref *purple_plugin_pref_new_with_name_and_label(const char *name, const char *label);

/**
 * purple_plugin_pref_destroy:
 * @pref: The preference to destroy
 *
 * Destroy a plugin preference
 */
void purple_plugin_pref_destroy(PurplePluginPref *pref);

/**
 * purple_plugin_pref_set_name:
 * @pref: The plugin pref
 * @name: The name of the pref
 *
 * Set a plugin pref name
 */
void purple_plugin_pref_set_name(PurplePluginPref *pref, const char *name);

/**
 * purple_plugin_pref_get_name:
 * @pref: The plugin pref
 *
 * Get a plugin pref name
 *
 * Returns: The name of the pref
 */
const char *purple_plugin_pref_get_name(PurplePluginPref *pref);

/**
 * purple_plugin_pref_set_label:
 * @pref:  The plugin pref
 * @label: The label for the plugin pref
 *
 * Set a plugin pref label
 */
void purple_plugin_pref_set_label(PurplePluginPref *pref, const char *label);

/**
 * purple_plugin_pref_get_label:
 * @pref: The plugin pref
 *
 * Get a plugin pref label
 *
 * Returns: The label for the plugin pref
 */
const char *purple_plugin_pref_get_label(PurplePluginPref *pref);

/**
 * purple_plugin_pref_set_bounds:
 * @pref: The plugin pref
 * @min:  The min value
 * @max:  The max value
 *
 * Set the bounds for an integer pref
 */
void purple_plugin_pref_set_bounds(PurplePluginPref *pref, int min, int max);

/**
 * purple_plugin_pref_get_bounds:
 * @pref: The plugin pref
 * @min:  The min value
 * @max:  The max value
 *
 * Get the bounds for an integer pref
 */
void purple_plugin_pref_get_bounds(PurplePluginPref *pref, int *min, int *max);

/**
 * purple_plugin_pref_set_pref_type:
 * @pref: The plugin pref
 * @type: The type
 *
 * Set the type of a plugin pref
 */
void purple_plugin_pref_set_pref_type(PurplePluginPref *pref, PurplePluginPrefType type);

/**
 * purple_plugin_pref_get_pref_type:
 * @pref: The plugin pref
 *
 * Get the type of a plugin pref
 *
 * Returns: The type
 */
PurplePluginPrefType purple_plugin_pref_get_pref_type(PurplePluginPref *pref);

/**
 * purple_plugin_pref_add_choice:
 * @pref:  The plugin pref
 * @label: The label for the choice
 * @choice:  A gpointer of the choice
 *
 * Set the choices for a choices plugin pref
 */
void purple_plugin_pref_add_choice(PurplePluginPref *pref, const char *label, gpointer choice);

/**
 * purple_plugin_pref_get_choices:
 * @pref: The plugin pref
 *
 * Get the choices for a choices plugin pref
 *
 * Returns: (transfer none): GList of the choices
 */
GList *purple_plugin_pref_get_choices(PurplePluginPref *pref);

/**
 * purple_plugin_pref_set_max_length:
 * @pref:       The plugin pref
 * @max_length: The max length of the string
 *
 * Set the max length for a string plugin pref
 */
void purple_plugin_pref_set_max_length(PurplePluginPref *pref, unsigned int max_length);

/**
 * purple_plugin_pref_get_max_length:
 * @pref: The plugin pref
 *
 * Get the max length for a string plugin pref
 *
 * Returns: the max length
 */
unsigned int purple_plugin_pref_get_max_length(PurplePluginPref *pref);

/**
 * purple_plugin_pref_set_masked:
 * @pref: The plugin pref
 * @mask: The value to set
 *
 * Sets the masking of a string plugin pref
 */
void purple_plugin_pref_set_masked(PurplePluginPref *pref, gboolean mask);

/**
 * purple_plugin_pref_get_masked:
 * @pref: The plugin pref
 *
 * Gets the masking of a string plugin pref
 *
 * Returns: The masking
 */
gboolean purple_plugin_pref_get_masked(PurplePluginPref *pref);

/**
 * purple_plugin_pref_set_format_type:
 * @pref:	 The plugin pref
 * @format: The format of the string
 *
 * Sets the format type for a formattable-string plugin pref. You need to set the
 * pref type to PURPLE_PLUGIN_PREF_STRING_FORMAT first before setting the format.
 */
void purple_plugin_pref_set_format_type(PurplePluginPref *pref, PurpleStringFormatType format);

/**
 * purple_plugin_pref_get_format_type:
 * @pref: The plugin pref
 *
 * Gets the format type of the formattable-string plugin pref.
 *
 * Returns: The format of the pref
 */
PurpleStringFormatType purple_plugin_pref_get_format_type(PurplePluginPref *pref);

G_END_DECLS

#endif /* _PURPLE_PLUGINPREF_H_ */
