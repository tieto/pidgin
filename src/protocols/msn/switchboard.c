/**
 * @file switchboard.c MSN switchboard functions
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
 */
#include "msn.h"
#include "prefs.h"
#include "switchboard.h"
#include "notification.h"
#include "utils.h"

#include "error.h"

static MsnTable *cbs_table;

/**************************************************************************
 * Utility functions
 **************************************************************************/
static void
send_clientcaps(MsnSwitchBoard *swboard)
{
	MsnMessage *msg;

	msg = msn_message_new();
	msn_message_set_content_type(msg, "text/x-clientcaps");
	msn_message_set_flag(msg, 'U');
	msn_message_set_bin_data(msg, MSN_CLIENTINFO, strlen(MSN_CLIENTINFO));

	msn_switchboard_send_msg(swboard, msg);

	msn_message_destroy(msg);
}

void
msn_switchboard_add_user(MsnSwitchBoard *swboard, const char *user)
{
	MsnCmdProc *cmdproc;
	GaimAccount *account;

	g_return_if_fail(swboard != NULL);

	cmdproc = swboard->servconn->cmdproc;
	account = swboard->servconn->session->account;

	swboard->users = g_list_prepend(swboard->users, g_strdup(user));
	swboard->current_users++;

	/* gaim_debug_info("msn", "user=[%s], total=%d\n", user,
	 * swboard->current_users); */

	if ((swboard->conv != NULL) && (gaim_conversation_get_type(swboard->conv) == GAIM_CONV_CHAT))
	{
		gaim_conv_chat_add_user(GAIM_CONV_CHAT(swboard->conv), user, NULL, GAIM_CBFLAGS_NONE, TRUE);
	}
	else if (swboard->current_users > 1 || swboard->total_users > 1)
	{
		if (swboard->conv == NULL ||
			gaim_conversation_get_type(swboard->conv) != GAIM_CONV_CHAT)
		{
			GList *l;

			/* gaim_debug_info("msn", "[chat] Switching to chat.\n"); */

			if (swboard->conv != NULL)
				gaim_conversation_destroy(swboard->conv);

			cmdproc->session->conv_seq++;
			swboard->chat_id = cmdproc->session->conv_seq;

			swboard->conv = serv_got_joined_chat(account->gc,
												 swboard->chat_id,
												 "MSN Chat");

			for (l = swboard->users; l != NULL; l = l->next)
			{
				const char *tmp_user;

				tmp_user = l->data;

				/* gaim_debug_info("msn", "[chat] Adding [%s].\n",
				 * tmp_user); */

				gaim_conv_chat_add_user(GAIM_CONV_CHAT(swboard->conv),
										tmp_user, NULL, GAIM_CBFLAGS_NONE, TRUE);
			}

			/* gaim_debug_info("msn", "[chat] We add ourselves.\n"); */

			gaim_conv_chat_add_user(GAIM_CONV_CHAT(swboard->conv),
									gaim_account_get_username(account),
									NULL, GAIM_CBFLAGS_NONE, TRUE);

			g_free(swboard->im_user);
			swboard->im_user = NULL;
		}
	}
	else if (swboard->conv == NULL)
	{
		swboard->conv = gaim_find_conversation_with_account(user, account);
	}
	else
	{
		gaim_debug_warning("msn", "This should not happen!"
						   "(msn_switchboard_add_user)\n");
	}
}

/**************************************************************************
 * Switchboard Commands
 **************************************************************************/
static void
ans_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSwitchBoard *swboard;

	swboard = cmdproc->servconn->data;
	swboard->ready = TRUE;
}

static void
bye_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	GaimAccount *account;
	MsnSwitchBoard *swboard;
	const char *user;

	account = cmdproc->session->account;
	swboard = cmdproc->servconn->data;
	user = cmd->params[0];

	if (swboard->hidden)
		return;

	if (swboard->current_users > 1)
	{
		gaim_conv_chat_remove_user(GAIM_CONV_CHAT(swboard->conv), user, NULL);
	}
	else
	{
		char *username;
		GaimConversation *conv;
		GaimBuddy *b;
		char *str = NULL;

		if ((b = gaim_find_buddy(account, user)) != NULL)
			username = gaim_escape_html(gaim_buddy_get_alias(b));
		else
			username = gaim_escape_html(user);

		if (cmd->param_count == 2 && atoi(cmd->params[1]) == 1)
		{
			if (gaim_prefs_get_bool("/plugins/prpl/msn/conv_timeout_notice"))
			{
				str = g_strdup_printf(_("The conversation has become "
										"inactive and timed out."));
			}
		}
		else
		{
			if (gaim_prefs_get_bool("/plugins/prpl/msn/conv_close_notice"))
			{
				str = g_strdup_printf(_("%s has closed the conversation "
										"window."), username);
			}
		}

		if (str != NULL &&
			(conv = gaim_find_conversation_with_account(user, account)) != NULL)
		{
			gaim_conversation_write(conv, NULL, str, GAIM_MESSAGE_SYSTEM,
									time(NULL));

			g_free(str);
		}

		msn_switchboard_disconnect(swboard);
		g_free(username);
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

	swboard->total_users = atoi(cmd->params[2]);

	msn_switchboard_add_user(swboard, cmd->params[3]);
}

static void
joi_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	GaimAccount *account;
	GaimConnection *gc;
	MsnSwitchBoard *swboard;
	const char *passport;

	passport = cmd->params[0];

	session = cmdproc->session;
	account = session->account;
	gc = account->gc;
	swboard = cmdproc->servconn->data;

	msn_switchboard_add_user(swboard, passport);

	swboard->user_joined = TRUE;

	/* msn_cmdproc_process_queue(cmdproc); */

	msn_switchboard_process_queue(swboard);

	send_clientcaps(swboard);
}

static void
msg_cmd_post(MsnCmdProc *cmdproc, MsnCommand *cmd, char *payload, size_t len)
{
	MsnMessage *msg;

	msg = msn_message_new_from_cmd(cmdproc->session, cmd);

	msn_message_parse_payload(msg, payload, len);
	/* msn_message_show_readable(msg, "SB RECV", FALSE); */

	msg->remote_user = g_strdup(cmd->params[0]);
	msn_cmdproc_process_msg(cmdproc, msg);

	msn_message_destroy(msg);
}

static void
msg_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	cmdproc->servconn->payload_len = atoi(cmd->params[2]);
	cmdproc->last_cmd->payload_cb = msg_cmd_post;
}

static void
nak_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	/*
	gaim_notify_error(cmdproc->session->account->gc, NULL,
					  _("A MSN message may not have been received."), NULL);
	 */
}

static void
ack_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
#if 0
	MsnMessage *msg;
	const char *body;

	msg = msn_message_new();
	msn_message_parse_payload(msg, cmd->trans->payload, cmd->trans->payload_len);

	body = msn_message_get_body(msg);

	gaim_debug_info("msn", "ACK: {%s}\n", body);

	msn_message_destroy(msg);
#endif
}

static void
out_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	GaimConnection *gc;
	MsnSwitchBoard *swboard;

	gc = cmdproc->session->account->gc;
	swboard = cmdproc->servconn->data;

	if (swboard->current_users > 1)
		serv_got_chat_left(gc, swboard->chat_id);

	msn_switchboard_disconnect(swboard);
}

static void
usr_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSwitchBoard *swboard;

	swboard = cmdproc->servconn->data;

#if 0
	GList *l;

	for (l = swboard->users; l != NULL; l = l->next)
	{
		const char *user;
		user = l->data;

		msn_cmdproc_send(cmdproc, "CAL", "%s", user);
	}
#endif

	swboard->ready = TRUE;
	msn_cmdproc_process_queue(cmdproc);
}

/**************************************************************************
 * Message Types
 **************************************************************************/
static void
plain_msg(MsnCmdProc *cmdproc, MsnMessage *msg)
{
	GaimConnection *gc;
	MsnSwitchBoard *swboard;
	const char *body;
	char *body_str;
	char *body_enc;
	char *body_final;
	int body_len;
	const char *passport;
	const char *value;

	gc = cmdproc->session->account->gc;
	swboard = cmdproc->servconn->data;

	body = msn_message_get_bin_data(msg, &body_len);
	body_str = g_strndup(body, body_len);
	body_enc = gaim_escape_html(body_str);
	g_free(body_str);

	passport = msg->remote_user;

	if (!strcmp(passport, "messenger@microsoft.com") &&
		strstr(body, "immediate security update"))
	{
		return;
	}

#if 0
	if ((value = msn_message_get_attr(msg, "User-Agent")) != NULL)
	{
		gaim_debug_misc("msn", "User-Agent = '%s'\n", value);
	}
#endif

	if ((value = msn_message_get_attr(msg, "X-MMS-IM-Format")) != NULL)
	{
		char *pre_format, *post_format;

		msn_parse_format(value, &pre_format, &post_format);

		body_final = g_strdup_printf("%s%s%s", pre_format, body_enc, post_format);

		g_free(pre_format);
		g_free(post_format);
		g_free(body_enc);
	}
	else
	{
		body_final = body_enc;
	}

	if (swboard->current_users > 1)
	{
		serv_got_chat_in(gc, swboard->chat_id, passport, 0, body_final,
						 time(NULL));
	}
	else
		serv_got_im(gc, passport, body_final, 0, time(NULL));

	g_free(body_final);
}

static void
control_msg(MsnCmdProc *cmdproc, MsnMessage *msg)
{
	GaimConnection *gc;
	MsnSwitchBoard *swboard;
	const char *value;
	char *passport;

	gc = cmdproc->session->account->gc;
	swboard = cmdproc->servconn->data;
	passport = msg->remote_user;

	if (swboard->current_users == 1 &&
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

	char *passport = msg->sender;

	session = cmdproc->session;
	swboard = cmdproc->servconn->swboard;

	clientcaps = msn_message_get_hashtable_from_body(msg);
#endif
}

void
msn_switchboard_send_msg(MsnSwitchBoard *swboard, MsnMessage *msg)
{
	MsnCmdProc *cmdproc;
	MsnTransaction *trans;
	char *payload;
	gsize payload_len;

	g_return_if_fail(swboard != NULL);
	g_return_if_fail(msg     != NULL);

	cmdproc = swboard->servconn->cmdproc;

	payload = msn_message_gen_payload(msg, &payload_len);

	/* msn_message_show_readable(msg, "SB SEND", FALSE); */

	trans = msn_transaction_new("MSG", "%c %d", msn_message_get_flag(msg),
								payload_len);

	if (msg->ack_cb != NULL)
		msn_transaction_add_cb(trans, "ACK", msg->ack_cb, msg->ack_data);

	trans->payload = payload;
	trans->payload_len = payload_len;

	msg->trans = trans;

	msn_cmdproc_send_trans(cmdproc, trans);
}

void
msn_switchboard_queue_msg(MsnSwitchBoard *swboard, MsnMessage *msg)
{
	g_return_if_fail(swboard != NULL);
	g_return_if_fail(msg     != NULL);

	gaim_debug_info("msn", "Appending message to queue.\n");

	g_queue_push_tail(swboard->im_queue, msg);

	msn_message_ref(msg);
}

void
msn_switchboard_process_queue(MsnSwitchBoard *swboard)
{
	MsnMessage *msg;

	g_return_if_fail(swboard != NULL);

	gaim_debug_info("msn", "Processing queue\n");

	while ((msg = g_queue_pop_head(swboard->im_queue)) != NULL)
	{
		gaim_debug_info("msn", "Sending message\n");
		msn_switchboard_send_msg(swboard, msg);
		msn_message_unref(msg);
	}
}

/**************************************************************************
 * Connect stuff
 **************************************************************************/
static void
connect_cb(MsnServConn *servconn)
{
	MsnSwitchBoard *swboard;
	MsnCmdProc *cmdproc;
	GaimAccount *account;

	cmdproc = servconn->cmdproc;
	g_return_if_fail(cmdproc != NULL);

	cmdproc->ready = TRUE;

	account = servconn->session->account;
	swboard = servconn->data;
	g_return_if_fail(swboard != NULL);

	swboard->user_joined = TRUE;

	if (msn_switchboard_is_invited(swboard))
	{
		msn_cmdproc_send(cmdproc, "ANS", "%s %s %s",
						 gaim_account_get_username(account),
						 swboard->auth_key, swboard->session_id);
	}
	else
	{
		msn_cmdproc_send(cmdproc, "USR", "%s %s",
						 gaim_account_get_username(account),
						 swboard->auth_key);
	}
}

static void
disconnect_cb(MsnServConn *servconn)
{
	MsnSwitchBoard *swboard;

	swboard = servconn->data;
	g_return_if_fail(swboard != NULL);

	msn_switchboard_destroy(swboard);
}

void
msn_switchboard_init(void)
{
	cbs_table = msn_table_new();

	msn_table_add_cmd(cbs_table, "ANS", "ANS", ans_cmd);
	msn_table_add_cmd(cbs_table, "ANS", "IRO", iro_cmd);

	msn_table_add_cmd(cbs_table, "MSG", "ACK", ack_cmd);
	msn_table_add_cmd(cbs_table, "MSG", "NAK", nak_cmd);

	msn_table_add_cmd(cbs_table, "USR", "USR", usr_cmd);

	msn_table_add_cmd(cbs_table, NULL, "MSG", msg_cmd);
	msn_table_add_cmd(cbs_table, NULL, "JOI", joi_cmd);
	msn_table_add_cmd(cbs_table, NULL, "BYE", bye_cmd);
	msn_table_add_cmd(cbs_table, NULL, "OUT", out_cmd);

#if 0
	/* They might skip the history */
	msn_table_add_cmd(cbs_table, NULL, "ACK", NULL);
#endif

	msn_table_add_error(cbs_table, "MSG", NULL);

	/* Register the message type callbacks. */
	msn_table_add_msg_type(cbs_table, "text/plain",
						   plain_msg);
	msn_table_add_msg_type(cbs_table, "text/x-msmsgscontrol",
						   control_msg);
	msn_table_add_msg_type(cbs_table, "text/x-clientcaps",
						   clientcaps_msg);
	msn_table_add_msg_type(cbs_table, "text/x-clientinfo",
						   clientcaps_msg);
	msn_table_add_msg_type(cbs_table, "application/x-msnmsgrp2p",
						   msn_p2p_msg);
	msn_table_add_msg_type(cbs_table, "text/x-mms-emoticon",
						   msn_emoticon_msg);
#if 0
	msn_table_add_msg_type(cbs_table, "text/x-msmmsginvite",
						   msn_invite_msg);
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

	swboard->session = session;
	swboard->servconn = servconn = msn_servconn_new(session, MSN_SERVER_SB);
	cmdproc = servconn->cmdproc;

	swboard->im_queue = g_queue_new();

	if (session->http_method)
		servconn->http_data->server_type = "SB";
	else
		msn_servconn_set_connect_cb(servconn, connect_cb);

	msn_servconn_set_disconnect_cb(servconn, disconnect_cb);

	servconn->data = swboard;

	session->switches = g_list_append(session->switches, swboard);

	cmdproc->cbs_table = cbs_table;

	return swboard;
}

void
msn_switchboard_destroy(MsnSwitchBoard *swboard)
{
	MsnSession *session;
	MsnMessage *msg;
	GList *l;

	g_return_if_fail(swboard != NULL);

	if (swboard->destroying)
		return;

	swboard->destroying = TRUE;

	if (swboard->im_user != NULL)
		g_free(swboard->im_user);

	if (swboard->auth_key != NULL)
		g_free(swboard->auth_key);

	if (swboard->session_id != NULL)
		g_free(swboard->session_id);

	for (l = swboard->users; l != NULL; l = l->next)
		g_free(l->data);

	session = swboard->session;
	session->switches = g_list_remove(session->switches, swboard);

	if (swboard->servconn != NULL)
		msn_servconn_destroy(swboard->servconn);

	while ((msg = g_queue_pop_head(swboard->im_queue)) != NULL)
		msn_message_destroy(msg);

	g_queue_free(swboard->im_queue);

	g_free(swboard);
}

#if 0
void
msn_switchboard_set_user(MsnSwitchBoard *swboard, const char *user)
{
	g_return_if_fail(swboard != NULL);

	if (swboard->user != NULL)
		g_free(swboard->user);

	swboard->user = g_strdup(user);
}

const char *
msn_switchboard_get_user(MsnSwitchBoard *swboard)
{
	g_return_val_if_fail(swboard != NULL, NULL);

	return swboard->user;
}
#endif

#if 0
static void
got_cal(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSwitchBoard *swboard;
	const char *user;

	swboard = cmdproc->servconn->data;

	user = cmd->params[0];

	msn_switchboard_add_user(swboard, user);
}
#endif

void
msn_switchboard_request_add_user(MsnSwitchBoard *swboard, const char *user)
{
	MsnTransaction *trans;
	MsnCmdProc *cmdproc;

	g_return_if_fail(swboard != NULL);

	cmdproc = swboard->servconn->cmdproc;

	trans = msn_transaction_new("CAL", "%s", user);
	/* msn_transaction_add_cb(trans, "CAL", got_cal, NULL); */

	if (swboard->ready)
		msn_cmdproc_send_trans(cmdproc, trans);
	else
		msn_cmdproc_queue_trans(cmdproc, trans);
}

void
msn_switchboard_set_auth_key(MsnSwitchBoard *swboard, const char *key)
{
	g_return_if_fail(swboard != NULL);
	g_return_if_fail(key != NULL);

	swboard->auth_key = g_strdup(key);
}

const char *
msn_switchboard_get_auth_key(MsnSwitchBoard *swboard)
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
msn_switchboard_get_session_id(MsnSwitchBoard *swboard)
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
msn_switchboard_is_invited(MsnSwitchBoard *swboard)
{
	g_return_val_if_fail(swboard != NULL, FALSE);

	return swboard->invited;
}

gboolean
msn_switchboard_connect(MsnSwitchBoard *swboard, const char *host, int port)
{
	g_return_val_if_fail(swboard != NULL, FALSE);

	return msn_servconn_connect(swboard->servconn, host, port);
}

void
msn_switchboard_disconnect(MsnSwitchBoard *swboard)
{
	g_return_if_fail(swboard != NULL);

	msn_servconn_disconnect(swboard->servconn);
}

static void
got_swboard(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSwitchBoard *swboard;
	char *host;
	int port;
	swboard = cmd->trans->data;

	msn_switchboard_set_auth_key(swboard, cmd->params[4]);

	msn_parse_socket(cmd->params[2], &host, &port);

	if (swboard->session->http_method)
	{
		GaimAccount *account;
		MsnSession *session;
		MsnServConn *servconn;

		port = 80;

		session = swboard->session;
		servconn = swboard->servconn;
		account = session->account;

		swboard->user_joined = TRUE;

		servconn->http_data->gateway_host = g_strdup(host);

#if 0
		servconn->connected = TRUE;
		servconn->cmdproc->ready = TRUE;
#endif

		if (msn_switchboard_is_invited(swboard))
		{
			msn_cmdproc_send(servconn->cmdproc, "ANS", "%s %s %s",
							 gaim_account_get_username(account),
							 swboard->auth_key, swboard->session_id);
		}
		else
		{
			msn_cmdproc_send(servconn->cmdproc, "USR", "%s %s",
							 gaim_account_get_username(account),
							 swboard->auth_key);
		}
	}

	msn_switchboard_connect(swboard, host, port);
}

void
msn_switchboard_request(MsnSwitchBoard *swboard)
{
	MsnCmdProc *cmdproc;
	MsnTransaction *trans;

	g_return_if_fail(swboard != NULL);

	cmdproc = swboard->session->notification->cmdproc;

	trans = msn_transaction_new("XFR", "%s", "SB");
	msn_transaction_add_cb(trans, "XFR", got_swboard, swboard);

	msn_cmdproc_send_trans(cmdproc, trans);
}
