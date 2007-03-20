/*
 * purple - Jabber Protocol Plugin
 *
 * Copyright (C) 2003, Nathan Walp <faceprint@faceprint.com>
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
 *
 */
#include "internal.h"
#include "request.h"
#include "server.h"

#include "xdata.h"

typedef enum {
	JABBER_X_DATA_IGNORE = 0,
	JABBER_X_DATA_TEXT_SINGLE,
	JABBER_X_DATA_TEXT_MULTI,
	JABBER_X_DATA_LIST_SINGLE,
	JABBER_X_DATA_LIST_MULTI,
	JABBER_X_DATA_BOOLEAN,
	JABBER_X_DATA_JID_SINGLE
} jabber_x_data_field_type;

struct jabber_x_data_data {
	GHashTable *fields;
	GSList *values;
	jabber_x_data_cb cb;
	gpointer user_data;
	JabberStream *js;
};

static void jabber_x_data_ok_cb(struct jabber_x_data_data *data, PurpleRequestFields *fields) {
	xmlnode *result = xmlnode_new("x");
	jabber_x_data_cb cb = data->cb;
	gpointer user_data = data->user_data;
	JabberStream *js = data->js;
	GList *groups, *flds;

	xmlnode_set_namespace(result, "jabber:x:data");
	xmlnode_set_attrib(result, "type", "submit");

	for(groups = purple_request_fields_get_groups(fields); groups; groups = groups->next) {
		for(flds = purple_request_field_group_get_fields(groups->data); flds; flds = flds->next) {
			xmlnode *fieldnode, *valuenode;
			PurpleRequestField *field = flds->data;
			const char *id = purple_request_field_get_id(field);
			jabber_x_data_field_type type = GPOINTER_TO_INT(g_hash_table_lookup(data->fields, id));

			switch(type) {
				case JABBER_X_DATA_TEXT_SINGLE:
				case JABBER_X_DATA_JID_SINGLE:
					{
					const char *value = purple_request_field_string_get_value(field);
					fieldnode = xmlnode_new_child(result, "field");
					xmlnode_set_attrib(fieldnode, "var", id);
					valuenode = xmlnode_new_child(fieldnode, "value");
					if(value)
						xmlnode_insert_data(valuenode, value, -1);
					break;
					}
				case JABBER_X_DATA_TEXT_MULTI:
					{
					char **pieces, **p;
					const char *value = purple_request_field_string_get_value(field);
					fieldnode = xmlnode_new_child(result, "field");
					xmlnode_set_attrib(fieldnode, "var", id);

					pieces = g_strsplit(value, "\n", -1);
					for(p = pieces; *p != NULL; p++) {
						valuenode = xmlnode_new_child(fieldnode, "value");
						xmlnode_insert_data(valuenode, *p, -1);
					}
					g_strfreev(pieces);
					}
					break;
				case JABBER_X_DATA_LIST_SINGLE:
				case JABBER_X_DATA_LIST_MULTI:
					{
					const GList *selected = purple_request_field_list_get_selected(field);
					char *value;
					fieldnode = xmlnode_new_child(result, "field");
					xmlnode_set_attrib(fieldnode, "var", id);

					while(selected) {
						value = purple_request_field_list_get_data(field, selected->data);
						valuenode = xmlnode_new_child(fieldnode, "value");
						if(value)
							xmlnode_insert_data(valuenode, value, -1);
						selected = selected->next;
					}
					}
					break;
				case JABBER_X_DATA_BOOLEAN:
					fieldnode = xmlnode_new_child(result, "field");
					xmlnode_set_attrib(fieldnode, "var", id);
					valuenode = xmlnode_new_child(fieldnode, "value");
					if(purple_request_field_bool_get_value(field))
						xmlnode_insert_data(valuenode, "1", -1);
					else
						xmlnode_insert_data(valuenode, "0", -1);
					break;
				case JABBER_X_DATA_IGNORE:
					break;
			}
		}
	}

	g_hash_table_destroy(data->fields);
	while(data->values) {
		g_free(data->values->data);
		data->values = g_slist_delete_link(data->values, data->values);
	}
	g_free(data);

	cb(js, result, user_data);
}

static void jabber_x_data_cancel_cb(struct jabber_x_data_data *data, PurpleRequestFields *fields) {
	xmlnode *result = xmlnode_new("x");
	jabber_x_data_cb cb = data->cb;
	gpointer user_data = data->user_data;
	JabberStream *js = data->js;
	g_hash_table_destroy(data->fields);
	while(data->values) {
		g_free(data->values->data);
		data->values = g_slist_delete_link(data->values, data->values);
	}
	g_free(data);

	xmlnode_set_namespace(result, "jabber:x:data");
	xmlnode_set_attrib(result, "type", "cancel");

	cb(js, result, user_data);
}

void *jabber_x_data_request(JabberStream *js, xmlnode *packet, jabber_x_data_cb cb, gpointer user_data)
{
	void *handle;
	xmlnode *fn, *x;
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;

	char *title = NULL;
	char *instructions = NULL;

	struct jabber_x_data_data *data = g_new0(struct jabber_x_data_data, 1);

	data->fields = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	data->user_data = user_data;
	data->cb = cb;
	data->js = js;

	fields = purple_request_fields_new();
	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	for(fn = xmlnode_get_child(packet, "field"); fn; fn = xmlnode_get_next_twin(fn)) {
		xmlnode *valuenode;
		const char *type = xmlnode_get_attrib(fn, "type");
		const char *label = xmlnode_get_attrib(fn, "label");
		const char *var = xmlnode_get_attrib(fn, "var");
		char *value = NULL;

		if(!type)
			continue;

		if(!var && strcmp(type, "fixed"))
			continue;
		if(!label)
			label = var;

		if((valuenode = xmlnode_get_child(fn, "value")))
			value = xmlnode_get_data(valuenode);


		/* XXX: handle <required/> */

		if(!strcmp(type, "text-private")) {
			if((valuenode = xmlnode_get_child(fn, "value")))
				value = xmlnode_get_data(valuenode);

			field = purple_request_field_string_new(var, label,
					value ? value : "", FALSE);
			purple_request_field_string_set_masked(field, TRUE);
			purple_request_field_group_add_field(group, field);

			g_hash_table_replace(data->fields, g_strdup(var), GINT_TO_POINTER(JABBER_X_DATA_TEXT_SINGLE));

			if(value)
				g_free(value);
		} else if(!strcmp(type, "text-multi") || !strcmp(type, "jid-multi")) {
			GString *str = g_string_new("");

			for(valuenode = xmlnode_get_child(fn, "value"); valuenode;
					valuenode = xmlnode_get_next_twin(valuenode)) {

				if(!(value = xmlnode_get_data(valuenode)))
					continue;

				g_string_append_printf(str, "%s\n", value);
				g_free(value);
			}

			field = purple_request_field_string_new(var, label,
					str->str, TRUE);
			purple_request_field_group_add_field(group, field);

			g_hash_table_replace(data->fields, g_strdup(var), GINT_TO_POINTER(JABBER_X_DATA_TEXT_MULTI));

			g_string_free(str, TRUE);
		} else if(!strcmp(type, "list-single") || !strcmp(type, "list-multi")) {
			xmlnode *optnode;
			GList *selected = NULL;

			field = purple_request_field_list_new(var, label);

			if(!strcmp(type, "list-multi")) {
				purple_request_field_list_set_multi_select(field, TRUE);
				g_hash_table_replace(data->fields, g_strdup(var),
						GINT_TO_POINTER(JABBER_X_DATA_LIST_MULTI));
			} else {
				g_hash_table_replace(data->fields, g_strdup(var),
						GINT_TO_POINTER(JABBER_X_DATA_LIST_SINGLE));
			}

			for(valuenode = xmlnode_get_child(fn, "value"); valuenode;
					valuenode = xmlnode_get_next_twin(valuenode)) {
				selected = g_list_prepend(selected, xmlnode_get_data(valuenode));
			}

			for(optnode = xmlnode_get_child(fn, "option"); optnode;
					optnode = xmlnode_get_next_twin(optnode)) {
				const char *lbl;

				if(!(valuenode = xmlnode_get_child(optnode, "value")))
					continue;

				if(!(value = xmlnode_get_data(valuenode)))
					continue;

				if(!(lbl = xmlnode_get_attrib(optnode, "label")))
					label = value;

				data->values = g_slist_prepend(data->values, value);

				purple_request_field_list_add(field, lbl, value);
				if(g_list_find_custom(selected, value, (GCompareFunc)strcmp))
					purple_request_field_list_add_selected(field, lbl);
			}
			purple_request_field_group_add_field(group, field);

			while(selected) {
				g_free(selected->data);
				selected = g_list_delete_link(selected, selected);
			}

		} else if(!strcmp(type, "boolean")) {
			gboolean def = FALSE;

			if((valuenode = xmlnode_get_child(fn, "value")))
				value = xmlnode_get_data(valuenode);

			if(value && (!g_ascii_strcasecmp(value, "yes") ||
						!g_ascii_strcasecmp(value, "true") || !g_ascii_strcasecmp(value, "1")))
				def = TRUE;

			field = purple_request_field_bool_new(var, label, def);
			purple_request_field_group_add_field(group, field);

			g_hash_table_replace(data->fields, g_strdup(var), GINT_TO_POINTER(JABBER_X_DATA_BOOLEAN));

			if(value)
				g_free(value);
		} else if(!strcmp(type, "fixed") && value) {
			if((valuenode = xmlnode_get_child(fn, "value")))
				value = xmlnode_get_data(valuenode);

			field = purple_request_field_label_new("", value);
			purple_request_field_group_add_field(group, field);

			if(value)
				g_free(value);
		} else if(!strcmp(type, "hidden")) {
			if((valuenode = xmlnode_get_child(fn, "value")))
				value = xmlnode_get_data(valuenode);

			field = purple_request_field_string_new(var, "", value ? value : "",
					FALSE);
			purple_request_field_set_visible(field, FALSE);
			purple_request_field_group_add_field(group, field);

			g_hash_table_replace(data->fields, g_strdup(var), GINT_TO_POINTER(JABBER_X_DATA_TEXT_SINGLE));

			if(value)
				g_free(value);
		} else { /* text-single, jid-single, and the default */
			if((valuenode = xmlnode_get_child(fn, "value")))
				value = xmlnode_get_data(valuenode);

			field = purple_request_field_string_new(var, label,
					value ? value : "", FALSE);
			purple_request_field_group_add_field(group, field);

			if(!strcmp(type, "jid-single")) {
				purple_request_field_set_type_hint(field, "screenname");
				g_hash_table_replace(data->fields, g_strdup(var), GINT_TO_POINTER(JABBER_X_DATA_JID_SINGLE));
			} else {
				g_hash_table_replace(data->fields, g_strdup(var), GINT_TO_POINTER(JABBER_X_DATA_TEXT_SINGLE));
			}

			if(value)
				g_free(value);
		}
	}

	if((x = xmlnode_get_child(packet, "title")))
		title = xmlnode_get_data(x);

	if((x = xmlnode_get_child(packet, "instructions")))
		instructions = xmlnode_get_data(x);

	handle = purple_request_fields(js->gc, title, title, instructions, fields,
			_("OK"), G_CALLBACK(jabber_x_data_ok_cb),
			_("Cancel"), G_CALLBACK(jabber_x_data_cancel_cb), data);

	if(title)
		g_free(title);
	if(instructions)
		g_free(instructions);

	return handle;
}


