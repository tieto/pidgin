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

#include "data.h"
#include "debug.h"
#include "xmlnode.h"
#include "util.h"
#include "conversation.h"
#include "iq.h"

/* hash table to store locally supplied data objects, by conversation and
 	alt text (smiley shortcut) */
static GHashTable *local_datas_by_alt = NULL;

/* hash table to store locally supplied data objects, by content id */
static GHashTable *local_datas_by_cid = NULL;

/* remote supplied data objects by content id */
static GHashTable *remote_datas_by_cid = NULL;


void
jabber_data_init(void)
{
	/* setup hash tables for storing data instances here */
	purple_debug_info("jabber", "Setting up data handling\n");

	local_datas_by_alt = g_hash_table_new(NULL, NULL);
	local_datas_by_cid = g_hash_table_new(NULL, NULL);
	remote_datas_by_cid = g_hash_table_new(NULL, NULL);
}

JabberData *
jabber_data_create_from_data(gconstpointer rawdata, gsize size, const char *type,
							 const char *alt)
{
	JabberData *data = g_new0(JabberData, 1);
	gchar *checksum = purple_util_get_image_checksum(rawdata, size);
	gchar cid[256];

	/* is there some better way to generate a content ID? */
	g_snprintf(cid, sizeof(cid), "%s@%s", checksum, g_get_host_name());
	g_free(checksum);

	data->cid = g_strdup(cid);
	data->type = g_strdup(type);
	data->alt = g_strdup(alt);
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
	data->alt = g_strdup(xmlnode_get_attrib(tag, "alt"));

	raw_data = xmlnode_get_data(tag);
	data->data = purple_base64_decode(raw_data, &size);
	data->size = size;

	g_free(raw_data);

	return data;
}


void
jabber_data_delete(JabberData *data)
{
	g_free(data->cid);
	g_free(data->alt);
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
jabber_data_get_alt(const JabberData *data)
{
	return data->alt;
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
	xmlnode_set_attrib(tag, "alt", data->alt);
	xmlnode_set_attrib(tag, "cid", data->cid);
	xmlnode_set_attrib(tag, "type", data->type);

	xmlnode_insert_data(tag, base64data, -1);

	g_free(base64data);

	return tag;
}

xmlnode *
jabber_data_get_xhtml_im(const JabberData *data)
{
	xmlnode *img = xmlnode_new("img");
	char src[128];

	xmlnode_set_attrib(img, "alt", data->alt);
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
jabber_data_find_local_by_alt(const PurpleConversation *conv, const char *alt)
{
	GHashTable *local_datas = g_hash_table_lookup(local_datas_by_alt, conv);

	if (local_datas) {
		return g_hash_table_lookup(local_datas, alt);
	} else {
		return NULL;
	}
}


const JabberData *
jabber_data_find_local_by_cid(const PurpleConversation *conv, const char *cid)
{
	GHashTable *local_datas = g_hash_table_lookup(local_datas_by_cid, conv);

	if (local_datas) {
		return g_hash_table_lookup(local_datas, cid);
	} else {
		return NULL;
	}
}

const JabberData *
jabber_data_find_remote_by_cid(const PurpleConversation *conv, const char *cid)
{
	GHashTable *remote_datas = g_hash_table_lookup(remote_datas_by_cid, conv);

	if (remote_datas) {
		return g_hash_table_lookup(remote_datas, cid);
	} else {
		return NULL;
	}
}

void
jabber_data_associate_local_with_conv(JabberData *data, PurpleConversation *conv)
{
	GHashTable *datas_by_alt = g_hash_table_lookup(local_datas_by_alt, conv);
	GHashTable *datas_by_cid = g_hash_table_lookup(local_datas_by_cid, conv);

	if (!datas_by_alt) {
		datas_by_alt = g_hash_table_new(g_str_hash, g_str_equal);
		g_hash_table_insert(local_datas_by_alt, conv, datas_by_alt);
	}

	if (!datas_by_cid) {
		datas_by_cid = g_hash_table_new(g_str_hash, g_str_equal);
		g_hash_table_insert(local_datas_by_cid, conv, datas_by_cid);
	}

	g_hash_table_insert(datas_by_alt, g_strdup(jabber_data_get_alt(data)), data);
	g_hash_table_insert(datas_by_cid, g_strdup(jabber_data_get_cid(data)), data);
}

void
jabber_data_associate_remote_with_conv(JabberData *data, PurpleConversation *conv)
{
	GHashTable *datas_by_cid = g_hash_table_lookup(remote_datas_by_cid, conv);

	if (!datas_by_cid) {
		datas_by_cid = g_hash_table_new(g_str_hash, g_str_equal);
		g_hash_table_insert(remote_datas_by_cid, conv, datas_by_cid);
	}

	g_hash_table_insert(datas_by_cid, g_strdup(jabber_data_get_cid(data)), data);
}

static void
jabber_data_delete_from_hash_table(gpointer key, gpointer value,
								   gpointer user_data)
{
	JabberData *data = (JabberData *) value;
	jabber_data_delete(data);
	g_free(key);
}

void
jabber_data_delete_associated_with_conv(PurpleConversation *conv)
{
	GHashTable *local_datas = g_hash_table_lookup(local_datas_by_cid, conv);
	GHashTable *remote_datas = g_hash_table_lookup(remote_datas_by_cid, conv);
	GHashTable *local_datas_alt = g_hash_table_lookup(local_datas_by_alt, conv);

	if (local_datas) {
		g_hash_table_foreach(local_datas, jabber_data_delete_from_hash_table,
							 NULL);
		g_hash_table_destroy(local_datas);
		g_hash_table_remove(local_datas_by_cid, conv);
	}
	if (remote_datas) {
		g_hash_table_foreach(remote_datas, jabber_data_delete_from_hash_table,
							 NULL);
		g_hash_table_destroy(remote_datas);
		g_hash_table_remove(remote_datas_by_cid, conv);
	}
	if (local_datas_alt) {
		g_hash_table_destroy(local_datas_alt);
		g_hash_table_remove(local_datas_by_alt, conv);
	}
}

void
jabber_data_parse(JabberStream *js, xmlnode *packet)
{
	JabberIq *result = NULL;
	const char *who = xmlnode_get_attrib(packet, "from");
	const PurpleConnection *gc = js->gc;
	const PurpleAccount *account = purple_connection_get_account(gc);
	const PurpleConversation *conv =
		purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, who, account);
	xmlnode *data_node = xmlnode_get_child(packet, "data");
	const JabberData *data =
		jabber_data_find_local_by_cid(conv, xmlnode_get_attrib(data_node, "cid"));

	if (!conv || !data) {
		xmlnode *item_not_found = xmlnode_new("item-not-found");

		result = jabber_iq_new(js, JABBER_IQ_ERROR);
		xmlnode_set_attrib(result->node, "to", who);
		xmlnode_set_attrib(result->node, "id", xmlnode_get_attrib(packet, "id"));
		xmlnode_insert_child(result->node, item_not_found);
	} else {
		result = jabber_iq_new(js, JABBER_IQ_RESULT);
		xmlnode_set_attrib(result->node, "to", who);
		xmlnode_set_attrib(result->node, "id", xmlnode_get_attrib(packet, "id"));
		xmlnode_insert_child(result->node,
							 jabber_data_get_xml_definition(data));
	}
	jabber_iq_send(result);
}
