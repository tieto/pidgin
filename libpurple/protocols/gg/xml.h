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

#ifndef _GGP_XML_H
#define _GGP_XML_H

#include <internal.h>
#include <xmlnode.h>

gboolean ggp_xml_get_string(const xmlnode *xml, gchar *childName, gchar **var);
gboolean ggp_xml_get_bool(const xmlnode *xml, gchar *childName, gboolean *var);
gboolean ggp_xml_get_uint(const xmlnode *xml, gchar *childName, unsigned int *var);

gboolean ggp_xml_set_string(xmlnode *xml, gchar *childName, const gchar *val);
gboolean ggp_xml_set_bool(xmlnode *xml, gchar *childName, gboolean val);
gboolean ggp_xml_set_uint(xmlnode *xml, gchar *childName, unsigned int val);

void ggp_xmlnode_remove_children(xmlnode *xml);

unsigned int ggp_xml_child_count(xmlnode *xml, const gchar *childName);

#endif /* _GGP_XML_H */
