/*
 * nmmessage.h
 *
 * Copyright © 2004 Unpublished Work of Novell, Inc. All Rights Reserved.
 *
 * THIS WORK IS AN UNPUBLISHED WORK OF NOVELL, INC. NO PART OF THIS WORK MAY BE
 * USED, PRACTICED, PERFORMED, COPIED, DISTRIBUTED, REVISED, MODIFIED,
 * TRANSLATED, ABRIDGED, CONDENSED, EXPANDED, COLLECTED, COMPILED, LINKED,
 * RECAST, TRANSFORMED OR ADAPTED WITHOUT THE PRIOR WRITTEN CONSENT OF NOVELL,
 * INC. ANY USE OR EXPLOITATION OF THIS WORK WITHOUT AUTHORIZATION COULD SUBJECT
 * THE PERPETRATOR TO CRIMINAL AND CIVIL LIABILITY.
 * 
 * AS BETWEEN [GAIM] AND NOVELL, NOVELL GRANTS [GAIM] THE RIGHT TO REPUBLISH
 * THIS WORK UNDER THE GPL (GNU GENERAL PUBLIC LICENSE) WITH ALL RIGHTS AND
 * LICENSES THEREUNDER.  IF YOU HAVE RECEIVED THIS WORK DIRECTLY OR INDIRECTLY
 * FROM [GAIM] AS PART OF SUCH A REPUBLICATION, YOU HAVE ALL RIGHTS AND LICENSES
 * GRANTED BY [GAIM] UNDER THE GPL.  IN CONNECTION WITH SUCH A REPUBLICATION, IF
 * ANYTHING IN THIS NOTICE CONFLICTS WITH THE TERMS OF THE GPL, SUCH TERMS
 * PREVAIL.
 *
 */

#ifndef __NM_MESSAGE_H__
#define __NM_MESSAGE_H__

typedef struct _NMMessage NMMessage;

#include "nmconference.h"

/**
 * Creates a new message. 
 *
 * The returned message should be released by calling
 * nm_release_message
 *
 * @param	text	The message text
 * @return			A newly allocated message	
 */
NMMessage *nm_create_message(const char *text);

/**
 * Releases a message.
 *
 * @param	msg		The message
 */
void nm_release_message(NMMessage * msg);

/**
 * Returns the message text
 *
 * @param	msg		The message
 * @return 			The message text
 */
const char *nm_message_get_text(NMMessage * msg);

/**
 * Sets the conference object for a message
 *
 * @param	msg		The message
 * @param	conf	The conference to associate with the message
 * @return			RVALUE_OK on success
 */
void nm_message_set_conference(NMMessage * msg, NMConference * conf);

/**
 * Returns the conference object associated with the message
 *
 * Note: this does not increment the reference count for the 
 * conference and the conference should NOT be released with 
 * nm_release_conference. If the reference needs to be kept
 * around nm_conference_add_ref should be called.
 *
 * @param	msg		The message
 * @return			The conference associated with this message
 */
NMConference *nm_message_get_conference(NMMessage * msg);

#endif
