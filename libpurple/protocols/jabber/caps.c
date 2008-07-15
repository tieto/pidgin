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

#include <glib.h>
#include "caps.h"
#include "cipher.h"
#include <string.h>
#include "internal.h"
#include "util.h"
#include "iq.h"

#define JABBER_CAPS_FILENAME "xmpp-caps.xml"

GHashTable *capstable = NULL; /* JabberCapsKey -> JabberCapsValue */
static gchar *caps_hash = NULL;

#if 0
typedef struct _JabberCapsValue {
	GList *identities; /* JabberCapsIdentity */
	GList *features; /* char * */
	GHashTable *ext; /* char * -> JabberCapsValueExt */
} JabberCapsValue;
#endif
typedef struct _JabberCapsClientInfo JabberCapsValue;

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
	
	return strcmp(name1->node,name2->node) == 0 && strcmp(name1->ver,name2->ver) == 0 && strcmp(name1->hash,name2->hash) == 0;
}

void jabber_caps_destroy_key(gpointer key) {
	JabberCapsKey *keystruct = key;
	g_free(keystruct->node);
	g_free(keystruct->ver);
	g_free(keystruct->hash);
	g_free(keystruct);
}

static void jabber_caps_destroy_value(gpointer value) {
	JabberCapsValue *valuestruct = value;
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

	while(valuestruct->forms) {
		g_free(valuestruct->forms->data);
		valuestruct->forms = g_list_delete_link(valuestruct->forms,valuestruct->forms);
	}
	//g_hash_table_destroy(valuestruct->ext);
	g_free(valuestruct);
}

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

static void jabber_caps_load(void);

void jabber_caps_init(void) {
	capstable = g_hash_table_new_full(jabber_caps_hash, jabber_caps_compare, jabber_caps_destroy_key, jabber_caps_destroy_value);
	jabber_caps_load();
	jabber_caps_calculate_own_hash();
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
			JabberCapsValue *value = g_new0(JabberCapsValue, 1);
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
					
					JabberCapsIdentity *id = g_new0(JabberCapsIdentity, 1);
					id->category = g_strdup(category);
					id->type = g_strdup(type);
					id->name = g_strdup(name);
					
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
	JabberCapsValue *props = value;
	xmlnode *root = user_data;
	xmlnode *client = xmlnode_new_child(root,"client");
	GList *iter;

	xmlnode_set_attrib(client,"node",g_strdup(clientinfo->node));
	xmlnode_set_attrib(client,"ver",g_strdup(clientinfo->ver));
	xmlnode_set_attrib(client,"hash",g_strdup(clientinfo->hash));
	for(iter = props->identities; iter; iter = g_list_next(iter)) {
		JabberCapsIdentity *id = iter->data;
		xmlnode *identity = xmlnode_new_child(client, "identity");
		xmlnode_set_attrib(identity, "category", g_strdup(id->category));
		xmlnode_set_attrib(identity, "type", g_strdup(id->type));
		if (id->name)
			xmlnode_set_attrib(identity, "name", g_strdup(id->name));
	}

	for(iter = props->features; iter; iter = g_list_next(iter)) {
		const char *feat = iter->data;
		xmlnode *feature = xmlnode_new_child(client, "feature");
		xmlnode_set_attrib(feature, "var", g_strdup(feat));
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

void jabber_caps_free_clientinfo(JabberCapsClientInfo *clientinfo) {
	if(!clientinfo)
		return;
	while(clientinfo->identities) {
		JabberCapsIdentity *id = clientinfo->identities->data;
		g_free(id->category);
		g_free(id->type);
		g_free(id->name);
		g_free(id);
		
		clientinfo->identities = g_list_delete_link(clientinfo->identities,clientinfo->identities);
	}
	while(clientinfo->features) {
		char *feat = clientinfo->features->data;
		g_free(feat);
		
		clientinfo->features = g_list_delete_link(clientinfo->features,clientinfo->features);
	}
	
	g_free(clientinfo);
}

typedef struct _jabber_caps_cbplususerdata {
	jabber_caps_get_info_cb cb;
	gpointer user_data;
	
	char *who;
	char *node;
	char *ver;
	char *hash;
	unsigned extOutstanding;
} jabber_caps_cbplususerdata;

typedef struct jabber_ext_userdata {
	jabber_caps_cbplususerdata *userdata;
	char *node;
} jabber_ext_userdata;

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
static void jabber_caps_client_iqcb(JabberStream *js, xmlnode *packet, gpointer data) {
	/* collect data and fetch all exts */
	xmlnode *query = xmlnode_get_child_with_namespace(packet, "query",
		"http://jabber.org/protocol/disco#info");
	xmlnode *child;
	jabber_caps_cbplususerdata *userdata = data;

	/* TODO: Better error checking! */
	if (!strcmp(xmlnode_get_attrib(packet, "type"), "error"))return;
	if (query) {
		// check hash
		JabberCapsClientInfo *info = jabber_caps_parse_client_info(query);
		gchar *hash = 0;
		if (!strcmp(userdata->hash, "sha-1")) {
			hash = jabber_caps_calcualte_hash(info, "sha1");
		} else if (!strcmp(userdata->hash, "md5")) {
			hash = jabber_caps_calcualte_hash(info, "md5");
		} else {
			// clean up
			return;	
		}

		printf("\n\tfrom:            %s", xmlnode_get_attrib(packet, "from"));
		printf("\n\tnode:            %s", xmlnode_get_attrib(query, "node"));
		printf("\n\tcalculated key:  %s", hash);
		printf("\n\thash:            %s", userdata->hash);
		printf("\n");
		
		if (strcmp(hash, userdata->ver)) {
			g_free(info);
			g_free(hash);
			printf("\n! ! ! invalid hash ! ! !");
			return;
		}
		
		g_free(hash);
		
		JabberCapsValue *value = g_new0(JabberCapsValue, 1);
		JabberCapsKey *key = g_new0(JabberCapsKey, 1);
#if 0
		value->ext = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, jabber_caps_ext_destroy_value);
#endif
		key->node = g_strdup(userdata->node);
		key->ver = g_strdup(userdata->ver);
		key->hash = g_strdup(userdata->hash);
		
		/* check whether it's stil not in the table */
		if (g_hash_table_lookup(capstable, key)) {
			jabber_caps_destroy_key(key);
			g_free(value);
			return;
		}
		
		
		for(child = query->child; child; child = child->next) {
			if(child->type != XMLNODE_TYPE_TAG)
				continue;
			if(!strcmp(child->name,"feature")) {
				const char *var = xmlnode_get_attrib(child, "var");
				if(!var)
					continue;
				value->features = g_list_append(value->features, g_strdup(var));
			} else if(!strcmp(child->name,"identity")) {
				const char *category = xmlnode_get_attrib(child, "category");
				const char *type = xmlnode_get_attrib(child, "type");
				const char *name = xmlnode_get_attrib(child, "name");
				
				JabberCapsIdentity *id = g_new0(JabberCapsIdentity, 1);
				id->category = g_strdup(category);
				id->type = g_strdup(type);
				id->name = g_strdup(name);

				value->identities = g_list_append(value->identities,id);
			} else if(!strcmp(child->name, "x")) {
				value->forms = g_list_append(value->forms, xmlnode_copy(child));	
			}
		}

		g_hash_table_replace(capstable, key, value);
		jabber_caps_store();
	}

#if 0
	/* fetch all exts */
	for(iter = userdata->ext; iter; iter = g_list_next(iter)) {
		JabberIq *iq = jabber_iq_new_query(js, JABBER_IQ_GET, "http://jabber.org/protocol/disco#info");
		xmlnode *query = xmlnode_get_child_with_namespace(iq->node, "query", "http://jabber.org/protocol/disco#info");
		char *node = g_strdup_printf("%s#%s", userdata->node, (const char*)iter->data);
		jabber_ext_userdata *ext_data = g_new0(jabber_ext_userdata, 1);
		ext_data->node = node;
		ext_data->userdata = userdata;

		xmlnode_set_attrib(query, "node", node);
		xmlnode_set_attrib(iq->node, "to", userdata->who);

		jabber_iq_set_callback(iq, jabber_caps_ext_iqcb, ext_data);
		jabber_iq_send(iq);
	}

	jabber_caps_get_info_check_completion(userdata);
#endif
}

void jabber_caps_get_info(JabberStream *js, const char *who, const char *node, const char *ver, const char *hash, jabber_caps_get_info_cb cb, gpointer user_data) {
	JabberCapsValue *client;
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

	g_hash_table_replace(jabber_contact_info, g_strdup(who), key);
	
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
			return strcmp(ac->lang, bc->lang);
		} else {
			return typ_cmp;
		}
	} else {
		return cat_cmp;
	}
}

static gint jabber_caps_jabber_feature_compare(gconstpointer a, gconstpointer b) {
	const JabberFeature *ac;
	const JabberFeature *bc;
	
	ac = a;
	bc = b;
	
	return strcmp(ac->namespace, bc->namespace);
}

static gint jabber_caps_string_compare(gconstpointer a, gconstpointer b) {
	const gchar *ac;
	const gchar *bc;
	
	ac = a;
	bc = b;
	
	return strcmp(ac, bc);
}

gchar *jabber_caps_get_formtype(const xmlnode *x) {
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

JabberCapsClientInfo *jabber_caps_parse_client_info(xmlnode *query) {
	xmlnode *child;
	
	if (!query) return 0;
	if (strcmp(query->xmlns,"http://jabber.org/protocol/disco#info")) return 0;
	
	JabberCapsClientInfo *info = g_new0(JabberCapsClientInfo, 1);
	
	for(child = query->child; child; child = child->next) {
		if (!strcmp(child->name,"identity")) {
			/* parse identity */
			const char *category = xmlnode_get_attrib(child, "category");
			const char *type = xmlnode_get_attrib(child, "type");
			const char *name = xmlnode_get_attrib(child, "name");

			JabberCapsIdentity *id = g_new0(JabberCapsIdentity, 1);
			id->category = g_strdup(category);
			id->type = g_strdup(type);
			id->name = g_strdup(name);
			
			info->identities = g_list_append(info->identities, id);
		} else if (!strcmp(child->name, "feature")) {
			/* parse feature */
			const char *var = xmlnode_get_attrib(child, "var");
			if(!var)
				continue;
			info->features = g_list_append(info->features, g_strdup(var));
		} else if (!strcmp(child->name, "x")) {
			if (!strcmp(child->xmlns, "jabber:x:data")) {
				/* x-data form */
				xmlnode *dataform = xmlnode_copy(child);
				info->forms = g_list_append(info->forms, dataform);
			}
		}
	}
	return info;
}

static gint jabber_caps_xdata_field_compare(gconstpointer a, gconstpointer b) {
	const JabberDataFormField *ac;
	const JabberDataFormField *bc;
	
	ac = a;
	bc = b;
	
	return strcmp(ac->var, bc->var);
}

GList *jabber_caps_xdata_get_fields(const xmlnode *x) {
	GList *fields = 0;
	xmlnode *field;
	xmlnode *value;
	JabberDataFormField *xdatafield;
	
	if(!x) return 0;
	
	for(field = xmlnode_get_child(x, "field"); field != 0; field = xmlnode_get_next_twin(field)) {
		xdatafield = g_new0(JabberDataFormField, 1);
		xdatafield->var = g_strdup(xmlnode_get_attrib(field, "var"));
		for(value = xmlnode_get_child(field, "value"); value != 0; value = xmlnode_get_next_twin(value)) {
			gchar *val = xmlnode_get_data(value);
			xdatafield->values = g_list_append(xdatafield->values, val);
		}
		xdatafield->values = g_list_sort(xdatafield->values, jabber_caps_string_compare);
		fields = g_list_append(fields, xdatafield);
	} 
	fields = g_list_sort(fields, jabber_caps_xdata_field_compare);
	return fields;
}

gchar *jabber_caps_verification_append(gchar *verification_string, gchar *string) {
	gchar *verification;
	verification = g_strconcat(verification_string, string, "<", NULL);
	g_free(verification_string);
	return verification;
}

gchar *jabber_caps_calcualte_hash(JabberCapsClientInfo *info, const char *hash) {
	GList *identities;
	GList *features;
	GList *xdata;
	gchar *verification = 0;
	gchar *feature_string;
	gchar *free_verification;
	gchar *identity_string;
	PurpleCipherContext *context;
	guint8 checksum[20];
	gsize checksum_size = 20;
	
	if (!info) return 0;
	
	/* sort identities, features and x-data forms */
	info->identities = g_list_sort(info->identities, jabber_caps_jabber_identity_compare);
	info->features = g_list_sort(info->features, jabber_caps_string_compare);
	info->forms = g_list_sort(info->forms, jabber_caps_jabber_xdata_compare);
	
	/* concat identities to the verification string */
	for(identities = info->identities; identities; identities = identities->next) {
		JabberIdentity *ident = (JabberIdentity*)identities->data;
		identity_string = g_strdup_printf("%s/%s//%s<", ident->category, ident->type, ident->name);
		free_verification = verification;
		if(verification == 0) verification = g_strdup(identity_string);
		 	else verification = g_strconcat(verification, identity_string, NULL);
		g_free(identity_string);
		g_free(free_verification);
	}
	
	/* concat features to the verification string */
	for(features = info->features; features; features = features->next) {
		feature_string = g_strdup_printf("%s", (gchar*)features->data);
		verification = jabber_caps_verification_append(verification, feature_string);
		g_free(feature_string);
	}
	
	/* concat x-data forms to the verification string */
	for(xdata = info->forms; xdata; xdata = xdata->next) {
		gchar *formtype = 0;
		GList *fields;
		/* append FORM_TYPE's field value to the verification string */
		formtype = jabber_caps_get_formtype((xmlnode*)xdata->data);
		verification = jabber_caps_verification_append(verification, formtype);
		g_free(formtype);
		
		for(fields = jabber_caps_xdata_get_fields((xmlnode*)(xdata->data)); fields != 0; fields = fields->next) {
			GList *value;
			JabberDataFormField *field = (JabberDataFormField*)fields->data; 
			if(strcmp(field->var, "FORM_TYPE")) {
				/* Append the value of the "var" attribute, followed by the '<' character. */
				verification = jabber_caps_verification_append(verification, field->var);
				/* For each <value/> element, append the XML character data, followed by the '<' character. */
				for(value = field->values; value != 0; value = value->next) {
					verification = jabber_caps_verification_append(verification, value->data);
				}
			}
			for(value = field->values; value != 0; value = value->next) {
				g_free(value->data);
			}
			g_free(field->var);
			g_list_free(field->values);
		}
		g_list_free(fields);
	}
	
	/* generate hash */
	context = purple_cipher_context_new_by_name(hash, NULL);
	if (context == NULL) {
		//purple_debug_error("jabber", "Could not find cipher\n");
		return 0;
	}
	purple_cipher_context_append(context, (guchar*)verification, strlen(verification));
	
	if (!purple_cipher_context_digest(context, strlen(verification), checksum, &checksum_size)) {
		//purple_debug_error("util", "Failed to get digest.\n");
	}
	purple_cipher_context_destroy(context);
	
	/* apply Base64 on hash */
	
	g_free(verification);
	verification = purple_base64_encode(checksum, checksum_size);
	
	return verification;
}

void jabber_caps_calculate_own_hash(JabberStream *js) {
	JabberCapsClientInfo *info;
	GList *iter = 0;
	GList *features = 0;

	if (jabber_identities == 0 && jabber_features == 0) return;

	/* sort features */
	if (jabber_features) {
		for (iter = jabber_features; iter; iter = iter->next) {
			JabberFeature *feat = iter->data;
			if(feat->is_enabled == NULL || feat->is_enabled(js, feat->namespace) == TRUE) {
				features = g_list_append(features, feat->namespace);
			}
		}
	}

	info = g_new0(JabberCapsClientInfo, 1);
	info->features = features;
	info->identities = jabber_identities;
	info->forms = 0;
	
	if (caps_hash) g_free(caps_hash);
	caps_hash = jabber_caps_calcualte_hash(info, "sha1");
	g_free(info);
	g_list_free(features);
}

const gchar* jabber_caps_get_own_hash() {
	return caps_hash;
}

void jabber_caps_broadcast_change() {
	GList *active_accounts = purple_accounts_get_all_active();
	for (active_accounts = purple_accounts_get_all_active(); active_accounts; active_accounts = active_accounts->next) {
		PurpleAccount *account = active_accounts->data;
		if (!strcmp(account->protocol_id, "jabber")) {
			PurpleConnection *conn = account->gc;
			JabberStream *js = conn->proto_data;
			xmlnode *presence = jabber_presence_create_js(js, JABBER_BUDDY_STATE_UNKNOWN, 0, 0);
			jabber_send(js, presence);
		}
	}
}

