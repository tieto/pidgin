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
		g_return_val_if_fail(xml != NULL, FALSE);
	}
	
	str = xmlnode_get_data(xml);
	g_return_val_if_fail(str != NULL, FALSE);
	
	*var = str;
	return TRUE;
}

gboolean ggp_xml_get_bool(const xmlnode *xml, gchar *childName, gboolean *var)
{
	char *str;
	gboolean succ;
	
	succ = ggp_xml_get_string(xml, childName, &str);
	g_return_val_if_fail(succ, FALSE);
	
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
	g_return_val_if_fail(succ, FALSE);
	
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
