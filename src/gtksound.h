/**
 * @file gtksound.h GTK+ Sound API
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
 */
#ifndef _GAIM_GTKSOUND_H_
#define _GAIM_GTKSOUND_H_

/**************************************************************************/
/** @name GTK+ Sound API                                                  */
/**************************************************************************/
/*@{*/

/**
 * Gets GTK Sound UI opsA
 *
 * @return UI operations struct
 */
GaimSoundUiOps *gaim_gtk_sound_get_ui_ops(void);

/**
 * Mutes or un-mutes login sounds.
 *
 * @param mute The mute state.
 */
void gaim_gtk_sound_set_login_mute(gboolean mute);

/**
 * Get the prefs option for an event.
 *
 * @param event The event.
 * @return The option.
 */
const char *gaim_gtk_sound_get_event_option(GaimSoundEventID event);

/**
 * Get the label for an event.
 *
 * @param event The event.
 * @return The label.
 */
char *gaim_gtk_sound_get_event_label(GaimSoundEventID event);

/*@}*/

#endif /* _GAIM_GTKSOUND_H_ */
