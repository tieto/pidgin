/**
 * @file request.c Request API
 * @ingroup core
 *
 * gaim
 *
 * Copyright (C) 2003 Christian Hammond <chipx86@gnupdate.org>
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
#include "request.h"

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
	GList *l;
	GaimRequestFieldGroup *group;

	g_return_if_fail(fields != NULL);

	for (l = fields->groups; l != NULL; l = l->next) {
		group = l->data;

		gaim_request_field_group_destroy(group);
	}

	g_list_free(fields->groups);

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
	}
}

GList *
gaim_request_fields_get_groups(const GaimRequestFields *fields)
{
	g_return_val_if_fail(fields != NULL, NULL);

	return fields->groups;
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

GaimRequestFieldGroup *
gaim_request_field_group_new(const char *title)
{
	GaimRequestFieldGroup *group;

	group = g_new0(GaimRequestFieldGroup, 1);

	if (title != NULL)
		group->title = g_strdup(title);

	return group;
}

void
gaim_request_field_group_destroy(GaimRequestFieldGroup *group)
{
	GaimRequestField *field;
	GList *l;

	g_return_if_fail(group != NULL);

	if (group->title != NULL)
		g_free(group->title);

	for (l = group->fields; l != NULL; l = l->next) {
		field = l->data;

		gaim_request_field_destroy(field);
	}

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

	if (group->fields_list != NULL) {
		g_hash_table_insert(group->fields_list->fields,
							g_strdup(gaim_request_field_get_id(field)), field);
	}
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

	return field;
}

void
gaim_request_field_destroy(GaimRequestField *field)
{
	g_return_if_fail(field != NULL);

	if (field->id != NULL)
		g_free(field->id);

	if (field->label != NULL)
		g_free(field->label);

	if (field->type == GAIM_REQUEST_FIELD_STRING) {
		if (field->u.string.default_value != NULL)
			g_free(field->u.string.default_value);

		if (field->u.string.value != NULL)
			g_free(field->u.string.value);
	}
	else if (field->type == GAIM_REQUEST_FIELD_CHOICE) {
		GList *l;

		for (l = field->u.choice.labels; l != NULL; l = l->next) {
			g_free(l->data);
		}

		if (field->u.choice.labels != NULL)
			g_list_free(field->u.choice.labels);
	}

	g_free(field);
}

void
gaim_request_field_set_label(GaimRequestField *field, const char *label)
{
	g_return_if_fail(field != NULL);

	if (field->label != NULL)
		g_free(field->label);

	field->label = (label == NULL ? NULL : g_strdup(label));
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

GaimRequestField *
gaim_request_field_string_new(const char *id, const char *text,
							  const char *default_value, gboolean multiline)
{
	GaimRequestField *field;

	g_return_val_if_fail(id   != NULL, NULL);
	g_return_val_if_fail(text != NULL, NULL);

	field = gaim_request_field_new(id, text, GAIM_REQUEST_FIELD_STRING);

	field->u.string.multiline = multiline;

	gaim_request_field_string_set_default_value(field, default_value);

	return field;
}

void
gaim_request_field_string_set_default_value(GaimRequestField *field,
											const char *default_value)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == GAIM_REQUEST_FIELD_STRING);

	if (field->u.string.default_value != NULL)
		g_free(field->u.string.default_value);

	field->u.string.default_value = (default_value == NULL
									  ? NULL : g_strdup(default_value));
}

void
gaim_request_field_string_set_value(GaimRequestField *field, const char *value)
{
	g_return_if_fail(field != NULL);
	g_return_if_fail(field->type == GAIM_REQUEST_FIELD_STRING);

	if (field->u.string.value != NULL)
		g_free(field->u.string.value);

	field->u.string.value = (value == NULL ? NULL : g_strdup(value));
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

GaimRequestField *
gaim_request_field_int_new(const char *id, const char *text,
						   int default_value)
{
	GaimRequestField *field;

	g_return_val_if_fail(id   != NULL, NULL);
	g_return_val_if_fail(text != NULL, NULL);

	field = gaim_request_field_new(id, text, GAIM_REQUEST_FIELD_INTEGER);

	gaim_request_field_int_set_default_value(field, default_value);

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

/* -- */

void *
gaim_request_input(void *handle, const char *title, const char *primary,
				   const char *secondary, const char *default_value,
				   gboolean multiline,
				   const char *ok_text, GCallback ok_cb,
				   const char *cancel_text, GCallback cancel_cb,
				   void *user_data)
{
	GaimRequestUiOps *ops;

	g_return_val_if_fail(primary != NULL, NULL);
	g_return_val_if_fail(ok_text != NULL, NULL);
	g_return_val_if_fail(ok_cb   != NULL, NULL);

	ops = gaim_get_request_ui_ops();

	if (ops != NULL && ops->request_input != NULL) {
		GaimRequestInfo *info;

		info            = g_new0(GaimRequestInfo, 1);
		info->type      = GAIM_REQUEST_INPUT;
		info->handle    = handle;
		info->ui_handle = ops->request_input(title, primary, secondary,
											 default_value, multiline,
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
					void *user_data, size_t choice_count, ...)
{
	void *ui_handle;
	va_list args;

	g_return_val_if_fail(primary != NULL,  NULL);
	g_return_val_if_fail(ok_text != NULL,  NULL);
	g_return_val_if_fail(ok_cb   != NULL,  NULL);
	g_return_val_if_fail(choice_count > 0, NULL);

	va_start(args, choice_count);
	ui_handle = gaim_request_choice_varg(handle, title, primary, secondary,
										 default_value, ok_text, ok_cb,
										 cancel_text, cancel_cb, user_data,
										 choice_count, args);
	va_end(args);

	return ui_handle;
}

void *
gaim_request_choice_varg(void *handle, const char *title,
						 const char *primary, const char *secondary,
						 unsigned int default_value,
						 const char *ok_text, GCallback ok_cb,
						 const char *cancel_text, GCallback cancel_cb,
						 void *user_data, size_t choice_count,
						 va_list choices)
{
	GaimRequestUiOps *ops;

	g_return_val_if_fail(primary != NULL,  NULL);
	g_return_val_if_fail(ok_text != NULL,  NULL);
	g_return_val_if_fail(ok_cb   != NULL,  NULL);
	g_return_val_if_fail(choice_count > 0, NULL);

	ops = gaim_get_request_ui_ops();

	if (ops != NULL && ops->request_choice != NULL) {
		GaimRequestInfo *info;

		info            = g_new0(GaimRequestInfo, 1);
		info->type      = GAIM_REQUEST_CHOICE;
		info->handle    = handle;
		info->ui_handle = ops->request_choice(title, primary, secondary,
											  default_value,
											  ok_text, ok_cb,
											  cancel_text, cancel_cb,
											  user_data, choice_count,
											  choices);

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

	g_return_val_if_fail(primary != NULL,  NULL);
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

	g_return_val_if_fail(primary != NULL,  NULL);
	g_return_val_if_fail(action_count > 0, NULL);

	ops = gaim_get_request_ui_ops();

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

	ops = gaim_get_request_ui_ops();

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

void
gaim_request_close(GaimRequestType type, void *ui_handle)
{
	GList *l;
	GaimRequestUiOps *ops;

	g_return_if_fail(ui_handle != NULL);

	ops = gaim_get_request_ui_ops();

	for (l = handles; l != NULL; l = l->next) {
		GaimRequestInfo *info = l->data;

		if (info->ui_handle == ui_handle) {
			handles = g_list_remove(handles, info);

			if (ops != NULL && ops->close_request != NULL)
				ops->close_request(info->type, ui_handle);

			g_free(info);

			break;
		}
	}
}

void
gaim_request_close_with_handle(void *handle)
{
	GList *l, *l_next;
	GaimRequestUiOps *ops;

	g_return_if_fail(handle != NULL);

	ops = gaim_get_request_ui_ops();

	for (l = handles; l != NULL; l = l_next) {
		GaimRequestInfo *info = l->data;

		l_next = l->next;

		if (info->handle == handle) {
			handles = g_list_remove(handles, info);

			if (ops != NULL && ops->close_request != NULL)
				ops->close_request(info->type, info->ui_handle);

			g_free(info);
		}
	}
}

void
gaim_set_request_ui_ops(GaimRequestUiOps *ops)
{
	request_ui_ops = ops;
}

GaimRequestUiOps *
gaim_get_request_ui_ops(void)
{
	return request_ui_ops;
}


