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
#ifndef _GAIM_ACCOUNTS_H_
#define _GAIM_ACCOUNTS_H_

#include <glib.h>

typedef struct _GaimAccountUiOps GaimAccountUiOps;
typedef struct _GaimAccount      GaimAccount;

typedef gboolean (*GaimFilterAccountFunc)(GaimAccount *account);

#include "connection.h"
#include "proxy.h"
#include "prpl.h"
#include "log.h"

struct _GaimAccountUiOps
{
	void (*notify_added)(GaimAccount *account, const char *remote_user,
						 const char *id, const char *alias,
						 const char *message);
};

struct _GaimAccount
{
	char *username;             /**< The username.                        */
	char *alias;                /**< The current alias.                   */
	char *password;             /**< The account password.                */
	char *user_info;            /**< User information.                    */

	char *buddy_icon;           /**< The buddy icon.                      */

	gboolean remember_pass;     /**< Remember the password.               */

	char *protocol_id;          /**< The ID of the protocol.              */

	GaimConnection *gc;         /**< The connection handle.               */

	GHashTable *settings;       /**< Protocol-specific settings.          */
	GHashTable *ui_settings;    /**< UI-specific settings.                */

	char *ip;                   /**< The IP address for transfers.        */
	GaimProxyInfo *proxy_info;  /**< Proxy information.  This will be set */
								/*   to NULL when the account inherits    */
								/*   proxy settings from global prefs.    */

	GSList *permit;             /**< Permit list.                         */
	GSList *deny;               /**< Deny list.                           */
	int perm_deny;              /**< The permit/deny setting.             */
 	GaimLog *system_log;        /**< The system log                       */
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
 *
 * @return The gaim connection.
 */
GaimConnection *gaim_account_connect(GaimAccount *account);

/**
 * Registers an account.
 *
 * @param account The account to register.
 *
 * @return The gaim connection.
 */
GaimConnection *gaim_account_register(GaimAccount *account);

/**
 * Disconnects from an account.
 *
 * @param account The account to disconnect from.
 *
 * @return The gaim connection.
 */
void gaim_account_disconnect(GaimAccount *account);

/**
 * Notifies the user that the account was added to a remote user's
 * buddy list.
 *
 * This will present a dialog so that the local user can add the buddy,
 * if not already added.
 *
 * @param account The account that was added.
 * @param remote_user The name of the user that added this account.
 * @param id          The optional ID of the local account. Rarely used.
 * @param alias       The optional alias of the user.
 * @param message     The optional message sent from the user adding you.
 */
void gaim_account_notify_added(GaimAccount *account, const char *remote_user,
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
 * Sets the account's protocol.
 *
 * @param account  The account.
 * @param protocol The protocol.
 */
void gaim_account_set_protocol(GaimAccount *account, GaimProtocol protocol);

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
 * Sets whether or not this account should auto-login for the specified
 * UI.
 *
 * @param account The account.
 * @param ui      The UI.
 * @param value   @c TRUE if it should check for mail.
 */
void gaim_account_set_auto_login(GaimAccount *account, const char *ui,
								 gboolean value);

/**
 * Sets the public IP address the account will use for such things
 * as file transfer.
 *
 * @param account The account.
 * @param ip      The IP address.
 */
void gaim_account_set_public_ip(GaimAccount *account, const char *ip);

/**
 * Sets the account's proxy information.
 *
 * @param account The account.
 * @param info    The proxy information.
 */
void gaim_account_set_proxy_info(GaimAccount *account, GaimProxyInfo *info);

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
 * Returns the account's protocol.
 *
 * @param account The account.
 *
 * @return The protocol.
 */
GaimProtocol gaim_account_get_protocol(const GaimAccount *account);

/**
 * Returns the account's protocol ID.
 *
 * @param account The account.
 *
 * @return The protocol ID.
 */
const char *gaim_account_get_protocol_id(const GaimAccount *account);

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
 * Returns whether or not this account should auto-login for the
 * specified UI.
 *
 * @param account The account.
 * @param ui      The UI.
 *
 * @return @c TRUE if it should auto-login on this UI.
 */
gboolean gaim_account_get_auto_login(const GaimAccount *account,
									 const char *ui);

/**
 * Returns the account's public IP address.
 *
 * @param account The account.
 *
 * @return The IP address.
 */
const char *gaim_account_get_public_ip(const GaimAccount *account);

/**
 * Returns the account's proxy information.
 *
 * @param account The account.
 *
 * @return The proxy information.
 */
GaimProxyInfo *gaim_account_get_proxy_info(const GaimAccount *account);

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
 * Create it if it doesn't already exist.
 *
 * @param gc The account.
 *
 * @return The log.
 */
GaimLog *gaim_account_get_log(GaimAccount *account);

/**
 * Frees the system log of an account
 *
 * @param gc The account.
 */
	void gaim_account_destroy_log(GaimAccount *account);

/*@}*/

/**************************************************************************/
/** @name Accounts API                                                    */
/**************************************************************************/
/*@{*/

/**
 * Loads the accounts.
 */
gboolean gaim_accounts_load();

/**
 * Force an immediate write of accounts.
 */
void gaim_accounts_sync();

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
 * Auto-logins to all accounts set to auto-login under the specified UI.
 *
 * @param ui The UI.
 */
void gaim_accounts_auto_login(const char *ui);

/**
 * Reorders an account.
 *
 * @param account   The account to reorder.
 * @param new_index The new index for the account.
 */
void gaim_accounts_reorder(GaimAccount *account, size_t new_index);

/**
 * Returns a list of all accounts.
 *
 * @return A list of all accounts.
 */
GList *gaim_accounts_get_all(void);

/**
 * Finds an account with the specified name and protocol id.
 *
 * @param name     The account username.
 * @param protocol The account protocol ID.
 *
 * @return The account, if found, or @c FALSE otherwise.
 */
GaimAccount *gaim_accounts_find(const char *name, const char *protocol);

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

#endif /* _GAIM_ACCOUNTS_H_ */
