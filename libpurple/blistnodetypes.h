/**
 * @file blistnodetypes.h Buddy, Chat, Contact and Group API
 * @ingroup core
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
#ifndef _PURPLE_BLISTNODE_TYPES_H_
#define _PURPLE_BLISTNODE_TYPES_H_

#include "blistnodes.h"

#define PURPLE_TYPE_BUDDY             (purple_buddy_get_type())
#define PURPLE_BUDDY(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_BUDDY, PurpleBuddy))
#define PURPLE_BUDDY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_BUDDY, PurpleBuddyClass))
#define PURPLE_IS_BUDDY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_BUDDY))
#define PURPLE_IS_BUDDY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_BUDDY))
#define PURPLE_BUDDY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_BUDDY, PurpleBuddyClass))

/** @copydoc _PurpleBuddy */
typedef struct _PurpleBuddy PurpleBuddy;
/** @copydoc _PurpleBuddyClass */
typedef struct _PurpleBuddyClass PurpleBuddyClass;

#define PURPLE_TYPE_CONTACT             (purple_contact_get_type())
#define PURPLE_CONTACT(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_CONTACT, PurpleContact))
#define PURPLE_CONTACT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_CONTACT, PurpleContactClass))
#define PURPLE_IS_CONTACT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_CONTACT))
#define PURPLE_IS_CONTACT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_CONTACT))
#define PURPLE_CONTACT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_CONTACT, PurpleContactClass))

/** @copydoc _PurpleContact */
typedef struct _PurpleContact PurpleContact;
/** @copydoc _PurpleContactClass */
typedef struct _PurpleContactClass PurpleContactClass;

#define PURPLE_TYPE_GROUP             (purple_group_get_type())
#define PURPLE_GROUP(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_GROUP, PurpleGroup))
#define PURPLE_GROUP_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_GROUP, PurpleGroupClass))
#define PURPLE_IS_GROUP(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_GROUP))
#define PURPLE_IS_GROUP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_GROUP))
#define PURPLE_GROUP_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_GROUP, PurpleGroupClass))

/** @copydoc _PurpleGroup */
typedef struct _PurpleGroup PurpleGroup;
/** @copydoc _PurpleGroupClass */
typedef struct _PurpleGroupClass PurpleGroupClass;

#define PURPLE_TYPE_CHAT             (purple_chat_get_type())
#define PURPLE_CHAT(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_CHAT, PurpleChat))
#define PURPLE_CHAT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_CHAT, PurpleChatClass))
#define PURPLE_IS_CHAT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_CHAT))
#define PURPLE_IS_CHAT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_CHAT))
#define PURPLE_CHAT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_CHAT, PurpleChatClass))

/** @copydoc _PurpleChat */
typedef struct _PurpleChat PurpleChat;
/** @copydoc _PurpleChatClass */
typedef struct _PurpleChatClass PurpleChatClass;

#include "account.h"
#include "buddyicon.h"
#include "media.h"
#include "status.h"

#define PURPLE_IS_BUDDY_ONLINE(b) \
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
 * A buddy on the buddy list.
 */
struct _PurpleBuddy {
	/** The node that this buddy inherits from */
	PurpleBListNode node;
};

/** The base class for all #PurpleBuddy's. */
struct _PurpleBuddyClass {
	/*< private >*/
	PurpleBListNodeClass node_class;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * A contact on the buddy list.
 */
struct _PurpleContact {
	/**
	 * The counting node that this contact inherits from. This keeps track
	 * of the counts of the buddies under this contact.
	 */
	PurpleCountingNode counting;
};

/** The base class for all #PurpleContact's. */
struct _PurpleContactClass {
	/*< private >*/
	PurpleCountingNodeClass counting_class;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * A group on the buddy list.
 */
struct _PurpleGroup {
	/**
	 * The counting node that this group inherits from. This keeps track
	 * of the counts of the chats and contacts under this group.
	 */
	PurpleCountingNode counting;
};

/** The base class for all #PurpleGroup's. */
struct _PurpleGroupClass {
	/*< private >*/
	PurpleCountingNodeClass counting_class;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * A chat on the buddy list.
 */
struct _PurpleChat {
	/** The node that this chat inherits from */
	PurpleBListNode node;
};

/** The base class for all #PurpleChat's. */
struct _PurpleChatClass {
	/*< private >*/
	PurpleBListNodeClass node_class;

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
 * Returns the GType for the PurpleBuddy object.
 */
GType purple_buddy_get_type(void);

/**
 * Creates a new buddy.
 *
 * This function only creates the PurpleBuddy. Use purple_blist_add_buddy
 * to add the buddy to the list and purple_account_add_buddy to sync up
 * with the server.
 *
 * @param account    The account this buddy will get added to
 * @param name       The name of the new buddy
 * @param alias      The alias of the new buddy (or NULL if unaliased)
 * @return           A newly allocated buddy
 *
 * @see purple_account_add_buddy
 * @see purple_blist_add_buddy
 */
PurpleBuddy *purple_buddy_new(PurpleAccount *account, const char *name, const char *alias);

/**
 * Sets a buddy's icon.
 *
 * This should only be called from within Purple. You probably want to
 * call purple_buddy_icon_set_data().
 *
 * @param buddy The buddy.
 * @param icon  The buddy icon.
 *
 * @see purple_buddy_icon_set_data()
 */
void purple_buddy_set_icon(PurpleBuddy *buddy, PurpleBuddyIcon *icon);

/**
 * Returns a buddy's icon.
 *
 * @param buddy The buddy.
 *
 * @return The buddy icon.
 */
PurpleBuddyIcon *purple_buddy_get_icon(const PurpleBuddy *buddy);

/**
 * Returns a buddy's account.
 *
 * @param buddy The buddy.
 *
 * @return The account
 */
PurpleAccount *purple_buddy_get_account(const PurpleBuddy *buddy);

/**
 * Sets a buddy's name
 *
 * @param buddy The buddy.
 * @param name  The name.
 */
void purple_buddy_set_name(PurpleBuddy *buddy, const char *name);

/**
 * Returns a buddy's name
 *
 * @param buddy The buddy.
 *
 * @return The name.
 */
const char *purple_buddy_get_name(const PurpleBuddy *buddy);

/**
 * Returns a buddy's protocol-specific data.
 *
 * This should only be called from the associated prpl.
 *
 * @param buddy The buddy.
 * @return      The protocol data.
 *
 * @see purple_buddy_set_protocol_data()
 */
gpointer purple_buddy_get_protocol_data(const PurpleBuddy *buddy);

/**
 * Sets a buddy's protocol-specific data.
 *
 * This should only be called from the associated prpl.
 *
 * @param buddy The buddy.
 * @param data  The data.
 *
 * @see purple_buddy_get_protocol_data()
 */
void purple_buddy_set_protocol_data(PurpleBuddy *buddy, gpointer data);

/**
 * Returns a buddy's contact.
 *
 * @param buddy The buddy.
 *
 * @return The buddy's contact.
 */
PurpleContact *purple_buddy_get_contact(PurpleBuddy *buddy);

/**
 * Returns a buddy's presence.
 *
 * @param buddy The buddy.
 *
 * @return The buddy's presence.
 */
PurplePresence *purple_buddy_get_presence(const PurpleBuddy *buddy);

/**
 * Updates a buddy's status.
 *
 * This should only be called from within Purple.
 *
 * @param buddy      The buddy whose status has changed.
 * @param old_status The status from which we are changing.
 */
void purple_buddy_update_status(PurpleBuddy *buddy, PurpleStatus *old_status);

/**
 * Gets the media caps from a buddy.
 *
 * @param buddy The buddy.
 * @return      The media caps.
 */
PurpleMediaCaps purple_buddy_get_media_caps(const PurpleBuddy *buddy);

/**
 * Sets the media caps for a buddy.
 *
 * @param buddy      The PurpleBuddy.
 * @param media_caps The PurpleMediaCaps.
 */
void purple_buddy_set_media_caps(PurpleBuddy *buddy, PurpleMediaCaps media_caps);

/**
 * Returns the alias of a buddy.
 *
 * @param buddy   The buddy whose alias will be returned.
 * @return        The alias (if set), server alias (if set),
 *                or NULL.
 */
const char *purple_buddy_get_alias_only(PurpleBuddy *buddy);

/**
 * Sets the server alias for a buddy.
 *
 * @param buddy  The buddy.
 * @param alias  The server alias to be set.
 */
void purple_buddy_set_server_alias(PurpleBuddy *buddy, const char *alias);

/**
 * Gets the server alias for a buddy.
 *
 * @param buddy  The buddy whose server alias will be returned
 * @return  The server alias, or NULL if it is not set.
 */
const char *purple_buddy_get_server_alias(PurpleBuddy *buddy);

/**
 * Returns the correct name to display for a buddy, taking the contact alias
 * into account. In order of precedence: the buddy's alias; the buddy's
 * contact alias; the buddy's server alias; the buddy's user name.
 *
 * @param buddy  The buddy whose alias will be returned
 * @return       The appropriate name or alias, or NULL.
 *
 */
const char *purple_buddy_get_contact_alias(PurpleBuddy *buddy);

/**
 * Returns the correct name to display for a buddy. In order of precedence:
 * the buddy's alias; the buddy's server alias; the buddy's contact alias;
 * the buddy's user name.
 *
 * @param buddy   The buddy whose alias will be returned.
 * @return        The appropriate name or alias, or NULL
 */
const char *purple_buddy_get_alias(PurpleBuddy *buddy);

/**
 * Sets the local alias for the buddy.
 *
 * @param buddy  The buddy
 * @param alias  The local alias for the buddy
 */
void purple_buddy_set_local_alias(PurpleBuddy *buddy, const char *alias);

/**
 * Returns the local alias for the buddy, or @c NULL if none exists.
 *
 * @param buddy  The buddy
 * @return       The local alias for the buddy
 */
const char *purple_buddy_get_local_alias(PurpleBuddy *buddy);

/**
 * Returns the group of which the buddy is a member.
 *
 * @param buddy   The buddy
 * @return        The group or NULL if the buddy is not in a group
 */
PurpleGroup *purple_buddy_get_group(PurpleBuddy *buddy);

/*@}*/

/**************************************************************************/
/** @name Contact API                                                     */
/**************************************************************************/
/*@{*/

/**
 * Returns the GType for the PurpleContact object.
 */
GType purple_contact_get_type(void);

/**
 * Creates a new contact
 *
 * @return       A new contact struct
 */
PurpleContact *purple_contact_new(void);

/**
 * Gets the PurpleGroup from a PurpleContact
 *
 * @param contact  The contact
 * @return         The group
 */
PurpleGroup *purple_contact_get_group(const PurpleContact *contact);

/**
 * Returns the highest priority buddy for a given contact.
 *
 * @param contact  The contact
 * @return The highest priority buddy
 */
PurpleBuddy *purple_contact_get_priority_buddy(PurpleContact *contact);

/**
 * Sets the alias for a contact.
 *
 * @param contact  The contact
 * @param alias    The alias
 */
void purple_contact_set_alias(PurpleContact *contact, const char *alias);

/**
 * Gets the alias for a contact.
 *
 * @param contact  The contact
 * @return  The alias, or NULL if it is not set.
 */
const char *purple_contact_get_alias(PurpleContact *contact);

/**
 * Determines whether an account owns any buddies in a given contact
 *
 * @param contact  The contact to search through.
 * @param account  The account.
 *
 * @return TRUE if there are any buddies from account in the contact, or FALSE otherwise.
 */
gboolean purple_contact_on_account(PurpleContact *contact, PurpleAccount *account);

/**
 * Invalidates the priority buddy so that the next call to
 * purple_contact_get_priority_buddy recomputes it.
 *
 * @param contact  The contact
 */
void purple_contact_invalidate_priority_buddy(PurpleContact *contact);

/**
 * Merges two contacts
 *
 * All of the buddies from source will be moved to target
 *
 * @param source  The contact to merge
 * @param node    The place to merge to (a buddy or contact)
 */
void purple_contact_merge(PurpleContact *source, PurpleBListNode *node);

/*@}*/

/**************************************************************************/
/** @name Chat API                                                        */
/**************************************************************************/
/*@{*/

/**
 * Returns the GType for the PurpleChat object.
 */
GType purple_chat_get_type(void);

/**
 * Creates a new chat for the buddy list
 *
 * @param account    The account this chat will get added to
 * @param alias      The alias of the new chat
 * @param components The info the prpl needs to join the chat.  The
 *                   hash function should be g_str_hash() and the
 *                   equal function should be g_str_equal().
 * @return           A newly allocated chat
 */
PurpleChat *purple_chat_new(PurpleAccount *account, const char *alias, GHashTable *components);

/**
 * Returns the correct name to display for a blist chat.
 *
 * @param chat   The chat whose name will be returned.
 * @return       The alias (if set), or first component value.
 */
const char *purple_chat_get_name(PurpleChat *chat);

/**
 * Sets the alias for a blist chat.
 *
 * @param chat   The chat
 * @param alias  The alias
 */
void purple_chat_set_alias(PurpleChat *chat, const char *alias);

/**
 * Returns the group of which the chat is a member.
 *
 * @param chat The chat.
 *
 * @return The parent group, or @c NULL if the chat is not in a group.
 */
PurpleGroup *purple_chat_get_group(PurpleChat *chat);

/**
 * Returns the account the chat belongs to.
 *
 * @param chat  The chat.
 *
 * @return  The account the chat belongs to.
 */
PurpleAccount *purple_chat_get_account(PurpleChat *chat);

/**
 * Get a hashtable containing information about a chat.
 *
 * @param chat  The chat.
 *
 * @constreturn  The hashtable.
 */
GHashTable *purple_chat_get_components(PurpleChat *chat);

/*@}*/

/**************************************************************************/
/** @name Group API                                                       */
/**************************************************************************/
/*@{*/

/**
 * Returns the GType for the PurpleGroup object.
 */
GType purple_group_get_type(void);

/**
 * Creates a new group
 *
 * You can't have more than one group with the same name.  Sorry.  If you pass
 * this the name of a group that already exists, it will return that group.
 *
 * @param name   The name of the new group
 * @return       A new group struct
*/
PurpleGroup *purple_group_new(const char *name);

/**
 * Returns a list of accounts that have buddies in this group
 *
 * @param g The group
 *
 * @return A GSList of accounts (which must be freed), or NULL if the group
 *         has no accounts.
 */
GSList *purple_group_get_accounts(PurpleGroup *g);

/**
 * Determines whether an account owns any buddies in a given group
 *
 * @param g       The group to search through.
 * @param account The account.
 *
 * @return TRUE if there are any buddies in the group, or FALSE otherwise.
 */
gboolean purple_group_on_account(PurpleGroup *g, PurpleAccount *account);

/**
 * Sets the name of a group.
 *
 * @param group The group.
 * @param name  The name of the group.
 */
void purple_group_set_name(PurpleGroup *group, const char *name);

/**
 * Returns the name of a group.
 *
 * @param group The group.
 *
 * @return The name of the group.
 */
const char *purple_group_get_name(PurpleGroup *group);

/*@}*/

G_END_DECLS

#endif /* _PURPLE_BLISTNODE_TYPES_H_ */
