/**
 * @file gtksound.h GTK+ Sound API
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
 */
#ifndef _PIDGINSOUND_H_
#define _PIDGINSOUND_H_

#include "sound.h"

/**************************************************************************/
/** @name GTK+ Sound API                                                  */
/**************************************************************************/
/*@{*/

/**
 * Get the prefs option for an event.
 *
 * @param event The event.
 * @return The option.
 */
const char *pidgin_sound_get_event_option(PurpleSoundEventID event);

/**
 * Get the label for an event.
 *
 * @param event The event.
 * @return The label.
 */
const char *pidgin_sound_get_event_label(PurpleSoundEventID event);

/**
 * Gets GTK+ sound UI ops.
 *
 * @return The UI operations structure.
 */
PurpleSoundUiOps *pidgin_sound_get_ui_ops(void);

/**
 * Get the handle for the GTK+ sound system.
 *
 * @return The handle to the sound system
 */
void *pidgin_sound_get_handle(void);

/*@}*/

#endif /* _PIDGINSOUND_H_ */
