#include "module.h"
#include "../perl-handlers.h"
#include "const-c.inc"

/* Prototypes for the BOOT section below. */
GAIM_PERL_BOOT_PROTO(Account);
GAIM_PERL_BOOT_PROTO(Account__Option);
GAIM_PERL_BOOT_PROTO(Buddy__Icon);
GAIM_PERL_BOOT_PROTO(BuddyList);
GAIM_PERL_BOOT_PROTO(Cipher);
GAIM_PERL_BOOT_PROTO(Cmd);
GAIM_PERL_BOOT_PROTO(Connection);
GAIM_PERL_BOOT_PROTO(Conversation);
GAIM_PERL_BOOT_PROTO(Core);
GAIM_PERL_BOOT_PROTO(Debug);
GAIM_PERL_BOOT_PROTO(Xfer);
GAIM_PERL_BOOT_PROTO(ImgStore);
GAIM_PERL_BOOT_PROTO(Log);
GAIM_PERL_BOOT_PROTO(Network);
GAIM_PERL_BOOT_PROTO(Notify);
GAIM_PERL_BOOT_PROTO(Plugin);
GAIM_PERL_BOOT_PROTO(PluginPref);
GAIM_PERL_BOOT_PROTO(Pounce);
GAIM_PERL_BOOT_PROTO(Prefs);
GAIM_PERL_BOOT_PROTO(Privacy);
GAIM_PERL_BOOT_PROTO(Proxy);
GAIM_PERL_BOOT_PROTO(Prpl);
GAIM_PERL_BOOT_PROTO(Request);
GAIM_PERL_BOOT_PROTO(Roomlist);
GAIM_PERL_BOOT_PROTO(SSL);
GAIM_PERL_BOOT_PROTO(SavedStatus);
GAIM_PERL_BOOT_PROTO(Serv);
GAIM_PERL_BOOT_PROTO(Signal);
GAIM_PERL_BOOT_PROTO(Sound);
GAIM_PERL_BOOT_PROTO(Status);
GAIM_PERL_BOOT_PROTO(Stringref);
GAIM_PERL_BOOT_PROTO(Util);
GAIM_PERL_BOOT_PROTO(XMLNode);

MODULE = Gaim  PACKAGE = Gaim  PREFIX = gaim_
PROTOTYPES: ENABLE

INCLUDE: const-xs.inc

BOOT:
	GAIM_PERL_BOOT(Account);
	GAIM_PERL_BOOT(Account__Option);
	GAIM_PERL_BOOT(Buddy__Icon);
	GAIM_PERL_BOOT(BuddyList);
	GAIM_PERL_BOOT(Cipher);
	GAIM_PERL_BOOT(Cmd);
	GAIM_PERL_BOOT(Connection);
	GAIM_PERL_BOOT(Conversation);
	GAIM_PERL_BOOT(Core);
	GAIM_PERL_BOOT(Debug);
	GAIM_PERL_BOOT(Xfer);
	GAIM_PERL_BOOT(ImgStore);
	GAIM_PERL_BOOT(Log);
	GAIM_PERL_BOOT(Network);
	GAIM_PERL_BOOT(Notify);
	GAIM_PERL_BOOT(Plugin);
	GAIM_PERL_BOOT(PluginPref);
	GAIM_PERL_BOOT(Pounce);
	GAIM_PERL_BOOT(Prefs);
	GAIM_PERL_BOOT(Privacy);
	GAIM_PERL_BOOT(Proxy);
	GAIM_PERL_BOOT(Prpl);
	GAIM_PERL_BOOT(Request);
	GAIM_PERL_BOOT(Roomlist);
	GAIM_PERL_BOOT(SSL);
	GAIM_PERL_BOOT(SavedStatus);
	GAIM_PERL_BOOT(Serv);
	GAIM_PERL_BOOT(Signal);
	GAIM_PERL_BOOT(Sound);
	GAIM_PERL_BOOT(Status);
	GAIM_PERL_BOOT(Stringref);
	GAIM_PERL_BOOT(Util);
	GAIM_PERL_BOOT(XMLNode);

void
timeout_add(plugin, seconds, callback, data = 0)
	Gaim::Plugin plugin
	int seconds
	SV *callback
	SV *data
CODE:
	gaim_perl_timeout_add(plugin, seconds, callback, data);

void
deinit()
CODE:
	gaim_perl_timeout_clear();


MODULE = Gaim  PACKAGE = Gaim  PREFIX = gaim_
PROTOTYPES: ENABLE

Gaim::Core
gaim_get_core()
