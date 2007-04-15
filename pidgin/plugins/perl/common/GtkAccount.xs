#include "gtkmodule.h"

MODULE = Pidgin::Account  PACKAGE = Pidgin::Account  PREFIX = pidgin_account_
PROTOTYPES: ENABLE

Purple::Handle
pidgin_account_get_handle()

MODULE = Pidgin::Account  PACKAGE = Pidgin::Account::Dialog  PREFIX = pidgin_account_dialog_
PROTOTYPES: ENABLE

void
pidgin_account_dialog_show(type, account)
	Pidgin::Account::Dialog::Type type
	Purple::Account account

MODULE = Pidgin::Account  PACKAGE = Pidgin::Account::Window  PREFIX = pidgin_accounts_window_
PROTOTYPES: ENABLE

void
pidgin_accounts_window_show()

void
pidgin_accounts_window_hide()
