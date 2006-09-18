#include "gtkmodule.h"

MODULE = Gaim::GtkUI::Roomlist  PACKAGE = Gaim::GtkUI::Roomlist  PREFIX = gaim_gtk_roomlist_
PROTOTYPES: ENABLE

gboolean
gaim_gtk_roomlist_is_showable()

MODULE = Gaim::GtkUI::Roomlist  PACKAGE = Gaim::GtkUI::Roomlist::Dialog  PREFIX = gaim_gtk_roomlist_dialog_
PROTOTYPES: ENABLE

void
gaim_gtk_roomlist_dialog_show()

void
gaim_gtk_roomlist_dialog_show_with_account(account)
	Gaim::Account account
