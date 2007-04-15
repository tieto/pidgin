#include "gtkmodule.h"

MODULE = Pidgin::Xfer  PACKAGE = Pidgin::Xfer  PREFIX = pidgin_xfer_
PROTOTYPES: ENABLE

void
pidgin_set_xfer_dialog(dialog)
	Pidgin::Xfer::Dialog dialog

Pidgin::Xfer::Dialog
pidgin_get_xfer_dialog()

MODULE = Pidgin::Xfer  PACKAGE = Pidgin::Xfer::Dialog  PREFIX = pidginxfer_dialog_
PROTOTYPES: ENABLE

Pidgin::Xfer::Dialog
pidginxfer_dialog_new(class)
    C_ARGS: /* void */

void
pidginxfer_dialog_destroy(dialog)
	Pidgin::Xfer::Dialog dialog

void
pidginxfer_dialog_show(dialog = NULL)
	Pidgin::Xfer::Dialog dialog

void
pidginxfer_dialog_hide(dialog)
	Pidgin::Xfer::Dialog dialog

void
pidginxfer_dialog_add_xfer(dialog, xfer)
	Pidgin::Xfer::Dialog dialog
	Purple::Xfer xfer

void
pidginxfer_dialog_remove_xfer(dialog, xfer)
	Pidgin::Xfer::Dialog dialog
	Purple::Xfer xfer

void
pidginxfer_dialog_cancel_xfer(dialog, xfer)
	Pidgin::Xfer::Dialog dialog
	Purple::Xfer xfer

void
pidginxfer_dialog_update_xfer(dialog, xfer)
	Pidgin::Xfer::Dialog dialog
	Purple::Xfer xfer
