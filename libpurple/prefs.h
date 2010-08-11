/**
 * @file prefs.h Prefs API
 * @ingroup core
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#ifndef _PURPLE_PREFS_H_
#define _PURPLE_PREFS_H_

#include <glib.h>

/**
 * Pref data types.
 */
typedef enum _PurplePrefType
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
 * Pref change callback type
 */

typedef void (*PurplePrefCallback) (const char *name, PurplePrefType type,
		gconstpointer val, gpointer data);

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************/
/** @name Prefs API                                                       */
/**************************************************************************/
/*@{*/

/**
 * Returns the prefs subsystem handle.
 *
 * @return The prefs subsystem handle.
 */
void *purple_prefs_get_handle(void);

/**
 * Initialize core prefs
 */
void purple_prefs_init(void);

/**
 * Uninitializes the prefs subsystem.
 */
void purple_prefs_uninit(void);

/**
 * Add a new typeless pref.
 *
 * @param name  The name of the pref
 */
void purple_prefs_add_none(const char *name);

/**
 * Add a new boolean pref.
 *
 * @param name  The name of the pref
 * @param value The initial value to set
 */
void purple_prefs_add_bool(const char *name, gboolean value);

/**
 * Add a new integer pref.
 *
 * @param name  The name of the pref
 * @param value The initial value to set
 */
void purple_prefs_add_int(const char *name, int value);

/**
 * Add a new string pref.
 *
 * @param name  The name of the pref
 * @param value The initial value to set
 */
void purple_prefs_add_string(const char *name, const char *value);

/**
 * Add a new string list pref.
 *
 * @param name  The name of the pref
 * @param value The initial value to set
 */
void purple_prefs_add_string_list(const char *name, GList *value);

/**
 * Add a new path pref.
 *
 * @param name  The name of the pref
 * @param value The initial value to set
 */
void purple_prefs_add_path(const char *name, const char *value);

/**
 * Add a new path list pref.
 *
 * @param name  The name of the pref
 * @param value The initial value to set
 */
void purple_prefs_add_path_list(const char *name, GList *value);


/**
 * Remove a pref.
 *
 * @param name The name of the pref
 */
void purple_prefs_remove(const char *name);

/**
 * Rename a pref
 *
 * @param oldname The old name of the pref
 * @param newname The new name for the pref
 */
void purple_prefs_rename(const char *oldname, const char *newname);

/**
 * Rename a boolean pref, toggling it's value
 *
 * @param oldname The old name of the pref
 * @param newname The new name for the pref
 */
void purple_prefs_rename_boolean_toggle(const char *oldname, const char *newname);

/**
 * Remove all prefs.
 */
void purple_prefs_destroy(void);

/**
 * Set raw pref value
 *
 * @param name  The name of the pref
 * @param value The value to set
 */
void purple_prefs_set_generic(const char *name, gpointer value);

/**
 * Set boolean pref value
 *
 * @param name  The name of the pref
 * @param value The value to set
 */
void purple_prefs_set_bool(const char *name, gboolean value);

/**
 * Set integer pref value
 *
 * @param name  The name of the pref
 * @param value The value to set
 */
void purple_prefs_set_int(const char *name, int value);

/**
 * Set string pref value
 *
 * @param name  The name of the pref
 * @param value The value to set
 */
void purple_prefs_set_string(const char *name, const char *value);

/**
 * Set string list pref value
 *
 * @param name  The name of the pref
 * @param value The value to set
 */
void purple_prefs_set_string_list(const char *name, GList *value);

/**
 * Set path pref value
 *
 * @param name  The name of the pref
 * @param value The value to set
 */
void purple_prefs_set_path(const char *name, const char *value);

/**
 * Set path list pref value
 *
 * @param name  The name of the pref
 * @param value The value to set
 */
void purple_prefs_set_path_list(const char *name, GList *value);


/**
 * Check if a pref exists
 *
 * @param name The name of the pref
 * @return TRUE if the pref exists.  Otherwise FALSE.
 */
gboolean purple_prefs_exists(const char *name);

/**
 * Get pref type
 *
 * @param name The name of the pref
 * @return The type of the pref
 */
PurplePrefType purple_prefs_get_type(const char *name);

/**
 * Get boolean pref value
 *
 * @param name The name of the pref
 * @return The value of the pref
 */
gboolean purple_prefs_get_bool(const char *name);

/**
 * Get integer pref value
 *
 * @param name The name of the pref
 * @return The value of the pref
 */
int purple_prefs_get_int(const char *name);

/**
 * Get string pref value
 *
 * @param name The name of the pref
 * @return The value of the pref
 */
const char *purple_prefs_get_string(const char *name);

/**
 * Get string list pref value
 *
 * @param name The name of the pref
 * @return The value of the pref
 */
GList *purple_prefs_get_string_list(const char *name);

/**
 * Get path pref value
 *
 * @param name The name of the pref
 * @return The value of the pref
 */
const char *purple_prefs_get_path(const char *name);

/**
 * Get path list pref value
 *
 * @param name The name of the pref
 * @return The value of the pref
 */
GList *purple_prefs_get_path_list(const char *name);


/**
 * Add a callback to a pref (and its children)
 */
guint purple_prefs_connect_callback(void *handle, const char *name, PurplePrefCallback cb,
		gpointer data);

/**
 * Remove a callback to a pref
 */
void purple_prefs_disconnect_callback(guint callback_id);

/**
 * Remove all pref callbacks by handle
 */
void purple_prefs_disconnect_by_handle(void *handle);

/**
 * Trigger callbacks as if the pref changed
 */
void purple_prefs_trigger_callback(const char *name);

/**
 * Read preferences
 */
gboolean purple_prefs_load(void);

/**
 * Rename legacy prefs and delete some that no longer exist.
 */
void purple_prefs_update_old(void);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* _PURPLE_PREFS_H_ */
