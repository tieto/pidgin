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

#ifndef PURPLE_COUNTING_NODE_H
#define PURPLE_COUNTING_NODE_H
/**
 * SECTION:blistnode
 * @section_id: libpurple-blistnode
 * @short_description: <filename>blistnode.h</filename>
 * @title: Buddy List Node and Counting Node types
 */

#include <glib.h>
#include <glib-object.h>

#include "blistnode.h"

#define PURPLE_TYPE_COUNTING_NODE             (purple_counting_node_get_type())
#define PURPLE_COUNTING_NODE(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_COUNTING_NODE, PurpleCountingNode))
#define PURPLE_COUNTING_NODE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_COUNTING_NODE, PurpleCountingNodeClass))
#define PURPLE_IS_COUNTING_NODE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_COUNTING_NODE))
#define PURPLE_IS_COUNTING_NODE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_COUNTING_NODE))
#define PURPLE_COUNTING_NODE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_COUNTING_NODE, PurpleCountingNodeClass))

typedef struct _PurpleCountingNode PurpleCountingNode;
typedef struct _PurpleCountingNodeClass PurpleCountingNodeClass;

/**
 * PurpleCountingNode:
 *
 * A node that keeps count of the number of children that it has. It tracks the
 * total number of children, the number of children corresponding to online
 * accounts, and the number of online children.
 *
 * The two types of counting nodes are:
 * <orderedlist>
 *  <listitem>Contact: Keeps track of the number of buddies under it.</listitem>
 *  <listitem>Group: Keeps track of the number of chats and contacts under it.
 *                                                                   </listitem>
 * </orderedlist>
 *
 * See #PurpleContact, #PurpleGroup
 */
struct _PurpleCountingNode {
	PurpleBlistNode node;
};

/**
 * PurpleCountingNodeClass:
 *
 * The base class for all #PurpleCountingNode's.
 */
struct _PurpleCountingNodeClass {
	PurpleBlistNodeClass node_class;

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

/**
 * purple_counting_node_get_type:
 *
 * Returns: The #GType for the #PurpleCountingNode object.
 */
GType purple_counting_node_get_type(void);

/**
 * purple_counting_node_get_total_size:
 * @counter:  The node
 *
 * Returns the total number of children of the counting node.
 *
 * Returns:  The total number of children of the node
 */
int purple_counting_node_get_total_size(PurpleCountingNode *counter);

/**
 * purple_counting_node_get_current_size:
 * @counter:  The node
 *
 * Returns the number of children of the counting node corresponding to online
 * accounts.
 *
 * Returns:  The number of children with online accounts
 */
int purple_counting_node_get_current_size(PurpleCountingNode *counter);

/**
 * purple_counting_node_get_online_count:
 * @counter:  The node
 *
 * Returns the number of children of the counting node that are online.
 *
 * Returns:  The total number of online children
 */
int purple_counting_node_get_online_count(PurpleCountingNode *counter);

/**
 * purple_counting_node_change_total_size:
 * @counter:  The node
 * @delta:    The value to change the total size by
 *
 * Changes the total number of children of the counting node. The provided
 * delta value is added to the count, or if it's negative, the count is
 * decreased.
 */
void purple_counting_node_change_total_size(PurpleCountingNode *counter, int delta);

/**
 * purple_counting_node_change_current_size:
 * @counter:  The node
 * @delta:    The value to change the current size by
 *
 * Changes the number of children of the counting node corresponding to online
 * accounts. The provided delta value is added to the count, or if it's
 * negative, the count is decreased.
 */
void purple_counting_node_change_current_size(PurpleCountingNode *counter, int delta);

/**
 * purple_counting_node_change_online_count:
 * @counter:  The node
 * @delta:    The value to change the online count by
 *
 * Changes the number of children of the counting node that are online. The
 * provided delta value is added to the count, or if it's negative, the count is
 * decreased.
 */
void purple_counting_node_change_online_count(PurpleCountingNode *counter, int delta);

/**
 * purple_counting_node_set_total_size:
 * @counter:    The node
 * @totalsize:  The total number of children of the node
 *
 * Sets the total number of children of the counting node.
 */
void purple_counting_node_set_total_size(PurpleCountingNode *counter, int totalsize);

/**
 * purple_counting_node_set_current_size:
 * @counter:      The node
 * @currentsize:  The number of children with online accounts
 *
 * Sets the number of children of the counting node corresponding to online
 * accounts.
 */
void purple_counting_node_set_current_size(PurpleCountingNode *counter, int currentsize);

/**
 * purple_counting_node_set_online_count:
 * @counter:      The node
 * @onlinecount:  The total number of online children
 *
 * Sets the number of children of the counting node that are online.
 */
void purple_counting_node_set_online_count(PurpleCountingNode *counter, int onlinecount);

G_END_DECLS

#endif /* PURPLE_COUNTING_NODE_H */
