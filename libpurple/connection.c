/**
 * @file connection.c Connection API
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
#define _PURPLE_CONNECTION_C_

#include "internal.h"
#include "account.h"
#include "buddylist.h"
#include "connection.h"
#include "dbus-maybe.h"
#include "debug.h"
#include "enums.h"
#include "http.h"
#include "log.h"
#include "notify.h"
#include "prefs.h"
#include "proxy.h"
#include "request.h"
#include "server.h"
#include "signals.h"
#include "util.h"

#define KEEPALIVE_INTERVAL 30

#define PURPLE_CONNECTION_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_CONNECTION, PurpleConnectionPrivate))

/** @copydoc _PurpleConnectionPrivate */
typedef struct _PurpleConnectionPrivate  PurpleConnectionPrivate;

/** Private data for a connection */
struct _PurpleConnectionPrivate
{
	PurplePlugin *prpl;           /**< The protocol plugin.              */
	PurpleConnectionFlags flags;  /**< Connection flags.                 */

	PurpleConnectionState state;  /**< The connection state.             */

	PurpleAccount *account;       /**< The account being connected to.   */
	char *password;               /**< The password used.                */

	GSList *active_chats;         /**< A list of active chats
	                                  (#PurpleChatConversation structs). */
	void *proto_data;             /**< Protocol-specific data.            
	                                  TODO Remove this, and use
	                                       protocol-specific subclasses  */

	char *display_name;           /**< How you appear to other people.   */
	guint keepalive;              /**< Keep-alive.                       */

	/** Wants to Die state.  This is set when the user chooses to log out, or
	 * when the protocol is disconnected and should not be automatically
	 * reconnected (incorrect password, etc.).  prpls should rely on
	 * purple_connection_error() to set this for them rather than
	 * setting it themselves.
	 * @see purple_connection_error_is_fatal
	 */
	gboolean wants_to_die;

	guint disconnect_timeout;  /**< Timer used for nasty stack tricks         */
	time_t last_received;      /**< When we last received a packet. Set by the
	                                prpl to avoid sending unneeded keepalives */
};

/* GObject property enums */
enum
{
	PROP_0,
	PROP_PRPL,
	PROP_FLAGS,
	PROP_STATE,
	PROP_ACCOUNT,
	PROP_PASSWORD,
	PROP_DISPLAY_NAME,
	PROP_LAST
};

static GObjectClass *parent_class;

static GList *connections = NULL;
static GList *connections_connecting = NULL;
static PurpleConnectionUiOps *connection_ui_ops = NULL;

static int connections_handle;

/**************************************************************************
 * Connection API
 **************************************************************************/
static gboolean
send_keepalive(gpointer data)
{
	PurpleConnection *gc = data;
	PurplePluginProtocolInfo *prpl_info;
	PurpleConnectionPrivate *priv = PURPLE_CONNECTION_GET_PRIVATE(gc);

	/* Only send keep-alives if we haven't heard from the
	 * server in a while.
	 */
	if ((time(NULL) - priv->last_received) < KEEPALIVE_INTERVAL)
		return TRUE;

	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(priv->prpl);
	if (prpl_info->keepalive)
		prpl_info->keepalive(gc);

	return TRUE;
}

static void
update_keepalive(PurpleConnection *gc, gboolean on)
{
	PurplePluginProtocolInfo *prpl_info = NULL;
	PurpleConnectionPrivate *priv = PURPLE_CONNECTION_GET_PRIVATE(gc);

	if (priv != NULL && priv->prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(priv->prpl);

	if (!prpl_info || !prpl_info->keepalive)
		return;

	if (on && !priv->keepalive)
	{
		purple_debug_info("connection", "Activating keepalive.\n");
		priv->keepalive = purple_timeout_add_seconds(KEEPALIVE_INTERVAL, send_keepalive, gc);
	}
	else if (!on && priv->keepalive > 0)
	{
		purple_debug_info("connection", "Deactivating keepalive.\n");
		purple_timeout_remove(priv->keepalive);
		priv->keepalive = 0;
	}
}

/*
 * d:)->-<
 *
 * d:O-\-<
 *
 * d:D-/-<
 *
 * d8D->-< DANCE!
 */

void
purple_connection_set_state(PurpleConnection *gc, PurpleConnectionState state)
{
	PurpleConnectionUiOps *ops;
	PurpleConnectionPrivate *priv = PURPLE_CONNECTION_GET_PRIVATE(gc);

	g_return_if_fail(priv != NULL);

	if (priv->state == state)
		return;

	priv->state = state;

	ops = purple_connections_get_ui_ops();

	if (priv->state == PURPLE_CONNECTION_CONNECTING) {
		connections_connecting = g_list_append(connections_connecting, gc);
	}
	else {
		connections_connecting = g_list_remove(connections_connecting, gc);
	}

	if (priv->state == PURPLE_CONNECTION_CONNECTED) {
		PurpleAccount *account;
		PurplePresence *presence;

		account = purple_connection_get_account(gc);
		presence = purple_account_get_presence(account);

		/* Set the time the account came online */
		purple_presence_set_login_time(presence, time(NULL));

		if (purple_prefs_get_bool("/purple/logging/log_system"))
		{
			PurpleLog *log = purple_account_get_log(account, TRUE);

			if (log != NULL)
			{
				char *msg = g_strdup_printf(_("+++ %s signed on"),
											purple_account_get_username(account));
				purple_log_write(log, PURPLE_MESSAGE_SYSTEM,
							   purple_account_get_username(account),
							   purple_presence_get_login_time(presence),
							   msg);
				g_free(msg);
			}
		}

		if (ops != NULL && ops->connected != NULL)
			ops->connected(gc);

		purple_blist_add_account(account);

		purple_signal_emit(purple_connections_get_handle(), "signed-on", gc);
		purple_signal_emit_return_1(purple_connections_get_handle(), "autojoin", gc);

		serv_set_permit_deny(gc);

		update_keepalive(gc, TRUE);
	}
	else if (priv->state == PURPLE_CONNECTION_DISCONNECTED) {
		PurpleAccount *account = purple_connection_get_account(gc);

		if (purple_prefs_get_bool("/purple/logging/log_system"))
		{
			PurpleLog *log = purple_account_get_log(account, FALSE);

			if (log != NULL)
			{
				char *msg = g_strdup_printf(_("+++ %s signed off"),
											purple_account_get_username(account));
				purple_log_write(log, PURPLE_MESSAGE_SYSTEM,
							   purple_account_get_username(account), time(NULL),
							   msg);
				g_free(msg);
			}
		}

		purple_account_destroy_log(account);

		if (ops != NULL && ops->disconnected != NULL)
			ops->disconnected(gc);
	}
}

void
purple_connection_set_flags(PurpleConnection *gc, PurpleConnectionFlags flags)
{
	PurpleConnectionPrivate *priv = PURPLE_CONNECTION_GET_PRIVATE(gc);

	g_return_if_fail(priv != NULL);

	priv->flags = flags;
}

void
purple_connection_set_account(PurpleConnection *gc, PurpleAccount *account)
{
	PurpleConnectionPrivate *priv = PURPLE_CONNECTION_GET_PRIVATE(gc);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(account != NULL);

	priv->account = account;
}

void
purple_connection_set_display_name(PurpleConnection *gc, const char *name)
{
	PurpleConnectionPrivate *priv = PURPLE_CONNECTION_GET_PRIVATE(gc);

	g_return_if_fail(priv != NULL);

	g_free(priv->display_name);
	priv->display_name = g_strdup(name);
}

void
purple_connection_set_protocol_data(PurpleConnection *gc, void *proto_data)
{
	PurpleConnectionPrivate *priv = PURPLE_CONNECTION_GET_PRIVATE(gc);

	g_return_if_fail(priv != NULL);

	priv->proto_data = proto_data;
}

PurpleConnectionState
purple_connection_get_state(const PurpleConnection *gc)
{
	PurpleConnectionPrivate *priv = PURPLE_CONNECTION_GET_PRIVATE(gc);

	g_return_val_if_fail(priv != NULL, PURPLE_CONNECTION_DISCONNECTED);

	return priv->state;
}

PurpleConnectionFlags
purple_connection_get_flags(const PurpleConnection *gc)
{
	PurpleConnectionPrivate *priv = PURPLE_CONNECTION_GET_PRIVATE(gc);

	g_return_val_if_fail(priv != NULL, 0);

	return priv->flags;
}

PurpleAccount *
purple_connection_get_account(const PurpleConnection *gc)
{
	PurpleConnectionPrivate *priv = PURPLE_CONNECTION_GET_PRIVATE(gc);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->account;
}

PurplePlugin *
purple_connection_get_prpl(const PurpleConnection *gc)
{
	PurpleConnectionPrivate *priv = PURPLE_CONNECTION_GET_PRIVATE(gc);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->prpl;
}

const char *
purple_connection_get_password(const PurpleConnection *gc)
{
	PurpleConnectionPrivate *priv = PURPLE_CONNECTION_GET_PRIVATE(gc);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->password;
}

GSList *
purple_connection_get_active_chats(const PurpleConnection *gc)
{
	PurpleConnectionPrivate *priv = PURPLE_CONNECTION_GET_PRIVATE(gc);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->active_chats;
}

const char *
purple_connection_get_display_name(const PurpleConnection *gc)
{
	PurpleConnectionPrivate *priv = PURPLE_CONNECTION_GET_PRIVATE(gc);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->display_name;
}

void *
purple_connection_get_protocol_data(const PurpleConnection *gc)
{
	PurpleConnectionPrivate *priv = PURPLE_CONNECTION_GET_PRIVATE(gc);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->proto_data;
}

void
_purple_connection_add_active_chat(PurpleConnection *gc, PurpleChatConversation *chat)
{
	PurpleConnectionPrivate *priv = PURPLE_CONNECTION_GET_PRIVATE(gc);

	g_return_if_fail(priv != NULL);

	priv->active_chats = g_slist_append(priv->active_chats, chat);
}

void
_purple_connection_remove_active_chat(PurpleConnection *gc, PurpleChatConversation *chat)
{
	PurpleConnectionPrivate *priv = PURPLE_CONNECTION_GET_PRIVATE(gc);

	g_return_if_fail(priv != NULL);

	priv->active_chats = g_slist_remove(priv->active_chats, chat);
}

gboolean
_purple_connection_wants_to_die(const PurpleConnection *gc)
{
	PurpleConnectionPrivate *priv = PURPLE_CONNECTION_GET_PRIVATE(gc);

	g_return_val_if_fail(priv != NULL, FALSE);

	return priv->wants_to_die;
}

void
purple_connection_update_progress(PurpleConnection *gc, const char *text,
								size_t step, size_t count)
{
	PurpleConnectionUiOps *ops;

	g_return_if_fail(gc   != NULL);
	g_return_if_fail(text != NULL);
	g_return_if_fail(step < count);
	g_return_if_fail(count > 1);

	ops = purple_connections_get_ui_ops();

	if (ops != NULL && ops->connect_progress != NULL)
		ops->connect_progress(gc, text, step, count);
}

void
purple_connection_notice(PurpleConnection *gc, const char *text)
{
	PurpleConnectionUiOps *ops;

	g_return_if_fail(gc   != NULL);
	g_return_if_fail(text != NULL);

	ops = purple_connections_get_ui_ops();

	if (ops != NULL && ops->notice != NULL)
		ops->notice(gc, text);
}

static gboolean
purple_connection_disconnect_cb(gpointer data)
{
	PurpleAccount *account;
	PurpleConnection *gc;
	PurpleConnectionPrivate *priv;

	account = data;
	gc = purple_account_get_connection(account);
	priv = PURPLE_CONNECTION_GET_PRIVATE(gc);

	if (priv != NULL)
		priv->disconnect_timeout = 0;

	purple_account_disconnect(account);
	return FALSE;
}

void
purple_connection_error (PurpleConnection *gc,
                                PurpleConnectionError reason,
                                const char *description)
{
	PurpleConnectionUiOps *ops;
	PurpleConnectionPrivate *priv = PURPLE_CONNECTION_GET_PRIVATE(gc);

	g_return_if_fail(priv != NULL);
	/* This sanity check relies on PURPLE_CONNECTION_ERROR_OTHER_ERROR
	 * being the last member of the PurpleConnectionError enum in
	 * connection.h; if other reasons are added after it, this check should
	 * be updated.
	 */
	if (reason > PURPLE_CONNECTION_ERROR_OTHER_ERROR) {
		purple_debug_error("connection",
			"purple_connection_error: reason %u isn't a "
			"valid reason\n", reason);
		reason = PURPLE_CONNECTION_ERROR_OTHER_ERROR;
	}

	if (description == NULL) {
		purple_debug_error("connection", "purple_connection_error called with NULL description\n");
		description = _("Unknown error");
	}

	/* If we've already got one error, we don't need any more */
	if (priv->disconnect_timeout > 0)
		return;

	priv->wants_to_die = purple_connection_error_is_fatal (reason);

	purple_debug_info("connection", "Connection error on %p (reason: %u description: %s)\n",
	                  gc, reason, description);

	ops = purple_connections_get_ui_ops();

	if (ops && ops->report_disconnect)
		ops->report_disconnect(gc, reason, description);

	purple_signal_emit(purple_connections_get_handle(), "connection-error",
		gc, reason, description);

	priv->disconnect_timeout = purple_timeout_add(0, purple_connection_disconnect_cb,
			purple_connection_get_account(gc));
}

void
purple_connection_ssl_error (PurpleConnection *gc,
                             PurpleSslErrorType ssl_error)
{
	PurpleConnectionError reason;

	switch (ssl_error) {
		case PURPLE_SSL_HANDSHAKE_FAILED:
			reason = PURPLE_CONNECTION_ERROR_ENCRYPTION_ERROR;
			break;
		case PURPLE_SSL_CONNECT_FAILED:
			reason = PURPLE_CONNECTION_ERROR_NETWORK_ERROR;
			break;
		case PURPLE_SSL_CERTIFICATE_INVALID:
			/* TODO: maybe PURPLE_SSL_* should be more specific? */
			reason = PURPLE_CONNECTION_ERROR_CERT_OTHER_ERROR;
			break;
		default:
			g_assert_not_reached ();
			reason = PURPLE_CONNECTION_ERROR_CERT_OTHER_ERROR;
	}

	purple_connection_error (gc, reason,
		purple_ssl_strerror(ssl_error));
}

gboolean
purple_connection_error_is_fatal (PurpleConnectionError reason)
{
	switch (reason)
	{
		case PURPLE_CONNECTION_ERROR_NETWORK_ERROR:
		case PURPLE_CONNECTION_ERROR_ENCRYPTION_ERROR:
			return FALSE;
		case PURPLE_CONNECTION_ERROR_INVALID_USERNAME:
		case PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED:
		case PURPLE_CONNECTION_ERROR_AUTHENTICATION_IMPOSSIBLE:
		case PURPLE_CONNECTION_ERROR_NO_SSL_SUPPORT:
		case PURPLE_CONNECTION_ERROR_NAME_IN_USE:
		case PURPLE_CONNECTION_ERROR_INVALID_SETTINGS:
		case PURPLE_CONNECTION_ERROR_OTHER_ERROR:
		case PURPLE_CONNECTION_ERROR_CERT_NOT_PROVIDED:
		case PURPLE_CONNECTION_ERROR_CERT_UNTRUSTED:
		case PURPLE_CONNECTION_ERROR_CERT_EXPIRED:
		case PURPLE_CONNECTION_ERROR_CERT_NOT_ACTIVATED:
		case PURPLE_CONNECTION_ERROR_CERT_HOSTNAME_MISMATCH:
		case PURPLE_CONNECTION_ERROR_CERT_FINGERPRINT_MISMATCH:
		case PURPLE_CONNECTION_ERROR_CERT_SELF_SIGNED:
		case PURPLE_CONNECTION_ERROR_CERT_OTHER_ERROR:
			return TRUE;
		default:
			g_return_val_if_reached(TRUE);
	}
}

void purple_connection_update_last_received(PurpleConnection *gc)
{
	PurpleConnectionPrivate *priv = PURPLE_CONNECTION_GET_PRIVATE(gc);

	g_return_if_fail(priv != NULL);

	priv->last_received = time(NULL);
}

/**************************************************************************
 * GBoxed code
 **************************************************************************/
static PurpleConnectionErrorInfo *
purple_connection_error_info_copy(PurpleConnectionErrorInfo *err)
{
	PurpleConnectionErrorInfo *newerr;

	g_return_val_if_fail(err != NULL, NULL);

	newerr = g_new(PurpleConnectionErrorInfo, 1);
	newerr->type        = err->type;
	newerr->description = g_strdup(err->description);

	return newerr;
}

static void
purple_connection_error_info_free(PurpleConnectionErrorInfo *err)
{
	g_return_if_fail(err != NULL);

	g_free(err->description);
	g_free(err);
}

GType
purple_connection_error_info_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		type = g_boxed_type_register_static("PurpleConnectionErrorInfo",
				(GBoxedCopyFunc)purple_connection_error_info_copy,
				(GBoxedFreeFunc)purple_connection_error_info_free);
	}

	return type;
}

/**************************************************************************
 * GObject code
 **************************************************************************/
/* GObject Property names */
#define PROP_PRPL_S          "prpl"
#define PROP_FLAGS_S         "flags"
#define PROP_STATE_S         "state"
#define PROP_ACCOUNT_S       "account"
#define PROP_PASSWORD_S      "password"
#define PROP_DISPLAY_NAME_S  "display-name"

/* Set method for GObject properties */
static void
purple_connection_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleConnection *gc = PURPLE_CONNECTION(obj);
	PurpleConnectionPrivate *priv = PURPLE_CONNECTION_GET_PRIVATE(gc);

	switch (param_id) {
		case PROP_PRPL:
#warning TODO: change get_pointer to get_object when prpl is a GObject
			priv->prpl = g_value_get_pointer(value);
			break;
		case PROP_FLAGS:
			purple_connection_set_flags(gc, g_value_get_flags(value));
			break;
		case PROP_STATE:
			purple_connection_set_state(gc, g_value_get_enum(value));
			break;
		case PROP_ACCOUNT:
			purple_connection_set_account(gc, g_value_get_object(value));
			break;
		case PROP_PASSWORD:
			g_free(priv->password);
			priv->password = g_strdup(g_value_get_string(value));
			break;
		case PROP_DISPLAY_NAME:
			purple_connection_set_display_name(gc, g_value_get_string(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Get method for GObject properties */
static void
purple_connection_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PurpleConnection *gc = PURPLE_CONNECTION(obj);

	switch (param_id) {
		case PROP_PRPL:
#warning TODO: change set_pointer to set_object when prpl is a GObject
			g_value_set_pointer(value, purple_connection_get_prpl(gc));
			break;
		case PROP_FLAGS:
			g_value_set_flags(value, purple_connection_get_flags(gc));
			break;
		case PROP_STATE:
			g_value_set_enum(value, purple_connection_get_state(gc));
			break;
		case PROP_ACCOUNT:
			g_value_set_object(value, purple_connection_get_account(gc));
			break;
		case PROP_PASSWORD:
			g_value_set_string(value, purple_connection_get_password(gc));
			break;
		case PROP_DISPLAY_NAME:
			g_value_set_string(value, purple_connection_get_display_name(gc));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* GObject initialization function */
static void
purple_connection_init(GTypeInstance *instance, gpointer klass)
{
	PurpleConnection *gc = PURPLE_CONNECTION(instance);

	purple_connection_set_state(gc, PURPLE_CONNECTION_CONNECTING);
	connections = g_list_append(connections, gc);

	PURPLE_DBUS_REGISTER_POINTER(gc, PurpleConnection);
}

/* Called when done constructing */
static void
purple_connection_constructed(GObject *object)
{
	PurpleConnection *gc = PURPLE_CONNECTION(object);
	PurpleAccount *account;

	G_OBJECT_CLASS(parent_class)->constructed(object);

	g_object_get(gc, PROP_ACCOUNT_S, &account, NULL);
	purple_account_set_connection(account, gc);

	purple_signal_emit(purple_connections_get_handle(), "signing-on", gc);
}

/* GObject dispose function */
static void
purple_connection_dispose(GObject *object)
{
	PurpleConnection *gc = PURPLE_CONNECTION(object);
	PurpleConnectionPrivate *priv = PURPLE_CONNECTION_GET_PRIVATE(gc);
	PurpleAccount *account;
	GSList *buddies;
	PurplePluginProtocolInfo *prpl_info = NULL;
	gboolean remove = FALSE;

	account = purple_connection_get_account(gc);

	purple_debug_info("connection", "Disconnecting connection %p\n", gc);

	if (purple_connection_get_state(gc) != PURPLE_CONNECTION_CONNECTING)
		remove = TRUE;

	purple_signal_emit(purple_connections_get_handle(), "signing-off", gc);

	while (priv->active_chats)
	{
		PurpleChatConversation *b = priv->active_chats->data;

		priv->active_chats = g_slist_remove(priv->active_chats, b);
		purple_chat_conversation_leave(b);
	}

	update_keepalive(gc, FALSE);

	purple_http_conn_cancel_all(gc);
	purple_proxy_connect_cancel_with_handle(gc);

	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(priv->prpl);
	if (prpl_info->close)
		(prpl_info->close)(gc);

	/* Clear out the proto data that was freed in the prpl close method*/
	buddies = purple_blist_find_buddies(account, NULL);
	while (buddies != NULL) {
		PurpleBuddy *buddy = buddies->data;
		purple_buddy_set_protocol_data(buddy, NULL);
		buddies = g_slist_delete_link(buddies, buddies);
	}

	connections = g_list_remove(connections, gc);

	purple_connection_set_state(gc, PURPLE_CONNECTION_DISCONNECTED);

	if (remove)
		purple_blist_remove_account(account);

	purple_signal_emit(purple_connections_get_handle(), "signed-off", gc);

	purple_account_request_close_with_account(account);
	purple_request_close_with_handle(gc);
	purple_notify_close_with_handle(gc);

	purple_debug_info("connection", "Destroying connection %p\n", gc);

	purple_account_set_connection(account, NULL);

	if (priv->disconnect_timeout > 0)
		purple_timeout_remove(priv->disconnect_timeout);

	PURPLE_DBUS_UNREGISTER_POINTER(gc);

	G_OBJECT_CLASS(parent_class)->dispose(object);
}

/* GObject finalize function */
static void
purple_connection_finalize(GObject *object)
{
	PurpleConnectionPrivate *priv = PURPLE_CONNECTION_GET_PRIVATE(object);

	purple_str_wipe(priv->password);
	g_free(priv->display_name);

	G_OBJECT_CLASS(parent_class)->finalize(object);
}

/* Class initializer function */
static void purple_connection_class_init(PurpleConnectionClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	obj_class->dispose = purple_connection_dispose;
	obj_class->finalize = purple_connection_finalize;
	obj_class->constructed = purple_connection_constructed;

	/* Setup properties */
	obj_class->get_property = purple_connection_get_property;
	obj_class->set_property = purple_connection_set_property;

#warning TODO: change spec_pointer to spec_object when prpl is a GObject
	g_object_class_install_property(obj_class, PROP_PRPL,
			g_param_spec_pointer(PROP_PRPL_S, _("Protocol plugin"),
				_("The prpl that is using the connection."),
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)
			);

	g_object_class_install_property(obj_class, PROP_FLAGS,
			g_param_spec_flags(PROP_FLAGS_S, _("Connection flags"),
				_("The flags of the connection."),
				PURPLE_TYPE_CONNECTION_FLAGS, 0,
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, PROP_STATE,
			g_param_spec_enum(PROP_STATE_S, _("Connection state"),
				_("The current state of the connection."),
				PURPLE_TYPE_CONNECTION_STATE, PURPLE_CONNECTION_DISCONNECTED,
				G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, PROP_ACCOUNT,
			g_param_spec_object(PROP_ACCOUNT_S, _("Account"),
				_("The account using the connection."), PURPLE_TYPE_ACCOUNT,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT)
			);

	g_object_class_install_property(obj_class, PROP_PASSWORD,
			g_param_spec_string(PROP_PASSWORD_S, _("Password"),
				_("The password used for connection."), NULL,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)
			);

	g_object_class_install_property(obj_class, PROP_DISPLAY_NAME,
			g_param_spec_string(PROP_DISPLAY_NAME_S, _("Display name"),
				_("Your name that appears to other people."), NULL,
				G_PARAM_READWRITE)
			);

	g_type_class_add_private(klass, sizeof(PurpleConnectionPrivate));
}

GType
purple_connection_get_type(void)
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleConnectionClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_connection_class_init,
			NULL,
			NULL,
			sizeof(PurpleConnection),
			0,
			(GInstanceInitFunc)purple_connection_init,
			NULL,
		};

		type = g_type_register_static(G_TYPE_OBJECT, "PurpleConnection",
				&info, 0);
	}

	return type;
}

void
_purple_connection_new(PurpleAccount *account, gboolean regist, const char *password)
{
	PurpleConnection *gc;
	PurplePlugin *prpl;
	PurplePluginProtocolInfo *prpl_info;

	g_return_if_fail(account != NULL);

	if (!purple_account_is_disconnected(account))
		return;

	prpl = purple_find_prpl(purple_account_get_protocol_id(account));

	if (prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);
	else {
		gchar *message;

		message = g_strdup_printf(_("Missing protocol plugin for %s"),
			purple_account_get_username(account));
		purple_notify_error(NULL, regist ? _("Registration Error") :
						  _("Connection Error"), message, NULL);
		g_free(message);
		return;
	}

	if (regist)
	{
		if (prpl_info->register_user == NULL)
			return;
	}
	else
	{
		if (((password == NULL) || (*password == '\0')) &&
			!(prpl_info->options & OPT_PROTO_NO_PASSWORD) &&
			!(prpl_info->options & OPT_PROTO_PASSWORD_OPTIONAL))
		{
			purple_debug_error("connection", "Cannot connect to account %s without "
							 "a password.\n", purple_account_get_username(account));
			return;
		}
	}

	gc = g_object_new(PURPLE_TYPE_CONNECTION,
			PROP_PRPL_S,      prpl,
			PROP_PASSWORD_S,  password,
			PROP_ACCOUNT_S,   account,
			NULL);

	if (regist)
	{
		purple_debug_info("connection", "Registering.  gc = %p\n", gc);

		/* set this so we don't auto-reconnect after registering */
		PURPLE_CONNECTION_GET_PRIVATE(gc)->wants_to_die = TRUE;

		prpl_info->register_user(account);
	}
	else
	{
		purple_debug_info("connection", "Connecting. gc = %p\n", gc);

		purple_signal_emit(purple_accounts_get_handle(), "account-connecting", account);
		prpl_info->login(account);
	}
}

void
_purple_connection_new_unregister(PurpleAccount *account, const char *password,
		PurpleAccountUnregistrationCb cb, void *user_data)
{
	/* Lots of copy/pasted code to avoid API changes. You might want to integrate that into the previous function when posssible. */
	PurpleConnection *gc;
	PurplePlugin *prpl;
	PurplePluginProtocolInfo *prpl_info;

	g_return_if_fail(account != NULL);

	prpl = purple_find_prpl(purple_account_get_protocol_id(account));

	if (prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);
	else {
		gchar *message;

		message = g_strdup_printf(_("Missing protocol plugin for %s"),
								  purple_account_get_username(account));
		purple_notify_error(NULL, _("Unregistration Error"), message, NULL);
		g_free(message);
		return;
	}

	if (!purple_account_is_disconnected(account)) {
		prpl_info->unregister_user(account, cb, user_data);
		return;
	}

	if (((password == NULL) || (*password == '\0')) &&
		!(prpl_info->options & OPT_PROTO_NO_PASSWORD) &&
		!(prpl_info->options & OPT_PROTO_PASSWORD_OPTIONAL))
	{
		purple_debug_error("connection", "Cannot connect to account %s without "
						   "a password.\n", purple_account_get_username(account));
		return;
	}

	gc = g_object_new(PURPLE_TYPE_CONNECTION,
			PROP_PRPL_S,      prpl,
			PROP_PASSWORD_S,  password,
			PROP_ACCOUNT_S,   account,
			NULL);

	purple_debug_info("connection", "Unregistering.  gc = %p\n", gc);

	prpl_info->unregister_user(account, cb, user_data);
}

/**************************************************************************
 * Connections API
 **************************************************************************/
void
purple_connections_disconnect_all(void)
{
	GList *l;
	PurpleConnection *gc;
	PurpleConnectionPrivate *priv;

	while ((l = purple_connections_get_all()) != NULL) {
		gc = l->data;
		priv = PURPLE_CONNECTION_GET_PRIVATE(gc);
		priv->wants_to_die = TRUE;
		purple_account_disconnect(priv->account);
	}
}

GList *
purple_connections_get_all(void)
{
	return connections;
}

GList *
purple_connections_get_connecting(void)
{
	return connections_connecting;
}

void
purple_connections_set_ui_ops(PurpleConnectionUiOps *ops)
{
	connection_ui_ops = ops;
}

PurpleConnectionUiOps *
purple_connections_get_ui_ops(void)
{
	return connection_ui_ops;
}

void
purple_connections_init(void)
{
	void *handle = purple_connections_get_handle();

	purple_signal_register(handle, "signing-on",
						 purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
						 PURPLE_TYPE_CONNECTION);

	purple_signal_register(handle, "signed-on",
						 purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
						 PURPLE_TYPE_CONNECTION);

	purple_signal_register(handle, "signing-off",
						 purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
						 PURPLE_TYPE_CONNECTION);

	purple_signal_register(handle, "signed-off",
						 purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
						 PURPLE_TYPE_CONNECTION);

	purple_signal_register(handle, "connection-error",
	                       purple_marshal_VOID__POINTER_INT_POINTER,
	                       G_TYPE_NONE, 3, PURPLE_TYPE_CONNECTION,
	                       PURPLE_TYPE_CONNECTION_ERROR, G_TYPE_STRING);

	purple_signal_register(handle, "autojoin",
	                       purple_marshal_BOOLEAN__POINTER, G_TYPE_NONE, 1,
	                       PURPLE_TYPE_CONNECTION);

}

void
purple_connections_uninit(void)
{
	purple_signals_unregister_by_instance(purple_connections_get_handle());
}

void *
purple_connections_get_handle(void)
{
	return &connections_handle;
}
