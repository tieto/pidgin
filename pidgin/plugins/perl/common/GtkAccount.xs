#include "gtkmodule.h"

MODULE = Gaim::GtkUI::Account  PACKAGE = Gaim::GtkUI::Account  PREFIX = gaim_gtk_account_
PROTOTYPES: ENABLE

Gaim::Handle
gaim_gtk_account_get_handle()

MODULE = Gaim::GtkUI::Account  PACKAGE = Gaim::GtkUI::Account::Dialog  PREFIX = gaim_gtk_account_dialog_
PROTOTYPES: ENABLE

void
gaim_gtk_account_dialog_show(type, account)
	Gaim::GtkUI::Account::Dialog::Type type
	Gaim::Account account

MODULE = Gaim::GtkUI::Account  PACKAGE = Gaim::GtkUI::Account::Window  PREFIX = gaim_gtk_accounts_window_
PROTOTYPES: ENABLE

void
gaim_gtk_accounts_window_show()

void
gaim_gtk_accounts_window_hide()
