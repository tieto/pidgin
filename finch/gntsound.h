/**
 * @file gntsound.h GNT Sound API
 * @ingroup finch
 *
 * finch
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _GNT_SOUND_H
#define _GNT_SOUND_H

#include "sound.h"

/**********************************************************************/
/** @name GNT Sound API																								*/
/**********************************************************************/
/*@{*/

/**
* Get the prefs option for an event.
*
* @param event The event.
* @return The option.
*/
const char *finch_sound_get_event_option(PurpleSoundEventID event);

/**
* Get the label for an event.
*
* @param event The event.
* @return The label.
*/
const char *finch_sound_get_event_label(PurpleSoundEventID event);

/*
 * Get the name of the active sound profile.
 *
 * @return the name
 */
const char *finch_sound_get_active_profile(void);

/**
 * Set the active profile.  If the profile doesn't exist, nothing is changed.
 * 
 * 
 */
void finch_sound_set_active_profile(const char *name);

/**
 * Returns whether a profile exists or not.
 *
 * @parame name The name of the profile to check for
 * return Existance value
 *
 */
gboolean finch_sound_profile_exists(const char *name);


/**
 * Get a list of available sound profiles.
 *
 * @return A list of strings denoting sound profile names.  Free this list when you're done with it.
 */
GList *finch_sound_get_profiles(void);

/**
* Gets GNT sound UI ops.
*
* @return The UI operations structure.
*/
PurpleSoundUiOps *finch_sound_get_ui_ops(void);

/**
 * Show the sound settings dialog.
 */
void finch_sounds_show_all(void);

/**
* Get the handle for the GNT sound system.
*
* @return The handle to the sound system
*/
void *finch_sound_get_handle(void);

/*@}*/

#endif
