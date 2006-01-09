#include "module.h"

MODULE = Gaim::XMLNode  PACKAGE = Gaim::XMLNode  PREFIX = xmlnode_
PROTOTYPES: ENABLE

Gaim::XMLNode
xmlnode_copy(class, src)
	xmlnode *src
    C_ARGS:
	src

void
xmlnode_free(node)
	xmlnode *node

Gaim::XMLNode
xmlnode_from_str(class, str, size)
	const char *str
	gssize size
    C_ARGS:
	str, size

const char *
xmlnode_get_attrib(node, attr)
	xmlnode *node
	const char *attr

Gaim::XMLNode
xmlnode_get_child(parent, name)
	const xmlnode *parent
	const char *name

Gaim::XMLNode
xmlnode_get_child_with_namespace(parent, name, xmlns)
	const xmlnode *parent
	const char *name
	const char *xmlns

char *
xmlnode_get_data(node)
	xmlnode *node

Gaim::XMLNode
xmlnode_get_next_twin(node)
	xmlnode *node

void
xmlnode_insert_child(parent, child)
	xmlnode *parent
	xmlnode *child

void
xmlnode_insert_data(node, data, size)
	xmlnode *node
	const char *data
	gssize size

Gaim::XMLNode
xmlnode_new(class, name)
	const char *name
    C_ARGS:
	name

Gaim::XMLNode
xmlnode_new_child(parent, name)
	xmlnode *parent
	const char *name

void
xmlnode_remove_attrib(node, attr)
	xmlnode *node
	const char *attr

void
xmlnode_set_attrib(node, attr, value)
	xmlnode *node
	const char *attr
	const char *value

char *
xmlnode_to_formatted_str(node, len)
	xmlnode *node
	int *len

char *
xmlnode_to_str(node, len)
	xmlnode *node
	int *len
