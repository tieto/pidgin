#include "module.h"

MODULE = Purple::Notify  PACKAGE = Purple::Notify  PREFIX = purple_notify_
PROTOTYPES: ENABLE

void
purple_notify_close(type, ui_handle)
	Purple::NotifyType type
	void * ui_handle

void
purple_notify_close_with_handle(handle)
	void * handle

void *
purple_notify_email(handle, subject, from, to, url, cb, user_data)
	void * handle
	const char *subject
	const char *from
	const char *to
	const char *url
	Purple::NotifyCloseCallback cb
	gpointer user_data

void *
purple_notify_emails(handle, count, detailed, subjects, froms, tos, urls, cb, user_data)
	void * handle
	size_t count
	gboolean detailed
	const char **subjects
	const char **froms
	const char **tos
	const char **urls
	Purple::NotifyCloseCallback cb
	gpointer user_data

void *
purple_notify_formatted(handle, title, primary, secondary, text, cb, user_data)
	void * handle
	const char *title
	const char *primary
	const char *secondary
	const char *text
	Purple::NotifyCloseCallback cb
	gpointer user_data

void *
purple_notify_userinfo(gc, who, user_info, cb, user_data)
	Purple::Connection gc
	const char *who
	Purple::NotifyUserInfo user_info
	Purple::NotifyCloseCallback cb
	gpointer user_data

Purple::NotifyUserInfo
purple_notify_user_info_new()

void
purple_notify_user_info_destroy(user_info)
	Purple::NotifyUserInfo user_info

void
purple_notify_user_info_get_entries(user_info)
	Purple::NotifyUserInfo user_info
PREINIT:
	const GList *l;
PPCODE:
	l = purple_notify_user_info_get_entries(user_info);
	for (; l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(purple_perl_bless_object(l->data, "Purple::NotifyUserInfoEntry")));
	}

gchar_own *
purple_notify_user_info_get_text_with_newline(user_info, newline)
	Purple::NotifyUserInfo user_info
	const char *newline

void purple_notify_user_info_add_pair(user_info, label, value)
	Purple::NotifyUserInfo user_info
	const char *label
	const char *value

void purple_notify_user_info_prepend_pair(user_info, label, value)
	Purple::NotifyUserInfo user_info
	const char *label
	const char *value

void purple_notify_user_info_add_section_break(user_info)
	Purple::NotifyUserInfo user_info

void purple_notify_user_info_add_section_header(user_info, label)
	Purple::NotifyUserInfo user_info
	const char *label

void purple_notify_user_info_remove_last_item(user_info)
	Purple::NotifyUserInfo user_info

gchar *
purple_notify_user_info_entry_get_label(user_info_entry)
	Purple::NotifyUserInfoEntry user_info_entry

gchar *
purple_notify_user_info_entry_get_value(user_info_entry)
	Purple::NotifyUserInfoEntry user_info_entry

Purple::NotifyUiOps
purple_notify_get_ui_ops()


void *
purple_notify_message(handle, type, title, primary, secondary, cb, user_data)
	void * handle
	Purple::NotifyMsgType type
	const char *title
	const char *primary
	const char *secondary
	Purple::NotifyCloseCallback cb
	gpointer user_data

void *
purple_notify_searchresults(gc, title, primary, secondary, results, cb, user_data)
	Purple::Connection gc
	const char *title
	const char *primary
	const char *secondary
	Purple::NotifySearchResults results
	Purple::NotifyCloseCallback cb
	gpointer user_data

void
purple_notify_set_ui_ops(ops)
	Purple::NotifyUiOps ops

void *
purple_notify_uri(handle, uri)
	void * handle
	const char *uri

