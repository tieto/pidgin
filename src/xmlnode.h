/**
 * @file xmlnode.h XML DOM functions
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

typedef enum _XMLNodeType
{
	XMLNODE_TYPE_TAG,
	XMLNODE_TYPE_ATTRIB,
	XMLNODE_TYPE_DATA
} XMLNodeType;

typedef struct _xmlnode
{
	char *name;
	XMLNodeType type;
	char *data;
	size_t data_sz;
	struct _xmlnode *parent;
	struct _xmlnode *child;
	struct _xmlnode *next;
} xmlnode;

xmlnode *xmlnode_new(const char *name);
xmlnode *xmlnode_new_child(xmlnode *parent, const char *name);
void xmlnode_insert_child(xmlnode *parent, xmlnode *child);
xmlnode *xmlnode_get_child(xmlnode *parent, const char *name);
xmlnode *xmlnode_get_child_with_namespace(xmlnode *parent, const char *name, const char *xmlns);
xmlnode *xmlnode_get_next_twin(xmlnode *node);
void xmlnode_insert_data(xmlnode *parent, const char *data, size_t size);
char *xmlnode_get_data(xmlnode *node);
void xmlnode_set_attrib(xmlnode *node, const char *attr, const char *value);
const char *xmlnode_get_attrib(xmlnode *node, const char *attr);
void xmlnode_remove_attrib(xmlnode *node, const char *attr);
char *xmlnode_to_str(xmlnode *node, int *len);
xmlnode *xmlnode_from_str(const char *str, size_t size);
xmlnode *xmlnode_copy(xmlnode *src);

void xmlnode_free(xmlnode *node);

#endif /* _GAIM_XMLNODE_H_ */
