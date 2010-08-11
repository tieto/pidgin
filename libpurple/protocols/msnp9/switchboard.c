/**
 * @file switchboard.c MSN switchboard functions
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#include "msn.h"
#include "prefs.h"
#include "switchboard.h"
#include "notification.h"
#include "msn-utils.h"

#include "error.h"

static MsnTable *cbs_table;

static void msg_error_helper(MsnCmdProc *cmdproc, MsnMessage *msg,
							 MsnMsgErrorType error);

/**************************************************************************
 * Main
 **************************************************************************/

MsnSwitchBoard *
msn_switchboard_new(MsnSession *session)
{
	MsnSwitchBoard *swboard;
	MsnServConn *servconn;

	g_return_val_if_fail(session != NULL, NULL);

	swboard = g_new0(MsnSwitchBoard, 1);

	swboard->session = session;
	swboard->servconn = servconn = msn_servconn_new(session, MSN_SERVCONN_SB);
	swboard->cmdproc = servconn->cmdproc;

	swboard->msg_queue = g_queue_new();
	swboard->empty = TRUE;

	swboard->cmdproc->data = swboard;
	swboard->cmdproc->cbs_table = cbs_table;

	session->switches = g_list_append(session->switches, swboard);

	return swboard;
}

void
msn_switchboard_destroy(MsnSwitchBoard *swboard)
{
	MsnSession *session;
	MsnMessage *msg;
	GList *l;

#ifdef MSN_DEBUG_SB
	purple_debug_info("msn", "switchboard_destroy: swboard(%p)\n", swboard);
#endif

	g_return_if_fail(swboard != NULL);

	if (swboard->destroying)
		return;

	swboard->destroying = TRUE;

	/* If it linked us is because its looking for trouble */
	while (swboard->slplinks != NULL)
		msn_slplink_destroy(swboard->slplinks->data);

	/* Destroy the message queue */
	while ((msg = g_queue_pop_head(swboard->msg_queue)) != NULL)
	{
		if (swboard->error != MSN_SB_ERROR_NONE)
		{
			/* The messages could not be sent due to a switchboard error */
			msg_error_helper(swboard->cmdproc, msg,
							 MSN_MSG_ERROR_SB);
		}
		msn_message_unref(msg);
	}

	g_queue_free(swboard->msg_queue);

	/* msg_error_helper will both remove the msg from ack_list and
	   unref it, so we don't need to do either here */
	while ((l = swboard->ack_list) != NULL)
		msg_error_helper(swboard->cmdproc, l->data, MSN_MSG_ERROR_SB);

	g_free(swboard->im_user);
	g_free(swboard->auth_key);
	g_free(swboard->session_id);

	for (l = swboard->users; l != NULL; l = l->next)
		g_free(l->data);

	session = swboard->session;
	session->switches = g_list_remove(session->switches, swboard);

#if 0
	/* This should never happen or we are in trouble. */
	if (swboard->servconn != NULL)
		msn_servconn_destroy(swboard->servconn);
#endif

	swboard->cmdproc->data = NULL;

	msn_servconn_set_disconnect_cb(swboard->servconn, NULL);

	msn_servconn_destroy(swboard->servconn);

	g_free(swboard);
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

int
msn_switchboard_get_chat_id(void)
{
	static int chat_id = 1;

	return chat_id++;
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

/**************************************************************************
 * Utility
 **************************************************************************/

static void
send_clientcaps(MsnSwitchBoard *swboard)
{
	MsnMessage *msg;

	msg = msn_message_new(MSN_MSG_CAPS);
	msn_message_set_content_type(msg, "text/x-clientcaps");
	msn_message_set_flag(msg, 'U');
	msn_message_set_bin_data(msg, MSN_CLIENTINFO, strlen(MSN_CLIENTINFO));

	msn_switchboard_send_msg(swboard, msg, TRUE);

	msn_message_destroy(msg);
}

static void
msn_switchboard_add_user(MsnSwitchBoard *swboard, const char *user)
{
	MsnCmdProc *cmdproc;
	PurpleAccount *account;

	g_return_if_fail(swboard != NULL);

	cmdproc = swboard->cmdproc;
	account = cmdproc->session->account;

	swboard->users = g_list_prepend(swboard->users, g_strdup(user));
	swboard->current_users++;
	swboard->empty = FALSE;

#ifdef MSN_DEBUG_CHAT
	purple_debug_info("msn", "user=[%s], total=%d\n", user,
					swboard->current_users);
#endif

	if (!(swboard->flag & MSN_SB_FLAG_IM) && (swboard->conv != NULL))
	{
		/* This is a helper switchboard. */
		purple_debug_error("msn", "switchboard_add_user: conv != NULL\n");
		return;
	}

	if ((swboard->conv != NULL) &&
		(purple_conversation_get_type(swboard->conv) == PURPLE_CONV_TYPE_CHAT))
	{
		purple_conv_chat_add_user(PURPLE_CONV_CHAT(swboard->conv), user, NULL,
								PURPLE_CBFLAGS_NONE, TRUE);
	}
	else if (swboard->current_users > 1 || swboard->total_users > 1)
	{
		if (swboard->conv == NULL ||
			purple_conversation_get_type(swboard->conv) != PURPLE_CONV_TYPE_CHAT)
		{
			GList *l;

#ifdef MSN_DEBUG_CHAT
			purple_debug_info("msn", "[chat] Switching to chat.\n");
#endif

#if 0
			/* this is bad - it causes msn_switchboard_close to be called on the
			 * switchboard we're in the middle of using :( */
			if (swboard->conv != NULL)
				purple_conversation_destroy(swboard->conv);
#endif

			swboard->chat_id = msn_switchboard_get_chat_id();
			swboard->flag |= MSN_SB_FLAG_IM;
			swboard->conv = serv_got_joined_chat(account->gc,
												 swboard->chat_id,
												 "MSN Chat");

			for (l = swboard->users; l != NULL; l = l->next)
			{
				const char *tmp_user;

				tmp_user = l->data;

#ifdef MSN_DEBUG_CHAT
				purple_debug_info("msn", "[chat] Adding [%s].\n", tmp_user);
#endif

				purple_conv_chat_add_user(PURPLE_CONV_CHAT(swboard->conv),
										tmp_user, NULL, PURPLE_CBFLAGS_NONE, TRUE);
			}

#ifdef MSN_DEBUG_CHAT
			purple_debug_info("msn", "[chat] We add ourselves.\n");
#endif

			purple_conv_chat_add_user(PURPLE_CONV_CHAT(swboard->conv),
									purple_account_get_username(account),
									NULL, PURPLE_CBFLAGS_NONE, TRUE);

			g_free(swboard->im_user);
			swboard->im_user = NULL;
		}
	}
	else if (swboard->conv == NULL)
	{
		swboard->conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM,
															user, account);
	}
	else
	{
		purple_debug_warning("msn", "switchboard_add_user: This should not happen!\n");
	}
}

static PurpleConversation *
msn_switchboard_get_conv(MsnSwitchBoard *swboard)
{
	PurpleAccount *account;

	g_return_val_if_fail(swboard != NULL, NULL);

	if (swboard->conv != NULL)
		return swboard->conv;

	purple_debug_error("msn", "Switchboard with unassigned conversation\n");

	account = swboard->session->account;

	return (swboard->conv = purple_conversation_new(PURPLE_CONV_TYPE_IM,
												  account, swboard->im_user));
}

static void
msn_switchboard_report_user(MsnSwitchBoard *swboard, PurpleMessageFlags flags, const char *msg)
{
	PurpleConversation *conv;

	g_return_if_fail(swboard != NULL);
	g_return_if_fail(msg != NULL);

	if ((conv = msn_switchboard_get_conv(swboard)) != NULL)
	{
		purple_conversation_write(conv, NULL, msg, flags, time(NULL));
	}
}

static void
swboard_error_helper(MsnSwitchBoard *swboard, int reason, const char *passport)
{
	g_return_if_fail(swboard != NULL);

	purple_debug_warning("msg", "Error: Unable to call the user %s for reason %i\n",
					   passport ? passport : "(null)", reason);

	/* TODO: if current_users > 0, this is probably a chat and an invite failed,
	 * we should report that in the chat or something */
	if (swboard->current_users == 0)
	{
		swboard->error = reason;
		msn_switchboard_close(swboard);
	}
}

static void
cal_error_helper(MsnTransaction *trans, int reason)
{
	MsnSwitchBoard *swboard;
	const char *passport;
	char **params;

	params = g_strsplit(trans->params, " ", 0);

	passport = params[0];

	swboard = trans->data;

	purple_debug_warning("msn", "cal_error_helper: command %s failed for reason %i\n",trans->command,reason);

	swboard_error_helper(swboard, reason, passport);

	g_strfreev(params);
}

static void
msg_error_helper(MsnCmdProc *cmdproc, MsnMessage *msg, MsnMsgErrorType error)
{
	MsnSwitchBoard *swboard;

	g_return_if_fail(cmdproc != NULL);
	g_return_if_fail(msg     != NULL);

	if ((error != MSN_MSG_ERROR_SB) && (msg->nak_cb != NULL))
		msg->nak_cb(msg, msg->ack_data);

	swboard = cmdproc->data;

	/* This is not good, and should be fixed somewhere else. */
	g_return_if_fail(swboard != NULL);

	if (msg->type == MSN_MSG_TEXT)
	{
		const char *format, *str_reason;
		char *body_str, *body_enc, *pre, *post;

#if 0
		if (swboard->conv == NULL)
		{
			if (msg->ack_ref)
				msn_message_unref(msg);

			return;
		}
#endif

		if (error == MSN_MSG_ERROR_TIMEOUT)
		{
			str_reason = _("Message may have not been sent "
						   "because a timeout occurred:");
		}
		else if (error == MSN_MSG_ERROR_SB)
		{
			switch (swboard->error)
			{
				case MSN_SB_ERROR_OFFLINE:
					str_reason = _("Message could not be sent, "
								   "not allowed while invisible:");
					break;
				case MSN_SB_ERROR_USER_OFFLINE:
					str_reason = _("Message could not be sent "
								   "because the user is offline:");
					break;
				case MSN_SB_ERROR_CONNECTION:
					str_reason = _("Message could not be sent "
								   "because a connection error occurred:");
					break;
				case MSN_SB_ERROR_TOO_FAST:
					str_reason = _("Message could not be sent "
								   "because we are sending too quickly:");
					break;
				case MSN_SB_ERROR_AUTHFAILED:
					str_reason = _("Message could not be sent "
								   "because we were unable to establish a "
								   "session with the server. This is "
								   "likely a server problem, try again in "
								   "a few minutes:");
					break;
				default:
					str_reason = _("Message could not be sent "
								   "because an error with "
								   "the switchboard occurred:");
					break;
			}
		}
		else
		{
			str_reason = _("Message may have not been sent "
						   "because an unknown error occurred:");
		}

		body_str = msn_message_to_string(msg);
		body_enc = g_markup_escape_text(body_str, -1);
		g_free(body_str);

		format = msn_message_get_attr(msg, "X-MMS-IM-Format");
		msn_parse_format(format, &pre, &post);
		body_str = g_strdup_printf("%s%s%s", pre ? pre : "",
								   body_enc ? body_enc : "", post ? post : "");
		g_free(body_enc);
		g_free(pre);
		g_free(post);

		msn_switchboard_report_user(swboard, PURPLE_MESSAGE_ERROR,
									str_reason);
		msn_switchboard_report_user(swboard, PURPLE_MESSAGE_RAW,
									body_str);

		g_free(body_str);
	}

	/* If a timeout occures we will want the msg around just in case we
	 * receive the ACK after the timeout. */
	if (msg->ack_ref && error != MSN_MSG_ERROR_TIMEOUT)
	{
		swboard->ack_list = g_list_remove(swboard->ack_list, msg);
		msn_message_unref(msg);
	}
}

/**************************************************************************
 * Message Stuff
 **************************************************************************/

/** Called when a message times out. */
static void
msg_timeout(MsnCmdProc *cmdproc, MsnTransaction *trans)
{
	MsnMessage *msg;

	msg = trans->data;

	msg_error_helper(cmdproc, msg, MSN_MSG_ERROR_TIMEOUT);
}

/** Called when we receive an error of a message. */
static void
msg_error(MsnCmdProc *cmdproc, MsnTransaction *trans, int error)
{
	msg_error_helper(cmdproc, trans->data, MSN_MSG_ERROR_UNKNOWN);
}

#if 0
/** Called when we receive an ack of a special message. */
static void
msg_ack(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnMessage *msg;

	msg = cmd->trans->data;

	if (msg->ack_cb != NULL)
		msg->ack_cb(msg->ack_data);

	msn_message_unref(msg);
}

/** Called when we receive a nak of a special message. */
static void
msg_nak(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnMessage *msg;

	msg = cmd->trans->data;

	msn_message_unref(msg);
}
#endif

static void
release_msg(MsnSwitchBoard *swboard, MsnMessage *msg)
{
	MsnCmdProc *cmdproc;
	MsnTransaction *trans;
	char *payload;
	gsize payload_len;

	g_return_if_fail(swboard != NULL);
	g_return_if_fail(msg     != NULL);

	cmdproc = swboard->cmdproc;

	payload = msn_message_gen_payload(msg, &payload_len);

#ifdef MSN_DEBUG_SB
	msn_message_show_readable(msg, "SB SEND", FALSE);
#endif

	trans = msn_transaction_new(cmdproc, "MSG", "%c %d",
								msn_message_get_flag(msg), payload_len);

	/* Data for callbacks */
	msn_transaction_set_data(trans, msg);

	if (msg->type == MSN_MSG_TEXT)
	{
		msg->ack_ref = TRUE;
		msn_message_ref(msg);
		swboard->ack_list = g_list_append(swboard->ack_list, msg);
		msn_transaction_set_timeout_cb(trans, msg_timeout);
	}
	else if (msg->type == MSN_MSG_SLP)
	{
		msg->ack_ref = TRUE;
		msn_message_ref(msg);
		swboard->ack_list = g_list_append(swboard->ack_list, msg);
		msn_transaction_set_timeout_cb(trans, msg_timeout);
#if 0
		if (msg->ack_cb != NULL)
		{
			msn_transaction_add_cb(trans, "ACK", msg_ack);
			msn_transaction_add_cb(trans, "NAK", msg_nak);
		}
#endif
	}

	trans->payload = payload;
	trans->payload_len = payload_len;

	msg->trans = trans;

	msn_cmdproc_send_trans(cmdproc, trans);
}

static void
queue_msg(MsnSwitchBoard *swboard, MsnMessage *msg)
{
	g_return_if_fail(swboard != NULL);
	g_return_if_fail(msg     != NULL);

	purple_debug_info("msn", "Appending message to queue.\n");

	g_queue_push_tail(swboard->msg_queue, msg);

	msn_message_ref(msg);
}

static void
process_queue(MsnSwitchBoard *swboard)
{
	MsnMessage *msg;

	g_return_if_fail(swboard != NULL);

	purple_debug_info("msn", "Processing queue\n");

	while ((msg = g_queue_pop_head(swboard->msg_queue)) != NULL)
	{
		purple_debug_info("msn", "Sending message\n");
		release_msg(swboard, msg);
		msn_message_unref(msg);
	}
}

gboolean
msn_switchboard_can_send(MsnSwitchBoard *swboard)
{
	g_return_val_if_fail(swboard != NULL, FALSE);

	if (swboard->empty || !g_queue_is_empty(swboard->msg_queue))
		return FALSE;

	return TRUE;
}

void
msn_switchboard_send_msg(MsnSwitchBoard *swboard, MsnMessage *msg,
						 gboolean queue)
{
	g_return_if_fail(swboard != NULL);
	g_return_if_fail(msg     != NULL);

	if (msn_switchboard_can_send(swboard))
		release_msg(swboard, msg);
	else if (queue)
		queue_msg(swboard, msg);
}

/**************************************************************************
 * Switchboard Commands
 **************************************************************************/

static void
ans_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSwitchBoard *swboard;

	swboard = cmdproc->data;
	swboard->ready = TRUE;
}

static void
bye_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSwitchBoard *swboard;
	const char *user;

	swboard = cmdproc->data;
	user = cmd->params[0];

	/* cmdproc->data is set to NULL when the switchboard is destroyed;
	 * we may get a bye shortly thereafter. */
	g_return_if_fail(swboard != NULL);

	if (!(swboard->flag & MSN_SB_FLAG_IM) && (swboard->conv != NULL))
		purple_debug_error("msn_switchboard", "bye_cmd: helper bug\n");

	if (swboard->conv == NULL)
	{
		/* This is a helper switchboard */
		msn_switchboard_destroy(swboard);
	}
	else if ((swboard->current_users > 1) ||
			 (purple_conversation_get_type(swboard->conv) == PURPLE_CONV_TYPE_CHAT))
	{
		/* This is a switchboard used for a chat */
		purple_conv_chat_remove_user(PURPLE_CONV_CHAT(swboard->conv), user, NULL);
		swboard->current_users--;
		if (swboard->current_users == 0)
			msn_switchboard_destroy(swboard);
	}
	else
	{
		/* This is a switchboard used for a im session */
		msn_switchboard_destroy(swboard);
	}
}

static void
iro_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	PurpleAccount *account;
	PurpleConnection *gc;
	MsnSwitchBoard *swboard;

	account = cmdproc->session->account;
	gc = account->gc;
	swboard = cmdproc->data;

	swboard->total_users = atoi(cmd->params[2]);

	msn_switchboard_add_user(swboard, cmd->params[3]);
}

static void
joi_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	PurpleAccount *account;
	PurpleConnection *gc;
	MsnSwitchBoard *swboard;
	const char *passport;

	passport = cmd->params[0];

	session = cmdproc->session;
	account = session->account;
	gc = account->gc;
	swboard = cmdproc->data;

	msn_switchboard_add_user(swboard, passport);

	process_queue(swboard);

	if (!session->http_method)
		send_clientcaps(swboard);

	if (swboard->closed)
		msn_switchboard_close(swboard);
}

static void
msg_cmd_post(MsnCmdProc *cmdproc, MsnCommand *cmd, char *payload, size_t len)
{
	MsnMessage *msg;

	msg = msn_message_new_from_cmd(cmdproc->session, cmd);

	msn_message_parse_payload(msg, payload, len);
#ifdef MSN_DEBUG_SB
	msn_message_show_readable(msg, "SB RECV", FALSE);
#endif

	if (msg->remote_user != NULL)
		g_free (msg->remote_user);

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
	MsnMessage *msg;

	msg = cmd->trans->data;
	g_return_if_fail(msg != NULL);

	msg_error_helper(cmdproc, msg, MSN_MSG_ERROR_NAK);
}

static void
ack_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSwitchBoard *swboard;
	MsnMessage *msg;

	msg = cmd->trans->data;

	if (msg->ack_cb != NULL)
		msg->ack_cb(msg, msg->ack_data);

	swboard = cmdproc->data;
	if (swboard)
		swboard->ack_list = g_list_remove(swboard->ack_list, msg);
	msn_message_unref(msg);
}

static void
out_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	PurpleConnection *gc;
	MsnSwitchBoard *swboard;

	gc = cmdproc->session->account->gc;
	swboard = cmdproc->data;

	if (swboard->current_users > 1)
		serv_got_chat_left(gc, swboard->chat_id);

	msn_switchboard_disconnect(swboard);
}

static void
usr_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSwitchBoard *swboard;

	swboard = cmdproc->data;

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
 * Message Handlers
 **************************************************************************/
static void
plain_msg(MsnCmdProc *cmdproc, MsnMessage *msg)
{
	PurpleConnection *gc;
	MsnSwitchBoard *swboard;
	const char *body;
	char *body_str;
	char *body_enc;
	char *body_final;
	size_t body_len;
	const char *passport;
	const char *value;

	gc = cmdproc->session->account->gc;
	swboard = cmdproc->data;

	body = msn_message_get_bin_data(msg, &body_len);
	body_str = g_strndup(body, body_len);
	body_enc = g_markup_escape_text(body_str, -1);
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
		purple_debug_misc("msn", "User-Agent = '%s'\n", value);
	}
#endif

	if ((value = msn_message_get_attr(msg, "X-MMS-IM-Format")) != NULL)
	{
		char *pre, *post;

		msn_parse_format(value, &pre, &post);

		body_final = g_strdup_printf("%s%s%s", pre ? pre : "",
									 body_enc ? body_enc : "", post ? post : "");

		g_free(pre);
		g_free(post);
		g_free(body_enc);
	}
	else
	{
		body_final = body_enc;
	}

	swboard->flag |= MSN_SB_FLAG_IM;

	if (swboard->current_users > 1 ||
		((swboard->conv != NULL) &&
		 purple_conversation_get_type(swboard->conv) == PURPLE_CONV_TYPE_CHAT))
	{
		/* If current_users is always ok as it should then there is no need to
		 * check if this is a chat. */
		if (swboard->current_users <= 1)
			purple_debug_misc("msn", "plain_msg: current_users(%d)\n",
							swboard->current_users);

		serv_got_chat_in(gc, swboard->chat_id, passport, 0, body_final,
						 time(NULL));
		if (swboard->conv == NULL)
		{
			swboard->conv = purple_find_chat(gc, swboard->chat_id);
			swboard->flag |= MSN_SB_FLAG_IM;
		}
	}
	else
	{
		serv_got_im(gc, passport, body_final, 0, time(NULL));
		if (swboard->conv == NULL)
		{
			swboard->conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM,
									passport, purple_connection_get_account(gc));
			swboard->flag |= MSN_SB_FLAG_IM;
		}
	}

	g_free(body_final);
}

static void
control_msg(MsnCmdProc *cmdproc, MsnMessage *msg)
{
	PurpleConnection *gc;
	MsnSwitchBoard *swboard;
	char *passport;

	gc = cmdproc->session->account->gc;
	swboard = cmdproc->data;
	passport = msg->remote_user;

	if (swboard->current_users == 1 &&
		msn_message_get_attr(msg, "TypingUser") != NULL)
	{
		serv_got_typing(gc, passport, MSN_TYPING_RECV_TIMEOUT,
						PURPLE_TYPING);
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

static void
nudge_msg(MsnCmdProc *cmdproc, MsnMessage *msg)
{
	MsnSwitchBoard *swboard;
	PurpleAccount *account;
	const char *user;

	swboard = cmdproc->data;
	account = cmdproc->session->account;
	user = msg->remote_user;

	serv_got_attention(account->gc, user, MSN_NUDGE);
}

/**************************************************************************
 * Connect stuff
 **************************************************************************/
static void
ans_usr_error(MsnCmdProc *cmdproc, MsnTransaction *trans, int error);

static void
connect_cb(MsnServConn *servconn)
{
	MsnSwitchBoard *swboard;
	MsnTransaction *trans;
	MsnCmdProc *cmdproc;
	PurpleAccount *account;

	cmdproc = servconn->cmdproc;
	g_return_if_fail(cmdproc != NULL);

	account = cmdproc->session->account;
	swboard = cmdproc->data;
	g_return_if_fail(swboard != NULL);

	if (msn_switchboard_is_invited(swboard))
	{
		swboard->empty = FALSE;

		trans = msn_transaction_new(cmdproc, "ANS", "%s %s %s",
									purple_account_get_username(account),
									swboard->auth_key, swboard->session_id);
	}
	else
	{
		trans = msn_transaction_new(cmdproc, "USR", "%s %s",
									purple_account_get_username(account),
									swboard->auth_key);
	}

	msn_transaction_set_error_cb(trans, ans_usr_error);
	msn_transaction_set_data(trans, swboard);
	msn_cmdproc_send_trans(cmdproc, trans);
}

static void
ans_usr_error(MsnCmdProc *cmdproc, MsnTransaction *trans, int error)
{
	MsnSwitchBoard *swboard;
	char **params;
	char *passport;
	int reason = MSN_SB_ERROR_UNKNOWN;

	if (error == 911)
	{
		reason = MSN_SB_ERROR_AUTHFAILED;
	}

	purple_debug_warning("msn", "ans_usr_error: command %s gave error %i\n", trans->command, error);

	params = g_strsplit(trans->params, " ", 0);
	passport = params[0];
	swboard = trans->data;

	swboard_error_helper(swboard, reason, passport);

	g_strfreev(params);
}

static void
disconnect_cb(MsnServConn *servconn)
{
	MsnSwitchBoard *swboard;

	swboard = servconn->cmdproc->data;
	g_return_if_fail(swboard != NULL);

	msn_servconn_set_disconnect_cb(swboard->servconn, NULL);

	msn_switchboard_destroy(swboard);
}

gboolean
msn_switchboard_connect(MsnSwitchBoard *swboard, const char *host, int port)
{
	g_return_val_if_fail(swboard != NULL, FALSE);

	msn_servconn_set_connect_cb(swboard->servconn, connect_cb);
	msn_servconn_set_disconnect_cb(swboard->servconn, disconnect_cb);

	return msn_servconn_connect(swboard->servconn, host, port);
}

void
msn_switchboard_disconnect(MsnSwitchBoard *swboard)
{
	g_return_if_fail(swboard != NULL);

	msn_servconn_disconnect(swboard->servconn);
}

/**************************************************************************
 * Call stuff
 **************************************************************************/
static void
got_cal(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
#if 0
	MsnSwitchBoard *swboard;
	const char *user;

	swboard = cmdproc->data;

	user = cmd->params[0];

	msn_switchboard_add_user(swboard, user);
#endif
}

static void
cal_timeout(MsnCmdProc *cmdproc, MsnTransaction *trans)
{
	purple_debug_warning("msn", "cal_timeout: command %s timed out\n", trans->command);

	cal_error_helper(trans, MSN_SB_ERROR_UNKNOWN);
}

static void
cal_error(MsnCmdProc *cmdproc, MsnTransaction *trans, int error)
{
	int reason = MSN_SB_ERROR_UNKNOWN;

	if (error == 215)
	{
		purple_debug_info("msn", "Invited user already in switchboard\n");
		return;
	}
	else if (error == 217)
	{
		reason = MSN_SB_ERROR_USER_OFFLINE;
	}

	purple_debug_warning("msn", "cal_error: command %s gave error %i\n", trans->command, error);

	cal_error_helper(trans, reason);
}

void
msn_switchboard_request_add_user(MsnSwitchBoard *swboard, const char *user)
{
	MsnTransaction *trans;
	MsnCmdProc *cmdproc;

	g_return_if_fail(swboard != NULL);

	cmdproc = swboard->cmdproc;

	trans = msn_transaction_new(cmdproc, "CAL", "%s", user);
	/* this doesn't do anything, but users seem to think that
	 * 'Unhandled command' is some kind of error, so we don't report it */
	msn_transaction_add_cb(trans, "CAL", got_cal);

	msn_transaction_set_data(trans, swboard);
	msn_transaction_set_timeout_cb(trans, cal_timeout);

	if (swboard->ready)
		msn_cmdproc_send_trans(cmdproc, trans);
	else
		msn_cmdproc_queue_trans(cmdproc, trans);
}

/**************************************************************************
 * Create & Transfer stuff
 **************************************************************************/

static void
got_swboard(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSwitchBoard *swboard;
	char *host;
	int port;
	swboard = cmd->trans->data;

	if (g_list_find(cmdproc->session->switches, swboard) == NULL)
		/* The conversation window was closed. */
		return;

	msn_switchboard_set_auth_key(swboard, cmd->params[4]);

	msn_parse_socket(cmd->params[2], &host, &port);

	if (!msn_switchboard_connect(swboard, host, port))
		msn_switchboard_destroy(swboard);

	g_free(host);
}

static void
xfr_error(MsnCmdProc *cmdproc, MsnTransaction *trans, int error)
{
	MsnSwitchBoard *swboard;
	int reason = MSN_SB_ERROR_UNKNOWN;

	if (error == 913)
		reason = MSN_SB_ERROR_OFFLINE;
	else if (error == 800)
		reason = MSN_SB_ERROR_TOO_FAST;

	swboard = trans->data;

	purple_debug_info("msn", "xfr_error %i for %s: trans %p, command %s, reason %i\n",
					error, (swboard->im_user ? swboard->im_user : "(null)"), trans,
					(trans->command ? trans->command : "(null)"), reason);

	swboard_error_helper(swboard, reason, swboard->im_user);
}

void
msn_switchboard_request(MsnSwitchBoard *swboard)
{
	MsnCmdProc *cmdproc;
	MsnTransaction *trans;

	g_return_if_fail(swboard != NULL);

	cmdproc = swboard->session->notification->cmdproc;

	trans = msn_transaction_new(cmdproc, "XFR", "%s", "SB");
	msn_transaction_add_cb(trans, "XFR", got_swboard);

	msn_transaction_set_data(trans, swboard);
	msn_transaction_set_error_cb(trans, xfr_error);

	msn_cmdproc_send_trans(cmdproc, trans);
}

void
msn_switchboard_close(MsnSwitchBoard *swboard)
{
	g_return_if_fail(swboard != NULL);

	if (swboard->error != MSN_SB_ERROR_NONE)
	{
		msn_switchboard_destroy(swboard);
	}
	else if (g_queue_is_empty(swboard->msg_queue) ||
			 !swboard->session->connected)
	{
		MsnCmdProc *cmdproc;
		cmdproc = swboard->cmdproc;
		msn_cmdproc_send_quick(cmdproc, "OUT", NULL, NULL);

		msn_switchboard_destroy(swboard);
	}
	else
	{
		swboard->closed = TRUE;
	}
}

gboolean
msn_switchboard_release(MsnSwitchBoard *swboard, MsnSBFlag flag)
{
	g_return_val_if_fail(swboard != NULL, FALSE);

	swboard->flag &= ~flag;

	if (flag == MSN_SB_FLAG_IM)
		/* Forget any conversation that used to be associated with this
		 * swboard. */
		swboard->conv = NULL;

	if (swboard->flag == 0)
	{
		msn_switchboard_close(swboard);
		return TRUE;
	}

	return FALSE;
}

/**************************************************************************
 * Init stuff
 **************************************************************************/

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

	msn_table_add_error(cbs_table, "MSG", msg_error);
	msn_table_add_error(cbs_table, "CAL", cal_error);

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
	msn_table_add_msg_type(cbs_table, "text/x-mms-animemoticon",
	                                           msn_emoticon_msg);
	msn_table_add_msg_type(cbs_table, "text/x-msnmsgr-datacast",
						   nudge_msg);
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
