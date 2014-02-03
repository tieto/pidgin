/* finch
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
/**
 * SECTION:gntxfer
 * @section_id: finch-gntxfer
 * @short_description: <filename>gntxfer.h</filename>
 * @title: File Transfer UI
 */

#ifndef _GNT_XFER_H_
#define _GNT_XFER_H_

#include "xfer.h"


/**************************************************************************/
/** @name GNT File Transfer Dialog API                                    */
/**************************************************************************/
/*@{*/

/**
 * finch_xfer_dialog_new:
 *
 * Creates a new file transfer dialog.
 *
 * Returns: The new dialog.
 */
void finch_xfer_dialog_new(void);

/**
 * finch_xfer_dialog_destroy:
 *
 * Destroys a file transfer dialog.
 */
void finch_xfer_dialog_destroy(void);

/**
 * finch_xfer_dialog_show:
 *
 * Displays the file transfer dialog given.
 * If dialog is %NULL, displays the default dialog, creating one if necessary
 */
void finch_xfer_dialog_show(void);

/**
 * finch_xfer_dialog_hide:
 *
 * Hides the file transfer dialog.
 */
void finch_xfer_dialog_hide(void);

/**
 * finch_xfer_dialog_add_xfer:
 * @xfer:   The file transfer.
 *
 * Adds a file transfer to the dialog.
 */
void finch_xfer_dialog_add_xfer(PurpleXfer *xfer);

/**
 * finch_xfer_dialog_remove_xfer:
 * @xfer:   The file transfer.
 *
 * Removes a file transfer from the dialog.
 */
void finch_xfer_dialog_remove_xfer(PurpleXfer *xfer);

/**
 * finch_xfer_dialog_cancel_xfer:
 * @xfer:   The file transfer that was cancelled.
 *
 * Indicate in a file transfer dialog that a transfer was cancelled.
 */
void finch_xfer_dialog_cancel_xfer(PurpleXfer *xfer);

/**
 * finch_xfer_dialog_update_xfer:
 * @xfer:   The file transfer.
 *
 * Updates the information for a transfer in the dialog.
 */
void finch_xfer_dialog_update_xfer(PurpleXfer *xfer);

/*@}*/

/**************************************************************************/
/** @name GNT  File Transfer API                                          */
/**************************************************************************/
/*@{*/

/**
 * finch_xfers_init:
 *
 * Initializes the GNT file transfer system.
 */
void finch_xfers_init(void);

/**
 * finch_xfers_uninit:
 *
 * Uninitializes the GNT file transfer system.
 */
void finch_xfers_uninit(void);

/**
 * finch_xfers_get_ui_ops:
 *
 * Returns the UI operations structure for the GNT file transfer UI.
 *
 * Returns: The GNT file transfer UI operations structure.
 */
PurpleXferUiOps *finch_xfers_get_ui_ops(void);

/*@}*/

#endif /* _GNT_XFER_H_ */
