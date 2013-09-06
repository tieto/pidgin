#include "module.h"

MODULE = Purple::XMLNode  PACKAGE = Purple::XMLNode  PREFIX = purple_xmlnode_
PROTOTYPES: ENABLE

Purple::XMLNode
purple_xmlnode_copy(src)
	Purple::XMLNode src

void
purple_xmlnode_free(node)
	Purple::XMLNode node

Purple::XMLNode
purple_xmlnode_from_str(const char *str, gssize length(str))
    PROTOTYPE: $

const char *
purple_xmlnode_get_name(node)
	Purple::XMLNode node
	CODE:
	RETVAL = node->name;
	OUTPUT:
	RETVAL

const char *
purple_xmlnode_get_attrib(node, attr)
	Purple::XMLNode node
	const char *attr

Purple::XMLNode
purple_xmlnode_get_child(parent, name)
	Purple::XMLNode parent
	const char *name
PREINIT:
	PurpleXmlNode *tmp;
CODE:
	if (!name || *name == '\0') {
		tmp = parent->child;
		while (tmp && tmp->type != PURPLE_XMLNODE_TYPE_TAG)
			tmp = tmp->next;
		RETVAL = tmp;
	} else
		RETVAL = purple_xmlnode_get_child(parent, name);
OUTPUT:
	RETVAL

Purple::XMLNode
purple_xmlnode_get_child_with_namespace(parent, name, xmlns)
	Purple::XMLNode parent
	const char *name
	const char *xmlns

gchar_own *
purple_xmlnode_get_data(node)
	Purple::XMLNode node

Purple::XMLNode
purple_xmlnode_get_next(node)
	Purple::XMLNode node
PREINIT:
	PurpleXmlNode *tmp;
CODE:
	tmp = node->next;
	while (tmp && tmp->type != PURPLE_XMLNODE_TYPE_TAG)
		tmp = tmp->next;
	RETVAL = tmp;
OUTPUT:
	RETVAL

Purple::XMLNode
purple_xmlnode_get_next_twin(node)
	Purple::XMLNode node

void
purple_xmlnode_insert_child(parent, child)
	Purple::XMLNode parent
	Purple::XMLNode child

void
purple_xmlnode_insert_data(node, data, size)
	Purple::XMLNode node
	const char *data
	gssize size

Purple::XMLNode
purple_xmlnode_new(class, name)
	const char *name
    C_ARGS:
	name

Purple::XMLNode
purple_xmlnode_new_child(parent, name)
	Purple::XMLNode parent
	const char *name

void
purple_xmlnode_remove_attrib(node, attr)
	Purple::XMLNode node
	const char *attr

void
purple_xmlnode_set_attrib(node, attr, value)
	Purple::XMLNode node
	const char *attr
	const char *value

gchar_own *
purple_xmlnode_to_formatted_str(node)
	Purple::XMLNode node
    CODE:
	RETVAL = purple_xmlnode_to_formatted_str(node, NULL);
    OUTPUT:
	RETVAL

gchar_own *
purple_xmlnode_to_str(node)
	Purple::XMLNode node
    CODE:
	RETVAL = purple_xmlnode_to_str(node, NULL);
    OUTPUT:
	RETVAL
