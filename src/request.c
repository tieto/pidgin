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
					void *user_data, const char *choice, ...)
{
	void *ui_handle;
	va_list args;

	g_return_val_if_fail(primary != NULL, NULL);
	g_return_val_if_fail(ok_text != NULL, NULL);
	g_return_val_if_fail(ok_cb   != NULL, NULL);
	g_return_val_if_fail(choice  != NULL, NULL);

	va_start(args, choice);
	ui_handle = gaim_request_choice_varg(handle, title, primary, secondary,
										 default_value, ok_text, ok_cb,
										 cancel_text, cancel_cb, user_data,
										 args);
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

	g_return_val_if_fail(primary != NULL, NULL);
	g_return_val_if_fail(ok_text != NULL, NULL);
	g_return_val_if_fail(ok_cb   != NULL, NULL);

	ops = gaim_get_request_ui_ops();

	if (ops != NULL && ops->request_input != NULL) {
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
					void *user_data, const char *action, ...)
{
	void *ui_handle;
	va_list args;

	g_return_val_if_fail(primary   != NULL, NULL);
	g_return_val_if_fail(action    != NULL, NULL);

	va_start(args, action);
	ui_handle = gaim_request_action_varg(handle, title, primary, secondary,
										 default_action, user_data, args);
	va_end(args);

	return ui_handle;
}

void *
gaim_request_action_varg(void *handle, const char *title,
						 const char *primary, const char *secondary,
						 unsigned int default_action, void *user_data,
						 va_list actions)
{
	GaimRequestUiOps *ops;

	g_return_val_if_fail(primary != NULL, NULL);

	ops = gaim_get_request_ui_ops();

	if (ops != NULL && ops->request_input != NULL) {
		GaimRequestInfo *info;

		info            = g_new0(GaimRequestInfo, 1);
		info->type      = GAIM_REQUEST_ACTION;
		info->handle    = handle;
		info->ui_handle = ops->request_action(title, primary, secondary,
											  default_action, user_data,
											  actions);

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


