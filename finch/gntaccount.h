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
/**
 * SECTION:gntaccount
 * @section_id: finch-gntaccount
 * @short_description: <filename>gntaccount.h</filename>
 * @title: Account API
 */

#include "account.h"

/**********************************************************************
 * GNT Account API
 **********************************************************************/

/**
 * finch_accounts_get_ui_ops:
 *
 * Get the ui-functions.
 *
 * Returns: The PurpleAccountUiOps structure populated with the appropriate functions.
 */
PurpleAccountUiOps *finch_accounts_get_ui_ops(void);

/**
 * finch_accounts_init:
 *
 * Perform necessary initializations.
 */
void finch_accounts_init(void);

/**
 * finch_accounts_uninit:
 *
 * Perform necessary uninitializations.
 */
void finch_accounts_uninit(void);

/**
 * finch_accounts_show_all:
 *
 * Show the account-manager dialog.
 */
void finch_accounts_show_all(void);

/**
 * finch_account_dialog_show:
 * @account:  The account to edit, or %NULL to create a new account.
 *
 * Show the edit dialog for an account.
 */
void finch_account_dialog_show(PurpleAccount *account);

#endif
