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
/**
 * SECTION:account
 * @section_id: libpurple-account
 * @short_description: <filename>account.h</filename>
 * @title: Account Object
 */

#include <glib.h>
#include <glib-object.h>

#define PURPLE_TYPE_ACCOUNT             (purple_account_get_type())
#define PURPLE_ACCOUNT(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_ACCOUNT, PurpleAccount))
#define PURPLE_ACCOUNT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_ACCOUNT, PurpleAccountClass))
#define PURPLE_IS_ACCOUNT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_ACCOUNT))
#define PURPLE_IS_ACCOUNT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_ACCOUNT))
#define PURPLE_ACCOUNT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_ACCOUNT, PurpleAccountClass))

typedef struct _PurpleAccount       PurpleAccount;
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
#include "protocol.h"
#include "status.h"
#include "keyring.h"
#include "xmlnode.h"

/**
 * PurpleAccountRequestType:
 * @PURPLE_ACCOUNT_REQUEST_AUTHORIZATION: Account authorization request
 *
 * Account request types.
 */
typedef enum
{
	PURPLE_ACCOUNT_REQUEST_AUTHORIZATION = 0
} PurpleAccountRequestType;

/**
 * PurpleAccountRequestResponse:
 *
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
 * PurpleAccountPrivacyType:
 *
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
 * PurpleAccount:
 * @ui_data: The UI data associated with this account. This is a convenience
 *           field provided to the UIs -- it is not used by the libpurple core.
 *
 * Structure representing an account.
 */
struct _PurpleAccount
{
	GObject gparent;

	/*< public >*/
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
/* Account API                                                            */
/**************************************************************************/

/**
 * purple_account_get_type:
 *
 * Returns: The #GType for the Account object.
 */
GType purple_account_get_type(void);

/**
 * purple_account_new:
 * @username:    The username.
 * @protocol_id: The protocol ID.
 *
 * Creates a new account.
 *
 * Returns: The new account.
 */
PurpleAccount *purple_account_new(const char *username, const char *protocol_id);

/**
 * purple_account_connect:
 * @account: The account to connect to.
 *
 * Connects to an account.
 */
void purple_account_connect(PurpleAccount *account);

/**
 * purple_account_set_register_callback:
 * @account:	      The account for which this callback should be used
 * @cb: (scope call): The callback
 * @user_data:	      The user data passed to the callback
 *
 * Sets the callback for successful registration.
 */
void purple_account_set_register_callback(PurpleAccount *account, PurpleAccountRegistrationCb cb, void *user_data);

/**
 * purple_account_register:
 * @account: The account to register.
 *
 * Registers an account.
 */
void purple_account_register(PurpleAccount *account);

/**
 * purple_account_register_completed:
 * @account: The account being registered.
 * @succeeded: Was the account registration successful?
 *
 * Registration of the account was completed.
 * Calls the registration call-back set with purple_account_set_register_callback().
 */
void purple_account_register_completed(PurpleAccount *account, gboolean succeeded);

/**
 * purple_account_unregister:
 * @account: The account to unregister.
 * @cb: (scope call): Optional callback to be called when unregistration is
 *                    complete
 * @user_data:        user data to pass to the callback
 *
 * Unregisters an account (deleting it from the server).
 */
void purple_account_unregister(PurpleAccount *account, PurpleAccountUnregistrationCb cb, void *user_data);

/**
 * purple_account_disconnect:
 * @account: The account to disconnect from.
 *
 * Disconnects from an account.
 */
void purple_account_disconnect(PurpleAccount *account);

/**
 * purple_account_is_disconnecting:
 * @account: The account
 *
 * Indicates if the account is currently being disconnected.
 *
 * Returns: TRUE if the account is being disconnected.
 */
gboolean purple_account_is_disconnecting(const PurpleAccount *account);

/**
 * purple_account_notify_added:
 * @account:     The account that was added.
 * @remote_user: The name of the user that added this account.
 * @id:          The optional ID of the local account. Rarely used.
 * @alias:       The optional alias of the user.
 * @message:     The optional message sent from the user adding you.
 *
 * Notifies the user that the account was added to a remote user's
 * buddy list.
 *
 * This will present a dialog informing the user that he was added to the
 * remote user's buddy list.
 */
void purple_account_notify_added(PurpleAccount *account, const char *remote_user,
                               const char *id, const char *alias,
                               const char *message);

/**
 * purple_account_request_add:
 * @account:     The account that was added.
 * @remote_user: The name of the user that added this account.
 * @id:          The optional ID of the local account. Rarely used.
 * @alias:       The optional alias of the user.
 * @message:     The optional message sent from the user adding you.
 *
 * Notifies the user that the account was addded to a remote user's buddy
 * list and asks ther user if they want to add the remote user to their buddy
 * list.
 *
 * This will present a dialog informing the local user that the remote user
 * added them to the remote user's buddy list and will ask if they want to add
 * the remote user to the buddy list.
 */
void purple_account_request_add(PurpleAccount *account, const char *remote_user,
                              const char *id, const char *alias,
                              const char *message);

/**
 * purple_account_request_authorization:
 * @account:      The account that was added
 * @remote_user:  The name of the user that added this account.
 * @id:           The optional ID of the local account. Rarely used.
 * @alias:        The optional alias of the remote user.
 * @message:      The optional message sent by the user wanting to add you.
 * @on_list:      Is the remote user already on the buddy list?
 * @auth_cb:      (scope call): The callback called when the local user accepts
 * @deny_cb:      (scope call): The callback called when the local user rejects
 * @user_data:    Data to be passed back to the above callbacks
 *
 * Notifies the user that a remote user has wants to add the local user
 * to his or her buddy list and requires authorization to do so.
 *
 * This will present a dialog informing the user of this and ask if the
 * user authorizes or denies the remote user from adding him.
 *
 * Returns: A UI-specific handle.
 */
void *purple_account_request_authorization(PurpleAccount *account, const char *remote_user,
					const char *id, const char *alias, const char *message, gboolean on_list,
					PurpleAccountRequestAuthorizationCb auth_cb, PurpleAccountRequestAuthorizationCb deny_cb, void *user_data);

/**
 * purple_account_request_close_with_account:
 * @account:	   The account for which requests should be closed
 *
 * Close account requests registered for the given PurpleAccount
 */
void purple_account_request_close_with_account(PurpleAccount *account);

/**
 * purple_account_request_close:
 * @ui_handle:	   The ui specific handle for which requests should be closed
 *
 * Close the account request for the given ui handle
 */
void purple_account_request_close(void *ui_handle);

/**
 * purple_account_request_password:
 * @account:     The account to request the password for.
 * @ok_cb:       (scope call): The callback for the OK button.
 * @cancel_cb:   (scope call): The callback for the cancel button.
 * @user_data:   User data to be passed into callbacks.
 *
 * Requests a password from the user for the account. Does not set the
 * account password on success; do that in ok_cb if desired.
 */
void purple_account_request_password(PurpleAccount *account, GCallback ok_cb,
				     GCallback cancel_cb, void *user_data);

/**
 * purple_account_request_change_password:
 * @account: The account to change the password on.
 *
 * Requests information from the user to change the account's password.
 */
void purple_account_request_change_password(PurpleAccount *account);

/**
 * purple_account_request_change_user_info:
 * @account: The account to change the user information on.
 *
 * Requests information from the user to change the account's
 * user information.
 */
void purple_account_request_change_user_info(PurpleAccount *account);

/**
 * purple_account_set_username:
 * @account:  The account.
 * @username: The username.
 *
 * Sets the account's username.
 */
void purple_account_set_username(PurpleAccount *account, const char *username);

/**
 * purple_account_set_password:
 * @account:  The account.
 * @password: The password.
 * @cb:       (scope call): A callback for once the password is saved.
 * @data:     A pointer to be passed to the callback.
 *
 * Sets the account's password.
 *
 * The password in the keyring might not be immediately updated, but the cached
 * version will be, and it is therefore safe to read the password back before
 * the callback has been triggered. One can also set a %NULL callback if
 * notification of saving to the keyring is not required.
 */
void purple_account_set_password(PurpleAccount *account, const gchar *password,
	PurpleKeyringSaveCallback cb, gpointer data);

/**
 * purple_account_set_private_alias:
 * @account: The account.
 * @alias:   The alias.
 *
 * Sets the account's private alias.
 */
void purple_account_set_private_alias(PurpleAccount *account, const char *alias);

/**
 * purple_account_set_user_info:
 * @account:   The account.
 * @user_info: The user information.
 *
 * Sets the account's user information
 */
void purple_account_set_user_info(PurpleAccount *account, const char *user_info);

/**
 * purple_account_set_buddy_icon_path:
 * @account: The account.
 * @path:	  The buddy icon non-cached path.
 *
 * Sets the account's buddy icon path.
 */
void purple_account_set_buddy_icon_path(PurpleAccount *account, const char *path);

/**
 * purple_account_set_protocol_id:
 * @account:     The account.
 * @protocol_id: The protocol ID.
 *
 * Sets the account's protocol ID.
 */
void purple_account_set_protocol_id(PurpleAccount *account,
								  const char *protocol_id);

/**
 * purple_account_set_connection:
 * @account: The account.
 * @gc:      The connection.
 *
 * Sets the account's connection.
 */
void purple_account_set_connection(PurpleAccount *account, PurpleConnection *gc);

/**
 * purple_account_set_remember_password:
 * @account: The account.
 * @value:   %TRUE if it should remember the password.
 *
 * Sets whether or not this account should save its password.
 */
void purple_account_set_remember_password(PurpleAccount *account, gboolean value);

/**
 * purple_account_set_check_mail:
 * @account: The account.
 * @value:   %TRUE if it should check for mail.
 *
 * Sets whether or not this account should check for mail.
 */
void purple_account_set_check_mail(PurpleAccount *account, gboolean value);

/**
 * purple_account_set_enabled:
 * @account: The account.
 * @ui:      The UI.
 * @value:   %TRUE if it is enabled.
 *
 * Sets whether or not this account is enabled for the specified
 * UI.
 */
void purple_account_set_enabled(PurpleAccount *account, const char *ui,
			      gboolean value);

/**
 * purple_account_set_proxy_info:
 * @account: The account.
 * @info:    The proxy information.
 *
 * Sets the account's proxy information.
 */
void purple_account_set_proxy_info(PurpleAccount *account, PurpleProxyInfo *info);

/**
 * purple_account_set_privacy_type:
 * @account:      The account.
 * @privacy_type: The privacy type.
 *
 * Sets the account's privacy type.
 */
void purple_account_set_privacy_type(PurpleAccount *account, PurpleAccountPrivacyType privacy_type);

/**
 * purple_account_set_status_types:
 * @account:      The account.
 * @status_types: The list of status types.
 *
 * Sets the account's status types.
 */
void purple_account_set_status_types(PurpleAccount *account, GList *status_types);

/**
 * purple_account_set_status:
 * @account:   The account.
 * @status_id: The ID of the status.
 * @active:    Whether @a status_id is to be activated (%TRUE) or
 *             deactivated (%FALSE).
 * @...:       A %NULL-terminated list of pairs of <type>const char *</type>
 *             attribute name followed by <type>const char *</type> attribute
 *             value for the status. (For example, one pair might be
 *             <literal>"message"</literal> followed by
 *             <literal>"hello, talk to me!"</literal>.)
 *
 * Variadic version of purple_account_set_status_list().
 */
void purple_account_set_status(PurpleAccount *account, const char *status_id,
	gboolean active, ...) G_GNUC_NULL_TERMINATED;


/**
 * purple_account_set_status_list:
 * @account:   The account.
 * @status_id: The ID of the status.
 * @active:    Whether @a status_id is to be activated (%TRUE) or
 *             deactivated (%FALSE).
 * @attrs:     A list of <type>const char *</type> attribute names followed by
 *             <type>const char *</type> attribute values for the status.
 *             (For example, one pair might be <literal>"message"</literal>
 *             followed by <literal>"hello, talk to me!"</literal>.)
 *
 * Activates or deactivates a status.  All changes to the statuses of
 * an account go through this function or purple_account_set_status().
 *
 * You can only deactivate an exclusive status by activating another exclusive
 * status.  So, if @a status_id is an exclusive status and @a active is @c
 * FALSE, this function does nothing.
 */
void purple_account_set_status_list(PurpleAccount *account,
	const char *status_id, gboolean active, GList *attrs);

/**
 * purple_account_set_public_alias:
 * @account:    The account
 * @alias:      The new public alias for this account or %NULL
 *              to unset the alias/nickname (or return it to
 *              a protocol-specific "default", like the username)
 * @success_cb: (scope call): A callback which will be called if the alias
 *              is successfully set on the server (or %NULL).
 * @failure_cb: (scope call): A callback which will be called if the alias
 *              is not successfully set on the server (or %NULL).
 *
 * Set a server-side (public) alias for this account.  The account
 * must already be connected.
 *
 * Currently, the public alias is not stored locally, although this
 * may change in a later version.
 */
void purple_account_set_public_alias(PurpleAccount *account,
	const char *alias, PurpleSetPublicAliasSuccessCallback success_cb,
	PurpleSetPublicAliasFailureCallback failure_cb);

/**
 * purple_account_get_public_alias:
 * @account:    The account
 * @success_cb: (scope call): A callback which will be called with the alias
 * @failure_cb: (scope call): A callback which will be called if the protocol is
 *              unable to retrieve the server-side alias.
 *
 * Fetch the server-side (public) alias for this account.  The account
 * must already be connected.
 */
void purple_account_get_public_alias(PurpleAccount *account,
	PurpleGetPublicAliasSuccessCallback success_cb,
	PurpleGetPublicAliasFailureCallback failure_cb);

/**
 * purple_account_get_silence_suppression:
 * @account: The account.
 *
 * Return whether silence suppression is used during voice call.
 *
 * Returns: %TRUE if suppression is used, or %FALSE if not.
 */
gboolean purple_account_get_silence_suppression(const PurpleAccount *account);

/**
 * purple_account_set_silence_suppression:
 * @account: The account.
 * @value:   %TRUE if suppression should be used.
 *
 * Sets whether silence suppression is used during voice call.
 */
void purple_account_set_silence_suppression(PurpleAccount *account,
											gboolean value);

/**
 * purple_account_clear_settings:
 * @account: The account.
 *
 * Clears all protocol-specific settings on an account.
 */
void purple_account_clear_settings(PurpleAccount *account);

/**
 * purple_account_remove_setting:
 * @account: The account.
 * @setting: The setting to remove.
 *
 * Removes an account-specific setting by name.
 */
void purple_account_remove_setting(PurpleAccount *account, const char *setting);

/**
 * purple_account_set_int:
 * @account: The account.
 * @name:    The name of the setting.
 * @value:   The setting's value.
 *
 * Sets a protocol-specific integer setting for an account.
 */
void purple_account_set_int(PurpleAccount *account, const char *name, int value);

/**
 * purple_account_set_string:
 * @account: The account.
 * @name:    The name of the setting.
 * @value:   The setting's value.
 *
 * Sets a protocol-specific string setting for an account.
 */
void purple_account_set_string(PurpleAccount *account, const char *name,
							 const char *value);

/**
 * purple_account_set_bool:
 * @account: The account.
 * @name:    The name of the setting.
 * @value:   The setting's value.
 *
 * Sets a protocol-specific boolean setting for an account.
 */
void purple_account_set_bool(PurpleAccount *account, const char *name,
						   gboolean value);

/**
 * purple_account_set_ui_int:
 * @account: The account.
 * @ui:      The UI name.
 * @name:    The name of the setting.
 * @value:   The setting's value.
 *
 * Sets a UI-specific integer setting for an account.
 */
void purple_account_set_ui_int(PurpleAccount *account, const char *ui,
							 const char *name, int value);

/**
 * purple_account_set_ui_string:
 * @account: The account.
 * @ui:      The UI name.
 * @name:    The name of the setting.
 * @value:   The setting's value.
 *
 * Sets a UI-specific string setting for an account.
 */
void purple_account_set_ui_string(PurpleAccount *account, const char *ui,
								const char *name, const char *value);

/**
 * purple_account_set_ui_bool:
 * @account: The account.
 * @ui:      The UI name.
 * @name:    The name of the setting.
 * @value:   The setting's value.
 *
 * Sets a UI-specific boolean setting for an account.
 */
void purple_account_set_ui_bool(PurpleAccount *account, const char *ui,
							  const char *name, gboolean value);

/**
 * purple_account_set_ui_data:
 * @account: The account.
 * @ui_data: A pointer to associate with this object.
 *
 * Set the UI data associated with this account.
 */
void purple_account_set_ui_data(PurpleAccount *account, gpointer ui_data);

/**
 * purple_account_get_ui_data:
 * @account: The account.
 *
 * Returns the UI data associated with this account.
 *
 * Returns: The UI data associated with this account.  This is a
 *         convenience field provided to the UIs--it is not
 *         used by the libpurple core.
 */
gpointer purple_account_get_ui_data(const PurpleAccount *account);

/**
 * purple_account_is_connected:
 * @account: The account.
 *
 * Returns whether or not the account is connected.
 *
 * Returns: %TRUE if connected, or %FALSE otherwise.
 */
gboolean purple_account_is_connected(const PurpleAccount *account);

/**
 * purple_account_is_connecting:
 * @account: The account.
 *
 * Returns whether or not the account is connecting.
 *
 * Returns: %TRUE if connecting, or %FALSE otherwise.
 */
gboolean purple_account_is_connecting(const PurpleAccount *account);

/**
 * purple_account_is_disconnected:
 * @account: The account.
 *
 * Returns whether or not the account is disconnected.
 *
 * Returns: %TRUE if disconnected, or %FALSE otherwise.
 */
gboolean purple_account_is_disconnected(const PurpleAccount *account);

/**
 * purple_account_get_username:
 * @account: The account.
 *
 * Returns the account's username.
 *
 * Returns: The username.
 */
const char *purple_account_get_username(const PurpleAccount *account);

/**
 * purple_account_get_password:
 * @account: The account.
 * @cb:      (scope call): The callback to give the password. (can be NULL)
 * @data:    A pointer passed to the callback.
 *
 * Reads the password for the account.
 *
 * This is an asynchronous call, that will return the password in a callback
 * once it has been read from the keyring. If the account is connected, and you
 * require the password immediately, then consider using @ref
 * purple_connection_get_password instead.
 *
 * If @cb is NULL, it tries to fetch the password from the local account
 * (and not try to fetch from keyring).
 *
 * Returns: the password, when cb is NULL and it finds the password
 *          NULL in all other cases (including when cb is not NULL)
 */
const gchar *purple_account_get_password(PurpleAccount *account,
	PurpleKeyringReadCallback cb, gpointer data);

/**
 * purple_account_get_private_alias:
 * @account: The account.
 *
 * Returns the account's private alias.
 *
 * Returns: The alias.
 */
const char *purple_account_get_private_alias(const PurpleAccount *account);

/**
 * purple_account_get_user_info:
 * @account: The account.
 *
 * Returns the account's user information.
 *
 * Returns: The user information.
 */
const char *purple_account_get_user_info(const PurpleAccount *account);

/**
 * purple_account_get_buddy_icon_path:
 * @account: The account.
 *
 * Gets the account's buddy icon path.
 *
 * Returns: The buddy icon's non-cached path.
 */
const char *purple_account_get_buddy_icon_path(const PurpleAccount *account);

/**
 * purple_account_get_protocol_id:
 * @account: The account.
 *
 * Returns the account's protocol ID.
 *
 * Returns: The protocol ID.
 */
const char *purple_account_get_protocol_id(const PurpleAccount *account);

/**
 * purple_account_get_protocol_name:
 * @account: The account.
 *
 * Returns the account's protocol name.
 *
 * Returns: The protocol name.
 */
const char *purple_account_get_protocol_name(const PurpleAccount *account);

/**
 * purple_account_get_connection:
 * @account: The account.
 *
 * Returns the account's connection.
 *
 * Returns: (transfer none): The connection.
 */
PurpleConnection *purple_account_get_connection(const PurpleAccount *account);

/**
 * purple_account_get_name_for_display:
 * @account: The account.
 *
 * Returns a name for this account appropriate for display to the user. In
 * order of preference: the account's alias; the contact or buddy alias (if
 * the account exists on its own buddy list); the connection's display name;
 * the account's username.
 *
 * Returns: The name to display.
 */
const gchar *purple_account_get_name_for_display(const PurpleAccount *account);

/**
 * purple_account_get_remember_password:
 * @account: The account.
 *
 * Returns whether or not this account should save its password.
 *
 * Returns: %TRUE if it should remember the password.
 */
gboolean purple_account_get_remember_password(const PurpleAccount *account);

/**
 * purple_account_get_check_mail:
 * @account: The account.
 *
 * Returns whether or not this account should check for mail.
 *
 * Returns: %TRUE if it should check for mail.
 */
gboolean purple_account_get_check_mail(const PurpleAccount *account);

/**
 * purple_account_get_enabled:
 * @account: The account.
 * @ui:      The UI.
 *
 * Returns whether or not this account is enabled for the
 * specified UI.
 *
 * Returns: %TRUE if it enabled on this UI.
 */
gboolean purple_account_get_enabled(const PurpleAccount *account,
				  const char *ui);

/**
 * purple_account_get_proxy_info:
 * @account: The account.
 *
 * Returns the account's proxy information.
 *
 * Returns: The proxy information.
 */
PurpleProxyInfo *purple_account_get_proxy_info(const PurpleAccount *account);

/**
 * purple_account_get_privacy_type:
 * @account:   The account.
 *
 * Returns the account's privacy type.
 *
 * Returns: The privacy type.
 */
PurpleAccountPrivacyType purple_account_get_privacy_type(const PurpleAccount *account);

/**
 * purple_account_privacy_permit_add:
 * @account:    The account.
 * @name:       The name of the user to add to the list.
 * @local_only: If TRUE, only the local list is updated, and not
 *                   the server.
 *
 * Adds a user to the account's permit list.
 *
 * Returns: TRUE if the user was added successfully, or %FALSE otherwise.
 */
gboolean purple_account_privacy_permit_add(PurpleAccount *account,
								const char *name, gboolean local_only);

/**
 * purple_account_privacy_permit_remove:
 * @account:    The account.
 * @name:       The name of the user to add to the list.
 * @local_only: If TRUE, only the local list is updated, and not
 *                   the server.
 *
 * Removes a user from the account's permit list.
 *
 * Returns: TRUE if the user was removed successfully, or %FALSE otherwise.
 */
gboolean purple_account_privacy_permit_remove(PurpleAccount *account,
									const char *name, gboolean local_only);

/**
 * purple_account_privacy_deny_add:
 * @account:    The account.
 * @name:       The name of the user to add to the list.
 * @local_only: If TRUE, only the local list is updated, and not
 *                   the server.
 *
 * Adds a user to the account's deny list.
 *
 * Returns: TRUE if the user was added successfully, or %FALSE otherwise.
 */
gboolean purple_account_privacy_deny_add(PurpleAccount *account,
									const char *name, gboolean local_only);

/**
 * purple_account_privacy_deny_remove:
 * @account:    The account.
 * @name:       The name of the user to add to the list.
 * @local_only: If TRUE, only the local list is updated, and not
 *                   the server.
 *
 * Removes a user from the account's deny list.
 *
 * Returns: TRUE if the user was removed successfully, or %FALSE otherwise.
 */
gboolean purple_account_privacy_deny_remove(PurpleAccount *account,
									const char *name, gboolean local_only);

/**
 * purple_account_privacy_allow:
 * @account:	The account.
 * @who:		The name of the user.
 *
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
 */
void purple_account_privacy_allow(PurpleAccount *account, const char *who);

/**
 * purple_account_privacy_deny:
 * @account:	The account.
 * @who:		The name of the user.
 *
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
 */
void purple_account_privacy_deny(PurpleAccount *account, const char *who);

/**
 * purple_account_privacy_get_permitted:
 * @account:	The account.
 *
 * Returns the account's permit list.
 *
 * Returns: (transfer none):     A list of the permitted users
 */
GSList *purple_account_privacy_get_permitted(PurpleAccount *account);

/**
 * purple_account_privacy_get_denied:
 * @account:	The account.
 *
 * Returns the account's deny list.
 *
 * Returns: (transfer none):     A list of the denied users
 */
GSList *purple_account_privacy_get_denied(PurpleAccount *account);

/**
 * purple_account_privacy_check:
 * @account:	The account.
 * @who:		The name of the user.
 *
 * Check the privacy-setting for a user.
 *
 * Returns: %FALSE if the specified account's privacy settings block the user
 *		or %TRUE otherwise. The meaning of "block" is protocol-dependent and
 *				generally relates to status and/or sending of messages.
 */
gboolean purple_account_privacy_check(PurpleAccount *account, const char *who);

/**
 * purple_account_get_active_status:
 * @account:   The account.
 *
 * Returns the active status for this account.  This looks through
 * the PurplePresence associated with this account and returns the
 * PurpleStatus that has its active flag set to "TRUE."  There can be
 * only one active PurpleStatus in a PurplePresence.
 *
 * Returns: The active status.
 */
PurpleStatus *purple_account_get_active_status(const PurpleAccount *account);

/**
 * purple_account_get_status:
 * @account:   The account.
 * @status_id: The status ID.
 *
 * Returns the account status with the specified ID.
 *
 * Returns: The status, or %NULL if it was never registered.
 */
PurpleStatus *purple_account_get_status(const PurpleAccount *account,
									const char *status_id);

/**
 * purple_account_get_status_type:
 * @account: The account.
 * @id:      The ID of the status type to find.
 *
 * Returns the account status type with the specified ID.
 *
 * Returns: The status type if found, or %NULL.
 */
PurpleStatusType *purple_account_get_status_type(const PurpleAccount *account,
											 const char *id);

/**
 * purple_account_get_status_type_with_primitive:
 * @account:   The account.
 * @primitive: The type of the status type to find.
 *
 * Returns the account status type with the specified primitive.
 * Note: It is possible for an account to have more than one
 * PurpleStatusType with the same primitive.  In this case, the
 * first PurpleStatusType is returned.
 *
 * Returns: The status if found, or %NULL.
 */
PurpleStatusType *purple_account_get_status_type_with_primitive(
							const PurpleAccount *account,
							PurpleStatusPrimitive primitive);

/**
 * purple_account_get_presence:
 * @account: The account.
 *
 * Returns the account's presence.
 *
 * Returns: The account's presence.
 */
PurplePresence *purple_account_get_presence(const PurpleAccount *account);

/**
 * purple_account_is_status_active:
 * @account:   The account.
 * @status_id: The status ID.
 *
 * Returns whether or not an account status is active.
 *
 * Returns: TRUE if active, or FALSE if not.
 */
gboolean purple_account_is_status_active(const PurpleAccount *account,
									   const char *status_id);

/**
 * purple_account_get_status_types:
 * @account: The account.
 *
 * Returns the account's status types.
 *
 * Returns: (transfer none): The account's status types.
 */
GList *purple_account_get_status_types(const PurpleAccount *account);

/**
 * purple_account_get_int:
 * @account:       The account.
 * @name:          The name of the setting.
 * @default_value: The default value.
 *
 * Returns a protocol-specific integer setting for an account.
 *
 * Returns: The value.
 */
int purple_account_get_int(const PurpleAccount *account, const char *name,
						 int default_value);

/**
 * purple_account_get_string:
 * @account:       The account.
 * @name:          The name of the setting.
 * @default_value: The default value.
 *
 * Returns a protocol-specific string setting for an account.
 *
 * Returns: The value.
 */
const char *purple_account_get_string(const PurpleAccount *account,
									const char *name,
									const char *default_value);

/**
 * purple_account_get_bool:
 * @account:       The account.
 * @name:          The name of the setting.
 * @default_value: The default value.
 *
 * Returns a protocol-specific boolean setting for an account.
 *
 * Returns: The value.
 */
gboolean purple_account_get_bool(const PurpleAccount *account, const char *name,
							   gboolean default_value);

/**
 * purple_account_get_ui_int:
 * @account:       The account.
 * @ui:            The UI name.
 * @name:          The name of the setting.
 * @default_value: The default value.
 *
 * Returns a UI-specific integer setting for an account.
 *
 * Returns: The value.
 */
int purple_account_get_ui_int(const PurpleAccount *account, const char *ui,
							const char *name, int default_value);

/**
 * purple_account_get_ui_string:
 * @account:       The account.
 * @ui:            The UI name.
 * @name:          The name of the setting.
 * @default_value: The default value.
 *
 * Returns a UI-specific string setting for an account.
 *
 * Returns: The value.
 */
const char *purple_account_get_ui_string(const PurpleAccount *account,
									   const char *ui, const char *name,
									   const char *default_value);

/**
 * purple_account_get_ui_bool:
 * @account:       The account.
 * @ui:            The UI name.
 * @name:          The name of the setting.
 * @default_value: The default value.
 *
 * Returns a UI-specific boolean setting for an account.
 *
 * Returns: The value.
 */
gboolean purple_account_get_ui_bool(const PurpleAccount *account, const char *ui,
								  const char *name, gboolean default_value);


/**
 * purple_account_get_log:
 * @account: The account.
 * @create:  Should it be created if it doesn't exist?
 *
 * Returns the system log for an account.
 *
 * Note: Callers should almost always pass %FALSE for @a create.
 *       Passing %TRUE could result in an existing log being reopened,
 *       if the log has already been closed, which not all loggers deal
 *       with appropriately.
 *
 * Returns: The log.
 */
PurpleLog *purple_account_get_log(PurpleAccount *account, gboolean create);

/**
 * purple_account_destroy_log:
 * @account: The account.
 *
 * Frees the system log of an account
 */
void purple_account_destroy_log(PurpleAccount *account);

/**
 * purple_account_add_buddy:
 * @account: The account.
 * @buddy: The buddy to add.
 * @message: The invite message.  This may be ignored by a protocol.
 *
 * Adds a buddy to the server-side buddy list for the specified account.
 */
void purple_account_add_buddy(PurpleAccount *account, PurpleBuddy *buddy, const char *message);

/**
 * purple_account_add_buddies:
 * @account: The account.
 * @buddies: The list of PurpleBlistNodes representing the buddies to add.
 * @message: The invite message.  This may be ignored by a protocol.
 *
 * Adds a list of buddies to the server-side buddy list.
 */
void purple_account_add_buddies(PurpleAccount *account, GList *buddies, const char *message);

/**
 * purple_account_remove_buddy:
 * @account: The account.
 * @buddy: The buddy to remove.
 * @group: The group to remove the buddy from.
 *
 * Removes a buddy from the server-side buddy list.
 */
void purple_account_remove_buddy(PurpleAccount *account, PurpleBuddy *buddy,
								PurpleGroup *group);

/**
 * purple_account_remove_buddies:
 * @account: The account.
 * @buddies: The list of buddies to remove.
 * @groups: The list of groups to remove buddies from.  Each node of this
 *               list should match the corresponding node of buddies.
 *
 * Removes a list of buddies from the server-side buddy list.
 *
 * Note: The lists buddies and groups are parallel lists.  Be sure that node n of
 *       groups matches node n of buddies.
 */
void purple_account_remove_buddies(PurpleAccount *account, GList *buddies,
									GList *groups);

/**
 * purple_account_remove_group:
 * @account: The account.
 * @group: The group to remove.
 *
 * Removes a group from the server-side buddy list.
 */
void purple_account_remove_group(PurpleAccount *account, PurpleGroup *group);

/**
 * purple_account_change_password:
 * @account: The account.
 * @orig_pw: The old password.
 * @new_pw: The new password.
 *
 * Changes the password on the specified account.
 */
void purple_account_change_password(PurpleAccount *account, const char *orig_pw,
									const char *new_pw);

/**
 * purple_account_supports_offline_message:
 * @account: The account
 * @buddy:   The buddy
 *
 * Whether the account supports sending offline messages to buddy.
 */
gboolean purple_account_supports_offline_message(PurpleAccount *account, PurpleBuddy *buddy);

/**
 * purple_account_get_current_error:
 * @account: The account whose error should be retrieved.
 *
 * Get the error that caused the account to be disconnected, or %NULL if the
 * account is happily connected or disconnected without an error.
 *
 * Returns: (transfer none): The type of error and a human-readable description
 *          of the current error, or %NULL if there is no current error.  This
 *          pointer is guaranteed to remain valid until the @ref
 *          account-error-changed signal is emitted for @a account.
 */
const PurpleConnectionErrorInfo *purple_account_get_current_error(PurpleAccount *account);

/**
 * purple_account_clear_current_error:
 * @account: The account whose error state should be cleared.
 *
 * Clear an account's current error state, resetting it to %NULL.
 */
void purple_account_clear_current_error(PurpleAccount *account);

G_END_DECLS

#endif /* _PURPLE_ACCOUNT_H_ */
