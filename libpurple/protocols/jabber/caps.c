/*
 * purple - Jabber Protocol Plugin
 *
 * Copyright (C) 2007, Andreas Monitzer <andy@monitzer.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA	 02111-1307	 USA
 *
 */

#include "internal.h"

#include "debug.h"
#include "caps.h"
#include "cipher.h"
#include "iq.h"
#include "presence.h"
#include "util.h"

#define JABBER_CAPS_FILENAME "xmpp-caps.xml"

typedef struct _JabberDataFormField {
	gchar *var;
	GList *values;
} JabberDataFormField;

typedef struct _JabberCapsKey {
	char *node;
	char *ver;
	char *hash;
} JabberCapsKey;

static GHashTable *capstable = NULL; /* JabberCapsKey -> JabberCapsClientInfo */

/**
 *	Processes a query-node and returns a JabberCapsClientInfo object with all relevant info.
 *	
 *	@param 	query 	A query object.
 *	@return 		A JabberCapsClientInfo object.
 */
static JabberCapsClientInfo *jabber_caps_parse_client_info(xmlnode *query);

#if 0
typedef struct _JabberCapsValue {
	GList *identities; /* JabberCapsIdentity */
	GList *features; /* char * */
	GHashTable *ext; /* char * -> JabberCapsValueExt */
} JabberCapsValue;
#endif

static guint jabber_caps_hash(gconstpointer key) {
	const JabberCapsKey *name = key;
	guint nodehash = g_str_hash(name->node);
	guint verhash = g_str_hash(name->ver);
	guint hashhash = g_str_hash(name->hash);
	return nodehash ^ verhash ^ hashhash;
}

static gboolean jabber_caps_compare(gconstpointer v1, gconstpointer v2) {
	const JabberCapsKey *name1 = v1;
	const JabberCapsKey *name2 = v2;
	
	return strcmp(name1->node, name2->node) == 0 &&
	       strcmp(name1->ver, name2->ver) == 0 &&
	       strcmp(name1->hash, name2->hash) == 0;
}

void jabber_caps_destroy_key(gpointer key) {
	JabberCapsKey *keystruct = key;
	g_free(keystruct->node);
	g_free(keystruct->ver);
	g_free(keystruct->hash);
	g_free(keystruct);
}

static void
jabber_caps_client_info_destroy(gpointer data) {
	JabberCapsClientInfo *info = data;
	while(info->identities) {
		JabberIdentity *id = info->identities->data;
		g_free(id->category);
		g_free(id->type);
		g_free(id->name);
		g_free(id->lang);
		g_free(id);
		info->identities = g_list_delete_link(info->identities, info->identities);
	}

	while(info->features) {
		g_free(info->features->data);
		info->features = g_list_delete_link(info->features, info->features);
	}

	while(info->forms) {
		g_free(info->forms->data);
		info->forms = g_list_delete_link(info->forms, info->forms);
	}

#if 0
	g_hash_table_destroy(valuestruct->ext);
#endif

	g_free(info);
}

#if 0
static void jabber_caps_ext_destroy_value(gpointer value) {
	JabberCapsValueExt *valuestruct = value;
	while(valuestruct->identities) {
		JabberCapsIdentity *id = valuestruct->identities->data;
		g_free(id->category);
		g_free(id->type);
		g_free(id->name);
		g_free(id);
		
		valuestruct->identities = g_list_delete_link(valuestruct->identities,valuestruct->identities);
	}
	while(valuestruct->features) {
		g_free(valuestruct->features->data);
		valuestruct->features = g_list_delete_link(valuestruct->features,valuestruct->features);
	}
	g_free(valuestruct);
}
#endif

static void jabber_caps_load(void);

void jabber_caps_init(void)
{
	capstable = g_hash_table_new_full(jabber_caps_hash, jabber_caps_compare, jabber_caps_destroy_key, jabber_caps_client_info_destroy);
	jabber_caps_load();
}

void jabber_caps_uninit(void)
{
	g_hash_table_destroy(capstable);
	capstable = NULL;
}

static void jabber_caps_load(void) {
	xmlnode *capsdata = purple_util_read_xml_from_file(JABBER_CAPS_FILENAME, "XMPP capabilities cache");
	xmlnode *client;

	if(!capsdata)
		return;

	if (strcmp(capsdata->name, "capabilities") != 0) {
		xmlnode_free(capsdata);
		return;
	}
	
	for(client = capsdata->child; client; client = client->next) {
		if(client->type != XMLNODE_TYPE_TAG)
			continue;
		if(!strcmp(client->name, "client")) {
			JabberCapsKey *key = g_new0(JabberCapsKey, 1);
			JabberCapsClientInfo *value = g_new0(JabberCapsClientInfo, 1);
			xmlnode *child;
			key->node = g_strdup(xmlnode_get_attrib(client,"node"));
			key->ver  = g_strdup(xmlnode_get_attrib(client,"ver"));
			key->hash = g_strdup(xmlnode_get_attrib(client,"hash"));
			for(child = client->child; child; child = child->next) {
				if(child->type != XMLNODE_TYPE_TAG)
					continue;
				if(!strcmp(child->name,"feature")) {
					const char *var = xmlnode_get_attrib(child, "var");
					if(!var)
						continue;
					value->features = g_list_append(value->features,g_strdup(var));
				} else if(!strcmp(child->name,"identity")) {
					const char *category = xmlnode_get_attrib(child, "category");
					const char *type = xmlnode_get_attrib(child, "type");
					const char *name = xmlnode_get_attrib(child, "name");
					const char *lang = xmlnode_get_attrib(child, "lang");
					JabberIdentity *id;

					if (!category || !type)
						continue;

					id = g_new0(JabberIdentity, 1);
					id->category = g_strdup(category);
					id->type = g_strdup(type);
					id->name = g_strdup(name);
					id->lang = g_strdup(lang);
					
					value->identities = g_list_append(value->identities,id);
				} else if(!strcmp(child->name,"x")) {
					value->forms = g_list_append(value->forms, xmlnode_copy(child));
				}
			}
			g_hash_table_replace(capstable, key, value);
		}
	}
	xmlnode_free(capsdata);
}

#if 0
static void jabber_caps_store_ext(gpointer key, gpointer value, gpointer user_data) {
	const char *extname = key;
	JabberCapsValueExt *props = value;
	xmlnode *root = user_data;
	xmlnode *ext = xmlnode_new_child(root,"ext");
	GList *iter;

	xmlnode_set_attrib(ext,"identifier",extname);

	for(iter = props->identities; iter; iter = g_list_next(iter)) {
		JabberCapsIdentity *id = iter->data;
		xmlnode *identity = xmlnode_new_child(ext, "identity");
		xmlnode_set_attrib(identity, "category", id->category);
		xmlnode_set_attrib(identity, "type", id->type);
		if (id->name)
			xmlnode_set_attrib(identity, "name", id->name);
	}

	for(iter = props->features; iter; iter = g_list_next(iter)) {
		const char *feat = iter->data;
		xmlnode *feature = xmlnode_new_child(ext, "feature");
		xmlnode_set_attrib(feature, "var", feat);
	}
}
#endif

static void jabber_caps_store_client(gpointer key, gpointer value, gpointer user_data) {
	JabberCapsKey *clientinfo = key;
	JabberCapsClientInfo *props = value;
	xmlnode *root = user_data;
	xmlnode *client = xmlnode_new_child(root, "client");
	GList *iter;

	xmlnode_set_attrib(client, "node", clientinfo->node);
	xmlnode_set_attrib(client, "ver", clientinfo->ver);
	xmlnode_set_attrib(client, "hash", clientinfo->hash);
	for(iter = props->identities; iter; iter = g_list_next(iter)) {
		JabberIdentity *id = iter->data;
		xmlnode *identity = xmlnode_new_child(client, "identity");
		xmlnode_set_attrib(identity, "category", id->category);
		xmlnode_set_attrib(identity, "type", id->type);
		if (id->name)
			xmlnode_set_attrib(identity, "name", id->name);
		if (id->lang)
			xmlnode_set_attrib(identity, "lang", id->lang);
	}

	for(iter = props->features; iter; iter = g_list_next(iter)) {
		const char *feat = iter->data;
		xmlnode *feature = xmlnode_new_child(client, "feature");
		xmlnode_set_attrib(feature, "var", feat);
	}
	
	for(iter = props->forms; iter; iter = g_list_next(iter)) {
		xmlnode *xdata = iter->data;
		xmlnode_insert_child(client, xmlnode_copy(xdata));
	}
}

static void jabber_caps_store(void) {
	char *str;
	int length = 0;
	xmlnode *root = xmlnode_new("capabilities");
	g_hash_table_foreach(capstable, jabber_caps_store_client, root);
	str = xmlnode_to_formatted_str(root, &length);
	xmlnode_free(root);
	purple_util_write_data_to_file(JABBER_CAPS_FILENAME, str, length);
	g_free(str);
}

#if 0
/* this function assumes that all information is available locally */
static JabberCapsClientInfo *jabber_caps_collect_info(const char *node, const char *ver, GList *ext) {
	JabberCapsClientInfo *result;
	JabberCapsKey *key = g_new0(JabberCapsKey, 1);
	JabberCapsValue *caps;
	GList *iter;

	key->node = (char *)node;
	key->ver = (char *)ver;

	caps = g_hash_table_lookup(capstable,key);

	g_free(key);

	if (caps == NULL)
		return NULL;

	result = g_new0(JabberCapsClientInfo, 1);

	/* join all information */
	for(iter = caps->identities; iter; iter = g_list_next(iter)) {
		JabberCapsIdentity *id = iter->data;
		JabberCapsIdentity *newid = g_new0(JabberCapsIdentity, 1);
		newid->category = g_strdup(id->category);
		newid->type = g_strdup(id->type);
		newid->name = g_strdup(id->name);
		
		result->identities = g_list_append(result->identities,newid);
	}
	for(iter = caps->features; iter; iter = g_list_next(iter)) {
		const char *feat = iter->data;
		char *newfeat = g_strdup(feat);
		
		result->features = g_list_append(result->features,newfeat);
	}
#if 0	
	for(iter = ext; iter; iter = g_list_next(iter)) {
		const char *extname = iter->data;
		JabberCapsValueExt *extinfo = g_hash_table_lookup(caps->ext,extname);
		
		if(extinfo) {
			GList *iter2;
			for(iter2 = extinfo->identities; iter2; iter2 = g_list_next(iter2)) {
				JabberCapsIdentity *id = iter2->data;
				JabberCapsIdentity *newid = g_new0(JabberCapsIdentity, 1);
				newid->category = g_strdup(id->category);
				newid->type = g_strdup(id->type);
				newid->name = g_strdup(id->name);
				
				result->identities = g_list_append(result->identities,newid);
			}
			for(iter2 = extinfo->features; iter2; iter2 = g_list_next(iter2)) {
				const char *feat = iter2->data;
				char *newfeat = g_strdup(feat);
				
				result->features = g_list_append(result->features,newfeat);
			}
		}
	}
#endif
	return result;
}
#endif

typedef struct _jabber_caps_cbplususerdata {
	jabber_caps_get_info_cb cb;
	gpointer user_data;
	
	char *who;
	char *node;
	char *ver;
	char *hash;
#if 0
	unsigned extOutstanding;
#endif
} jabber_caps_cbplususerdata;

#if 0
typedef struct jabber_ext_userdata {
	jabber_caps_cbplususerdata *userdata;
	char *node;
} jabber_ext_userdata;
#endif

#if 0
static void jabber_caps_get_info_check_completion(jabber_caps_cbplususerdata *userdata) {
	if(userdata->extOutstanding == 0) {
		userdata->cb(jabber_caps_collect_info(userdata->node, userdata->ver, userdata->ext), userdata->user_data);
		g_free(userdata->who);
		g_free(userdata->node);
		g_free(userdata->ver);
		while(userdata->ext) {
			g_free(userdata->ext->data);
			userdata->ext = g_list_delete_link(userdata->ext,userdata->ext);
		}
		g_free(userdata);
	}
}
#endif
#if 0
static void jabber_caps_ext_iqcb(JabberStream *js, xmlnode *packet, gpointer data) {
	/* collect data and fetch all exts */
	xmlnode *query = xmlnode_get_child_with_namespace(packet, "query", "http://jabber.org/protocol/disco#info");
	jabber_ext_userdata *extuserdata = data;
	jabber_caps_cbplususerdata *userdata = extuserdata->userdata;
	const char *node = extuserdata->node;

	--userdata->extOutstanding;

	/* TODO: Better error handling */
	printf("\n\tjabber_caps_ext_iqcb for %s", xmlnode_get_attrib(packet, "from"));
	if(node && query) {
		const char *key;
		JabberCapsValue *client;
		xmlnode *child;
		JabberCapsValueExt *value = g_new0(JabberCapsValueExt, 1);
		JabberCapsKey *clientkey = g_new0(JabberCapsKey, 1);

		clientkey->node = userdata->node;
		clientkey->ver = userdata->ver;

		client = g_hash_table_lookup(capstable, clientkey);

		g_free(clientkey);

		/* split node by #, key either points to \0 or the correct ext afterwards */
		for(key = node; key[0] != '\0'; ++key) {
			if(key[0] == '#') {
				++key;
				break;
			}
		}

		for(child = query->child; child; child = child->next) {
			if(child->type != XMLNODE_TYPE_TAG)
				continue;
			if(!strcmp(child->name,"feature")) {
				const char *var = xmlnode_get_attrib(child, "var");
				if(!var)
					continue;
				value->features = g_list_append(value->features,g_strdup(var));
			} else if(!strcmp(child->name,"identity")) {
				const char *category = xmlnode_get_attrib(child, "category");
				const char *type = xmlnode_get_attrib(child, "type");
				const char *name = xmlnode_get_attrib(child, "name");

				JabberCapsIdentity *id = g_new0(JabberCapsIdentity, 1);
				id->category = g_strdup(category);
				id->type = g_strdup(type);
				id->name = g_strdup(name);

				value->identities = g_list_append(value->identities,id);
			}
		}
		jabber_caps_store();
	}

	g_free(extuserdata->node);
	g_free(extuserdata);
	jabber_caps_get_info_check_completion(userdata);
}
#endif

static void
jabber_caps_client_iqcb(JabberStream *js, xmlnode *packet, gpointer data)
{
	xmlnode *query = xmlnode_get_child_with_namespace(packet, "query",
		"http://jabber.org/protocol/disco#info");
	jabber_caps_cbplususerdata *userdata = data;
	JabberCapsClientInfo *info, *value;
	gchar *hash;
	const char *type = xmlnode_get_attrib(packet, "type");
	JabberCapsKey key;

	if (!query || !strcmp(type, "error")) {
		userdata->cb(NULL, userdata->user_data);

		g_free(userdata->who);
		g_free(userdata->node);
		g_free(userdata->ver);
		g_free(userdata->hash);
		g_free(userdata);
		return;
	}

	/* check hash */
	info = jabber_caps_parse_client_info(query);

	if (!strcmp(userdata->hash, "sha-1")) {
		hash = jabber_caps_calculate_hash(info, "sha1");
	} else if (!strcmp(userdata->hash, "md5")) {
		hash = jabber_caps_calculate_hash(info, "md5");
	} else {
		purple_debug_warning("jabber", "unknown caps hash algorithm: %s\n", userdata->hash);

		userdata->cb(NULL, userdata->user_data);

		jabber_caps_client_info_destroy(info);
		g_free(userdata->who);
		g_free(userdata->node);
		g_free(userdata->ver);
		g_free(userdata->hash);
		g_free(userdata);		
		return;	
	}

	if (!hash || strcmp(hash, userdata->ver)) {
		purple_debug_warning("jabber", "caps hash from %s did not match\n", xmlnode_get_attrib(packet, "from"));
		userdata->cb(NULL, userdata->user_data);

		jabber_caps_client_info_destroy(info);
		g_free(userdata->who);
		g_free(userdata->node);
		g_free(userdata->ver);
		g_free(userdata->hash);
		g_free(userdata);		
		g_free(hash);
		return;
	}

	key.node = userdata->node;
	key.ver = userdata->ver;
	key.hash = userdata->hash;

	/* check whether it's not in the table */
	if ((value = g_hash_table_lookup(capstable, &key))) {
		JabberCapsClientInfo *tmp = info;
		info = value;
		jabber_caps_client_info_destroy(tmp);
	} else {
		JabberCapsKey *n_key = g_new(JabberCapsKey, 1);
		n_key->node = userdata->node;
		n_key->ver = userdata->ver;
		n_key->hash = userdata->hash;
		userdata->node = userdata->ver = userdata->hash = NULL;

		g_hash_table_insert(capstable, n_key, info);
		jabber_caps_store();
	}

	userdata->cb(info, userdata->user_data);

	/* capstable will free info */
	g_free(userdata->who);
	g_free(userdata->node);
	g_free(userdata->ver);
	g_free(userdata->hash);	
	g_free(userdata);
	g_free(hash);
}

void jabber_caps_get_info(JabberStream *js, const char *who, const char *node,
		const char *ver, const char *hash, jabber_caps_get_info_cb cb,
		gpointer user_data)
{
	JabberCapsClientInfo *client;
	JabberCapsKey *key = g_new0(JabberCapsKey, 1);
	jabber_caps_cbplususerdata *userdata = g_new0(jabber_caps_cbplususerdata, 1);
	userdata->cb = cb;
	userdata->user_data = user_data;
	userdata->who = g_strdup(who);
	userdata->node = g_strdup(node);
	userdata->ver = g_strdup(ver);
	userdata->hash = g_strdup(hash);

	key->node = g_strdup(node);
	key->ver = g_strdup(ver);
	key->hash = g_strdup(hash);
	
	client = g_hash_table_lookup(capstable, key);
	
	if(!client) {
		JabberIq *iq = jabber_iq_new_query(js,JABBER_IQ_GET,"http://jabber.org/protocol/disco#info");
		xmlnode *query = xmlnode_get_child_with_namespace(iq->node,"query","http://jabber.org/protocol/disco#info");
		char *nodever = g_strdup_printf("%s#%s", node, ver);
		xmlnode_set_attrib(query, "node", nodever);
		g_free(nodever);
		xmlnode_set_attrib(iq->node, "to", who);

		jabber_iq_set_callback(iq,jabber_caps_client_iqcb,userdata);
		jabber_iq_send(iq);
	}
#if 0
	} else {
		GList *iter;
		/* fetch unknown exts only */
		for(iter = userdata->ext; iter; iter = g_list_next(iter)) {
			JabberCapsValueExt *extvalue = g_hash_table_lookup(client->ext, (const char*)iter->data);
			JabberIq *iq;
			xmlnode *query;
			char *nodever;
			jabber_ext_userdata *ext_data;

			if(extvalue) {
				/* we already have this ext, don't bother with it */
				--userdata->extOutstanding;
				continue;
			}

			ext_data = g_new0(jabber_ext_userdata, 1);

			iq = jabber_iq_new_query(js,JABBER_IQ_GET,"http://jabber.org/protocol/disco#info");
			query = xmlnode_get_child_with_namespace(iq->node,"query","http://jabber.org/protocol/disco#info");
			nodever = g_strdup_printf("%s#%s", node, (const char*)iter->data);
			xmlnode_set_attrib(query, "node", nodever);
			xmlnode_set_attrib(iq->node, "to", who);

			ext_data->node = nodever;
			ext_data->userdata = userdata;

			jabber_iq_set_callback(iq, jabber_caps_ext_iqcb, ext_data);
			jabber_iq_send(iq);
		}
		/* maybe we have all data available anyways? This is the ideal case where no network traffic is necessary */
		jabber_caps_get_info_check_completion(userdata);
	}
#endif
}

static gint jabber_caps_jabber_identity_compare(gconstpointer a, gconstpointer b) {
	const JabberIdentity *ac;
	const JabberIdentity *bc;
	gint cat_cmp;
	gint typ_cmp;
	
	ac = a;
	bc = b;

	if ((cat_cmp = strcmp(ac->category, bc->category)) == 0) {
		if ((typ_cmp = strcmp(ac->type, bc->type)) == 0) {
			if (!ac->lang && !bc->lang) {
				return 0;
			} else if (ac->lang && !bc->lang) {
				return 1;
			} else if (!ac->lang && bc->lang) {
				return -1;
			} else {
				return strcmp(ac->lang, bc->lang);
			}
		} else {
			return typ_cmp;
		}
	} else {
		return cat_cmp;
	}
}

#if 0
static gint jabber_caps_jabber_feature_compare(gconstpointer a, gconstpointer b) {
	const JabberFeature *ac;
	const JabberFeature *bc;
	
	ac = a;
	bc = b;
	
	return strcmp(ac->namespace, bc->namespace);
}
#endif

static gchar *jabber_caps_get_formtype(const xmlnode *x) {
	xmlnode *formtypefield;
	formtypefield = xmlnode_get_child(x, "field");
	while (formtypefield && strcmp(xmlnode_get_attrib(formtypefield, "var"), "FORM_TYPE")) formtypefield = xmlnode_get_next_twin(formtypefield);
	formtypefield = xmlnode_get_child(formtypefield, "value");
	return xmlnode_get_data(formtypefield);;
}

static gint jabber_caps_jabber_xdata_compare(gconstpointer a, gconstpointer b) {
	const xmlnode *aformtypefield = a;
	const xmlnode *bformtypefield = b;
	char *aformtype;
	char *bformtype;
	int result;

	aformtype = jabber_caps_get_formtype(aformtypefield);
	bformtype = jabber_caps_get_formtype(bformtypefield);
	
	result = strcmp(aformtype, bformtype);
	g_free(aformtype);
	g_free(bformtype);
	return result;
}

static JabberCapsClientInfo *jabber_caps_parse_client_info(xmlnode *query)
{
	xmlnode *child;
	JabberCapsClientInfo *info;

	if (!query || strcmp(query->xmlns, "http://jabber.org/protocol/disco#info"))
		return 0;
	
	info = g_new0(JabberCapsClientInfo, 1);
	
	for(child = query->child; child; child = child->next) {
		if (!strcmp(child->name,"identity")) {
			/* parse identity */
			const char *category = xmlnode_get_attrib(child, "category");
			const char *type = xmlnode_get_attrib(child, "type");
			const char *name = xmlnode_get_attrib(child, "name");
			const char *lang = xmlnode_get_attrib(child, "lang");
			JabberIdentity *id;

			if (!category || !type)
				continue;

			id = g_new0(JabberIdentity, 1);
			id->category = g_strdup(category);
			id->type = g_strdup(type);
			id->name = g_strdup(name);
			id->lang = g_strdup(lang);

			info->identities = g_list_append(info->identities, id);
		} else if (!strcmp(child->name, "feature")) {
			/* parse feature */
			const char *var = xmlnode_get_attrib(child, "var");
			if(!var)
				continue;
			info->features = g_list_append(info->features, g_strdup(var));
		} else if (!strcmp(child->name, "x")) {
			if (child->xmlns && !strcmp(child->xmlns, "jabber:x:data")) {
				/* x-data form */
				xmlnode *dataform = xmlnode_copy(child);
				info->forms = g_list_append(info->forms, dataform);
			}
		}
	}
	return info;
}

static gint jabber_caps_xdata_field_compare(gconstpointer a, gconstpointer b)
{
	const JabberDataFormField *ac = a;
	const JabberDataFormField *bc = b;

	return strcmp(ac->var, bc->var);
}

static GList* jabber_caps_xdata_get_fields(const xmlnode *x)
{
	GList *fields = NULL;
	xmlnode *field;

	if (!x)
		return NULL;

	for (field = xmlnode_get_child(x, "field"); field; field = xmlnode_get_next_twin(field)) {
		xmlnode *value;
		JabberDataFormField *xdatafield = g_new0(JabberDataFormField, 1);
		xdatafield->var = g_strdup(xmlnode_get_attrib(field, "var"));

		for (value = xmlnode_get_child(field, "value"); value; value = xmlnode_get_next_twin(value)) {
			gchar *val = xmlnode_get_data(value);
			xdatafield->values = g_list_append(xdatafield->values, val);
		}

		xdatafield->values = g_list_sort(xdatafield->values, (GCompareFunc)strcmp);
		fields = g_list_append(fields, xdatafield);
	}

	fields = g_list_sort(fields, jabber_caps_xdata_field_compare);
	return fields;
}

static GString*
jabber_caps_verification_append(GString *verification, const gchar *string)
{
	verification = g_string_append(verification, string);
	return g_string_append_c(verification, '<');
}

gchar *jabber_caps_calculate_hash(JabberCapsClientInfo *info, const char *hash)
{
	GList *node;
	GString *verification;
	PurpleCipherContext *context;
	guint8 checksum[20];
	gsize checksum_size = 20;

	if (!info || !(context = purple_cipher_context_new_by_name(hash, NULL)))
		return NULL;

	/* sort identities, features and x-data forms */
	info->identities = g_list_sort(info->identities, jabber_caps_jabber_identity_compare);
	info->features = g_list_sort(info->features, (GCompareFunc)strcmp);
	info->forms = g_list_sort(info->forms, jabber_caps_jabber_xdata_compare);

	verification = g_string_new("");

	/* concat identities to the verification string */
	for (node = info->identities; node; node = node->next) {
		JabberIdentity *id = (JabberIdentity*)node->data;

		g_string_append_printf(verification, "%s/%s/%s/%s<", id->category,
		        id->type, id->lang ? id->lang : "", id->name);
	}

	/* concat features to the verification string */
	for (node = info->features; node; node = node->next) {
		verification = jabber_caps_verification_append(verification, node->data);
	}

	/* concat x-data forms to the verification string */
	for(node = info->forms; node; node = node->next) {
		xmlnode *data = (xmlnode *)node->data;
		gchar *formtype = jabber_caps_get_formtype(data);
		GList *fields = jabber_caps_xdata_get_fields(data);

		/* append FORM_TYPE's field value to the verification string */
		verification = jabber_caps_verification_append(verification, formtype);
		g_free(formtype);

		while (fields) {
			GList *value;
			JabberDataFormField *field = (JabberDataFormField*)fields->data; 

			if (strcmp(field->var, "FORM_TYPE")) {
				/* Append the "var" attribute */
				verification = jabber_caps_verification_append(verification, field->var);
				/* Append <value/> element's cdata */
				for(value = field->values; value; value = value->next) {
					verification = jabber_caps_verification_append(verification, value->data);
					g_free(value->data);
				}
			}

			g_free(field->var);
			g_list_free(field->values);

			fields = g_list_delete_link(fields, fields);
		}
	}

	/* generate hash */
	purple_cipher_context_append(context, (guchar*)verification->str, verification->len);

	if (!purple_cipher_context_digest(context, verification->len, checksum, &checksum_size)) {
		/* purple_debug_error("util", "Failed to get digest.\n"); */
		g_string_free(verification, TRUE);
		purple_cipher_context_destroy(context);
		return NULL;
	}

	g_string_free(verification, TRUE);
	purple_cipher_context_destroy(context);

	return purple_base64_encode(checksum, checksum_size);
}

void jabber_caps_calculate_own_hash(JabberStream *js) {
	JabberCapsClientInfo info;
	GList *iter = 0;
	GList *features = 0;

	if (!jabber_identities && !jabber_features) {
		/* This really shouldn't ever happen */
		purple_debug_warning("jabber", "No features or identities, cannot calculate own caps hash.\n");
		g_free(js->caps_hash);
		js->caps_hash = NULL;
		return;
	}

	/* build the currently-supported list of features */
	if (jabber_features) {
		for (iter = jabber_features; iter; iter = iter->next) {
			JabberFeature *feat = iter->data;
			if(!feat->is_enabled || feat->is_enabled(js, feat->namespace)) {
				features = g_list_append(features, feat->namespace);
			}
		}
	}

	info.features = features;
	info.identities = jabber_identities;
	info.forms = NULL;

	g_free(js->caps_hash);
	js->caps_hash = jabber_caps_calculate_hash(&info, "sha1");
	g_list_free(features);
}

const gchar* jabber_caps_get_own_hash(JabberStream *js)
{
	if (!js->caps_hash)
		jabber_caps_calculate_own_hash(js);

	return js->caps_hash;
}

void jabber_caps_broadcast_change()
{
	GList *node, *accounts = purple_accounts_get_all_active();

	for (node = accounts; node; node = node->next) {
		PurpleAccount *account = node->data;
		const char *prpl_id = purple_account_get_protocol_id(account);
		if (!strcmp("prpl-jabber", prpl_id)) {
			PurpleConnection *gc = purple_account_get_connection(account);
			jabber_presence_send(gc->proto_data, TRUE);
		}
	}

	g_list_free(accounts);
}

