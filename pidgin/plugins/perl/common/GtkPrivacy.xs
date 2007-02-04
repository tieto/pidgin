#include "gtkmodule.h"

MODULE = Pidgin::Privacy  PACKAGE = Pidgin::Privacy  PREFIX = pidgin_
PROTOTYPES: ENABLE

void
pidgin_request_add_permit(account, name)
	Gaim::Account account
	const char * name

void
pidgin_request_add_block(account, name)
	Gaim::Account account
	const char * name

MODULE = Pidgin::Privacy  PACKAGE = Pidgin::Privacy::Dialog  PREFIX = pidgin_privacy_dialog_
PROTOTYPES: ENABLE

void
pidgin_privacy_dialog_show()

void
pidgin_privacy_dialog_hide()
