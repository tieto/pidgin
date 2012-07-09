#ifndef _GGP_XML_H
#define _GGP_XML_H

#include <internal.h>
#include <xmlnode.h>

gboolean ggp_xml_get_string(const xmlnode *xml, gchar *childName, gchar **var);
gboolean ggp_xml_get_bool(const xmlnode *xml, gchar *childName, gboolean *var);
gboolean ggp_xml_get_uint(const xmlnode *xml, gchar *childName, unsigned int *var);

#endif /* _GGP_XML_H */
