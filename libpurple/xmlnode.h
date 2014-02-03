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
 * SECTION:xmlnode
 * @section_id: libpurple-xmlnode
 * @short_description: <filename>xmlnode.h</filename>
 * @title: XML DOM functions
 */

#ifndef _PURPLE_XMLNODE_H_
#define _PURPLE_XMLNODE_H_

#include <glib.h>
#include <glib-object.h>

#define PURPLE_TYPE_XMLNODE  (purple_xmlnode_get_type())

/**
 * PurpleXmlNodeType:
 * @PURPLE_XMLNODE_TYPE_TAG:    Just a tag
 * @PURPLE_XMLNODE_TYPE_ATTRIB: Has attributes
 * @PURPLE_XMLNODE_TYPE_DATA:   Has data
 *
 * The valid types for an PurpleXmlNode
 */
typedef enum
{
	PURPLE_XMLNODE_TYPE_TAG,
	PURPLE_XMLNODE_TYPE_ATTRIB,
	PURPLE_XMLNODE_TYPE_DATA

} PurpleXmlNodeType;

/**
 * PurpleXmlNode:
 * @name:          The name of the node.
 * @xmlns:         The namespace of the node.
 * @type:          The type of the node.
 * @data:          The data for the node.
 * @data_sz:       The size of the data.
 * @parent:        The parent node or %NULL.
 * @child:         The child node or %NULL.
 * @lastchild:     The last child node or %NULL.
 * @next:          The next node or %NULL.
 * @prefix:        The namespace prefix if any.
 * @namespace_map: The namespace map.
 *
 * An PurpleXmlNode.
 */
typedef struct _PurpleXmlNode PurpleXmlNode;
struct _PurpleXmlNode
{
	char *name;
	char *xmlns;
	PurpleXmlNodeType type;
	char *data;
	size_t data_sz;
	PurpleXmlNode *parent;
	PurpleXmlNode *child;
	PurpleXmlNode *lastchild;
	PurpleXmlNode *next;
	char *prefix;
	GHashTable *namespace_map;
};

G_BEGIN_DECLS

/**
 * purple_xmlnode_get_type:
 *
 * Returns the GType for the PurpleXmlNode boxed structure.
 */
GType purple_xmlnode_get_type(void);

/**
 * purple_xmlnode_new:
 * @name: The name of the node.
 *
 * Creates a new PurpleXmlNode.
 *
 * Returns: The new node.
 */
PurpleXmlNode *purple_xmlnode_new(const char *name);

/**
 * purple_xmlnode_new_child:
 * @parent: The parent node.
 * @name:   The name of the child node.
 *
 * Creates a new PurpleXmlNode child.
 *
 * Returns: The new child node.
 */
PurpleXmlNode *purple_xmlnode_new_child(PurpleXmlNode *parent, const char *name);

/**
 * purple_xmlnode_insert_child:
 * @parent: The parent node to insert child into.
 * @child:  The child node to insert into parent.
 *
 * Inserts a node into a node as a child.
 */
void purple_xmlnode_insert_child(PurpleXmlNode *parent, PurpleXmlNode *child);

/**
 * purple_xmlnode_get_child:
 * @parent: The parent node.
 * @name:   The child's name.
 *
 * Gets a child node named name.
 *
 * Returns: The child or NULL.
 */
PurpleXmlNode *purple_xmlnode_get_child(const PurpleXmlNode *parent, const char *name);

/**
 * purple_xmlnode_get_child_with_namespace:
 * @parent: The parent node.
 * @name:   The child's name.
 * @xmlns:  The namespace.
 *
 * Gets a child node named name in a namespace.
 *
 * Returns: The child or NULL.
 */
PurpleXmlNode *purple_xmlnode_get_child_with_namespace(const PurpleXmlNode *parent, const char *name, const char *xmlns);

/**
 * purple_xmlnode_get_next_twin:
 * @node: The node of a twin to find.
 *
 * Gets the next node with the same name as node.
 *
 * Returns: The twin of node or NULL.
 */
PurpleXmlNode *purple_xmlnode_get_next_twin(PurpleXmlNode *node);

/**
 * purple_xmlnode_insert_data:
 * @node:   The node to insert data into.
 * @data:   The data to insert.
 * @size:   The size of the data to insert.  If data is
 *               null-terminated you can pass in -1.
 *
 * Inserts data into a node.
 */
void purple_xmlnode_insert_data(PurpleXmlNode *node, const char *data, gssize size);

/**
 * purple_xmlnode_get_data:
 * @node: The node to get data from.
 *
 * Gets (escaped) data from a node.
 *
 * Returns: The data from the node or NULL. This data is in raw escaped format.
 *         You must g_free this string when finished using it.
 */
char *purple_xmlnode_get_data(const PurpleXmlNode *node);

/**
 * purple_xmlnode_get_data_unescaped:
 * @node: The node to get data from.
 *
 * Gets unescaped data from a node.
 *
 * Returns: The data from the node, in unescaped form.   You must g_free
 *         this string when finished using it.
 */
char *purple_xmlnode_get_data_unescaped(const PurpleXmlNode *node);

/**
 * purple_xmlnode_set_attrib:
 * @node:  The node to set an attribute for.
 * @attr:  The name of the attribute.
 * @value: The value of the attribute.
 *
 * Sets an attribute for a node.
 */
void purple_xmlnode_set_attrib(PurpleXmlNode *node, const char *attr, const char *value);

/**
 * purple_xmlnode_set_attrib_full:
 * @node:   The node to set an attribute for.
 * @attr:   The name of the attribute to set
 * @xmlns:  The namespace of the attribute to set
 * @prefix: The prefix of the attribute to set
 * @value:  The value of the attribute
 *
 * Sets a namespaced attribute for a node
 */
void purple_xmlnode_set_attrib_full(PurpleXmlNode *node, const char *attr, const char *xmlns,
		const char *prefix, const char *value);

/**
 * purple_xmlnode_get_attrib:
 * @node: The node to get an attribute from.
 * @attr: The attribute to get.
 *
 * Gets an attribute from a node.
 *
 * Returns: The value of the attribute.
 */
const char *purple_xmlnode_get_attrib(const PurpleXmlNode *node, const char *attr);

/**
 * purple_xmlnode_get_attrib_with_namespace:
 * @node:  The node to get an attribute from.
 * @attr:  The attribute to get
 * @xmlns: The namespace of the attribute to get
 *
 * Gets a namespaced attribute from a node
 *
 * Returns: The value of the attribute/
 */
const char *purple_xmlnode_get_attrib_with_namespace(const PurpleXmlNode *node, const char *attr, const char *xmlns);

/**
 * purple_xmlnode_remove_attrib:
 * @node: The node to remove an attribute from.
 * @attr: The attribute to remove.
 *
 * Removes an attribute from a node.
 */
void purple_xmlnode_remove_attrib(PurpleXmlNode *node, const char *attr);

/**
 * purple_xmlnode_remove_attrib_with_namespace:
 * @node:  The node to remove an attribute from
 * @attr:  The attribute to remove
 * @xmlns: The namespace of the attribute to remove
 *
 * Removes a namespaced attribute from a node
 */
void purple_xmlnode_remove_attrib_with_namespace(PurpleXmlNode *node, const char *attr, const char *xmlns);

/**
 * purple_xmlnode_set_namespace:
 * @node: The node to qualify
 * @xmlns: The namespace of the node
 *
 * Sets the namespace of a node
 */
void purple_xmlnode_set_namespace(PurpleXmlNode *node, const char *xmlns);

/**
 * purple_xmlnode_get_namespace:
 * @node: The node to get the namepsace from
 *
 * Returns the namespace of a node
 *
 * Returns: The namespace of this node
 */
const char *purple_xmlnode_get_namespace(const PurpleXmlNode *node);

/**
 * purple_xmlnode_get_default_namespace:
 * @node: The node for which to return the default namespace
 *
 * Returns the current default namespace.  The default
 * namespace is the current namespace which applies to child
 * elements which are unprefixed and which do not contain their
 * own namespace.
 *
 * For example, given:
 * \verbatim
 * <iq type='get' xmlns='jabber:client' xmlns:ns1='http://example.org/ns1'>
 *     <ns1:element><child1/></ns1:element>
 * </iq>
 * \endverbatim
 *
 * The default namespace of all nodes (including 'child1') is "jabber:client",
 * though the namespace for 'element' is "http://example.org/ns1".
 *
 * Returns: The default namespace of this node
 */
const char *purple_xmlnode_get_default_namespace(const PurpleXmlNode *node);

/**
 * purple_xmlnode_get_prefix_namespace:
 * @node: The node from which to start the search.
 * @prefix: The prefix for which to return the associated namespace.
 *
 * Returns the defined namespace for a prefix.
 *
 * Returns: The namespace for this prefix.
 */
const char *purple_xmlnode_get_prefix_namespace(const PurpleXmlNode *node, const char *prefix);

/**
 * purple_xmlnode_set_prefix:
 * @node:   The node to qualify
 * @prefix: The prefix of the node
 *
 * Sets the prefix of a node
 */
void purple_xmlnode_set_prefix(PurpleXmlNode *node, const char *prefix);

/**
 * purple_xmlnode_get_prefix:
 * @node: The node to get the prefix from
 *
 * Returns the prefix of a node
 *
 * Returns: The prefix of this node
 */
const char *purple_xmlnode_get_prefix(const PurpleXmlNode *node);

/**
 * purple_xmlnode_strip_prefixes:
 * @node: The node from which to strip prefixes
 *
 * Remove all element prefixes from an PurpleXmlNode tree.  The prefix's
 * namespace is transformed into the default namespace for an element.
 *
 * Note that this will not necessarily remove all prefixes in use
 * (prefixed attributes may still exist), and that this usage may
 * break some applications (SOAP / XPath apparently often rely on
 * the prefixes having the same name.
 */
void purple_xmlnode_strip_prefixes(PurpleXmlNode *node);

/**
 * purple_xmlnode_get_parent:
 * @child: The child node.
 *
 * Gets the parent node.
 *
 * Returns: The parent or NULL.
 */
PurpleXmlNode *purple_xmlnode_get_parent(const PurpleXmlNode *child);

/**
 * purple_xmlnode_to_str:
 * @node: The starting node to output.
 * @len:  Address for the size of the string.
 *
 * Returns the node in a string of xml.
 *
 * Returns: The node represented as a string.  You must
 *         g_free this string when finished using it.
 */
char *purple_xmlnode_to_str(const PurpleXmlNode *node, int *len);

/**
 * purple_xmlnode_to_formatted_str:
 * @node: The starting node to output.
 * @len:  Address for the size of the string.
 *
 * Returns the node in a string of human readable xml.
 *
 * Returns: The node as human readable string including
 *         tab and new line characters.  You must
 *         g_free this string when finished using it.
 */
char *purple_xmlnode_to_formatted_str(const PurpleXmlNode *node, int *len);

/**
 * purple_xmlnode_from_str:
 * @str:  The string of xml.
 * @size: The size of the string, or -1 if @str is
 *             NUL-terminated.
 *
 * Creates a node from a string of XML.  Calling this on the
 * root node of an XML document will parse the entire document
 * into a tree of nodes, and return the PurpleXmlNode of the root.
 *
 * Returns: The new node.
 */
PurpleXmlNode *purple_xmlnode_from_str(const char *str, gssize size);

/**
 * purple_xmlnode_copy:
 * @src: The node to copy.
 *
 * Creates a new node from the source node.
 *
 * Returns: A new copy of the src node.
 */
PurpleXmlNode *purple_xmlnode_copy(const PurpleXmlNode *src);

/**
 * purple_xmlnode_free:
 * @node: The node to free.
 *
 * Frees a node and all of its children.
 */
void purple_xmlnode_free(PurpleXmlNode *node);

/**
 * purple_xmlnode_from_file:
 * @dir:  The directory where the file is located
 * @filename:  The filename
 * @description:  A description of the file being parsed. Displayed to
 *        the user if the file cannot be read.
 * @process:  The subsystem that is calling purple_xmlnode_from_file. Used as
 *        the category for debugging.
 *
 * Creates a node from a XML File.  Calling this on the
 * root node of an XML document will parse the entire document
 * into a tree of nodes, and return the PurpleXmlNode of the root.
 *
 * Returns: The new node or NULL if an error occurred.
 */
PurpleXmlNode *purple_xmlnode_from_file(const char *dir, const char *filename,
		const char *description, const char *process);

G_END_DECLS

#endif /* _PURPLE_XMLNODE_H_ */

