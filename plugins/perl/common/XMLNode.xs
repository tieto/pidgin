
#include "module.h"

/* TODO


*/

MODULE = Gaim::XMLNode  PACKAGE = Gaim::XMLNode  PREFIX = xmlnode_
PROTOTYPES: ENABLE


xmlnode *
xmlnode_copy(src)
	xmlnode *src

void 
xmlnode_free(node)
	xmlnode *node

xmlnode *
xmlnode_from_str(str, size)
	const char *str
	gssize size

const char *
xmlnode_get_attrib(node, attr)
	xmlnode *node
	const char *attr

xmlnode *
xmlnode_get_child(parent, name)
	const xmlnode *parent
	const char *name

xmlnode *
xmlnode_get_child_with_namespace(parent, name, xmlns)
	const xmlnode *parent
	const char *name
	const char *xmlns

char *
xmlnode_get_data(node)
	xmlnode *node

xmlnode *
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

xmlnode *
xmlnode_new(name)
	const char *name

xmlnode *
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

