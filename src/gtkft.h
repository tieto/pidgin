/**
 * @file gtkft.h The GTK+ file transfer UI
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
 *
 */
#ifndef _GAIM_GTK_FT_H_
#define _GAIM_GTK_FT_H_

/**************************************************************************/
/** @name GTK+ File Transfer API                                          */
/**************************************************************************/
/*@{*/

/**
 * Displays the file transfer dialog.
 */
void gaim_gtkxfer_dialog_show(void);

/**
 * Hides the file transfer dialog.
 */
void gaim_gtkxfer_dialog_hide(void);

/**
 * Returns the UI operations structure for the GTK+ file transfer UI.
 *
 * @return The GTK+ file transfer UI operations structure.
 */
struct gaim_xfer_ui_ops *gaim_get_gtk_xfer_ui_ops(void);

/*@}*/

#endif /* _GAIM_GTK_FT_H_ */
