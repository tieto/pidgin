/**
 * @file switchboard.c MSN switchboard functions
 *
 * gaim
 *
 * Copyright (C) 2003, Christian Hammond <chipx86@gnupdate.org>
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
#include "msn.h"
#include "prefs.h"
#include "switchboard.h"
#include "utils.h"

static GHashTable *switchboard_commands  = NULL;
static GHashTable *switchboard_msg_types = NULL;

/**************************************************************************
 * Utility functions
 **************************************************************************/
static gboolean
send_clientcaps(MsnSwitchBoard *swboard)
{
	MsnMessage *msg;

	if (swboard->buddy_icon_xfer != NULL)
		return TRUE;

	msg = msn_message_new();
	msn_message_set_content_type(msg, "text/x-clientcaps");
	msn_message_set_charset(msg, NULL);
	msn_message_set_attr(msg, "User-Agent", NULL);
	msn_message_set_body(msg, MSN_CLIENTINFO);

	if (!msn_switchboard_send_msg(swboard, msg)) {
		msn_switchboard_destroy(swboard);

		msn_message_destroy(msg);

		return FALSE;
	}

	msn_message_destroy(msg);

	return TRUE;
}

/**************************************************************************
 * Catch-all commands
 **************************************************************************/
static gboolean
blank_cmd(MsnServConn *servconn, const char *command, const char **params,
		   size_t param_count)
{
	return TRUE;
}

static gboolean
unknown_cmd(MsnServConn *servconn, const char *command, const char **params,
			 size_t param_count)
{
	gaim_debug(GAIM_DEBUG_ERROR, "msg",
			   "Handled switchboard message: %s\n", command);

	return FALSE;
}

/**************************************************************************
 * Switchboard Commands
 **************************************************************************/
static gboolean
ans_cmd(MsnServConn *servconn, const char *command, const char **params,
		 size_t param_count)
{
#if 0
	GaimAccount *account = servconn->session->account;
#endif
	MsnSwitchBoard *swboard = servconn->data;

#if 0
	if (swboard->chat != NULL)
		gaim_chat_add_user(GAIM_CHAT(swboard->chat),
						   gaim_account_get_username(account), NULL);
#endif

	return send_clientcaps(swboard);
}

static gboolean
bye_cmd(MsnServConn *servconn, const char *command, const char **params,
		 size_t param_count)
{
	GaimConnection *gc = servconn->session->account->gc;
	MsnSwitchBoard *swboard = servconn->data;
	const char *user = params[0];

	if (swboard->hidden)
		return TRUE;

	if (swboard->chat != NULL)
		gaim_chat_remove_user(GAIM_CHAT(swboard->chat), user, NULL);
	else {
		const char *username;
		GaimConversation *conv;
		struct buddy *b;
		char buf[MSN_BUF_LEN];

		if ((b = gaim_find_buddy(gc->account, user)) != NULL)
			username = gaim_get_buddy_alias(b);
		else
			username = user;

		*buf = '\0';

		if (param_count == 2 && atoi(params[1]) == 1) {
			if (gaim_prefs_get_bool("/plugins/prpl/msn/conv_timeout_notice")) {
				g_snprintf(buf, sizeof(buf),
						   _("The conversation has become inactive "
							 "and timed out."));
			}
		}
		else {
			if (gaim_prefs_get_bool("/plugins/prpl/msn/conv_close_notice")) {
				g_snprintf(buf, sizeof(buf),
						   _("%s has closed the conversation window."),
						   username);
			}
		}

		if (*buf != '\0' && (conv = gaim_find_conversation(user)) != NULL) {
			gaim_conversation_write(conv, NULL, buf, -1, WFLAG_SYSTEM,
									time(NULL));
		}

		msn_switchboard_destroy(swboard);

		return FALSE;
	}

	return TRUE;
}

static gboolean
iro_cmd(MsnServConn *servconn, const char *command, const char **params,
		 size_t param_count)
{
	GaimAccount *account = servconn->session->account;
	GaimConnection *gc = account->gc;
	MsnSwitchBoard *swboard = servconn->data;

	swboard->total_users = atoi(params[2]);

	if (swboard->total_users > 1) {
		if (swboard->chat == NULL) {
			swboard->chat = serv_got_joined_chat(gc, ++swboard->chat_id,
												 "MSN Chat");

			gaim_chat_add_user(GAIM_CHAT(swboard->chat),
							   gaim_account_get_username(account), NULL);
		}

		gaim_chat_add_user(GAIM_CHAT(swboard->chat), params[3], NULL);
	}

	return TRUE;
}

static gboolean
joi_cmd(MsnServConn *servconn, const char *command, const char **params,
		 size_t param_count)
{
	GaimAccount *account = servconn->session->account;
	GaimConnection *gc = account->gc;
	MsnSwitchBoard *swboard = servconn->data;
	const char *passport;

	passport = params[0];

	if (swboard->total_users == 1) {
		swboard->chat = serv_got_joined_chat(gc, ++swboard->chat_id,
											 "MSN Chat");
		gaim_chat_add_user(GAIM_CHAT(swboard->chat),
						   msn_user_get_passport(swboard->user), NULL);
		gaim_chat_add_user(GAIM_CHAT(swboard->chat),
						   gaim_account_get_username(account), NULL);

		msn_user_unref(swboard->user);
	}

	if (swboard->chat != NULL)
		gaim_chat_add_user(GAIM_CHAT(swboard->chat), passport, NULL);

	swboard->total_users++;

	while (servconn->txqueue) {
		char *buf = servconn->txqueue->data;

		servconn->txqueue = g_slist_remove(servconn->txqueue, buf);

		if (msn_servconn_write(swboard->servconn, buf, strlen(buf)) < 0) {
			msn_switchboard_destroy(swboard);

			return FALSE;
		}
	}

	return send_clientcaps(swboard);
}

static gboolean
msg_cmd(MsnServConn *servconn, const char *command, const char **params,
		 size_t param_count)
{
	gaim_debug(GAIM_DEBUG_INFO, "msn", "Found message. Parsing.\n");

	servconn->parsing_multiline = TRUE;
	servconn->multiline_type    = MSN_MULTILINE_MSG;
	servconn->multiline_len     = atoi(params[2]);
	
	servconn->msg_passport = g_strdup(params[0]);
	servconn->msg_friendly = g_strdup(params[1]);

	return TRUE;
}

static gboolean
nak_cmd(MsnServConn *servconn, const char *command, const char **params,
		 size_t param_count)
{
	/*
	 * TODO: Investigate this, as it seems to occur frequently with
	 *       the old prpl.
	 */
	gaim_notify_error(servconn->session->account->gc, NULL,
					  _("An MSN message may not have been received."), NULL);

	return TRUE;
}

static gboolean
out_cmd(MsnServConn *servconn, const char *command, const char **params,
		 size_t param_count)
{
	GaimConnection *gc = servconn->session->account->gc;
	MsnSwitchBoard *swboard = servconn->data;

	if (swboard->chat != NULL)
		serv_got_chat_left(gc, gaim_chat_get_id(GAIM_CHAT(swboard->chat)));

	msn_switchboard_destroy(swboard);

	return FALSE;
}

static gboolean
usr_cmd(MsnServConn *servconn, const char *command, const char **params,
		 size_t param_count)
{
	MsnSwitchBoard *swboard = servconn->data;

	if (!msn_switchboard_send_command(swboard, "CAL",
									  msn_user_get_passport(swboard->user))) {
		msn_switchboard_destroy(swboard);

		return FALSE;
	}

	return TRUE;
}

/**************************************************************************
 * Message Types
 **************************************************************************/
static gboolean
plain_msg(MsnServConn *servconn, MsnMessage *msg)
{
	GaimConnection *gc = servconn->session->account->gc;
	MsnSwitchBoard *swboard = servconn->data;
	char *body;
	const char *value;
	char *format;
	int flags = 0;

	body = g_strdup(msn_message_get_body(msg));

	gaim_debug(GAIM_DEBUG_INFO, "msn", "Checking User-Agent...\n");

	if ((value = msn_message_get_attr(msg, "User-Agent")) != NULL) {
		gaim_debug(GAIM_DEBUG_MISC, "msn", "value = '%s'\n", value);
		if (!g_ascii_strncasecmp(value, "Gaim", 4)) {
			gaim_debug(GAIM_DEBUG_INFO, "msn", "Setting GAIMUSER flag.\n");
			flags |= IM_FLAG_GAIMUSER;
		}
	}

	if ((value = msn_message_get_attr(msg, "X-MMS-IM-Format")) != NULL) {
		format = msn_parse_format(value);

		body = g_strdup_printf("%s%s", format, body);
	}

	if (swboard->chat != NULL)
		serv_got_chat_in(gc, gaim_chat_get_id(GAIM_CHAT(swboard->chat)),
						 servconn->msg_passport, flags, body, time(NULL));
	else
		serv_got_im(gc, servconn->msg_passport, body, flags, time(NULL), -1);

	g_free(body);

	return TRUE;
}

static gboolean
control_msg(MsnServConn *servconn, MsnMessage *msg)
{
	GaimConnection *gc = servconn->session->account->gc;
	MsnSwitchBoard *swboard = servconn->data;
	const char *value;

	if (swboard->chat == NULL &&
		(value = msn_message_get_attr(msg, "TypingUser")) != NULL) {

		serv_got_typing(gc, servconn->msg_passport, MSN_TYPING_RECV_TIMEOUT,
						GAIM_TYPING);
	}

	return TRUE;
}

static gboolean
clientcaps_msg(MsnServConn *servconn, MsnMessage *msg)
{
	MsnSession *session = servconn->session;
	MsnSwitchBoard *swboard = servconn->data;
	MsnUser *user;
	GHashTable *clientcaps;
	const char *value;

	user = msn_user_new(session, servconn->msg_passport, NULL);

	clientcaps = msn_message_get_hashtable_from_body(msg);

	if (swboard->chat == NULL) {
		if ((value = g_hash_table_lookup(clientcaps, "Buddy-Icons")) != NULL)
			msn_buddy_icon_invite(swboard);
	}

	return TRUE;
}

/**************************************************************************
 * Connect stuff
 **************************************************************************/
static gboolean
connect_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnServConn *servconn = data;
	GaimAccount *account = servconn->session->account;
	MsnSwitchBoard *swboard = servconn->data;
	char outparams[MSN_BUF_LEN];

	if (servconn->fd != source)
		servconn->fd = source;

	swboard->in_use = TRUE;

	gaim_debug(GAIM_DEBUG_INFO, "msn", "Connecting to switchboard...\n");

	if (msn_switchboard_is_invited(swboard)) {
		g_snprintf(outparams, sizeof(outparams), "%s %s %s",
				   gaim_account_get_username(account),
				   swboard->auth_key, swboard->session_id);

		if (!msn_switchboard_send_command(swboard, "ANS", outparams)) {
			msn_switchboard_destroy(swboard);

			return FALSE;
		}
	}
	else {
		g_snprintf(outparams, sizeof(outparams), "%s %s",
				   gaim_account_get_username(account), swboard->auth_key);

		if (!msn_switchboard_send_command(swboard, "USR", outparams)) {
			msn_switchboard_destroy(swboard);

			return FALSE;
		}
	}

	return TRUE;
}

static void
failed_read_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnServConn *servconn = data;

	msn_switchboard_destroy(servconn->data);
}

MsnSwitchBoard *
msn_switchboard_new(MsnSession *session)
{
	MsnSwitchBoard *swboard;
	MsnServConn *servconn;

	g_return_val_if_fail(session != NULL, NULL);

	swboard = g_new0(MsnSwitchBoard, 1);

	swboard->servconn = servconn = msn_servconn_new(session);
	msn_servconn_set_connect_cb(servconn, connect_cb);
	msn_servconn_set_failed_read_cb(servconn, failed_read_cb);

	servconn->data = swboard;

	session->switches = g_list_append(session->switches, swboard);

	if (switchboard_commands == NULL) {
		/* Register the command callbacks. */
		msn_servconn_register_command(servconn, "ACK",      blank_cmd);
		msn_servconn_register_command(servconn, "ANS",      ans_cmd);
		msn_servconn_register_command(servconn, "BYE",      bye_cmd);
		msn_servconn_register_command(servconn, "CAL",      blank_cmd);
		msn_servconn_register_command(servconn, "IRO",      iro_cmd);
		msn_servconn_register_command(servconn, "JOI",      joi_cmd);
		msn_servconn_register_command(servconn, "MSG",      msg_cmd);
		msn_servconn_register_command(servconn, "NAK",      nak_cmd);
		msn_servconn_register_command(servconn, "NLN",      blank_cmd);
		msn_servconn_register_command(servconn, "OUT",      out_cmd);
		msn_servconn_register_command(servconn, "USR",      usr_cmd);
		msn_servconn_register_command(servconn, "_unknown_",unknown_cmd);

		/* Register the message type callbacks. */
		msn_servconn_register_msg_type(servconn, "text/plain",plain_msg);
		msn_servconn_register_msg_type(servconn, "text/x-msmsgscontrol",
									  control_msg);
		msn_servconn_register_msg_type(servconn, "text/x-clientcaps",
									  clientcaps_msg);
		msn_servconn_register_msg_type(servconn, "text/x-clientinfo",
									  clientcaps_msg);
		msn_servconn_register_msg_type(servconn, "application/x-buddyicon",
									   msn_buddy_icon_msg);

		/* Save these for future use. */
		switchboard_commands  = servconn->commands;
		switchboard_msg_types = servconn->msg_types;
	}
	else {
		g_hash_table_destroy(servconn->commands);
		g_hash_table_destroy(servconn->msg_types);

		servconn->commands  = switchboard_commands;
		servconn->msg_types = switchboard_msg_types;
	}

	return swboard;
}

void
msn_switchboard_destroy(MsnSwitchBoard *swboard)
{
	MsnSession *session;

	g_return_if_fail(swboard != NULL);

	session = swboard->servconn->session;

	if (swboard->servconn->connected)
		msn_switchboard_disconnect(swboard);

	if (swboard->user != NULL)
		msn_user_unref(swboard->user);

	if (swboard->auth_key != NULL)
		g_free(swboard->auth_key);

	if (swboard->session_id != NULL)
		g_free(swboard->session_id);

	session->switches = g_list_remove(session->switches, swboard);

	msn_servconn_destroy(swboard->servconn);

	g_free(swboard);
}

void
msn_switchboard_set_user(MsnSwitchBoard *swboard, MsnUser *user)
{
	g_return_if_fail(swboard != NULL);

	swboard->user = user;

	msn_user_ref(user);
}

MsnUser *
msn_switchboard_get_user(const MsnSwitchBoard *swboard)
{
	g_return_val_if_fail(swboard != NULL, NULL);

	return swboard->user;
}

void
msn_switchboard_set_auth_key(MsnSwitchBoard *swboard, const char *key)
{
	g_return_if_fail(swboard != NULL);
	g_return_if_fail(key != NULL);

	swboard->auth_key = g_strdup(key);
}

const char *
msn_switchboard_get_auth_key(const MsnSwitchBoard *swboard)
{
	g_return_val_if_fail(swboard != NULL, NULL);

	return swboard->auth_key;
}

void
msn_switchboard_set_session_id(MsnSwitchBoard *swboard, const char *id)
{
	g_return_if_fail(swboard != NULL);
	g_return_if_fail(id != NULL);

	if (swboard->session_id != NULL)
		g_free(swboard->session_id);

	swboard->session_id = g_strdup(id);
}

const char *
msn_switchboard_get_session_id(const MsnSwitchBoard *swboard)
{
	g_return_val_if_fail(swboard != NULL, NULL);

	return swboard->session_id;
}

void
msn_switchboard_set_invited(MsnSwitchBoard *swboard, gboolean invited)
{
	g_return_if_fail(swboard != NULL);

	swboard->invited = invited;
}

gboolean
msn_switchboard_is_invited(const MsnSwitchBoard *swboard)
{
	g_return_val_if_fail(swboard != NULL, FALSE);

	return swboard->invited;
}

gboolean
msn_switchboard_connect(MsnSwitchBoard *swboard, const char *server, int port)
{
	g_return_val_if_fail(swboard != NULL, FALSE);

	msn_servconn_set_server(swboard->servconn, server, port);

	if (msn_servconn_connect(swboard->servconn))
		swboard->in_use = TRUE;

	return swboard->in_use;
}

void
msn_switchboard_disconnect(MsnSwitchBoard *swboard)
{
	g_return_if_fail(swboard != NULL);
	g_return_if_fail(swboard->servconn->connected);

	msn_servconn_disconnect(swboard->servconn);

	swboard->in_use = FALSE;
}

gboolean
msn_switchboard_send_msg(MsnSwitchBoard *swboard, MsnMessage *msg)
{
	char *buf;
	int ret;

	g_return_val_if_fail(swboard != NULL, FALSE);
	g_return_val_if_fail(msg != NULL, FALSE);

	msn_message_set_transaction_id(msg, ++swboard->trId);
	buf = msn_message_build_string(msg);

	if (swboard->servconn->txqueue != NULL || !swboard->in_use) {
		gaim_debug(GAIM_DEBUG_INFO, "msn", "Appending message to queue.\n");

		swboard->servconn->txqueue =
			g_slist_append(swboard->servconn->txqueue, buf);

		return TRUE;
	}

	ret = msn_servconn_write(swboard->servconn, buf, strlen(buf));

	g_free(buf);

	return (ret > 0);
}

gboolean
msn_switchboard_send_command(MsnSwitchBoard *swboard, const char *command,
							 const char *params)
{
	char buf[MSN_BUF_LEN];

	g_return_val_if_fail(swboard != NULL, FALSE);
	g_return_val_if_fail(command != NULL, FALSE);

	if (params == NULL)
		g_snprintf(buf, sizeof(buf), "%s %u\r\n", command,
				   ++swboard->trId);
	else
		g_snprintf(buf, sizeof(buf), "%s %u %s\r\n",
				   command, ++swboard->trId, params);

	return (msn_servconn_write(swboard->servconn, buf, strlen(buf)) > 0);
}
