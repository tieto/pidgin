/**
 * @file gtkft.h GTK+ file transfer UI
 * @ingroup gtkui
 *
 * gaim
 *
 * Copyright (C) 2003, Christian Hammond <chipx86@gnupdate.org>
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
#ifndef _GAIM_GTK_FT_H_
#define _GAIM_GTK_FT_H_

#include "ft.h"

/**
 * A file transfer dialog.
 * 
 * The structure is opaque, as nobody should be touching anything inside of
 * it.
 */
struct gaim_gtkxfer_dialog;

/**************************************************************************/
/** @name GTK+ File Transfer Dialog API                                   */
/**************************************************************************/
/*@{*/

/**
 * Creates a new file transfer dialog.
 *
 * @return The new dialog.
 */
struct gaim_gtkxfer_dialog *gaim_gtkxfer_dialog_new(void);

/**
 * Destroys a file transfer dialog.
 *
 * @param dialog The file transfer dialog.
 */
void gaim_gtkxfer_dialog_destroy(struct gaim_gtkxfer_dialog *dialog);

/**
 * Displays the file transfer dialog.
 *
 * @param dialog The file transfer dialog to show.
 */
void gaim_gtkxfer_dialog_show(struct gaim_gtkxfer_dialog *dialog);

/**
 * Hides the file transfer dialog.
 *
 * @param dialog The file transfer dialog to hide.
 */
void gaim_gtkxfer_dialog_hide(struct gaim_gtkxfer_dialog *dialog);

/**
 * Shows the file transfer dialog, creating a new one if necessary
 */
void gaim_show_xfer_dialog();

/**
 * Adds a file transfer to the dialog.
 *
 * @param dialog The file transfer dialog.
 * @param xfer   The file transfer.
 */
void gaim_gtkxfer_dialog_add_xfer(struct gaim_gtkxfer_dialog *dialog,
								  struct gaim_xfer *xfer);

/**
 * Removes a file transfer from the dialog.
 *
 * @param dialog The file transfer dialog.
 * @param xfer   The file transfer.
 */
void gaim_gtkxfer_dialog_remove_xfer(struct gaim_gtkxfer_dialog *dialog,
									 struct gaim_xfer *xfer);

/**
 * Indicate in a file transfer dialog that a transfer was canceled.
 *
 * @param dialog The file transfer dialog.
 * @param xfer   The file transfer that was canceled.
 */
void gaim_gtkxfer_dialog_cancel_xfer(struct gaim_gtkxfer_dialog *dialog,
									 struct gaim_xfer *xfer);

/**
 * Updates the information for a transfer in the dialog.
 *
 * @param dialog The file transfer dialog.
 * @param xfer   The file transfer.
 */
void gaim_gtkxfer_dialog_update_xfer(struct gaim_gtkxfer_dialog *dialog,
									 struct gaim_xfer *xfer);

/*@}*/

/**************************************************************************/
/** @name GTK+ File Transfer API                                          */
/**************************************************************************/
/*@{*/

/**
 * Sets gaim's main file transfer dialog.
 *
 * @param dialog The main dialog.
 */
void gaim_set_gtkxfer_dialog(struct gaim_gtkxfer_dialog *dialog);

/**
 * Returns gaim's main file transfer dialog.
 *
 * @return The main dialog.
 */
struct gaim_gtkxfer_dialog *gaim_get_gtkxfer_dialog(void);

/**
 * Returns the UI operations structure for the GTK+ file transfer UI.
 *
 * @return The GTK+ file transfer UI operations structure.
 */
struct gaim_xfer_ui_ops *gaim_get_gtk_xfer_ui_ops(void);

/*@}*/

#endif /* _GAIM_GTK_FT_H_ */
