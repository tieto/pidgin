/**
 * @file gtkaccount.h Account Editor dialog
 * @ingroup gtkui
 *
 * gaim
 *
 * Copyright (C) 2002-2003, Christian Hammond <chipx86@gnupdate.org>
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
#ifndef _GAIM_GTK_ACCOUNT_H_
#define _GAIM_GTK_ACCOUNT_H_

typedef enum
{
	GAIM_GTK_ADD_ACCOUNT_DIALOG,
	GAIM_GTK_MODIFY_ACCOUNT_DIALOG

} GaimGtkAccountDialogType;


/**
 * Shows the accounts window.
 */
void gaim_gtk_accounts_window_show(void);

/**
 * Hides the accounts window.
 */
void gaim_gtk_accounts_window_hide(void);

/**
 * Shows an add/modify account dialog.
 *
 * @param type    The type of dialog.
 * @param account The associated account, or @c NULL for an Add dialog.
 */
void gaim_gtk_account_dialog_show(GaimGtkAccountDialogType type,
								  GaimAccount *account);

#endif /* _GAIM_GTK_ACCOUNT_H_ */

