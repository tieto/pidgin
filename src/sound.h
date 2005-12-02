/**
 * @file sound.h Sound API
 * @ingroup core
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
#ifndef _GAIM_SOUND_H_
#define _GAIM_SOUND_H_

#include "account.h"

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
	void (*uninit)(void);
	void (*play_file)(const char *filename);
	void (*play_event)(GaimSoundEventID event);

} GaimSoundUiOps;

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************/
/** @name Sound API                                                       */
/**************************************************************************/
/*@{*/

/**
 * Plays the specified sound file.
 *
 * @param filename The file to play.
 * @param account The account that this sound is associated with, or
 *        NULL if the sound is not associated with any specific
 *        account.  This is needed for the "sounds while away?"
 *        preference to work correctly.
 */
void gaim_sound_play_file(const char *filename, const GaimAccount *account);

/**
 * Plays the sound associated with the specified event.
 *
 * @param event The event.
 * @param account The account that this sound is associated with, or
 *        NULL if the sound is not associated with any specific
 *        account.  This is needed for the "sounds while away?"
 *        preference to work correctly.
 */
void gaim_sound_play_event(GaimSoundEventID event, const GaimAccount *account);

/**
 * Sets the UI sound operations
 *
 * @param ops The UI sound operations structure.
 */
void gaim_sound_set_ui_ops(GaimSoundUiOps *ops);

/**
 * Gets the UI sound operations
 *
 * @return The UI sound operations structure.
 */
GaimSoundUiOps *gaim_sound_get_ui_ops(void);

/**
 * Initializes the sound subsystem
 */
void gaim_sound_init(void);

/**
 * Shuts down the sound subsystem
 */
void gaim_sound_uninit(void);

/**
 * Returns the sound subsystem handle.
 *
 * @return The sound subsystem handle.
 */
void *gaim_sounds_get_handle(void);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* _GAIM_SOUND_H_ */
