/**
 * @file prefs.h Prefs API
 *
 * gaim
 *
 * Copyright (C) 2003, Nathan Walp <faceprint@faceprint.com>
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

#ifndef _PREFS_H_
#define _PREFS_H_

#include <glib.h>

/**
 * Pref data types.
 */
typedef enum _GaimPrefType
{
	GAIM_PREF_NONE,
	GAIM_PREF_BOOLEAN,
	GAIM_PREF_INT,
	GAIM_PREF_STRING,
	GAIM_PREF_STRING_LIST
} GaimPrefType;

/**
 * Pref change callback type
 */

typedef void (*GaimPrefCallback) (const char *name, GaimPrefType type,
		gpointer val, gpointer data);

/**************************************************************************/
/** @name Prefs API                                                       */
/**************************************************************************/
/*@{*/

/**
 * Initialize core prefs
 */
void gaim_prefs_init();

/**
 * Add a new typeless pref.
 *
 * @param name  The name of the pref
 */
void gaim_prefs_add_none(const char *name);

/**
 * Add a new boolean pref.
 *
 * @param name  The name of the pref
 * @param value The initial value to set
 */
void gaim_prefs_add_bool(const char *name, gboolean value);

/**
 * Add a new integer pref.
 *
 * @param name  The name of the pref
 * @param value The initial value to set
 */
void gaim_prefs_add_int(const char *name, int value);

/**
 * Add a new string pref.
 *
 * @param name  The name of the pref
 * @param value The initial value to set
 */
void gaim_prefs_add_string(const char *name, const char *value);

/**
 * Add a new string list pref.
 *
 * @param name  The name of the pref
 * @param value The initial value to set
 */
void gaim_prefs_add_string_list(const char *name, GList *value);

/**
 * Remove a pref.
 *
 * @param name The name of the pref
 */
void gaim_prefs_remove(const char *name);

/**
 * Remove all prefs.
 */
void gaim_prefs_destroy();

/**
 * Set raw pref value
 *
 * @param name  The name of the pref
 * @param value The value to set
 */
void gaim_prefs_set_generic(const char *name, gpointer value);

/**
 * Set boolean pref value
 *
 * @param name  The name of the pref
 * @param value The value to set
 */
void gaim_prefs_set_bool(const char *name, gboolean value);

/**
 * Set integer pref value
 *
 * @param name  The name of the pref
 * @param value The value to set
 */
void gaim_prefs_set_int(const char *name, int value);

/**
 * Set string pref value
 *
 * @param name  The name of the pref
 * @param value The value to set
 */
void gaim_prefs_set_string(const char *name, const char *value);

/**
 * Set string pref value
 *
 * @param name  The name of the pref
 * @param value The value to set
 */
void gaim_prefs_set_string_list(const char *name, GList *value);

/**
 * Get boolean pref value
 *
 * @param name The name of the pref
 * @return The value of the pref
 */
gboolean gaim_prefs_get_bool(const char *name);

/**
 * Get integer pref value
 *
 * @param name The name of the pref
 * @return The value of the pref
 */
int gaim_prefs_get_int(const char *name);

/**
 * Get string pref value
 *
 * @param name The name of the pref
 * @return The value of the pref
 */
const char *gaim_prefs_get_string(const char *name);

/**
 * Get string pref value
 *
 * @param name The name of the pref
 * @return The value of the pref
 */
GList *gaim_prefs_get_string_list(const char *name);

/**
 * Add a callback to a pref (and its children)
 */
guint gaim_prefs_connect_callback(const char *name, GaimPrefCallback cb,
		gpointer data);

/**
 * Remove a callback to a pref
 */
void gaim_prefs_disconnect_callback(guint callback_id);

/**
 * Trigger callbacks as if the pref changed
 */
void gaim_prefs_trigger_callback(const char *name);

/**
 * Read preferences
 */
gboolean gaim_prefs_load();

/**
 * Force an immediate write of preferences
 */
void gaim_prefs_sync();

/*@}*/

#endif /* _PREFS_H_ */
