/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * Rewritten from scratch during Google Summer of Code 2012
 * by Tomek Wasilczyk (http://www.wasilczyk.pl).
 *
 * Previously implemented by:
 *  - Arkadiusz Miskiewicz <misiek@pld.org.pl> - first implementation (2001);
 *  - Bartosz Oler <bartosz@bzimage.us> - reimplemented during GSoC 2005;
 *  - Krzysztof Klinikowski <grommasher@gmail.com> - some parts (2009-2011).
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

#include "xml.h"

#include "debug.h"

gboolean ggp_xml_get_string(const xmlnode *xml, gchar *childName, gchar **var)
{
	char *str;
	
	g_return_val_if_fail(xml != NULL, FALSE);
	g_return_val_if_fail(var != NULL, FALSE);
	
	if (childName != NULL)
	{
		xml = xmlnode_get_child(xml, childName);
		if (xml == NULL)
			return FALSE;
	}
	
	str = xmlnode_get_data(xml);
	if (str == NULL)
		return FALSE;
	
	*var = str;
	return TRUE;
}

gboolean ggp_xml_get_bool(const xmlnode *xml, gchar *childName, gboolean *var)
{
	char *str;
	gboolean succ;
	
	succ = ggp_xml_get_string(xml, childName, &str);
	if (!succ)
		return FALSE;
	
	*var = (strcmp(str, "true") == 0 ||
		strcmp(str, "True") == 0 ||
		strcmp(str, "TRUE") == 0 ||
		strcmp(str, "1") == 0);
	g_free(str);
	
	return TRUE;
}

gboolean ggp_xml_get_uint(const xmlnode *xml, gchar *childName, unsigned int *var)
{
	char *str, *endptr;
	gboolean succ;
	unsigned int val;
	
	succ = ggp_xml_get_string(xml, childName, &str);
	if (!succ)
		return FALSE;
	
	errno = 0;
	val = strtoul(str, &endptr, 10);
	
	succ = (errno != ERANGE && endptr[0] == '\0');
	g_free(str);
	
	if (succ)
		*var = val;
	return succ;
}

gboolean ggp_xml_set_string(xmlnode *xml, gchar *childName, const gchar *val)
{
	g_return_val_if_fail(xml != NULL, FALSE);
	g_return_val_if_fail(val != NULL, FALSE);
	
	if (childName != NULL)
	{
		xmlnode *child = xmlnode_get_child(xml, childName);
		if (child == NULL)
			child = xmlnode_new_child(xml, childName);
		xml = child;
	}
	
	ggp_xmlnode_remove_children(xml);
	xmlnode_insert_data(xml, val, -1);
	
	return TRUE;
}

gboolean ggp_xml_set_bool(xmlnode *xml, gchar *childName, gboolean val)
{
	return ggp_xml_set_string(xml, childName, val ? "true" : "false");
}

gboolean ggp_xml_set_uint(xmlnode *xml, gchar *childName, unsigned int val)
{
	gchar buff[20];
	g_snprintf(buff, sizeof(buff), "%u", val);
	return ggp_xml_set_string(xml, childName, buff);
}

void ggp_xmlnode_remove_children(xmlnode *xml)
{
	xmlnode *child = xml->child;
	while (child)
	{
		xmlnode *next = child->next;
		if (child->type != XMLNODE_TYPE_ATTRIB)
			xmlnode_free(child);
		child = next;
	}
}

unsigned int ggp_xml_child_count(xmlnode *xml, const gchar *childName)
{
	xmlnode *child;
	unsigned int count = 0;
	
	g_return_val_if_fail(xml != NULL, 0);
	
	if (childName)
	{
		child = xmlnode_get_child(xml, childName);
		while (child)
		{
			child = xmlnode_get_next_twin(child);
			count++;
		}
	}
	else
	{
		child = xml->child;
		while (child)
		{
			child = child->next;
			count++;
		}
	}
	
	return count;
}
