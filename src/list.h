/**
 * @file list.h Buddy List API
 *
 * gaim
 *
 * Copyright (C) 2003, Sean Egan <sean.egan@binghamton.edu>
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
 *
 */

/* I can't believe I let ChipX86 inspire me to write good code. -Sean */

#ifndef _LIST_H_
#define _LIST_H_

#include <glib.h>

/**************************************************************************/
/* Enumerations                                                           */
/**************************************************************************/
enum gaim_blist_node_type {
	GAIM_BLIST_GROUP_NODE,
	GAIM_BLIST_BUDDY_NODE,
	GAIM_BLIST_OTHER_NODE,
};

#define GAIM_BLIST_NODE_IS_BUDDY(n) ((n)->type == GAIM_BLIST_BUDDY_NODE)
#define GAIM_BLIST_NODE_IS_GROUP(n) ((n)->type == GAIM_BLIST_GROUP_NODE)

/**************************************************************************/
/* Data Structures                                                        */
/**************************************************************************/

typedef struct _GaimBlistNode GaimBlistNode;
/**
 * A Buddy list node.  This can represent a group, a buddy, or anything else.  This is a base class for struct buddy and
 * struct group and for anything else that wants to put itself in the buddy list. */
struct _GaimBlistNode {
	enum gaim_blist_node_type type;        /**< The type of node this is       */
	GaimBlistNode *prev;                   /**< The sibling before this buddy. */
	GaimBlistNode *next;                   /**< The sibling after this buddy.  */
        GaimBlistNode *parent;                 /**< The parent of this node        */
	GaimBlistNode *child;                  /**< The child of this node         */
	void          *ui_data;                /**< The UI can put data here.      */
};

/**
 * A buddy.  This contains everything Gaim will ever need to know about someone on the buddy list.  Everything.
 */
struct buddy {
	GaimBlistNode node;                     /**< The node that this buddy inherits from */
	char *name;                             /**< The screenname of the buddy. */
	char *alias;                            /**< The user-set alias of the buddy */
	char *server_alias;                     /**< The server-specified alias of the buddy.  (i.e. MSN "Friendly Names") */ 
	int present;                            /**< This is 0 if the buddy appears offline, 1 if he appears online, and 2 if
						    he has recently signed on */
	int evil;                               /**< The warning level */
	time_t signon;                          /**< The time the buddy signed on. */
	int idle;                               /**< The time the buddy has been idle in minutes. */
        int uc;                                 /**< This is a cryptic bitmask that makes sense only to the prpl.  This will get changed */
	void *proto_data;                       /**< This allows the prpl to associate whatever data it wants with a buddy */
	struct gaim_account *account;           /**< the account this buddy belongs to */ 
	GHashTable *settings;                   /**< per-buddy settings from the XML buddy list, set by plugins and the likes. */
};

/**
 * A group.  This contains everything Gaim will ever need to know about a group.
 */
struct group {
	GaimBlistNode node;                    /**< The node that this group inherits from */
	char *name;                            /**< The name of this group. */
	GSList *members;                       /**< The buddies in this group.  This is different from node.child in that it will only
						  contain buddies. */
};


/**
 * The Buddy List
 */
struct gaim_buddy_list {
	GaimBlistNode *root;                    /**< The first node in the buddy list */
	struct gaim_blist_ui_ops *ui_ops;       /**< The UI operations for the buddy list */

	void *ui_data;                          /**< UI-specific data. */
};

/**
 * Buddy list UI operations.
 *
 * Any UI representing a buddy list must assign a filled-out gaim_window_ops
 * structure to the buddy list core.
 */
struct gaim_blist_ui_ops
{
	void (*new_list)(struct gaim_buddy_list *list); /**< Sets UI-specific data on a buddy list. */
	void (*new_node)(GaimBlistNode *node);      /**< Sets UI-specific data on a node. */
	void (*show)(struct gaim_buddy_list *list);     /**< The core will call this when its finished doing it's core stuff */
	void (*update)(struct gaim_buddy_list *list, 
		       GaimBlistNode *node);            /**< This will update a node in the buddy list. */
	void (*remove)(struct gaim_buddy_list *list,
		       GaimBlistNode *node);            /**< This removes a node from the list */
	void (*destroy)(struct gaim_buddy_list *list);  /**< When the list gets destroyed, this gets called to destroy the UI. */
	void (*set_visible)(struct gaim_buddy_list *list, 
			    gboolean show);             /**< Hides or unhides the buddy list */
	
}; 

/**************************************************************************/
/** @name Buddy List API                                                  */
/**************************************************************************/
/*@{*/

/**
 * Creates a new buddy list
 */
struct gaim_buddy_list *gaim_blist_new();

/**
 * Sets the main buddy list.
 *
 * @return The main buddy list.
 */
void gaim_set_blist(struct gaim_buddy_list *blist);

/**
 * Returns the main buddy list.
 *
 * @return The main buddy list.
 */
struct gaim_buddy_list *gaim_get_blist(void);

/**
 * Shows the buddy list, creating a new one if necessary.
 *
 */
void gaim_blist_show();


/**
 * Destroys the buddy list window.
 */
void gaim_blist_destroy();

/**
 * Hides or unhides the buddy list.
 *
 * @param show   Whether or not to show the buddy list
 */
void gaim_blist_set_visible(gboolean show);

/**
 * Updates a buddy's status.
 * 
 * This needs to not take an int.
 *
 * @param buddy   The buddy whose status has changed
 * @param status  The new status in cryptic prpl-understood code
 */
void gaim_blist_update_buddy_status(struct buddy *buddy, int status);


/**
 * Updates a buddy's presence.
 *
 * @param buddy    The buddy whose presence has changed
 * @param presence The new presence
 */
void gaim_blist_update_buddy_presence(struct buddy *buddy, int presence);


/**
 * Updates a buddy's idle time.
 *
 * @param buddy  The buddy whose idle time has changed
 * @param idle   The buddy's idle time in minutes.
 */
void gaim_blist_update_buddy_idle(struct buddy *buddy, int idle);


/**
 * Updates a buddy's warning level.
 *
 * @param buddy  The buddy whose warning level has changed
 * @param evil   The warning level as an int from 0 to 100 (or higher, I guess... but that'd be weird)
 */
void gaim_blist_update_buddy_evil(struct buddy *buddy, int warning);

/**
 * Updates a buddy's warning level.
 *
 * @param buddy  The buddy whose buddy icon has changed
 */
void gaim_blist_update_buddy_icon(struct buddy *buddy);



/**
 * Renames a buddy in the buddy list.
 *
 * @param buddy  The buddy whose name will be changed.
 * @param name   The new name of the buddy.
 */
void gaim_blist_rename_buddy(struct buddy *buddy, const char *name);


/**
 * Aliases a buddy in the buddy list.
 *
 * @param buddy  The buddy whose alias will be changed.
 * @param alias  The buddy's alias.
 */
void gaim_blist_alias_buddy(struct buddy *buddy, const char *alias);


/**
 * Renames a group
 *
 * @param group  The group to rename
 * @param name   The new name
 */
void gaim_blist_rename_group(struct group *group, const char *name);


/**
 * Creates a new buddy
 *
 * @param account    The account this buddy will get added to
 * @param screenname The screenname of the new buddy
 * @param alias      The alias of the new buddy (or NULL if unaliased)
 * @return           A newly allocated buddy
 */
struct buddy *gaim_buddy_new(struct gaim_account *account, const char *screenname, const char *alias);

/**
 * Adds a new buddy to the buddy list.
 *
 * The buddy will be inserted right after node or appended to the end
 * of group if node is NULL.  If both are NULL, the buddy will be added to
 * the "Buddies" group.
 *
 * @param buddy  The new buddy who gets added
 * @param group  The group to add the new buddy to.
 * @param node   The insertion point 
 */
void gaim_blist_add_buddy(struct buddy *buddy, struct group *group, GaimBlistNode *node);

/**
 * Creates a new group
 *
 * You can't have more than one group with the same name.  Sorry.  If you pass this the 
 * name of a group that already exists, it will return that group.
 *
 * @param name   The name of the new group
 * @return       A new group struct 
*/
struct group *gaim_group_new(const char *name);

/**
 * Adds a new group to the buddy list.
 *
 * The new group will be inserted after insert or appended to the end of
 * the list if node is NULL.
 *
 * @param group  The group to add the new buddy to.
 * @param node   The insertion point 
 */
void gaim_blist_add_group(struct group *group, GaimBlistNode *node);

/**
 * Removes a buddy from the buddy list and frees the memory allocated to it.
 *
 * @param buddy   The buddy to be removed
 */
void gaim_blist_remove_buddy(struct buddy *buddy);

/**
 * Removes a group from the buddy list and frees the memory allocated to it and to
 * its children
 *
 * @param group   The group to be removed
 */
void gaim_blist_remove_group(struct group *group);


/**
 * Returns the alias of a buddy.
 *
 * @param buddy   The buddy whose name will be returned.
 * @return        The alias (if set), server alias (if option is set), or NULL.
 */
char *gaim_get_buddy_alias_only(struct buddy *buddy);


/**
 * Returns the correct name to display for a buddy.
 *
 * @param buddy   The buddy whose name will be returned.
 * @return        The alias (if set), server alias (if option is set), screenname, or "Unknown"
 */
char *gaim_get_buddy_alias(struct buddy *buddy);

/**
 * Finds the buddy struct given a screenname and an account
 *
 * @param name    The buddy's screenname
 * @param account The account this buddy belongs to
 * @return        The buddy or NULL if the buddy does not exist
 */
struct buddy *gaim_find_buddy(struct gaim_account *account, const char *name);   

/**
 * Finds a group by name
 *
 * @param name    The groups name
 * @return        The group or NULL if the group does not exist
 */
struct group *gaim_find_group(const char *name);   

/**
 * Returns the group of which the buddy is a member.
 *
 * @param buddy   The buddy
 * @return        The group or NULL if the buddy is not in a group
 */
struct group *gaim_find_buddys_group(struct buddy *buddy);


/**
 * Returns a list of accounts that have buddies in this group
 *
 * @param group   The group
 * @return        A list of gaim_accounts
 */
GSList *gaim_group_get_accounts(struct group *g);

/**
 * Determines whether an account owns any buddies in a given group
 *
 * @param g  The group to search through.
 * @account  The account.
 */
gboolean gaim_group_on_account(struct group *g, struct gaim_account *account);

/**
 * Called when an account gets signed off.  Sets the presence of all the buddies to 0
 * and tells the UI to update them.
 *
 * @param account   The account 
 */
void gaim_blist_remove_account(struct gaim_account *account);


/**
 * Determines the total size of a group
 *
 * @param group  The group
 * @param offline Count buddies in offline accounts
 * @return The number of buddies in the group
 */
int gaim_blist_get_group_size(struct group *group, gboolean offline);

/**
 * Determines the number of online buddies in a group
 *
 * @param group The group
 * @return The number of online buddies in the group, or 0 if the group is NULL
 */
int gaim_blist_get_group_online_count(struct group *group);

/*@}*/

/****************************************************************************************/
/** @name Buddy list file management API                                                */
/****************************************************************************************/

/*@{*/
/**
 * Saves the buddy list to file
 */
void gaim_blist_save();

/**
 * Parses the toc-style buddy list used in older versions of Gaim and for SSI in toc.c
 *
 * @param account  This is the account that the buddies and groups from config will get added to
 * @param config   This is the toc-style buddy list data
 */
void parse_toc_buddy_list(struct gaim_account *account, char *config);


/**
 * Loads the buddy list from ~/.gaim/blist.xml.
 */
void gaim_blist_load();

/**
 * Associates some data with the buddy in the xml buddy list
 *
 * @param b      The buddy the data is associated with
 * @param key    The key used to retrieve the data
 * @param value  The data to set
 */
void gaim_buddy_set_setting(struct buddy *b, const char *key, const char *value);

/**
 * Retrieves data from the XML buddy list set by gaim_buddy_set_setting())
 *
 * @param b      The buddy to retrieve data from
 * @param key    The key to retrieve the data with
 * @return       The associated data or NULL if no data is associated
 */
char *gaim_buddy_get_setting(struct buddy *b, const char *key);

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
void gaim_set_blist_ui_ops(struct gaim_blist_ui_ops *ops);

/**
 * Returns the UI operations structure to be used for the buddy list.
 *
 * @return The UI operations structure.
 */
struct gaim_blist_ui_ops *gaim_get_blist_ui_ops(void);

/*@}*/

#endif /* _LIST_H_ */
