/**
 * @file gtkroomlist.h GTK+ Room List UI
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
#ifndef _PIDGINROOMLIST_H_
#define _PIDGINROOMLIST_H_

#include "roomlist.h"

/**
 * Initializes the room list subsystem.
 */
void pidgin_roomlist_init(void);

/**
 * Determines if showing the room list dialog is a valid action.
 *
 * @return TRUE if there are accounts online that support listing
 *         chat rooms.  Otherwise return FALSE.
 */
gboolean pidgin_roomlist_is_showable(void);

/**
 * Shows a new roomlist dialog.
 */
void pidgin_roomlist_dialog_show(void);

/**
 * Shows a new room list dialog and fetches the list for the specified account.
 *
 * @param account The account to use.
 */
void pidgin_roomlist_dialog_show_with_account(PurpleAccount *account);

#endif /* _PIDGINROOMLIST_H_ */
