#include "module.h"

MODULE = Gaim::Notify  PACKAGE = Gaim::Notify  PREFIX = gaim_notify_
PROTOTYPES: ENABLE

void
gaim_notify_close(type, ui_handle)
	Gaim::NotifyType type
	void * ui_handle

void
gaim_notify_close_with_handle(handle)
	void * handle

void *
gaim_notify_email(handle, subject, from, to, url, cb, user_data)
	void * handle
	const char *subject
	const char *from
	const char *to
	const char *url
	Gaim::NotifyCloseCallback cb
	gpointer user_data

void *
gaim_notify_emails(handle, count, detailed, subjects, froms, tos, urls, cb, user_data)
	void * handle
	size_t count
	gboolean detailed
	const char **subjects
	const char **froms
	const char **tos
	const char **urls
	Gaim::NotifyCloseCallback cb
	gpointer user_data

void *
gaim_notify_formatted(handle, title, primary, secondary, text, cb, user_data)
	void * handle
	const char *title
	const char *primary
	const char *secondary
	const char *text
	Gaim::NotifyCloseCallback cb
	gpointer user_data

void *
gaim_notify_userinfo(gc, who, user_info, cb, user_data)
	Gaim::Connection gc
	const char *who
	Gaim::NotifyUserInfo user_info
	Gaim::NotifyCloseCallback cb
	gpointer user_data

Gaim::NotifyUserInfo
gaim_notify_user_info_new()

void
gaim_notify_user_info_destroy(user_info)
	Gaim::NotifyUserInfo user_info

void
gaim_notify_user_info_get_entries(user_info)
	Gaim::NotifyUserInfo user_info
PREINIT:
	const GList *l;
PPCODE:
	l = gaim_notify_user_info_get_entries(user_info);
	for (; l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::NotifyUserInfoEntry")));
	}

gchar_own *
gaim_notify_user_info_get_text_with_newline(user_info, newline)
	Gaim::NotifyUserInfo user_info
	const char *newline

void gaim_notify_user_info_add_pair(user_info, label, value)
	Gaim::NotifyUserInfo user_info
	const char *label
	const char *value

void gaim_notify_user_info_prepend_pair(user_info, label, value)
	Gaim::NotifyUserInfo user_info
	const char *label
	const char *value

void gaim_notify_user_info_add_section_break(user_info)
	Gaim::NotifyUserInfo user_info

void gaim_notify_user_info_add_section_header(user_info, label)
	Gaim::NotifyUserInfo user_info
	const char *label

void gaim_notify_user_info_remove_last_item(user_info)
	Gaim::NotifyUserInfo user_info

gchar *
gaim_notify_user_info_entry_get_label(user_info_entry)
	Gaim::NotifyUserInfoEntry user_info_entry

gchar *
gaim_notify_user_info_entry_get_value(user_info_entry)
	Gaim::NotifyUserInfoEntry user_info_entry

Gaim::NotifyUiOps
gaim_notify_get_ui_ops()


void *
gaim_notify_message(handle, type, title, primary, secondary, cb, user_data)
	void * handle
	Gaim::NotifyMsgType type
	const char *title
	const char *primary
	const char *secondary
	Gaim::NotifyCloseCallback cb
	gpointer user_data

void *
gaim_notify_searchresults(gc, title, primary, secondary, results, cb, user_data)
	Gaim::Connection gc
	const char *title
	const char *primary
	const char *secondary
	Gaim::NotifySearchResults results
	Gaim::NotifyCloseCallback cb
	gpointer user_data

void
gaim_notify_set_ui_ops(ops)
	Gaim::NotifyUiOps ops

void *
gaim_notify_uri(handle, uri)
	void * handle
	const char *uri

