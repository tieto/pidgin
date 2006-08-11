/**
 * @file request.c Request API
 * @ingroup core
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "notify.h"
#include "request.h"
#include "debug.h"

static GaimRequestUiOps *request_ui_ops = NULL;
static GList *handles = NULL;

typedef struct
{
	GaimRequestType type;
	void *handle;
	void *ui_handle;

} GaimRequestInfo;


GaimRequestFields *
gaim_request_fields_new(void)
{
	GaimRequestFields *fields;

	fields = g_new0(GaimRequestFields, 1);

	fields->fields = g_hash_table_new_full(g_str_hash, g_str_equal,
										   g_free, NULL);

	return fields;
}

void
gaim_request_fields_destroy(GaimRequestFields *fields)
{
	g_return_if_fail(fields != NULL);

	g_list_foreach(fields->groups, (GFunc)gaim_request_field_group_destroy, NULL);
	g_list_free(fields->groups);
	g_list_free(fields->required_fields);
	g_hash_table_destroy(fields->fields);
	g_free(fields);
}

void
gaim_request_fields_add_group(GaimRequestFields *fields,
							  GaimRequestFieldGroup *group)
{
	GList *l;
	GaimRequestField *field;

	g_return_if_fail(fields != NULL);
	g_return_if_fail(group  != NULL);

	fields->groups = g_list_append(fields->groups, group);

	group->fields_list = fields;

	for (l = gaim_request_field_group_get_fields(group);
		 l != NULL;
		 l = l->next) {

		field = l->data;

		g_hash_table_insert(fields->fields,
			g_strdup(gaim_request_field_get_id(field)), field);

		if (gaim_request_field_is_required(field)) {
			fields->required_fields =
				g_list_append(fields->required_fields, field);
		}

	}
}

GList *
gaim_request_fields_get_groups(const GaimRequestFields *fields)
{
	g_return_val_if_fail(fields != NULL, NULL);

	return fields->groups;
}

gboolean
gaim_request_fields_exists(const GaimRequestFields *fields, const char *id)
{
	g_return_val_if_fail(fields != NULL, FALSE);
	g_return_val_if_fail(id     != NULL, FALSE);

	return (g_hash_table_lookup(fields->fields, id) != NULL);
}

const GList *
gaim_request_fields_get_required(const GaimRequestFields *fields)
{
	g_return_val_if_fail(fields != NULL, NULL);

	return fields->required_fields;
}

gboolean
gaim_request_fields_is_field_required(const GaimRequestFields *fields,
									  const char *id)
{
	GaimRequestField *field;

	g_return_val_if_fail(fields != NULL, FALSE);
	g_return_val_if_fail(id     != NULL, FALSE);

	if ((field = gaim_request_fields_get_field(fields, id)) == NULL)
		return FALSE;

	return gaim_request_field_is_required(field);
}

gboolean
gaim_request_fields_all_required_filled(const GaimRequestFields *fields)
{
	GList *l;

	g_return_val_if_fail(fields != NULL, FALSE);

	for (l = fields->required_fields; l != NULL; l = l->next)
	{
		GaimRequestField *field = (GaimRequestField *)l->data;

		switch (gaim_request_field_get_type(field))
		{
			case GAIM_REQUEST_FIELD_STRING:
				if (gaim_request_field_string_get_value(field) == NULL)
					return FALSE;

				break;

			default:
				break;
		}
	}

	return TRUE;
}

GaimRequestField *
gaim_request_fields_get_field(const GaimRequestFields *fields, const char *id)
{
	GaimRequestField *field;

	g_return_val_if_fail(fields != NULL, NULL);
	g_return_val_if_fail(id     != NULL, NULL);

	field = g_hash_table_lookup(fields->fields, id);

	g_return_val_if_fail(field != NULL, NULL);

	return field;
}

const char *
gaim_request_fields_get_string(const GaimRequestFields *fields, const char *id)
{
	GaimRequestField *field;

	g_return_val_if_fail(fields != NULL, NULL);
	g_return_val_if_fail(id     != NULL, NULL);

	if ((field = gaim_request_fields_get_field(fields, id)) == NULL)
		return NULL;

	return gaim_request_field_string_get_value(field);
}

int
gaim_request_fields_get_integer(const GaimRequestFields *fields,
								const char *id)
{
	GaimRequestField *field;

	g_return_val_if_fail(fields != NULL, 0);
	g_return_val_if_fail(id     != NULL, 0);

	if ((field = gaim_request_fields_get_field(fields, id)) == NULL)
		return 0;

	return gaim_request_field_int_get_value(field);
}

gboolean
gaim_request_fields_get_bool(const GaimRequestFields *fields, const char *id)
{
	GaimRequestField *field;

	g_return_val_if_fail(fields != NULL, FALSE);
	g_return_val_if_fail(id     != NULL, FALSE);

	if ((field = gaim_request_fields_get_field(fields, id)) == NULL)
		return FALSE;

	return gaim_request_field_bool_get_value(field);
}

int
gaim_request_fields_get_choice(const GaimRequestFields *fields, const char *id)
{
	GaimRequestField *field;

	g_return_val_if_fail(fields != NULL, -1);
	g_return_val_if_fail(id     != NULL, -1);

	if ((field = gaim_request_fields_get_field(fields, id)) == NULL)
		return -1;

	return gaim_request_field_choice_get_value(field);
}

GaimAccount *
gaim_request_fields_get_account(const GaimRequestFields *fields,
								const char *id)
{
	GaimRequestField *field;

	g_return_val_if_fail(fields != NULL, NULL);
	g_return_val_if_fail(id     != NULL, NULL);

	if ((field = gaim_request_fields_get_field(fields, id)) == NULL)
		return NULL;

	return gaim_request_field_account_get_value(field);
}

GaimRequestFieldGroup *
gaim_request_field_group_new(const char *title)
{
	GaimRequestFieldGroup *group;

	group = g_new0(GaimRequestFieldGroup, 1);

	group->title = g_strdup(title);

	return group;
}

void
gaim_request_field_group_destroy(GaimRequestFieldGroup *group)
{
	g_return_if_fail(group != NULL);

	g_free(group->title);

	g_list_foreach(group->fields, (GFunc)gaim_request_field_destroy, NULL);
	g_list_free(group->fields);

	g_free(group);
}

void
gaim_request_field_group_add_field(GaimRequestFieldGroup *group,
								   GaimRequestField *field)
{
	g_return_if_fail(group != NULL);
	g_return_if_fail(field != NULL);

	group->fields = g_list_append(group->fields, field);

	if (group->fields_list != NULL)
	{
		g_hash_table_insert(group->fields_list->fields,
							g_strdup(gaim_request_field_get_id(field)), field);

		if (gaim_request_field_is_required(field))
		{
			group->fields_list->required_fields =
				g_list_append(group->fields_list->required_fields, field);
		}
	}

	field->group = group;

}

const char *
gaim_request_field_group_get_title(const GaimRequestFieldGroup *group)
{
	g_return_val_if_fail(group != NULL, NULL);

	return group->title;
}

GList *
gaim_request_field_group_get_fields(const GaimRequestFieldGroup *group)
{
	g_return_val_if_fail(group != NULL, NULL);

	return group->fields;
}

GaimRequestField *
gaim_request_field_new(const char *id, const char *text,
					   GaimRequestFieldType type)
{
	GaimRequestField *field;

	g_return_val_if_fail(id   != NULL, NULL);
	g_return_val_if_fail(type != GAIM_REQUEST_FIELD_NONE, NULL);

	field = g_new0(GaimRequestField, 1);

	field->id   = g_strdup(id);
	field->type = type;

	gaim_request_field_set_label(field, text);
	gaim_request_field_set_visible(field, TRUE);

	return field;
}

void
gaim_request_field_destroy(GaimRequestField *field)
{
	g_return_if_fail(field != NULL);

	g_free(field->id);
	g_free(field->label);
	g_free(field->type_hint);

	if (field->type == GAIM_REQUEST_FIELD_STRING)
	{
		g_free(field->u.string.default_value);
		g_free(field->u.string.value);
	}
	else if (field->type == GAIM_REQUEST_FIELD_CHOICE)
	{
		if (field->u.choice.labels != NULL)
		{
			g_list_foreach(field->u.choice.labels, (GFunc)g_free, NULL);
			g_list_free(field->u.choice.labels);
		}
	}
	else if (field->type == GAIM_REQUEST_FIELD_LIST)
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
gaim_request_field_set_label(GaimRequestField *field, const char *label)
{
	g_return_if_fail(field != NULL);

	g_free(field->label);
	field->label = g_strdup(label);
}

void
gaim_request_field_set_visible(GaimRequestField *field, gboolean visible)
{
	g_return_if_fail(field != NULL);

	field->visible = visible;
}

void
gaim_request_field_set_type_hint(GaimRequestField *field,
								 const char *type_hint)
{
	g_return_if_fail(field != NULL);

	g_free(field->type_hint);
	field->type_hint = g_strdup(type_hint);
}

void
gaim_request_field_set_required(GaimRequestField *field, gboolean required)
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

GaimRequestFieldType
gaim_request_field_get_type(const GaimRequestField *field)
{
	g_return_val_if_fail(field != NULL, GAIM_REQUEST_FIELD_NONE);

	return field->type;
}

const char *
gaim_request_field_get_id(const GaimRequestField *field)
{
	g_return_val_if_fail(field != NULL, NULL);

	return field->id;
}

const char *
gaim_request_field_get_label(const GaimRequestField *field)
{
	g_return_val_if_fail(field != NULL, NULL);

	return field->label;
}

gboolean
gaim_request_field_is_visible(const GaimRequestField *field)
{
	g_return_val_if_fail(field != NULL, FALSE);

	return field->visible;
}

const char *
gaim_request_field_get_type_hint(const GaimRequestField *field)
{
	g_return_val_if_fail(field != NULL, NULL);

	return field->type_hint;
}

gboolean
gaim_request_field_is_required(const GaimRequestField *field)
{
	g_return_val_if_fail(field != NULL, FALSE);

	return field->required;
}

GaimRequestField *
gaim_request_field_string_new(const char *id, const char *text,
							  const char *default_value, gboolean multiline)
{
	GaimRequestField *field;

	g_return_val_if_fail(id   != NULL, NULL);
	g_return_val_if_fail(text != NULL, NULL);

	field = gaim_request_field_new(id, text, GAIM_REQUEST_FIELD_STRING);

	field->u.string.multiline = multiline;
	field->u.string.editable  = TRUE;

	gaim_request_field_string_set_default_value(field, default_value);
	gaim_request_field_string_set_value(field, default_value);

	return field;
}

void
gaim_request_field_string_set_default_value(GaimRequestField *field,
											const char *default_value)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == GAIM_REQUEST_FIELD_STRING);

	g_free(field->u.string.default_value);
	field->u.string.default_value = g_strdup(default_value);
}

void
gaim_request_field_string_set_value(GaimRequestField *field, const char *value)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == GAIM_REQUEST_FIELD_STRING);

	g_free(field->u.string.value);
	field->u.string.value = g_strdup(value);
}

void
gaim_request_field_string_set_masked(GaimRequestField *field, gboolean masked)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == GAIM_REQUEST_FIELD_STRING);

	field->u.string.masked = masked;
}

void
gaim_request_field_string_set_editable(GaimRequestField *field,
									   gboolean editable)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == GAIM_REQUEST_FIELD_STRING);

	field->u.string.editable = editable;
}

const char *
gaim_request_field_string_get_default_value(const GaimRequestField *field)
{
	g_return_val_if_fail(field != NULL, NULL);
	g_return_val_if_fail(field->type == GAIM_REQUEST_FIELD_STRING, NULL);

	return field->u.string.default_value;
}

const char *
gaim_request_field_string_get_value(const GaimRequestField *field)
{
	g_return_val_if_fail(field != NULL, NULL);
	g_return_val_if_fail(field->type == GAIM_REQUEST_FIELD_STRING, NULL);

	return field->u.string.value;
}

gboolean
gaim_request_field_string_is_multiline(const GaimRequestField *field)
{
	g_return_val_if_fail(field != NULL, FALSE);
	g_return_val_if_fail(field->type == GAIM_REQUEST_FIELD_STRING, FALSE);

	return field->u.string.multiline;
}

gboolean
gaim_request_field_string_is_masked(const GaimRequestField *field)
{
	g_return_val_if_fail(field != NULL, FALSE);
	g_return_val_if_fail(field->type == GAIM_REQUEST_FIELD_STRING, FALSE);

	return field->u.string.masked;
}

gboolean
gaim_request_field_string_is_editable(const GaimRequestField *field)
{
	g_return_val_if_fail(field != NULL, FALSE);
	g_return_val_if_fail(field->type == GAIM_REQUEST_FIELD_STRING, FALSE);

	return field->u.string.editable;
}

GaimRequestField *
gaim_request_field_int_new(const char *id, const char *text,
						   int default_value)
{
	GaimRequestField *field;

	g_return_val_if_fail(id   != NULL, NULL);
	g_return_val_if_fail(text != NULL, NULL);

	field = gaim_request_field_new(id, text, GAIM_REQUEST_FIELD_INTEGER);

	gaim_request_field_int_set_default_value(field, default_value);
	gaim_request_field_int_set_value(field, default_value);

	return field;
}

void
gaim_request_field_int_set_default_value(GaimRequestField *field,
										 int default_value)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == GAIM_REQUEST_FIELD_INTEGER);

	field->u.integer.default_value = default_value;
}

void
gaim_request_field_int_set_value(GaimRequestField *field, int value)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == GAIM_REQUEST_FIELD_INTEGER);

	field->u.integer.value = value;
}

int
gaim_request_field_int_get_default_value(const GaimRequestField *field)
{
	g_return_val_if_fail(field != NULL, 0);
	g_return_val_if_fail(field->type == GAIM_REQUEST_FIELD_INTEGER, 0);

	return field->u.integer.default_value;
}

int
gaim_request_field_int_get_value(const GaimRequestField *field)
{
	g_return_val_if_fail(field != NULL, 0);
	g_return_val_if_fail(field->type == GAIM_REQUEST_FIELD_INTEGER, 0);

	return field->u.integer.value;
}

GaimRequestField *
gaim_request_field_bool_new(const char *id, const char *text,
							gboolean default_value)
{
	GaimRequestField *field;

	g_return_val_if_fail(id   != NULL, NULL);
	g_return_val_if_fail(text != NULL, NULL);

	field = gaim_request_field_new(id, text, GAIM_REQUEST_FIELD_BOOLEAN);

	gaim_request_field_bool_set_default_value(field, default_value);
	gaim_request_field_bool_set_value(field, default_value);

	return field;
}

void
gaim_request_field_bool_set_default_value(GaimRequestField *field,
										  gboolean default_value)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == GAIM_REQUEST_FIELD_BOOLEAN);

	field->u.boolean.default_value = default_value;
}

void
gaim_request_field_bool_set_value(GaimRequestField *field, gboolean value)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == GAIM_REQUEST_FIELD_BOOLEAN);

	field->u.boolean.value = value;
}

gboolean
gaim_request_field_bool_get_default_value(const GaimRequestField *field)
{
	g_return_val_if_fail(field != NULL, FALSE);
	g_return_val_if_fail(field->type == GAIM_REQUEST_FIELD_BOOLEAN, FALSE);

	return field->u.boolean.default_value;
}

gboolean
gaim_request_field_bool_get_value(const GaimRequestField *field)
{
	g_return_val_if_fail(field != NULL, FALSE);
	g_return_val_if_fail(field->type == GAIM_REQUEST_FIELD_BOOLEAN, FALSE);

	return field->u.boolean.value;
}

GaimRequestField *
gaim_request_field_choice_new(const char *id, const char *text,
							  int default_value)
{
	GaimRequestField *field;

	g_return_val_if_fail(id   != NULL, NULL);
	g_return_val_if_fail(text != NULL, NULL);

	field = gaim_request_field_new(id, text, GAIM_REQUEST_FIELD_CHOICE);

	gaim_request_field_choice_set_default_value(field, default_value);
	gaim_request_field_choice_set_value(field, default_value);

	return field;
}

void
gaim_request_field_choice_add(GaimRequestField *field, const char *label)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(label != NULL);
	g_return_if_fail(field->type == GAIM_REQUEST_FIELD_CHOICE);

	field->u.choice.labels = g_list_append(field->u.choice.labels,
											g_strdup(label));
}

void
gaim_request_field_choice_set_default_value(GaimRequestField *field,
											int default_value)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == GAIM_REQUEST_FIELD_CHOICE);

	field->u.choice.default_value = default_value;
}

void
gaim_request_field_choice_set_value(GaimRequestField *field,
											int value)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == GAIM_REQUEST_FIELD_CHOICE);

	field->u.choice.value = value;
}

int
gaim_request_field_choice_get_default_value(const GaimRequestField *field)
{
	g_return_val_if_fail(field != NULL, -1);
	g_return_val_if_fail(field->type == GAIM_REQUEST_FIELD_CHOICE, -1);

	return field->u.choice.default_value;
}

int
gaim_request_field_choice_get_value(const GaimRequestField *field)
{
	g_return_val_if_fail(field != NULL, -1);
	g_return_val_if_fail(field->type == GAIM_REQUEST_FIELD_CHOICE, -1);

	return field->u.choice.value;
}

GList *
gaim_request_field_choice_get_labels(const GaimRequestField *field)
{
	g_return_val_if_fail(field != NULL, NULL);
	g_return_val_if_fail(field->type == GAIM_REQUEST_FIELD_CHOICE, NULL);

	return field->u.choice.labels;
}

GaimRequestField *
gaim_request_field_list_new(const char *id, const char *text)
{
	GaimRequestField *field;

	g_return_val_if_fail(id   != NULL, NULL);

	field = gaim_request_field_new(id, text, GAIM_REQUEST_FIELD_LIST);

	field->u.list.item_data = g_hash_table_new_full(g_str_hash, g_str_equal,
													g_free, NULL);

	field->u.list.selected_table =
		g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	return field;
}

void
gaim_request_field_list_set_multi_select(GaimRequestField *field,
										 gboolean multi_select)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == GAIM_REQUEST_FIELD_LIST);

	field->u.list.multiple_selection = multi_select;
}

gboolean
gaim_request_field_list_get_multi_select(const GaimRequestField *field)
{
	g_return_val_if_fail(field != NULL, FALSE);
	g_return_val_if_fail(field->type == GAIM_REQUEST_FIELD_LIST, FALSE);

	return field->u.list.multiple_selection;
}

void *
gaim_request_field_list_get_data(const GaimRequestField *field,
								 const char *text)
{
	g_return_val_if_fail(field != NULL, NULL);
	g_return_val_if_fail(text  != NULL, NULL);
	g_return_val_if_fail(field->type == GAIM_REQUEST_FIELD_LIST, NULL);

	return g_hash_table_lookup(field->u.list.item_data, text);
}

void
gaim_request_field_list_add(GaimRequestField *field, const char *item,
							void *data)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(item  != NULL);
	g_return_if_fail(data  != NULL);
	g_return_if_fail(field->type == GAIM_REQUEST_FIELD_LIST);

	field->u.list.items = g_list_append(field->u.list.items, g_strdup(item));

	g_hash_table_insert(field->u.list.item_data, g_strdup(item), data);
}

void
gaim_request_field_list_add_selected(GaimRequestField *field, const char *item)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(item  != NULL);
	g_return_if_fail(field->type == GAIM_REQUEST_FIELD_LIST);

	if (!gaim_request_field_list_get_multi_select(field) &&
		field->u.list.selected != NULL)
	{
		gaim_debug_warning("request",
						   "More than one item added to non-multi-select "
						   "field %s\n",
						   gaim_request_field_get_id(field));
		return;
	}

	field->u.list.selected = g_list_append(field->u.list.selected,
										   g_strdup(item));

	g_hash_table_insert(field->u.list.selected_table, g_strdup(item), NULL);
}

void
gaim_request_field_list_clear_selected(GaimRequestField *field)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == GAIM_REQUEST_FIELD_LIST);

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
gaim_request_field_list_set_selected(GaimRequestField *field, const GList *items)
{
	const GList *l;

	g_return_if_fail(field != NULL);
	g_return_if_fail(items != NULL);
	g_return_if_fail(field->type == GAIM_REQUEST_FIELD_LIST);

	gaim_request_field_list_clear_selected(field);

	if (!gaim_request_field_list_get_multi_select(field) &&
		g_list_length((GList*)items) > 1)
	{
		gaim_debug_warning("request",
						   "More than one item added to non-multi-select "
						   "field %s\n",
						   gaim_request_field_get_id(field));
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
gaim_request_field_list_is_selected(const GaimRequestField *field,
									const char *item)
{
	g_return_val_if_fail(field != NULL, FALSE);
	g_return_val_if_fail(item  != NULL, FALSE);
	g_return_val_if_fail(field->type == GAIM_REQUEST_FIELD_LIST, FALSE);

	return g_hash_table_lookup_extended(field->u.list.selected_table,
										item, NULL, NULL);
}

const GList *
gaim_request_field_list_get_selected(const GaimRequestField *field)
{
	g_return_val_if_fail(field != NULL, NULL);
	g_return_val_if_fail(field->type == GAIM_REQUEST_FIELD_LIST, NULL);

	return field->u.list.selected;
}

const GList *
gaim_request_field_list_get_items(const GaimRequestField *field)
{
	g_return_val_if_fail(field != NULL, NULL);
	g_return_val_if_fail(field->type == GAIM_REQUEST_FIELD_LIST, NULL);

	return field->u.list.items;
}

GaimRequestField *
gaim_request_field_label_new(const char *id, const char *text)
{
	GaimRequestField *field;

	g_return_val_if_fail(id   != NULL, NULL);
	g_return_val_if_fail(text != NULL, NULL);

	field = gaim_request_field_new(id, text, GAIM_REQUEST_FIELD_LABEL);

	return field;
}

GaimRequestField *
gaim_request_field_image_new(const char *id, const char *text, const char *buf, gsize size)
{
	GaimRequestField *field;

	g_return_val_if_fail(id   != NULL, NULL);
	g_return_val_if_fail(text != NULL, NULL);
	g_return_val_if_fail(buf  != NULL, NULL);
	g_return_val_if_fail(size > 0, NULL);

	field = gaim_request_field_new(id, text, GAIM_REQUEST_FIELD_IMAGE);

	field->u.image.buffer  = g_memdup(buf, size);
	field->u.image.size    = size;
	field->u.image.scale_x = 1;
	field->u.image.scale_y = 1;

	return field;
}

void
gaim_request_field_image_set_scale(GaimRequestField *field, unsigned int x, unsigned int y)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == GAIM_REQUEST_FIELD_IMAGE);

	field->u.image.scale_x = x;
	field->u.image.scale_y = y;
}

const char *
gaim_request_field_image_get_buffer(GaimRequestField *field)
{
	g_return_val_if_fail(field != NULL, NULL);
	g_return_val_if_fail(field->type == GAIM_REQUEST_FIELD_IMAGE, NULL);

	return field->u.image.buffer;
}

gsize
gaim_request_field_image_get_size(GaimRequestField *field)
{
	g_return_val_if_fail(field != NULL, 0);
	g_return_val_if_fail(field->type == GAIM_REQUEST_FIELD_IMAGE, 0);

	return field->u.image.size;
}

unsigned int
gaim_request_field_image_get_scale_x(GaimRequestField *field)
{
	g_return_val_if_fail(field != NULL, 0);
	g_return_val_if_fail(field->type == GAIM_REQUEST_FIELD_IMAGE, 0);

	return field->u.image.scale_x;
}

unsigned int
gaim_request_field_image_get_scale_y(GaimRequestField *field)
{
	g_return_val_if_fail(field != NULL, 0);
	g_return_val_if_fail(field->type == GAIM_REQUEST_FIELD_IMAGE, 0);

	return field->u.image.scale_y;
}

GaimRequestField *
gaim_request_field_account_new(const char *id, const char *text,
							   GaimAccount *account)
{
	GaimRequestField *field;

	g_return_val_if_fail(id   != NULL, NULL);
	g_return_val_if_fail(text != NULL, NULL);

	field = gaim_request_field_new(id, text, GAIM_REQUEST_FIELD_ACCOUNT);

	if (account == NULL && gaim_connections_get_all() != NULL)
	{
		account = gaim_connection_get_account(
			(GaimConnection *)gaim_connections_get_all()->data);
	}

	gaim_request_field_account_set_default_value(field, account);
	gaim_request_field_account_set_value(field, account);

	return field;
}

void
gaim_request_field_account_set_default_value(GaimRequestField *field,
											 GaimAccount *default_value)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == GAIM_REQUEST_FIELD_ACCOUNT);

	field->u.account.default_account = default_value;
}

void
gaim_request_field_account_set_value(GaimRequestField *field,
									 GaimAccount *value)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == GAIM_REQUEST_FIELD_ACCOUNT);

	field->u.account.account = value;
}

void
gaim_request_field_account_set_show_all(GaimRequestField *field,
										gboolean show_all)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == GAIM_REQUEST_FIELD_ACCOUNT);

	if (field->u.account.show_all == show_all)
		return;

	field->u.account.show_all = show_all;

	if (!show_all)
	{
		if (gaim_account_is_connected(field->u.account.default_account))
		{
			gaim_request_field_account_set_default_value(field,
				(GaimAccount *)gaim_connections_get_all()->data);
		}

		if (gaim_account_is_connected(field->u.account.account))
		{
			gaim_request_field_account_set_value(field,
				(GaimAccount *)gaim_connections_get_all()->data);
		}
	}
}

void
gaim_request_field_account_set_filter(GaimRequestField *field,
									  GaimFilterAccountFunc filter_func)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == GAIM_REQUEST_FIELD_ACCOUNT);

	field->u.account.filter_func = filter_func;
}

GaimAccount *
gaim_request_field_account_get_default_value(const GaimRequestField *field)
{
	g_return_val_if_fail(field != NULL, NULL);
	g_return_val_if_fail(field->type == GAIM_REQUEST_FIELD_ACCOUNT, NULL);

	return field->u.account.default_account;
}

GaimAccount *
gaim_request_field_account_get_value(const GaimRequestField *field)
{
	g_return_val_if_fail(field != NULL, NULL);
	g_return_val_if_fail(field->type == GAIM_REQUEST_FIELD_ACCOUNT, NULL);

	return field->u.account.account;
}

gboolean
gaim_request_field_account_get_show_all(const GaimRequestField *field)
{
	g_return_val_if_fail(field != NULL, FALSE);
	g_return_val_if_fail(field->type == GAIM_REQUEST_FIELD_ACCOUNT, FALSE);

	return field->u.account.show_all;
}

GaimFilterAccountFunc
gaim_request_field_account_get_filter(const GaimRequestField *field)
{
	g_return_val_if_fail(field != NULL, FALSE);
	g_return_val_if_fail(field->type == GAIM_REQUEST_FIELD_ACCOUNT, FALSE);

	return field->u.account.filter_func;
}

/* -- */

void *
gaim_request_input(void *handle, const char *title, const char *primary,
				   const char *secondary, const char *default_value,
				   gboolean multiline, gboolean masked, gchar *hint,
				   const char *ok_text, GCallback ok_cb,
				   const char *cancel_text, GCallback cancel_cb,
				   void *user_data)
{
	GaimRequestUiOps *ops;

	g_return_val_if_fail(ok_text != NULL, NULL);
	g_return_val_if_fail(ok_cb   != NULL, NULL);

	ops = gaim_request_get_ui_ops();

	if (ops != NULL && ops->request_input != NULL) {
		GaimRequestInfo *info;

		info            = g_new0(GaimRequestInfo, 1);
		info->type      = GAIM_REQUEST_INPUT;
		info->handle    = handle;
		info->ui_handle = ops->request_input(title, primary, secondary,
											 default_value,
											 multiline, masked, hint,
											 ok_text, ok_cb,
											 cancel_text, cancel_cb,
											 user_data);

		handles = g_list_append(handles, info);

		return info->ui_handle;
	}

	return NULL;
}

void *
gaim_request_choice(void *handle, const char *title, const char *primary,
					const char *secondary, unsigned int default_value,
					const char *ok_text, GCallback ok_cb,
					const char *cancel_text, GCallback cancel_cb,
					void *user_data, ...)
{
	void *ui_handle;
	va_list args;

	g_return_val_if_fail(ok_text != NULL,  NULL);
	g_return_val_if_fail(ok_cb   != NULL,  NULL);

	va_start(args, user_data);
	ui_handle = gaim_request_choice_varg(handle, title, primary, secondary,
					     default_value, ok_text, ok_cb,
					     cancel_text, cancel_cb, user_data, args);
	va_end(args);

	return ui_handle;
}

void *
gaim_request_choice_varg(void *handle, const char *title,
			 const char *primary, const char *secondary,
			 unsigned int default_value,
			 const char *ok_text, GCallback ok_cb,
			 const char *cancel_text, GCallback cancel_cb,
			 void *user_data, va_list choices)
{
	GaimRequestUiOps *ops;

	g_return_val_if_fail(ok_text != NULL,  NULL);
	g_return_val_if_fail(ok_cb   != NULL,  NULL);

	ops = gaim_request_get_ui_ops();

	if (ops != NULL && ops->request_choice != NULL) {
		GaimRequestInfo *info;

		info            = g_new0(GaimRequestInfo, 1);
		info->type      = GAIM_REQUEST_CHOICE;
		info->handle    = handle;
		info->ui_handle = ops->request_choice(title, primary, secondary,
						      default_value,
						      ok_text, ok_cb,
						      cancel_text, cancel_cb,
						      user_data, choices);

		handles = g_list_append(handles, info);

		return info->ui_handle;
	}

	return NULL;
}

void *
gaim_request_action(void *handle, const char *title, const char *primary,
					const char *secondary, unsigned int default_action,
					void *user_data, size_t action_count, ...)
{
	void *ui_handle;
	va_list args;

	g_return_val_if_fail(action_count > 0, NULL);

	va_start(args, action_count);
	ui_handle = gaim_request_action_varg(handle, title, primary, secondary,
										 default_action, user_data,
										 action_count, args);
	va_end(args);

	return ui_handle;
}

void *
gaim_request_action_varg(void *handle, const char *title,
						 const char *primary, const char *secondary,
						 unsigned int default_action, void *user_data,
						 size_t action_count, va_list actions)
{
	GaimRequestUiOps *ops;

	g_return_val_if_fail(action_count > 0, NULL);

	ops = gaim_request_get_ui_ops();

	if (ops != NULL && ops->request_action != NULL) {
		GaimRequestInfo *info;

		info            = g_new0(GaimRequestInfo, 1);
		info->type      = GAIM_REQUEST_ACTION;
		info->handle    = handle;
		info->ui_handle = ops->request_action(title, primary, secondary,
											  default_action, user_data,
											  action_count, actions);

		handles = g_list_append(handles, info);

		return info->ui_handle;
	}

	return NULL;
}

void *
gaim_request_fields(void *handle, const char *title, const char *primary,
					const char *secondary, GaimRequestFields *fields,
					const char *ok_text, GCallback ok_cb,
					const char *cancel_text, GCallback cancel_cb,
					void *user_data)
{
	GaimRequestUiOps *ops;

	g_return_val_if_fail(fields  != NULL, NULL);
	g_return_val_if_fail(ok_text != NULL, NULL);
	g_return_val_if_fail(ok_cb   != NULL, NULL);

	ops = gaim_request_get_ui_ops();

	if (ops != NULL && ops->request_fields != NULL) {
		GaimRequestInfo *info;

		info            = g_new0(GaimRequestInfo, 1);
		info->type      = GAIM_REQUEST_FIELDS;
		info->handle    = handle;
		info->ui_handle = ops->request_fields(title, primary, secondary,
											  fields, ok_text, ok_cb,
											  cancel_text, cancel_cb,
											  user_data);

		handles = g_list_append(handles, info);

		return info->ui_handle;
	}

	return NULL;
}

void *
gaim_request_file(void *handle, const char *title, const char *filename,
				  gboolean savedialog,
				  GCallback ok_cb, GCallback cancel_cb, void *user_data)
{
	GaimRequestUiOps *ops;

	ops = gaim_request_get_ui_ops();

	if (ops != NULL && ops->request_file != NULL) {
		GaimRequestInfo *info;

		info            = g_new0(GaimRequestInfo, 1);
		info->type      = GAIM_REQUEST_FILE;
		info->handle    = handle;
		info->ui_handle = ops->request_file(title, filename, savedialog,
											ok_cb, cancel_cb, user_data);
		handles = g_list_append(handles, info);
		return info->ui_handle;
	}

	return NULL;
}

void *
gaim_request_folder(void *handle, const char *title, const char *dirname,
				  GCallback ok_cb, GCallback cancel_cb, void *user_data)
{
	GaimRequestUiOps *ops;

	ops = gaim_request_get_ui_ops();

	if (ops != NULL && ops->request_file != NULL) {
		GaimRequestInfo *info;

		info            = g_new0(GaimRequestInfo, 1);
		info->type      = GAIM_REQUEST_FOLDER;
		info->handle    = handle;
		info->ui_handle = ops->request_folder(title, dirname,
											ok_cb, cancel_cb, user_data);
		handles = g_list_append(handles, info);
		return info->ui_handle;
	}

	return NULL;
}

static void
gaim_request_close_info(GaimRequestInfo *info)
{
	GaimRequestUiOps *ops;

	ops = gaim_request_get_ui_ops();

	gaim_notify_close_with_handle(info->ui_handle);
	gaim_request_close_with_handle(info->ui_handle);

	if (ops != NULL && ops->close_request != NULL)
		ops->close_request(info->type, info->ui_handle);

	g_free(info);
}

void
gaim_request_close(GaimRequestType type, void *ui_handle)
{
	GList *l;

	g_return_if_fail(ui_handle != NULL);

	for (l = handles; l != NULL; l = l->next) {
		GaimRequestInfo *info = l->data;

		if (info->ui_handle == ui_handle) {
			handles = g_list_remove(handles, info);
			gaim_request_close_info(info);
			break;
		}
	}
}

void
gaim_request_close_with_handle(void *handle)
{
	GList *l, *l_next;

	g_return_if_fail(handle != NULL);

	for (l = handles; l != NULL; l = l_next) {
		GaimRequestInfo *info = l->data;

		l_next = l->next;

		if (info->handle == handle) {
			handles = g_list_remove(handles, info);
			gaim_request_close_info(info);
		}
	}
}

void
gaim_request_set_ui_ops(GaimRequestUiOps *ops)
{
	request_ui_ops = ops;
}

GaimRequestUiOps *
gaim_request_get_ui_ops(void)
{
	return request_ui_ops;
}
