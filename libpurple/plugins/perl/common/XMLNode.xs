#include "module.h"

MODULE = Purple::XMLNode  PACKAGE = Purple::XMLNode  PREFIX = xmlnode_
PROTOTYPES: ENABLE

Purple::XMLNode
xmlnode_copy(src)
	Purple::XMLNode src

void
xmlnode_free(node)
	Purple::XMLNode node

Purple::XMLNode
xmlnode_from_str(const char *str, gssize length(str))
    PROTOTYPE: $

const char *
xmlnode_get_name(node)
	Purple::XMLNode node
	CODE:
	RETVAL = node->name;
	OUTPUT:
	RETVAL

const char *
xmlnode_get_attrib(node, attr)
	Purple::XMLNode node
	const char *attr

Purple::XMLNode
xmlnode_get_child(parent, name)
	Purple::XMLNode parent
	const char *name

Purple::XMLNode
xmlnode_get_child_with_namespace(parent, name, xmlns)
	Purple::XMLNode parent
	const char *name
	const char *xmlns

gchar_own *
xmlnode_get_data(node)
	Purple::XMLNode node

Purple::XMLNode
xmlnode_get_next_twin(node)
	Purple::XMLNode node

void
xmlnode_insert_child(parent, child)
	Purple::XMLNode parent
	Purple::XMLNode child

void
xmlnode_insert_data(node, data, size)
	Purple::XMLNode node
	const char *data
	gssize size

Purple::XMLNode
xmlnode_new(class, name)
	const char *name
    C_ARGS:
	name

Purple::XMLNode
xmlnode_new_child(parent, name)
	Purple::XMLNode parent
	const char *name

void
xmlnode_remove_attrib(node, attr)
	Purple::XMLNode node
	const char *attr

void
xmlnode_set_attrib(node, attr, value)
	Purple::XMLNode node
	const char *attr
	const char *value

gchar_own *
xmlnode_to_formatted_str(node)
	Purple::XMLNode node
    CODE:
	RETVAL = xmlnode_to_formatted_str(node, NULL);
    OUTPUT:
	RETVAL

gchar_own *
xmlnode_to_str(node)
	Purple::XMLNode node
    CODE:
	RETVAL = xmlnode_to_str(node, NULL);
    OUTPUT:
	RETVAL
