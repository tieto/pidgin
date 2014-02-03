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
/**
 * SECTION:blistnodetypes
 * @section_id: libpurple-blistnodetypes
 * @title: Buddy, Chat, Contact and Group node objects
 * @short_description: <filename>blistnodetypes.h</filename>
 */

#ifndef _PURPLE_BLISTNODE_TYPES_H_
#define _PURPLE_BLISTNODE_TYPES_H_

#include "blistnode.h"

#define PURPLE_TYPE_BUDDY             (purple_buddy_get_type())
#define PURPLE_BUDDY(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_BUDDY, PurpleBuddy))
#define PURPLE_BUDDY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_BUDDY, PurpleBuddyClass))
#define PURPLE_IS_BUDDY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_BUDDY))
#define PURPLE_IS_BUDDY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_BUDDY))
#define PURPLE_BUDDY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_BUDDY, PurpleBuddyClass))

typedef struct _PurpleBuddy PurpleBuddy;
typedef struct _PurpleBuddyClass PurpleBuddyClass;

#define PURPLE_TYPE_CONTACT             (purple_contact_get_type())
#define PURPLE_CONTACT(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_CONTACT, PurpleContact))
#define PURPLE_CONTACT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_CONTACT, PurpleContactClass))
#define PURPLE_IS_CONTACT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_CONTACT))
#define PURPLE_IS_CONTACT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_CONTACT))
#define PURPLE_CONTACT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_CONTACT, PurpleContactClass))

typedef struct _PurpleContact PurpleContact;
typedef struct _PurpleContactClass PurpleContactClass;

#define PURPLE_TYPE_GROUP             (purple_group_get_type())
#define PURPLE_GROUP(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_GROUP, PurpleGroup))
#define PURPLE_GROUP_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_GROUP, PurpleGroupClass))
#define PURPLE_IS_GROUP(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_GROUP))
#define PURPLE_IS_GROUP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_GROUP))
#define PURPLE_GROUP_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_GROUP, PurpleGroupClass))

typedef struct _PurpleGroup PurpleGroup;
typedef struct _PurpleGroupClass PurpleGroupClass;

#define PURPLE_TYPE_CHAT             (purple_chat_get_type())
#define PURPLE_CHAT(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_CHAT, PurpleChat))
#define PURPLE_CHAT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_CHAT, PurpleChatClass))
#define PURPLE_IS_CHAT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_CHAT))
#define PURPLE_IS_CHAT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_CHAT))
#define PURPLE_CHAT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_CHAT, PurpleChatClass))

typedef struct _PurpleChat PurpleChat;
typedef struct _PurpleChatClass PurpleChatClass;

#include "account.h"
#include "buddyicon.h"
#include "media.h"
#include "presence.h"
#include "status.h"

#define PURPLE_BUDDY_IS_ONLINE(b) \
	(PURPLE_IS_BUDDY(b) \
	&& purple_account_is_connected(purple_buddy_get_account(PURPLE_BUDDY(b))) \
	&& purple_presence_is_online(purple_buddy_get_presence(PURPLE_BUDDY(b))))

#define PURPLE_BLIST_NODE_NAME(n) \
	(PURPLE_IS_CHAT(n) ? purple_chat_get_name(PURPLE_CHAT(n)) : \
	PURPLE_IS_BUDDY(n) ? purple_buddy_get_name(PURPLE_BUDDY(n)) : NULL)

/**************************************************************************/
/* Data Structures                                                        */
/**************************************************************************/
/**
 * PurpleBuddy:
 * @node: The node that this buddy inherits from
 *
 * A buddy on the buddy list.
 */
struct _PurpleBuddy {
	PurpleBlistNode node;
};

/**
 * PurpleBuddyClass:
 *
 * The base class for all #PurpleBuddy's.
 */
struct _PurpleBuddyClass {
	PurpleBlistNodeClass node_class;

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * PurpleContact:
 * @counting: The counting node that this contact inherits from. This keeps
 *            track of the counts of the buddies under this contact.
 *
 * A contact on the buddy list.
 */
struct _PurpleContact {
	PurpleCountingNode counting;
};

/**
 * PurpleContactClass:
 *
 * The base class for all #PurpleContact's.
 */
struct _PurpleContactClass {
	PurpleCountingNodeClass counting_class;

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * PurpleGroup:
 * @counting: The counting node that this group inherits from. This keeps track
 *            of the counts of the chats and contacts under this group.
 *
 * A group on the buddy list.
 */
struct _PurpleGroup {
	PurpleCountingNode counting;
};

/**
 * PurpleGroupClass:
 *
 * The base class for all #PurpleGroup's.
 */
struct _PurpleGroupClass {
	PurpleCountingNodeClass counting_class;

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * PurpleChat:
 * @node: The node that this chat inherits from
 *
 * A chat on the buddy list.
 */
struct _PurpleChat {
	PurpleBlistNode node;
};

/**
 * PurpleChatClass:
 *
 * The base class for all #PurpleChat's.
 */
struct _PurpleChatClass {
	PurpleBlistNodeClass node_class;

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

/**************************************************************************/
/** @name Buddy API                                                       */
/**************************************************************************/
/*@{*/

/**
 * purple_buddy_get_type:
 *
 * Returns the GType for the PurpleBuddy object.
 */
GType purple_buddy_get_type(void);

/**
 * purple_buddy_new:
 * @account:    The account this buddy will get added to
 * @name:       The name of the new buddy
 * @alias:      The alias of the new buddy (or NULL if unaliased)
 *
 * Creates a new buddy.
 *
 * This function only creates the PurpleBuddy. Use purple_blist_add_buddy
 * to add the buddy to the list and purple_account_add_buddy to sync up
 * with the server.
 *
 * Returns:           A newly allocated buddy
 *
 * @see purple_account_add_buddy
 * @see purple_blist_add_buddy
 */
PurpleBuddy *purple_buddy_new(PurpleAccount *account, const char *name, const char *alias);

/**
 * purple_buddy_set_icon:
 * @buddy: The buddy.
 * @icon:  The buddy icon.
 *
 * Sets a buddy's icon.
 *
 * This should only be called from within Purple. You probably want to
 * call purple_buddy_icon_set_data().
 *
 * @see purple_buddy_icon_set_data()
 */
void purple_buddy_set_icon(PurpleBuddy *buddy, PurpleBuddyIcon *icon);

/**
 * purple_buddy_get_icon:
 * @buddy: The buddy.
 *
 * Returns a buddy's icon.
 *
 * Returns: The buddy icon.
 */
PurpleBuddyIcon *purple_buddy_get_icon(const PurpleBuddy *buddy);

/**
 * purple_buddy_get_account:
 * @buddy: The buddy.
 *
 * Returns a buddy's account.
 *
 * Returns: The account
 */
PurpleAccount *purple_buddy_get_account(const PurpleBuddy *buddy);

/**
 * purple_buddy_set_name:
 * @buddy: The buddy.
 * @name:  The name.
 *
 * Sets a buddy's name
 */
void purple_buddy_set_name(PurpleBuddy *buddy, const char *name);

/**
 * purple_buddy_get_name:
 * @buddy: The buddy.
 *
 * Returns a buddy's name
 *
 * Returns: The name.
 */
const char *purple_buddy_get_name(const PurpleBuddy *buddy);

/**
 * purple_buddy_get_protocol_data:
 * @buddy: The buddy.
 *
 * Returns a buddy's protocol-specific data.
 *
 * This should only be called from the associated protocol.
 *
 * Returns:      The protocol data.
 *
 * @see purple_buddy_set_protocol_data()
 */
gpointer purple_buddy_get_protocol_data(const PurpleBuddy *buddy);

/**
 * purple_buddy_set_protocol_data:
 * @buddy: The buddy.
 * @data:  The data.
 *
 * Sets a buddy's protocol-specific data.
 *
 * This should only be called from the associated protocol.
 *
 * @see purple_buddy_get_protocol_data()
 */
void purple_buddy_set_protocol_data(PurpleBuddy *buddy, gpointer data);

/**
 * purple_buddy_get_contact:
 * @buddy: The buddy.
 *
 * Returns a buddy's contact.
 *
 * Returns: The buddy's contact.
 */
PurpleContact *purple_buddy_get_contact(PurpleBuddy *buddy);

/**
 * purple_buddy_get_presence:
 * @buddy: The buddy.
 *
 * Returns a buddy's presence.
 *
 * Returns: The buddy's presence.
 */
PurplePresence *purple_buddy_get_presence(const PurpleBuddy *buddy);

/**
 * purple_buddy_update_status:
 * @buddy:      The buddy whose status has changed.
 * @old_status: The status from which we are changing.
 *
 * Updates a buddy's status.
 *
 * This should only be called from within Purple.
 */
void purple_buddy_update_status(PurpleBuddy *buddy, PurpleStatus *old_status);

/**
 * purple_buddy_get_media_caps:
 * @buddy: The buddy.
 *
 * Gets the media caps from a buddy.
 *
 * Returns:      The media caps.
 */
PurpleMediaCaps purple_buddy_get_media_caps(const PurpleBuddy *buddy);

/**
 * purple_buddy_set_media_caps:
 * @buddy:      The PurpleBuddy.
 * @media_caps: The PurpleMediaCaps.
 *
 * Sets the media caps for a buddy.
 */
void purple_buddy_set_media_caps(PurpleBuddy *buddy, PurpleMediaCaps media_caps);

/**
 * purple_buddy_get_alias_only:
 * @buddy:   The buddy whose alias will be returned.
 *
 * Returns the alias of a buddy.
 *
 * Returns:        The alias (if set), server alias (if set),
 *                or NULL.
 */
const char *purple_buddy_get_alias_only(PurpleBuddy *buddy);

/**
 * purple_buddy_set_server_alias:
 * @buddy:  The buddy.
 * @alias:  The server alias to be set.
 *
 * Sets the server alias for a buddy.
 */
void purple_buddy_set_server_alias(PurpleBuddy *buddy, const char *alias);

/**
 * purple_buddy_get_server_alias:
 * @buddy:  The buddy whose server alias will be returned
 *
 * Gets the server alias for a buddy.
 *
 * Returns:  The server alias, or NULL if it is not set.
 */
const char *purple_buddy_get_server_alias(PurpleBuddy *buddy);

/**
 * purple_buddy_get_contact_alias:
 * @buddy:  The buddy whose alias will be returned
 *
 * Returns the correct name to display for a buddy, taking the contact alias
 * into account. In order of precedence: the buddy's alias; the buddy's
 * contact alias; the buddy's server alias; the buddy's user name.
 *
 * Returns:       The appropriate name or alias, or NULL.
 *
 */
const char *purple_buddy_get_contact_alias(PurpleBuddy *buddy);

/**
 * purple_buddy_get_alias:
 * @buddy:   The buddy whose alias will be returned.
 *
 * Returns the correct name to display for a buddy. In order of precedence:
 * the buddy's local alias; the buddy's server alias; the buddy's contact alias;
 * the buddy's user name.
 *
 * Returns:        The appropriate name or alias, or NULL
 */
const char *purple_buddy_get_alias(PurpleBuddy *buddy);

/**
 * purple_buddy_set_local_alias:
 * @buddy:  The buddy
 * @alias:  The local alias for the buddy
 *
 * Sets the local alias for the buddy.
 */
void purple_buddy_set_local_alias(PurpleBuddy *buddy, const char *alias);

/**
 * purple_buddy_get_local_alias:
 * @buddy:  The buddy
 *
 * Returns the local alias for the buddy, or %NULL if none exists.
 *
 * Returns:       The local alias for the buddy
 */
const char *purple_buddy_get_local_alias(PurpleBuddy *buddy);

/**
 * purple_buddy_get_group:
 * @buddy:   The buddy
 *
 * Returns the group of which the buddy is a member.
 *
 * Returns:        The group or NULL if the buddy is not in a group
 */
PurpleGroup *purple_buddy_get_group(PurpleBuddy *buddy);

/*@}*/

/**************************************************************************/
/** @name Contact API                                                     */
/**************************************************************************/
/*@{*/

/**
 * purple_contact_get_type:
 *
 * Returns the GType for the PurpleContact object.
 */
GType purple_contact_get_type(void);

/**
 * purple_contact_new:
 *
 * Creates a new contact
 *
 * Returns:       A new contact struct
 */
PurpleContact *purple_contact_new(void);

/**
 * purple_contact_get_group:
 * @contact:  The contact
 *
 * Gets the PurpleGroup from a PurpleContact
 *
 * Returns:         The group
 */
PurpleGroup *purple_contact_get_group(const PurpleContact *contact);

/**
 * purple_contact_get_priority_buddy:
 * @contact:  The contact
 *
 * Returns the highest priority buddy for a given contact.
 *
 * Returns: The highest priority buddy
 */
PurpleBuddy *purple_contact_get_priority_buddy(PurpleContact *contact);

/**
 * purple_contact_set_alias:
 * @contact:  The contact
 * @alias:    The alias
 *
 * Sets the alias for a contact.
 */
void purple_contact_set_alias(PurpleContact *contact, const char *alias);

/**
 * purple_contact_get_alias:
 * @contact:  The contact
 *
 * Gets the alias for a contact.
 *
 * Returns:  The alias, or NULL if it is not set.
 */
const char *purple_contact_get_alias(PurpleContact *contact);

/**
 * purple_contact_on_account:
 * @contact:  The contact to search through.
 * @account:  The account.
 *
 * Determines whether an account owns any buddies in a given contact
 *
 * Returns: TRUE if there are any buddies from account in the contact, or FALSE otherwise.
 */
gboolean purple_contact_on_account(PurpleContact *contact, PurpleAccount *account);

/**
 * purple_contact_invalidate_priority_buddy:
 * @contact:  The contact
 *
 * Invalidates the priority buddy so that the next call to
 * purple_contact_get_priority_buddy recomputes it.
 */
void purple_contact_invalidate_priority_buddy(PurpleContact *contact);

/**
 * purple_contact_merge:
 * @source:  The contact to merge
 * @node:    The place to merge to (a buddy or contact)
 *
 * Merges two contacts
 *
 * All of the buddies from source will be moved to target
 */
void purple_contact_merge(PurpleContact *source, PurpleBlistNode *node);

/*@}*/

/**************************************************************************/
/** @name Chat API                                                        */
/**************************************************************************/
/*@{*/

/**
 * purple_chat_get_type:
 *
 * Returns the GType for the PurpleChat object.
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

/*@}*/

/**************************************************************************/
/** @name Group API                                                       */
/**************************************************************************/
/*@{*/

/**
 * purple_group_get_type:
 *
 * Returns the GType for the PurpleGroup object.
 */
GType purple_group_get_type(void);

/**
 * purple_group_new:
 * @name:   The name of the new group
 *
 * Creates a new group
 *
 * You can't have more than one group with the same name.  Sorry.  If you pass
 * this the name of a group that already exists, it will return that group.
 *
 * Returns:       A new group struct
*/
PurpleGroup *purple_group_new(const char *name);

/**
 * purple_group_get_accounts:
 * @g: The group
 *
 * Returns a list of accounts that have buddies in this group
 *
 * Returns: A GSList of accounts (which must be freed), or NULL if the group
 *         has no accounts.
 */
GSList *purple_group_get_accounts(PurpleGroup *g);

/**
 * purple_group_on_account:
 * @g:       The group to search through.
 * @account: The account.
 *
 * Determines whether an account owns any buddies in a given group
 *
 * Returns: TRUE if there are any buddies in the group, or FALSE otherwise.
 */
gboolean purple_group_on_account(PurpleGroup *g, PurpleAccount *account);

/**
 * purple_group_set_name:
 * @group: The group.
 * @name:  The name of the group.
 *
 * Sets the name of a group.
 */
void purple_group_set_name(PurpleGroup *group, const char *name);

/**
 * purple_group_get_name:
 * @group: The group.
 *
 * Returns the name of a group.
 *
 * Returns: The name of the group.
 */
const char *purple_group_get_name(PurpleGroup *group);

/*@}*/

G_END_DECLS

#endif /* _PURPLE_BLISTNODE_TYPES_H_ */
