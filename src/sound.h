/**
 * @file sound.h Sound API
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
 */

#ifndef _GAIM_SOUND_H_
#define _GAIM_SOUND_H_

/**************************************************************************/
/** Data Structures                                                       */
/**************************************************************************/


/**
 * A type of conversation.
 */

typedef enum _GaimSoundEventID
{
	GAIM_SOUND_BUDDY_ARRIVE = 0, /**< Buddy signs on.                       */
	GAIM_SOUND_BUDDY_LEAVE,      /**< Buddy signs off.                      */
	GAIM_SOUND_RECEIVE,          /**< Receive an IM.                        */
	GAIM_SOUND_FIRST_RECEIVE,    /**< Receive an IM that starts a conv.     */
	GAIM_SOUND_SEND,             /**< Send an IM.                           */
	GAIM_SOUND_CHAT_JOIN,        /**< Someone joins a chat.                 */
	GAIM_SOUND_CHAT_LEAVE,       /**< Someone leaves a chat.                */
	GAIM_SOUND_CHAT_YOU_SAY,     /**< You say something in a chat.          */
	GAIM_SOUND_CHAT_SAY,         /**< Someone else says somthing in a chat. */
	GAIM_SOUND_POUNCE_DEFAULT,   /**< Default sound for a buddy pounce.     */
	GAIM_SOUND_CHAT_NICK,        /**< Someone says your name in a chat.     */
	GAIM_NUM_SOUNDS              /**< Total number of sounds.               */
} GaimSoundEventID;

/**************************************************************************/
/** @name Sound API                                                       */
/**************************************************************************/
/*@{*/

/**
 * Lets the sound subsystem know when the sound output method has changed.
 */
void gaim_sound_change_output_method();

/**
 * Properly shuts down the sound system.
 */
void gaim_sound_quit();

/**
 * Plays the specified sound file.
 *
 * @param filename The file to play.
 */
void gaim_sound_play_file(char *filename);

/**
 * Plays the sound associated with the specified event.
 *
 * @param event The event.
 */
void gaim_sound_play_event(GaimSoundEventID event);

/**
 * Mutes or un-mutes sounds.
 *
 * @param mute The mute state.
 */
void gaim_sound_set_mute(gboolean mute);

/**
 * Gets mute state for sounds.
 *
 * @return The mute state.
 */
gboolean gaim_sound_get_mute();

/**
 * Mutes or un-mutes login sounds.
 *
 * @param mute The mute state.
 */
void gaim_sound_set_login_mute(gboolean mute);

/**
 * Set sound file for an event.
 *
 * @param event The event.
 * @param filename The sound file.
 */
void gaim_sound_set_event_file(GaimSoundEventID event, const char *filename);

/** Get sound file for an event.
 *
 * @param event The event.
 * @return The filename if set, otherwise @c NULL.
 */
char *gaim_sound_get_event_file(GaimSoundEventID event);

/**
 * Get the prefs option for an event.
 *
 * @param event The event.
 * @return The option.
 */
guint gaim_sound_get_event_option(GaimSoundEventID event);

/**
 * Get the label for an event.
 *
 * @param event The event.
 * @return The label.
 */
char *gaim_sound_get_event_label(GaimSoundEventID event);

/**
 * Set sound command for command mode.
 *
 * @param cmd The command.
 */
void gaim_sound_set_command(const char *cmd);

/**
 * Get sound command for command mode.
 *
 * @return The command if set, otherwise @c NULL.
 */
char *gaim_sound_get_command();

/*@}*/

#endif /* _GAIM_SOUND_H_ */
