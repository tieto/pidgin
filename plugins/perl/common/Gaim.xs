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
debug(category, string)
	const char *category
	const char *string
CODE:
	gaim_debug(GAIM_DEBUG_INFO, category, string);

void
deinit()
PREINIT:
	GList *l;
CODE:
	gaim_perl_timeout_clear();


BOOT:
	GAIM_PERL_BOOT(Account);

