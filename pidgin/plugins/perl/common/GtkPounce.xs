#include "gtkmodule.h"

MODULE = Pidgin::Pounce  PACKAGE = Pidgin::Pounce  PREFIX = pidgin_pounce_
PROTOTYPES: ENABLE

void
pidgin_pounce_editor_show(account, name, cur_pounce)
	Purple::Account account
	const char * name
	Purple::Pounce cur_pounce

void
pidgin_pounce_editor_show_with_parent(parent, account, name, cur_pounce)
	void * parent
	Purple::Account account
	const char * name
	Purple::Pounce cur_pounce

MODULE = Pidgin::Pounce  PACKAGE = Pidgin::Pounces  PREFIX = pidgin_pounces_
PROTOTYPES: ENABLE

Purple::Handle
pidgin_pounces_get_handle()

MODULE = Pidgin::Pounce  PACKAGE = Pidgin::Pounces::Manager  PREFIX = pidgin_pounces_manager_
PROTOTYPES: ENABLE

void
pidgin_pounces_manager_show()

void
pidgin_pounces_manager_show_with_parent(parent)
	void * parent

void
pidgin_pounces_manager_hide()
