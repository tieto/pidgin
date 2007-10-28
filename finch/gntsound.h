/**
 * @file gntsound.h GNT Sound API
 * @ingroup finch
 */

/* finch
 *
 * Finch is the legal property of its developers, whose names are too numerous
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
#ifndef _GNT_SOUND_H
#define _GNT_SOUND_H

#include "sound.h"

/**********************************************************************/
/** @name GNT Sound API                                               */
/**********************************************************************/
/*@{*/

/**
 * Get the name of the active sound profile.
 *
 * @return The name of the profile
 *
 * @since 2.1.0
 */
const char *finch_sound_get_active_profile(void);

/**
 * Set the active profile.  If the profile doesn't exist, nothing is changed.
 * 
 * @param name  The name of the profile
 *
 * @since 2.1.0
 */
void finch_sound_set_active_profile(const char *name);

/**
 * Get a list of available sound profiles.
 *
 * @return A list of strings denoting sound profile names.
 *         Caller must free the list (but not the data).
 *
 * @since 2.1.0
 */
GList *finch_sound_get_profiles(void);

/**
 * Determine whether any sound will be played or not.
 *
 * @return Returns FALSE if preference is set to 'No sound', or if volume is
 *         set to zero.
 *
 * @since 2.2.0
 */
gboolean finch_sound_is_enabled(void);

/**
 * Gets GNT sound UI ops.
 *
 * @return The UI operations structure.
 *
 * @since 2.1.0
 */
PurpleSoundUiOps *finch_sound_get_ui_ops(void);

/**
 * Show the sound settings dialog.
 *
 * @since 2.1.0
 */
void finch_sounds_show_all(void);

/*@}*/

#endif
