#include "module.h"

MODULE = Gaim::Log  PACKAGE = Gaim::Log  PREFIX = gaim_log_
PROTOTYPES: ENABLE

int
gaim_log_common_sizer(log)
	Gaim::Log log

void
gaim_log_common_writer(log, ext)
	Gaim::Log log
	const char *ext

gint
gaim_log_compare(y, z)
	gconstpointer y
	gconstpointer z

void
gaim_log_free(log)
	Gaim::Log log

gchar_own *
gaim_log_get_log_dir(type, name, account)
	Gaim::LogType type
	const char *name
	Gaim::Account account

void
gaim_log_get_log_sets()
PREINIT:
	GHashTable *l;
PPCODE:
	l = gaim_log_get_log_sets();
	XPUSHs(sv_2mortal(gaim_perl_bless_object(l, "GHashTable")));

void
gaim_log_get_logs(type, name, account)
	Gaim::LogType type
	const char *name
	Gaim::Account account
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_log_get_logs(type, name, account); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::ListEntry")));
	}

int
gaim_log_get_size(log)
	Gaim::Log log

void
gaim_log_get_system_logs(account)
	Gaim::Account account
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_log_get_system_logs(account); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::ListEntry")));
	}

int
gaim_log_get_total_size(type, name, account)
	Gaim::LogType type
	const char *name
	Gaim::Account account

void
gaim_log_init()

void
gaim_log_logger_free(logger)
	Gaim::Log::Logger logger

void
gaim_log_logger_get_options()
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_log_logger_get_options(); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::ListEntry")));
	}

gchar_own *
gaim_log_read(log, flags)
	Gaim::Log log
	Gaim::Log::ReadFlags flags

gint
gaim_log_set_compare(y, z)
	gconstpointer y
	gconstpointer z
