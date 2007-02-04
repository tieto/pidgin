#include "gtkmodule.h"

MODULE = Pidgin::Pounce  PACKAGE = Pidgin::Pounce  PREFIX = pidgin_pounce_
PROTOTYPES: ENABLE

void
pidgin_pounce_editor_show(account, name, cur_pounce)
	Gaim::Account account
	const char * name
	Gaim::Pounce cur_pounce

MODULE = Pidgin::Pounce  PACKAGE = Pidgin::Pounces  PREFIX = pidgin_pounces_
PROTOTYPES: ENABLE

Gaim::Handle
pidgin_pounces_get_handle()

MODULE = Pidgin::Pounce  PACKAGE = Pidgin::Pounces::Manager  PREFIX = pidgin_pounces_manager_
PROTOTYPES: ENABLE

void
pidgin_pounces_manager_show()

void
pidgin_pounces_manager_hide()
