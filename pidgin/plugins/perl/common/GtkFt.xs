#include "gtkmodule.h"

MODULE = Pidgin::Xfer  PACKAGE = Pidgin::Xfer  PREFIX = pidgin_
PROTOTYPES: ENABLE

void
pidgin_set_xfer_dialog(dialog)
	Pidgin::Xfer::Dialog dialog

Pidgin::Xfer::Dialog
pidgin_get_xfer_dialog()

MODULE = Pidgin::Xfer  PACKAGE = Pidgin::Xfer::Dialog  PREFIX = pidgin_xfer_dialog_
PROTOTYPES: ENABLE

Pidgin::Xfer::Dialog
pidgin_xfer_dialog_new(class)
    C_ARGS: /* void */

void
pidgin_xfer_dialog_destroy(dialog)
	Pidgin::Xfer::Dialog dialog

void
pidgin_xfer_dialog_show(dialog = NULL)
	Pidgin::Xfer::Dialog dialog

void
pidgin_xfer_dialog_hide(dialog)
	Pidgin::Xfer::Dialog dialog

void
pidgin_xfer_dialog_add_xfer(dialog, xfer)
	Pidgin::Xfer::Dialog dialog
	Purple::Xfer xfer

void
pidgin_xfer_dialog_remove_xfer(dialog, xfer)
	Pidgin::Xfer::Dialog dialog
	Purple::Xfer xfer

void
pidgin_xfer_dialog_cancel_xfer(dialog, xfer)
	Pidgin::Xfer::Dialog dialog
	Purple::Xfer xfer

void
pidgin_xfer_dialog_update_xfer(dialog, xfer)
	Pidgin::Xfer::Dialog dialog
	Purple::Xfer xfer
