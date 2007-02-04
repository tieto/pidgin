#include "gtkmodule.h"

MODULE = Pidgin::Roomlist  PACKAGE = Pidgin::Roomlist  PREFIX = pidgin_roomlist_
PROTOTYPES: ENABLE

gboolean
pidgin_roomlist_is_showable()

MODULE = Pidgin::Roomlist  PACKAGE = Pidgin::Roomlist::Dialog  PREFIX = pidgin_roomlist_dialog_
PROTOTYPES: ENABLE

void
pidgin_roomlist_dialog_show()

void
pidgin_roomlist_dialog_show_with_account(account)
	Gaim::Account account
