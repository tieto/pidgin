/**
 * @file gntft.h GNT File Transfer UI
 * @ingroup gntui
 *
 * finch
 *
 * Finch is the legal property of its developers, whose names are too numerous
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
#ifndef _FINCHFT_H_
#define _FINCHFT_H_

#include "ft.h"


/**************************************************************************/
/** @name GNT File Transfer Dialog API                                    */
/**************************************************************************/
/*@{*/

/**
 * Creates a new file transfer dialog.
 *
 * @return The new dialog.
 */
void finch_xfer_dialog_new(void);

/**
 * Destroys a file transfer dialog.
 *
 * @param dialog The file transfer dialog.
 */
void finch_xfer_dialog_destroy(void);

/**
 * Displays the file transfer dialog given.
 * If dialog is @c NULL, displays the default dialog, creating one if necessary
 *
 * @param dialog The file transfer dialog to show.
 */
void finch_xfer_dialog_show(void);

/**
 * Hides the file transfer dialog.
 *
 * @param dialog The file transfer dialog to hide.
 */
void finch_xfer_dialog_hide();

/**
 * Adds a file transfer to the dialog.
 *
 * @param dialog The file transfer dialog.
 * @param xfer   The file transfer.
 */
void finch_xfer_dialog_add_xfer(PurpleXfer *xfer);

/**
 * Removes a file transfer from the dialog.
 *
 * @param dialog The file transfer dialog.
 * @param xfer   The file transfer.
 */
void finch_xfer_dialog_remove_xfer(PurpleXfer *xfer);

/**
 * Indicate in a file transfer dialog that a transfer was canceled.
 *
 * @param dialog The file transfer dialog.
 * @param xfer   The file transfer that was canceled.
 */
void finch_xfer_dialog_cancel_xfer(PurpleXfer *xfer);

/**
 * Updates the information for a transfer in the dialog.
 *
 * @param dialog The file transfer dialog.
 * @param xfer   The file transfer.
 */
void finch_xfer_dialog_update_xfer(PurpleXfer *xfer);

/*@}*/

/**************************************************************************/
/** @name GNT  File Transfer API                                          */
/**************************************************************************/
/*@{*/

/**
 * Initializes the GNT file transfer system.
 */
void finch_xfers_init(void);

/**
 * Uninitializes the GNT file transfer system.
 */
void finch_xfers_uninit(void);

/**
 * Returns the UI operations structure for the GNT file transfer UI.
 *
 * @return The GNT file transfer UI operations structure.
 */
PurpleXferUiOps *finch_xfers_get_ui_ops(void);

/*@}*/

#endif /* _FINCHFT_H_ */
