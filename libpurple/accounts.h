/**
 * @file accounts.h Accounts API
 * @ingroup core
 * @see @ref account-signals
 */

/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
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
#ifndef _PURPLE_ACCOUNTS_H_
#define _PURPLE_ACCOUNTS_H_

#include "account.h"
#include "status.h"

/** @copydoc _PurpleAccountUiOps */
typedef struct _PurpleAccountUiOps  PurpleAccountUiOps;

/**  Account UI operations, used to notify the user of status changes and when
 *   buddies add this account to their buddy lists.
 */
struct _PurpleAccountUiOps
{
	/** A buddy who is already on this account's buddy list added this account
	 *  to their buddy list.
	 */
	void (*notify_added)(PurpleAccount *account,
	                     const char *remote_user,
	                     const char *id,
	                     const char *alias,
	                     const char *message);

	/** This account's status changed. */
	void (*status_changed)(PurpleAccount *account,
	                       PurpleStatus *status);

	/** Someone we don't have on our list added us; prompt to add them. */
	void (*request_add)(PurpleAccount *account,
	                    const char *remote_user,
	                    const char *id,
	                    const char *alias,
	                    const char *message);

	/** Prompt for authorization when someone adds this account to their buddy
	 * list.  To authorize them to see this account's presence, call \a
	 * authorize_cb (\a message, \a user_data); otherwise call
	 * \a deny_cb (\a message, \a user_data);
	 * @return a UI-specific handle, as passed to #close_account_request.
	 */
	void *(*request_authorize)(PurpleAccount *account,
	                           const char *remote_user,
	                           const char *id,
	                           const char *alias,
	                           const char *message,
	                           gboolean on_list,
	                           PurpleAccountRequestAuthorizationCb authorize_cb,
	                           PurpleAccountRequestAuthorizationCb deny_cb,
	                           void *user_data);

	/** Close a pending request for authorization.  \a ui_handle is a handle
	 *  as returned by #request_authorize.
	 */
	void (*close_account_request)(void *ui_handle);

	void (*permit_added)(PurpleAccount *account, const char *name);
	void (*permit_removed)(PurpleAccount *account, const char *name);
	void (*deny_added)(PurpleAccount *account, const char *name);
	void (*deny_removed)(PurpleAccount *account, const char *name);

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

/**************************************************************************/
/** @name Accounts API                                                    */
/**************************************************************************/
/*@{*/

/**
 * Adds an account to the list of accounts.
 *
 * @param account The account.
 */
void purple_accounts_add(PurpleAccount *account);

/**
 * Removes an account from the list of accounts.
 *
 * @param account The account.
 */
void purple_accounts_remove(PurpleAccount *account);

/**
 * Deletes an account.
 *
 * This will remove any buddies from the buddy list that belong to this
 * account, buddy pounces that belong to this account, and will also
 * destroy @a account.
 *
 * @param account The account.
 */
void purple_accounts_delete(PurpleAccount *account);

/**
 * Reorders an account.
 *
 * @param account   The account to reorder.
 * @param new_index The new index for the account.
 */
void purple_accounts_reorder(PurpleAccount *account, gint new_index);

/**
 * Returns a list of all accounts.
 *
 * @constreturn A list of all accounts.
 */
GList *purple_accounts_get_all(void);

/**
 * Returns a list of all enabled accounts
 *
 * @return A list of all enabled accounts. The list is owned
 *         by the caller, and must be g_list_free()d to avoid
 *         leaking the nodes.
 */
GList *purple_accounts_get_all_active(void);

/**
 * Finds an account with the specified name and protocol id.
 *
 * @param name     The account username.
 * @param protocol The account protocol ID.
 *
 * @return The account, if found, or @c FALSE otherwise.
 */
PurpleAccount *purple_accounts_find(const char *name, const char *protocol);

/**
 * This is called by the core after all subsystems and what
 * not have been initialized.  It sets all enabled accounts
 * to their startup status by signing them on, setting them
 * away, etc.
 *
 * You probably shouldn't call this unless you really know
 * what you're doing.
 */
void purple_accounts_restore_current_statuses(void);

/*@}*/


/**************************************************************************/
/** @name UI Registration Functions                                       */
/**************************************************************************/
/*@{*/
/**
 * Sets the UI operations structure to be used for accounts.
 *
 * @param ops The UI operations structure.
 */
void purple_accounts_set_ui_ops(PurpleAccountUiOps *ops);

/**
 * Returns the UI operations structure used for accounts.
 *
 * @return The UI operations structure in use.
 */
PurpleAccountUiOps *purple_accounts_get_ui_ops(void);

/*@}*/


/**************************************************************************/
/** @name Accounts Subsystem                                              */
/**************************************************************************/
/*@{*/

/**
 * Returns the accounts subsystem handle.
 *
 * @return The accounts subsystem handle.
 */
void *purple_accounts_get_handle(void);

/**
 * Initializes the accounts subsystem.
 */
void purple_accounts_init(void);

/**
 * Uninitializes the accounts subsystem.
 */
void purple_accounts_uninit(void);

/**
 * Schedules saving of accounts
 */
void purple_accounts_schedule_save(void);

/*@}*/

G_END_DECLS

#endif /* _PURPLE_ACCOUNTS_H_ */
