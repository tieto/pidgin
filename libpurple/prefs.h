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

#ifndef _PURPLE_PREFS_H_
#define _PURPLE_PREFS_H_
/**
 * SECTION:prefs
 * @section_id: libpurple-prefs
 * @short_description: <filename>prefs.h</filename>
 * @title: Preferences API
 */

#include <glib.h>

/**
 * PurplePrefType:
 * @PURPLE_PREF_NONE:        No type.
 * @PURPLE_PREF_BOOLEAN:     Boolean.
 * @PURPLE_PREF_INT:         Integer.
 * @PURPLE_PREF_STRING:      String.
 * @PURPLE_PREF_STRING_LIST: List of strings.
 * @PURPLE_PREF_PATH:        Path.
 * @PURPLE_PREF_PATH_LIST:   List of paths.
 *
 * Preference data types.
 */
typedef enum
{
	PURPLE_PREF_NONE,
	PURPLE_PREF_BOOLEAN,
	PURPLE_PREF_INT,
	PURPLE_PREF_STRING,
	PURPLE_PREF_STRING_LIST,
	PURPLE_PREF_PATH,
	PURPLE_PREF_PATH_LIST

} PurplePrefType;

/**
 * PurplePrefCallback:
 * @name: the name of the preference which has changed.
 * @type: the type of the preferenced named @name
 * @val:  the new value of the preferencs; should be cast to the correct
 *             type.  For instance, to recover the value of a #PURPLE_PREF_INT
 *             preference, use <literal>GPOINTER_TO_INT(val)</literal>.
 *             Alternatively, just call purple_prefs_get_int(),
 *             purple_prefs_get_string_list() etc.
 * @data: Arbitrary data specified when the callback was connected with
 *             purple_prefs_connect_callback().
 *
 * The type of callbacks for preference changes.
 *
 * See purple_prefs_connect_callback().
 */
typedef void (*PurplePrefCallback) (const char *name, PurplePrefType type,
		gconstpointer val, gpointer data);

G_BEGIN_DECLS

/**************************************************************************/
/*  Prefs API
    Preferences are named according to a directory-like structure.
    Example: "/plugins/core/potato/is_from_idaho" (probably a boolean)    */
/**************************************************************************/

/**
 * purple_prefs_get_handle:
 *
 * Returns the prefs subsystem handle.
 *
 * Returns: The prefs subsystem handle.
 */
void *purple_prefs_get_handle(void);

/**
 * purple_prefs_init:
 *
 * Initialize core prefs
 */
void purple_prefs_init(void);

/**
 * purple_prefs_uninit:
 *
 * Uninitializes the prefs subsystem.
 */
void purple_prefs_uninit(void);

/**
 * purple_prefs_add_none:
 * @name:  The name of the pref
 *
 * Add a new typeless pref.
 */
void purple_prefs_add_none(const char *name);

/**
 * purple_prefs_add_bool:
 * @name:  The name of the pref
 * @value: The initial value to set
 *
 * Add a new boolean pref.
 */
void purple_prefs_add_bool(const char *name, gboolean value);

/**
 * purple_prefs_add_int:
 * @name:  The name of the pref
 * @value: The initial value to set
 *
 * Add a new integer pref.
 */
void purple_prefs_add_int(const char *name, int value);

/**
 * purple_prefs_add_string:
 * @name:  The name of the pref
 * @value: The initial value to set
 *
 * Add a new string pref.
 */
void purple_prefs_add_string(const char *name, const char *value);

/**
 * purple_prefs_add_string_list:
 * @name:  The name of the pref
 * @value: The initial value to set
 *
 * Add a new string list pref.
 *
 * Note: This function takes a copy of the strings in the value list. The list
 *       itself and original copies of the strings are up to the caller to
 *       free.
 */
void purple_prefs_add_string_list(const char *name, GList *value);

/**
 * purple_prefs_add_path:
 * @name:  The name of the pref
 * @value: The initial value to set
 *
 * Add a new path pref.
 */
void purple_prefs_add_path(const char *name, const char *value);

/**
 * purple_prefs_add_path_list:
 * @name:  The name of the pref
 * @value: The initial value to set
 *
 * Add a new path list pref.
 *
 * Note: This function takes a copy of the strings in the value list. The list
 *       itself and original copies of the strings are up to the caller to
 *       free.
 */
void purple_prefs_add_path_list(const char *name, GList *value);


/**
 * purple_prefs_remove:
 * @name: The name of the pref
 *
 * Remove a pref.
 */
void purple_prefs_remove(const char *name);

/**
 * purple_prefs_rename:
 * @oldname: The old name of the pref
 * @newname: The new name for the pref
 *
 * Rename a pref
 */
void purple_prefs_rename(const char *oldname, const char *newname);

/**
 * purple_prefs_rename_boolean_toggle:
 * @oldname: The old name of the pref
 * @newname: The new name for the pref
 *
 * Rename a boolean pref, toggling it's value
 */
void purple_prefs_rename_boolean_toggle(const char *oldname, const char *newname);

/**
 * purple_prefs_destroy:
 *
 * Remove all prefs.
 */
void purple_prefs_destroy(void);

/**
 * purple_prefs_set_bool:
 * @name:  The name of the pref
 * @value: The value to set
 *
 * Set boolean pref value
 */
void purple_prefs_set_bool(const char *name, gboolean value);

/**
 * purple_prefs_set_int:
 * @name:  The name of the pref
 * @value: The value to set
 *
 * Set integer pref value
 */
void purple_prefs_set_int(const char *name, int value);

/**
 * purple_prefs_set_string:
 * @name:  The name of the pref
 * @value: The value to set
 *
 * Set string pref value
 */
void purple_prefs_set_string(const char *name, const char *value);

/**
 * purple_prefs_set_string_list:
 * @name:  The name of the pref
 * @value: The value to set
 *
 * Set string list pref value
 */
void purple_prefs_set_string_list(const char *name, GList *value);

/**
 * purple_prefs_set_path:
 * @name:  The name of the pref
 * @value: The value to set
 *
 * Set path pref value
 */
void purple_prefs_set_path(const char *name, const char *value);

/**
 * purple_prefs_set_path_list:
 * @name:  The name of the pref
 * @value: The value to set
 *
 * Set path list pref value
 */
void purple_prefs_set_path_list(const char *name, GList *value);


/**
 * purple_prefs_exists:
 * @name: The name of the pref
 *
 * Check if a pref exists
 *
 * Returns: TRUE if the pref exists.  Otherwise FALSE.
 */
gboolean purple_prefs_exists(const char *name);

/**
 * purple_prefs_get_pref_type:
 * @name: The name of the pref
 *
 * Get pref type
 *
 * Returns: The type of the pref
 */
PurplePrefType purple_prefs_get_pref_type(const char *name);

/**
 * purple_prefs_get_bool:
 * @name: The name of the pref
 *
 * Get boolean pref value
 *
 * Returns: The value of the pref
 */
gboolean purple_prefs_get_bool(const char *name);

/**
 * purple_prefs_get_int:
 * @name: The name of the pref
 *
 * Get integer pref value
 *
 * Returns: The value of the pref
 */
int purple_prefs_get_int(const char *name);

/**
 * purple_prefs_get_string:
 * @name: The name of the pref
 *
 * Get string pref value
 *
 * Returns: The value of the pref
 */
const char *purple_prefs_get_string(const char *name);

/**
 * purple_prefs_get_string_list:
 * @name: The name of the pref
 *
 * Get string list pref value
 *
 * Returns: The value of the pref
 */
GList *purple_prefs_get_string_list(const char *name);

/**
 * purple_prefs_get_path:
 * @name: The name of the pref
 *
 * Get path pref value
 *
 * Returns: The value of the pref
 */
const char *purple_prefs_get_path(const char *name);

/**
 * purple_prefs_get_path_list:
 * @name: The name of the pref
 *
 * Get path list pref value
 *
 * Returns: The value of the pref
 */
GList *purple_prefs_get_path_list(const char *name);

/**
 * purple_prefs_get_children_names:
 * @name: The parent pref
 *
 * Returns a list of children for a pref
 *
 * Returns: A list of newly allocated strings denoting the names of the children.
 *         Returns %NULL if there are no children or if pref doesn't exist.
 *         The caller must free all the strings and the list.
 */
GList *purple_prefs_get_children_names(const char *name);

/**
 * purple_prefs_connect_callback:
 * @handle:   The handle of the receiver.
 * @name:     The name of the preference
 * @cb:       The callback function
 * @data:     The data to pass to the callback function.
 *
 * Add a callback to a pref (and its children)
 *
 * See purple_prefs_disconnect_callback().
 *
 * Returns: An id to disconnect the callback
 */
guint purple_prefs_connect_callback(void *handle, const char *name, PurplePrefCallback cb,
		gpointer data);

/**
 * purple_prefs_disconnect_callback:
 *
 * Remove a callback to a pref
 */
void purple_prefs_disconnect_callback(guint callback_id);

/**
 * purple_prefs_disconnect_by_handle:
 *
 * Remove all pref callbacks by handle
 */
void purple_prefs_disconnect_by_handle(void *handle);

/**
 * purple_prefs_trigger_callback:
 *
 * Trigger callbacks as if the pref changed
 */
void purple_prefs_trigger_callback(const char *name);

/**
 * purple_prefs_load:
 *
 * Read preferences
 */
gboolean purple_prefs_load(void);

G_END_DECLS

#endif /* _PURPLE_PREFS_H_ */
