#include "module.h"

MODULE = Gaim::XMLNode  PACKAGE = Gaim::XMLNode  PREFIX = xmlnode_
PROTOTYPES: ENABLE

Gaim::XMLNode
xmlnode_copy(class, src)
	Gaim::XMLNode src
    C_ARGS:
	src

void
xmlnode_free(node)
	Gaim::XMLNode node

Gaim::XMLNode
xmlnode_from_str(class, str, size)
	const char *str
	gssize size
    C_ARGS:
	str, size

const char *
xmlnode_get_attrib(node, attr)
	Gaim::XMLNode node
	const char *attr

Gaim::XMLNode
xmlnode_get_child(parent, name)
	Gaim::XMLNode parent
	const char *name

Gaim::XMLNode
xmlnode_get_child_with_namespace(parent, name, xmlns)
	Gaim::XMLNode parent
	const char *name
	const char *xmlns

gchar_own *
xmlnode_get_data(node)
	Gaim::XMLNode node

Gaim::XMLNode
xmlnode_get_next_twin(node)
	Gaim::XMLNode node

void
xmlnode_insert_child(parent, child)
	Gaim::XMLNode parent
	Gaim::XMLNode child

void
xmlnode_insert_data(node, data, size)
	Gaim::XMLNode node
	const char *data
	gssize size

Gaim::XMLNode
xmlnode_new(class, name)
	const char *name
    C_ARGS:
	name

Gaim::XMLNode
xmlnode_new_child(parent, name)
	Gaim::XMLNode parent
	const char *name

void
xmlnode_remove_attrib(node, attr)
	Gaim::XMLNode node
	const char *attr

void
xmlnode_set_attrib(node, attr, value)
	Gaim::XMLNode node
	const char *attr
	const char *value

gchar_own *
xmlnode_to_formatted_str(node, len)
	Gaim::XMLNode node
	int *len

gchar_own *
xmlnode_to_str(node, len)
	Gaim::XMLNode node
	int *len
