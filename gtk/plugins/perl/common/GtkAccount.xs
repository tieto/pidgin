#include "gtkmodule.h"

MODULE = Gaim::Gtk::Account  PACKAGE = Gaim::Gtk::Account  PREFIX = gaim_gtk_account_
PROTOTYPES: ENABLE

void *
gaim_gtk_account_get_handle()

MODULE = Gaim::Gtk::Account  PACKAGE = Gaim::Gtk::Account::Dialog  PREFIX = gaim_gtk_account_dialog_
PROTOTYPES: ENABLE

void
gaim_gtk_account_dialog_show(type, account)
	Gaim::Gtk::Account::Dialog::Type type
	Gaim::Account account

MODULE = Gaim::Gtk::Account  PACKAGE = Gaim::Gtk::Account::Window  PREFIX = gaim_gtk_accounts_window_
PROTOTYPES: ENABLE

void
gaim_gtk_accounts_window_show()

void
gaim_gtk_accounts_window_hide()
