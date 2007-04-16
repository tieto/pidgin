#include "module.h"

MODULE = Purple::Log  PACKAGE = Purple::Log  PREFIX = purple_log_
PROTOTYPES: ENABLE

int
purple_log_common_sizer(log)
	Purple::Log log

void
purple_log_common_writer(log, ext)
	Purple::Log log
	const char *ext

gint
purple_log_compare(y, z)
	gconstpointer y
	gconstpointer z

void
purple_log_free(log)
	Purple::Log log

gchar_own *
purple_log_get_log_dir(type, name, account)
	Purple::LogType type
	const char *name
	Purple::Account account

void
purple_log_get_log_sets()
PREINIT:
	GHashTable *l;
PPCODE:
	l = purple_log_get_log_sets();
	XPUSHs(sv_2mortal(purple_perl_bless_object(l, "GHashTable")));

void
purple_log_get_logs(type, name, account)
	Purple::LogType type
	const char *name
	Purple::Account account
PREINIT:
	GList *l;
PPCODE:
	for (l = purple_log_get_logs(type, name, account); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(purple_perl_bless_object(l->data, "Purple::ListEntry")));
	}

int
purple_log_get_size(log)
	Purple::Log log

void
purple_log_get_system_logs(account)
	Purple::Account account
PREINIT:
	GList *l;
PPCODE:
	for (l = purple_log_get_system_logs(account); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(purple_perl_bless_object(l->data, "Purple::ListEntry")));
	}

int
purple_log_get_total_size(type, name, account)
	Purple::LogType type
	const char *name
	Purple::Account account

void
purple_log_init()

void
purple_log_logger_free(logger)
	Purple::Log::Logger logger

void
purple_log_logger_get_options()
PREINIT:
	GList *l;
PPCODE:
	for (l = purple_log_logger_get_options(); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(purple_perl_bless_object(l->data, "Purple::ListEntry")));
	}

gchar_own *
purple_log_read(log, flags)
	Purple::Log log
	Purple::Log::ReadFlags flags

gint
purple_log_set_compare(y, z)
	gconstpointer y
	gconstpointer z
