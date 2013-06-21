/**
 * @file conversations.h Conversations API
 * @ingroup core
 * @see @ref conversation-signals
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
#ifndef _PURPLE_CONVERSATIONS_H_
#define _PURPLE_CONVERSATIONS_H_

#include "conversation.h"

G_BEGIN_DECLS

/**************************************************************************/
/** @name Conversations Subsystem                                         */
/**************************************************************************/
/*@{*/

/**
 * Returns a list of all conversations.
 *
 * This list includes both IMs and chats.
 *
 * @constreturn A GList of all conversations.
 */
GList *purple_conversations_get(void);

/**
 * Returns a list of all IMs.
 *
 * @constreturn A GList of all IMs.
 */
GList *purple_conversations_get_ims(void);

/**
 * Returns a list of all chats.
 *
 * @constreturn A GList of all chats.
 */
GList *purple_conversations_get_chats(void);

/**
 * Finds a conversation with the specified type, name, and Purple account.
 *
 * @param type The type of the conversation.
 * @param name The name of the conversation.
 * @param account The purple_account associated with the conversation.
 *
 * @return The conversation if found, or @c NULL otherwise.
 */
PurpleConversation *purple_conversations_find_with_account(
		PurpleConversationType type, const char *name,
		const PurpleAccount *account);

/**
 * Finds a chat with the specified chat ID.
 *
 * @param gc The purple_connection.
 * @param id The chat ID.
 *
 * @return The chat conversation.
 */
PurpleChatConversation *purple_conversations_find_chat(const PurpleConnection *gc, int id);

/**
 * Sets the default conversation UI operations structure.
 *
 * @param ops  The UI conversation operations structure.
 */
void purple_conversations_set_ui_ops(PurpleConversationUiOps *ops);

/**
 * Returns the conversation subsystem handle.
 *
 * @return The conversation subsystem handle.
 */
void *purple_conversations_get_handle(void);

/**
 * Initializes the conversation subsystem.
 */
void purple_conversations_init(void);

/**
 * Uninitializes the conversation subsystem.
 */
void purple_conversations_uninit(void);

/*@}*/

G_END_DECLS

#endif /* _PURPLE_CONVERSATIONS_H_ */
