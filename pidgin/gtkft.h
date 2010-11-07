/**
 * @file gtkft.h GTK+ File Transfer UI
 * @ingroup pidgin
 */

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
#ifndef _PIDGINFT_H_
#define _PIDGINFT_H_

#include "ft.h"

/**
 * A file transfer dialog.
 *
 * The structure is opaque, as nobody should be touching anything inside of
 * it.
 */
typedef struct _PidginXferDialog PidginXferDialog;

/**************************************************************************/
/** @name GTK+ File Transfer Dialog API                                   */
/**************************************************************************/
/*@{*/

/**
 * Creates a new file transfer dialog.
 *
 * @return The new dialog.
 */
PidginXferDialog *pidgin_xfer_dialog_new(void);

/**
 * Destroys a file transfer dialog.
 *
 * @param dialog The file transfer dialog.
 */
void pidgin_xfer_dialog_destroy(PidginXferDialog *dialog);

/**
 * Displays the file transfer dialog given.
 * If dialog is @c NULL, displays the default dialog, creating one if necessary
 *
 * @param dialog The file transfer dialog to show.
 */
void pidgin_xfer_dialog_show(PidginXferDialog *dialog);

/**
 * Hides the file transfer dialog.
 *
 * @param dialog The file transfer dialog to hide.
 */
void pidgin_xfer_dialog_hide(PidginXferDialog *dialog);

/**
 * Adds a file transfer to the dialog.
 *
 * @param dialog The file transfer dialog.
 * @param xfer   The file transfer.
 */
void pidgin_xfer_dialog_add_xfer(PidginXferDialog *dialog, PurpleXfer *xfer);

/**
 * Removes a file transfer from the dialog.
 *
 * @param dialog The file transfer dialog.
 * @param xfer   The file transfer.
 */
void pidgin_xfer_dialog_remove_xfer(PidginXferDialog *dialog,
									 PurpleXfer *xfer);

/**
 * Indicate in a file transfer dialog that a transfer was canceled.
 *
 * @param dialog The file transfer dialog.
 * @param xfer   The file transfer that was canceled.
 */
void pidgin_xfer_dialog_cancel_xfer(PidginXferDialog *dialog,
									 PurpleXfer *xfer);

/**
 * Updates the information for a transfer in the dialog.
 *
 * @param dialog The file transfer dialog.
 * @param xfer   The file transfer.
 */
void pidgin_xfer_dialog_update_xfer(PidginXferDialog *dialog,
									 PurpleXfer *xfer);

/*@}*/

/**************************************************************************/
/** @name GTK+ File Transfer API                                          */
/**************************************************************************/
/*@{*/

/**
 * Initializes the GTK+ file transfer system.
 */
void pidgin_xfers_init(void);

/**
 * Uninitializes the GTK+ file transfer system.
 */
void pidgin_xfers_uninit(void);

/**
 * Sets pidgin's main file transfer dialog.
 *
 * @param dialog The main dialog.
 */
void pidgin_set_xfer_dialog(PidginXferDialog *dialog);

/**
 * Returns pirgin's main file transfer dialog.
 *
 * @return The main dialog.
 */
PidginXferDialog *pidgin_get_xfer_dialog(void);

/**
 * Returns the UI operations structure for the GTK+ file transfer UI.
 *
 * @return The GTK+ file transfer UI operations structure.
 */
PurpleXferUiOps *pidgin_xfers_get_ui_ops(void);

/*@}*/

#endif /* _PIDGINFT_H_ */
