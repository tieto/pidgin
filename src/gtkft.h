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
#ifndef _GAIM_GTKFT_H_
#define _GAIM_GTKFT_H_

#include "ft.h"

/**
 * A file transfer dialog.
 *
 * The structure is opaque, as nobody should be touching anything inside of
 * it.
 */
typedef struct _GaimGtkXferDialog GaimGtkXferDialog;

/**************************************************************************/
/** @name GTK+ File Transfer Dialog API                                   */
/**************************************************************************/
/*@{*/

/**
 * Creates a new file transfer dialog.
 *
 * @return The new dialog.
 */
GaimGtkXferDialog *gaim_gtkxfer_dialog_new(void);

/**
 * Destroys a file transfer dialog.
 *
 * @param dialog The file transfer dialog.
 */
void gaim_gtkxfer_dialog_destroy(GaimGtkXferDialog *dialog);

/**
 * Displays the file transfer dialog.
 *
 * @param dialog The file transfer dialog to show.
 */
void gaim_gtkxfer_dialog_show(GaimGtkXferDialog *dialog);

/**
 * Hides the file transfer dialog.
 *
 * @param dialog The file transfer dialog to hide.
 */
void gaim_gtkxfer_dialog_hide(GaimGtkXferDialog *dialog);

/**
 * Shows the file transfer dialog, creating a new one if necessary
 */
void gaim_show_xfer_dialog(void);

/**
 * Adds a file transfer to the dialog.
 *
 * @param dialog The file transfer dialog.
 * @param xfer   The file transfer.
 */
void gaim_gtkxfer_dialog_add_xfer(GaimGtkXferDialog *dialog, GaimXfer *xfer);

/**
 * Removes a file transfer from the dialog.
 *
 * @param dialog The file transfer dialog.
 * @param xfer   The file transfer.
 */
void gaim_gtkxfer_dialog_remove_xfer(GaimGtkXferDialog *dialog,
									 GaimXfer *xfer);

/**
 * Indicate in a file transfer dialog that a transfer was canceled.
 *
 * @param dialog The file transfer dialog.
 * @param xfer   The file transfer that was canceled.
 */
void gaim_gtkxfer_dialog_cancel_xfer(GaimGtkXferDialog *dialog,
									 GaimXfer *xfer);

/**
 * Updates the information for a transfer in the dialog.
 *
 * @param dialog The file transfer dialog.
 * @param xfer   The file transfer.
 */
void gaim_gtkxfer_dialog_update_xfer(GaimGtkXferDialog *dialog,
									 GaimXfer *xfer);

/*@}*/

/**************************************************************************/
/** @name GTK+ File Transfer API                                          */
/**************************************************************************/
/*@{*/

/**
 * Initializes the GTK+ file transfer system.
 */
void gaim_gtk_xfers_init(void);

/**
 * Uninitializes the GTK+ file transfer system.
 */
void gaim_gtk_xfers_uninit(void);

/**
 * Sets gaim's main file transfer dialog.
 *
 * @param dialog The main dialog.
 */
void gaim_set_gtkxfer_dialog(GaimGtkXferDialog *dialog);

/**
 * Returns gaim's main file transfer dialog.
 *
 * @return The main dialog.
 */
GaimGtkXferDialog *gaim_get_gtkxfer_dialog(void);

/**
 * Returns the UI operations structure for the GTK+ file transfer UI.
 *
 * @return The GTK+ file transfer UI operations structure.
 */
GaimXferUiOps *gaim_gtk_xfers_get_ui_ops(void);

/*@}*/

#endif /* _GAIM_GTKFT_H_ */
