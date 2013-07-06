/**
 * @file blistnode.h Buddy list node API
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
#ifndef _PURPLE_BLIST_NODE_H_
#define _PURPLE_BLIST_NODE_H_

#include <glib.h>
#include <glib-object.h>

#define PURPLE_TYPE_BLIST_NODE             (purple_blist_node_get_type())
#define PURPLE_BLIST_NODE(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_BLIST_NODE, PurpleBListNode))
#define PURPLE_BLIST_NODE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_BLIST_NODE, PurpleBListNodeClass))
#define PURPLE_IS_BLIST_NODE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_BLIST_NODE))
#define PURPLE_IS_BLIST_NODE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_BLIST_NODE))
#define PURPLE_BLIST_NODE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_BLIST_NODE, PurpleBListNodeClass))

/** @copydoc _PurpleBListNode */
typedef struct _PurpleBListNode PurpleBListNode;
/** @copydoc _PurpleBListNodeClass */
typedef struct _PurpleBListNodeClass PurpleBListNodeClass;

/**************************************************************************/
/* Data Structures                                                        */
/**************************************************************************/

/**
 * A Buddy list node.  This can represent a group, a buddy, or anything else.
 * This is a base class for PurpleBuddy, PurpleContact, PurpleGroup, and for
 * anything else that wants to put itself in the buddy list. */
struct _PurpleBListNode {
	/*< private >*/
	GObject gparent;

	/** The UI data associated with this node. This is a convenience
	 *  field provided to the UIs -- it is not used by the libpurple core.
	 */
	gpointer ui_data;

	PurpleBListNode *prev;    /**< The sibling before this buddy. */
	PurpleBListNode *next;    /**< The sibling after this buddy.  */
	PurpleBListNode *parent;  /**< The parent of this node        */
	PurpleBListNode *child;   /**< The child of this node         */
};

/** The base class for all #PurpleBListNode's. */
struct _PurpleBListNodeClass {
	/*< private >*/
	GObjectClass gparent_class;

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/* TODO add PurpleCountingNode */

G_BEGIN_DECLS

/**************************************************************************/
/** @name Buddy list node API                                             */
/**************************************************************************/
/*@{*/

/**
 * Returns the GType for the PurpleBListNode object.
 */
GType purple_blist_node_get_type(void);

/**
 * Returns the next node of a given node. This function is to be used to iterate
 * over the tree returned by purple_get_blist.
 *
 * @param node		A node.
 * @param offline	Whether to include nodes for offline accounts
 * @return	The next node
 * @see purple_blist_node_get_parent
 * @see purple_blist_node_get_first_child
 * @see purple_blist_node_get_sibling_next
 * @see purple_blist_node_get_sibling_prev
 */
PurpleBListNode *purple_blist_node_next(PurpleBListNode *node, gboolean offline);

/**
 * Returns the parent node of a given node.
 *
 * @param node A node.
 * @return  The parent node.
 *
 * @see purple_blist_node_get_first_child
 * @see purple_blist_node_get_sibling_next
 * @see purple_blist_node_get_sibling_prev
 * @see purple_blist_node_next
 */
PurpleBListNode *purple_blist_node_get_parent(PurpleBListNode *node);

/**
 * Returns the the first child node of a given node.
 *
 * @param node A node.
 * @return  The child node.
 *
 * @see purple_blist_node_get_parent
 * @see purple_blist_node_get_sibling_next
 * @see purple_blist_node_get_sibling_prev
 * @see purple_blist_node_next
 */
PurpleBListNode *purple_blist_node_get_first_child(PurpleBListNode *node);

/**
 * Returns the sibling node of a given node.
 *
 * @param node A node.
 * @return  The sibling node.
 *
 * @see purple_blist_node_get_parent
 * @see purple_blist_node_get_first_child
 * @see purple_blist_node_get_sibling_prev
 * @see purple_blist_node_next
 */
PurpleBListNode *purple_blist_node_get_sibling_next(PurpleBListNode *node);

/**
 * Returns the previous sibling node of a given node.
 *
 * @param node A node.
 * @return  The sibling node.
 *
 * @see purple_blist_node_get_parent
 * @see purple_blist_node_get_first_child
 * @see purple_blist_node_get_sibling_next
 * @see purple_blist_node_next
 */
PurpleBListNode *purple_blist_node_get_sibling_prev(PurpleBListNode *node);

/**
 * Returns the UI data of a given node.
 *
 * @param node The node.
 * @return The UI data.
 */
gpointer purple_blist_node_get_ui_data(const PurpleBListNode *node);

/**
 * Sets the UI data of a given node.
 *
 * @param node The node.
 * @param ui_data The UI data.
 */
void purple_blist_node_set_ui_data(PurpleBListNode *node, gpointer ui_data);

/**
 * Checks whether a named setting exists for a node in the buddy list
 *
 * @param node  The node to check from which to check settings
 * @param key   The identifier of the data
 *
 * @return TRUE if a value exists, or FALSE if there is no setting
 */
gboolean purple_blist_node_has_setting(PurpleBListNode *node, const char *key);

/**
 * Associates a boolean with a node in the buddy list
 *
 * @param node  The node to associate the data with
 * @param key   The identifier for the data
 * @param value The value to set
 */
void purple_blist_node_set_bool(PurpleBListNode *node, const char *key, gboolean value);

/**
 * Retrieves a named boolean setting from a node in the buddy list
 *
 * @param node  The node to retrieve the data from
 * @param key   The identifier of the data
 *
 * @return The value, or FALSE if there is no setting
 */
gboolean purple_blist_node_get_bool(PurpleBListNode *node, const char *key);

/**
 * Associates an integer with a node in the buddy list
 *
 * @param node  The node to associate the data with
 * @param key   The identifier for the data
 * @param value The value to set
 */
void purple_blist_node_set_int(PurpleBListNode *node, const char *key, int value);

/**
 * Retrieves a named integer setting from a node in the buddy list
 *
 * @param node  The node to retrieve the data from
 * @param key   The identifier of the data
 *
 * @return The value, or 0 if there is no setting
 */
int purple_blist_node_get_int(PurpleBListNode *node, const char *key);

/**
 * Associates a string with a node in the buddy list
 *
 * @param node  The node to associate the data with
 * @param key   The identifier for the data
 * @param value The value to set
 */
void purple_blist_node_set_string(PurpleBListNode *node, const char *key,
		const char *value);

/**
 * Retrieves a named string setting from a node in the buddy list
 *
 * @param node  The node to retrieve the data from
 * @param key   The identifier of the data
 *
 * @return The value, or NULL if there is no setting
 */
const char *purple_blist_node_get_string(PurpleBListNode *node, const char *key);

/**
 * Removes a named setting from a blist node
 *
 * @param node  The node from which to remove the setting
 * @param key   The name of the setting
 */
void purple_blist_node_remove_setting(PurpleBListNode *node, const char *key);

/**
 * Sets whether the node should be saved with the buddy list or not
 *
 * @param node  The node
 * @param dont_save TRUE if the node should NOT be saved, FALSE if node should
 *                  be saved
 */
void purple_blist_node_set_dont_save(PurpleBListNode *node, gboolean dont_save);

/**
 * Gets whether the node should be saved with the buddy list or not
 *
 * @param node  The node
 *
 * @return TRUE if the node should NOT be saved, FALSE if node should be saved
 */
gboolean purple_blist_node_get_dont_save(PurpleBListNode *node);

/*@}*/

/**
 * Retrieves the extended menu items for a buddy list node.
 * @param n The blist node for which to obtain the extended menu items.
 * @return  A list of PurpleMenuAction items, as harvested by the
 *          blist-node-extended-menu signal.
 */
GList *purple_blist_node_get_extended_menu(PurpleBListNode *n);

/*@}*/

G_END_DECLS

#endif /* _PURPLE_BLIST_NODE_H_ */
