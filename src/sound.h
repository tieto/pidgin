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
 * A type of sound.
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

typedef struct _GaimSoundUiOps
{
	void (*init)(void);
	void (*shutdown)(void);
	void (*play_file)(const char *filename);
	void (*play_event)(GaimSoundEventID event);

} GaimSoundUiOps;

/**************************************************************************/
/** @name Sound API                                                       */
/**************************************************************************/
/*@{*/

/**
 * Sets the UI sound operations
 *
 * @param ops The UI sound operations structure.
 */
void gaim_set_sound_ui_ops(GaimSoundUiOps *ops);

/**
 * Gets the UI sound operations
 *
 * @return The UI sound operations structure.
 */
GaimSoundUiOps *gaim_get_sound_ui_ops(void);

/**
 * Initializes the sound subsystem
 */
void gaim_sound_init(void);

/**
 * Shuts down the sound subsystem
 */
void gaim_sound_shutdown(void);

/**
 * Plays the specified sound file.
 *
 * @param filename The file to play.
 */
void gaim_sound_play_file(const char *filename);

/**
 * Plays the sound associated with the specified event.
 *
 * @param event The event.
 */
void gaim_sound_play_event(GaimSoundEventID event);

/*@}*/

#endif /* _GAIM_SOUND_H_ */
