/**
 * @file request.c Request API
 * @ingroup core
 */

/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
#define _PURPLE_REQUEST_C_

#include "internal.h"

#include "notify.h"
#include "request.h"
#include "debug.h"

static PurpleRequestUiOps *request_ui_ops = NULL;
static GList *handles = NULL;

typedef struct
{
	PurpleRequestType type;
	void *handle;
	void *ui_handle;

} PurpleRequestInfo;

struct _PurpleRequestField
{
	PurpleRequestFieldType type;
	PurpleRequestFieldGroup *group;

	char *id;
	char *label;
	char *type_hint;

	gboolean visible;
	gboolean required;

	union
	{
		struct
		{
			gboolean multiline;
			gboolean masked;
			gboolean editable;
			char *default_value;
			char *value;

		} string;

		struct
		{
			int default_value;
			int value;
			int lower_bound;
			int upper_bound;
		} integer;

		struct
		{
			gboolean default_value;
			gboolean value;

		} boolean;

		struct
		{
			gpointer default_value;
			gpointer value;

			GList *elements;
			GDestroyNotify data_destroy;
		} choice;

		struct
		{
			GList *items;
			GList *icons;
			GHashTable *item_data;
			GList *selected;
			GHashTable *selected_table;

			gboolean multiple_selection;

		} list;

		struct
		{
			PurpleAccount *default_account;
			PurpleAccount *account;
			gboolean show_all;

			PurpleFilterAccountFunc filter_func;

		} account;

		struct
		{
			unsigned int scale_x;
			unsigned int scale_y;
			const char *buffer;
			gsize size;
		} image;

		struct
		{
			PurpleCertificate *cert;
		} certificate;

	} u;

	void *ui_data;
	char *tooltip;

	PurpleRequestFieldValidator validator;
	void *validator_data;
};

struct _PurpleRequestFields
{
	GList *groups;

	GHashTable *fields;

	GList *required_fields;

	GList *validated_fields;

	void *ui_data;
};

struct _PurpleRequestFieldGroup
{
	PurpleRequestFields *fields_list;

	char *title;

	GList *fields;
};

struct _PurpleRequestCommonParameters
{
	int ref_count;

	PurpleAccount *account;
	PurpleConversation *conv;

	PurpleRequestIconType icon_type;
	gconstpointer icon_data;
	gsize icon_size;

	gboolean html;

	gboolean compact;

	PurpleRequestHelpCb help_cb;
	gpointer help_data;
};

PurpleRequestCommonParameters *
purple_request_cpar_new(void)
{
	return g_new0(PurpleRequestCommonParameters, 1);
}

PurpleRequestCommonParameters *
purple_request_cpar_from_connection(PurpleConnection *gc)
{
	if (gc == NULL)
		return purple_request_cpar_new();
	return purple_request_cpar_from_account(
		purple_connection_get_account(gc));
}

PurpleRequestCommonParameters *
purple_request_cpar_from_account(PurpleAccount *account)
{
	PurpleRequestCommonParameters *cpar;

	cpar = purple_request_cpar_new();
	purple_request_cpar_set_account(cpar, account);

	return cpar;
}

PurpleRequestCommonParameters *
purple_request_cpar_from_conversation(PurpleConversation *conv)
{
	PurpleRequestCommonParameters *cpar;
	PurpleAccount *account = NULL;

	if (conv != NULL) {
		account = purple_connection_get_account(
			purple_conversation_get_connection(conv));
	}

	cpar = purple_request_cpar_new();
	purple_request_cpar_set_account(cpar, account);
	purple_request_cpar_set_conversation(cpar, conv);

	return cpar;
}

void
purple_request_cpar_ref(PurpleRequestCommonParameters *cpar)
{
	g_return_if_fail(cpar != NULL);

	cpar->ref_count++;
}

PurpleRequestCommonParameters *
purple_request_cpar_unref(PurpleRequestCommonParameters *cpar)
{
	if (cpar == NULL)
		return NULL;

	if (--cpar->ref_count > 0)
		return cpar;

	g_free(cpar);
	return NULL;
}

void
purple_request_cpar_set_account(PurpleRequestCommonParameters *cpar,
	PurpleAccount *account)
{
	g_return_if_fail(cpar != NULL);

	cpar->account = account;
}

PurpleAccount *
purple_request_cpar_get_account(PurpleRequestCommonParameters *cpar)
{
	if (cpar == NULL)
		return NULL;

	return cpar->account;
}

void
purple_request_cpar_set_conversation(PurpleRequestCommonParameters *cpar,
	PurpleConversation *conv)
{
	g_return_if_fail(cpar != NULL);

	cpar->conv = conv;
}

PurpleConversation *
purple_request_cpar_get_conversation(PurpleRequestCommonParameters *cpar)
{
	if (cpar == NULL)
		return NULL;

	return cpar->conv;
}

void
purple_request_cpar_set_icon(PurpleRequestCommonParameters *cpar,
	PurpleRequestIconType icon_type)
{
	g_return_if_fail(cpar != NULL);

	cpar->icon_type = icon_type;
}

PurpleRequestIconType
purple_request_cpar_get_icon(PurpleRequestCommonParameters *cpar)
{
	if (cpar == NULL)
		return PURPLE_REQUEST_ICON_DEFAULT;

	return cpar->icon_type;
}

void
purple_request_cpar_set_custom_icon(PurpleRequestCommonParameters *cpar,
	gconstpointer icon_data, gsize icon_size)
{
	g_return_if_fail(cpar != NULL);
	g_return_if_fail((icon_data == NULL) == (icon_size == 0));

	cpar->icon_data = icon_data;
	cpar->icon_size = icon_size;
}

gconstpointer
purple_request_cpar_get_custom_icon(PurpleRequestCommonParameters *cpar,
	gsize *icon_size)
{
	if (cpar == NULL) {
		if (icon_size != NULL)
			*icon_size = 0;
		return NULL;
	}

	if (icon_size != NULL)
		*icon_size = cpar->icon_size;
	return cpar->icon_data;
}

void
purple_request_cpar_set_html(PurpleRequestCommonParameters *cpar,
	gboolean enabled)
{
	g_return_if_fail(cpar != NULL);

	cpar->html = enabled;
}

gboolean
purple_request_cpar_is_html(PurpleRequestCommonParameters *cpar)
{
	if (cpar == NULL)
		return FALSE;

	return cpar->html;
}

void
purple_request_cpar_set_compact(PurpleRequestCommonParameters *cpar,
	gboolean compact)
{
	g_return_if_fail(cpar != NULL);

	cpar->compact = compact;
}

gboolean
purple_request_cpar_is_compact(PurpleRequestCommonParameters *cpar)
{
	if (cpar == NULL)
		return FALSE;

	return cpar->compact;
}

void
purple_request_cpar_set_help_cb(PurpleRequestCommonParameters *cpar,
	PurpleRequestHelpCb cb, gpointer user_data)
{
	g_return_if_fail(cpar != NULL);

	cpar->help_cb = cb;
	cpar->help_data = cb ? user_data : NULL;
}

PurpleRequestHelpCb
purple_request_cpar_get_help_cb(PurpleRequestCommonParameters *cpar,
	gpointer *user_data)
{
	if (cpar == NULL)
		return NULL;

	if (user_data != NULL)
		*user_data = cpar->help_data;
	return cpar->help_cb;
}

PurpleRequestFields *
purple_request_fields_new(void)
{
	PurpleRequestFields *fields;

	fields = g_new0(PurpleRequestFields, 1);

	fields->fields = g_hash_table_new_full(g_str_hash, g_str_equal,
										   g_free, NULL);

	return fields;
}

void
purple_request_fields_destroy(PurpleRequestFields *fields)
{
	g_return_if_fail(fields != NULL);

	g_list_foreach(fields->groups, (GFunc)purple_request_field_group_destroy, NULL);
	g_list_free(fields->groups);
	g_list_free(fields->required_fields);
	g_list_free(fields->validated_fields);
	g_hash_table_destroy(fields->fields);
	g_free(fields);
}

void
purple_request_fields_add_group(PurpleRequestFields *fields,
							  PurpleRequestFieldGroup *group)
{
	GList *l;
	PurpleRequestField *field;

	g_return_if_fail(fields != NULL);
	g_return_if_fail(group  != NULL);

	fields->groups = g_list_append(fields->groups, group);

	group->fields_list = fields;

	for (l = purple_request_field_group_get_fields(group);
		 l != NULL;
		 l = l->next) {

		field = l->data;

		g_hash_table_insert(fields->fields,
			g_strdup(purple_request_field_get_id(field)), field);

		if (purple_request_field_is_required(field)) {
			fields->required_fields =
				g_list_append(fields->required_fields, field);
		}

		if (purple_request_field_is_validatable(field)) {
			fields->validated_fields =
				g_list_append(fields->validated_fields, field);
		}

	}
}

GList *
purple_request_fields_get_groups(const PurpleRequestFields *fields)
{
	g_return_val_if_fail(fields != NULL, NULL);

	return fields->groups;
}

gboolean
purple_request_fields_exists(const PurpleRequestFields *fields, const char *id)
{
	g_return_val_if_fail(fields != NULL, FALSE);
	g_return_val_if_fail(id     != NULL, FALSE);

	return (g_hash_table_lookup(fields->fields, id) != NULL);
}

const GList *
purple_request_fields_get_required(const PurpleRequestFields *fields)
{
	g_return_val_if_fail(fields != NULL, NULL);

	return fields->required_fields;
}

const GList *
purple_request_fields_get_validatable(const PurpleRequestFields *fields)
{
	g_return_val_if_fail(fields != NULL, NULL);

	return fields->validated_fields;
}

gboolean
purple_request_fields_is_field_required(const PurpleRequestFields *fields,
									  const char *id)
{
	PurpleRequestField *field;

	g_return_val_if_fail(fields != NULL, FALSE);
	g_return_val_if_fail(id     != NULL, FALSE);

	if ((field = purple_request_fields_get_field(fields, id)) == NULL)
		return FALSE;

	return purple_request_field_is_required(field);
}

gpointer
purple_request_field_get_ui_data(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, NULL);

	return field->ui_data;
}

void
purple_request_field_set_ui_data(PurpleRequestField *field,
                                 gpointer ui_data)
{
	g_return_if_fail(field != NULL);

	field->ui_data = ui_data;
}

gboolean
purple_request_fields_all_required_filled(const PurpleRequestFields *fields)
{
	GList *l;

	g_return_val_if_fail(fields != NULL, FALSE);

	for (l = fields->required_fields; l != NULL; l = l->next)
	{
		PurpleRequestField *field = (PurpleRequestField *)l->data;

		if (!purple_request_field_is_filled(field))
			return FALSE;
	}

	return TRUE;
}

gboolean
purple_request_fields_all_valid(const PurpleRequestFields *fields)
{
	GList *l;

	g_return_val_if_fail(fields != NULL, FALSE);

	for (l = fields->validated_fields; l != NULL; l = l->next)
	{
		PurpleRequestField *field = (PurpleRequestField *)l->data;

		if (!purple_request_field_is_valid(field, NULL))
			return FALSE;
	}

	return TRUE;
}

PurpleRequestField *
purple_request_fields_get_field(const PurpleRequestFields *fields, const char *id)
{
	PurpleRequestField *field;

	g_return_val_if_fail(fields != NULL, NULL);
	g_return_val_if_fail(id     != NULL, NULL);

	field = g_hash_table_lookup(fields->fields, id);

	g_return_val_if_fail(field != NULL, NULL);

	return field;
}

const char *
purple_request_fields_get_string(const PurpleRequestFields *fields, const char *id)
{
	PurpleRequestField *field;

	g_return_val_if_fail(fields != NULL, NULL);
	g_return_val_if_fail(id     != NULL, NULL);

	if ((field = purple_request_fields_get_field(fields, id)) == NULL)
		return NULL;

	return purple_request_field_string_get_value(field);
}

int
purple_request_fields_get_integer(const PurpleRequestFields *fields,
								const char *id)
{
	PurpleRequestField *field;

	g_return_val_if_fail(fields != NULL, 0);
	g_return_val_if_fail(id     != NULL, 0);

	if ((field = purple_request_fields_get_field(fields, id)) == NULL)
		return 0;

	return purple_request_field_int_get_value(field);
}

gboolean
purple_request_fields_get_bool(const PurpleRequestFields *fields, const char *id)
{
	PurpleRequestField *field;

	g_return_val_if_fail(fields != NULL, FALSE);
	g_return_val_if_fail(id     != NULL, FALSE);

	if ((field = purple_request_fields_get_field(fields, id)) == NULL)
		return FALSE;

	return purple_request_field_bool_get_value(field);
}

gpointer
purple_request_fields_get_choice(const PurpleRequestFields *fields,
	const char *id)
{
	PurpleRequestField *field;

	g_return_val_if_fail(fields != NULL, NULL);
	g_return_val_if_fail(id != NULL, NULL);

	if ((field = purple_request_fields_get_field(fields, id)) == NULL)
		return NULL;

	return purple_request_field_choice_get_value(field);
}

PurpleAccount *
purple_request_fields_get_account(const PurpleRequestFields *fields,
								const char *id)
{
	PurpleRequestField *field;

	g_return_val_if_fail(fields != NULL, NULL);
	g_return_val_if_fail(id     != NULL, NULL);

	if ((field = purple_request_fields_get_field(fields, id)) == NULL)
		return NULL;

	return purple_request_field_account_get_value(field);
}

gpointer purple_request_fields_get_ui_data(const PurpleRequestFields *fields)
{
	g_return_val_if_fail(fields != NULL, NULL);

	return fields->ui_data;
}

void purple_request_fields_set_ui_data(PurpleRequestFields *fields, gpointer ui_data)
{
	g_return_if_fail(fields != NULL);

	fields->ui_data = ui_data;
}

PurpleRequestFieldGroup *
purple_request_field_group_new(const char *title)
{
	PurpleRequestFieldGroup *group;

	group = g_new0(PurpleRequestFieldGroup, 1);

	group->title = g_strdup(title);

	return group;
}

void
purple_request_field_group_destroy(PurpleRequestFieldGroup *group)
{
	g_return_if_fail(group != NULL);

	g_free(group->title);

	g_list_foreach(group->fields, (GFunc)purple_request_field_destroy, NULL);
	g_list_free(group->fields);

	g_free(group);
}

void
purple_request_field_group_add_field(PurpleRequestFieldGroup *group,
								   PurpleRequestField *field)
{
	g_return_if_fail(group != NULL);
	g_return_if_fail(field != NULL);

	group->fields = g_list_append(group->fields, field);

	if (group->fields_list != NULL)
	{
		g_hash_table_insert(group->fields_list->fields,
							g_strdup(purple_request_field_get_id(field)), field);

		if (purple_request_field_is_required(field))
		{
			group->fields_list->required_fields =
				g_list_append(group->fields_list->required_fields, field);
		}
		
		if (purple_request_field_is_validatable(field))
		{
			group->fields_list->validated_fields =
				g_list_append(group->fields_list->validated_fields, field);
		}
	}

	field->group = group;

}

const char *
purple_request_field_group_get_title(const PurpleRequestFieldGroup *group)
{
	g_return_val_if_fail(group != NULL, NULL);

	return group->title;
}

GList *
purple_request_field_group_get_fields(const PurpleRequestFieldGroup *group)
{
	g_return_val_if_fail(group != NULL, NULL);

	return group->fields;
}

PurpleRequestFields *
purple_request_field_group_get_fields_list(const PurpleRequestFieldGroup *group)
{
	g_return_val_if_fail(group != NULL, NULL);

	return group->fields_list;
}

PurpleRequestField *
purple_request_field_new(const char *id, const char *text,
					   PurpleRequestFieldType type)
{
	PurpleRequestField *field;

	g_return_val_if_fail(id   != NULL, NULL);
	g_return_val_if_fail(type != PURPLE_REQUEST_FIELD_NONE, NULL);

	field = g_new0(PurpleRequestField, 1);

	field->id   = g_strdup(id);
	field->type = type;

	purple_request_field_set_label(field, text);
	purple_request_field_set_visible(field, TRUE);

	return field;
}

void
purple_request_field_destroy(PurpleRequestField *field)
{
	g_return_if_fail(field != NULL);

	g_free(field->id);
	g_free(field->label);
	g_free(field->type_hint);
	g_free(field->tooltip);

	if (field->type == PURPLE_REQUEST_FIELD_STRING)
	{
		g_free(field->u.string.default_value);
		g_free(field->u.string.value);
	}
	else if (field->type == PURPLE_REQUEST_FIELD_CHOICE)
	{
		if (field->u.choice.elements != NULL)
		{
			GList *it = field->u.choice.elements;
			while (it != NULL) {
				g_free(it->data);
				it = g_list_next(it); /* value */
				if (it->data && field->u.choice.data_destroy)
					field->u.choice.data_destroy(it->data);
				if (it == NULL)
					break;
				it = g_list_next(it); /* next label */
			}
			g_list_free(field->u.choice.elements);
		}
	}
	else if (field->type == PURPLE_REQUEST_FIELD_LIST)
	{
		if (field->u.list.items != NULL)
		{
			g_list_foreach(field->u.list.items, (GFunc)g_free, NULL);
			g_list_free(field->u.list.items);
		}

		if (field->u.list.selected != NULL)
		{
			g_list_foreach(field->u.list.selected, (GFunc)g_free, NULL);
			g_list_free(field->u.list.selected);
		}

		g_hash_table_destroy(field->u.list.item_data);
		g_hash_table_destroy(field->u.list.selected_table);
	}

	g_free(field);
}

void
purple_request_field_set_label(PurpleRequestField *field, const char *label)
{
	g_return_if_fail(field != NULL);

	g_free(field->label);
	field->label = g_strdup(label);
}

void
purple_request_field_set_visible(PurpleRequestField *field, gboolean visible)
{
	g_return_if_fail(field != NULL);

	field->visible = visible;
}

void
purple_request_field_set_type_hint(PurpleRequestField *field,
								 const char *type_hint)
{
	g_return_if_fail(field != NULL);

	g_free(field->type_hint);
	field->type_hint = g_strdup(type_hint);
}

void
purple_request_field_set_tooltip(PurpleRequestField *field, const char *tooltip)
{
	g_return_if_fail(field != NULL);

	g_free(field->tooltip);
	field->tooltip = g_strdup(tooltip);
}

void
purple_request_field_set_required(PurpleRequestField *field, gboolean required)
{
	g_return_if_fail(field != NULL);

	if (field->required == required)
		return;

	field->required = required;

	if (field->group != NULL)
	{
		if (required)
		{
			field->group->fields_list->required_fields =
				g_list_append(field->group->fields_list->required_fields,
							  field);
		}
		else
		{
			field->group->fields_list->required_fields =
				g_list_remove(field->group->fields_list->required_fields,
							  field);
		}
	}
}

PurpleRequestFieldType
purple_request_field_get_type(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, PURPLE_REQUEST_FIELD_NONE);

	return field->type;
}

PurpleRequestFieldGroup *
purple_request_field_get_group(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, NULL);

	return field->group;
}

const char *
purple_request_field_get_id(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, NULL);

	return field->id;
}

const char *
purple_request_field_get_label(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, NULL);

	return field->label;
}

gboolean
purple_request_field_is_visible(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, FALSE);

	return field->visible;
}

const char *
purple_request_field_get_type_hint(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, NULL);

	return field->type_hint;
}

const char *
purple_request_field_get_tooltip(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, NULL);

	return field->tooltip;
}

gboolean
purple_request_field_is_required(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, FALSE);

	return field->required;
}

gboolean
purple_request_field_is_filled(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, FALSE);

	switch (purple_request_field_get_type(field))
	{
		case PURPLE_REQUEST_FIELD_STRING:
			return (purple_request_field_string_get_value(field) != NULL &&
				*(purple_request_field_string_get_value(field)) != '\0');
		default:
			return TRUE;
	}
}

void
purple_request_field_set_validator(PurpleRequestField *field,
	PurpleRequestFieldValidator validator, void *user_data)
{
	g_return_if_fail(field != NULL);

	field->validator = validator;
	field->validator_data = validator ? user_data : NULL;

	if (field->group != NULL)
	{
		PurpleRequestFields *flist = field->group->fields_list;
		flist->validated_fields = g_list_remove(flist->validated_fields,
			field);
		if (validator)
		{
			flist->validated_fields = g_list_append(
				flist->validated_fields, field);
		}
	}
}

gboolean
purple_request_field_is_validatable(PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, FALSE);

	return field->validator != NULL;
}

gboolean
purple_request_field_is_valid(PurpleRequestField *field, gchar **errmsg)
{
	gboolean valid;

	g_return_val_if_fail(field != NULL, FALSE);

	if (!field->validator)
		return TRUE;

	if (!purple_request_field_is_required(field) &&
		!purple_request_field_is_filled(field))
		return TRUE;

	valid = field->validator(field, errmsg, field->validator_data);

	if (valid && errmsg)
		*errmsg = NULL;

	return valid;
}

PurpleRequestField *
purple_request_field_string_new(const char *id, const char *text,
							  const char *default_value, gboolean multiline)
{
	PurpleRequestField *field;

	g_return_val_if_fail(id   != NULL, NULL);
	g_return_val_if_fail(text != NULL, NULL);

	field = purple_request_field_new(id, text, PURPLE_REQUEST_FIELD_STRING);

	field->u.string.multiline = multiline;
	field->u.string.editable  = TRUE;

	purple_request_field_string_set_default_value(field, default_value);
	purple_request_field_string_set_value(field, default_value);

	return field;
}

void
purple_request_field_string_set_default_value(PurpleRequestField *field,
											const char *default_value)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == PURPLE_REQUEST_FIELD_STRING);

	g_free(field->u.string.default_value);
	field->u.string.default_value = g_strdup(default_value);
}

void
purple_request_field_string_set_value(PurpleRequestField *field, const char *value)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == PURPLE_REQUEST_FIELD_STRING);

	g_free(field->u.string.value);
	field->u.string.value = g_strdup(value);
}

void
purple_request_field_string_set_masked(PurpleRequestField *field, gboolean masked)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == PURPLE_REQUEST_FIELD_STRING);

	field->u.string.masked = masked;
}

void
purple_request_field_string_set_editable(PurpleRequestField *field,
									   gboolean editable)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == PURPLE_REQUEST_FIELD_STRING);

	field->u.string.editable = editable;
}

const char *
purple_request_field_string_get_default_value(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, NULL);
	g_return_val_if_fail(field->type == PURPLE_REQUEST_FIELD_STRING, NULL);

	return field->u.string.default_value;
}

const char *
purple_request_field_string_get_value(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, NULL);
	g_return_val_if_fail(field->type == PURPLE_REQUEST_FIELD_STRING, NULL);

	return field->u.string.value;
}

gboolean
purple_request_field_string_is_multiline(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, FALSE);
	g_return_val_if_fail(field->type == PURPLE_REQUEST_FIELD_STRING, FALSE);

	return field->u.string.multiline;
}

gboolean
purple_request_field_string_is_masked(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, FALSE);
	g_return_val_if_fail(field->type == PURPLE_REQUEST_FIELD_STRING, FALSE);

	return field->u.string.masked;
}

gboolean
purple_request_field_string_is_editable(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, FALSE);
	g_return_val_if_fail(field->type == PURPLE_REQUEST_FIELD_STRING, FALSE);

	return field->u.string.editable;
}

PurpleRequestField *
purple_request_field_int_new(const char *id, const char *text,
	int default_value, int lower_bound, int upper_bound)
{
	PurpleRequestField *field;

	g_return_val_if_fail(id   != NULL, NULL);
	g_return_val_if_fail(text != NULL, NULL);

	field = purple_request_field_new(id, text, PURPLE_REQUEST_FIELD_INTEGER);

	purple_request_field_int_set_lower_bound(field, lower_bound);
	purple_request_field_int_set_upper_bound(field, upper_bound);
	purple_request_field_int_set_default_value(field, default_value);
	purple_request_field_int_set_value(field, default_value);

	return field;
}

void
purple_request_field_int_set_default_value(PurpleRequestField *field,
										 int default_value)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == PURPLE_REQUEST_FIELD_INTEGER);

	field->u.integer.default_value = default_value;
}

void
purple_request_field_int_set_lower_bound(PurpleRequestField *field,
	int lower_bound)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == PURPLE_REQUEST_FIELD_INTEGER);

	field->u.integer.lower_bound = lower_bound;
}

void
purple_request_field_int_set_upper_bound(PurpleRequestField *field,
	int upper_bound)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == PURPLE_REQUEST_FIELD_INTEGER);

	field->u.integer.upper_bound = upper_bound;
}

void
purple_request_field_int_set_value(PurpleRequestField *field, int value)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == PURPLE_REQUEST_FIELD_INTEGER);

	if (value < field->u.integer.lower_bound ||
		value > field->u.integer.upper_bound) {
		purple_debug_error("request", "Int value out of bounds\n");
		return;
	}

	field->u.integer.value = value;
}

int
purple_request_field_int_get_default_value(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, 0);
	g_return_val_if_fail(field->type == PURPLE_REQUEST_FIELD_INTEGER, 0);

	return field->u.integer.default_value;
}

int
purple_request_field_int_get_lower_bound(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, 0);
	g_return_val_if_fail(field->type == PURPLE_REQUEST_FIELD_INTEGER, 0);

	return field->u.integer.lower_bound;
}

int
purple_request_field_int_get_upper_bound(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, 0);
	g_return_val_if_fail(field->type == PURPLE_REQUEST_FIELD_INTEGER, 0);

	return field->u.integer.upper_bound;
}

int
purple_request_field_int_get_value(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, 0);
	g_return_val_if_fail(field->type == PURPLE_REQUEST_FIELD_INTEGER, 0);

	return field->u.integer.value;
}

PurpleRequestField *
purple_request_field_bool_new(const char *id, const char *text,
							gboolean default_value)
{
	PurpleRequestField *field;

	g_return_val_if_fail(id   != NULL, NULL);
	g_return_val_if_fail(text != NULL, NULL);

	field = purple_request_field_new(id, text, PURPLE_REQUEST_FIELD_BOOLEAN);

	purple_request_field_bool_set_default_value(field, default_value);
	purple_request_field_bool_set_value(field, default_value);

	return field;
}

void
purple_request_field_bool_set_default_value(PurpleRequestField *field,
										  gboolean default_value)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == PURPLE_REQUEST_FIELD_BOOLEAN);

	field->u.boolean.default_value = default_value;
}

void
purple_request_field_bool_set_value(PurpleRequestField *field, gboolean value)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == PURPLE_REQUEST_FIELD_BOOLEAN);

	field->u.boolean.value = value;
}

gboolean
purple_request_field_bool_get_default_value(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, FALSE);
	g_return_val_if_fail(field->type == PURPLE_REQUEST_FIELD_BOOLEAN, FALSE);

	return field->u.boolean.default_value;
}

gboolean
purple_request_field_bool_get_value(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, FALSE);
	g_return_val_if_fail(field->type == PURPLE_REQUEST_FIELD_BOOLEAN, FALSE);

	return field->u.boolean.value;
}

PurpleRequestField *
purple_request_field_choice_new(const char *id, const char *text,
	gpointer default_value)
{
	PurpleRequestField *field;

	g_return_val_if_fail(id   != NULL, NULL);
	g_return_val_if_fail(text != NULL, NULL);

	field = purple_request_field_new(id, text, PURPLE_REQUEST_FIELD_CHOICE);

	purple_request_field_choice_set_default_value(field, default_value);
	purple_request_field_choice_set_value(field, default_value);

	return field;
}

void
purple_request_field_choice_add(PurpleRequestField *field, const char *label,
	gpointer value)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(label != NULL);
	g_return_if_fail(field->type == PURPLE_REQUEST_FIELD_CHOICE);

	field->u.choice.elements = g_list_append(field->u.choice.elements,
		g_strdup(label));
	field->u.choice.elements = g_list_append(field->u.choice.elements,
		value);
}

void
purple_request_field_choice_set_default_value(PurpleRequestField *field,
	gpointer default_value)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == PURPLE_REQUEST_FIELD_CHOICE);

	field->u.choice.default_value = default_value;
}

void
purple_request_field_choice_set_value(PurpleRequestField *field, gpointer value)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == PURPLE_REQUEST_FIELD_CHOICE);

	field->u.choice.value = value;
}

gpointer
purple_request_field_choice_get_default_value(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, NULL);
	g_return_val_if_fail(field->type == PURPLE_REQUEST_FIELD_CHOICE, NULL);

	return field->u.choice.default_value;
}

gpointer
purple_request_field_choice_get_value(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, NULL);
	g_return_val_if_fail(field->type == PURPLE_REQUEST_FIELD_CHOICE, NULL);

	return field->u.choice.value;
}

GList *
purple_request_field_choice_get_elements(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, NULL);
	g_return_val_if_fail(field->type == PURPLE_REQUEST_FIELD_CHOICE, NULL);

	return field->u.choice.elements;
}

void
purple_request_field_choice_set_data_destructor(PurpleRequestField *field,
	GDestroyNotify destroy)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == PURPLE_REQUEST_FIELD_CHOICE);

	field->u.choice.data_destroy = destroy;
}

PurpleRequestField *
purple_request_field_list_new(const char *id, const char *text)
{
	PurpleRequestField *field;

	g_return_val_if_fail(id   != NULL, NULL);

	field = purple_request_field_new(id, text, PURPLE_REQUEST_FIELD_LIST);

	field->u.list.item_data = g_hash_table_new_full(g_str_hash, g_str_equal,
													g_free, NULL);

	field->u.list.selected_table =
		g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	return field;
}

void
purple_request_field_list_set_multi_select(PurpleRequestField *field,
										 gboolean multi_select)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == PURPLE_REQUEST_FIELD_LIST);

	field->u.list.multiple_selection = multi_select;
}

gboolean
purple_request_field_list_get_multi_select(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, FALSE);
	g_return_val_if_fail(field->type == PURPLE_REQUEST_FIELD_LIST, FALSE);

	return field->u.list.multiple_selection;
}

void *
purple_request_field_list_get_data(const PurpleRequestField *field,
								 const char *text)
{
	g_return_val_if_fail(field != NULL, NULL);
	g_return_val_if_fail(text  != NULL, NULL);
	g_return_val_if_fail(field->type == PURPLE_REQUEST_FIELD_LIST, NULL);

	return g_hash_table_lookup(field->u.list.item_data, text);
}

void
purple_request_field_list_add_icon(PurpleRequestField *field, const char *item, const char* icon_path,
							void *data)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(item  != NULL);
	g_return_if_fail(data  != NULL);
	g_return_if_fail(field->type == PURPLE_REQUEST_FIELD_LIST);

	if (icon_path)
	{
		if (field->u.list.icons == NULL)
		{
			GList *l;
			for (l = field->u.list.items ; l != NULL ; l = l->next)
			{
				/* Order doesn't matter, because we're just
				 * filing in blank items.  So, we use
				 * g_list_prepend() because it's faster. */
				field->u.list.icons = g_list_prepend(field->u.list.icons, NULL);
			}
		}
		field->u.list.icons = g_list_append(field->u.list.icons, g_strdup(icon_path));
	}
	else if (field->u.list.icons)
	{
		/* Keep this even with the items list. */
		field->u.list.icons = g_list_append(field->u.list.icons, NULL);
	}

	field->u.list.items = g_list_append(field->u.list.items, g_strdup(item));
	g_hash_table_insert(field->u.list.item_data, g_strdup(item), data);
}

void
purple_request_field_list_add_selected(PurpleRequestField *field, const char *item)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(item  != NULL);
	g_return_if_fail(field->type == PURPLE_REQUEST_FIELD_LIST);

	if (!purple_request_field_list_get_multi_select(field) &&
		field->u.list.selected != NULL)
	{
		purple_debug_warning("request",
						   "More than one item added to non-multi-select "
						   "field %s\n",
						   purple_request_field_get_id(field));
		return;
	}

	field->u.list.selected = g_list_append(field->u.list.selected,
										   g_strdup(item));

	g_hash_table_insert(field->u.list.selected_table, g_strdup(item), NULL);
}

void
purple_request_field_list_clear_selected(PurpleRequestField *field)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == PURPLE_REQUEST_FIELD_LIST);

	if (field->u.list.selected != NULL)
	{
		g_list_foreach(field->u.list.selected, (GFunc)g_free, NULL);
		g_list_free(field->u.list.selected);
		field->u.list.selected = NULL;
	}

	g_hash_table_destroy(field->u.list.selected_table);

	field->u.list.selected_table =
		g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
}

void
purple_request_field_list_set_selected(PurpleRequestField *field, GList *items)
{
	GList *l;

	g_return_if_fail(field != NULL);
	g_return_if_fail(items != NULL);
	g_return_if_fail(field->type == PURPLE_REQUEST_FIELD_LIST);

	purple_request_field_list_clear_selected(field);

	if (!purple_request_field_list_get_multi_select(field) &&
		items && items->next)
	{
		purple_debug_warning("request",
						   "More than one item added to non-multi-select "
						   "field %s\n",
						   purple_request_field_get_id(field));
		return;
	}

	for (l = items; l != NULL; l = l->next)
	{
		field->u.list.selected = g_list_append(field->u.list.selected,
					g_strdup(l->data));
		g_hash_table_insert(field->u.list.selected_table,
							g_strdup((char *)l->data), NULL);
	}
}

gboolean
purple_request_field_list_is_selected(const PurpleRequestField *field,
									const char *item)
{
	g_return_val_if_fail(field != NULL, FALSE);
	g_return_val_if_fail(item  != NULL, FALSE);
	g_return_val_if_fail(field->type == PURPLE_REQUEST_FIELD_LIST, FALSE);

	return g_hash_table_lookup_extended(field->u.list.selected_table,
										item, NULL, NULL);
}

GList *
purple_request_field_list_get_selected(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, NULL);
	g_return_val_if_fail(field->type == PURPLE_REQUEST_FIELD_LIST, NULL);

	return field->u.list.selected;
}

GList *
purple_request_field_list_get_items(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, NULL);
	g_return_val_if_fail(field->type == PURPLE_REQUEST_FIELD_LIST, NULL);

	return field->u.list.items;
}

GList *
purple_request_field_list_get_icons(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, NULL);
	g_return_val_if_fail(field->type == PURPLE_REQUEST_FIELD_LIST, NULL);

	return field->u.list.icons;
}

PurpleRequestField *
purple_request_field_label_new(const char *id, const char *text)
{
	PurpleRequestField *field;

	g_return_val_if_fail(id   != NULL, NULL);
	g_return_val_if_fail(text != NULL, NULL);

	field = purple_request_field_new(id, text, PURPLE_REQUEST_FIELD_LABEL);

	return field;
}

PurpleRequestField *
purple_request_field_image_new(const char *id, const char *text, const char *buf, gsize size)
{
	PurpleRequestField *field;

	g_return_val_if_fail(id   != NULL, NULL);
	g_return_val_if_fail(text != NULL, NULL);
	g_return_val_if_fail(buf  != NULL, NULL);
	g_return_val_if_fail(size > 0, NULL);

	field = purple_request_field_new(id, text, PURPLE_REQUEST_FIELD_IMAGE);

	field->u.image.buffer  = g_memdup(buf, size);
	field->u.image.size    = size;
	field->u.image.scale_x = 1;
	field->u.image.scale_y = 1;

	return field;
}

void
purple_request_field_image_set_scale(PurpleRequestField *field, unsigned int x, unsigned int y)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == PURPLE_REQUEST_FIELD_IMAGE);

	field->u.image.scale_x = x;
	field->u.image.scale_y = y;
}

const char *
purple_request_field_image_get_buffer(PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, NULL);
	g_return_val_if_fail(field->type == PURPLE_REQUEST_FIELD_IMAGE, NULL);

	return field->u.image.buffer;
}

gsize
purple_request_field_image_get_size(PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, 0);
	g_return_val_if_fail(field->type == PURPLE_REQUEST_FIELD_IMAGE, 0);

	return field->u.image.size;
}

unsigned int
purple_request_field_image_get_scale_x(PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, 0);
	g_return_val_if_fail(field->type == PURPLE_REQUEST_FIELD_IMAGE, 0);

	return field->u.image.scale_x;
}

unsigned int
purple_request_field_image_get_scale_y(PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, 0);
	g_return_val_if_fail(field->type == PURPLE_REQUEST_FIELD_IMAGE, 0);

	return field->u.image.scale_y;
}

PurpleRequestField *
purple_request_field_account_new(const char *id, const char *text,
							   PurpleAccount *account)
{
	PurpleRequestField *field;

	g_return_val_if_fail(id   != NULL, NULL);
	g_return_val_if_fail(text != NULL, NULL);

	field = purple_request_field_new(id, text, PURPLE_REQUEST_FIELD_ACCOUNT);

	if (account == NULL && purple_connections_get_all() != NULL)
	{
		account = purple_connection_get_account(
			(PurpleConnection *)purple_connections_get_all()->data);
	}

	purple_request_field_account_set_default_value(field, account);
	purple_request_field_account_set_value(field, account);

	return field;
}

void
purple_request_field_account_set_default_value(PurpleRequestField *field,
											 PurpleAccount *default_value)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == PURPLE_REQUEST_FIELD_ACCOUNT);

	field->u.account.default_account = default_value;
}

void
purple_request_field_account_set_value(PurpleRequestField *field,
									 PurpleAccount *value)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == PURPLE_REQUEST_FIELD_ACCOUNT);

	field->u.account.account = value;
}

void
purple_request_field_account_set_show_all(PurpleRequestField *field,
										gboolean show_all)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == PURPLE_REQUEST_FIELD_ACCOUNT);

	if (field->u.account.show_all == show_all)
		return;

	field->u.account.show_all = show_all;

	if (!show_all)
	{
		if (purple_account_is_connected(field->u.account.default_account))
		{
			purple_request_field_account_set_default_value(field,
				(PurpleAccount *)purple_connections_get_all()->data);
		}

		if (purple_account_is_connected(field->u.account.account))
		{
			purple_request_field_account_set_value(field,
				(PurpleAccount *)purple_connections_get_all()->data);
		}
	}
}

void
purple_request_field_account_set_filter(PurpleRequestField *field,
									  PurpleFilterAccountFunc filter_func)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == PURPLE_REQUEST_FIELD_ACCOUNT);

	field->u.account.filter_func = filter_func;
}

PurpleAccount *
purple_request_field_account_get_default_value(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, NULL);
	g_return_val_if_fail(field->type == PURPLE_REQUEST_FIELD_ACCOUNT, NULL);

	return field->u.account.default_account;
}

PurpleAccount *
purple_request_field_account_get_value(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, NULL);
	g_return_val_if_fail(field->type == PURPLE_REQUEST_FIELD_ACCOUNT, NULL);

	return field->u.account.account;
}

gboolean
purple_request_field_account_get_show_all(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, FALSE);
	g_return_val_if_fail(field->type == PURPLE_REQUEST_FIELD_ACCOUNT, FALSE);

	return field->u.account.show_all;
}

PurpleFilterAccountFunc
purple_request_field_account_get_filter(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, FALSE);
	g_return_val_if_fail(field->type == PURPLE_REQUEST_FIELD_ACCOUNT, FALSE);

	return field->u.account.filter_func;
}

PurpleRequestField *
purple_request_field_certificate_new(const char *id, const char *text, PurpleCertificate *cert)
{
	PurpleRequestField *field;

	g_return_val_if_fail(id   != NULL, NULL);
	g_return_val_if_fail(text != NULL, NULL);
	g_return_val_if_fail(cert != NULL, NULL);

	field = purple_request_field_new(id, text, PURPLE_REQUEST_FIELD_CERTIFICATE);

	field->u.certificate.cert = cert;

	return field;
}

PurpleCertificate *
purple_request_field_certificate_get_value(const PurpleRequestField *field)
{
	g_return_val_if_fail(field != NULL, NULL);
	g_return_val_if_fail(field->type == PURPLE_REQUEST_FIELD_CERTIFICATE, NULL);

	return field->u.certificate.cert;
}

/* -- */

gboolean
purple_request_field_email_validator(PurpleRequestField *field, gchar **errmsg,
	void *user_data)
{
	const char *value;

	g_return_val_if_fail(field != NULL, FALSE);
	g_return_val_if_fail(field->type == PURPLE_REQUEST_FIELD_STRING, FALSE);

	value = purple_request_field_string_get_value(field);

	if (value != NULL && purple_email_is_valid(value))
		return TRUE;

	if (errmsg)
		*errmsg = g_strdup(_("Invalid email address"));
	return FALSE;
}

gboolean
purple_request_field_alphanumeric_validator(PurpleRequestField *field,
	gchar **errmsg, void *allowed_characters)
{
	const char *value;
	gchar invalid_char = '\0';

	g_return_val_if_fail(field != NULL, FALSE);
	g_return_val_if_fail(field->type == PURPLE_REQUEST_FIELD_STRING, FALSE);

	value = purple_request_field_string_get_value(field);

	g_return_val_if_fail(value != NULL, FALSE);

	if (allowed_characters)
	{
		gchar *value_r = g_strdup(value);
		g_strcanon(value_r, allowed_characters, '\0');
		invalid_char = value[strlen(value_r)];
		g_free(value_r);
	}
	else
	{
		while (value)
		{
			if (!g_ascii_isalnum(*value))
			{
				invalid_char = *value;
				break;
			}
			value++;
		}
	}
	if (!invalid_char)
		return TRUE;

	if (errmsg)
		*errmsg = g_strdup_printf(_("Invalid character '%c'"),
			invalid_char);
	return FALSE;
}

/* -- */

static gchar *
purple_request_strip_html_custom(const gchar *html)
{
	gchar *tmp, *ret;

	tmp = purple_strreplace(html, "\n", "<br>");
	ret = purple_markup_strip_html(tmp);
	g_free(tmp);

	return ret;
}

static gchar **
purple_request_strip_html(PurpleRequestCommonParameters *cpar,
	const char **primary, const char **secondary)
{
	PurpleRequestUiOps *ops = purple_request_get_ui_ops();
	gchar **ret;

	if (!purple_request_cpar_is_html(cpar))
		return NULL;
	if (ops->features & PURPLE_REQUEST_FEATURE_HTML)
		return NULL;

	ret = g_new0(gchar*, 3);
	*primary = ret[0] = purple_request_strip_html_custom(*primary);
	*secondary = ret[1] = purple_request_strip_html_custom(*secondary);

	return ret;
}

void *
purple_request_input(void *handle, const char *title, const char *primary,
				   const char *secondary, const char *default_value,
				   gboolean multiline, gboolean masked, gchar *hint,
				   const char *ok_text, GCallback ok_cb,
				   const char *cancel_text, GCallback cancel_cb,
				   PurpleRequestCommonParameters *cpar,
				   void *user_data)
{
	PurpleRequestUiOps *ops;

	if (G_UNLIKELY(ok_text == NULL || ok_cb == NULL)) {
		purple_request_cpar_unref(cpar);
		g_warn_if_fail(ok_text != NULL);
		g_warn_if_fail(ok_cb != NULL);
		g_return_val_if_reached(NULL);
	}

	ops = purple_request_get_ui_ops();

	if (ops != NULL && ops->request_input != NULL) {
		PurpleRequestInfo *info;
		gchar **tmp;

		tmp = purple_request_strip_html(cpar, &primary, &secondary);

		info            = g_new0(PurpleRequestInfo, 1);
		info->type      = PURPLE_REQUEST_INPUT;
		info->handle    = handle;
		info->ui_handle = ops->request_input(title, primary, secondary,
			default_value, multiline, masked, hint, ok_text, ok_cb,
			cancel_text, cancel_cb, cpar, user_data);

		handles = g_list_append(handles, info);

		g_strfreev(tmp);
		purple_request_cpar_unref(cpar);
		return info->ui_handle;
	}

	purple_request_cpar_unref(cpar);
	return NULL;
}

void *
purple_request_choice(void *handle, const char *title, const char *primary,
	const char *secondary, gpointer default_value, const char *ok_text,
	GCallback ok_cb, const char *cancel_text, GCallback cancel_cb,
	PurpleRequestCommonParameters *cpar, void *user_data, ...)
{
	void *ui_handle;
	va_list args;

	if (G_UNLIKELY(ok_text == NULL || ok_cb == NULL)) {
		purple_request_cpar_unref(cpar);
		g_warn_if_fail(ok_text != NULL);
		g_warn_if_fail(ok_cb != NULL);
		g_return_val_if_reached(NULL);
	}

	va_start(args, user_data);
	ui_handle = purple_request_choice_varg(handle, title, primary, secondary,
					     default_value, ok_text, ok_cb,
					     cancel_text, cancel_cb,
					     cpar, user_data, args);
	va_end(args);

	return ui_handle;
}

void *
purple_request_choice_varg(void *handle, const char *title, const char *primary,
	const char *secondary, gpointer default_value, const char *ok_text,
	GCallback ok_cb, const char *cancel_text, GCallback cancel_cb,
	PurpleRequestCommonParameters *cpar, void *user_data, va_list choices)
{
	PurpleRequestUiOps *ops;

	if (G_UNLIKELY(ok_text == NULL || ok_cb == NULL ||
		cancel_text == NULL))
	{
		purple_request_cpar_unref(cpar);
		g_warn_if_fail(ok_text != NULL);
		g_warn_if_fail(ok_cb != NULL);
		g_warn_if_fail(cancel_text != NULL);
		g_return_val_if_reached(NULL);
	}

	ops = purple_request_get_ui_ops();

	if (ops != NULL && ops->request_choice != NULL) {
		PurpleRequestInfo *info;
		gchar **tmp;

		tmp = purple_request_strip_html(cpar, &primary, &secondary);

		info            = g_new0(PurpleRequestInfo, 1);
		info->type      = PURPLE_REQUEST_CHOICE;
		info->handle    = handle;
		info->ui_handle = ops->request_choice(title, primary, secondary,
			default_value, ok_text, ok_cb, cancel_text, cancel_cb,
			cpar, user_data, choices);

		handles = g_list_append(handles, info);

		g_strfreev(tmp);
		purple_request_cpar_unref(cpar);
		return info->ui_handle;
	}

	purple_request_cpar_unref(cpar);
	return NULL;
}

void *
purple_request_action(void *handle, const char *title, const char *primary,
	const char *secondary, int default_action,
	PurpleRequestCommonParameters *cpar, void *user_data,
	size_t action_count, ...)
{
	void *ui_handle;
	va_list args;

	va_start(args, action_count);
	ui_handle = purple_request_action_varg(handle, title, primary,
		secondary, default_action, cpar, user_data, action_count, args);
	va_end(args);

	return ui_handle;
}

void *
purple_request_action_varg(void *handle, const char *title, const char *primary,
	const char *secondary, int default_action,
	PurpleRequestCommonParameters *cpar, void *user_data,
	size_t action_count, va_list actions)
{
	PurpleRequestUiOps *ops;

	ops = purple_request_get_ui_ops();

	if (ops != NULL && ops->request_action != NULL) {
		PurpleRequestInfo *info;
		gchar **tmp;

		tmp = purple_request_strip_html(cpar, &primary, &secondary);

		info            = g_new0(PurpleRequestInfo, 1);
		info->type      = PURPLE_REQUEST_ACTION;
		info->handle    = handle;
		info->ui_handle = ops->request_action(title, primary, secondary,
			default_action, cpar, user_data, action_count, actions);

		handles = g_list_append(handles, info);

		g_strfreev(tmp);
		purple_request_cpar_unref(cpar);
		return info->ui_handle;
	}

	purple_request_cpar_unref(cpar);
	return NULL;
}

void *
purple_request_wait(void *handle, const char *title, const char *primary,
	const char *secondary, GCallback cancel_cb,
	PurpleRequestCommonParameters *cpar, void *user_data)
{
	PurpleRequestUiOps *ops;

	if (title == NULL)
		title = _("Please wait");
	if (primary == NULL)
		primary = _("Please wait...");

	ops = purple_request_get_ui_ops();

	if (ops != NULL && ops->request_wait != NULL) {
		PurpleRequestInfo *info;
		gchar **tmp;

		tmp = purple_request_strip_html(cpar, &primary, &secondary);

		info            = g_new0(PurpleRequestInfo, 1);
		info->type      = PURPLE_REQUEST_WAIT;
		info->handle    = handle;
		info->ui_handle = ops->request_wait(title, primary, secondary,
			cancel_cb, cpar, user_data);

		handles = g_list_append(handles, info);

		g_strfreev(tmp);
		purple_request_cpar_unref(cpar);
		return info->ui_handle;
	}

	if (cpar == NULL)
		cpar = purple_request_cpar_new();
	if (purple_request_cpar_get_icon(cpar) == PURPLE_REQUEST_ICON_DEFAULT)
		purple_request_cpar_set_icon(cpar, PURPLE_REQUEST_ICON_WAIT);

	return purple_request_action(handle, title, primary, secondary,
		PURPLE_DEFAULT_ACTION_NONE, cpar, user_data,
		cancel_cb ? 1 : 0, _("Cancel"), cancel_cb);
}

void *
purple_request_fields(void *handle, const char *title, const char *primary,
	const char *secondary, PurpleRequestFields *fields, const char *ok_text,
	GCallback ok_cb, const char *cancel_text, GCallback cancel_cb,
	PurpleRequestCommonParameters *cpar, void *user_data)
{
	PurpleRequestUiOps *ops;

	if (G_UNLIKELY(fields == NULL || ok_text == NULL || ok_cb == NULL ||
		cancel_text == NULL))
	{
		purple_request_cpar_unref(cpar);
		g_warn_if_fail(fields != NULL);
		g_warn_if_fail(ok_text != NULL);
		g_warn_if_fail(ok_cb != NULL);
		g_warn_if_fail(cancel_text != NULL);
		g_return_val_if_reached(NULL);
	}

	ops = purple_request_get_ui_ops();

	if (ops != NULL && ops->request_fields != NULL) {
		PurpleRequestInfo *info;
		gchar **tmp;

		tmp = purple_request_strip_html(cpar, &primary, &secondary);

		info            = g_new0(PurpleRequestInfo, 1);
		info->type      = PURPLE_REQUEST_FIELDS;
		info->handle    = handle;
		info->ui_handle = ops->request_fields(title, primary, secondary,
			fields, ok_text, ok_cb, cancel_text, cancel_cb,
			cpar, user_data);

		handles = g_list_append(handles, info);

		g_strfreev(tmp);
		purple_request_cpar_unref(cpar);
		return info->ui_handle;
	}

	purple_request_cpar_unref(cpar);
	return NULL;
}

void *
purple_request_file(void *handle, const char *title, const char *filename,
	gboolean savedialog, GCallback ok_cb, GCallback cancel_cb,
	PurpleRequestCommonParameters *cpar, void *user_data)
{
	PurpleRequestUiOps *ops;

	ops = purple_request_get_ui_ops();

	if (ops != NULL && ops->request_file != NULL) {
		PurpleRequestInfo *info;

		info            = g_new0(PurpleRequestInfo, 1);
		info->type      = PURPLE_REQUEST_FILE;
		info->handle    = handle;
		info->ui_handle = ops->request_file(title, filename, savedialog,
			ok_cb, cancel_cb, cpar, user_data);
		handles = g_list_append(handles, info);

		purple_request_cpar_unref(cpar);
		return info->ui_handle;
	}

	purple_request_cpar_unref(cpar);
	return NULL;
}

void *
purple_request_folder(void *handle, const char *title, const char *dirname,
	GCallback ok_cb, GCallback cancel_cb,
	PurpleRequestCommonParameters *cpar, void *user_data)
{
	PurpleRequestUiOps *ops;

	ops = purple_request_get_ui_ops();

	if (ops != NULL && ops->request_file != NULL) {
		PurpleRequestInfo *info;

		info            = g_new0(PurpleRequestInfo, 1);
		info->type      = PURPLE_REQUEST_FOLDER;
		info->handle    = handle;
		info->ui_handle = ops->request_folder(title, dirname, ok_cb,
			cancel_cb, cpar, user_data);
		handles = g_list_append(handles, info);

		purple_request_cpar_unref(cpar);
		return info->ui_handle;
	}

	purple_request_cpar_unref(cpar);
	return NULL;
}

void *
purple_request_certificate(void *handle, const char *title,
                                  const char *primary, const char *secondary,
                                  PurpleCertificate *cert,
                                  const char *ok_text, GCallback ok_cb,
                                  const char *cancel_text, GCallback cancel_cb,
                                  void *user_data)
{
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;

	fields = purple_request_fields_new();
	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);
	field = purple_request_field_certificate_new("certificate", "Certificate", cert);
	purple_request_field_group_add_field(group, field);

	return purple_request_fields(handle, title, primary, secondary, fields,
	                             ok_text, ok_cb, cancel_text, cancel_cb,
	                             NULL, user_data);
}

static void
purple_request_close_info(PurpleRequestInfo *info)
{
	PurpleRequestUiOps *ops;

	ops = purple_request_get_ui_ops();

	purple_notify_close_with_handle(info->ui_handle);
	purple_request_close_with_handle(info->ui_handle);

	if (ops != NULL && ops->close_request != NULL)
		ops->close_request(info->type, info->ui_handle);

	g_free(info);
}

void
purple_request_close(PurpleRequestType type, void *ui_handle)
{
	GList *l;

	g_return_if_fail(ui_handle != NULL);

	for (l = handles; l != NULL; l = l->next) {
		PurpleRequestInfo *info = l->data;

		if (info->ui_handle == ui_handle) {
			handles = g_list_remove(handles, info);
			purple_request_close_info(info);
			break;
		}
	}
}

void
purple_request_close_with_handle(void *handle)
{
	GList *l, *l_next;

	g_return_if_fail(handle != NULL);

	for (l = handles; l != NULL; l = l_next) {
		PurpleRequestInfo *info = l->data;

		l_next = l->next;

		if (info->handle == handle) {
			handles = g_list_remove(handles, info);
			purple_request_close_info(info);
		}
	}
}

void
purple_request_set_ui_ops(PurpleRequestUiOps *ops)
{
	request_ui_ops = ops;
}

PurpleRequestUiOps *
purple_request_get_ui_ops(void)
{
	return request_ui_ops;
}
