#include "gtkmodule.h"

MODULE = Gaim::GtkUI::Xfer  PACKAGE = Gaim::GtkUI::Xfer  PREFIX = gaim_gtk_xfer_
PROTOTYPES: ENABLE

void
gaim_set_gtkxfer_dialog(dialog)
	Gaim::GtkUI::Xfer::Dialog dialog

Gaim::GtkUI::Xfer::Dialog
gaim_get_gtkxfer_dialog()

MODULE = Gaim::GtkUI::Xfer  PACKAGE = Gaim::GtkUI::Xfer::Dialog  PREFIX = gaim_gtkxfer_dialog_
PROTOTYPES: ENABLE

Gaim::GtkUI::Xfer::Dialog
gaim_gtkxfer_dialog_new()

void
gaim_gtkxfer_dialog_destroy(dialog)
	Gaim::GtkUI::Xfer::Dialog dialog

void
gaim_gtkxfer_dialog_show(dialog = NULL)
	Gaim::GtkUI::Xfer::Dialog dialog

void
gaim_gtkxfer_dialog_hide(dialog)
	Gaim::GtkUI::Xfer::Dialog dialog

void
gaim_gtkxfer_dialog_add_xfer(dialog, xfer)
	Gaim::GtkUI::Xfer::Dialog dialog
	Gaim::Xfer xfer

void
gaim_gtkxfer_dialog_remove_xfer(dialog, xfer)
	Gaim::GtkUI::Xfer::Dialog dialog
	Gaim::Xfer xfer

void
gaim_gtkxfer_dialog_cancel_xfer(dialog, xfer)
	Gaim::GtkUI::Xfer::Dialog dialog
	Gaim::Xfer xfer

void
gaim_gtkxfer_dialog_update_xfer(dialog, xfer)
	Gaim::GtkUI::Xfer::Dialog dialog
	Gaim::Xfer xfer
