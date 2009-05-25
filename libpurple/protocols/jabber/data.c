/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */

#include <stdlib.h>
#include <glib.h>
#include <string.h>

#include "internal.h"
#include "data.h"
#include "debug.h"
#include "xmlnode.h"
#include "util.h"
#include "iq.h"

static GHashTable *local_data_by_alt = NULL;
static GHashTable *local_data_by_cid = NULL;
static GHashTable *remote_data_by_cid = NULL;

JabberData *
jabber_data_create_from_data(gconstpointer rawdata, gsize size, const char *type,
	JabberStream *js)
{
	JabberData *data = g_new0(JabberData, 1);
	gchar *checksum = purple_util_get_image_checksum(rawdata, size);
	gchar cid[256];

	g_snprintf(cid, sizeof(cid), "sha1+%s@bob.xmpp.org", checksum);
	g_free(checksum);

	data->cid = g_strdup(cid);
	data->type = g_strdup(type);
	data->size = size;

	data->data = g_memdup(rawdata, size);

	return data;
}

JabberData *
jabber_data_create_from_xml(xmlnode *tag)
{
	JabberData *data = g_new0(JabberData, 1);
	gsize size;
	gpointer raw_data = NULL;

	if (data == NULL) {
		purple_debug_error("jabber", "Could not allocate data object\n");
		g_free(data);
		return NULL;
	}

	/* check if this is a "data" tag */
	if (strcmp(tag->name, "data") != 0) {
		purple_debug_error("jabber", "Invalid data element");
		g_free(data);
		return NULL;
	}

	data->cid = g_strdup(xmlnode_get_attrib(tag, "cid"));
	data->type = g_strdup(xmlnode_get_attrib(tag, "type"));

	raw_data = xmlnode_get_data(tag);
	data->data = purple_base64_decode(raw_data, &size);
	data->size = size;

	g_free(raw_data);

	return data;
}


static void
jabber_data_delete(gpointer cbdata)
{
	JabberData *data = cbdata;
	g_free(data->cid);
	g_free(data->type);
	g_free(data->data);
	g_free(data);
}

const char *
jabber_data_get_cid(const JabberData *data)
{
	return data->cid;
}


const char *
jabber_data_get_type(const JabberData *data)
{
	return data->type;
}

gsize
jabber_data_get_size(const JabberData *data)
{
	return data->size;
}

gpointer
jabber_data_get_data(const JabberData *data)
{
	return data->data;
}

xmlnode *
jabber_data_get_xml_definition(const JabberData *data)
{
	xmlnode *tag = xmlnode_new("data");
	char *base64data = purple_base64_encode(data->data, data->size);

	xmlnode_set_namespace(tag, XEP_0231_NAMESPACE);
	xmlnode_set_attrib(tag, "cid", data->cid);
	xmlnode_set_attrib(tag, "type", data->type);

	xmlnode_insert_data(tag, base64data, -1);

	g_free(base64data);

	return tag;
}

xmlnode *
jabber_data_get_xhtml_im(const JabberData *data, const gchar *alt)
{
	xmlnode *img = xmlnode_new("img");
	char src[128];

	xmlnode_set_attrib(img, "alt", alt);
	g_snprintf(src, sizeof(src), "cid:%s", data->cid);
	xmlnode_set_attrib(img, "src", src);

	return img;
}

xmlnode *
jabber_data_get_xml_request(const gchar *cid)
{
	xmlnode *tag = xmlnode_new("data");

	xmlnode_set_namespace(tag, XEP_0231_NAMESPACE);
	xmlnode_set_attrib(tag, "cid", cid);

	return tag;
}

const JabberData *
jabber_data_find_local_by_alt(const gchar *alt)
{
	purple_debug_info("jabber", "looking up local smiley with alt = %s\n", alt);
	return g_hash_table_lookup(local_data_by_alt, alt);
}

const JabberData *
jabber_data_find_local_by_cid(const gchar *cid)
{
	purple_debug_info("jabber", "lookup local smiley with cid = %s\n", cid);
	return g_hash_table_lookup(local_data_by_cid, cid);
}

const JabberData *
jabber_data_find_remote_by_cid(const gchar *cid)
{
	purple_debug_info("jabber", "lookup remote smiley with cid = %s\n", cid);

	return g_hash_table_lookup(remote_data_by_cid, cid);
}

void
jabber_data_associate_local(JabberData *data, const gchar *alt)
{
	purple_debug_info("jabber", "associating local smiley\n alt = %s, cid = %s\n",
		alt, jabber_data_get_cid(data));
	g_hash_table_insert(local_data_by_alt, g_strdup(alt), data);
	g_hash_table_insert(local_data_by_cid, g_strdup(jabber_data_get_cid(data)),
		data);
}

void
jabber_data_associate_remote(JabberData *data)
{
	purple_debug_info("jabber", "associating remote smiley, cid = %s\n",
		jabber_data_get_cid(data));
	g_hash_table_insert(remote_data_by_cid, g_strdup(jabber_data_get_cid(data)),
		data);
}

void
jabber_data_parse(JabberStream *js, const char *who, JabberIqType type,
                  const char *id, xmlnode *data_node)
{
	JabberIq *result = NULL;
	const char *cid = xmlnode_get_attrib(data_node, "cid");
	const JabberData *data = cid ? jabber_data_find_local_by_cid(cid) : NULL;

	if (!data) {
		xmlnode *item_not_found = xmlnode_new("item-not-found");

		result = jabber_iq_new(js, JABBER_IQ_ERROR);
		if (who)
			xmlnode_set_attrib(result->node, "to", who);
		xmlnode_set_attrib(result->node, "id", id);
		xmlnode_insert_child(result->node, item_not_found);
	} else {
		result = jabber_iq_new(js, JABBER_IQ_RESULT);
		if (who)
			xmlnode_set_attrib(result->node, "to", who);
		xmlnode_set_attrib(result->node, "id", id);
		xmlnode_insert_child(result->node,
							 jabber_data_get_xml_definition(data));
	}
	jabber_iq_send(result);
}

void
jabber_data_init(void)
{
	purple_debug_info("jabber", "creating hash tables for data objects\n");
	local_data_by_alt = g_hash_table_new_full(g_str_hash, g_str_equal,
		g_free, NULL);
	local_data_by_cid = g_hash_table_new_full(g_str_hash, g_str_equal,
		g_free, jabber_data_delete);
	remote_data_by_cid = g_hash_table_new_full(g_str_hash, g_str_equal,
		g_free, jabber_data_delete);

	jabber_iq_register_handler("data", XEP_0231_NAMESPACE, jabber_data_parse);
}

void
jabber_data_uninit(void)
{
	purple_debug_info("jabber", "destroying hash tables for data objects\n");
	g_hash_table_destroy(local_data_by_alt);
	g_hash_table_destroy(local_data_by_cid);
	g_hash_table_destroy(remote_data_by_cid);
	local_data_by_alt = local_data_by_cid = remote_data_by_cid = NULL;
}
