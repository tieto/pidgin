/**
 * @file xmlnode.h XML DOM functions
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
#ifndef _GAIM_XMLNODE_H_
#define _GAIM_XMLNODE_H_

/**
 * The valid types for an xmlnode
 */
typedef enum _XMLNodeType
{
	XMLNODE_TYPE_TAG,		/**< Just a tag */
	XMLNODE_TYPE_ATTRIB,	/**< Has attributes */
	XMLNODE_TYPE_DATA		/**< Has data */
} XMLNodeType;

/**
 * An xmlnode.
 */
typedef struct _xmlnode
{
	char *name;					/**< The name of the node. */
#ifdef HAVE_LIBXML
	char *namespace;                    /**< The namespace of the node */
#endif
	XMLNodeType type;			/**< The type of the node. */
	char *data;					/**< The data for the node. */
	size_t data_sz;				/**< The size of the data. */
	struct _xmlnode *parent;	/**< The parent node or @c NULL.*/
	struct _xmlnode *child;		/**< The child node or @c NULL.*/
	struct _xmlnode *lastchild;	/**< The last child node or @c NULL.*/
	struct _xmlnode *next;		/**< The next node or @c NULL. */
} xmlnode;

/**
 * Creates a new xmlnode.
 *
 * @param name The name of the node.
 *
 * @return The new node.
 */
xmlnode *xmlnode_new(const char *name);

/**
 * Creates a new xmlnode child.
 *
 * @param parent The parent node.
 * @param name   The name of the child node.
 *
 * @return The new child node.
 */
xmlnode *xmlnode_new_child(xmlnode *parent, const char *name);

/**
 * Inserts a node into a node as a child.
 *
 * @param parent The parent node to insert child into.
 * @param child  The child node to insert into parent.
 */
void xmlnode_insert_child(xmlnode *parent, xmlnode *child);

/**
 * Gets a child node named name.
 *
 * @param parent The parent node.
 * @param name   The child's name.
 *
 * @return The child or NULL.
 */
xmlnode *xmlnode_get_child(const xmlnode *parent, const char *name);

/**
 * Gets a child node named name in a namespace.
 *
 * @param parent The parent node.
 * @param name   The child's name.
 * @param xmlns  The namespace.
 *
 * @return The child or NULL.
 */
xmlnode *xmlnode_get_child_with_namespace(const xmlnode *parent, const char *name, const char *xmlns);

/**
 * Gets the next node with the same name as node.
 *
 * @param node The node of a twin to find.
 *
 * @return The twin of node or NULL.
 */
xmlnode *xmlnode_get_next_twin(xmlnode *node);

/**
 * Inserts data into a node.
 *
 * @param node   The node to insert data into.
 * @param data   The data to insert.
 * @param size   The size of the data to insert.  If data is
 *               null-terminated you can pass in -1.
 */
void xmlnode_insert_data(xmlnode *node, const char *data, gssize size);

/**
 * Gets data from a node.
 *
 * @param node The node to get data from.
 *
 * @return The data from the node.
 */
char *xmlnode_get_data(xmlnode *node);

/**
 * Sets an attribute for a node.
 *
 * @param node  The node to set an attribute for.
 * @param attr  The name of the attribute.
 * @param value The value of the attribute.
 */
void xmlnode_set_attrib(xmlnode *node, const char *attr, const char *value);

/**
 * Gets an attribute from a node.
 *
 * @param node The node to get an attribute from.
 * @param attr The attribute to get.
 *
 * @return The value of the attribute.
 */
const char *xmlnode_get_attrib(xmlnode *node, const char *attr);

/**
 * Removes an attribute from a node.
 *
 * @param node The node to remove an attribute from.
 * @param attr The attribute to remove.
 */
void xmlnode_remove_attrib(xmlnode *node, const char *attr);

/**
 * Sets the namespace of a node
 *
 * @param node The node to qualify
 * @param xmlns The namespace of the node
 */
void xmlnode_set_namespace(xmlnode *node, const char *xmlns);

/**
 * Returns the namespace of a node
 *
 * @param node The node to get the namepsace from
 * @return The namespace of this node
 */
const char *xmlnode_get_namespace(xmlnode *node);

/**
 * Returns the node in a string of xml.
 *
 * @param node The starting node to output.
 * @param len  Address for the size of the string.
 *
 * @return The node represented as a string.  You must
 *         g_free this string when finished using it.
 */
char *xmlnode_to_str(xmlnode *node, int *len);

/**
 * Returns the node in a string of human readable xml.
 *
 * @param node The starting node to output.
 * @param len  Address for the size of the string.
 *
 * @return The node as human readable string including
 *         tab and new line characters.  You must
 *         g_free this string when finished using it.
 */
char *xmlnode_to_formatted_str(xmlnode *node, int *len);

/**
 * Creates a node from a string of XML.  Calling this on the
 * root node of an XML document will parse the entire document
 * into a tree of nodes, and return the xmlnode of the root.
 *
 * @param str  The string of xml.
 * @param size The size of the string, or -1 if @a str is
 *             NUL-terminated.
 *
 * @return The new node.
 */
xmlnode *xmlnode_from_str(const char *str, gssize size);

/**
 * Creates a new node from the source node.
 *
 * @param src The node to copy.
 *
 * @return A new copy of the src node.
 */
xmlnode *xmlnode_copy(xmlnode *src);

/**
 * Frees a node and all of it's children.
 *
 * @param node The node to free.
 */
void xmlnode_free(xmlnode *node);

#endif /* _GAIM_XMLNODE_H_ */
