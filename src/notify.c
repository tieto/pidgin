/**
 * @file notify.c Notification API
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

static GaimNotifyUiOps *notify_ui_ops = NULL;
static GList *handles = NULL;

typedef struct
{
	GaimNotifyType type;
	void *handle;
	void *ui_handle;

} GaimNotifyInfo;

void *
gaim_notify_message(void *handle, GaimNotifyMsgType type,
					const char *title, const char *primary,
					const char *secondary, GCallback cb, void *user_data)
{
	GaimNotifyUiOps *ops;

	g_return_val_if_fail(primary != NULL, NULL);

	ops = gaim_notify_get_ui_ops();

	if (ops != NULL && ops->notify_message != NULL) {
		GaimNotifyInfo *info;

		info            = g_new0(GaimNotifyInfo, 1);
		info->type      = GAIM_NOTIFY_MESSAGE;
		info->handle    = handle;
		info->ui_handle = ops->notify_message(type, title, primary,
											  secondary, cb, user_data);

		handles = g_list_append(handles, info);

		return info->ui_handle;
	}

	return NULL;
}

void *
gaim_notify_email(void *handle, const char *subject, const char *from,
				  const char *to, const char *url, GCallback cb,
				  void *user_data)
{
	GaimNotifyUiOps *ops;

	ops = gaim_notify_get_ui_ops();

	if (ops != NULL && ops->notify_email != NULL) {
		GaimNotifyInfo *info;

		info            = g_new0(GaimNotifyInfo, 1);
		info->type      = GAIM_NOTIFY_EMAIL;
		info->handle    = handle;
		info->ui_handle = ops->notify_email(subject, from, to, url, cb,
											user_data);

		handles = g_list_append(handles, info);

		return info->ui_handle;
	}

	return NULL;
}

void *
gaim_notify_emails(void *handle, size_t count, gboolean detailed,
				   const char **subjects, const char **froms,
				   const char **tos, const char **urls,
				   GCallback cb, void *user_data)
{
	GaimNotifyUiOps *ops;

	g_return_val_if_fail(count != 0, NULL);

	if (count == 1) {
		return gaim_notify_email(handle,
								 (subjects == NULL ? NULL : *subjects),
								 (froms    == NULL ? NULL : *froms),
								 (tos      == NULL ? NULL : *tos),
								 (urls     == NULL ? NULL : *urls),
								 cb, user_data);
	}

	ops = gaim_notify_get_ui_ops();

	if (ops != NULL && ops->notify_emails != NULL) {
		GaimNotifyInfo *info;

		info            = g_new0(GaimNotifyInfo, 1);
		info->type      = GAIM_NOTIFY_EMAILS;
		info->handle    = handle;
		info->ui_handle = ops->notify_emails(count, detailed, subjects,
											 froms, tos, urls, cb, user_data);

		handles = g_list_append(handles, info);

		return info->ui_handle;
	}

	return NULL;
}

void *
gaim_notify_formatted(void *handle, const char *title, const char *primary,
					  const char *secondary, const char *text,
					  GCallback cb, void *user_data)
{
	GaimNotifyUiOps *ops;

	g_return_val_if_fail(primary != NULL, NULL);

	ops = gaim_notify_get_ui_ops();

	if (ops != NULL && ops->notify_formatted != NULL) {
		GaimNotifyInfo *info;

		info            = g_new0(GaimNotifyInfo, 1);
		info->type      = GAIM_NOTIFY_FORMATTED;
		info->handle    = handle;
		info->ui_handle = ops->notify_formatted(title, primary, secondary,
												text, cb, user_data);

		handles = g_list_append(handles, info);

		return info->ui_handle;
	}

	return NULL;
}

void *gaim_notify_userinfo(GaimConnection *gc, const char *who, const char *title,
						   const char *primary, const char *secondary, 
						   const char *text, GCallback cb, void *user_data)
{
	GaimNotifyUiOps *ops;

	g_return_val_if_fail(primary != NULL, NULL);

	ops = gaim_notify_get_ui_ops();

	if (ops != NULL && ops->notify_userinfo != NULL) {
		GaimNotifyInfo *info;

		info            = g_new0(GaimNotifyInfo, 1);
		info->type      = GAIM_NOTIFY_USERINFO;
		info->handle    = gc;
		info->ui_handle = ops->notify_userinfo(gc, who, title, primary,
											   secondary, text, cb, user_data);

		handles = g_list_append(handles, info);

		return info->ui_handle;
	}

	return NULL;
}

void *
gaim_notify_uri(void *handle, const char *uri)
{
	GaimNotifyUiOps *ops;

	g_return_val_if_fail(uri != NULL, NULL);

	ops = gaim_notify_get_ui_ops();

	if (ops != NULL && ops->notify_uri != NULL) {
		GaimNotifyInfo *info;

		info            = g_new0(GaimNotifyInfo, 1);
		info->type      = GAIM_NOTIFY_URI;
		info->handle    = handle;
		info->ui_handle = ops->notify_uri(uri);

		handles = g_list_append(handles, info);

		return info->ui_handle;
	}

	return NULL;
}

void
gaim_notify_close(GaimNotifyType type, void *ui_handle)
{
	GList *l;
	GaimNotifyUiOps *ops;

	g_return_if_fail(ui_handle != NULL);

	ops = gaim_notify_get_ui_ops();

	for (l = handles; l != NULL; l = l->next) {
		GaimNotifyInfo *info = l->data;

		if (info->ui_handle == ui_handle) {
			handles = g_list_remove(handles, info);

			if (ops != NULL && ops->close_notify != NULL)
				ops->close_notify(info->type, ui_handle);

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

	ops = gaim_notify_get_ui_ops();

	for (l = handles; l != NULL; l = l_next) {
		GaimNotifyInfo *info = l->data;

		l_next = l->next;

		if (info->handle == handle) {
			handles = g_list_remove(handles, info);

			if (ops != NULL && ops->close_notify != NULL)
				ops->close_notify(info->type, info->ui_handle);

			g_free(info);
		}
	}
}

void
gaim_notify_set_ui_ops(GaimNotifyUiOps *ops)
{
	notify_ui_ops = ops;
}

GaimNotifyUiOps *
gaim_notify_get_ui_ops(void)
{
	return notify_ui_ops;
}
