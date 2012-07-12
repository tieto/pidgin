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

#endif /* _GGP_XML_H */
