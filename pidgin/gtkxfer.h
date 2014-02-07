/* pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#ifndef _PIDGINXFER_H_
#define _PIDGINXFER_H_
/**
 * SECTION:gtkxfer
 * @section_id: pidgin-gtkxfer
 * @short_description: <filename>gtkxfer.h</filename>
 * @title: File Transfer UI
 */

#include "xfer.h"

/**
 * PidginXferDialog:
 *
 * A file transfer dialog.
 *
 * The structure is opaque, as nobody should be touching anything inside of
 * it.
 */
typedef struct _PidginXferDialog PidginXferDialog;

G_BEGIN_DECLS

/**************************************************************************/
/* GTK+ File Transfer Dialog API                                          */
/**************************************************************************/

/**
 * pidgin_xfer_dialog_new:
 *
 * Creates a new file transfer dialog.
 *
 * Returns: The new dialog.
 */
PidginXferDialog *pidgin_xfer_dialog_new(void);

/**
 * pidgin_xfer_dialog_destroy:
 * @dialog: The file transfer dialog.
 *
 * Destroys a file transfer dialog.
 */
void pidgin_xfer_dialog_destroy(PidginXferDialog *dialog);

/**
 * pidgin_xfer_dialog_show:
 * @dialog: The file transfer dialog to show.
 *
 * Displays the file transfer dialog given.
 * If dialog is %NULL, displays the default dialog, creating one if necessary
 */
void pidgin_xfer_dialog_show(PidginXferDialog *dialog);

/**
 * pidgin_xfer_dialog_hide:
 * @dialog: The file transfer dialog to hide.
 *
 * Hides the file transfer dialog.
 */
void pidgin_xfer_dialog_hide(PidginXferDialog *dialog);

/**
 * pidgin_xfer_dialog_add_xfer:
 * @dialog: The file transfer dialog.
 * @xfer:   The file transfer.
 *
 * Adds a file transfer to the dialog.
 */
void pidgin_xfer_dialog_add_xfer(PidginXferDialog *dialog, PurpleXfer *xfer);

/**
 * pidgin_xfer_dialog_remove_xfer:
 * @dialog: The file transfer dialog.
 * @xfer:   The file transfer.
 *
 * Removes a file transfer from the dialog.
 */
void pidgin_xfer_dialog_remove_xfer(PidginXferDialog *dialog,
									 PurpleXfer *xfer);

/**
 * pidgin_xfer_dialog_cancel_xfer:
 * @dialog: The file transfer dialog.
 * @xfer:   The file transfer that was cancelled.
 *
 * Indicate in a file transfer dialog that a transfer was cancelled.
 */
void pidgin_xfer_dialog_cancel_xfer(PidginXferDialog *dialog,
									 PurpleXfer *xfer);

/**
 * pidgin_xfer_dialog_update_xfer:
 * @dialog: The file transfer dialog.
 * @xfer:   The file transfer.
 *
 * Updates the information for a transfer in the dialog.
 */
void pidgin_xfer_dialog_update_xfer(PidginXferDialog *dialog,
									 PurpleXfer *xfer);

/**************************************************************************/
/* GTK+ File Transfer API                                                 */
/**************************************************************************/

/**
 * pidgin_xfers_init:
 *
 * Initializes the GTK+ file transfer system.
 */
void pidgin_xfers_init(void);

/**
 * pidgin_xfers_uninit:
 *
 * Uninitializes the GTK+ file transfer system.
 */
void pidgin_xfers_uninit(void);

/**
 * pidgin_set_xfer_dialog:
 * @dialog: The main dialog.
 *
 * Sets pidgin's main file transfer dialog.
 */
void pidgin_set_xfer_dialog(PidginXferDialog *dialog);

/**
 * pidgin_get_xfer_dialog:
 *
 * Returns pirgin's main file transfer dialog.
 *
 * Returns: The main dialog.
 */
PidginXferDialog *pidgin_get_xfer_dialog(void);

/**
 * pidgin_xfers_get_ui_ops:
 *
 * Returns the UI operations structure for the GTK+ file transfer UI.
 *
 * Returns: The GTK+ file transfer UI operations structure.
 */
PurpleXferUiOps *pidgin_xfers_get_ui_ops(void);

G_END_DECLS

#endif /* _PIDGINXFER_H_ */
