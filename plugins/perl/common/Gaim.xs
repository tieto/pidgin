#include "module.h"
#include "../perl-handlers.h"

MODULE = Gaim  PACKAGE = Gaim
PROTOTYPES: ENABLE

void
timeout_add(plugin, seconds, func, arg)
	Gaim::Plugin plugin
	int seconds
	const char *func
	void *arg
CODE:
	gaim_perl_timeout_add(plugin, seconds, func, arg);

void
debug(level, category, string)
	const char *level
	const char *category
	const char *string
CODE:
	if (!strcmp(level, "misc"))
		gaim_debug(GAIM_DEBUG_MISC, category, string);
	else if (!strcmp(level, "info"))
		gaim_debug(GAIM_DEBUG_INFO, category, string);
	else if (!strcmp(level, "warning"))
		gaim_debug(GAIM_DEBUG_WARNING, category, string);
	else if (!strcmp(level, "error"))
		gaim_debug(GAIM_DEBUG_ERROR, category, string);
	else if (!strcmp(level, "fatal"))
		gaim_debug(GAIM_DEBUG_FATAL, category, string);
	else
		croak("Unknown debug level type '%s'", level);

void
signal_connect(instance, signal, plugin, func, data)
	void *instance
	const char *signal
	Gaim::Plugin plugin
	const char *func
	void *data
CODE:
	gaim_perl_signal_connect(plugin, instance, signal, func, data);

void
signal_disconnect(instance, signal, plugin, func)
	void *instance
	const char *signal
	Gaim::Plugin plugin
	const char *func
CODE:
	gaim_perl_signal_disconnect(plugin, instance, signal, func);

void
debug_misc(category, string)
	const char *category
	const char *string
CODE:
	gaim_debug(GAIM_DEBUG_MISC, category, string);

void
debug_info(category, string)
	const char *category
	const char *string
CODE:
	gaim_debug(GAIM_DEBUG_INFO, category, string);

void
debug_warning(category, string)
	const char *category
	const char *string
CODE:
	gaim_debug(GAIM_DEBUG_WARNING, category, string);

void
debug_error(category, string)
	const char *category
	const char *string
CODE:
	gaim_debug(GAIM_DEBUG_ERROR, category, string);

void
debug_fatal(category, string)
	const char *category
	const char *string
CODE:
	gaim_debug(GAIM_DEBUG_FATAL, category, string);

void
deinit()
PREINIT:
	GList *l;
CODE:
	gaim_perl_timeout_clear();


BOOT:
	GAIM_PERL_BOOT(Account);
	GAIM_PERL_BOOT(BuddyList);
	GAIM_PERL_BOOT(BuddyList__Group);
	GAIM_PERL_BOOT(BuddyList__Buddy);
	GAIM_PERL_BOOT(BuddyList__Chat);

