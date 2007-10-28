/**
 * @file gntaccount.h GNT Account API
 * @ingroup finch
 */

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
#ifndef _GNT_ACCOUNT_H
#define _GNT_ACCOUNT_H

#include "account.h"

/**********************************************************************
 * @name GNT Account API
 **********************************************************************/
/*@{*/

/**
 * Get the ui-functions.
 *
 * @return The PurpleAccountUiOps structure populated with the appropriate functions.
 */
PurpleAccountUiOps *finch_accounts_get_ui_ops(void);

/**
 * Perform necessary initializations.
 */
void finch_accounts_init(void);

/**
 * Perform necessary uninitializations.
 */
void finch_accounts_uninit(void);

/**
 * Show the account-manager dialog.
 */
void finch_accounts_show_all(void);

/**
 * Show the edit dialog for an account.
 *
 * @param account  The account to edit, or @c NULL to create a new account.
 *
 * @since 2.2.0
 */
void finch_account_dialog_show(PurpleAccount *account);

/*@}*/

#endif
