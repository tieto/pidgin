/**
 * @file gtkprivacy.h GTK+ Privacy UI
 * @ingroup gtkui
 *
 * gaim
 *
 * Copyright (C) 2003 Christian Hammond <chipx86@gnupdate.org>
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
#ifndef _GAIM_GTK_PRIVACY_H_
#define _GAIM_GTK_PRIVACY_H_

#include "privacy.h"

/**
 * Initializes the GTK+ privacy subsystem.
 */
void gaim_gtk_privacy_init(void);

/**
 * Shows the privacy dialog.
 */
void gaim_gtk_privacy_dialog_show(void);

/**
 * Hides the privacy dialog.
 */
void gaim_gtk_privacy_dialog_hide(void);

/**
 * Requests confirmation to add a user to the allow list for an account,
 * and then adds it.
 *
 * If @a name is not specified, an input dialog will be presented.
 *
 * @param account The account.
 * @param name    The name of the user to add.
 */
void gaim_gtk_request_add_permit(GaimAccount *account, const char *name);

/**
 * Requests confirmation to add a user to the block list for an account,
 * and then adds it.
 *
 * If @a name is not specified, an input dialog will be presented.
 *
 * @param account The account.
 * @param name    The name of the user to add.
 */
void gaim_gtk_request_add_block(GaimAccount *account, const char *name);

/**
 * Returns the UI operations structure for the GTK+ privacy subsystem.
 *
 * @return The GTK+ UI privacy operations structure.
 */
GaimPrivacyUiOps *gaim_gtk_privacy_get_ui_ops(void);

#endif /* _GAIM_GTK_PRIVACY_H_ */
