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
#include "msnslp.h"
#include "prefs.h"
#include "switchboard.h"
#include "utils.h"

static MsnTable *cbs_table = NULL;

/**************************************************************************
 * Utility functions
 **************************************************************************/
static void
send_clientcaps(MsnSwitchBoard *swboard)
{
	MsnMessage *msg;

	msg = msn_message_new();
	msn_message_set_content_type(msg, "text/x-clientcaps");
	msn_message_set_charset(msg, NULL);
	msn_message_set_attr(msg, "User-Agent", NULL);
	msn_message_set_body(msg, MSN_CLIENTINFO);

	msn_switchboard_send_msg(swboard, msg);

	msn_message_destroy(msg);
}

/**************************************************************************
 * Switchboard Commands
 **************************************************************************/
static void
ans_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSwitchBoard *swboard;
	MsnSession *session;

	swboard = cmdproc->servconn->data;
	session = cmdproc->session;

	/* send_clientcaps(swboard); */

	if (0 && session->protocol_ver >= 9)
	{
		MsnUser *local_user, *remote_user;

		remote_user = msn_user_new(session,
				msn_user_get_passport(msn_switchboard_get_user(swboard)),
				NULL);
		local_user = msn_user_new(session,
								  gaim_account_get_username(session->account),
								  NULL);

		if (msn_user_get_object(remote_user) != NULL)
		{
			swboard->slp_session = msn_slp_session_new(swboard, TRUE);

			msn_slp_session_request_user_display(swboard->slp_session,
					local_user, remote_user,
					msn_user_get_object(remote_user));
		}
	}

	swboard->joined = TRUE;
}

static void
bye_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	GaimAccount *account;
	MsnSwitchBoard *swboard;
	const char *user = cmd->params[0];

	account = cmdproc->session->account;
	swboard = cmdproc->servconn->data;

	if (swboard->hidden)
		return;

	if (swboard->chat != NULL)
		gaim_conv_chat_remove_user(GAIM_CONV_CHAT(swboard->chat), user, NULL);
	else
	{
		const char *username;
		GaimConversation *conv;
		GaimBuddy *b;
		char buf[MSN_BUF_LEN];

		if ((b = gaim_find_buddy(account, user)) != NULL)
			username = gaim_get_buddy_alias(b);
		else
			username = user;

		*buf = '\0';

		if (cmd->param_count == 2 && atoi(cmd->params[1]) == 1)
		{
			if (gaim_prefs_get_bool("/plugins/prpl/msn/conv_timeout_notice"))
			{
				g_snprintf(buf, sizeof(buf),
						   _("The conversation has become inactive "
							 "and timed out."));
			}
		}
		else
		{
			if (gaim_prefs_get_bool("/plugins/prpl/msn/conv_close_notice"))
			{
				g_snprintf(buf, sizeof(buf),
						   _("%s has closed the conversation window."),
						   username);
			}
		}

		if (*buf != '\0' &&
			(conv = gaim_find_conversation_with_account(user, account)) != NULL)
		{
			gaim_conversation_write(conv, NULL, buf, GAIM_MESSAGE_SYSTEM,
									time(NULL));
		}

		msn_switchboard_destroy(swboard);
	}
}

static void
iro_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	GaimAccount *account;
	GaimConnection *gc;
	MsnSwitchBoard *swboard;

	account = cmdproc->session->account;
	gc = account->gc;
	swboard = cmdproc->servconn->data;

	swboard->total_users = atoi(cmd->params[2]) + 1;

	if (swboard->total_users > 2)
	{
		if (swboard->chat == NULL)
		{
			GaimConversation *conv;

			conv = gaim_find_conversation_with_account(
				msn_user_get_passport(swboard->user), account);

			cmdproc->session->last_chat_id++;
			swboard->chat_id = cmdproc->session->last_chat_id;
			swboard->chat = serv_got_joined_chat(gc, swboard->chat_id,
												 "MSN Chat");

			gaim_conv_chat_add_user(GAIM_CONV_CHAT(swboard->chat),
							   gaim_account_get_username(account), NULL);

			gaim_conversation_destroy(conv);
		}

		gaim_conv_chat_add_user(GAIM_CONV_CHAT(swboard->chat), cmd->params[3], NULL);
	}
}

static void
joi_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	GaimAccount *account;
	GaimConnection *gc;
	MsnSwitchBoard *swboard;
	const char *passport;

	account = cmdproc->session->account;
	gc = account->gc;
	swboard = cmdproc->servconn->data;
	passport = cmd->params[0];

	if (swboard->total_users == 2 && swboard->chat == NULL)
	{
		GaimConversation *conv;

		conv = gaim_find_conversation_with_account(
			msn_user_get_passport(swboard->user), account);

		cmdproc->session->last_chat_id++;
		swboard->chat_id = cmdproc->session->last_chat_id;
		swboard->chat = serv_got_joined_chat(gc, swboard->chat_id, "MSN Chat");
		gaim_conv_chat_add_user(GAIM_CONV_CHAT(swboard->chat),
								msn_user_get_passport(swboard->user), NULL);
		gaim_conv_chat_add_user(GAIM_CONV_CHAT(swboard->chat),
								gaim_account_get_username(account), NULL);

		msn_user_unref(swboard->user);

		gaim_conversation_destroy(conv);
	}

	if (swboard->chat != NULL)
		gaim_conv_chat_add_user(GAIM_CONV_CHAT(swboard->chat), passport, NULL);

	swboard->total_users++;

	swboard->joined = TRUE;

	msn_cmdproc_process_queue(cmdproc);

	send_clientcaps(swboard);
}

static void
msg_cmd_post(MsnCmdProc *cmdproc, char *payload, size_t len)
{
	MsnMessage *msg = msn_message_new();

	msn_message_parse_payload(msg, payload, len);

	msg->passport = cmdproc->temp;
	msn_cmdproc_process_msg(cmdproc, msg);
	g_free(cmdproc->temp);
	cmdproc->temp = NULL;

	msn_message_destroy(msg);
}

static void
msg_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	gaim_debug(GAIM_DEBUG_INFO, "msn", "Found message. Parsing.\n");

	cmdproc->payload_cb  = msg_cmd_post;
	cmdproc->servconn->payload_len = atoi(cmd->params[2]);
	cmdproc->temp = g_strdup(cmd->params[0]);
}

static void
nak_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	/*
	 * TODO: Investigate this, as it seems to occur frequently with
	 *       the old prpl.
	 *
	 * shx: This will only happend in the new protocol when we ask for it
	 *      in the message flags.
	 */
	gaim_notify_error(cmdproc->servconn->session->account->gc, NULL,
					  _("An MSN message may not have been received."), NULL);
}

static void
out_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	GaimConnection *gc;
	MsnSwitchBoard *swboard;

	gc = cmdproc->servconn->session->account->gc;
	swboard = cmdproc->servconn->data;

	if (swboard->chat != NULL)
	{
		serv_got_chat_left(gc,
			gaim_conv_chat_get_id(GAIM_CONV_CHAT(swboard->chat)));
	}

	msn_switchboard_destroy(swboard);
}

static void
usr_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSwitchBoard *swboard;

	swboard = cmdproc->servconn->data;

	msn_cmdproc_send(swboard->cmdproc, "CAL", "%s",
					 msn_user_get_passport(swboard->user));
}

/**************************************************************************
 * Message Types
 **************************************************************************/
static void
plain_msg(MsnCmdProc *cmdproc, MsnMessage *msg)
{
	GaimConnection *gc;
	MsnSwitchBoard *swboard;
	char *body;
	char *passport;
	const char *value;

	gc = cmdproc->session->account->gc;
	swboard = cmdproc->servconn->data;
	body = gaim_escape_html(msn_message_get_body(msg));
	passport = msg->passport;

	if (!strcmp(passport, "messenger@microsoft.com") &&
		strstr(body, "immediate security update"))
	{
		g_free(body);

		return;
	}

#if 0
	gaim_debug(GAIM_DEBUG_INFO, "msn", "Checking User-Agent...\n");

	if ((value = msn_message_get_attr(msg, "User-Agent")) != NULL) {
		gaim_debug(GAIM_DEBUG_MISC, "msn", "value = '%s'\n", value);
	}
#endif

	if ((value = msn_message_get_attr(msg, "X-MMS-IM-Format")) != NULL)
	{
		char *pre_format, *post_format;

		msn_parse_format(value, &pre_format, &post_format);

		body = g_strdup_printf("%s%s%s", pre_format, body, post_format);

		g_free(pre_format);
		g_free(post_format);
	}

	if (swboard->chat != NULL)
	{
		serv_got_chat_in(gc,
						 gaim_conv_chat_get_id(GAIM_CONV_CHAT(swboard->chat)),
						 passport, 0, body, time(NULL));
	}
	else
		serv_got_im(gc, passport, body, 0, time(NULL));

	g_free(body);
}

static void
control_msg(MsnCmdProc *cmdproc, MsnMessage *msg)
{
	GaimConnection *gc;
	MsnSwitchBoard *swboard;
	char *passport;
	const char *value;

	gc = cmdproc->session->account->gc;
	swboard = cmdproc->servconn->data;
	passport = msg->passport;

	if (swboard->chat == NULL &&
		(value = msn_message_get_attr(msg, "TypingUser")) != NULL)
	{
		serv_got_typing(gc, passport, MSN_TYPING_RECV_TIMEOUT,
						GAIM_TYPING);
	}
}

static void
clientcaps_msg(MsnCmdProc *cmdproc, MsnMessage *msg)
{
#if 0
	MsnSession *session;
	MsnSwitchBoard *swboard;
	MsnUser *user;
	GHashTable *clientcaps;
	const char *value;

	session = cmdproc->session;
	swboard = cmdproc->servconn->data;

	user = msn_user_new(session, msg->passport, NULL);

	clientcaps = msn_message_get_hashtable_from_body(msg);
#endif
}

/**************************************************************************
 * Connect stuff
 **************************************************************************/
static gboolean
connect_cb(MsnServConn *servconn)
{
	GaimAccount *account;
	MsnSwitchBoard *swboard;

	account = servconn->session->account;
	swboard = servconn->data;

	swboard->in_use = TRUE;

	gaim_debug_info("msn", "Connecting to switchboard...\n");

	if (msn_switchboard_is_invited(swboard))
	{
		msn_cmdproc_send(swboard->cmdproc, "ANS", "%s %s %s",
						 gaim_account_get_username(account),
						 swboard->auth_key, swboard->session_id);
	}
	else
	{
		msn_cmdproc_send(swboard->cmdproc, "USR", "%s %s",
						 gaim_account_get_username(account),
						 swboard->auth_key);
	}

	if (swboard->cmdproc->error)
		return FALSE;

	return TRUE;
}

static void
disconnect_cb(MsnServConn *servconn)
{
	MsnSwitchBoard *swboard;

	swboard = servconn->data;

	if (!swboard->destroying)
		msn_switchboard_destroy(swboard);
}

void
msn_switchboard_init(void)
{
	cbs_table = msn_table_new();

	/* Register the command callbacks. */
	msn_table_add_cmd(cbs_table, NULL, "ACK", NULL);

	msn_table_add_cmd(cbs_table, "ANS", "ANS", ans_cmd);
	msn_table_add_cmd(cbs_table, "ANS", "IRO", iro_cmd);

	msn_table_add_cmd(cbs_table, "MSG", "NAK", nak_cmd);

	msn_table_add_cmd(cbs_table, "USR", "USR", usr_cmd);

	msn_table_add_cmd(cbs_table, NULL, "MSG", msg_cmd);
	msn_table_add_cmd(cbs_table, NULL, "JOI", joi_cmd);
	msn_table_add_cmd(cbs_table, NULL, "BYE", bye_cmd);
	msn_table_add_cmd(cbs_table, NULL, "OUT", out_cmd);

	/* Register the message type callbacks. */
	msn_table_add_msg_type(cbs_table, "text/plain",
							plain_msg);
	msn_table_add_msg_type(cbs_table, "text/x-msmsgscontrol",
							control_msg);
	msn_table_add_msg_type(cbs_table, "text/x-clientcaps",
							clientcaps_msg);
	msn_table_add_msg_type(cbs_table, "text/x-clientinfo",
							clientcaps_msg);
#if 0
	msn_table_add_msg_type(cbs_table, "application/x-msnmsgrp2p",
									msn_p2p_msg);
#endif
}

void
msn_switchboard_end(void)
{
	msn_table_destroy(cbs_table);
}

MsnSwitchBoard *
msn_switchboard_new(MsnSession *session)
{
	MsnSwitchBoard *swboard;
	MsnServConn *servconn;
	MsnCmdProc *cmdproc;

	g_return_val_if_fail(session != NULL, NULL);

	swboard = g_new0(MsnSwitchBoard, 1);

	swboard->servconn = servconn = msn_servconn_new(session, MSN_SERVER_SB);
	cmdproc = swboard->cmdproc = servconn->cmdproc;

	msn_servconn_set_connect_cb(servconn, connect_cb);
	msn_servconn_set_disconnect_cb(servconn, disconnect_cb);

	if (session->http_method)
		swboard->servconn->http_data->server_type = "SB";

	servconn->data = swboard;

	session->switches = g_list_append(session->switches, swboard);

	cmdproc->cbs_table  = cbs_table;

	return swboard;
}

void
msn_switchboard_destroy(MsnSwitchBoard *swboard)
{
	MsnSession *session;

	g_return_if_fail(swboard != NULL);
	g_return_if_fail(!swboard->destroying);

	swboard->destroying = TRUE;
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
msn_switchboard_connect(MsnSwitchBoard *swboard, const char *host, int port)
{
	g_return_val_if_fail(swboard != NULL, FALSE);

	if (msn_servconn_connect(swboard->servconn, host, port))
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

void
msn_switchboard_send_msg(MsnSwitchBoard *swboard, MsnMessage *msg)
{
	MsnCmdProc *cmdproc;
	MsnTransaction *trans;
	char *payload;
	size_t payload_len;

	g_return_if_fail(swboard != NULL);
	g_return_if_fail(msg     != NULL);

	cmdproc = swboard->servconn->cmdproc;
	payload = msn_message_gen_payload(msg, &payload_len);

	trans = msn_transaction_new("MSG", "%c %d", msn_message_get_flag(msg),
								payload_len);

	trans->payload = payload;
	trans->payload_len = payload_len;

	if (!g_queue_is_empty(cmdproc->txqueue) || !swboard->joined)
		msn_cmdproc_queue_trans(cmdproc, trans);
	else
		msn_cmdproc_send_trans(cmdproc, trans);
}
