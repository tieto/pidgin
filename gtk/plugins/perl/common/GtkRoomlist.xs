#include "gtkmodule.h"

MODULE = Gaim::GtkUI::Roomlist  PACKAGE = Gaim::GtkUI::Roomlist  PREFIX = gaim_gtk_roomlist_
PROTOTYPES: ENABLE

gboolean
gaim_gtk_roomlist_is_showable()

MODULE = Gaim::GtkUI::Roomlist  PACKAGE = Gaim::GtkUI::Roomlist::Dialog  PREFIX = gaim_gtk_roomlist_dialog_
PROTOTYPES: ENABLE

void
gaim_gtk_roomlist_dialog_show(class)
    C_ARGS: /* void */

Gaim::GtkUI::Roomlist::Dialog
gaim_gtk_roomlist_dialog_new(class)
    C_ARGS: /* void */

Gaim::GtkUI::Roomlist::Dialog
gaim_gtk_roomlist_dialog_new_with_account(class, account)
	Gaim::Account account
    C_ARGS:
	account
