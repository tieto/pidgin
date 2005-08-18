#include "module.h"
#include "../perl-handlers.h"
#include "const-c.inc"

MODULE = Gaim  PACKAGE = Gaim  PREFIX = gaim_
PROTOTYPES: ENABLE

INCLUDE: const-xs.inc

void
timeout_add(plugin, seconds, callback, data = 0)
	Gaim::Plugin plugin
	int seconds
	SV *callback
	SV *data
CODE:
	gaim_perl_timeout_add(plugin, seconds, callback, data);

void
signal_connect(instance, signal, plugin, callback, data = 0)
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
CODE:
	gaim_perl_timeout_clear();


BOOT:
	GAIM_PERL_BOOT(Account);
	GAIM_PERL_BOOT(Account__Option); 
	GAIM_PERL_BOOT(Buddy__Icon);
	GAIM_PERL_BOOT(BuddyList);
	GAIM_PERL_BOOT(Cipher);
	GAIM_PERL_BOOT(Cmds);
	GAIM_PERL_BOOT(Connection);
	GAIM_PERL_BOOT(Conv);
	GAIM_PERL_BOOT(Xfer);
	GAIM_PERL_BOOT(ImgStore);
	GAIM_PERL_BOOT(Log);
	GAIM_PERL_BOOT(Network);
	GAIM_PERL_BOOT(Notify);
	GAIM_PERL_BOOT(Plugin);
	GAIM_PERL_BOOT(Pref); 
	GAIM_PERL_BOOT(Pounce);
	GAIM_PERL_BOOT(Prefs);
	GAIM_PERL_BOOT(Privacy);
	GAIM_PERL_BOOT(Proxy);
	GAIM_PERL_BOOT(Prpl);
	GAIM_PERL_BOOT(Request);
	GAIM_PERL_BOOT(Roomlist);
	GAIM_PERL_BOOT(SSL);
	GAIM_PERL_BOOT(SavedStatus);
	GAIM_PERL_BOOT(Sound);
	GAIM_PERL_BOOT(Status);
	GAIM_PERL_BOOT(Stringref);
	GAIM_PERL_BOOT(Util);
	GAIM_PERL_BOOT(XMLNode); 
