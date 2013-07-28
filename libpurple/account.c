/**
 * @file account.c Account API
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
#include "internal.h"
#include "accounts.h"
#include "core.h"
#include "dbus-maybe.h"
#include "debug.h"
#include "network.h"
#include "notify.h"
#include "pounce.h"
#include "prefs.h"
#include "request.h"
#include "server.h"
#include "signals.h"
#include "util.h"

#define PURPLE_ACCOUNT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_ACCOUNT, PurpleAccountPrivate))

typedef struct
{
	char *username;             /**< The username.                          */
	char *alias;                /**< How you appear to yourself.            */
	char *password;             /**< The account password.                  */
	char *user_info;            /**< User information.                      */

	char *buddy_icon_path;      /**< The buddy icon's non-cached path.      */

	gboolean remember_pass;     /**< Remember the password.                 */

	/*
	 * TODO: After a GObject representing a protocol is ready, use it
	 * here instead of the protocol ID.
	 */
	char *protocol_id;          /**< The ID of the protocol.                */

	PurpleConnection *gc;         /**< The connection handle.               */
	gboolean disconnecting;     /**< The account is currently disconnecting */

	GHashTable *settings;       /**< Protocol-specific settings.            */
	GHashTable *ui_settings;    /**< UI-specific settings.                  */

	PurpleProxyInfo *proxy_info;  /**< Proxy information.  This will be set */
								/*   to NULL when the account inherits      */
								/*   proxy settings from global prefs.      */

	/*
	 * TODO: Supplementing the next two linked lists with hash tables
	 * should help performance a lot when these lists are long.  This
	 * matters quite a bit for protocols like MSN, where all your
	 * buddies are added to your permit list.  Currently we have to
	 * iterate through the entire list if we want to check if someone
	 * is permitted or denied.  We should do this for 3.0.0.
	 * Or maybe use a GTree.
	 */
	GSList *permit;             /**< Permit list.                           */
	GSList *deny;               /**< Deny list.                             */
	PurpleAccountPrivacyType privacy_type;  /**< The permit/deny setting.   */

	GList *status_types;        /**< Status types.                          */

	PurplePresence *presence;     /**< Presence.                            */
	PurpleLog *system_log;        /**< The system log                       */

	PurpleAccountRegistrationCb registration_cb;
	void *registration_cb_user_data;

	PurpleConnectionErrorInfo *current_error;	/**< Errors */
} PurpleAccountPrivate;

typedef struct
{
	char *ui;
	GValue value;

} PurpleAccountSetting;

typedef struct
{
	PurpleAccountRequestType type;
	PurpleAccount *account;
	void *ui_handle;
	char *user;
	gpointer userdata;
	PurpleAccountRequestAuthorizationCb auth_cb;
	PurpleAccountRequestAuthorizationCb deny_cb;
	guint ref;
} PurpleAccountRequestInfo;

typedef struct
{
	PurpleCallback cb;
	gpointer data;
} PurpleCallbackBundle;

/* GObject Property enums */
enum
{
	PROP_0,
	PROP_USERNAME,
	PROP_PRIVATE_ALIAS,
	PROP_ENABLED,
	PROP_CONNECTION,
	PROP_PROTOCOL_ID,
	PROP_USER_INFO,
	PROP_BUDDY_ICON_PATH,
	PROP_REMEMBER_PASSWORD,
	PROP_CHECK_MAIL,
	PROP_LAST
};

static GObjectClass  *parent_class = NULL;
static GList         *handles = NULL;

void _purple_account_set_current_error(PurpleAccount *account,
		PurpleConnectionErrorInfo *new_err);

/***************
 * Account API *
 ***************/
void
purple_account_set_register_callback(PurpleAccount *account, PurpleAccountRegistrationCb cb, void *user_data)
{
	PurpleAccountPrivate *priv;

	g_return_if_fail(account != NULL);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	priv->registration_cb = cb;
	priv->registration_cb_user_data = user_data;
}

static void
purple_account_register_got_password_cb(PurpleAccount *account,
	const gchar *password, GError *error, gpointer data)
{
	g_return_if_fail(account != NULL);

	_purple_connection_new(account, TRUE, password);
}

void
purple_account_register(PurpleAccount *account)
{
	g_return_if_fail(account != NULL);

	purple_debug_info("account", "Registering account %s\n",
					purple_account_get_username(account));

	purple_keyring_get_password(account,
		purple_account_register_got_password_cb, NULL);
}

static void
purple_account_unregister_got_password_cb(PurpleAccount *account,
	const gchar *password, GError *error, gpointer data)
{
	PurpleCallbackBundle *cbb = data;
	PurpleAccountUnregistrationCb cb;

	cb = (PurpleAccountUnregistrationCb)cbb->cb;
	_purple_connection_new_unregister(account, password, cb, cbb->data);

	g_free(cbb);
}

void
purple_account_register_completed(PurpleAccount *account, gboolean succeeded)
{
	PurpleAccountPrivate *priv;

	g_return_if_fail(account != NULL);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	if (priv->registration_cb)
		(priv->registration_cb)(account, succeeded, priv->registration_cb_user_data);
}

void
purple_account_unregister(PurpleAccount *account, PurpleAccountUnregistrationCb cb, void *user_data)
{
	PurpleCallbackBundle *cbb;

	g_return_if_fail(account != NULL);

	purple_debug_info("account", "Unregistering account %s\n",
					  purple_account_get_username(account));

	cbb = g_new0(PurpleCallbackBundle, 1);
	cbb->cb = PURPLE_CALLBACK(cb);
	cbb->data = user_data;

	purple_keyring_get_password(account,
		purple_account_unregister_got_password_cb, cbb);
}

static void
request_password_ok_cb(PurpleAccount *account, PurpleRequestFields *fields)
{
	const char *entry;
	gboolean remember;

	entry = purple_request_fields_get_string(fields, "password");
	remember = purple_request_fields_get_bool(fields, "remember");

	if (!entry || !*entry)
	{
		purple_notify_error(account, NULL, _("Password is required to sign on."), NULL);
		return;
	}

	purple_account_set_remember_password(account, remember);

	purple_account_set_password(account, entry, NULL, NULL);
	_purple_connection_new(account, FALSE, entry);
}

static void
request_password_cancel_cb(PurpleAccount *account, PurpleRequestFields *fields)
{
	/* Disable the account as the user has cancelled connecting */
	purple_account_set_enabled(account, purple_core_get_ui(), FALSE);
}


void
purple_account_request_password(PurpleAccount *account, GCallback ok_cb,
				GCallback cancel_cb, void *user_data)
{
	gchar *primary;
	const gchar *username;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;
	PurpleRequestFields *fields;

	/* Close any previous password request windows */
	purple_request_close_with_handle(account);

	username = purple_account_get_username(account);
	primary = g_strdup_printf(_("Enter password for %s (%s)"), username,
								  purple_account_get_protocol_name(account));

	fields = purple_request_fields_new();
	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	field = purple_request_field_string_new("password", _("Enter Password"), NULL, FALSE);
	purple_request_field_string_set_masked(field, TRUE);
	purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_bool_new("remember", _("Save password"), FALSE);
	purple_request_field_group_add_field(group, field);

	purple_request_fields(account,
                        NULL,
                        primary,
                        NULL,
                        fields,
                        _("OK"), ok_cb,
                        _("Cancel"), cancel_cb,
						account, NULL, NULL,
                        user_data);
	g_free(primary);
}

static void
purple_account_connect_got_password_cb(PurpleAccount *account,
	const gchar *password, GError *error, gpointer data)
{
	PurplePluginProtocolInfo *prpl_info = data;

	if ((password == NULL || *password == '\0') &&
		!(prpl_info->options & OPT_PROTO_NO_PASSWORD) &&
		!(prpl_info->options & OPT_PROTO_PASSWORD_OPTIONAL))
		purple_account_request_password(account,
			G_CALLBACK(request_password_ok_cb),
			G_CALLBACK(request_password_cancel_cb), account);
	else
		_purple_connection_new(account, FALSE, password);
}

void
purple_account_connect(PurpleAccount *account)
{
	const char *username;
	PurplePluginProtocolInfo *prpl_info;
	PurpleAccountPrivate *priv;

	g_return_if_fail(account != NULL);

	username = purple_account_get_username(account);

	if (!purple_account_get_enabled(account, purple_core_get_ui())) {
		purple_debug_info("account",
				  "Account %s not enabled, not connecting.\n",
				  username);
		return;
	}

	prpl_info = purple_find_protocol_info(purple_account_get_protocol_id(account));
	if (prpl_info == NULL) {
		gchar *message;

		message = g_strdup_printf(_("Missing protocol plugin for %s"), username);
		purple_notify_error(account, _("Connection Error"), message, NULL);
		g_free(message);
		return;
	}

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	purple_debug_info("account", "Connecting to account %s.\n", username);

	if (priv->password != NULL) {
		purple_account_connect_got_password_cb(account,
			priv->password, NULL, prpl_info);
	} else {
		purple_keyring_get_password(account,
			purple_account_connect_got_password_cb, prpl_info);
	}
}

void
purple_account_disconnect(PurpleAccount *account)
{
	PurpleConnection *gc;
	PurpleAccountPrivate *priv;
	const char *username;

	g_return_if_fail(account != NULL);
	g_return_if_fail(!purple_account_is_disconnected(account));

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	username = purple_account_get_username(account);
	purple_debug_info("account", "Disconnecting account %s (%p)\n",
	                  username ? username : "(null)", account);

	priv->disconnecting = TRUE;

	gc = purple_account_get_connection(account);
	g_object_unref(gc);
	purple_account_set_connection(account, NULL);

	priv->disconnecting = FALSE;
}

gboolean
purple_account_is_disconnecting(const PurpleAccount *account)
{
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(account != NULL, TRUE);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);
	return priv->disconnecting;
}

void
purple_account_notify_added(PurpleAccount *account, const char *remote_user,
                          const char *id, const char *alias,
                          const char *message)
{
	PurpleAccountUiOps *ui_ops;

	g_return_if_fail(account     != NULL);
	g_return_if_fail(remote_user != NULL);

	ui_ops = purple_accounts_get_ui_ops();

	if (ui_ops != NULL && ui_ops->notify_added != NULL)
		ui_ops->notify_added(account, remote_user, id, alias, message);
}

void
purple_account_request_add(PurpleAccount *account, const char *remote_user,
                         const char *id, const char *alias,
                         const char *message)
{
	PurpleAccountUiOps *ui_ops;

	g_return_if_fail(account     != NULL);
	g_return_if_fail(remote_user != NULL);

	ui_ops = purple_accounts_get_ui_ops();

	if (ui_ops != NULL && ui_ops->request_add != NULL)
		ui_ops->request_add(account, remote_user, id, alias, message);
}

static PurpleAccountRequestInfo *
purple_account_request_info_unref(PurpleAccountRequestInfo *info)
{
	if (--info->ref)
		return info;

	/* TODO: This will leak info->user_data, but there is no callback to just clean that up */
	g_free(info->user);
	g_free(info);
	return NULL;
}

static void
purple_account_request_close_info(PurpleAccountRequestInfo *info)
{
	PurpleAccountUiOps *ops;

	ops = purple_accounts_get_ui_ops();

	if (ops != NULL && ops->close_account_request != NULL)
		ops->close_account_request(info->ui_handle);

	purple_account_request_info_unref(info);
}

void
purple_account_request_close_with_account(PurpleAccount *account)
{
	GList *l, *l_next;

	g_return_if_fail(account != NULL);

	for (l = handles; l != NULL; l = l_next) {
		PurpleAccountRequestInfo *info = l->data;

		l_next = l->next;

		if (info->account == account) {
			handles = g_list_remove(handles, info);
			purple_account_request_close_info(info);
		}
	}
}

void
purple_account_request_close(void *ui_handle)
{
	GList *l, *l_next;

	g_return_if_fail(ui_handle != NULL);

	for (l = handles; l != NULL; l = l_next) {
		PurpleAccountRequestInfo *info = l->data;

		l_next = l->next;

		if (info->ui_handle == ui_handle) {
			handles = g_list_remove(handles, info);
			purple_account_request_close_info(info);
		}
	}
}

static void
request_auth_cb(const char *message, void *data)
{
	PurpleAccountRequestInfo *info = data;

	handles = g_list_remove(handles, info);

	if (info->auth_cb != NULL)
		info->auth_cb(message, info->userdata);

	purple_signal_emit(purple_accounts_get_handle(),
			"account-authorization-granted", info->account, info->user, message);

	purple_account_request_info_unref(info);
}

static void
request_deny_cb(const char *message, void *data)
{
	PurpleAccountRequestInfo *info = data;

	handles = g_list_remove(handles, info);

	if (info->deny_cb != NULL)
		info->deny_cb(message, info->userdata);

	purple_signal_emit(purple_accounts_get_handle(),
			"account-authorization-denied", info->account, info->user, message);

	purple_account_request_info_unref(info);
}

void *
purple_account_request_authorization(PurpleAccount *account, const char *remote_user,
				     const char *id, const char *alias, const char *message, gboolean on_list,
				     PurpleAccountRequestAuthorizationCb auth_cb, PurpleAccountRequestAuthorizationCb deny_cb, void *user_data)
{
	PurpleAccountUiOps *ui_ops;
	PurpleAccountRequestInfo *info;
	int plugin_return;
	char *response = NULL;

	g_return_val_if_fail(account     != NULL, NULL);
	g_return_val_if_fail(remote_user != NULL, NULL);

	ui_ops = purple_accounts_get_ui_ops();

	plugin_return = GPOINTER_TO_INT(
			purple_signal_emit_return_1(
				purple_accounts_get_handle(),
				"account-authorization-requested",
				account, remote_user, message, &response
			));

	switch (plugin_return)
	{
		case PURPLE_ACCOUNT_RESPONSE_IGNORE:
			g_free(response);
			return NULL;
		case PURPLE_ACCOUNT_RESPONSE_ACCEPT:
			if (auth_cb != NULL)
				auth_cb(response, user_data);
			g_free(response);
			return NULL;
		case PURPLE_ACCOUNT_RESPONSE_DENY:
			if (deny_cb != NULL)
				deny_cb(response, user_data);
			g_free(response);
			return NULL;
	}

	g_free(response);

	if (ui_ops != NULL && ui_ops->request_authorize != NULL) {
		info            = g_new0(PurpleAccountRequestInfo, 1);
		info->type      = PURPLE_ACCOUNT_REQUEST_AUTHORIZATION;
		info->account   = account;
		info->auth_cb   = auth_cb;
		info->deny_cb   = deny_cb;
		info->userdata  = user_data;
		info->user      = g_strdup(remote_user);
		info->ref       = 2;  /* We hold an extra ref to make sure info remains valid
		                         if any of the callbacks are called synchronously. We
		                         unref it after the function call */

		info->ui_handle = ui_ops->request_authorize(account, remote_user, id, alias, message,
							    on_list, request_auth_cb, request_deny_cb, info);

		info = purple_account_request_info_unref(info);
		if (info) {
			handles = g_list_append(handles, info);
			return info->ui_handle;
		}
	}

	return NULL;
}

static void
change_password_cb(PurpleAccount *account, PurpleRequestFields *fields)
{
	const char *orig_pass, *new_pass_1, *new_pass_2;

	orig_pass  = purple_request_fields_get_string(fields, "password");
	new_pass_1 = purple_request_fields_get_string(fields, "new_password_1");
	new_pass_2 = purple_request_fields_get_string(fields, "new_password_2");

	if (g_utf8_collate(new_pass_1, new_pass_2))
	{
		purple_notify_error(account, NULL,
						  _("New passwords do not match."), NULL);

		return;
	}

	if ((purple_request_fields_is_field_required(fields, "password") &&
			(orig_pass == NULL || *orig_pass == '\0')) ||
		(purple_request_fields_is_field_required(fields, "new_password_1") &&
			(new_pass_1 == NULL || *new_pass_1 == '\0')) ||
		(purple_request_fields_is_field_required(fields, "new_password_2") &&
			(new_pass_2 == NULL || *new_pass_2 == '\0')))
	{
		purple_notify_error(account, NULL,
						  _("Fill out all fields completely."), NULL);
		return;
	}

	purple_account_change_password(account, orig_pass, new_pass_1);
}

void
purple_account_request_change_password(PurpleAccount *account)
{
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;
	PurpleConnection *gc;
	PurplePluginProtocolInfo *prpl_info = NULL;
	char primary[256];

	g_return_if_fail(account != NULL);
	g_return_if_fail(purple_account_is_connected(account));

	gc = purple_account_get_connection(account);
	if (gc != NULL)
		prpl_info = purple_connection_get_protocol_info(gc);

	fields = purple_request_fields_new();

	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	field = purple_request_field_string_new("password", _("Original password"),
										  NULL, FALSE);
	purple_request_field_string_set_masked(field, TRUE);
	if (!prpl_info || !(prpl_info->options & OPT_PROTO_PASSWORD_OPTIONAL))
		purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_string_new("new_password_1",
										  _("New password"),
										  NULL, FALSE);
	purple_request_field_string_set_masked(field, TRUE);
	if (!prpl_info || !(prpl_info->options & OPT_PROTO_PASSWORD_OPTIONAL))
		purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_string_new("new_password_2",
										  _("New password (again)"),
										  NULL, FALSE);
	purple_request_field_string_set_masked(field, TRUE);
	if (!prpl_info || !(prpl_info->options & OPT_PROTO_PASSWORD_OPTIONAL))
		purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	g_snprintf(primary, sizeof(primary), _("Change password for %s"),
			   purple_account_get_username(account));

	/* I'm sticking this somewhere in the code: bologna */

	purple_request_fields(purple_account_get_connection(account),
						NULL,
						primary,
						_("Please enter your current password and your "
						  "new password."),
						fields,
						_("OK"), G_CALLBACK(change_password_cb),
						_("Cancel"), NULL,
						account, NULL, NULL,
						account);
}

static void
set_user_info_cb(PurpleAccount *account, const char *user_info)
{
	PurpleConnection *gc;

	purple_account_set_user_info(account, user_info);
	gc = purple_account_get_connection(account);
	serv_set_info(gc, user_info);
}

void
purple_account_request_change_user_info(PurpleAccount *account)
{
	PurpleConnection *gc;
	char primary[256];

	g_return_if_fail(account != NULL);
	g_return_if_fail(purple_account_is_connected(account));

	gc = purple_account_get_connection(account);

	g_snprintf(primary, sizeof(primary),
			   _("Change user information for %s"),
			   purple_account_get_username(account));

	purple_request_input(gc, _("Set User Info"), primary, NULL,
					   purple_account_get_user_info(account),
					   TRUE, FALSE, ((gc != NULL) &&
					   (purple_connection_get_flags(gc) & PURPLE_CONNECTION_FLAG_HTML) ? "html" : NULL),
					   _("Save"), G_CALLBACK(set_user_info_cb),
					   _("Cancel"), NULL,
					   account, NULL, NULL,
					   account);
}

void
purple_account_set_username(PurpleAccount *account, const char *username)
{
	PurpleBlistUiOps *blist_ops;
	PurpleAccountPrivate *priv;

	g_return_if_fail(account != NULL);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	g_free(priv->username);
	priv->username = g_strdup(username);

	purple_accounts_schedule_save();

	/* if the name changes, we should re-write the buddy list
	 * to disk with the new name */
	blist_ops = purple_blist_get_ui_ops();
	if (blist_ops != NULL && blist_ops->save_account != NULL)
		blist_ops->save_account(account);
}

void 
purple_account_set_password(PurpleAccount *account, const gchar *password,
	PurpleKeyringSaveCallback cb, gpointer data)
{
	PurpleAccountPrivate *priv;

	g_return_if_fail(account != NULL);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	purple_str_wipe(priv->password);
	priv->password = g_strdup(password);

	purple_accounts_schedule_save();

	if (!purple_account_get_remember_password(account)) {
		purple_debug_info("account",
			"Password for %s set, not sent to keyring.\n",
			purple_account_get_username(account));

		if (cb != NULL)
			cb(account, NULL, data);
	} else {
		purple_keyring_set_password(account, password, cb, data);
	}
}

void
purple_account_set_private_alias(PurpleAccount *account, const char *alias)
{
	PurpleAccountPrivate *priv;

	g_return_if_fail(account != NULL);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	/*
	 * Do nothing if alias and priv->alias are both NULL.  Or if
	 * they're the exact same string.
	 */
	if (alias == priv->alias)
		return;

	if ((!alias && priv->alias) || (alias && !priv->alias) ||
			g_utf8_collate(priv->alias, alias))
	{
		char *old = priv->alias;

		priv->alias = g_strdup(alias);
		purple_signal_emit(purple_accounts_get_handle(), "account-alias-changed",
						 account, old);
		g_free(old);

		purple_accounts_schedule_save();
	}
}

void
purple_account_set_user_info(PurpleAccount *account, const char *user_info)
{
	PurpleAccountPrivate *priv;

	g_return_if_fail(account != NULL);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	g_free(priv->user_info);
	priv->user_info = g_strdup(user_info);

	purple_accounts_schedule_save();
}

void purple_account_set_buddy_icon_path(PurpleAccount *account, const char *path)
{
	PurpleAccountPrivate *priv;

	g_return_if_fail(account != NULL);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	g_free(priv->buddy_icon_path);
	priv->buddy_icon_path = g_strdup(path);

	purple_accounts_schedule_save();
}

void
purple_account_set_protocol_id(PurpleAccount *account, const char *protocol_id)
{
	PurpleAccountPrivate *priv;

	g_return_if_fail(account     != NULL);
	g_return_if_fail(protocol_id != NULL);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	g_free(priv->protocol_id);
	priv->protocol_id = g_strdup(protocol_id);

	purple_accounts_schedule_save();
}

void
purple_account_set_connection(PurpleAccount *account, PurpleConnection *gc)
{
	PurpleAccountPrivate *priv;

	g_return_if_fail(account != NULL);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);
	priv->gc = gc;
}

void
purple_account_set_remember_password(PurpleAccount *account, gboolean value)
{
	PurpleAccountPrivate *priv;

	g_return_if_fail(account != NULL);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);
	priv->remember_pass = value;

	purple_accounts_schedule_save();
}

void
purple_account_set_check_mail(PurpleAccount *account, gboolean value)
{
	g_return_if_fail(account != NULL);

	purple_account_set_bool(account, "check-mail", value);
}

void
purple_account_set_enabled(PurpleAccount *account, const char *ui,
			 gboolean value)
{
	PurpleConnection *gc;
	PurpleAccountPrivate *priv;
	gboolean was_enabled = FALSE;

	g_return_if_fail(account != NULL);
	g_return_if_fail(ui      != NULL);

	was_enabled = purple_account_get_enabled(account, ui);

	purple_account_set_ui_bool(account, ui, "auto-login", value);
	gc = purple_account_get_connection(account);

	if(was_enabled && !value)
		purple_signal_emit(purple_accounts_get_handle(), "account-disabled", account);
	else if(!was_enabled && value)
		purple_signal_emit(purple_accounts_get_handle(), "account-enabled", account);

	if ((gc != NULL) && (_purple_connection_wants_to_die(gc)))
		return;

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	if (value && purple_presence_is_online(priv->presence))
		purple_account_connect(account);
	else if (!value && !purple_account_is_disconnected(account))
		purple_account_disconnect(account);
}

void
purple_account_set_proxy_info(PurpleAccount *account, PurpleProxyInfo *info)
{
	PurpleAccountPrivate *priv;

	g_return_if_fail(account != NULL);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	if (priv->proxy_info != NULL)
		purple_proxy_info_destroy(priv->proxy_info);

	priv->proxy_info = info;

	purple_accounts_schedule_save();
}

void
purple_account_set_privacy_type(PurpleAccount *account, PurpleAccountPrivacyType privacy_type)
{
	PurpleAccountPrivate *priv;

	g_return_if_fail(account != NULL);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);
	priv->privacy_type = privacy_type;
}

void
purple_account_set_status_types(PurpleAccount *account, GList *status_types)
{
	PurpleAccountPrivate *priv;

	g_return_if_fail(account != NULL);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	/* Out with the old... */
	if (priv->status_types != NULL)
	{
		g_list_foreach(priv->status_types, (GFunc)purple_status_type_destroy, NULL);
		g_list_free(priv->status_types);
	}

	/* In with the new... */
	priv->status_types = status_types;
}

void
purple_account_set_status(PurpleAccount *account, const char *status_id,
						gboolean active, ...)
{
	GList *attrs = NULL;
	const gchar *id;
	gpointer data;
	va_list args;

	va_start(args, active);
	while ((id = va_arg(args, const char *)) != NULL)
	{
		attrs = g_list_append(attrs, (char *)id);
		data = va_arg(args, void *);
		attrs = g_list_append(attrs, data);
	}
	purple_account_set_status_list(account, status_id, active, attrs);
	g_list_free(attrs);
	va_end(args);
}

void
purple_account_set_status_list(PurpleAccount *account, const char *status_id,
							 gboolean active, GList *attrs)
{
	PurpleStatus *status;

	g_return_if_fail(account   != NULL);
	g_return_if_fail(status_id != NULL);

	status = purple_account_get_status(account, status_id);
	if (status == NULL)
	{
		purple_debug_error("account",
				   "Invalid status ID '%s' for account %s (%s)\n",
				   status_id, purple_account_get_username(account),
				   purple_account_get_protocol_id(account));
		return;
	}

	if (active || purple_status_is_independent(status))
		purple_status_set_active_with_attrs_list(status, active, attrs);

	/*
	 * Our current statuses are saved to accounts.xml (so that when we
	 * reconnect, we go back to the previous status).
	 */
	purple_accounts_schedule_save();
}

struct public_alias_closure
{
	PurpleAccount *account;
	gpointer failure_cb;
};

static gboolean
set_public_alias_unsupported(gpointer data)
{
	struct public_alias_closure *closure = data;
	PurpleSetPublicAliasFailureCallback failure_cb = closure->failure_cb;

	failure_cb(closure->account,
	           _("This protocol does not support setting a public alias."));
	g_free(closure);

	return FALSE;
}

void
purple_account_set_public_alias(PurpleAccount *account,
		const char *alias, PurpleSetPublicAliasSuccessCallback success_cb,
		PurpleSetPublicAliasFailureCallback failure_cb)
{
	PurpleConnection *gc;
	PurplePluginProtocolInfo *prpl_info = NULL;

	g_return_if_fail(account != NULL);
	g_return_if_fail(purple_account_is_connected(account));

	gc = purple_account_get_connection(account);
	prpl_info = purple_connection_get_protocol_info(gc);

	if (PURPLE_PROTOCOL_PLUGIN_HAS_FUNC(prpl_info, set_public_alias))
		prpl_info->set_public_alias(gc, alias, success_cb, failure_cb);
	else if (failure_cb) {
		struct public_alias_closure *closure =
				g_new0(struct public_alias_closure, 1);
		closure->account = account;
		closure->failure_cb = failure_cb;
		purple_timeout_add(0, set_public_alias_unsupported, closure);
	}
}

static gboolean
get_public_alias_unsupported(gpointer data)
{
	struct public_alias_closure *closure = data;
	PurpleGetPublicAliasFailureCallback failure_cb = closure->failure_cb;

	failure_cb(closure->account,
	           _("This protocol does not support fetching the public alias."));
	g_free(closure);

	return FALSE;
}

void
purple_account_get_public_alias(PurpleAccount *account,
		PurpleGetPublicAliasSuccessCallback success_cb,
		PurpleGetPublicAliasFailureCallback failure_cb)
{
	PurpleConnection *gc;
	PurplePluginProtocolInfo *prpl_info = NULL;

	g_return_if_fail(account != NULL);
	g_return_if_fail(purple_account_is_connected(account));

	gc = purple_account_get_connection(account);
	prpl_info = purple_connection_get_protocol_info(gc);

	if (PURPLE_PROTOCOL_PLUGIN_HAS_FUNC(prpl_info, get_public_alias))
		prpl_info->get_public_alias(gc, success_cb, failure_cb);
	else if (failure_cb) {
		struct public_alias_closure *closure =
				g_new0(struct public_alias_closure, 1);
		closure->account = account;
		closure->failure_cb = failure_cb;
		purple_timeout_add(0, get_public_alias_unsupported, closure);
	}
}

gboolean
purple_account_get_silence_suppression(const PurpleAccount *account)
{
	return purple_account_get_bool(account, "silence-suppression", FALSE);
}

void
purple_account_set_silence_suppression(PurpleAccount *account, gboolean value)
{
	g_return_if_fail(account != NULL);

	purple_account_set_bool(account, "silence-suppression", value);
}

static void
delete_setting(void *data)
{
	PurpleAccountSetting *setting = (PurpleAccountSetting *)data;

	g_free(setting->ui);
	g_value_unset(&setting->value);

	g_free(setting);
}

void
purple_account_clear_settings(PurpleAccount *account)
{
	PurpleAccountPrivate *priv;

	g_return_if_fail(account != NULL);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);
	g_hash_table_destroy(priv->settings);

	priv->settings = g_hash_table_new_full(g_str_hash, g_str_equal,
											  g_free, delete_setting);
}

void
purple_account_remove_setting(PurpleAccount *account, const char *setting)
{
	PurpleAccountPrivate *priv;

	g_return_if_fail(account != NULL);
	g_return_if_fail(setting != NULL);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	g_hash_table_remove(priv->settings, setting);
}

void
purple_account_set_int(PurpleAccount *account, const char *name, int value)
{
	PurpleAccountSetting *setting;
	PurpleAccountPrivate *priv;

	g_return_if_fail(account != NULL);
	g_return_if_fail(name    != NULL);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	setting = g_new0(PurpleAccountSetting, 1);

	g_value_init(&setting->value, G_TYPE_INT);
	g_value_set_int(&setting->value, value);

	g_hash_table_insert(priv->settings, g_strdup(name), setting);

	purple_accounts_schedule_save();
}

void
purple_account_set_string(PurpleAccount *account, const char *name,
						const char *value)
{
	PurpleAccountSetting *setting;
	PurpleAccountPrivate *priv;

	g_return_if_fail(account != NULL);
	g_return_if_fail(name    != NULL);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	setting = g_new0(PurpleAccountSetting, 1);

	g_value_init(&setting->value, G_TYPE_STRING);
	g_value_set_string(&setting->value, value);

	g_hash_table_insert(priv->settings, g_strdup(name), setting);

	purple_accounts_schedule_save();
}

void
purple_account_set_bool(PurpleAccount *account, const char *name, gboolean value)
{
	PurpleAccountSetting *setting;
	PurpleAccountPrivate *priv;

	g_return_if_fail(account != NULL);
	g_return_if_fail(name    != NULL);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	setting = g_new0(PurpleAccountSetting, 1);

	g_value_init(&setting->value, G_TYPE_BOOLEAN);
	g_value_set_boolean(&setting->value, value);

	g_hash_table_insert(priv->settings, g_strdup(name), setting);

	purple_accounts_schedule_save();
}

static GHashTable *
get_ui_settings_table(PurpleAccount *account, const char *ui)
{
	GHashTable *table;
	PurpleAccountPrivate *priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	table = g_hash_table_lookup(priv->ui_settings, ui);

	if (table == NULL) {
		table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
									  delete_setting);
		g_hash_table_insert(priv->ui_settings, g_strdup(ui), table);
	}

	return table;
}

void
purple_account_set_ui_int(PurpleAccount *account, const char *ui,
						const char *name, int value)
{
	PurpleAccountSetting *setting;
	GHashTable *table;

	g_return_if_fail(account != NULL);
	g_return_if_fail(ui      != NULL);
	g_return_if_fail(name    != NULL);

	setting = g_new0(PurpleAccountSetting, 1);

	setting->ui            = g_strdup(ui);
	g_value_init(&setting->value, G_TYPE_INT);
	g_value_set_int(&setting->value, value);

	table = get_ui_settings_table(account, ui);

	g_hash_table_insert(table, g_strdup(name), setting);

	purple_accounts_schedule_save();
}

void
purple_account_set_ui_string(PurpleAccount *account, const char *ui,
						   const char *name, const char *value)
{
	PurpleAccountSetting *setting;
	GHashTable *table;

	g_return_if_fail(account != NULL);
	g_return_if_fail(ui      != NULL);
	g_return_if_fail(name    != NULL);

	setting = g_new0(PurpleAccountSetting, 1);

	setting->ui           = g_strdup(ui);
	g_value_init(&setting->value, G_TYPE_STRING);
	g_value_set_string(&setting->value, value);

	table = get_ui_settings_table(account, ui);

	g_hash_table_insert(table, g_strdup(name), setting);

	purple_accounts_schedule_save();
}

void
purple_account_set_ui_bool(PurpleAccount *account, const char *ui,
						 const char *name, gboolean value)
{
	PurpleAccountSetting *setting;
	GHashTable *table;

	g_return_if_fail(account != NULL);
	g_return_if_fail(ui      != NULL);
	g_return_if_fail(name    != NULL);

	setting = g_new0(PurpleAccountSetting, 1);

	setting->ui         = g_strdup(ui);
	g_value_init(&setting->value, G_TYPE_BOOLEAN);
	g_value_set_boolean(&setting->value, value);

	table = get_ui_settings_table(account, ui);

	g_hash_table_insert(table, g_strdup(name), setting);

	purple_accounts_schedule_save();
}

static PurpleConnectionState
purple_account_get_state(const PurpleAccount *account)
{
	PurpleConnection *gc;

	g_return_val_if_fail(account != NULL, PURPLE_CONNECTION_DISCONNECTED);

	gc = purple_account_get_connection(account);
	if (!gc)
		return PURPLE_CONNECTION_DISCONNECTED;

	return purple_connection_get_state(gc);
}

gboolean
purple_account_is_connected(const PurpleAccount *account)
{
	return (purple_account_get_state(account) == PURPLE_CONNECTION_CONNECTED);
}

gboolean
purple_account_is_connecting(const PurpleAccount *account)
{
	return (purple_account_get_state(account) == PURPLE_CONNECTION_CONNECTING);
}

gboolean
purple_account_is_disconnected(const PurpleAccount *account)
{
	return (purple_account_get_state(account) == PURPLE_CONNECTION_DISCONNECTED);
}

const char *
purple_account_get_username(const PurpleAccount *account)
{
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(account != NULL, NULL);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);
	return priv->username;
}

static void
purple_account_get_password_got(PurpleAccount *account,
	const gchar *password, GError *error, gpointer data)
{
	PurpleCallbackBundle *cbb = data;
	PurpleKeyringReadCallback cb;
	PurpleAccountPrivate *priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	purple_debug_info("account",
		"Read password for account %s from async keyring.\n",
		purple_account_get_username(account));

	purple_str_wipe(priv->password);
	priv->password = g_strdup(password);

	cb = (PurpleKeyringReadCallback)cbb->cb;
	if (cb != NULL)
		cb(account, password, error, cbb->data);

	g_free(cbb);
}

void
purple_account_get_password(PurpleAccount *account,
	PurpleKeyringReadCallback cb, gpointer data)
{
	PurpleAccountPrivate *priv;

	if (account == NULL) {
		cb(NULL, NULL, NULL, data);
		return;
	}

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	if (priv->password != NULL) {
		purple_debug_info("account",
			"Reading password for account %s from cache.\n",
			purple_account_get_username(account));
		cb(account, priv->password, NULL, data);
	} else {
		PurpleCallbackBundle *cbb = g_new0(PurpleCallbackBundle, 1);
		cbb->cb = PURPLE_CALLBACK(cb);
		cbb->data = data;

		purple_debug_info("account",
			"Reading password for account %s from async keyring.\n",
			purple_account_get_username(account));
		purple_keyring_get_password(account, 
			purple_account_get_password_got, cbb);
	}
}

const char *
purple_account_get_private_alias(const PurpleAccount *account)
{
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(account != NULL, NULL);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);
	return priv->alias;
}

const char *
purple_account_get_user_info(const PurpleAccount *account)
{
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(account != NULL, NULL);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);
	return priv->user_info;
}

const char *
purple_account_get_buddy_icon_path(const PurpleAccount *account)
{
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(account != NULL, NULL);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);
	return priv->buddy_icon_path;
}

const char *
purple_account_get_protocol_id(const PurpleAccount *account)
{
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(account != NULL, NULL);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);
	return priv->protocol_id;
}

const char *
purple_account_get_protocol_name(const PurpleAccount *account)
{
	PurplePluginProtocolInfo *p;

	g_return_val_if_fail(account != NULL, NULL);

	p = purple_find_protocol_info(purple_account_get_protocol_id(account));

	return ((p && p->name) ? _(p->name) : _("Unknown"));
}

PurpleConnection *
purple_account_get_connection(const PurpleAccount *account)
{
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(account != NULL, NULL);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);
	return priv->gc;
}

const gchar *
purple_account_get_name_for_display(const PurpleAccount *account)
{
	PurpleBuddy *self = NULL;
	PurpleConnection *gc = NULL;
	const gchar *name = NULL, *username = NULL, *displayname = NULL;

	name = purple_account_get_private_alias(account);

	if (name) {
		return name;
	}

	username = purple_account_get_username(account);
	self = purple_blist_find_buddy((PurpleAccount *)account, username);

	if (self) {
		const gchar *calias= purple_buddy_get_contact_alias(self);

		/* We don't want to return the buddy name if the buddy/contact
		 * doesn't have an alias set. */
		if (!purple_strequal(username, calias)) {
			return calias;
		}
	}

	gc = purple_account_get_connection(account);
	displayname = purple_connection_get_display_name(gc);

	if (displayname) {
		return displayname;
	}

	return username;
}

gboolean
purple_account_get_remember_password(const PurpleAccount *account)
{
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(account != NULL, FALSE);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);
	return priv->remember_pass;
}

gboolean
purple_account_get_check_mail(const PurpleAccount *account)
{
	g_return_val_if_fail(account != NULL, FALSE);

	return purple_account_get_bool(account, "check-mail", FALSE);
}

gboolean
purple_account_get_enabled(const PurpleAccount *account, const char *ui)
{
	g_return_val_if_fail(account != NULL, FALSE);
	g_return_val_if_fail(ui      != NULL, FALSE);

	return purple_account_get_ui_bool(account, ui, "auto-login", FALSE);
}

PurpleProxyInfo *
purple_account_get_proxy_info(const PurpleAccount *account)
{
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(account != NULL, NULL);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);
	return priv->proxy_info;
}

PurpleAccountPrivacyType
purple_account_get_privacy_type(const PurpleAccount *account)
{
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(account != NULL, PURPLE_ACCOUNT_PRIVACY_ALLOW_ALL);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);
	return priv->privacy_type;
}

gboolean
purple_account_privacy_permit_add(PurpleAccount *account, const char *who,
						gboolean local_only)
{
	GSList *l;
	char *name;
	PurpleBuddy *buddy;
	PurpleBlistUiOps *blist_ops;
	PurpleAccountPrivate *priv;
	PurpleAccountUiOps *ui_ops = purple_accounts_get_ui_ops();

	g_return_val_if_fail(account != NULL, FALSE);
	g_return_val_if_fail(who     != NULL, FALSE);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);
	name = g_strdup(purple_normalize(account, who));

	for (l = priv->permit; l != NULL; l = l->next) {
		if (g_str_equal(name, l->data))
			/* This buddy already exists */
			break;
	}

	if (l != NULL)
	{
		/* This buddy already exists, so bail out */
		g_free(name);
		return FALSE;
	}

	priv->permit = g_slist_append(priv->permit, name);

	if (!local_only && purple_account_is_connected(account))
		serv_add_permit(purple_account_get_connection(account), who);

	if (ui_ops != NULL && ui_ops->permit_added != NULL)
		ui_ops->permit_added(account, who);

	blist_ops = purple_blist_get_ui_ops();
	if (blist_ops != NULL && blist_ops->save_account != NULL)
		blist_ops->save_account(account);

	/* This lets the UI know a buddy has had its privacy setting changed */
	buddy = purple_blist_find_buddy(account, name);
	if (buddy != NULL) {
		purple_signal_emit(purple_blist_get_handle(),
                "buddy-privacy-changed", buddy);
	}
	return TRUE;
}

gboolean
purple_account_privacy_permit_remove(PurpleAccount *account, const char *who,
						   gboolean local_only)
{
	GSList *l;
	const char *name;
	PurpleBuddy *buddy;
	char *del;
	PurpleBlistUiOps *blist_ops;
	PurpleAccountPrivate *priv;
	PurpleAccountUiOps *ui_ops = purple_accounts_get_ui_ops();

	g_return_val_if_fail(account != NULL, FALSE);
	g_return_val_if_fail(who     != NULL, FALSE);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);
	name = purple_normalize(account, who);

	for (l = priv->permit; l != NULL; l = l->next) {
		if (g_str_equal(name, l->data))
			/* We found the buddy we were looking for */
			break;
	}

	if (l == NULL)
		/* We didn't find the buddy we were looking for, so bail out */
		return FALSE;

	/* We should not free l->data just yet. There can be occasions where
	 * l->data == who. In such cases, freeing l->data here can cause crashes
	 * later when who is used. */
	del = l->data;
	priv->permit = g_slist_delete_link(priv->permit, l);

	if (!local_only && purple_account_is_connected(account))
		serv_rem_permit(purple_account_get_connection(account), who);

	if (ui_ops != NULL && ui_ops->permit_removed != NULL)
		ui_ops->permit_removed(account, who);

	blist_ops = purple_blist_get_ui_ops();
	if (blist_ops != NULL && blist_ops->save_account != NULL)
		blist_ops->save_account(account);

	buddy = purple_blist_find_buddy(account, name);
	if (buddy != NULL) {
		purple_signal_emit(purple_blist_get_handle(),
                "buddy-privacy-changed", buddy);
	}
	g_free(del);
	return TRUE;
}

gboolean
purple_account_privacy_deny_add(PurpleAccount *account, const char *who,
					  gboolean local_only)
{
	GSList *l;
	char *name;
	PurpleBuddy *buddy;
	PurpleBlistUiOps *blist_ops;
	PurpleAccountPrivate *priv;
	PurpleAccountUiOps *ui_ops = purple_accounts_get_ui_ops();

	g_return_val_if_fail(account != NULL, FALSE);
	g_return_val_if_fail(who     != NULL, FALSE);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);
	name = g_strdup(purple_normalize(account, who));

	for (l = priv->deny; l != NULL; l = l->next) {
		if (g_str_equal(name, l->data))
			/* This buddy already exists */
			break;
	}

	if (l != NULL)
	{
		/* This buddy already exists, so bail out */
		g_free(name);
		return FALSE;
	}

	priv->deny = g_slist_append(priv->deny, name);

	if (!local_only && purple_account_is_connected(account))
		serv_add_deny(purple_account_get_connection(account), who);

	if (ui_ops != NULL && ui_ops->deny_added != NULL)
		ui_ops->deny_added(account, who);

	blist_ops = purple_blist_get_ui_ops();
	if (blist_ops != NULL && blist_ops->save_account != NULL)
		blist_ops->save_account(account);

	buddy = purple_blist_find_buddy(account, name);
	if (buddy != NULL) {
		purple_signal_emit(purple_blist_get_handle(),
                "buddy-privacy-changed", buddy);
	}
	return TRUE;
}

gboolean
purple_account_privacy_deny_remove(PurpleAccount *account, const char *who,
						 gboolean local_only)
{
	GSList *l;
	const char *normalized;
	char *name;
	PurpleBuddy *buddy;
	PurpleBlistUiOps *blist_ops;
	PurpleAccountPrivate *priv;
	PurpleAccountUiOps *ui_ops = purple_accounts_get_ui_ops();

	g_return_val_if_fail(account != NULL, FALSE);
	g_return_val_if_fail(who     != NULL, FALSE);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);
	normalized = purple_normalize(account, who);

	for (l = priv->deny; l != NULL; l = l->next) {
		if (g_str_equal(normalized, l->data))
			/* We found the buddy we were looking for */
			break;
	}

	if (l == NULL)
		/* We didn't find the buddy we were looking for, so bail out */
		return FALSE;

	buddy = purple_blist_find_buddy(account, normalized);

	name = l->data;
	priv->deny = g_slist_delete_link(priv->deny, l);

	if (!local_only && purple_account_is_connected(account))
		serv_rem_deny(purple_account_get_connection(account), name);

	if (ui_ops != NULL && ui_ops->deny_removed != NULL)
		ui_ops->deny_removed(account, who);

	if (buddy != NULL) {
		purple_signal_emit(purple_blist_get_handle(),
                "buddy-privacy-changed", buddy);
	}

	g_free(name);

	blist_ops = purple_blist_get_ui_ops();
	if (blist_ops != NULL && blist_ops->save_account != NULL)
		blist_ops->save_account(account);

	return TRUE;
}

/**
 * This makes sure your permit list contains all buddies from your
 * buddy list and ONLY buddies from your buddy list.
 */
static void
add_all_buddies_to_permit_list(PurpleAccount *account, gboolean local)
{
	GSList *list;
	PurpleAccountPrivate *priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	/* Remove anyone in the permit list who is not in the buddylist */
	for (list = priv->permit; list != NULL; ) {
		char *person = list->data;
		list = list->next;
		if (!purple_blist_find_buddy(account, person))
			purple_account_privacy_permit_remove(account, person, local);
	}

	/* Now make sure everyone in the buddylist is in the permit list */
	list = purple_blist_find_buddies(account, NULL);
	while (list != NULL)
	{
		PurpleBuddy *buddy = list->data;
		const gchar *name = purple_buddy_get_name(buddy);

		if (!g_slist_find_custom(priv->permit, name, (GCompareFunc)g_utf8_collate))
			purple_account_privacy_permit_add(account, name, local);
		list = g_slist_delete_link(list, list);
	}
}

void
purple_account_privacy_allow(PurpleAccount *account, const char *who)
{
	GSList *list;
	PurpleAccountPrivacyType type = purple_account_get_privacy_type(account);
	PurpleAccountPrivate *priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	switch (type) {
		case PURPLE_ACCOUNT_PRIVACY_ALLOW_ALL:
			return;
		case PURPLE_ACCOUNT_PRIVACY_ALLOW_USERS:
			purple_account_privacy_permit_add(account, who, FALSE);
			break;
		case PURPLE_ACCOUNT_PRIVACY_DENY_USERS:
			purple_account_privacy_deny_remove(account, who, FALSE);
			break;
		case PURPLE_ACCOUNT_PRIVACY_DENY_ALL:
			{
				/* Empty the allow-list. */
				const char *norm = purple_normalize(account, who);
				for (list = priv->permit; list != NULL;) {
					char *person = list->data;
					list = list->next;
					if (!purple_strequal(norm, person))
						purple_account_privacy_permit_remove(account, person, FALSE);
				}
				purple_account_privacy_permit_add(account, who, FALSE);
				purple_account_set_privacy_type(account, PURPLE_ACCOUNT_PRIVACY_ALLOW_USERS);
			}
			break;
		case PURPLE_ACCOUNT_PRIVACY_ALLOW_BUDDYLIST:
			if (!purple_blist_find_buddy(account, who)) {
				add_all_buddies_to_permit_list(account, FALSE);
				purple_account_privacy_permit_add(account, who, FALSE);
				purple_account_set_privacy_type(account, PURPLE_ACCOUNT_PRIVACY_ALLOW_USERS);
			}
			break;
		default:
			g_return_if_reached();
	}

	/* Notify the server if the privacy setting was changed */
	if (type != purple_account_get_privacy_type(account) && purple_account_is_connected(account))
		serv_set_permit_deny(purple_account_get_connection(account));
}

void
purple_account_privacy_deny(PurpleAccount *account, const char *who)
{
	GSList *list;
	PurpleAccountPrivacyType type = purple_account_get_privacy_type(account);
	PurpleAccountPrivate *priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	switch (type) {
		case PURPLE_ACCOUNT_PRIVACY_ALLOW_ALL:
			{
				/* Empty the deny-list. */
				const char *norm = purple_normalize(account, who);
				for (list = priv->deny; list != NULL; ) {
					char *person = list->data;
					list = list->next;
					if (!purple_strequal(norm, person))
						purple_account_privacy_deny_remove(account, person, FALSE);
				}
				purple_account_privacy_deny_add(account, who, FALSE);
				purple_account_set_privacy_type(account, PURPLE_ACCOUNT_PRIVACY_DENY_USERS);
			}
			break;
		case PURPLE_ACCOUNT_PRIVACY_ALLOW_USERS:
			purple_account_privacy_permit_remove(account, who, FALSE);
			break;
		case PURPLE_ACCOUNT_PRIVACY_DENY_USERS:
			purple_account_privacy_deny_add(account, who, FALSE);
			break;
		case PURPLE_ACCOUNT_PRIVACY_DENY_ALL:
			break;
		case PURPLE_ACCOUNT_PRIVACY_ALLOW_BUDDYLIST:
			if (purple_blist_find_buddy(account, who)) {
				add_all_buddies_to_permit_list(account, FALSE);
				purple_account_privacy_permit_remove(account, who, FALSE);
				purple_account_set_privacy_type(account, PURPLE_ACCOUNT_PRIVACY_ALLOW_USERS);
			}
			break;
		default:
			g_return_if_reached();
	}

	/* Notify the server if the privacy setting was changed */
	if (type != purple_account_get_privacy_type(account) && purple_account_is_connected(account))
		serv_set_permit_deny(purple_account_get_connection(account));
}

GSList *
purple_account_privacy_get_permitted(PurpleAccount *account)
{
	PurpleAccountPrivate *priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->permit;
}

GSList *
purple_account_privacy_get_denied(PurpleAccount *account)
{
	PurpleAccountPrivate *priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->deny;
}

gboolean
purple_account_privacy_check(PurpleAccount *account, const char *who)
{
	GSList *list;
	PurpleAccountPrivate *priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	switch (purple_account_get_privacy_type(account)) {
		case PURPLE_ACCOUNT_PRIVACY_ALLOW_ALL:
			return TRUE;

		case PURPLE_ACCOUNT_PRIVACY_DENY_ALL:
			return FALSE;

		case PURPLE_ACCOUNT_PRIVACY_ALLOW_USERS:
			who = purple_normalize(account, who);
			for (list=priv->permit; list!=NULL; list=list->next) {
				if (g_str_equal(who, list->data))
					return TRUE;
			}
			return FALSE;

		case PURPLE_ACCOUNT_PRIVACY_DENY_USERS:
			who = purple_normalize(account, who);
			for (list=priv->deny; list!=NULL; list=list->next) {
				if (g_str_equal(who, list->data))
					return FALSE;
			}
			return TRUE;

		case PURPLE_ACCOUNT_PRIVACY_ALLOW_BUDDYLIST:
			return (purple_blist_find_buddy(account, who) != NULL);

		default:
			g_return_val_if_reached(TRUE);
	}
}

PurpleStatus *
purple_account_get_active_status(const PurpleAccount *account)
{
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(account   != NULL, NULL);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);
	return purple_presence_get_active_status(priv->presence);
}

PurpleStatus *
purple_account_get_status(const PurpleAccount *account, const char *status_id)
{
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(account   != NULL, NULL);
	g_return_val_if_fail(status_id != NULL, NULL);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	return purple_presence_get_status(priv->presence, status_id);
}

PurpleStatusType *
purple_account_get_status_type(const PurpleAccount *account, const char *id)
{
	GList *l;

	g_return_val_if_fail(account != NULL, NULL);
	g_return_val_if_fail(id      != NULL, NULL);

	for (l = purple_account_get_status_types(account); l != NULL; l = l->next)
	{
		PurpleStatusType *status_type = (PurpleStatusType *)l->data;

		if (purple_strequal(purple_status_type_get_id(status_type), id))
			return status_type;
	}

	return NULL;
}

PurpleStatusType *
purple_account_get_status_type_with_primitive(const PurpleAccount *account, PurpleStatusPrimitive primitive)
{
	GList *l;

	g_return_val_if_fail(account != NULL, NULL);

	for (l = purple_account_get_status_types(account); l != NULL; l = l->next)
	{
		PurpleStatusType *status_type = (PurpleStatusType *)l->data;

		if (purple_status_type_get_primitive(status_type) == primitive)
			return status_type;
	}

	return NULL;
}

PurplePresence *
purple_account_get_presence(const PurpleAccount *account)
{
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(account != NULL, NULL);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);
	return priv->presence;
}

gboolean
purple_account_is_status_active(const PurpleAccount *account,
							  const char *status_id)
{
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(account   != NULL, FALSE);
	g_return_val_if_fail(status_id != NULL, FALSE);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	return purple_presence_is_status_active(priv->presence, status_id);
}

GList *
purple_account_get_status_types(const PurpleAccount *account)
{
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(account != NULL, NULL);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);
	return priv->status_types;
}

int
purple_account_get_int(const PurpleAccount *account, const char *name,
					 int default_value)
{
	PurpleAccountSetting *setting;
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(account != NULL, default_value);
	g_return_val_if_fail(name    != NULL, default_value);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	setting = g_hash_table_lookup(priv->settings, name);

	if (setting == NULL)
		return default_value;

	g_return_val_if_fail(G_VALUE_HOLDS_INT(&setting->value), default_value);

	return g_value_get_int(&setting->value);
}

const char *
purple_account_get_string(const PurpleAccount *account, const char *name,
						const char *default_value)
{
	PurpleAccountSetting *setting;
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(account != NULL, default_value);
	g_return_val_if_fail(name    != NULL, default_value);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	setting = g_hash_table_lookup(priv->settings, name);

	if (setting == NULL)
		return default_value;

	g_return_val_if_fail(G_VALUE_HOLDS_STRING(&setting->value), default_value);

	return g_value_get_string(&setting->value);
}

gboolean
purple_account_get_bool(const PurpleAccount *account, const char *name,
					  gboolean default_value)
{
	PurpleAccountSetting *setting;
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(account != NULL, default_value);
	g_return_val_if_fail(name    != NULL, default_value);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	setting = g_hash_table_lookup(priv->settings, name);

	if (setting == NULL)
		return default_value;

	g_return_val_if_fail(G_VALUE_HOLDS_BOOLEAN(&setting->value), default_value);

	return g_value_get_boolean(&setting->value);
}

int
purple_account_get_ui_int(const PurpleAccount *account, const char *ui,
						const char *name, int default_value)
{
	PurpleAccountSetting *setting;
	PurpleAccountPrivate *priv;
	GHashTable *table;

	g_return_val_if_fail(account != NULL, default_value);
	g_return_val_if_fail(ui      != NULL, default_value);
	g_return_val_if_fail(name    != NULL, default_value);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	if ((table = g_hash_table_lookup(priv->ui_settings, ui)) == NULL)
		return default_value;

	if ((setting = g_hash_table_lookup(table, name)) == NULL)
		return default_value;

	g_return_val_if_fail(G_VALUE_HOLDS_INT(&setting->value), default_value);

	return g_value_get_int(&setting->value);
}

const char *
purple_account_get_ui_string(const PurpleAccount *account, const char *ui,
						   const char *name, const char *default_value)
{
	PurpleAccountSetting *setting;
	PurpleAccountPrivate *priv;
	GHashTable *table;

	g_return_val_if_fail(account != NULL, default_value);
	g_return_val_if_fail(ui      != NULL, default_value);
	g_return_val_if_fail(name    != NULL, default_value);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	if ((table = g_hash_table_lookup(priv->ui_settings, ui)) == NULL)
		return default_value;

	if ((setting = g_hash_table_lookup(table, name)) == NULL)
		return default_value;

	g_return_val_if_fail(G_VALUE_HOLDS_STRING(&setting->value), default_value);

	return g_value_get_string(&setting->value);
}

gboolean
purple_account_get_ui_bool(const PurpleAccount *account, const char *ui,
						 const char *name, gboolean default_value)
{
	PurpleAccountSetting *setting;
	PurpleAccountPrivate *priv;
	GHashTable *table;

	g_return_val_if_fail(account != NULL, default_value);
	g_return_val_if_fail(ui      != NULL, default_value);
	g_return_val_if_fail(name    != NULL, default_value);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	if ((table = g_hash_table_lookup(priv->ui_settings, ui)) == NULL)
		return default_value;

	if ((setting = g_hash_table_lookup(table, name)) == NULL)
		return default_value;

	g_return_val_if_fail(G_VALUE_HOLDS_BOOLEAN(&setting->value), default_value);

	return g_value_get_boolean(&setting->value);
}

gpointer
purple_account_get_ui_data(const PurpleAccount *account)
{
	g_return_val_if_fail(account != NULL, NULL);

	return account->ui_data;
}

void
purple_account_set_ui_data(PurpleAccount *account, gpointer ui_data)
{
	g_return_if_fail(account != NULL);

	account->ui_data = ui_data;
}

PurpleLog *
purple_account_get_log(PurpleAccount *account, gboolean create)
{
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(account != NULL, NULL);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	if(!priv->system_log && create){
		PurplePresence *presence;
		int login_time;

		presence = purple_account_get_presence(account);
		login_time = purple_presence_get_login_time(presence);

		priv->system_log	 = purple_log_new(PURPLE_LOG_SYSTEM,
				purple_account_get_username(account), account, NULL,
				(login_time != 0) ? login_time : time(NULL), NULL);
	}

	return priv->system_log;
}

void
purple_account_destroy_log(PurpleAccount *account)
{
	PurpleAccountPrivate *priv;

	g_return_if_fail(account != NULL);

	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	if(priv->system_log){
		purple_log_free(priv->system_log);
		priv->system_log = NULL;
	}
}

void
purple_account_add_buddy(PurpleAccount *account, PurpleBuddy *buddy, const char *message)
{
	PurplePluginProtocolInfo *prpl_info = NULL;
	PurpleConnection *gc;

	g_return_if_fail(account != NULL);
	g_return_if_fail(buddy != NULL);

	gc = purple_account_get_connection(account);
	if (gc != NULL)
		prpl_info = purple_connection_get_protocol_info(gc);

	if (prpl_info != NULL) {
		if (PURPLE_PROTOCOL_PLUGIN_HAS_FUNC(prpl_info, add_buddy))
			prpl_info->add_buddy(gc, buddy, purple_buddy_get_group(buddy), message);
	}
}

void
purple_account_add_buddies(PurpleAccount *account, GList *buddies, const char *message)
{
	PurplePluginProtocolInfo *prpl_info = NULL;
	PurpleConnection *gc = purple_account_get_connection(account);

	if (gc != NULL)
		prpl_info = purple_connection_get_protocol_info(gc);

	if (prpl_info) {
		GList *cur, *groups = NULL;

		/* Make a list of what group each buddy is in */
		for (cur = buddies; cur != NULL; cur = cur->next) {
			PurpleBuddy *buddy = cur->data;
			groups = g_list_append(groups, purple_buddy_get_group(buddy));
		}

		if (PURPLE_PROTOCOL_PLUGIN_HAS_FUNC(prpl_info, add_buddies))
			prpl_info->add_buddies(gc, buddies, groups, message);
		else if (PURPLE_PROTOCOL_PLUGIN_HAS_FUNC(prpl_info, add_buddy)) {
			GList *curb = buddies, *curg = groups;

			while ((curb != NULL) && (curg != NULL)) {
				prpl_info->add_buddy(gc, curb->data, curg->data, message);
				curb = curb->next;
				curg = curg->next;
			}
		}

		g_list_free(groups);
	}
}

void
purple_account_remove_buddy(PurpleAccount *account, PurpleBuddy *buddy,
		PurpleGroup *group)
{
	PurplePluginProtocolInfo *prpl_info = NULL;
	PurpleConnection *gc = purple_account_get_connection(account);

	if (gc != NULL)
		prpl_info = purple_connection_get_protocol_info(gc);

	if (prpl_info && prpl_info->remove_buddy)
		prpl_info->remove_buddy(gc, buddy, group);
}

void
purple_account_remove_buddies(PurpleAccount *account, GList *buddies, GList *groups)
{
	PurplePluginProtocolInfo *prpl_info = NULL;
	PurpleConnection *gc = purple_account_get_connection(account);

	if (gc != NULL)
		prpl_info = purple_connection_get_protocol_info(gc);

	if (prpl_info) {
		if (prpl_info->remove_buddies)
			prpl_info->remove_buddies(gc, buddies, groups);
		else {
			GList *curb = buddies;
			GList *curg = groups;
			while ((curb != NULL) && (curg != NULL)) {
				purple_account_remove_buddy(account, curb->data, curg->data);
				curb = curb->next;
				curg = curg->next;
			}
		}
	}
}

void
purple_account_remove_group(PurpleAccount *account, PurpleGroup *group)
{
	PurplePluginProtocolInfo *prpl_info = NULL;
	PurpleConnection *gc = purple_account_get_connection(account);

	if (gc != NULL)
		prpl_info = purple_connection_get_protocol_info(gc);

	if (prpl_info && prpl_info->remove_group)
		prpl_info->remove_group(gc, group);
}

void
purple_account_change_password(PurpleAccount *account, const char *orig_pw,
		const char *new_pw)
{
	PurplePluginProtocolInfo *prpl_info = NULL;
	PurpleConnection *gc = purple_account_get_connection(account);

	purple_account_set_password(account, new_pw, NULL, NULL);

	if (gc != NULL)
		prpl_info = purple_connection_get_protocol_info(gc);

	if (prpl_info && prpl_info->change_passwd)
		prpl_info->change_passwd(gc, orig_pw, new_pw);
}

gboolean purple_account_supports_offline_message(PurpleAccount *account, PurpleBuddy *buddy)
{
	PurpleConnection *gc;
	PurplePluginProtocolInfo *prpl_info = NULL;

	g_return_val_if_fail(account, FALSE);
	g_return_val_if_fail(buddy, FALSE);

	gc = purple_account_get_connection(account);
	if (gc == NULL)
		return FALSE;

	prpl_info = purple_connection_get_protocol_info(gc);

	if (!prpl_info || !prpl_info->offline_message)
		return FALSE;
	return prpl_info->offline_message(buddy);
}

void
_purple_account_set_current_error(PurpleAccount *account,
		PurpleConnectionErrorInfo *new_err)
{
	PurpleConnectionErrorInfo *old_err;
	PurpleAccountPrivate *priv;

	g_return_if_fail(account != NULL);
	priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	old_err = priv->current_error;

	if(new_err == old_err)
		return;

	priv->current_error = new_err;

	purple_signal_emit(purple_accounts_get_handle(),
	                   "account-error-changed",
	                   account, old_err, new_err);
	purple_accounts_schedule_save();

	if(old_err)
		g_free(old_err->description);

	PURPLE_DBUS_UNREGISTER_POINTER(old_err);
	g_free(old_err);
}

const PurpleConnectionErrorInfo *
purple_account_get_current_error(PurpleAccount *account)
{
	PurpleAccountPrivate *priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	return priv->current_error;
}

void
purple_account_clear_current_error(PurpleAccount *account)
{
	_purple_account_set_current_error(account, NULL);
}

static xmlnode *
status_attribute_to_xmlnode(const PurpleStatus *status, const PurpleStatusType *type,
		const PurpleStatusAttribute *attr)
{
	xmlnode *node;
	const char *id;
	char *value = NULL;
	PurpleStatusAttribute *default_attr;
	GValue *default_value;
	GType attr_type;
	GValue *attr_value;

	id = purple_status_attribute_get_id(attr);
	g_return_val_if_fail(id, NULL);

	attr_value = purple_status_get_attr_value(status, id);
	g_return_val_if_fail(attr_value, NULL);
	attr_type = G_VALUE_TYPE(attr_value);

	/*
	 * If attr_value is a different type than it should be
	 * then don't write it to the file.
	 */
	default_attr = purple_status_type_get_attr(type, id);
	default_value = purple_status_attribute_get_value(default_attr);
	if (attr_type != G_VALUE_TYPE(default_value))
		return NULL;

	/*
	 * If attr_value is the same as the default for this status
	 * then there is no need to write it to the file.
	 */
	if (attr_type == G_TYPE_STRING)
	{
		const char *string_value = g_value_get_string(attr_value);
		const char *default_string_value = g_value_get_string(default_value);
		if (purple_strequal(string_value, default_string_value))
			return NULL;
		value = g_strdup(g_value_get_string(attr_value));
	}
	else if (attr_type == G_TYPE_INT)
	{
		int int_value = g_value_get_int(attr_value);
		if (int_value == g_value_get_int(default_value))
			return NULL;
		value = g_strdup_printf("%d", int_value);
	}
	else if (attr_type == G_TYPE_BOOLEAN)
	{
		gboolean boolean_value = g_value_get_boolean(attr_value);
		if (boolean_value == g_value_get_boolean(default_value))
			return NULL;
		value = g_strdup(boolean_value ?
								"true" : "false");
	}
	else
	{
		return NULL;
	}

	g_return_val_if_fail(value, NULL);

	node = xmlnode_new("attribute");

	xmlnode_set_attrib(node, "id", id);
	xmlnode_set_attrib(node, "value", value);

	g_free(value);

	return node;
}

static xmlnode *
status_attrs_to_xmlnode(const PurpleStatus *status)
{
	PurpleStatusType *type = purple_status_get_status_type(status);
	xmlnode *node, *child;
	GList *attrs, *attr;

	node = xmlnode_new("attributes");

	attrs = purple_status_type_get_attrs(type);
	for (attr = attrs; attr != NULL; attr = attr->next)
	{
		child = status_attribute_to_xmlnode(status, type, (const PurpleStatusAttribute *)attr->data);
		if (child)
			xmlnode_insert_child(node, child);
	}

	return node;
}

static xmlnode *
status_to_xmlnode(const PurpleStatus *status)
{
	xmlnode *node, *child;

	node = xmlnode_new("status");
	xmlnode_set_attrib(node, "type", purple_status_get_id(status));
	if (purple_status_get_name(status) != NULL)
		xmlnode_set_attrib(node, "name", purple_status_get_name(status));
	xmlnode_set_attrib(node, "active", purple_status_is_active(status) ? "true" : "false");

	child = status_attrs_to_xmlnode(status);
	xmlnode_insert_child(node, child);

	return node;
}

static xmlnode *
statuses_to_xmlnode(const PurplePresence *presence)
{
	xmlnode *node, *child;
	GList *statuses;
	PurpleStatus *status;

	node = xmlnode_new("statuses");

	statuses = purple_presence_get_statuses(presence);
	for (; statuses != NULL; statuses = statuses->next)
	{
		status = statuses->data;
		if (purple_status_type_is_saveable(purple_status_get_status_type(status)))
		{
			child = status_to_xmlnode(status);
			xmlnode_insert_child(node, child);
		}
	}

	return node;
}

static xmlnode *
proxy_settings_to_xmlnode(PurpleProxyInfo *proxy_info)
{
	xmlnode *node, *child;
	PurpleProxyType proxy_type;
	const char *value;
	int int_value;
	char buf[21];

	proxy_type = purple_proxy_info_get_type(proxy_info);

	node = xmlnode_new("proxy");

	child = xmlnode_new_child(node, "type");
	xmlnode_insert_data(child,
			(proxy_type == PURPLE_PROXY_USE_GLOBAL ? "global" :
			 proxy_type == PURPLE_PROXY_NONE       ? "none"   :
			 proxy_type == PURPLE_PROXY_HTTP       ? "http"   :
			 proxy_type == PURPLE_PROXY_SOCKS4     ? "socks4" :
			 proxy_type == PURPLE_PROXY_SOCKS5     ? "socks5" :
			 proxy_type == PURPLE_PROXY_TOR        ? "tor" :
			 proxy_type == PURPLE_PROXY_USE_ENVVAR ? "envvar" : "unknown"), -1);

	if ((value = purple_proxy_info_get_host(proxy_info)) != NULL)
	{
		child = xmlnode_new_child(node, "host");
		xmlnode_insert_data(child, value, -1);
	}

	if ((int_value = purple_proxy_info_get_port(proxy_info)) != 0)
	{
		g_snprintf(buf, sizeof(buf), "%d", int_value);
		child = xmlnode_new_child(node, "port");
		xmlnode_insert_data(child, buf, -1);
	}

	if ((value = purple_proxy_info_get_username(proxy_info)) != NULL)
	{
		child = xmlnode_new_child(node, "username");
		xmlnode_insert_data(child, value, -1);
	}

	if ((value = purple_proxy_info_get_password(proxy_info)) != NULL)
	{
		child = xmlnode_new_child(node, "password");
		xmlnode_insert_data(child, value, -1);
	}

	return node;
}

static xmlnode *
current_error_to_xmlnode(PurpleConnectionErrorInfo *err)
{
	xmlnode *node, *child;
	char type_str[3];

	node = xmlnode_new("current_error");

	if(err == NULL)
		return node;

	/* It doesn't make sense to have transient errors persist across a
	 * restart.
	 */
	if(!purple_connection_error_is_fatal (err->type))
		return node;

	child = xmlnode_new_child(node, "type");
	g_snprintf(type_str, sizeof(type_str), "%u", err->type);
	xmlnode_insert_data(child, type_str, -1);

	child = xmlnode_new_child(node, "description");
	if(err->description) {
		char *utf8ized = purple_utf8_try_convert(err->description);
		if(utf8ized == NULL)
			utf8ized = purple_utf8_salvage(err->description);
		xmlnode_insert_data(child, utf8ized, -1);
		g_free(utf8ized);
	}

	return node;
}

static void
setting_to_xmlnode(gpointer key, gpointer value, gpointer user_data)
{
	const char *name;
	PurpleAccountSetting *setting;
	xmlnode *node, *child;
	char buf[21];

	name    = (const char *)key;
	setting = (PurpleAccountSetting *)value;
	node    = (xmlnode *)user_data;

	child = xmlnode_new_child(node, "setting");
	xmlnode_set_attrib(child, "name", name);

	if (G_VALUE_HOLDS_INT(&setting->value)) {
		xmlnode_set_attrib(child, "type", "int");
		g_snprintf(buf, sizeof(buf), "%d", g_value_get_int(&setting->value));
		xmlnode_insert_data(child, buf, -1);
	}
	else if (G_VALUE_HOLDS_STRING(&setting->value) && g_value_get_string(&setting->value) != NULL) {
		xmlnode_set_attrib(child, "type", "string");
		xmlnode_insert_data(child, g_value_get_string(&setting->value), -1);
	}
	else if (G_VALUE_HOLDS_BOOLEAN(&setting->value)) {
		xmlnode_set_attrib(child, "type", "bool");
		g_snprintf(buf, sizeof(buf), "%d", g_value_get_boolean(&setting->value));
		xmlnode_insert_data(child, buf, -1);
	}
}

static void
ui_setting_to_xmlnode(gpointer key, gpointer value, gpointer user_data)
{
	const char *ui;
	GHashTable *table;
	xmlnode *node, *child;

	ui    = (const char *)key;
	table = (GHashTable *)value;
	node  = (xmlnode *)user_data;

	if (g_hash_table_size(table) > 0)
	{
		child = xmlnode_new_child(node, "settings");
		xmlnode_set_attrib(child, "ui", ui);
		g_hash_table_foreach(table, setting_to_xmlnode, child);
	}
}

xmlnode *
purple_account_to_xmlnode(PurpleAccount *account)
{
	xmlnode *node, *child;
	const char *tmp;
	PurplePresence *presence;
	PurpleProxyInfo *proxy_info;
	PurpleAccountPrivate *priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	node = xmlnode_new("account");

	child = xmlnode_new_child(node, "protocol");
	xmlnode_insert_data(child, purple_account_get_protocol_id(account), -1);

	child = xmlnode_new_child(node, "name");
	xmlnode_insert_data(child, purple_account_get_username(account), -1);

	if (purple_account_get_remember_password(account))
	{
		const char *keyring_id = NULL;
		const char *mode = NULL;
		char *data = NULL;
		GError *error = NULL;
		GDestroyNotify destroy = NULL;
		gboolean exported = purple_keyring_export_password(account,
			&keyring_id, &mode, &data, &error, &destroy);

		if (error != NULL) {
			purple_debug_error("account",
				"Failed to export password for account %s: %s.\n",
				purple_account_get_username(account),
				error->message);
		} else if (exported) {
			child = xmlnode_new_child(node, "password");
			if (keyring_id != NULL)
				xmlnode_set_attrib(child, "keyring_id", keyring_id);
			if (mode != NULL)
				xmlnode_set_attrib(child, "mode", mode);
			if (data != NULL)
				xmlnode_insert_data(child, data, -1);

			if (destroy != NULL)
				destroy(data);
		}
	}

	if ((tmp = purple_account_get_private_alias(account)) != NULL)
	{
		child = xmlnode_new_child(node, "alias");
		xmlnode_insert_data(child, tmp, -1);
	}

	if ((presence = purple_account_get_presence(account)) != NULL)
	{
		child = statuses_to_xmlnode(presence);
		xmlnode_insert_child(node, child);
	}

	if ((tmp = purple_account_get_user_info(account)) != NULL)
	{
		/* TODO: Do we need to call purple_str_strip_char(tmp, '\r') here? */
		child = xmlnode_new_child(node, "userinfo");
		xmlnode_insert_data(child, tmp, -1);
	}

	if (g_hash_table_size(priv->settings) > 0)
	{
		child = xmlnode_new_child(node, "settings");
		g_hash_table_foreach(priv->settings, setting_to_xmlnode, child);
	}

	if (g_hash_table_size(priv->ui_settings) > 0)
	{
		g_hash_table_foreach(priv->ui_settings, ui_setting_to_xmlnode, node);
	}

	if ((proxy_info = purple_account_get_proxy_info(account)) != NULL)
	{
		child = proxy_settings_to_xmlnode(proxy_info);
		xmlnode_insert_child(node, child);
	}

	child = current_error_to_xmlnode(priv->current_error);
	xmlnode_insert_child(node, child);

	return node;
}

/****************
 * GObject Code *
 ****************/

/* GObject Property names */
#define PROP_USERNAME_S           "username"
#define PROP_PRIVATE_ALIAS_S      "private-alias"
#define PROP_ENABLED_S            "enabled"
#define PROP_CONNECTION_S         "connection"
#define PROP_PROTOCOL_ID_S        "protocol-id"
#define PROP_USER_INFO_S          "userinfo"
#define PROP_BUDDY_ICON_PATH_S    "buddy-icon-path"
#define PROP_REMEMBER_PASSWORD_S  "remember-password"
#define PROP_CHECK_MAIL_S         "check-mail"

/* Set method for GObject properties */
static void
purple_account_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleAccount *account = PURPLE_ACCOUNT(obj);

	switch (param_id) {
		case PROP_USERNAME:
			purple_account_set_username(account, g_value_get_string(value));
			break;
		case PROP_PRIVATE_ALIAS:
			purple_account_set_private_alias(account, g_value_get_string(value));
			break;
		case PROP_ENABLED:
			purple_account_set_enabled(account, purple_core_get_ui(),
					g_value_get_boolean(value));
			break;
		case PROP_CONNECTION:
			purple_account_set_connection(account, g_value_get_object(value));
			break;
		case PROP_PROTOCOL_ID:
			purple_account_set_protocol_id(account, g_value_get_string(value));
			break;
		case PROP_USER_INFO:
			purple_account_set_user_info(account, g_value_get_string(value));
			break;
		case PROP_BUDDY_ICON_PATH:
			purple_account_set_buddy_icon_path(account,
					g_value_get_string(value));
			break;
		case PROP_REMEMBER_PASSWORD:
			purple_account_set_remember_password(account,
					g_value_get_boolean(value));
			break;
		case PROP_CHECK_MAIL:
			purple_account_set_check_mail(account, g_value_get_boolean(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Get method for GObject properties */
static void
purple_account_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PurpleAccount *account = PURPLE_ACCOUNT(obj);

	switch (param_id) {
		case PROP_USERNAME:
			g_value_set_string(value, purple_account_get_username(account));
			break;
		case PROP_PRIVATE_ALIAS:
			g_value_set_string(value, purple_account_get_private_alias(account));
			break;
		case PROP_ENABLED:
			g_value_set_boolean(value, purple_account_get_enabled(account,
					purple_core_get_ui()));
			break;
		case PROP_CONNECTION:
			g_value_set_object(value, purple_account_get_connection(account));
			break;
		case PROP_PROTOCOL_ID:
			g_value_set_string(value, purple_account_get_protocol_id(account));
			break;
		case PROP_USER_INFO:
			g_value_set_string(value, purple_account_get_user_info(account));
			break;
		case PROP_BUDDY_ICON_PATH:
			g_value_set_string(value,
					purple_account_get_buddy_icon_path(account));
			break;
		case PROP_REMEMBER_PASSWORD:
			g_value_set_boolean(value,
					purple_account_get_remember_password(account));
			break;
		case PROP_CHECK_MAIL:
			g_value_set_boolean(value, purple_account_get_check_mail(account));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* GObject initialization function */
static void purple_account_init(GTypeInstance *instance, gpointer klass)
{
	PurpleAccount *account = PURPLE_ACCOUNT(instance);
	PurpleAccountPrivate *priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	priv->settings = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, delete_setting);
	priv->ui_settings = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, (GDestroyNotify)g_hash_table_destroy);
	priv->system_log = NULL;

	priv->privacy_type = PURPLE_ACCOUNT_PRIVACY_ALLOW_ALL;

	PURPLE_DBUS_REGISTER_POINTER(account, PurpleAccount);
}

/* Called when done constructing */
static void
purple_account_constructed(GObject *object)
{
	PurpleAccount *account = PURPLE_ACCOUNT(object);
	PurpleAccountPrivate *priv = PURPLE_ACCOUNT_GET_PRIVATE(account);
	gchar *username, *protocol_id;
	PurplePluginProtocolInfo *prpl_info = NULL;
	PurpleStatusType *status_type;

	parent_class->constructed(object);

	g_object_get(object,
			PROP_USERNAME_S,    &username,
			PROP_PROTOCOL_ID_S, &protocol_id,
			NULL);

	purple_signal_emit(purple_accounts_get_handle(), "account-created",
			account);

	prpl_info = purple_find_protocol_info(protocol_id);
	if (prpl_info == NULL) {
		g_free(username);
		g_free(protocol_id);
		return;
	}

	if (prpl_info->status_types != NULL)
		purple_account_set_status_types(account,
				prpl_info->status_types(account));

	priv->presence = PURPLE_PRESENCE(purple_account_presence_new(account));

	status_type = purple_account_get_status_type_with_primitive(account,
			PURPLE_STATUS_AVAILABLE);
	if (status_type != NULL)
		purple_presence_set_status_active(priv->presence,
										purple_status_type_get_id(status_type),
										TRUE);
	else
		purple_presence_set_status_active(priv->presence,
										"offline",
										TRUE);

	g_free(username);
	g_free(protocol_id);
}

/* GObject dispose function */
static void
purple_account_dispose(GObject *object)
{
	GList *l;
	PurpleAccount *account = PURPLE_ACCOUNT(object);
	PurpleAccountPrivate *priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	purple_debug_info("account", "Destroying account %p\n", account);
	purple_signal_emit(purple_accounts_get_handle(), "account-destroying",
						account);

	for (l = purple_conversations_get_all(); l != NULL; l = l->next)
	{
		PurpleConversation *conv = (PurpleConversation *)l->data;

		if (purple_conversation_get_account(conv) == account)
			purple_conversation_set_account(conv, NULL);
	}

	purple_account_set_status_types(account, NULL);

	if (priv->proxy_info)
		purple_proxy_info_destroy(priv->proxy_info);

	if (priv->presence)
		g_object_unref(priv->presence);

	if(priv->system_log)
		purple_log_free(priv->system_log);

	if (priv->current_error) {
		g_free(priv->current_error->description);
		g_free(priv->current_error);
	}

	PURPLE_DBUS_UNREGISTER_POINTER(priv->current_error);
	PURPLE_DBUS_UNREGISTER_POINTER(account);

	parent_class->dispose(object);
}

/* GObject finalize function */
static void
purple_account_finalize(GObject *object)
{
	PurpleAccount *account = PURPLE_ACCOUNT(object);
	PurpleAccountPrivate *priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	g_free(priv->username);
	g_free(priv->alias);
	purple_str_wipe(priv->password);
	g_free(priv->user_info);
	g_free(priv->buddy_icon_path);
	g_free(priv->protocol_id);

	g_hash_table_destroy(priv->settings);
	g_hash_table_destroy(priv->ui_settings);

	while (priv->deny) {
		g_free(priv->deny->data);
		priv->deny = g_slist_delete_link(priv->deny, priv->deny);
	}

	while (priv->permit) {
		g_free(priv->permit->data);
		priv->permit = g_slist_delete_link(priv->permit, priv->permit);
	}

	parent_class->finalize(object);
}

/* Class initializer function */
static void
purple_account_class_init(PurpleAccountClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	obj_class->dispose = purple_account_dispose;
	obj_class->finalize = purple_account_finalize;
	obj_class->constructed = purple_account_constructed;

	/* Setup properties */
	obj_class->get_property = purple_account_get_property;
	obj_class->set_property = purple_account_set_property;

	g_object_class_install_property(obj_class, PROP_USERNAME,
			g_param_spec_string(PROP_USERNAME_S, _("Username"),
				_("The username for the account."), NULL,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT)
			);

	g_object_class_install_property(obj_class, PROP_PRIVATE_ALIAS,
			g_param_spec_string(PROP_PRIVATE_ALIAS_S, _("Private Alias"),
				_("The private alias for the account."), NULL,
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, PROP_USER_INFO,
			g_param_spec_string(PROP_USER_INFO_S, _("User information"),
				_("Detailed user information for the account."), NULL,
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, PROP_BUDDY_ICON_PATH,
			g_param_spec_string(PROP_BUDDY_ICON_PATH_S, _("Buddy icon path"),
				_("Path to the buddyicon for the account."), NULL,
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, PROP_ENABLED,
			g_param_spec_boolean(PROP_ENABLED_S, _("Enabled"),
				_("Whether the account is enabled or not."), FALSE,
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, PROP_REMEMBER_PASSWORD,
			g_param_spec_boolean(PROP_REMEMBER_PASSWORD_S, _("Remember password"),
				_("Whether to remember and store the password for this account."), FALSE,
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, PROP_CHECK_MAIL,
			g_param_spec_boolean(PROP_CHECK_MAIL_S, _("Check mail"),
				_("Whether to check mails for this account."), FALSE,
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, PROP_CONNECTION,
			g_param_spec_object(PROP_CONNECTION_S, _("Connection"),
				_("The connection for the account."), PURPLE_TYPE_CONNECTION,
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, PROP_PROTOCOL_ID,
			g_param_spec_string(PROP_PROTOCOL_ID_S, _("Protocol ID"),
				_("ID of the protocol that is responsible for the account."), NULL,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)
			);

	g_type_class_add_private(klass, sizeof(PurpleAccountPrivate));
}

GType
purple_account_get_type(void)
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleAccountClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_account_class_init,
			NULL,
			NULL,
			sizeof(PurpleAccount),
			0,
			(GInstanceInitFunc)purple_account_init,
			NULL,
		};

		type = g_type_register_static(G_TYPE_OBJECT,
				"PurpleAccount",
				&info, 0);
	}

	return type;
}

PurpleAccount *
purple_account_new(const char *username, const char *protocol_id)
{
	PurpleAccount *account;

	g_return_val_if_fail(username != NULL, NULL);
	g_return_val_if_fail(protocol_id != NULL, NULL);

	account = purple_accounts_find(username, protocol_id);

	if (account != NULL)
		return account;

	account = g_object_new(PURPLE_TYPE_ACCOUNT,
					PROP_USERNAME_S,    username,
					PROP_PROTOCOL_ID_S, protocol_id,
					NULL);

	return account;
}
