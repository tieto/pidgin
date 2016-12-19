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

#ifndef PURPLE_CHAT_H
#define PURPLE_CHAT_H

/**
 * SECTION:chat
 * @section_id: libpurple-chat
 * @short_description: <filename>chat.h</filename>
 * @title: Chat Object
 */

#define PURPLE_TYPE_CHAT             (purple_chat_get_type())
#define PURPLE_CHAT(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_CHAT, PurpleChat))
#define PURPLE_CHAT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_CHAT, PurpleChatClass))
#define PURPLE_IS_CHAT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_CHAT))
#define PURPLE_IS_CHAT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_CHAT))
#define PURPLE_CHAT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_CHAT, PurpleChatClass))

typedef struct _PurpleChat PurpleChat;
typedef struct _PurpleChatClass PurpleChatClass;

#include "blistnode.h"
#include "group.h"

/**
 * PurpleChat:
 *
 * A chat on the buddy list.
 */
struct _PurpleChat {
	PurpleBlistNode node;
};

struct _PurpleChatClass {
	PurpleBlistNodeClass node_class;

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

/**
 * purple_chat_get_type:
 *
 * Returns: The #GType for the #PurpleChat object.
 */
GType purple_chat_get_type(void);

/**
 * purple_chat_new:
 * @account:    The account this chat will get added to
 * @alias:      The alias of the new chat
 * @components: The info the protocol needs to join the chat.  The
 *                   hash function should be g_str_hash() and the
 *                   equal function should be g_str_equal().
 *
 * Creates a new chat for the buddy list
 *
 * Returns:           A newly allocated chat
 */
PurpleChat *purple_chat_new(PurpleAccount *account, const char *alias, GHashTable *components);

/**
 * purple_chat_get_name:
 * @chat:   The chat whose name will be returned.
 *
 * Returns the correct name to display for a blist chat.
 *
 * Returns:       The alias (if set), or first component value.
 */
const char *purple_chat_get_name(PurpleChat *chat);

/**
 * purple_chat_get_name_only:
 * @chat:   The chat whose name will be returned.
 *
 * Returns the name of the chat
 *
 * Returns:       The first component value.
 */
const char *purple_chat_get_name_only(PurpleChat *chat);

/**
 * purple_chat_set_alias:
 * @chat:   The chat
 * @alias:  The alias
 *
 * Sets the alias for a blist chat.
 */
void purple_chat_set_alias(PurpleChat *chat, const char *alias);

/**
 * purple_chat_get_group:
 * @chat: The chat.
 *
 * Returns the group of which the chat is a member.
 *
 * Returns: The parent group, or %NULL if the chat is not in a group.
 */
PurpleGroup *purple_chat_get_group(PurpleChat *chat);

/**
 * purple_chat_get_account:
 * @chat:  The chat.
 *
 * Returns the account the chat belongs to.
 *
 * Returns:  The account the chat belongs to.
 */
PurpleAccount *purple_chat_get_account(PurpleChat *chat);

/**
 * purple_chat_get_components:
 * @chat:  The chat.
 *
 * Get a hashtable containing information about a chat.
 *
 * Returns: (transfer none):  The hashtable.
 */
GHashTable *purple_chat_get_components(PurpleChat *chat);

G_END_DECLS

#endif /* PURPLE_CHAT_H */
