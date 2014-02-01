/**
 * @file sound.h Sound API
 * @ingroup core
 * @see @ref sound-signals
 */

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
 */
#ifndef _PURPLE_SOUND_H_
#define _PURPLE_SOUND_H_

#include "account.h"

/**************************************************************************/
/** Data Structures                                                       */
/**************************************************************************/


/**
 * PurpleSoundEventID:
 * @PURPLE_SOUND_BUDDY_ARRIVE:   Buddy signs on.
 * @PURPLE_SOUND_BUDDY_LEAVE:    Buddy signs off.
 * @PURPLE_SOUND_RECEIVE:        Receive an IM.
 * @PURPLE_SOUND_FIRST_RECEIVE:  Receive an IM that starts a conv.
 * @PURPLE_SOUND_SEND:           Send an IM.
 * @PURPLE_SOUND_CHAT_JOIN:      Someone joins a chat.
 * @PURPLE_SOUND_CHAT_LEAVE:     Someone leaves a chat.
 * @PURPLE_SOUND_CHAT_YOU_SAY:   You say something in a chat.
 * @PURPLE_SOUND_CHAT_SAY:       Someone else says somthing in a chat.
 * @PURPLE_SOUND_POUNCE_DEFAULT: Default sound for a buddy pounce.
 * @PURPLE_SOUND_CHAT_NICK:      Someone says your name in a chat.
 * @PURPLE_SOUND_GOT_ATTENTION:  Got an attention.
 * @PURPLE_NUM_SOUNDS:           Total number of sounds.
 *
 * A type of sound.
 */

typedef enum
{
	PURPLE_SOUND_BUDDY_ARRIVE = 0,
	PURPLE_SOUND_BUDDY_LEAVE,
	PURPLE_SOUND_RECEIVE,
	PURPLE_SOUND_FIRST_RECEIVE,
	PURPLE_SOUND_SEND,
	PURPLE_SOUND_CHAT_JOIN,
	PURPLE_SOUND_CHAT_LEAVE,
	PURPLE_SOUND_CHAT_YOU_SAY,
	PURPLE_SOUND_CHAT_SAY,
	PURPLE_SOUND_POUNCE_DEFAULT,
	PURPLE_SOUND_CHAT_NICK,
	PURPLE_SOUND_GOT_ATTENTION,
	PURPLE_NUM_SOUNDS

} PurpleSoundEventID;

/**
 * PurpleSoundUiOps:
 *
 * Operations used by the core to request that particular sound files, or the
 * sound associated with a particular event, should be played.
 */
typedef struct _PurpleSoundUiOps
{
	void (*init)(void);
	void (*uninit)(void);
	void (*play_file)(const char *filename);
	void (*play_event)(PurpleSoundEventID event);

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
} PurpleSoundUiOps;

G_BEGIN_DECLS

/**************************************************************************/
/** @name Sound API                                                       */
/**************************************************************************/
/*@{*/

/**
 * purple_sound_play_file:
 * @filename: The file to play.
 * @account: The account that this sound is associated with, or
 *        NULL if the sound is not associated with any specific
 *        account.  This is needed for the "sounds while away?"
 *        preference to work correctly.
 *
 * Plays the specified sound file.
 */
void purple_sound_play_file(const char *filename, const PurpleAccount *account);

/**
 * purple_sound_play_event:
 * @event: The event.
 * @account: The account that this sound is associated with, or
 *        NULL if the sound is not associated with any specific
 *        account.  This is needed for the "sounds while away?"
 *        preference to work correctly.
 *
 * Plays the sound associated with the specified event.
 */
void purple_sound_play_event(PurpleSoundEventID event, const PurpleAccount *account);

/**
 * purple_sound_set_ui_ops:
 * @ops: The UI sound operations structure.
 *
 * Sets the UI sound operations
 */
void purple_sound_set_ui_ops(PurpleSoundUiOps *ops);

/**
 * purple_sound_get_ui_ops:
 *
 * Gets the UI sound operations
 *
 * Returns: The UI sound operations structure.
 */
PurpleSoundUiOps *purple_sound_get_ui_ops(void);

/**
 * purple_sound_init:
 *
 * Initializes the sound subsystem
 */
void purple_sound_init(void);

/**
 * purple_sound_uninit:
 *
 * Shuts down the sound subsystem
 */
void purple_sound_uninit(void);

/**
 * purple_sounds_get_handle:
 *
 * Returns the sound subsystem handle.
 *
 * Returns: The sound subsystem handle.
 */
void *purple_sounds_get_handle(void);

/*@}*/

G_END_DECLS

#endif /* _PURPLE_SOUND_H_ */
