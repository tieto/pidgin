/**
 * @file cmdproc.c MSN command processor functions
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
#include "cmdproc.h"

typedef struct
{
	char *command;
	MsnMessage *msg;

} MsnQueueEntry;

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

	if (cmdproc->last_trans != NULL)
		g_free(cmdproc->last_trans);

	while ((trans = g_queue_pop_head(cmdproc->txqueue)) != NULL)
		msn_transaction_destroy(trans);

	g_queue_free(cmdproc->txqueue);

	while (cmdproc->msg_queue != NULL)
	{
		MsnQueueEntry *entry = cmdproc->msg_queue->data;

		msn_cmdproc_unqueue_message(cmdproc, entry->msg);
	}

	msn_history_destroy(cmdproc->history);
}

void
msn_cmdproc_process_queue(MsnCmdProc *cmdproc)
{
	MsnTransaction *trans;

	while ((trans = g_queue_pop_head(cmdproc->txqueue)) != NULL &&
		   cmdproc->error == 0)
	{
		msn_cmdproc_send_trans(cmdproc, trans);
	}
}

void
msn_cmdproc_queue_trans(MsnCmdProc *cmdproc, MsnTransaction *trans)
{
	g_return_if_fail(cmdproc != NULL);
	g_return_if_fail(trans   != NULL);

	gaim_debug_info("msn", "Appending command to queue.\n");
	
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

	gaim_debug_misc("msn", "%c: %s %03d: %s\n", tmp,
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
	msn_history_add(cmdproc->history, trans);

	data = msn_transaction_to_string(trans);
	
	cmdproc->last_trans = g_strdup(data);

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
	char *params;
	va_list arg;
	size_t len;

	g_return_if_fail(cmdproc != NULL);
	g_return_if_fail(command != NULL);

	servconn = cmdproc->servconn;

	va_start(arg, format);
	params = g_strdup_vprintf(format, arg);
	va_end(arg);

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

	trans = g_new0(MsnTransaction, 1);

	trans->command = g_strdup(command);

	va_start(arg, format);
	trans->params = g_strdup_vprintf(format, arg);
	va_end(arg);

	msn_cmdproc_send_trans(cmdproc, trans);
}

void
msn_cmdproc_process_msg(MsnCmdProc *cmdproc, MsnMessage *msg)
{
	MsnServConn *servconn;
	MsnMsgCb cb;

	servconn = cmdproc->servconn;

	cb = g_hash_table_lookup(cmdproc->cbs_table->msgs,
							 msn_message_get_content_type(msg));

	if (cb == NULL)
	{
		gaim_debug_warning("msn", "Unhandled content-type '%s': %s\n",
						   msn_message_get_content_type(msg),
						   msn_message_get_body(msg));

		return;
	}

	cb(cmdproc, msg);
}

void
msn_cmdproc_process_payload(MsnCmdProc *cmdproc, char *payload, int payload_len)
{
	g_return_if_fail(cmdproc             != NULL);
	g_return_if_fail(cmdproc->payload_cb != NULL);

	cmdproc->payload_cb(cmdproc, payload, payload_len);
}

void
msn_cmdproc_process_cmd(MsnCmdProc *cmdproc, MsnCommand *cmd)
{
	MsnSession *session;
	MsnServConn *servconn;
	MsnTransaction *trans = NULL;
	MsnTransCb cb = NULL;
	GSList *l, *l_next = NULL;

	session = cmdproc->session;
	servconn = cmdproc->servconn;

	if (cmd->trId)
		trans = msn_history_find(cmdproc->history, cmd->trId);

	if (g_ascii_isdigit(cmd->command[0]))
	{
		if (trans != NULL)
		{
			MsnErrorCb error_cb = NULL;
			int error;

			error = atoi(cmd->command);
			if (cmdproc->cbs_table->errors != NULL)
				error_cb = g_hash_table_lookup(cmdproc->cbs_table->errors, trans->command);

			if (error_cb != NULL)
				error_cb(cmdproc, trans, error);
			else
			{
#if 1
				msn_error_handle(cmdproc->session, error);
#else
				gaim_debug_warning("msn", "Unhandled error '%s'\n",
								   cmd->command);
#endif
			}

			return;
		}
	}

	if (cmdproc->cbs_table->async != NULL)
		cb = g_hash_table_lookup(cmdproc->cbs_table->async, cmd->command);

	if (cb == NULL && cmd->trId)
	{
		if (trans != NULL)
		{
			cmd->trans = trans;

			if (trans->callbacks)
				cb = g_hash_table_lookup(trans->callbacks, cmd->command);
		}
	}

	if (cb != NULL)
		cb(cmdproc, cmd);
	else
	{
		gaim_debug_warning("msn", "Unhandled command '%s'\n",
							cmd->command);

		return;
	}

	if (g_list_find(session->servconns, servconn) == NULL)
		return;

	/* Process all queued messages that are waiting on this command. */
	for (l = cmdproc->msg_queue; l != NULL; l = l_next)
	{
		MsnQueueEntry *entry = l->data;
		MsnMessage *msg;

		l_next = l->next;

		if (entry->command == NULL ||
			!g_ascii_strcasecmp(entry->command, cmd->command))
		{
			msg = entry->msg;

			msn_message_ref(msg);

			msn_cmdproc_process_msg(cmdproc, msg);

			msn_cmdproc_unqueue_message(cmdproc, entry->msg);

			msn_message_destroy(msg);
			entry->msg = NULL;
		}
	}
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

void
msn_cmdproc_queue_message(MsnCmdProc *cmdproc, const char *command,
						  MsnMessage *msg)
{
	MsnQueueEntry *entry;

	g_return_if_fail(cmdproc != NULL);
	g_return_if_fail(msg != NULL);

	entry          = g_new0(MsnQueueEntry, 1);
	entry->msg     = msg;
	entry->command = (command == NULL ? NULL : g_strdup(command));

	cmdproc->msg_queue = g_slist_append(cmdproc->msg_queue, entry);

	msn_message_ref(msg);
}

void
msn_cmdproc_unqueue_message(MsnCmdProc *cmdproc, MsnMessage *msg)
{
	MsnQueueEntry *entry = NULL;
	GSList *l;

	g_return_if_fail(cmdproc != NULL);
	g_return_if_fail(msg != NULL);

	for (l = cmdproc->msg_queue; l != NULL; l = l->next)
	{
		entry = l->data;

		if (entry->msg == msg)
			break;

		entry = NULL;
	}

	g_return_if_fail(entry != NULL);

	msn_message_unref(msg);

	cmdproc->msg_queue = g_slist_remove(cmdproc->msg_queue, entry);

	if (entry->command != NULL)
		g_free(entry->command);

	g_free(entry);
}
