/**
 * @file gntconv.h GNT Conversation API
 * @ingroup gntui
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
#ifndef _GNT_CONV_H
#define _GNT_CONV_H

#include "conversation.h"

/***************************************************************************
 * @name GNT Conversations API
 ***************************************************************************/
/*@{*/

/**
 * Get the ui-functions.
 *
 * @return The GaimConversationUiOps populated with the appropriate functions.
 */
GaimConversationUiOps *gg_conv_get_ui_ops(void);

/**
 * Perform the necessary initializations.
 */
void gg_conversation_init(void);

/**
 * Perform the necessary uninitializations.
 */
void gg_conversation_uninit(void);

/**
 * Set a conversation as active in a contactized conversation
 *
 * @param conv The conversation to make active.
 */
void gg_conversation_set_active(GaimConversation *conv);

/*@}*/

#endif
