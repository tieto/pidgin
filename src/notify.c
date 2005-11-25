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
					const char *secondary, GHookFunc cb, gpointer user_data)
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
				  const char *to, const char *url, GHookFunc cb,
				  gpointer user_data)
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
				   GHookFunc cb, gpointer user_data)
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
					  GHookFunc cb, gpointer user_data)
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

void *
gaim_notify_searchresults(GaimConnection *gc, const char *title,
						  const char *primary, const char *secondary,
						  GaimNotifySearchResults *results, GHookFunc cb, gpointer user_data)
{
	GaimNotifyUiOps *ops;

	ops = gaim_notify_get_ui_ops();

	if (ops != NULL && ops->notify_searchresults != NULL) {
		GaimNotifyInfo *info;

		info            = g_new0(GaimNotifyInfo, 1);
		info->type      = GAIM_NOTIFY_SEARCHRESULTS;
		info->handle    = gc;
		info->ui_handle = ops->notify_searchresults(gc, title, primary,
													secondary, results,
													cb, user_data);

		handles = g_list_append(handles, info);

		return info->ui_handle;
	}

	return NULL;
}

void
gaim_notify_searchresults_free(GaimNotifySearchResults *results)
{
	GList *l, *m;

	g_return_if_fail(results != NULL);

	for (l = results->buttons; l != NULL; l = l->next) {
		GaimNotifySearchButton *button = l->data;

		results->buttons = g_list_remove(results->buttons, button);
		g_free(button);
	}
	g_list_free(results->buttons);

	for (l = results->rows; l != NULL; l = l->next) {
		GList *row = l->data;

		for (m = row; m != NULL; m = m->next) {
			gchar *str = m->data;

			m = g_list_remove(m, str);
			g_free(str);
		}

		results->rows = g_list_remove(results->rows, row);
		g_list_free(row);
	}
	g_list_free(results->rows);

	for (l = results->columns; l != NULL; l = l->next) {
		GaimNotifySearchColumn *column = l->data;

		results->columns = g_list_remove(results->columns, column);
		g_free(column->title);
		g_free(column);
	}
	g_list_free(results->columns);
}

void
gaim_notify_searchresults_new_rows(GaimConnection *gc,
		GaimNotifySearchResults *results,
		void *data, gpointer user_data)
{
	GaimNotifyUiOps *ops;

	ops = gaim_notify_get_ui_ops();

	if (ops != NULL && ops->notify_searchresults != NULL) {
		ops->notify_searchresults_new_rows(gc, results, data, user_data);
	}
}

void
gaim_notify_searchresults_button_add(GaimNotifySearchResults *results,
									 GaimNotifySearchButtonType type,
									 GaimNotifySearchResultsCallback cb)
{
	GaimNotifySearchButton *button;

	g_return_if_fail(results != NULL);
	g_return_if_fail(cb != NULL);

	button = g_new0(GaimNotifySearchButton, 1);
	button->callback = cb;
	button->type = type;

	results->buttons = g_list_append(results->buttons, button);
}

GaimNotifySearchResults *
gaim_notify_searchresults_new()
{
	GaimNotifySearchResults *rs = g_new0(GaimNotifySearchResults, 1);

	return rs;
}

void
gaim_notify_searchresults_column_add(GaimNotifySearchResults *results,
									 GaimNotifySearchColumn *column)
{
	g_return_if_fail(results != NULL);
	g_return_if_fail(column  != NULL);

	results->columns = g_list_append(results->columns, column);
}

void gaim_notify_searchresults_row_add(GaimNotifySearchResults *results,
									   GList *row)
{
	g_return_if_fail(results != NULL);
	g_return_if_fail(row     != NULL);

	results->rows = g_list_append(results->rows, row);
}

GaimNotifySearchColumn *
gaim_notify_searchresults_column_new(const char *title)
{
	GaimNotifySearchColumn *sc;

	g_return_val_if_fail(title != NULL, NULL);

	sc = g_new0(GaimNotifySearchColumn, 1);
	sc->title = g_strdup(title);

	return sc;
}

int
gaim_notify_searchresults_get_columns_count(GaimNotifySearchResults *results)
{
	g_return_val_if_fail(results != NULL, -1);

	return g_list_length(results->columns);
}

int
gaim_notify_searchresults_get_rows_count(GaimNotifySearchResults *results)
{
	g_return_val_if_fail(results != NULL, -1);

	return g_list_length(results->rows);
}

char *
gaim_notify_searchresults_column_get_title(GaimNotifySearchResults *results,
										   unsigned int column_id)
{
	g_return_val_if_fail(results != NULL, NULL);

	return ((GaimNotifySearchColumn *)g_list_nth_data(results->columns, column_id))->title;
}

GList *
gaim_notify_searchresults_row_get(GaimNotifySearchResults *results,
								  unsigned int row_id)
{
	g_return_val_if_fail(results != NULL, NULL);

	return g_list_nth_data(results->rows, row_id);
}

void *
gaim_notify_userinfo(GaimConnection *gc, const char *who,
						   const char *text, GHookFunc cb, gpointer user_data)
{
	GaimNotifyUiOps *ops;

	g_return_val_if_fail(who != NULL, NULL);

	ops = gaim_notify_get_ui_ops();

	if (ops != NULL && ops->notify_userinfo != NULL) {
		GaimNotifyInfo *info;
		char *infotext = g_strdup(text);

		info            = g_new0(GaimNotifyInfo, 1);
		info->type      = GAIM_NOTIFY_USERINFO;
		info->handle    = gc;

		gaim_signal_emit(gaim_notify_get_handle(), "displaying-userinfo",
						 gaim_connection_get_account(gc), who, &infotext);

		info->ui_handle = ops->notify_userinfo(gc, who,
											   infotext, cb, user_data);

		handles = g_list_append(handles, info);

		g_free(infotext);
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

void *
gaim_notify_get_handle(void)
{
	static int handle;

	return &handle;
}

void
gaim_notify_init(void)
{
	gpointer handle = gaim_notify_get_handle();

	gaim_signal_register(handle, "displaying-userinfo",
						 gaim_marshal_VOID__POINTER_POINTER_POINTER, NULL, 3,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT),
						 gaim_value_new(GAIM_TYPE_STRING),
						 gaim_value_new_outgoing(GAIM_TYPE_STRING));
}

void
gaim_notify_uninit(void)
{
	gaim_signals_unregister_by_instance(gaim_notify_get_handle());
}
