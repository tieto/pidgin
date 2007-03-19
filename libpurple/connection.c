/**
 * @file connection.c Connection API
 * @ingroup core
 *
 * purple
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "internal.h"
#include "account.h"
#include "blist.h"
#include "connection.h"
#include "dbus-maybe.h"
#include "debug.h"
#include "log.h"
#include "notify.h"
#include "prefs.h"
#include "proxy.h"
#include "request.h"
#include "server.h"
#include "signals.h"
#include "util.h"

static GList *connections = NULL;
static GList *connections_connecting = NULL;
static PurpleConnectionUiOps *connection_ui_ops = NULL;

static int connections_handle;

static gboolean
send_keepalive(gpointer data)
{
	PurpleConnection *gc = data;
	PurplePluginProtocolInfo *prpl_info = NULL;

	if (gc != NULL && gc->prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if (prpl_info && prpl_info->keepalive)
		prpl_info->keepalive(gc);

	return TRUE;
}

static void
update_keepalive(PurpleConnection *gc, gboolean on)
{
	PurplePluginProtocolInfo *prpl_info = NULL;

	if (gc != NULL && gc->prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if (!prpl_info || !prpl_info->keepalive)
		return;

	if (on && !gc->keepalive)
	{
		purple_debug_info("connection", "Activating keepalive.\n");
		gc->keepalive = purple_timeout_add(30000, send_keepalive, gc);
	}
	else if (!on && gc->keepalive > 0)
	{
		purple_debug_info("connection", "Deactivating keepalive.\n");
		purple_timeout_remove(gc->keepalive);
		gc->keepalive = 0;
	}
}

void
purple_connection_new(PurpleAccount *account, gboolean regist, const char *password)
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
			purple_debug_error("connection", "Can not connect to account %s without "
							 "a password.\n", purple_account_get_username(account));
			return;
		}
	}

	gc = g_new0(PurpleConnection, 1);
	PURPLE_DBUS_REGISTER_POINTER(gc, PurpleConnection);

	gc->prpl = prpl;
	if ((password != NULL) && (*password != '\0'))
		gc->password = g_strdup(password);
	purple_connection_set_account(gc, account);
	purple_connection_set_state(gc, PURPLE_CONNECTING);
	connections = g_list_append(connections, gc);
	purple_account_set_connection(account, gc);

	purple_signal_emit(purple_connections_get_handle(), "signing-on", gc);

	if (regist)
	{
		purple_debug_info("connection", "Registering.  gc = %p\n", gc);

		/* set this so we don't auto-reconnect after registering */
		gc->wants_to_die = TRUE;

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
purple_connection_destroy(PurpleConnection *gc)
{
	PurpleAccount *account;
	GSList *buddies, *tmp;
#if 0
	GList *wins;
#endif
	PurplePluginProtocolInfo *prpl_info = NULL;
	gboolean remove = FALSE;

	g_return_if_fail(gc != NULL);

	account = purple_connection_get_account(gc);

	purple_debug_info("connection", "Disconnecting connection %p\n", gc);

	if (purple_connection_get_state(gc) != PURPLE_CONNECTING)
		remove = TRUE;

	purple_signal_emit(purple_connections_get_handle(), "signing-off", gc);

	while (gc->buddy_chats)
	{
		PurpleConversation *b = gc->buddy_chats->data;

		gc->buddy_chats = g_slist_remove(gc->buddy_chats, b);
		purple_conv_chat_left(PURPLE_CONV_CHAT(b));
	}

	update_keepalive(gc, FALSE);

	purple_proxy_connect_cancel_with_handle(gc);

	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);
	if (prpl_info->close)
		(prpl_info->close)(gc);

	/* Clear out the proto data that was freed in the prpl close method*/
	buddies = purple_find_buddies(account, NULL);
	for (tmp = buddies; tmp; tmp = tmp->next) {
		PurpleBuddy *buddy = tmp->data;
		buddy->proto_data = NULL;
	}
	g_slist_free(buddies);

	connections = g_list_remove(connections, gc);

	purple_connection_set_state(gc, PURPLE_DISCONNECTED);

	if (remove)
		purple_blist_remove_account(account);

	purple_signal_emit(purple_connections_get_handle(), "signed-off", gc);

#if 0
	/* see comment later in file on if 0'd same code */
	/*
	 * XXX This is a hack! Remove this and replace it with a better event
	 *     notification system.
	 */
	for (wins = purple_get_windows(); wins != NULL; wins = wins->next) {
		PurpleConvWindow *win = (PurpleConvWindow *)wins->data;
		purple_conversation_update(purple_conv_window_get_conversation_at(win, 0),
								 PURPLE_CONV_ACCOUNT_OFFLINE);
	}
#endif

	purple_account_request_close_with_account(account);
	purple_request_close_with_handle(gc);
	purple_notify_close_with_handle(gc);

	purple_debug_info("connection", "Destroying connection %p\n", gc);

	purple_account_set_connection(account, NULL);

	g_free(gc->password);
	g_free(gc->display_name);

	if (gc->disconnect_timeout)
		purple_timeout_remove(gc->disconnect_timeout);

	PURPLE_DBUS_UNREGISTER_POINTER(gc);
	g_free(gc);
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

	g_return_if_fail(gc != NULL);

	if (gc->state == state)
		return;

	gc->state = state;

	ops = purple_connections_get_ui_ops();

	if (gc->state == PURPLE_CONNECTING) {
		connections_connecting = g_list_append(connections_connecting, gc);
	}
	else {
		connections_connecting = g_list_remove(connections_connecting, gc);
	}

	if (gc->state == PURPLE_CONNECTED) {
		PurpleAccount *account;
		PurplePresence *presence;

		account = purple_connection_get_account(gc);
		presence = purple_account_get_presence(account);

		/* Set the time the account came online */
		purple_presence_set_login_time(presence, time(NULL));

		if (purple_prefs_get_bool("/core/logging/log_system"))
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

		serv_set_permit_deny(gc);

		update_keepalive(gc, TRUE);
	}
	else if (gc->state == PURPLE_DISCONNECTED) {
		PurpleAccount *account = purple_connection_get_account(gc);

		if (purple_prefs_get_bool("/core/logging/log_system"))
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
purple_connection_set_account(PurpleConnection *gc, PurpleAccount *account)
{
	g_return_if_fail(gc != NULL);
	g_return_if_fail(account != NULL);

	gc->account = account;
}

void
purple_connection_set_display_name(PurpleConnection *gc, const char *name)
{
	g_return_if_fail(gc != NULL);

	g_free(gc->display_name);
	gc->display_name = g_strdup(name);
}

PurpleConnectionState
purple_connection_get_state(const PurpleConnection *gc)
{
	g_return_val_if_fail(gc != NULL, PURPLE_DISCONNECTED);

	return gc->state;
}

PurpleAccount *
purple_connection_get_account(const PurpleConnection *gc)
{
	g_return_val_if_fail(gc != NULL, NULL);

	return gc->account;
}

const char *
purple_connection_get_password(const PurpleConnection *gc)
{
	g_return_val_if_fail(gc != NULL, NULL);

	return gc->password;
}

const char *
purple_connection_get_display_name(const PurpleConnection *gc)
{
	g_return_val_if_fail(gc != NULL, NULL);

	return gc->display_name;
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
	PurpleAccount *account = data;
	char *password = g_strdup(purple_account_get_password(account));
	purple_account_disconnect(account);
	purple_account_set_password(account, password);
	g_free(password);
	return FALSE;
}

void
purple_connection_error(PurpleConnection *gc, const char *text)
{
	PurpleConnectionUiOps *ops;

	g_return_if_fail(gc   != NULL);

	if (text != NULL) {
		g_critical("purple_connection_error: check `text != NULL' failed");
		text = _("Unknown error");
	}

	/* If we've already got one error, we don't need any more */
	if (gc->disconnect_timeout)
		return;

	ops = purple_connections_get_ui_ops();

	if (ops != NULL && ops->report_disconnect != NULL)
		ops->report_disconnect(gc, text);

	gc->disconnect_timeout = purple_timeout_add(0, purple_connection_disconnect_cb,
			purple_connection_get_account(gc));
}

void
purple_connections_disconnect_all(void)
{
	GList *l;
	PurpleConnection *gc;

	while ((l = purple_connections_get_all()) != NULL) {
		gc = l->data;
		gc->wants_to_die = TRUE;
		purple_account_disconnect(gc->account);
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
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONNECTION));

	purple_signal_register(handle, "signed-on",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONNECTION));

	purple_signal_register(handle, "signing-off",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONNECTION));

	purple_signal_register(handle, "signed-off",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_CONNECTION));
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
