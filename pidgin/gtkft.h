/**
 * @file gtkft.h GTK+ File Transfer UI
 * @ingroup gtkui
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
PidginXferDialog *pidginxfer_dialog_new(void);

/**
 * Destroys a file transfer dialog.
 *
 * @param dialog The file transfer dialog.
 */
void pidginxfer_dialog_destroy(PidginXferDialog *dialog);

/**
 * Displays the file transfer dialog given.
 * If dialog is @c NULL, displays the default dialog, creating one if necessary
 *
 * @param dialog The file transfer dialog to show.
 */
void pidginxfer_dialog_show(PidginXferDialog *dialog);

/**
 * Hides the file transfer dialog.
 *
 * @param dialog The file transfer dialog to hide.
 */
void pidginxfer_dialog_hide(PidginXferDialog *dialog);

/**
 * Adds a file transfer to the dialog.
 *
 * @param dialog The file transfer dialog.
 * @param xfer   The file transfer.
 */
void pidginxfer_dialog_add_xfer(PidginXferDialog *dialog, GaimXfer *xfer);

/**
 * Removes a file transfer from the dialog.
 *
 * @param dialog The file transfer dialog.
 * @param xfer   The file transfer.
 */
void pidginxfer_dialog_remove_xfer(PidginXferDialog *dialog,
									 GaimXfer *xfer);

/**
 * Indicate in a file transfer dialog that a transfer was canceled.
 *
 * @param dialog The file transfer dialog.
 * @param xfer   The file transfer that was canceled.
 */
void pidginxfer_dialog_cancel_xfer(PidginXferDialog *dialog,
									 GaimXfer *xfer);

/**
 * Updates the information for a transfer in the dialog.
 *
 * @param dialog The file transfer dialog.
 * @param xfer   The file transfer.
 */
void pidginxfer_dialog_update_xfer(PidginXferDialog *dialog,
									 GaimXfer *xfer);

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
 * Sets gaim's main file transfer dialog.
 *
 * @param dialog The main dialog.
 */
void gaim_set_gtkxfer_dialog(PidginXferDialog *dialog);

/**
 * Returns gaim's main file transfer dialog.
 *
 * @return The main dialog.
 */
PidginXferDialog *gaim_get_gtkxfer_dialog(void);

/**
 * Returns the UI operations structure for the GTK+ file transfer UI.
 *
 * @return The GTK+ file transfer UI operations structure.
 */
GaimXferUiOps *pidgin_xfers_get_ui_ops(void);

/*@}*/

#endif /* _PIDGINFT_H_ */
