/**
 * @file notify.c Notification API
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
#include "notify.h"

static GaimNotifyUiOps *notify_ui_ops = NULL;
static GList *handles = NULL;

typedef struct
{
	GaimNotifyType type;
	void *handle;
	void *ptr;

} GaimNotifyInfo;

void *
gaim_notify_message(void *handle, GaimNotifyType type,
					const char *title, const char *primary,
					const char *secondary, GCallback cb, void *user_data)
{
	GaimNotifyUiOps *ops;

	g_return_val_if_fail(primary != NULL, NULL);

	ops = gaim_get_notify_ui_ops();

	if (ops != NULL && ops->notify_message != NULL) {
		GaimNotifyInfo *info;

		info         = g_new0(GaimNotifyInfo, 1);
		info->type   = GAIM_NOTIFY_MESSAGE;
		info->handle = handle;
		info->ptr    = ops->notify_message(type, title, primary, secondary,
										   cb, user_data);

		handles = g_list_append(handles, info);

		return info->ptr;
	}

	return NULL;
}

void *
gaim_notify_email(void *handle, const char *subject, const char *from,
				  const char *to, const char *url, GCallback cb,
				  void *user_data)
{
	GaimNotifyUiOps *ops;

	ops = gaim_get_notify_ui_ops();

	if (ops != NULL && ops->notify_email != NULL) {
		GaimNotifyInfo *info;

		info         = g_new0(GaimNotifyInfo, 1);
		info->type   = GAIM_NOTIFY_EMAIL;
		info->handle = handle;
		info->ptr    = ops->notify_email(subject, from, to, url, cb,
										 user_data);

		handles = g_list_append(handles, info);

		return info->ptr;
	}

	return NULL;
}

void *
gaim_notify_emails(void *handle, size_t count, const char **subjects,
				   const char **froms, const char **tos, const char **urls,
				   GCallback cb, void *user_data)
{
	GaimNotifyUiOps *ops;

	g_return_val_if_fail(count != 0, NULL);

	ops = gaim_get_notify_ui_ops();

	if (ops != NULL && ops->notify_emails != NULL) {
		GaimNotifyInfo *info;

		info         = g_new0(GaimNotifyInfo, 1);
		info->type   = GAIM_NOTIFY_EMAILS;
		info->handle = handle;
		info->ptr    = ops->notify_emails(count, subjects, froms, tos, urls,
										  cb, user_data);

		handles = g_list_append(handles, info);

		return info->ptr;
	}

	return NULL;
}

void
gaim_notify_close(GaimNotifyType type, void *ptr)
{
	GList *l;
	GaimNotifyUiOps *ops;

	g_return_if_fail(ptr != NULL);

	ops = gaim_get_notify_ui_ops();

	for (l = handles; l != NULL; l = l->next) {
		GaimNotifyInfo *info = l->data;

		if (info->ptr == ptr) {
			handles = g_list_remove(handles, info);

			if (ops != NULL && ops->close_notify != NULL)
				ops->close_notify(info->type, ptr);

			g_free(info);

			break;
		}
	}
}

void
gaim_notify_close_with_handle(void *handle)
{
	GList *l, *l_next;
	GaimNotifyUiOps *ops;

	g_return_if_fail(handle != NULL);

	ops = gaim_get_notify_ui_ops();

	for (l = handles; l != NULL; l = l_next) {
		GaimNotifyInfo *info = l->data;

		l_next = l->next;

		if (info->handle == handle) {
			handles = g_list_remove(handles, info);

			if (ops != NULL && ops->close_notify != NULL)
				ops->close_notify(info->type, info->ptr);

			g_free(info);
		}
	}
}

void
gaim_set_notify_ui_ops(GaimNotifyUiOps *ops)
{
	notify_ui_ops = ops;
}

GaimNotifyUiOps *
gaim_get_notify_ui_ops(void)
{
	return notify_ui_ops;
}

