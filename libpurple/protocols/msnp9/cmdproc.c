/**
 * @file cmdproc.c MSN command processor functions
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
#include "cmdproc.h"

MsnCmdProc *
msn_cmdproc_new(MsnSession *session)
{
	MsnCmdProc *cmdproc;

	cmdproc = g_new0(MsnCmdProc, 1);

	cmdproc->session = session;
	cmdproc->txqueue = g_queue_new();
	cmdproc->history = msn_history_new();

	return cmdproc;
}

void
msn_cmdproc_destroy(MsnCmdProc *cmdproc)
{
	MsnTransaction *trans;

	while ((trans = g_queue_pop_head(cmdproc->txqueue)) != NULL)
		msn_transaction_destroy(trans);

	g_queue_free(cmdproc->txqueue);

	msn_history_destroy(cmdproc->history);

	if (cmdproc->last_cmd != NULL)
		msn_command_destroy(cmdproc->last_cmd);

	g_free(cmdproc);
}

void
msn_cmdproc_process_queue(MsnCmdProc *cmdproc)
{
	MsnTransaction *trans;

	while ((trans = g_queue_pop_head(cmdproc->txqueue)) != NULL)
		msn_cmdproc_send_trans(cmdproc, trans);
}

void
msn_cmdproc_queue_trans(MsnCmdProc *cmdproc, MsnTransaction *trans)
{
	g_return_if_fail(cmdproc != NULL);
	g_return_if_fail(trans   != NULL);

	g_queue_push_tail(cmdproc->txqueue, trans);
}

static void
show_debug_cmd(MsnCmdProc *cmdproc, gboolean incoming, const char *command)
{
	MsnServConn *servconn;
	const char *names[] = { "NS", "SB" };
	char *show;
	char tmp;
	size_t len;

	servconn = cmdproc->servconn;
	len = strlen(command);
	show = g_strdup(command);

	tmp = (incoming) ? 'S' : 'C';

	if ((show[len - 1] == '\n') && (show[len - 2] == '\r'))
	{
		show[len - 2] = '\0';
	}

	purple_debug_misc("msn", "%c: %s %03d: %s\n", tmp,
					names[servconn->type], servconn->num, show);

	g_free(show);
}

void
msn_cmdproc_send_trans(MsnCmdProc *cmdproc, MsnTransaction *trans)
{
	MsnServConn *servconn;
	char *data;
	size_t len;

	g_return_if_fail(cmdproc != NULL);
	g_return_if_fail(trans != NULL);

	servconn = cmdproc->servconn;

	if (!servconn->connected)
		return;

	msn_history_add(cmdproc->history, trans);

	data = msn_transaction_to_string(trans);

	len = strlen(data);

	show_debug_cmd(cmdproc, FALSE, data);

	if (trans->callbacks == NULL)
		trans->callbacks = g_hash_table_lookup(cmdproc->cbs_table->cmds,
											   trans->command);

	if (trans->payload != NULL)
	{
		data = g_realloc(data, len + trans->payload_len);
		memcpy(data + len, trans->payload, trans->payload_len);
		len += trans->payload_len;

		/*
		 * We're done with trans->payload.  Free it so that the memory
		 * doesn't sit around in cmdproc->history.
		 */
		g_free(trans->payload);
		trans->payload = NULL;
		trans->payload_len = 0;
	}

	msn_servconn_write(servconn, data, len);

	g_free(data);
}

void
msn_cmdproc_send_quick(MsnCmdProc *cmdproc, const char *command,
					   const char *format, ...)
{
	MsnServConn *servconn;
	char *data;
	char *params = NULL;
	va_list arg;
	size_t len;

	g_return_if_fail(cmdproc != NULL);
	g_return_if_fail(command != NULL);

	servconn = cmdproc->servconn;

	if (!servconn->connected)
		return;

	if (format != NULL)
	{
		va_start(arg, format);
		params = g_strdup_vprintf(format, arg);
		va_end(arg);
	}

	if (params != NULL)
		data = g_strdup_printf("%s %s\r\n", command, params);
	else
		data = g_strdup_printf("%s\r\n", command);

	g_free(params);

	len = strlen(data);

	show_debug_cmd(cmdproc, FALSE, data);

	msn_servconn_write(servconn, data, len);

	g_free(data);
}

void
msn_cmdproc_send(MsnCmdProc *cmdproc, const char *command,
				 const char *format, ...)
{
	MsnTransaction *trans;
	va_list arg;

	g_return_if_fail(cmdproc != NULL);
	g_return_if_fail(command != NULL);

	if (!cmdproc->servconn->connected)
		return;

	trans = g_new0(MsnTransaction, 1);

	trans->command = g_strdup(command);

	if (format != NULL)
	{
		va_start(arg, format);
		trans->params = g_strdup_vprintf(format, arg);
		va_end(arg);
	}

	msn_cmdproc_send_trans(cmdproc, trans);
}

void
msn_cmdproc_process_payload(MsnCmdProc *cmdproc, char *payload,
							int payload_len)
{
	MsnCommand *last;

	g_return_if_fail(cmdproc != NULL);

	last = cmdproc->last_cmd;
	last->payload = g_memdup(payload, payload_len);
	last->payload_len = payload_len;

	if (last->payload_cb != NULL)
		last->payload_cb(cmdproc, last, payload, payload_len);
}

void
msn_cmdproc_process_msg(MsnCmdProc *cmdproc, MsnMessage *msg)
{
	MsnMsgTypeCb cb;

	if (msn_message_get_content_type(msg) == NULL)
	{
		purple_debug_misc("msn", "failed to find message content\n");
		return;
	}

	cb = g_hash_table_lookup(cmdproc->cbs_table->msgs,
							 msn_message_get_content_type(msg));

	if (cb == NULL)
	{
		purple_debug_warning("msn", "Unhandled content-type '%s'\n",
						   msn_message_get_content_type(msg));

		return;
	}

	cb(cmdproc, msg);
}

void
msn_cmdproc_process_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnTransCb cb = NULL;
	MsnTransaction *trans = NULL;

	if (cmd->trId)
		trans = msn_history_find(cmdproc->history, cmd->trId);

	if (trans != NULL)
		if (trans->timer)
			purple_timeout_remove(trans->timer);

	if (g_ascii_isdigit(cmd->command[0]))
	{
		if (trans != NULL)
		{
			MsnErrorCb error_cb = NULL;
			int error;

			error = atoi(cmd->command);

			if (trans->error_cb != NULL)
				error_cb = trans->error_cb;

			if (error_cb == NULL && cmdproc->cbs_table->errors != NULL)
				error_cb = g_hash_table_lookup(cmdproc->cbs_table->errors, trans->command);

			if (error_cb != NULL)
			{
				error_cb(cmdproc, trans, error);
			}
			else
			{
#if 1
				msn_error_handle(cmdproc->session, error);
#else
				purple_debug_warning("msn", "Unhandled error '%s'\n",
								   cmd->command);
#endif
			}

			return;
		}
	}

	if (cmdproc->cbs_table->async != NULL)
		cb = g_hash_table_lookup(cmdproc->cbs_table->async, cmd->command);

	if (cb == NULL && trans != NULL)
	{
		cmd->trans = trans;

		if (trans->callbacks != NULL)
			cb = g_hash_table_lookup(trans->callbacks, cmd->command);
	}

	if (cb == NULL && cmdproc->cbs_table->fallback != NULL)
		cb = g_hash_table_lookup(cmdproc->cbs_table->fallback, cmd->command);

	if (cb != NULL)
	{
		cb(cmdproc, cmd);
	}
	else
	{
		purple_debug_warning("msn", "Unhandled command '%s'\n",
						   cmd->command);
	}

	if (trans != NULL && trans->pendent_cmd != NULL)
		msn_transaction_unqueue_cmd(trans, cmdproc);
}

void
msn_cmdproc_process_cmd_text(MsnCmdProc *cmdproc, const char *command)
{
	show_debug_cmd(cmdproc, TRUE, command);

	if (cmdproc->last_cmd != NULL)
		msn_command_destroy(cmdproc->last_cmd);

	cmdproc->last_cmd = msn_command_from_string(command);

	msn_cmdproc_process_cmd(cmdproc, cmdproc->last_cmd);
}
