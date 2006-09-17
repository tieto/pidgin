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
GAIM_PERL_BOOT_PROTO(GtkUI__Account);
GAIM_PERL_BOOT_PROTO(GtkUI__BuddyList);
GAIM_PERL_BOOT_PROTO(GtkUI__Connection);
GAIM_PERL_BOOT_PROTO(GtkUI__Conversation);
GAIM_PERL_BOOT_PROTO(GtkUI__Conversation__Window);
GAIM_PERL_BOOT_PROTO(GtkUI__Debug);
GAIM_PERL_BOOT_PROTO(GtkUI__Dialogs);
GAIM_PERL_BOOT_PROTO(GtkUI__IMHtml);
GAIM_PERL_BOOT_PROTO(GtkUI__IMHtmlToolbar);
GAIM_PERL_BOOT_PROTO(GtkUI__Log);
GAIM_PERL_BOOT_PROTO(GtkUI__MenuTray);
GAIM_PERL_BOOT_PROTO(GtkUI__Plugin);
GAIM_PERL_BOOT_PROTO(GtkUI__PluginPref);
GAIM_PERL_BOOT_PROTO(GtkUI__Pounce);
GAIM_PERL_BOOT_PROTO(GtkUI__Prefs);
GAIM_PERL_BOOT_PROTO(GtkUI__Privacy);
GAIM_PERL_BOOT_PROTO(GtkUI__Roomlist);
GAIM_PERL_BOOT_PROTO(GtkUI__Status);
#ifndef _WIN32
GAIM_PERL_BOOT_PROTO(GtkUI__Session);
#endif
GAIM_PERL_BOOT_PROTO(GtkUI__Sound);
GAIM_PERL_BOOT_PROTO(GtkUI__StatusBox);
GAIM_PERL_BOOT_PROTO(GtkUI__Themes);
GAIM_PERL_BOOT_PROTO(GtkUI__Utils);
GAIM_PERL_BOOT_PROTO(GtkUI__Xfer);

MODULE = Gaim::Gtk  PACKAGE = Gaim::Gtk PREFIX = gaim_gtk_
PROTOTYPES: ENABLE

BOOT:
GAIM_PERL_BOOT(GtkUI__Account);
GAIM_PERL_BOOT(GtkUI__BuddyList);
GAIM_PERL_BOOT(GtkUI__Connection);
GAIM_PERL_BOOT(GtkUI__Conversation);
GAIM_PERL_BOOT(GtkUI__Conversation__Window);
GAIM_PERL_BOOT(GtkUI__Debug);
GAIM_PERL_BOOT(GtkUI__Dialogs);
GAIM_PERL_BOOT(GtkUI__IMHtml);
GAIM_PERL_BOOT(GtkUI__IMHtmlToolbar);
GAIM_PERL_BOOT(GtkUI__Log);
GAIM_PERL_BOOT(GtkUI__MenuTray);
GAIM_PERL_BOOT(GtkUI__Plugin);
GAIM_PERL_BOOT(GtkUI__PluginPref);
GAIM_PERL_BOOT(GtkUI__Pounce);
GAIM_PERL_BOOT(GtkUI__Prefs);
GAIM_PERL_BOOT(GtkUI__Privacy);
GAIM_PERL_BOOT(GtkUI__Roomlist);
GAIM_PERL_BOOT(GtkUI__Status);
#ifndef _WIN32
GAIM_PERL_BOOT(GtkUI__Session);
#endif
GAIM_PERL_BOOT(GtkUI__Sound);
GAIM_PERL_BOOT(GtkUI__StatusBox);
GAIM_PERL_BOOT(GtkUI__Themes);
GAIM_PERL_BOOT(GtkUI__Utils);
GAIM_PERL_BOOT(GtkUI__Xfer);
