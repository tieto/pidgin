/**
 * @file blist.h Buddy List API
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
#ifndef _GAIM_BLIST_H_
#define _GAIM_BLIST_H_

/* I can't believe I let ChipX86 inspire me to write good code. -Sean */

#include <glib.h>

typedef struct _GaimBuddyList GaimBuddyList;
typedef struct _GaimBlistUiOps GaimBlistUiOps;
typedef struct _GaimBlistNode GaimBlistNode;

typedef struct _GaimChat GaimChat;
typedef struct _GaimGroup GaimGroup;
typedef struct _GaimContact GaimContact;
typedef struct _GaimBuddy GaimBuddy;

/**************************************************************************/
/* Enumerations                                                           */
/**************************************************************************/
typedef enum
{
	GAIM_BLIST_GROUP_NODE,
	GAIM_BLIST_CONTACT_NODE,
	GAIM_BLIST_BUDDY_NODE,
	GAIM_BLIST_CHAT_NODE,
	GAIM_BLIST_OTHER_NODE

} GaimBlistNodeType;

#define GAIM_BLIST_NODE_IS_CHAT(n)    ((n)->type == GAIM_BLIST_CHAT_NODE)
#define GAIM_BLIST_NODE_IS_BUDDY(n)   ((n)->type == GAIM_BLIST_BUDDY_NODE)
#define GAIM_BLIST_NODE_IS_CONTACT(n) ((n)->type == GAIM_BLIST_CONTACT_NODE)
#define GAIM_BLIST_NODE_IS_GROUP(n)   ((n)->type == GAIM_BLIST_GROUP_NODE)

#define GAIM_BUDDY_IS_ONLINE(b) \
	((b) != NULL && gaim_account_is_connected((b)->account) && \
	 gaim_presence_is_online(gaim_buddy_get_presence(b)))

typedef enum
{
	GAIM_BLIST_NODE_FLAG_NO_SAVE = 1 /**< node should not be saved with the buddy list */

} GaimBlistNodeFlags;

#define GAIM_BLIST_NODE_HAS_FLAG(b, f) ((b)->flags & (f))
#define GAIM_BLIST_NODE_SHOULD_SAVE(b) (! GAIM_BLIST_NODE_HAS_FLAG(b, GAIM_BLIST_NODE_FLAG_NO_SAVE))

#define GAIM_BLIST_NODE_NAME(n) ((n)->type == GAIM_BLIST_CHAT_NODE  ? gaim_chat_get_name((GaimChat*)n) :        \
				     (n)->type == GAIM_BLIST_BUDDY_NODE ? gaim_buddy_get_name((GaimBuddy*)n) : NULL)

#include "account.h"
#include "buddyicon.h"
#include "status.h"

/**************************************************************************/
/* Data Structures                                                        */
/**************************************************************************/

/**
 * A Buddy list node.  This can represent a group, a buddy, or anything else.
 * This is a base class for struct buddy and struct group and for anything
 * else that wants to put itself in the buddy list. */
struct _GaimBlistNode {
	GaimBlistNodeType type;             /**< The type of node this is       */
	GaimBlistNode *prev;                /**< The sibling before this buddy. */
	GaimBlistNode *next;                /**< The sibling after this buddy.  */
	GaimBlistNode *parent;              /**< The parent of this node        */
	GaimBlistNode *child;               /**< The child of this node         */
	GHashTable *settings;               /**< per-node settings              */
	void          *ui_data;             /**< The UI can put data here.      */
	GaimBlistNodeFlags flags;           /**< The buddy flags                */
};

/**
 * A buddy.  This contains everything Gaim will ever need to know about someone on the buddy list.  Everything.
 */
struct _GaimBuddy {
	GaimBlistNode node;                     /**< The node that this buddy inherits from */
	char *name;                             /**< The screenname of the buddy. */
	char *alias;                            /**< The user-set alias of the buddy */
	char *server_alias;                     /**< The server-specified alias of the buddy.  (i.e. MSN "Friendly Names") */
	void *proto_data;                       /**< This allows the prpl to associate whatever data it wants with a buddy */
	GaimBuddyIcon *icon;                    /**< The buddy icon. */
	GaimAccount *account;					/**< the account this buddy belongs to */
	GaimPresence *presence;
};

/**
 * A contact.  This contains everything Gaim will ever need to know about a contact.
 */
struct _GaimContact {
	GaimBlistNode node;		/**< The node that this contact inherits from. */
	char *alias;            /**< The user-set alias of the contact */
	int totalsize;		    /**< The number of buddies in this contact */
	int currentsize;	    /**< The number of buddies in this contact corresponding to online accounts */
	int online;			    /**< The number of buddies in this contact who are currently online */
	GaimBuddy *priority;    /**< The "top" buddy for this contact */
	gboolean priority_valid; /**< Is priority valid? */
};


/**
 * A group.  This contains everything Gaim will ever need to know about a group.
 */
struct _GaimGroup {
	GaimBlistNode node;                    /**< The node that this group inherits from */
	char *name;                            /**< The name of this group. */
	int totalsize;			       /**< The number of chats and contacts in this group */
	int currentsize;		       /**< The number of chats and contacts in this group corresponding to online accounts */
	int online;			       /**< The number of chats and contacts in this group who are currently online */
};

/**
 * A chat.  This contains everything Gaim needs to put a chat room in the
 * buddy list.
 */
struct _GaimChat {
	GaimBlistNode node;      /**< The node that this chat inherits from */
	char *alias;             /**< The display name of this chat. */
	GHashTable *components;  /**< the stuff the protocol needs to know to join the chat */
	GaimAccount *account; /**< The account this chat is attached to */
};


/**
 * The Buddy List
 */
struct _GaimBuddyList {
	GaimBlistNode *root;          /**< The first node in the buddy list */
	GHashTable *buddies;          /**< Every buddy in this list */
	void *ui_data;                /**< UI-specific data. */
};

/**
 * Buddy list UI operations.
 *
 * Any UI representing a buddy list must assign a filled-out GaimBlistUiOps
 * structure to the buddy list core.
 */
struct _GaimBlistUiOps
{
	void (*new_list)(GaimBuddyList *list); /**< Sets UI-specific data on a buddy list. */
	void (*new_node)(GaimBlistNode *node); /**< Sets UI-specific data on a node. */
	void (*show)(GaimBuddyList *list);     /**< The core will call this when it's finished doing its core stuff */
	void (*update)(GaimBuddyList *list,
		       GaimBlistNode *node);       /**< This will update a node in the buddy list. */
	void (*remove)(GaimBuddyList *list,
		       GaimBlistNode *node);       /**< This removes a node from the list */
	void (*destroy)(GaimBuddyList *list);  /**< When the list gets destroyed, this gets called to destroy the UI. */
	void (*set_visible)(GaimBuddyList *list,
			    gboolean show);            /**< Hides or unhides the buddy list */
	void (*request_add_buddy)(GaimAccount *account, const char *username,
							  const char *group, const char *alias);
	void (*request_add_chat)(GaimAccount *account, GaimGroup *group,
							 const char *alias, const char *name);
	void (*request_add_group)(void);
};

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************/
/** @name Buddy List API                                                  */
/**************************************************************************/
/*@{*/

/**
 * Creates a new buddy list
 *
 * @return The new buddy list.
 */
GaimBuddyList *gaim_blist_new(void);

/**
 * Sets the main buddy list.
 *
 * @param blist The buddy list you want to use.
 */
void gaim_set_blist(GaimBuddyList *blist);

/**
 * Returns the main buddy list.
 *
 * @return The main buddy list.
 */
GaimBuddyList *gaim_get_blist(void);

/**
 * Returns the root node of the main buddy list.
 *
 * @return The root node.
 */
GaimBlistNode *gaim_blist_get_root(void);

/**
 * Returns the next node of a given node. This function is to be used to iterate
 * over the tree returned by gaim_get_blist.
 *
 * @param node		A node.
 * @param offline	Whether to include nodes for offline accounts
 * @return	The next node
 */
GaimBlistNode *gaim_blist_node_next(GaimBlistNode *node, gboolean offline);

/**
 * Shows the buddy list, creating a new one if necessary.
 */
void gaim_blist_show(void);


/**
 * Destroys the buddy list window.
 */
void gaim_blist_destroy(void);

/**
 * Hides or unhides the buddy list.
 *
 * @param show   Whether or not to show the buddy list
 */
void gaim_blist_set_visible(gboolean show);

/**
 * Updates a buddy's status.
 *
 * @param buddy      The buddy whose status has changed.
 * @param old_status The status from which we are changing.
 */
void gaim_blist_update_buddy_status(GaimBuddy *buddy, GaimStatus *old_status);

/**
 * Updates a buddy's icon.
 *
 * @param buddy  The buddy whose buddy icon has changed
 */
void gaim_blist_update_buddy_icon(GaimBuddy *buddy);

/**
 * Renames a buddy in the buddy list.
 *
 * @param buddy  The buddy whose name will be changed.
 * @param name   The new name of the buddy.
 */
void gaim_blist_rename_buddy(GaimBuddy *buddy, const char *name);

/**
 * Aliases a contact in the buddy list.
 *
 * @param contact The contact whose alias will be changed.
 * @param alias   The contact's alias.
 */
void gaim_blist_alias_contact(GaimContact *contact, const char *alias);

/**
 * Aliases a buddy in the buddy list.
 *
 * @param buddy  The buddy whose alias will be changed.
 * @param alias  The buddy's alias.
 */
void gaim_blist_alias_buddy(GaimBuddy *buddy, const char *alias);

/**
 * Sets the server-sent alias of a buddy in the buddy list.
 * PRPLs should call serv_got_alias() instead of this.
 *
 * @param buddy  The buddy whose alias will be changed.
 * @param alias  The buddy's "official" alias.
 */
void gaim_blist_server_alias_buddy(GaimBuddy *buddy, const char *alias);

/**
 * Aliases a chat in the buddy list.
 *
 * @param chat  The chat whose alias will be changed.
 * @param alias The chat's new alias.
 */
void gaim_blist_alias_chat(GaimChat *chat, const char *alias);

/**
 * Renames a group
 *
 * @param group  The group to rename
 * @param name   The new name
 */
void gaim_blist_rename_group(GaimGroup *group, const char *name);

/**
 * Creates a new chat for the buddy list
 *
 * @param account    The account this chat will get added to
 * @param alias      The alias of the new chat
 * @param components The info the prpl needs to join the chat
 * @return           A newly allocated chat
 */
GaimChat *gaim_chat_new(GaimAccount *account, const char *alias, GHashTable *components);

/**
 * Adds a new chat to the buddy list.
 *
 * The chat will be inserted right after node or appended to the end
 * of group if node is NULL.  If both are NULL, the buddy will be added to
 * the "Chats" group.
 *
 * @param chat  The new chat who gets added
 * @param group  The group to add the new chat to.
 * @param node   The insertion point
 */
void gaim_blist_add_chat(GaimChat *chat, GaimGroup *group, GaimBlistNode *node);

/**
 * Creates a new buddy
 *
 * @param account    The account this buddy will get added to
 * @param screenname The screenname of the new buddy
 * @param alias      The alias of the new buddy (or NULL if unaliased)
 * @return           A newly allocated buddy
 */
GaimBuddy *gaim_buddy_new(GaimAccount *account, const char *screenname, const char *alias);

/**
 * Sets a buddy's icon.
 *
 * This should only be called from within Gaim. You probably want to
 * call gaim_buddy_icon_set_data().
 *
 * @param buddy The buddy.
 * @param icon  The buddy icon.
 *
 * @see gaim_buddy_icon_set_data()
 */
void gaim_buddy_set_icon(GaimBuddy *buddy, GaimBuddyIcon *icon);

/**
 * Returns a buddy's account.
 *
 * @param buddy The buddy.
 *
 * @return The account
 */
GaimAccount *gaim_buddy_get_account(const GaimBuddy *buddy);

/**
 * Returns a buddy's name
 *
 * @param buddy The buddy.
 *
 * @return The name.
 */
const char *gaim_buddy_get_name(const GaimBuddy *buddy);

/**
 * Returns a buddy's icon.
 *
 * @param buddy The buddy.
 *
 * @return The buddy icon.
 */
GaimBuddyIcon *gaim_buddy_get_icon(const GaimBuddy *buddy);

/**
 * Returns a buddy's contact.
 *
 * @param buddy The buddy.
 *
 * @return The buddy's contact.
 */
GaimContact *gaim_buddy_get_contact(GaimBuddy *buddy);

/**
 * Returns a buddy's presence.
 *
 * @param buddy The buddy.
 *
 * @return The buddy's presence.
 */
GaimPresence *gaim_buddy_get_presence(const GaimBuddy *buddy);

/**
 * Adds a new buddy to the buddy list.
 *
 * The buddy will be inserted right after node or prepended to the
 * group if node is NULL.  If both are NULL, the buddy will be added to
 * the "Buddies" group.
 *
 * @param buddy   The new buddy who gets added
 * @param contact The optional contact to place the buddy in.
 * @param group   The group to add the new buddy to.
 * @param node    The insertion point
 */
void gaim_blist_add_buddy(GaimBuddy *buddy, GaimContact *contact, GaimGroup *group, GaimBlistNode *node);

/**
 * Creates a new group
 *
 * You can't have more than one group with the same name.  Sorry.  If you pass
 * this the * name of a group that already exists, it will return that group.
 *
 * @param name   The name of the new group
 * @return       A new group struct
*/
GaimGroup *gaim_group_new(const char *name);

/**
 * Adds a new group to the buddy list.
 *
 * The new group will be inserted after insert or prepended to the list if
 * node is NULL.
 *
 * @param group  The group
 * @param node   The insertion point
 */
void gaim_blist_add_group(GaimGroup *group, GaimBlistNode *node);

/**
 * Creates a new contact
 *
 * @return       A new contact struct
 */
GaimContact *gaim_contact_new(void);

/**
 * Adds a new contact to the buddy list.
 *
 * The new contact will be inserted after insert or prepended to the list if
 * node is NULL.
 *
 * @param contact The contact
 * @param group   The group to add the contact to
 * @param node    The insertion point
 */
void gaim_blist_add_contact(GaimContact *contact, GaimGroup *group, GaimBlistNode *node);

/**
 * Merges two contacts
 *
 * All of the buddies from source will be moved to target
 *
 * @param source  The contact to merge
 * @param node    The place to merge to (a buddy or contact)
 */
void gaim_blist_merge_contact(GaimContact *source, GaimBlistNode *node);

/**
 * Returns the highest priority buddy for a given contact.
 *
 * @param contact  The contact
 * @return The highest priority buddy
 */
GaimBuddy *gaim_contact_get_priority_buddy(GaimContact *contact);

/**
 * Sets the alias for a contact.
 *
 * @param contact  The contact
 * @param alias    The alias to set, or NULL to unset
 */
void gaim_contact_set_alias(GaimContact *contact, const char *alias);

/**
 * Gets the alias for a contact.
 *
 * @param contact  The contact
 * @return  The alias, or NULL if it is not set.
 */
const char *gaim_contact_get_alias(GaimContact *contact);

/**
 * Determines whether an account owns any buddies in a given contact
 *
 * @param contact  The contact to search through.
 * @param account  The account.
 *
 * @return TRUE if there are any buddies from account in the contact, or FALSE otherwise.
 */
gboolean gaim_contact_on_account(GaimContact *contact, GaimAccount *account);

/**
 * Invalidates the priority buddy so that the next call to
 * gaim_contact_get_priority_buddy recomputes it.
 *
 * @param contact  The contact
 */
void gaim_contact_invalidate_priority_buddy(GaimContact *contact);
/**
 * Removes a buddy from the buddy list and frees the memory allocated to it.
 *
 * @param buddy   The buddy to be removed
 */
void gaim_blist_remove_buddy(GaimBuddy *buddy);

/**
 * Removes a contact, and any buddies it contains, and frees the memory
 * allocated to it.
 *
 * @param contact The contact to be removed
 */
void gaim_blist_remove_contact(GaimContact *contact);

/**
 * Removes a chat from the buddy list and frees the memory allocated to it.
 *
 * @param chat   The chat to be removed
 */
void gaim_blist_remove_chat(GaimChat *chat);

/**
 * Removes a group from the buddy list and frees the memory allocated to it and to
 * its children
 *
 * @param group   The group to be removed
 */
void gaim_blist_remove_group(GaimGroup *group);

/**
 * Returns the alias of a buddy.
 *
 * @param buddy   The buddy whose name will be returned.
 * @return        The alias (if set), server alias (if set),
 *                or NULL.
 */
const char *gaim_buddy_get_alias_only(GaimBuddy *buddy);

/**
 * Gets the server alias for a buddy.
 *
 * @param buddy  The buddy whose name will be returned
 * @return  The server alias, or NULL if it is not set.
 */
const char *gaim_buddy_get_server_alias(GaimBuddy *buddy);

/**
 * Returns the correct name to display for a buddy, taking the contact alias
 * into account. In order of precedence: the buddy's alias; the buddy's
 * contact alias; the buddy's server alias; the buddy's user name.
 *
 * @param buddy  The buddy whose name will be returned
 * @return       The appropriate name or alias, or NULL.
 *
 */
const char *gaim_buddy_get_contact_alias(GaimBuddy *buddy);

/**
 * Returns the correct alias for this user, ignoring server aliases.  Used
 * when a user-recognizable name is required.  In order: buddy's alias; buddy's
 * contact alias; buddy's user name.
 * 
 * @param buddy  The buddy whose alias will be returned.
 * @return       The appropriate name or alias.
 */
const char *gaim_buddy_get_local_alias(GaimBuddy *buddy);

/**
 * Returns the correct name to display for a buddy. In order of precedence:
 * the buddy's alias; the buddy's server alias; the buddy's contact alias;
 * the buddy's user name.
 *
 * @param buddy   The buddy whose name will be returned.
 * @return        The appropriate name or alias, or NULL
 */
const char *gaim_buddy_get_alias(GaimBuddy *buddy);

/**
 * Returns the correct name to display for a blist chat.
 *
 * @param chat   The chat whose name will be returned.
 * @return       The alias (if set), or first component value.
 */
const char *gaim_chat_get_name(GaimChat *chat);

/**
 * Finds the buddy struct given a screenname and an account
 *
 * @param account The account this buddy belongs to
 * @param name    The buddy's screenname
 * @return        The buddy or NULL if the buddy does not exist
 */
GaimBuddy *gaim_find_buddy(GaimAccount *account, const char *name);

/**
 * Finds the buddy struct given a screenname, an account, and a group
 *
 * @param account The account this buddy belongs to
 * @param name    The buddy's screenname
 * @param group   The group to look in
 * @return        The buddy or NULL if the buddy does not exist in the group
 */
GaimBuddy *gaim_find_buddy_in_group(GaimAccount *account, const char *name,
		GaimGroup *group);

/**
 * Finds all GaimBuddy structs given a screenname and an account
 *
 * @param account The account this buddy belongs to
 * @param name    The buddy's screenname
 *
 * @return        A GSList of buddies (which must be freed), or NULL if the buddy doesn't exist
 */
GSList *gaim_find_buddies(GaimAccount *account, const char *name);


/**
 * Finds a group by name
 *
 * @param name    The groups name
 * @return        The group or NULL if the group does not exist
 */
GaimGroup *gaim_find_group(const char *name);

/**
 * Finds a chat by name.
 *
 * @param account The chat's account.
 * @param name    The chat's name.
 *
 * @return The chat, or @c NULL if the chat does not exist.
 */
GaimChat *gaim_blist_find_chat(GaimAccount *account, const char *name);

/**
 * Returns the group of which the chat is a member.
 *
 * @param chat The chat.
 *
 * @return The parent group, or @c NULL if the chat is not in a group.
 */
GaimGroup *gaim_chat_get_group(GaimChat *chat);

/**
 * Returns the group of which the buddy is a member.
 *
 * @param buddy   The buddy
 * @return        The group or NULL if the buddy is not in a group
 */
GaimGroup *gaim_buddy_get_group(GaimBuddy *buddy);


/**
 * Returns a list of accounts that have buddies in this group
 *
 * @param g The group
 *
 * @return A list of gaim_accounts
 */
GSList *gaim_group_get_accounts(GaimGroup *g);

/**
 * Determines whether an account owns any buddies in a given group
 *
 * @param g       The group to search through.
 * @param account The account.
 *
 * @return TRUE if there are any buddies in the group, or FALSE otherwise.
 */
gboolean gaim_group_on_account(GaimGroup *g, GaimAccount *account);

/**
 * Called when an account gets signed on.  Tells the UI to update all the
 * buddies.
 *
 * @param account   The account
 */
void gaim_blist_add_account(GaimAccount *account);


/**
 * Called when an account gets signed off.  Sets the presence of all the buddies to 0
 * and tells the UI to update them.
 *
 * @param account   The account
 */
void gaim_blist_remove_account(GaimAccount *account);


/**
 * Determines the total size of a group
 *
 * @param group  The group
 * @param offline Count buddies in offline accounts
 * @return The number of buddies in the group
 */
int gaim_blist_get_group_size(GaimGroup *group, gboolean offline);

/**
 * Determines the number of online buddies in a group
 *
 * @param group The group
 * @return The number of online buddies in the group, or 0 if the group is NULL
 */
int gaim_blist_get_group_online_count(GaimGroup *group);

/*@}*/

/****************************************************************************************/
/** @name Buddy list file management API                                                */
/****************************************************************************************/

/**
 * Loads the buddy list from ~/.gaim/blist.xml.
 */
void gaim_blist_load(void);

/**
 * Schedule a save of the blist.xml file.  This is used by the privacy
 * API whenever the privacy settings are changed.  If you make a change
 * to blist.xml using one of the functions in the buddy list API, then
 * the buddy list is saved automatically, so you should not need to
 * call this.
 */
void gaim_blist_schedule_save(void);

/**
 * Requests from the user information needed to add a buddy to the
 * buddy list.
 *
 * @param account  The account the buddy is added to.
 * @param username The username of the buddy.
 * @param group    The name of the group to place the buddy in.
 * @param alias    The optional alias for the buddy.
 */
void gaim_blist_request_add_buddy(GaimAccount *account, const char *username,
								  const char *group, const char *alias);

/**
 * Requests from the user information needed to add a chat to the
 * buddy list.
 *
 * @param account The account the buddy is added to.
 * @param group   The optional group to add the chat to.
 * @param alias   The optional alias for the chat.
 * @param name    The required chat name.
 */
void gaim_blist_request_add_chat(GaimAccount *account, GaimGroup *group,
								 const char *alias, const char *name);

/**
 * Requests from the user information needed to add a group to the
 * buddy list.
 */
void gaim_blist_request_add_group(void);

/**
 * Associates a boolean with a node in the buddy list
 *
 * @param node  The node to associate the data with
 * @param key   The identifier for the data
 * @param value The value to set
 */
void gaim_blist_node_set_bool(GaimBlistNode *node, const char *key, gboolean value);

/**
 * Retrieves a named boolean setting from a node in the buddy list
 *
 * @param node  The node to retrieve the data from
 * @param key   The identifier of the data
 *
 * @return The value, or FALSE if there is no setting
 */
gboolean gaim_blist_node_get_bool(GaimBlistNode *node, const char *key);

/**
 * Associates an integer with a node in the buddy list
 *
 * @param node  The node to associate the data with
 * @param key   The identifier for the data
 * @param value The value to set
 */
void gaim_blist_node_set_int(GaimBlistNode *node, const char *key, int value);

/**
 * Retrieves a named integer setting from a node in the buddy list
 *
 * @param node  The node to retrieve the data from
 * @param key   The identifier of the data
 *
 * @return The value, or 0 if there is no setting
 */
int gaim_blist_node_get_int(GaimBlistNode *node, const char *key);

/**
 * Associates a string with a node in the buddy list
 *
 * @param node  The node to associate the data with
 * @param key   The identifier for the data
 * @param value The value to set
 */
void gaim_blist_node_set_string(GaimBlistNode *node, const char *key,
		const char *value);

/**
 * Retrieves a named string setting from a node in the buddy list
 *
 * @param node  The node to retrieve the data from
 * @param key   The identifier of the data
 *
 * @return The value, or NULL if there is no setting
 */
const char *gaim_blist_node_get_string(GaimBlistNode *node, const char *key);

/**
 * Removes a named setting from a blist node
 *
 * @param node  The node from which to remove the setting
 * @param key   The name of the setting
 */
void gaim_blist_node_remove_setting(GaimBlistNode *node, const char *key);

/**
 * Set the flags for the given node.  Setting a node's flags will overwrite
 * the old flags, so if you want to save them, you must first call
 * gaim_blist_node_get_flags and modify that appropriately.
 *
 * @param node  The node on which to set the flags.
 * @param flags The flags to set.  This is a bitmask.
 */
void gaim_blist_node_set_flags(GaimBlistNode *node, GaimBlistNodeFlags flags);

/**
 * Get the current flags on a given node.
 *
 * @param node The node from which to get the flags.
 *
 * @return The flags on the node.  This is a bitmask.
 */
GaimBlistNodeFlags gaim_blist_node_get_flags(GaimBlistNode *node);

/*@}*/

/**
 * Retrieves the extended menu items for a buddy list node.
 * @param n The blist node for which to obtain the extended menu items.
 * @return  A list of GaimMenuAction items, as harvested by the
 *          blist-node-extended-menu signal.
 */
GList *gaim_blist_node_get_extended_menu(GaimBlistNode *n);

/**************************************************************************/
/** @name UI Registration Functions                                       */
/**************************************************************************/
/*@{*/

/**
 * Sets the UI operations structure to be used for the buddy list.
 *
 * @param ops The ops struct.
 */
void gaim_blist_set_ui_ops(GaimBlistUiOps *ops);

/**
 * Returns the UI operations structure to be used for the buddy list.
 *
 * @return The UI operations structure.
 */
GaimBlistUiOps *gaim_blist_get_ui_ops(void);

/*@}*/

/**************************************************************************/
/** @name Buddy List Subsystem                                            */
/**************************************************************************/
/*@{*/

/**
 * Returns the handle for the buddy list subsystem.
 *
 * @return The buddy list subsystem handle.
 */
void *gaim_blist_get_handle(void);

/**
 * Initializes the buddy list subsystem.
 */
void gaim_blist_init(void);

/**
 * Uninitializes the buddy list subsystem.
 */
void gaim_blist_uninit(void);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* _GAIM_BLIST_H_ */
