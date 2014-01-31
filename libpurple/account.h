/**
 * @file account.h Account API
 * @ingroup core
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
#ifndef _PURPLE_ACCOUNT_H_
#define _PURPLE_ACCOUNT_H_

#include <glib.h>
#include <glib-object.h>

#define PURPLE_TYPE_ACCOUNT             (purple_account_get_type())
#define PURPLE_ACCOUNT(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_ACCOUNT, PurpleAccount))
#define PURPLE_ACCOUNT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_ACCOUNT, PurpleAccountClass))
#define PURPLE_IS_ACCOUNT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_ACCOUNT))
#define PURPLE_IS_ACCOUNT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_ACCOUNT))
#define PURPLE_ACCOUNT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_ACCOUNT, PurpleAccountClass))

/** @copydoc _PurpleAccount */
typedef struct _PurpleAccount       PurpleAccount;
/** @copydoc _PurpleAccountClass */
typedef struct _PurpleAccountClass  PurpleAccountClass;

typedef gboolean (*PurpleFilterAccountFunc)(PurpleAccount *account);
typedef void (*PurpleAccountRequestAuthorizationCb)(const char *, void *);
typedef void (*PurpleAccountRegistrationCb)(PurpleAccount *account, gboolean succeeded, void *user_data);
typedef void (*PurpleAccountUnregistrationCb)(PurpleAccount *account, gboolean succeeded, void *user_data);
typedef void (*PurpleSetPublicAliasSuccessCallback)(PurpleAccount *account, const char *new_alias);
typedef void (*PurpleSetPublicAliasFailureCallback)(PurpleAccount *account, const char *error);
typedef void (*PurpleGetPublicAliasSuccessCallback)(PurpleAccount *account, const char *alias);
typedef void (*PurpleGetPublicAliasFailureCallback)(PurpleAccount *account, const char *error);

#include "connection.h"
#include "log.h"
#include "proxy.h"
#include "prpl.h"
#include "status.h"
#include "keyring.h"
#include "xmlnode.h"

/**
 * Account request types.
 */
typedef enum
{
	PURPLE_ACCOUNT_REQUEST_AUTHORIZATION = 0 /* Account authorization request */
} PurpleAccountRequestType;

/**
 * Account request response types
 */
typedef enum
{
	PURPLE_ACCOUNT_RESPONSE_IGNORE = -2,
	PURPLE_ACCOUNT_RESPONSE_DENY = -1,
	PURPLE_ACCOUNT_RESPONSE_PASS = 0,
	PURPLE_ACCOUNT_RESPONSE_ACCEPT = 1
} PurpleAccountRequestResponse;

/**
 * Privacy data types.
 */
typedef enum
{
	PURPLE_ACCOUNT_PRIVACY_ALLOW_ALL = 1,
	PURPLE_ACCOUNT_PRIVACY_DENY_ALL,
	PURPLE_ACCOUNT_PRIVACY_ALLOW_USERS,
	PURPLE_ACCOUNT_PRIVACY_DENY_USERS,
	PURPLE_ACCOUNT_PRIVACY_ALLOW_BUDDYLIST
} PurpleAccountPrivacyType;

/**
 * Structure representing an account.
 */
struct _PurpleAccount
{
	GObject gparent;

	/** The UI data associated with this account. This is a convenience
	 *  field provided to the UIs -- it is not used by the libpurple core.
	 */
	gpointer ui_data;
};

/**
 * PurpleAccountClass:
 *
 * The base class for all #PurpleAccount's.
 */
struct _PurpleAccountClass {
	GObjectClass parent_class;

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

/**************************************************************************/
/** @name Account API                                                     */
/**************************************************************************/
/*@{*/

/**
 * Returns the GType for the Account object.
 */
GType purple_account_get_type(void);

/**
 * Creates a new account.
 *
 * @username:    The username.
 * @protocol_id: The protocol ID.
 *
 * Returns: The new account.
 */
PurpleAccount *purple_account_new(const char *username, const char *protocol_id);

/**
 * Connects to an account.
 *
 * @account: The account to connect to.
 */
void purple_account_connect(PurpleAccount *account);

/**
 * Sets the callback for successful registration.
 *
 * @account:	The account for which this callback should be used
 * @cb:	The callback
 * @user_data:	The user data passed to the callback
 */
void purple_account_set_register_callback(PurpleAccount *account, PurpleAccountRegistrationCb cb, void *user_data);

/**
 * Registers an account.
 *
 * @account: The account to register.
 */
void purple_account_register(PurpleAccount *account);

/**
 * Registration of the account was completed.
 * Calls the registration call-back set with purple_account_set_register_callback().
 *
 * @account: The account being registered.
 * @succeeded: Was the account registration successful?
 */
void purple_account_register_completed(PurpleAccount *account, gboolean succeeded);

/**
 * Unregisters an account (deleting it from the server).
 *
 * @account: The account to unregister.
 * @cb: Optional callback to be called when unregistration is complete
 * @user_data: user data to pass to the callback
 */
void purple_account_unregister(PurpleAccount *account, PurpleAccountUnregistrationCb cb, void *user_data);

/**
 * Disconnects from an account.
 *
 * @account: The account to disconnect from.
 */
void purple_account_disconnect(PurpleAccount *account);

/**
 * Indicates if the account is currently being disconnected.
 *
 * @account: The account
 *
 * Returns: TRUE if the account is being disconnected.
 */
gboolean purple_account_is_disconnecting(const PurpleAccount *account);

/**
 * Notifies the user that the account was added to a remote user's
 * buddy list.
 *
 * This will present a dialog informing the user that he was added to the
 * remote user's buddy list.
 *
 * @account:     The account that was added.
 * @remote_user: The name of the user that added this account.
 * @id:          The optional ID of the local account. Rarely used.
 * @alias:       The optional alias of the user.
 * @message:     The optional message sent from the user adding you.
 */
void purple_account_notify_added(PurpleAccount *account, const char *remote_user,
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
 * @account:     The account that was added.
 * @remote_user: The name of the user that added this account.
 * @id:          The optional ID of the local account. Rarely used.
 * @alias:       The optional alias of the user.
 * @message:     The optional message sent from the user adding you.
 */
void purple_account_request_add(PurpleAccount *account, const char *remote_user,
                              const char *id, const char *alias,
                              const char *message);

/**
 * Notifies the user that a remote user has wants to add the local user
 * to his or her buddy list and requires authorization to do so.
 *
 * This will present a dialog informing the user of this and ask if the
 * user authorizes or denies the remote user from adding him.
 *
 * @account:      The account that was added
 * @remote_user:  The name of the user that added this account.
 * @id:           The optional ID of the local account. Rarely used.
 * @alias:        The optional alias of the remote user.
 * @message:      The optional message sent by the user wanting to add you.
 * @on_list:      Is the remote user already on the buddy list?
 * @auth_cb:      The callback called when the local user accepts
 * @deny_cb:      The callback called when the local user rejects
 * @user_data:    Data to be passed back to the above callbacks
 *
 * Returns: A UI-specific handle.
 */
void *purple_account_request_authorization(PurpleAccount *account, const char *remote_user,
					const char *id, const char *alias, const char *message, gboolean on_list,
					PurpleAccountRequestAuthorizationCb auth_cb, PurpleAccountRequestAuthorizationCb deny_cb, void *user_data);

/**
 * Close account requests registered for the given PurpleAccount
 *
 * @account:	   The account for which requests should be closed
 */
void purple_account_request_close_with_account(PurpleAccount *account);

/**
 * Close the account request for the given ui handle
 *
 * @ui_handle:	   The ui specific handle for which requests should be closed
 */
void purple_account_request_close(void *ui_handle);

/**
 * Requests a password from the user for the account. Does not set the
 * account password on success; do that in ok_cb if desired.
 *
 * @account:     The account to request the password for.
 * @ok_cb:       The callback for the OK button.
 * @cancel_cb:   The callback for the cancel button.
 * @user_data:   User data to be passed into callbacks.
 */
void purple_account_request_password(PurpleAccount *account, GCallback ok_cb,
				     GCallback cancel_cb, void *user_data);

/**
 * Requests information from the user to change the account's password.
 *
 * @account: The account to change the password on.
 */
void purple_account_request_change_password(PurpleAccount *account);

/**
 * Requests information from the user to change the account's
 * user information.
 *
 * @account: The account to change the user information on.
 */
void purple_account_request_change_user_info(PurpleAccount *account);

/**
 * Sets the account's username.
 *
 * @account:  The account.
 * @username: The username.
 */
void purple_account_set_username(PurpleAccount *account, const char *username);

/**
 * Sets the account's password.
 *
 * The password in the keyring might not be immediately updated, but the cached
 * version will be, and it is therefore safe to read the password back before
 * the callback has been triggered. One can also set a NULL callback if
 * notification of saving to the keyring is not required.
 *
 * @account:  The account.
 * @password: The password.
 * @cb:       A callback for once the password is saved.
 * @data:     A pointer to be passed to the callback.
 */
void purple_account_set_password(PurpleAccount *account, const gchar *password,
	PurpleKeyringSaveCallback cb, gpointer data);

/**
 * Sets the account's private alias.
 *
 * @account: The account.
 * @alias:   The alias.
 */
void purple_account_set_private_alias(PurpleAccount *account, const char *alias);

/**
 * Sets the account's user information
 *
 * @account:   The account.
 * @user_info: The user information.
 */
void purple_account_set_user_info(PurpleAccount *account, const char *user_info);

/**
 * Sets the account's buddy icon path.
 *
 * @account: The account.
 * @path:	  The buddy icon non-cached path.
 */
void purple_account_set_buddy_icon_path(PurpleAccount *account, const char *path);

/**
 * Sets the account's protocol ID.
 *
 * @account:     The account.
 * @protocol_id: The protocol ID.
 */
void purple_account_set_protocol_id(PurpleAccount *account,
								  const char *protocol_id);

/**
 * Sets the account's connection.
 *
 * @account: The account.
 * @gc:      The connection.
 */
void purple_account_set_connection(PurpleAccount *account, PurpleConnection *gc);

/**
 * Sets whether or not this account should save its password.
 *
 * @account: The account.
 * @value:   %TRUE if it should remember the password.
 */
void purple_account_set_remember_password(PurpleAccount *account, gboolean value);

/**
 * Sets whether or not this account should check for mail.
 *
 * @account: The account.
 * @value:   %TRUE if it should check for mail.
 */
void purple_account_set_check_mail(PurpleAccount *account, gboolean value);

/**
 * Sets whether or not this account is enabled for the specified
 * UI.
 *
 * @account: The account.
 * @ui:      The UI.
 * @value:   %TRUE if it is enabled.
 */
void purple_account_set_enabled(PurpleAccount *account, const char *ui,
			      gboolean value);

/**
 * Sets the account's proxy information.
 *
 * @account: The account.
 * @info:    The proxy information.
 */
void purple_account_set_proxy_info(PurpleAccount *account, PurpleProxyInfo *info);

/**
 * Sets the account's privacy type.
 *
 * @account:      The account.
 * @privacy_type: The privacy type.
 */
void purple_account_set_privacy_type(PurpleAccount *account, PurpleAccountPrivacyType privacy_type);

/**
 * Sets the account's status types.
 *
 * @account:      The account.
 * @status_types: The list of status types.
 */
void purple_account_set_status_types(PurpleAccount *account, GList *status_types);

/**
 * Variadic version of purple_account_set_status_list(); the variadic list
 * replaces @a attrs, and should be <tt>NULL</tt>-terminated.
 *
 * @copydoc purple_account_set_status_list()
 */
void purple_account_set_status(PurpleAccount *account, const char *status_id,
	gboolean active, ...) G_GNUC_NULL_TERMINATED;


/**
 * Activates or deactivates a status.  All changes to the statuses of
 * an account go through this function or purple_account_set_status().
 *
 * You can only deactivate an exclusive status by activating another exclusive
 * status.  So, if @a status_id is an exclusive status and @a active is @c
 * FALSE, this function does nothing.
 *
 * @account:   The account.
 * @status_id: The ID of the status.
 * @active:    Whether @a status_id is to be activated (<tt>TRUE</tt>) or
 *                  deactivated (<tt>FALSE</tt>).
 * @attrs:     A list of <tt>const char *</tt> attribute names followed by
 *                  <tt>const char *</tt> attribute values for the status.
 *                  (For example, one pair might be <tt>"message"</tt> followed
 *                  by <tt>"hello, talk to me!"</tt>.)
 */
void purple_account_set_status_list(PurpleAccount *account,
	const char *status_id, gboolean active, GList *attrs);

/**
 * Set a server-side (public) alias for this account.  The account
 * must already be connected.
 *
 * Currently, the public alias is not stored locally, although this
 * may change in a later version.
 *
 * @account:    The account
 * @alias:      The new public alias for this account or NULL
 *                   to unset the alias/nickname (or return it to
 *                   a protocol-specific "default", like the username)
 * @success_cb: A callback which will be called if the alias
 *                   is successfully set on the server (or NULL).
 * @failure_cb: A callback which will be called if the alias
 *                   is not successfully set on the server (or NULL).
 */
void purple_account_set_public_alias(PurpleAccount *account,
	const char *alias, PurpleSetPublicAliasSuccessCallback success_cb,
	PurpleSetPublicAliasFailureCallback failure_cb);

/**
 * Fetch the server-side (public) alias for this account.  The account
 * must already be connected.
 *
 * @account:    The account
 * @success_cb: A callback which will be called with the alias
 * @failure_cb: A callback which will be called if the prpl is
 *                   unable to retrieve the server-side alias.
 */
void purple_account_get_public_alias(PurpleAccount *account,
	PurpleGetPublicAliasSuccessCallback success_cb,
	PurpleGetPublicAliasFailureCallback failure_cb);

/**
 * Return whether silence suppression is used during voice call.
 *
 * @account: The account.
 *
 * Returns: %TRUE if suppression is used, or %FALSE if not.
 */
gboolean purple_account_get_silence_suppression(const PurpleAccount *account);

/**
 * Sets whether silence suppression is used during voice call.
 *
 * @account: The account.
 * @value:   %TRUE if suppression should be used.
 */
void purple_account_set_silence_suppression(PurpleAccount *account,
											gboolean value);

/**
 * Clears all protocol-specific settings on an account.
 *
 * @account: The account.
 */
void purple_account_clear_settings(PurpleAccount *account);

/**
 * Removes an account-specific setting by name.
 *
 * @account: The account.
 * @setting: The setting to remove.
 */
void purple_account_remove_setting(PurpleAccount *account, const char *setting);

/**
 * Sets a protocol-specific integer setting for an account.
 *
 * @account: The account.
 * @name:    The name of the setting.
 * @value:   The setting's value.
 */
void purple_account_set_int(PurpleAccount *account, const char *name, int value);

/**
 * Sets a protocol-specific string setting for an account.
 *
 * @account: The account.
 * @name:    The name of the setting.
 * @value:   The setting's value.
 */
void purple_account_set_string(PurpleAccount *account, const char *name,
							 const char *value);

/**
 * Sets a protocol-specific boolean setting for an account.
 *
 * @account: The account.
 * @name:    The name of the setting.
 * @value:   The setting's value.
 */
void purple_account_set_bool(PurpleAccount *account, const char *name,
						   gboolean value);

/**
 * Sets a UI-specific integer setting for an account.
 *
 * @account: The account.
 * @ui:      The UI name.
 * @name:    The name of the setting.
 * @value:   The setting's value.
 */
void purple_account_set_ui_int(PurpleAccount *account, const char *ui,
							 const char *name, int value);

/**
 * Sets a UI-specific string setting for an account.
 *
 * @account: The account.
 * @ui:      The UI name.
 * @name:    The name of the setting.
 * @value:   The setting's value.
 */
void purple_account_set_ui_string(PurpleAccount *account, const char *ui,
								const char *name, const char *value);

/**
 * Sets a UI-specific boolean setting for an account.
 *
 * @account: The account.
 * @ui:      The UI name.
 * @name:    The name of the setting.
 * @value:   The setting's value.
 */
void purple_account_set_ui_bool(PurpleAccount *account, const char *ui,
							  const char *name, gboolean value);

/**
 * Set the UI data associated with this account.
 *
 * @account: The account.
 * @ui_data: A pointer to associate with this object.
 */
void purple_account_set_ui_data(PurpleAccount *account, gpointer ui_data);

/**
 * Returns the UI data associated with this account.
 *
 * @account: The account.
 *
 * Returns: The UI data associated with this account.  This is a
 *         convenience field provided to the UIs--it is not
 *         used by the libuprple core.
 */
gpointer purple_account_get_ui_data(const PurpleAccount *account);

/**
 * Returns whether or not the account is connected.
 *
 * @account: The account.
 *
 * Returns: %TRUE if connected, or %FALSE otherwise.
 */
gboolean purple_account_is_connected(const PurpleAccount *account);

/**
 * Returns whether or not the account is connecting.
 *
 * @account: The account.
 *
 * Returns: %TRUE if connecting, or %FALSE otherwise.
 */
gboolean purple_account_is_connecting(const PurpleAccount *account);

/**
 * Returns whether or not the account is disconnected.
 *
 * @account: The account.
 *
 * Returns: %TRUE if disconnected, or %FALSE otherwise.
 */
gboolean purple_account_is_disconnected(const PurpleAccount *account);

/**
 * Returns the account's username.
 *
 * @account: The account.
 *
 * Returns: The username.
 */
const char *purple_account_get_username(const PurpleAccount *account);

/**
 * Reads the password for the account.
 *
 * This is an asynchronous call, that will return the password in a callback
 * once it has been read from the keyring. If the account is connected, and you
 * require the password immediately, then consider using @ref
 * purple_connection_get_password instead.
 *
 * @account: The account.
 * @cb:      The callback to give the password.
 * @data:    A pointer passed to the callback.
 */
void purple_account_get_password(PurpleAccount *account,
	PurpleKeyringReadCallback cb, gpointer data);

/**
 * Returns the account's private alias.
 *
 * @account: The account.
 *
 * Returns: The alias.
 */
const char *purple_account_get_private_alias(const PurpleAccount *account);

/**
 * Returns the account's user information.
 *
 * @account: The account.
 *
 * Returns: The user information.
 */
const char *purple_account_get_user_info(const PurpleAccount *account);

/**
 * Gets the account's buddy icon path.
 *
 * @account: The account.
 *
 * Returns: The buddy icon's non-cached path.
 */
const char *purple_account_get_buddy_icon_path(const PurpleAccount *account);

/**
 * Returns the account's protocol ID.
 *
 * @account: The account.
 *
 * Returns: The protocol ID.
 */
const char *purple_account_get_protocol_id(const PurpleAccount *account);

/**
 * Returns the account's protocol name.
 *
 * @account: The account.
 *
 * Returns: The protocol name.
 */
const char *purple_account_get_protocol_name(const PurpleAccount *account);

/**
 * Returns the account's connection.
 *
 * @account: The account.
 *
 * Returns: The connection.
 */
PurpleConnection *purple_account_get_connection(const PurpleAccount *account);

/**
 * Returns a name for this account appropriate for display to the user. In
 * order of preference: the account's alias; the contact or buddy alias (if
 * the account exists on its own buddy list); the connection's display name;
 * the account's username.
 *
 * @account: The account.
 *
 * Returns: The name to display.
 */
const gchar *purple_account_get_name_for_display(const PurpleAccount *account);

/**
 * Returns whether or not this account should save its password.
 *
 * @account: The account.
 *
 * Returns: %TRUE if it should remember the password.
 */
gboolean purple_account_get_remember_password(const PurpleAccount *account);

/**
 * Returns whether or not this account should check for mail.
 *
 * @account: The account.
 *
 * Returns: %TRUE if it should check for mail.
 */
gboolean purple_account_get_check_mail(const PurpleAccount *account);

/**
 * Returns whether or not this account is enabled for the
 * specified UI.
 *
 * @account: The account.
 * @ui:      The UI.
 *
 * Returns: %TRUE if it enabled on this UI.
 */
gboolean purple_account_get_enabled(const PurpleAccount *account,
				  const char *ui);

/**
 * Returns the account's proxy information.
 *
 * @account: The account.
 *
 * Returns: The proxy information.
 */
PurpleProxyInfo *purple_account_get_proxy_info(const PurpleAccount *account);

/**
 * Returns the account's privacy type.
 *
 * @account:   The account.
 *
 * Returns: The privacy type.
 */
PurpleAccountPrivacyType purple_account_get_privacy_type(const PurpleAccount *account);

/**
 * Adds a user to the account's permit list.
 *
 * @account:    The account.
 * @name:       The name of the user to add to the list.
 * @local_only: If TRUE, only the local list is updated, and not
 *                   the server.
 *
 * Returns: TRUE if the user was added successfully, or %FALSE otherwise.
 */
gboolean purple_account_privacy_permit_add(PurpleAccount *account,
								const char *name, gboolean local_only);

/**
 * Removes a user from the account's permit list.
 *
 * @account:    The account.
 * @name:       The name of the user to add to the list.
 * @local_only: If TRUE, only the local list is updated, and not
 *                   the server.
 *
 * Returns: TRUE if the user was removed successfully, or %FALSE otherwise.
 */
gboolean purple_account_privacy_permit_remove(PurpleAccount *account,
									const char *name, gboolean local_only);

/**
 * Adds a user to the account's deny list.
 *
 * @account:    The account.
 * @name:       The name of the user to add to the list.
 * @local_only: If TRUE, only the local list is updated, and not
 *                   the server.
 *
 * Returns: TRUE if the user was added successfully, or %FALSE otherwise.
 */
gboolean purple_account_privacy_deny_add(PurpleAccount *account,
									const char *name, gboolean local_only);

/**
 * Removes a user from the account's deny list.
 *
 * @account:    The account.
 * @name:       The name of the user to add to the list.
 * @local_only: If TRUE, only the local list is updated, and not
 *                   the server.
 *
 * Returns: TRUE if the user was removed successfully, or %FALSE otherwise.
 */
gboolean purple_account_privacy_deny_remove(PurpleAccount *account,
									const char *name, gboolean local_only);

/**
 * Allow a user to send messages. If current privacy setting for the account is:
 *		PURPLE_ACCOUNT_PRIVACY_ALLOW_USERS:	The user is added to the allow-list.
 *		PURPLE_ACCOUNT_PRIVACY_DENY_USERS	:	The user is removed from the
 *		                                        deny-list.
 *		PURPLE_ACCOUNT_PRIVACY_ALLOW_ALL	:	No changes made.
 *		PURPLE_ACCOUNT_PRIVACY_DENY_ALL	:	The privacy setting is changed to
 *									PURPLE_ACCOUNT_PRIVACY_ALLOW_USERS and the
 *									user is added to the allow-list.
 *		PURPLE_ACCOUNT_PRIVACY_ALLOW_BUDDYLIST: No changes made if the user is
 *									already in the buddy-list. Otherwise the
 *									setting is changed to
 *		PURPLE_ACCOUNT_PRIVACY_ALLOW_USERS, all the buddies are added to the
 *									allow-list, and the user is also added to
 *									the allow-list.
 *
 * The changes are reflected on the server. The previous allow/deny list is not
 * restored if the privacy setting is changed.
 *
 * @account:	The account.
 * @who:		The name of the user.
 */
void purple_account_privacy_allow(PurpleAccount *account, const char *who);

/**
 * Block messages from a user. If current privacy setting for the account is:
 *		PURPLE_ACCOUNT_PRIVACY_ALLOW_USERS:	The user is removed from the
 *											allow-list.
 *		PURPLE_ACCOUNT_PRIVACY_DENY_USERS:	The user is added to the deny-list.
 *		PURPLE_ACCOUNT_PRIVACY_DENY_ALL:	No changes made.
 *		PURPLE_ACCOUNT_PRIVACY_ALLOW_ALL:	The privacy setting is changed to
 *									PURPLE_ACCOUNT_PRIVACY_DENY_USERS and the
 *									user is added to the deny-list.
 *		PURPLE_ACCOUNT_PRIVACY_ALLOW_BUDDYLIST: If the user is not in the
 *									buddy-list, then no changes made. Otherwise,
 *									the setting is changed to
 *									PURPLE_ACCOUNT_PRIVACY_ALLOW_USERS, all
 *									the buddies are added to the allow-list, and
 *									this user is removed from the list.
 *
 * The changes are reflected on the server. The previous allow/deny list is not
 * restored if the privacy setting is changed.
 *
 * @account:	The account.
 * @who:		The name of the user.
 */
void purple_account_privacy_deny(PurpleAccount *account, const char *who);

/**
 * Returns the account's permit list.
 *
 * @account:	The account.
 * Returns: (TODO const):     A list of the permitted users
 */
GSList *purple_account_privacy_get_permitted(PurpleAccount *account);

/**
 * Returns the account's deny list.
 *
 * @account:	The account.
 * Returns: (TODO const):     A list of the denied users
 */
GSList *purple_account_privacy_get_denied(PurpleAccount *account);

/**
 * Check the privacy-setting for a user.
 *
 * @account:	The account.
 * @who:		The name of the user.
 *
 * Returns: %FALSE if the specified account's privacy settings block the user
 *		or %TRUE otherwise. The meaning of "block" is protocol-dependent and
 *				generally relates to status and/or sending of messages.
 */
gboolean purple_account_privacy_check(PurpleAccount *account, const char *who);

/**
 * Returns the active status for this account.  This looks through
 * the PurplePresence associated with this account and returns the
 * PurpleStatus that has its active flag set to "TRUE."  There can be
 * only one active PurpleStatus in a PurplePresence.
 *
 * @account:   The account.
 *
 * Returns: The active status.
 */
PurpleStatus *purple_account_get_active_status(const PurpleAccount *account);

/**
 * Returns the account status with the specified ID.
 *
 * Note that this works differently than purple_buddy_get_status() in that
 * it will only return NULL if the status was not registered.
 *
 * @account:   The account.
 * @status_id: The status ID.
 *
 * Returns: The status, or NULL if it was never registered.
 */
PurpleStatus *purple_account_get_status(const PurpleAccount *account,
									const char *status_id);

/**
 * Returns the account status type with the specified ID.
 *
 * @account: The account.
 * @id:      The ID of the status type to find.
 *
 * Returns: The status type if found, or NULL.
 */
PurpleStatusType *purple_account_get_status_type(const PurpleAccount *account,
											 const char *id);

/**
 * Returns the account status type with the specified primitive.
 * Note: It is possible for an account to have more than one
 * PurpleStatusType with the same primitive.  In this case, the
 * first PurpleStatusType is returned.
 *
 * @account:   The account.
 * @primitive: The type of the status type to find.
 *
 * Returns: The status if found, or NULL.
 */
PurpleStatusType *purple_account_get_status_type_with_primitive(
							const PurpleAccount *account,
							PurpleStatusPrimitive primitive);

/**
 * Returns the account's presence.
 *
 * @account: The account.
 *
 * Returns: The account's presence.
 */
PurplePresence *purple_account_get_presence(const PurpleAccount *account);

/**
 * Returns whether or not an account status is active.
 *
 * @account:   The account.
 * @status_id: The status ID.
 *
 * Returns: TRUE if active, or FALSE if not.
 */
gboolean purple_account_is_status_active(const PurpleAccount *account,
									   const char *status_id);

/**
 * Returns the account's status types.
 *
 * @account: The account.
 *
 * Returns: (TODO const): The account's status types.
 */
GList *purple_account_get_status_types(const PurpleAccount *account);

/**
 * Returns a protocol-specific integer setting for an account.
 *
 * @account:       The account.
 * @name:          The name of the setting.
 * @default_value: The default value.
 *
 * Returns: The value.
 */
int purple_account_get_int(const PurpleAccount *account, const char *name,
						 int default_value);

/**
 * Returns a protocol-specific string setting for an account.
 *
 * @account:       The account.
 * @name:          The name of the setting.
 * @default_value: The default value.
 *
 * Returns: The value.
 */
const char *purple_account_get_string(const PurpleAccount *account,
									const char *name,
									const char *default_value);

/**
 * Returns a protocol-specific boolean setting for an account.
 *
 * @account:       The account.
 * @name:          The name of the setting.
 * @default_value: The default value.
 *
 * Returns: The value.
 */
gboolean purple_account_get_bool(const PurpleAccount *account, const char *name,
							   gboolean default_value);

/**
 * Returns a UI-specific integer setting for an account.
 *
 * @account:       The account.
 * @ui:            The UI name.
 * @name:          The name of the setting.
 * @default_value: The default value.
 *
 * Returns: The value.
 */
int purple_account_get_ui_int(const PurpleAccount *account, const char *ui,
							const char *name, int default_value);

/**
 * Returns a UI-specific string setting for an account.
 *
 * @account:       The account.
 * @ui:            The UI name.
 * @name:          The name of the setting.
 * @default_value: The default value.
 *
 * Returns: The value.
 */
const char *purple_account_get_ui_string(const PurpleAccount *account,
									   const char *ui, const char *name,
									   const char *default_value);

/**
 * Returns a UI-specific boolean setting for an account.
 *
 * @account:       The account.
 * @ui:            The UI name.
 * @name:          The name of the setting.
 * @default_value: The default value.
 *
 * Returns: The value.
 */
gboolean purple_account_get_ui_bool(const PurpleAccount *account, const char *ui,
								  const char *name, gboolean default_value);


/**
 * Returns the system log for an account.
 *
 * @account: The account.
 * @create:  Should it be created if it doesn't exist?
 *
 * Returns: The log.
 *
 * Note: Callers should almost always pass %FALSE for @a create.
 *       Passing %TRUE could result in an existing log being reopened,
 *       if the log has already been closed, which not all loggers deal
 *       with appropriately.
 */
PurpleLog *purple_account_get_log(PurpleAccount *account, gboolean create);

/**
 * Frees the system log of an account
 *
 * @account: The account.
 */
void purple_account_destroy_log(PurpleAccount *account);

/**
 * Adds a buddy to the server-side buddy list for the specified account.
 *
 * @account: The account.
 * @buddy: The buddy to add.
 * @message: The invite message.  This may be ignored by a prpl.
 */
void purple_account_add_buddy(PurpleAccount *account, PurpleBuddy *buddy, const char *message);

/**
 * Adds a list of buddies to the server-side buddy list.
 *
 * @account: The account.
 * @buddies: The list of PurpleBlistNodes representing the buddies to add.
 * @message: The invite message.  This may be ignored by a prpl.
 */
void purple_account_add_buddies(PurpleAccount *account, GList *buddies, const char *message);

/**
 * Removes a buddy from the server-side buddy list.
 *
 * @account: The account.
 * @buddy: The buddy to remove.
 * @group: The group to remove the buddy from.
 */
void purple_account_remove_buddy(PurpleAccount *account, PurpleBuddy *buddy,
								PurpleGroup *group);

/**
 * Removes a list of buddies from the server-side buddy list.
 *
 * Note: The lists buddies and groups are parallel lists.  Be sure that node n of
 *       groups matches node n of buddies.
 *
 * @account: The account.
 * @buddies: The list of buddies to remove.
 * @groups: The list of groups to remove buddies from.  Each node of this
 *               list should match the corresponding node of buddies.
 */
void purple_account_remove_buddies(PurpleAccount *account, GList *buddies,
									GList *groups);

/**
 * Removes a group from the server-side buddy list.
 *
 * @account: The account.
 * @group: The group to remove.
 */
void purple_account_remove_group(PurpleAccount *account, PurpleGroup *group);

/**
 * Changes the password on the specified account.
 *
 * @account: The account.
 * @orig_pw: The old password.
 * @new_pw: The new password.
 */
void purple_account_change_password(PurpleAccount *account, const char *orig_pw,
									const char *new_pw);

/**
 * Whether the account supports sending offline messages to buddy.
 *
 * @account: The account
 * @buddy:   The buddy
 */
gboolean purple_account_supports_offline_message(PurpleAccount *account, PurpleBuddy *buddy);

/**
 * Get the error that caused the account to be disconnected, or %NULL if the
 * account is happily connected or disconnected without an error.
 *
 * @account: The account whose error should be retrieved.
 * Returns: (TODO const):   The type of error and a human-readable description of the
 *                current error, or %NULL if there is no current error.  This
 *                pointer is guaranteed to remain valid until the @ref
 *                account-error-changed signal is emitted for @a account.
 */
const PurpleConnectionErrorInfo *purple_account_get_current_error(PurpleAccount *account);

/**
 * Clear an account's current error state, resetting it to %NULL.
 *
 * @account: The account whose error state should be cleared.
 */
void purple_account_clear_current_error(PurpleAccount *account);

/*@}*/

G_END_DECLS

#endif /* _PURPLE_ACCOUNT_H_ */
