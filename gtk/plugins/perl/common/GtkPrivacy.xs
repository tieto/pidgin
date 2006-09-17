#include "gtkmodule.h"

MODULE = Gaim::GtkUI::Privacy  PACKAGE = Gaim::GtkUI::Privacy  PREFIX = gaim_gtk_
PROTOTYPES: ENABLE

void
gaim_gtk_request_add_permit(account, name)
	Gaim::Account account
	const char * name

void
gaim_gtk_request_add_block(account, name)
	Gaim::Account account
	const char * name

MODULE = Gaim::GtkUI::Privacy  PACKAGE = Gaim::GtkUI::Privacy::Dialog  PREFIX = gaim_gtk_privacy_dialog_
PROTOTYPES: ENABLE

void
gaim_gtk_privacy_dialog_show()

void
gaim_gtk_privacy_dialog_hide()
