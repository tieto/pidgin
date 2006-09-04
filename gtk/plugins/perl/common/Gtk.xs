#include "gtkmodule.h"

/*
#define GAIM_PERL_BOOT_PROTO(x) \
	void boot_Gaim__##x(pTHX_ CV *cv);

#define GAIM_PERL_BOOT(x) \
	gaim_perl_callXS(boot_Gaim__##x, cv, mark)

static void
gaim_perl_callXS(void (*subaddr)(pTHX_ CV *cv), CV *cv, SV **mark)
{
	dSP;

	PUSHMARK(mark);
	(*subaddr)(aTHX_ cv);

	PUTBACK;
}
*/

/* Prototypes for the BOOT section below. */
GAIM_PERL_BOOT_PROTO(Gtk__Account);
GAIM_PERL_BOOT_PROTO(Gtk__BuddyList);
GAIM_PERL_BOOT_PROTO(Gtk__Connection);
GAIM_PERL_BOOT_PROTO(Gtk__Conversation);
GAIM_PERL_BOOT_PROTO(Gtk__Conversation__Window);
GAIM_PERL_BOOT_PROTO(Gtk__Debug);
GAIM_PERL_BOOT_PROTO(Gtk__Dialogs);
GAIM_PERL_BOOT_PROTO(Gtk__IMHtml);
GAIM_PERL_BOOT_PROTO(Gtk__IMHtmlToolbar);
GAIM_PERL_BOOT_PROTO(Gtk__Log);
GAIM_PERL_BOOT_PROTO(Gtk__MenuTray);
GAIM_PERL_BOOT_PROTO(Gtk__Plugin);
GAIM_PERL_BOOT_PROTO(Gtk__PluginPref);
GAIM_PERL_BOOT_PROTO(Gtk__Pounce);
GAIM_PERL_BOOT_PROTO(Gtk__Prefs);
GAIM_PERL_BOOT_PROTO(Gtk__Privacy);
GAIM_PERL_BOOT_PROTO(Gtk__Roomlist);
GAIM_PERL_BOOT_PROTO(Gtk__Status);
#ifndef _WIN32
GAIM_PERL_BOOT_PROTO(Gtk__Session);
#endif
GAIM_PERL_BOOT_PROTO(Gtk__Sound);
GAIM_PERL_BOOT_PROTO(Gtk__StatusBox);
GAIM_PERL_BOOT_PROTO(Gtk__Themes);
GAIM_PERL_BOOT_PROTO(Gtk__Utils);
GAIM_PERL_BOOT_PROTO(Gtk__Xfer);

MODULE = Gaim::Gtk  PACKAGE = Gaim::Gtk PREFIX = gaim_gtk_
PROTOTYPES: ENABLE

BOOT:
	GAIM_PERL_BOOT(Gtk__Debug);
GAIM_PERL_BOOT(Gtk__Account);
GAIM_PERL_BOOT(Gtk__BuddyList);
GAIM_PERL_BOOT(Gtk__Connection);
GAIM_PERL_BOOT(Gtk__Conversation);
GAIM_PERL_BOOT(Gtk__Conversation__Window);
GAIM_PERL_BOOT(Gtk__Debug);
GAIM_PERL_BOOT(Gtk__Dialogs);
GAIM_PERL_BOOT(Gtk__IMHtml);
GAIM_PERL_BOOT(Gtk__IMHtmlToolbar);
GAIM_PERL_BOOT(Gtk__Log);
GAIM_PERL_BOOT(Gtk__MenuTray);
GAIM_PERL_BOOT(Gtk__Plugin);
GAIM_PERL_BOOT(Gtk__PluginPref);
GAIM_PERL_BOOT(Gtk__Pounce);
GAIM_PERL_BOOT(Gtk__Prefs);
GAIM_PERL_BOOT(Gtk__Privacy);
GAIM_PERL_BOOT(Gtk__Roomlist);
GAIM_PERL_BOOT(Gtk__Status);
#ifndef _WIN32
GAIM_PERL_BOOT(Gtk__Session);
#endif
GAIM_PERL_BOOT(Gtk__Sound);
GAIM_PERL_BOOT(Gtk__StatusBox);
GAIM_PERL_BOOT(Gtk__Themes);
GAIM_PERL_BOOT(Gtk__Utils);
GAIM_PERL_BOOT(Gtk__Xfer);
