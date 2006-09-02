#include "gtkmodule.h"

MODULE = Gaim::Gtk::Xfer  PACKAGE = Gaim::Gtk::Xfer  PREFIX = gaim_gtk_xfer_
PROTOTYPES: ENABLE

void
gaim_set_gtkxfer_dialog(dialog)
	Gaim::Gtk::Xfer::Dialog dialog

Gaim::Gtk::Xfer::Dialog
gaim_get_gtkxfer_dialog()

MODULE = Gaim::Gtk::Xfer  PACKAGE = Gaim::Gtk::Xfer::Dialog  PREFIX = gaim_gtkxfer_dialog_
PROTOTYPES: ENABLE

Gaim::Gtk::Xfer::Dialog
gaim_gtkxfer_dialog_new()

void
gaim_gtkxfer_dialog_destroy(dialog)
	Gaim::Gtk::Xfer::Dialog dialog

void
gaim_gtkxfer_dialog_show(dialog = NULL)
	Gaim::Gtk::Xfer::Dialog dialog

void
gaim_gtkxfer_dialog_hide(dialog)
	Gaim::Gtk::Xfer::Dialog dialog

void
gaim_gtkxfer_dialog_add_xfer(dialog, xfer)
	Gaim::Gtk::Xfer::Dialog dialog
	Gaim::Xfer xfer

void
gaim_gtkxfer_dialog_remove_xfer(dialog, xfer)
	Gaim::Gtk::Xfer::Dialog dialog
	Gaim::Xfer xfer

void
gaim_gtkxfer_dialog_cancel_xfer(dialog, xfer)
	Gaim::Gtk::Xfer::Dialog dialog
	Gaim::Xfer xfer

void
gaim_gtkxfer_dialog_update_xfer(dialog, xfer)
	Gaim::Gtk::Xfer::Dialog dialog
	Gaim::Xfer xfer
