#include "module.h"
#include "../perl-handlers.h"

#include "const-c.inc"

MODULE = Gaim  PACKAGE = Gaim  PREFIX = gaim_
PROTOTYPES: ENABLE

INCLUDE: const-xs.inc

void
timeout_add(plugin, seconds, callback, data)
	Gaim::Plugin plugin
	int seconds
	SV *callback
	SV *data
CODE:
	gaim_perl_timeout_add(plugin, seconds, callback, data);

void
signal_connect(instance, signal, plugin, callback, data)
	void *instance
	const char *signal
	Gaim::Plugin plugin
	SV *callback
	SV *data
CODE:
	gaim_perl_signal_connect(plugin, instance, signal, callback, data);

void
signal_disconnect(instance, signal, plugin)
	void *instance
	const char *signal
	Gaim::Plugin plugin
CODE:
	gaim_perl_signal_disconnect(plugin, instance, signal);

void
gaim_debug(level, category, string)
	Gaim::DebugLevel level
	const char *category
	const char *string

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
	GAIM_PERL_BOOT(Connection);

