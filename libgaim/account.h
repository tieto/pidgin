/**
 * @file account.h Account API
 * @ingroup core
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 *
 * @see @ref account-signals
 */
#ifndef _GAIM_ACCOUNT_H_
#define _GAIM_ACCOUNT_H_

#include <glib.h>

typedef struct _GaimAccountUiOps GaimAccountUiOps;
typedef struct _GaimAccount      GaimAccount;

typedef gboolean (*GaimFilterAccountFunc)(GaimAccount *account);

#include "connection.h"
#include "log.h"
#include "proxy.h"
#include "prpl.h"
#include "status.h"

struct _GaimAccountUiOps
{
	/* A buddy we already have added us to their buddy list. */
	void (*notify_added)(GaimAccount *account, const char *remote_user,
	                     const char *id, const char *alias,
	                     const char *message);
	void (*status_changed)(GaimAccount *account, GaimStatus *status);
	/* Someone we don't have on our list added us. Will prompt to add them. */
	void (*request_add)(GaimAccount *account, const char *remote_user,
	                    const char *id, const char *alias,
	                    const char *message);
};

struct _GaimAccount
{
	char *username;             /**< The username.                          */
	char *alias;                /**< How you appear to yourself.            */
	char *password;             /**< The account password.                  */
	char *user_info;            /**< User information.                      */

	char *buddy_icon;           /**< The buddy icon.                        */

	gboolean remember_pass;     /**< Remember the password.                 */

	char *protocol_id;          /**< The ID of the protocol.                */

	GaimConnection *gc;         /**< The connection handle.                 */
	gboolean disconnecting;     /**< The account is currently disconnecting */

	GHashTable *settings;       /**< Protocol-specific settings.            */
	GHashTable *ui_settings;    /**< UI-specific settings.                  */

	GaimProxyInfo *proxy_info;  /**< Proxy information.  This will be set   */
								/*   to NULL when the account inherits      */
								/*   proxy settings from global prefs.      */

	GSList *permit;             /**< Permit list.                           */
	GSList *deny;               /**< Deny list.                             */
	int perm_deny;              /**< The permit/deny setting.               */

	GList *status_types;        /**< Status types.                          */

	GaimPresence *presence;     /**< Presence.                              */
	GaimLog *system_log;        /**< The system log                         */

	void *ui_data;              /**< The UI can put data here.              */
};

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************/
/** @name Account API                                                     */
/**************************************************************************/
/*@{*/

/**
 * Creates a new account.
 *
 * @param username    The username.
 * @param protocol_id The protocol ID.
 *
 * @return The new account.
 */
GaimAccount *gaim_account_new(const char *username, const char *protocol_id);

/**
 * Destroys an account.
 *
 * @param account The account to destroy.
 */
void gaim_account_destroy(GaimAccount *account);

/**
 * Connects to an account.
 *
 * @param account The account to connect to.
 */
void gaim_account_connect(GaimAccount *account);

/**
 * Registers an account.
 *
 * @param account The account to register.
 */
void gaim_account_register(GaimAccount *account);

/**
 * Disconnects from an account.
 *
 * @param account The account to disconnect from.
 */
void gaim_account_disconnect(GaimAccount *account);

/**
 * Notifies the user that the account was added to a remote user's
 * buddy list.
 *
 * This will present a dialog informing the user that he was added to the
 * remote user's buddy list.
 *
 * @param account     The account that was added.
 * @param remote_user The name of the user that added this account.
 * @param id          The optional ID of the local account. Rarely used.
 * @param alias       The optional alias of the user.
 * @param message     The optional message sent from the user adding you.
 */
void gaim_account_notify_added(GaimAccount *account, const char *remote_user,
                               const char *id, const char *alias,
                               const char *message);

/**
 * Notifies the user that the account was addded to a remote user's buddy
 * list and asks ther user if they want to add the remote user to their buddy
 * list.
 *
 * This will present a dialog informing the local user that the remote user
 * added them to the remote user's buddy list and will ask if they want to add
 * the remote user to the buddy list.
 *
 * @param account     The account that was added.
 * @param remote_user The name of the user that added this account.
 * @param id          The optional ID of the local account. Rarely used.
 * @param alias       The optional alias of the user.
 * @param message     The optional message sent from the user adding you.
 */
void gaim_account_request_add(GaimAccount *account, const char *remote_user,
                              const char *id, const char *alias,
                              const char *message);
/**
 * Requests information from the user to change the account's password.
 *
 * @param account The account to change the password on.
 */
void gaim_account_request_change_password(GaimAccount *account);

/**
 * Requests information from the user to change the account's
 * user information.
 *
 * @param account The account to change the user information on.
 */
void gaim_account_request_change_user_info(GaimAccount *account);

/**
 * Sets the account's username.
 *
 * @param account  The account.
 * @param username The username.
 */
void gaim_account_set_username(GaimAccount *account, const char *username);

/**
 * Sets the account's password.
 *
 * @param account  The account.
 * @param password The password.
 */
void gaim_account_set_password(GaimAccount *account, const char *password);

/**
 * Sets the account's alias.
 *
 * @param account The account.
 * @param alias   The alias.
 */
void gaim_account_set_alias(GaimAccount *account, const char *alias);

/**
 * Sets the account's user information
 *
 * @param account   The account.
 * @param user_info The user information.
 */
void gaim_account_set_user_info(GaimAccount *account, const char *user_info);

/**
 * Sets the account's buddy icon.
 *
 * @param account The account.
 * @param icon    The buddy icon file.
 */
void gaim_account_set_buddy_icon(GaimAccount *account, const char *icon);

/**
 * Sets the account's protocol ID.
 *
 * @param account     The account.
 * @param protocol_id The protocol ID.
 */
void gaim_account_set_protocol_id(GaimAccount *account,
								  const char *protocol_id);

/**
 * Sets the account's connection.
 *
 * @param account The account.
 * @param gc      The connection.
 */
void gaim_account_set_connection(GaimAccount *account, GaimConnection *gc);

/**
 * Sets whether or not this account should save its password.
 *
 * @param account The account.
 * @param value   @c TRUE if it should remember the password.
 */
void gaim_account_set_remember_password(GaimAccount *account, gboolean value);

/**
 * Sets whether or not this account should check for mail.
 *
 * @param account The account.
 * @param value   @c TRUE if it should check for mail.
 */
void gaim_account_set_check_mail(GaimAccount *account, gboolean value);

/**
 * Sets whether or not this account is enabled for the specified
 * UI.
 *
 * @param account The account.
 * @param ui      The UI.
 * @param value   @c TRUE if it is enabled.
 */
void gaim_account_set_enabled(GaimAccount *account, const char *ui,
			      gboolean value);

/**
 * Sets the account's proxy information.
 *
 * @param account The account.
 * @param info    The proxy information.
 */
void gaim_account_set_proxy_info(GaimAccount *account, GaimProxyInfo *info);

/**
 * Sets the account's status types.
 *
 * @param account      The account.
 * @param status_types The list of status types.
 */
void gaim_account_set_status_types(GaimAccount *account, GList *status_types);

/**
 * Activates or deactivates a status.  All changes to the statuses of
 * an account go through this function or gaim_account_set_status_vargs
 * or gaim_account_set_status_list.
 *
 * Only independent statuses can be deactivated with this. To deactivate
 * an exclusive status, activate a different (and exclusive?) status.
 *
 * @param account   The account.
 * @param status_id The ID of the status.
 * @param active    The active state.
 * @param ...       Optional NULL-terminated attributes passed for the
 *                  new status, in an id, value pair.
 */
void gaim_account_set_status(GaimAccount *account, const char *status_id,
							 gboolean active, ...);


/**
 * Activates or deactivates a status.  All changes to the statuses of
 * an account go through this function or gaim_account_set_status or
 * gaim_account_set_status_list.
 *
 * Only independent statuses can be deactivated with this. To deactivate
 * an exclusive status, activate a different (and exclusive?) status.
 *
 * @param account   The account.
 * @param status_id The ID of the status.
 * @param active    The active state.
 * @param args      The va_list of attributes.
 */
void gaim_account_set_status_vargs(GaimAccount *account,
								   const char *status_id,
								   gboolean active, va_list args);

/**
 * Activates or deactivates a status.  All changes to the statuses of
 * an account go through this function or gaim_account_set_status or
 * gaim_account_set_status_vargs.
 *
 * Only independent statuses can be deactivated with this. To deactivate
 * an exclusive status, activate a different (and exclusive?) status.
 *
 * @param account   The account.
 * @param status_id The ID of the status.
 * @param active    The active state.
 * @param attrs		A list of attributes in key/value pairs
 */
void gaim_account_set_status_list(GaimAccount *account,
								  const char *status_id,
								  gboolean active, GList *attrs);

/**
 * Clears all protocol-specific settings on an account.
 *
 * @param account The account.
 */
void gaim_account_clear_settings(GaimAccount *account);

/**
 * Sets a protocol-specific integer setting for an account.
 *
 * @param account The account.
 * @param name    The name of the setting.
 * @param value   The setting's value.
 */
void gaim_account_set_int(GaimAccount *account, const char *name, int value);

/**
 * Sets a protocol-specific string setting for an account.
 *
 * @param account The account.
 * @param name    The name of the setting.
 * @param value   The setting's value.
 */
void gaim_account_set_string(GaimAccount *account, const char *name,
							 const char *value);

/**
 * Sets a protocol-specific boolean setting for an account.
 *
 * @param account The account.
 * @param name    The name of the setting.
 * @param value   The setting's value.
 */
void gaim_account_set_bool(GaimAccount *account, const char *name,
						   gboolean value);

/**
 * Sets a UI-specific integer setting for an account.
 *
 * @param account The account.
 * @param ui      The UI name.
 * @param name    The name of the setting.
 * @param value   The setting's value.
 */
void gaim_account_set_ui_int(GaimAccount *account, const char *ui,
							 const char *name, int value);

/**
 * Sets a UI-specific string setting for an account.
 *
 * @param account The account.
 * @param ui      The UI name.
 * @param name    The name of the setting.
 * @param value   The setting's value.
 */
void gaim_account_set_ui_string(GaimAccount *account, const char *ui,
								const char *name, const char *value);

/**
 * Sets a UI-specific boolean setting for an account.
 *
 * @param account The account.
 * @param ui      The UI name.
 * @param name    The name of the setting.
 * @param value   The setting's value.
 */
void gaim_account_set_ui_bool(GaimAccount *account, const char *ui,
							  const char *name, gboolean value);

/**
 * Returns whether or not the account is connected.
 *
 * @param account The account.
 *
 * @return @c TRUE if connected, or @c FALSE otherwise.
 */
gboolean gaim_account_is_connected(const GaimAccount *account);

/**
 * Returns whether or not the account is connecting.
 *
 * @param account The account.
 *
 * @return @c TRUE if connecting, or @c FALSE otherwise.
 */
gboolean gaim_account_is_connecting(const GaimAccount *account);

/**
 * Returns whether or not the account is disconnected.
 *
 * @param account The account.
 *
 * @return @c TRUE if disconnected, or @c FALSE otherwise.
 */
gboolean gaim_account_is_disconnected(const GaimAccount *account);

/**
 * Returns the account's username.
 *
 * @param account The account.
 *
 * @return The username.
 */
const char *gaim_account_get_username(const GaimAccount *account);

/**
 * Returns the account's password.
 *
 * @param account The account.
 *
 * @return The password.
 */
const char *gaim_account_get_password(const GaimAccount *account);

/**
 * Returns the account's alias.
 *
 * @param account The account.
 *
 * @return The alias.
 */
const char *gaim_account_get_alias(const GaimAccount *account);

/**
 * Returns the account's user information.
 *
 * @param account The account.
 *
 * @return The user information.
 */
const char *gaim_account_get_user_info(const GaimAccount *account);

/**
 * Returns the account's buddy icon filename.
 *
 * @param account The account.
 *
 * @return The buddy icon filename.
 */
const char *gaim_account_get_buddy_icon(const GaimAccount *account);

/**
 * Returns the account's protocol ID.
 *
 * @param account The account.
 *
 * @return The protocol ID.
 */
const char *gaim_account_get_protocol_id(const GaimAccount *account);

/**
 * Returns the account's protocol name.
 *
 * @param account The account.
 *
 * @return The protocol name.
 */
const char *gaim_account_get_protocol_name(const GaimAccount *account);

/**
 * Returns the account's connection.
 *
 * @param account The account.
 *
 * @return The connection.
 */
GaimConnection *gaim_account_get_connection(const GaimAccount *account);

/**
 * Returns whether or not this account should save its password.
 *
 * @param account The account.
 *
 * @return @c TRUE if it should remember the password.
 */
gboolean gaim_account_get_remember_password(const GaimAccount *account);

/**
 * Returns whether or not this account should check for mail.
 *
 * @param account The account.
 *
 * @return @c TRUE if it should check for mail.
 */
gboolean gaim_account_get_check_mail(const GaimAccount *account);

/**
 * Returns whether or not this account is enabled for the
 * specified UI.
 *
 * @param account The account.
 * @param ui      The UI.
 *
 * @return @c TRUE if it enabled on this UI.
 */
gboolean gaim_account_get_enabled(const GaimAccount *account,
				  const char *ui);

/**
 * Returns the account's proxy information.
 *
 * @param account The account.
 *
 * @return The proxy information.
 */
GaimProxyInfo *gaim_account_get_proxy_info(const GaimAccount *account);

/**
 * Returns the active status for this account.  This looks through
 * the GaimPresence associated with this account and returns the
 * GaimStatus that has its active flag set to "TRUE."  There can be
 * only one active GaimStatus in a GaimPresence.
 *
 * @param account   The account.
 *
 * @return The active status.
 */
GaimStatus *gaim_account_get_active_status(const GaimAccount *account);

/**
 * Returns the account status with the specified ID.
 *
 * Note that this works differently than gaim_buddy_get_status() in that
 * it will only return NULL if the status was not registered.
 *
 * @param account   The account.
 * @param status_id The status ID.
 *
 * @return The status, or NULL if it was never registered.
 */
GaimStatus *gaim_account_get_status(const GaimAccount *account,
									const char *status_id);

/**
 * Returns the account status type with the specified ID.
 *
 * @param account The account.
 * @param id      The ID of the status type to find.
 *
 * @return The status type if found, or NULL.
 */
GaimStatusType *gaim_account_get_status_type(const GaimAccount *account,
											 const char *id);

/**
 * Returns the account status type with the specified primitive.
 * Note: It is possible for an account to have more than one
 * GaimStatusType with the same primitive.  In this case, the
 * first GaimStatusType is returned.
 *
 * @param account   The account.
 * @param primitive The type of the status type to find.
 *
 * @return The status if found, or NULL.
 */
GaimStatusType *gaim_account_get_status_type_with_primitive(
							const GaimAccount *account,
							GaimStatusPrimitive primitive);

/**
 * Returns the account's presence.
 *
 * @param account The account.
 *
 * @return The account's presence.
 */
GaimPresence *gaim_account_get_presence(const GaimAccount *account);

/**
 * Returns whether or not an account status is active.
 *
 * @param account   The account.
 * @param status_id The status ID.
 *
 * @return TRUE if active, or FALSE if not.
 */
gboolean gaim_account_is_status_active(const GaimAccount *account,
									   const char *status_id);

/**
 * Returns the account's status types.
 *
 * @param account The account.
 *
 * @return The account's status types.
 */
const GList *gaim_account_get_status_types(const GaimAccount *account);

/**
 * Returns a protocol-specific integer setting for an account.
 *
 * @param account       The account.
 * @param name          The name of the setting.
 * @param default_value The default value.
 *
 * @return The value.
 */
int gaim_account_get_int(const GaimAccount *account, const char *name,
						 int default_value);

/**
 * Returns a protocol-specific string setting for an account.
 *
 * @param account       The account.
 * @param name          The name of the setting.
 * @param default_value The default value.
 *
 * @return The value.
 */
const char *gaim_account_get_string(const GaimAccount *account,
									const char *name,
									const char *default_value);

/**
 * Returns a protocol-specific boolean setting for an account.
 *
 * @param account       The account.
 * @param name          The name of the setting.
 * @param default_value The default value.
 *
 * @return The value.
 */
gboolean gaim_account_get_bool(const GaimAccount *account, const char *name,
							   gboolean default_value);

/**
 * Returns a UI-specific integer setting for an account.
 *
 * @param account       The account.
 * @param ui            The UI name.
 * @param name          The name of the setting.
 * @param default_value The default value.
 *
 * @return The value.
 */
int gaim_account_get_ui_int(const GaimAccount *account, const char *ui,
							const char *name, int default_value);

/**
 * Returns a UI-specific string setting for an account.
 *
 * @param account       The account.
 * @param ui            The UI name.
 * @param name          The name of the setting.
 * @param default_value The default value.
 *
 * @return The value.
 */
const char *gaim_account_get_ui_string(const GaimAccount *account,
									   const char *ui, const char *name,
									   const char *default_value);

/**
 * Returns a UI-specific boolean setting for an account.
 *
 * @param account       The account.
 * @param ui            The UI name.
 * @param name          The name of the setting.
 * @param default_value The default value.
 *
 * @return The value.
 */
gboolean gaim_account_get_ui_bool(const GaimAccount *account, const char *ui,
								  const char *name, gboolean default_value);


/**
 * Returns the system log for an account.
 *
 * @param account The account.
 * @param create  Should it be created if it doesn't exist?
 *
 * @return The log.
 *
 * @note Callers should almost always pass @c FALSE for @a create.
 *       Passing @c TRUE could result in an existing log being reopened,
 *       if the log has already been closed, which not all loggers deal
 *       with appropriately.
 */
GaimLog *gaim_account_get_log(GaimAccount *account, gboolean create);

/**
 * Frees the system log of an account
 *
 * @param account The account.
 */
void gaim_account_destroy_log(GaimAccount *account);

/**
 * Adds a buddy to the server-side buddy list for the specified account.
 *
 * @param account The account.
 * @param buddy The buddy to add.
 */
void gaim_account_add_buddy(GaimAccount *account, GaimBuddy *buddy);
/**
 * Adds a list of buddies to the server-side buddy list.
 *
 * @param account The account.
 * @param buddies The list of GaimBlistNodes representing the buddies to add.
 */
void gaim_account_add_buddies(GaimAccount *account, GList *buddies);

/**
 * Removes a buddy from the server-side buddy list.
 *
 * @param account The account.
 * @param buddy The buddy to remove.
 * @param group The group to remove the buddy from.
 */
void gaim_account_remove_buddy(GaimAccount *account, GaimBuddy *buddy,
								GaimGroup *group);

/**
 * Removes a list of buddies from the server-side buddy list.
 *
 * @note The lists buddies and groups are parallel lists.  Be sure that node n of
 *       groups matches node n of buddies.
 *
 * @param account The account.
 * @param buddies The list of buddies to remove.
 * @param groups The list of groups to remove buddies from.  Each node of this
 *               list should match the corresponding node of buddies.
 */
void gaim_account_remove_buddies(GaimAccount *account, GList *buddies,
									GList *groups);

/**
 * Removes a group from the server-side buddy list.
 *
 * @param account The account.
 * @param group The group to remove.
 */
void gaim_account_remove_group(GaimAccount *account, GaimGroup *group);

/**
 * Changes the password on the specified account.
 *
 * @param account The account.
 * @param orig_pw The old password.
 * @param new_pw The new password.
 */
void gaim_account_change_password(GaimAccount *account, const char *orig_pw,
									const char *new_pw);

/**
 * Whether the account supports sending offline messages to buddy.
 *
 * @param account The account
 * @param buddy   The buddy
 */
gboolean gaim_account_supports_offline_message(GaimAccount *account, GaimBuddy *buddy);

/*@}*/

/**************************************************************************/
/** @name Accounts API                                                    */
/**************************************************************************/
/*@{*/

/**
 * Adds an account to the list of accounts.
 *
 * @param account The account.
 */
void gaim_accounts_add(GaimAccount *account);

/**
 * Removes an account from the list of accounts.
 *
 * @param account The account.
 */
void gaim_accounts_remove(GaimAccount *account);

/**
 * Deletes an account.
 *
 * This will remove any buddies from the buddy list that belong to this
 * account, buddy pounces that belong to this account, and will also
 * destroy @a account.
 *
 * @param account The account.
 */
void gaim_accounts_delete(GaimAccount *account);

/**
 * Reorders an account.
 *
 * @param account   The account to reorder.
 * @param new_index The new index for the account.
 */
void gaim_accounts_reorder(GaimAccount *account, gint new_index);

/**
 * Returns a list of all accounts.
 *
 * @return A list of all accounts.
 */
GList *gaim_accounts_get_all(void);

/**
 * Returns a list of all enabled accounts
 *
 * @return A list of all enabled accounts. The list is owned
 *         by the caller, and must be g_list_free()d to avoid
 *         leaking the nodes.
 */
GList *gaim_accounts_get_all_active(void);

/**
 * Finds an account with the specified name and protocol id.
 *
 * @param name     The account username.
 * @param protocol The account protocol ID.
 *
 * @return The account, if found, or @c FALSE otherwise.
 */
GaimAccount *gaim_accounts_find(const char *name, const char *protocol);

/**
 * This is called by the core after all subsystems and what
 * not have been initialized.  It sets all enabled accounts
 * to their startup status by signing them on, setting them
 * away, etc.
 *
 * You probably shouldn't call this unless you really know
 * what you're doing.
 */
void gaim_accounts_restore_current_statuses(void);

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
void gaim_accounts_set_ui_ops(GaimAccountUiOps *ops);

/**
 * Returns the UI operations structure used for accounts.
 *
 * @return The UI operations structure in use.
 */
GaimAccountUiOps *gaim_accounts_get_ui_ops(void);

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
void *gaim_accounts_get_handle(void);

/**
 * Initializes the accounts subsystem.
 */
void gaim_accounts_init(void);

/**
 * Uninitializes the accounts subsystem.
 */
void gaim_accounts_uninit(void);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* _GAIM_ACCOUNT_H_ */
