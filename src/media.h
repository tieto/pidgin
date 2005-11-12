/**
 * @file media.h Voice and Video API
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
#ifndef _GAIM_MEDIA_H_
#define _GAIM_MEDIA_H_

typedef struct _GaimVoiceChat GaimVoiceChat;
typedef struct _GaimMediaPrplOps GaimMediaPrplOps;

#include "connection.h"

/* Forward declarations so I needn't #include mediastreamer headers in
 * nearly every single file */
struct _MSFilter;
struct _MSSync;

/**
 * The state of a VoiceChat that should be reflected in the UI
 */
typedef enum {
	GAIM_MEDIA_STATE_CALLING,       /**< An outgoing call is waiting to be answered */
	GAIM_MEDIA_STATE_RECEIVING,     /**< An incoming call is waiting to be answered */
	GAIM_MEDIA_STATE_IN_PROGRESS,   /**< The call is connected                      */
	GAIM_MEDIA_STATE_ERROR,         /**< Some error occured                         */
} GaimMediaState;

/**
 * UI Operations
 */
typedef struct {
	void (*new_voice_chat)(GaimVoiceChat *vc);                       /**< A new voice chat is created             */
	void (*destroy)(GaimVoiceChat *vc);                              /**< The voice chat has been destroyed       */
	void (*state_change)(GaimVoiceChat *vc, GaimMediaState state);   /**< The state of the voice chat has changed */
} GaimMediaUiOps;

/**
 * PRPL Operations
 */
struct _GaimMediaPrplOps {
	void (*call)(GaimConnection *gc, const char *who);               /**< Request an outgoing call to 'who'    */
	void (*init_filters)(GaimVoiceChat *);                           /**< Create the media stream for the call */
	void (*accept)(GaimVoiceChat *);                                 /**< Accept an incoming call              */
	void (*reject)(GaimVoiceChat *);                                 /**< Reject an incoming call              */
	void (*terminate)(GaimVoiceChat *);                              /**< Terminate an in-progress call        */
};

#ifdef HAVE_VV

/**
 * Initializes mediastreamer and ortp
 */
void gaim_media_init(void);

/**************************************************************************/
/** @name Voice Chat API **************************************************/
/**************************************************************************/
/*@{*/

/**
 * Creates a new voice chat
 * This function creates a new voice chat object, and tells the UI about it. The UI
 * probably won't want to do anything, until the state is changed to Incoming or Outgoing,
 * but the UI should initialize any data structures it needs on this call
 *
 * @param gc   The connection this chat is happening on
 * @param name The person on the other end of the call
 * @return     The new voice chat
 */
GaimVoiceChat *gaim_voice_chat_new(GaimConnection *gc, const char *name);

/**
 * Destroys a voice chat
 *
 * @param vc  The voice chat to destroy
 */
void gaim_voice_chat_destroy(GaimVoiceChat *vc);

/**
 * Acessor function of the name of the other user on the voice chat
 *
 * @param vc  The voice chat
 * @return    The name
 */
const char *gaim_voice_chat_get_name(GaimVoiceChat *vc);

/**
 * Accessor for the GaimConnection of the voice chat
 *
 * @param vc  The voice chat
 * @return    The GaimConnection
 */
GaimConnection *gaim_voice_chat_get_connection(GaimVoiceChat *vc);

/**
 * Accessor for the UI data
 *
 * @param vc  The voice chat
 * @return    The UI data
 */
void *gaim_voice_chat_get_ui_data(GaimVoiceChat *vc);

/**
 * Mutator for the UI Data
 *
 * @param vc   The voice chat
 * @param data The data
 */
void gaim_voice_chat_set_ui_data(GaimVoiceChat *vc, void *data);

/**
 * Accessor for the protocol data
 *
 * @param vc   The voice chat
 * @return     The protocol data
 */
void *gaim_voice_chat_get_proto_data(GaimVoiceChat *vc);

/**
 * Mutator for the protocol data
 *
 * @param vc   The voice chat
 * @param data The protocol data
 */
void gaim_voice_chat_set_proto_data(GaimVoiceChat *vc, void *data);

/**
 * Accessor for the state
 *
 * @param vc  The voice chat
 * @return    The state
 */
GaimMediaState gaim_voice_chat_get_state(GaimVoiceChat *vc);

/**
 * Mutator for the state
 *
 * @param vc    The voice chat
 * @param state The state
 */
void gaim_voice_chat_set_state(GaimVoiceChat *vc, GaimMediaState state);

/**
 * Accepts an incoming voice chat
 *
 * @param vc  The voice chat
 */
void gaim_voice_chat_accept(GaimVoiceChat *vc);

/**
 * Rejects an incoming voice chat
 *
 * @param vc  The voice chat
 */
void gaim_voice_chat_reject(GaimVoiceChat *vc);

/**
 * Terminates an in-progress voice chat
 *
 * @param vc  The voice chat
 */
void gaim_voice_chat_terminate(GaimVoiceChat *vc);

/**
 * Accessor for the microphone and speaker MSFilter objects
 *
 * @param vc         The voice chat
 * @param microphone A pointer to return the microphone filter in, or NULL.
 * @param speaker    A poitner to reutrn the speaker filter in, or NULL.
 */
void gaim_voice_chat_get_filters(GaimVoiceChat *vc, struct _MSFilter **microphone, struct _MSFilter **speaker);

/**
 * Accessor for the Mediastreamer timer
 *
 * @param vc        The voice chat
 * @return          The timer
 */
struct _MSSync *gaim_voice_chat_get_timer(GaimVoiceChat *vc);

/*@}*/

/**************************************************************************/
/** @name UI Registration Functions                                       */
/**************************************************************************/
/*@{*/

/**
 * Sets the UI operations structure to be used for the buddy list.
 *
 * @param ops The ops struct.
 */
void gaim_media_set_ui_ops(GaimMediaUiOps *ops);

/**
 * Returns the UI operations structure to be used for the buddy list.
 *
 * @return The UI operations structure.
 */
GaimMediaUiOps *gaim_media_get_ui_ops(void);

/*@}*/

#endif  /* HAVE_VV */

#endif  /* _GAIM_MEDIA_H_ */
