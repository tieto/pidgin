/**
 * @file servconn.c Server connection functions
 *
 * gaim
 *
 * Copyright (C) 2003 Christian Hammond <chipx86@gnupdate.org>
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
#include "servconn.h"

static gboolean
__process_single_line(MsnServConn *servconn, char *str)
{
	MsnServConnCommandCb cb;
	gboolean result;
	size_t param_count = 0;
	char *command, *param_start;
	char **params = NULL;

	command = str;

	/**
	 * See how many spaces we have in this.
	 */
	param_start = strchr(command, ' ');

	if (param_start != NULL) {
		params = g_strsplit(param_start + 1, " ", 0);

		for (param_count = 0; params[param_count] != NULL; param_count++)
			;

		*param_start = '\0';
	}

	cb = g_hash_table_lookup(servconn->commands, command);

	if (cb == NULL) {
		cb = g_hash_table_lookup(servconn->commands, "_unknown_");

		if (cb == NULL) {
			gaim_debug(GAIM_DEBUG_WARNING, "msn",
					   "Unhandled command '%s'\n", str);

			if (params != NULL)
				g_strfreev(params);

			return TRUE;
		}
	}

	result = cb(servconn, command, (const char **)params, param_count);

	if (params != NULL)
		g_strfreev(params);

	return result;
}

static gboolean
__process_multi_line(MsnServConn *servconn, char *buffer)
{
	MsnServConnMsgCb cb;
	MsnMessage *msg;
	char msg_str[MSN_BUF_LEN];

	g_snprintf(msg_str, sizeof(msg_str),
			   "MSG %s %s %d\r\n%s",
			   servconn->msg_passport, servconn->msg_friendly,
			   servconn->msg_len, buffer);

	msg = msn_message_new_from_str(servconn->session, msg_str);

	cb = g_hash_table_lookup(servconn->msg_types,
							 msn_message_get_content_type(msg));

	if (cb == NULL) {
		gaim_debug(GAIM_DEBUG_WARNING, "msn",
				   "Unhandled content-type '%s': %s\n",
				   msn_message_get_content_type(msg),
				   msn_message_get_body(msg));

		msn_message_destroy(msg);

		return FALSE;
	}

	cb(servconn, msg);

	msn_message_destroy(msg);

	return TRUE;
}

static void
__connect_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnServConn *servconn = data;

	if (servconn->connect_cb(data, source, cond))
		servconn->inpa = gaim_input_add(servconn->fd, GAIM_INPUT_READ,
										servconn->login_cb, data);
}

MsnServConn *
msn_servconn_new(MsnSession *session)
{
	MsnServConn *servconn;

	g_return_val_if_fail(session != NULL, NULL);

	servconn = g_new0(MsnServConn, 1);

	servconn->login_cb = msn_servconn_parse_data;
	servconn->session = session;

	servconn->commands = g_hash_table_new_full(g_str_hash, g_str_equal,
											   g_free, NULL);

	servconn->msg_types = g_hash_table_new_full(g_str_hash, g_str_equal,
												g_free, NULL);

	return servconn;
}

gboolean
msn_servconn_connect(MsnServConn *servconn)
{
	int i;

	g_return_val_if_fail(servconn != NULL, FALSE);
	g_return_val_if_fail(servconn->server != NULL, FALSE);
	g_return_val_if_fail(!servconn->connected, TRUE);

	i = proxy_connect(servconn->session->account, servconn->server,
					  servconn->port, __connect_cb, servconn);

	if (i == 0)
		servconn->connected = TRUE;

	return servconn->connected;
}

void
msn_servconn_disconnect(MsnServConn *servconn)
{
	g_return_if_fail(servconn != NULL);
	g_return_if_fail(servconn->connected);

	close(servconn->fd);

	if (servconn->inpa)
		gaim_input_remove(servconn->inpa);

	g_free(servconn->rxqueue);

	while (servconn->txqueue) {
		g_free(servconn->txqueue->data);

		servconn->txqueue = g_slist_remove(servconn->txqueue,
										   servconn->txqueue->data);
	}

	servconn->connected = FALSE;
}

void
msn_servconn_destroy(MsnServConn *servconn)
{
	g_return_if_fail(servconn != NULL);

	if (servconn->connected)
		msn_servconn_disconnect(servconn);

	if (servconn->server != NULL)
		g_free(servconn->server);

	g_free(servconn);
}

void
msn_servconn_set_server(MsnServConn *servconn, const char *server, int port)
{
	g_return_if_fail(servconn != NULL);
	g_return_if_fail(server != NULL);
	g_return_if_fail(port > 0);

	if (servconn->server != NULL)
		g_free(servconn->server);

	servconn->server = g_strdup(server);
	servconn->port   = port;
}

const char *
msn_servconn_get_server(const MsnServConn *servconn)
{
	g_return_val_if_fail(servconn != NULL, NULL);

	return servconn->server;
}

int
msn_servconn_get_port(const MsnServConn *servconn)
{
	g_return_val_if_fail(servconn != NULL, 0);

	return servconn->port;
}

void
msn_servconn_set_connect_cb(MsnServConn *servconn,
							gboolean (*connect_cb)(gpointer, gint,
												   GaimInputCondition))
{
	g_return_if_fail(servconn != NULL);

	servconn->connect_cb = connect_cb;
}

void
msn_servconn_set_failed_read_cb(MsnServConn *servconn,
								void (*failed_read_cb)(gpointer, gint,
													   GaimInputCondition))
{
	g_return_if_fail(servconn != NULL);

	servconn->failed_read_cb = failed_read_cb;
}

size_t
msn_servconn_write(MsnServConn *servconn, const char *buf, size_t size)
{
	g_return_val_if_fail(servconn != NULL, 0);

	gaim_debug(GAIM_DEBUG_MISC, "msn", "C: %s%s", buf,
			   (*(buf + size - 1) == '\n' ? "" : "\n"));

	return write(servconn->fd, buf, size);
}

gboolean
msn_servconn_send_command(MsnServConn *servconn, const char *command,
						  const char *params)
{
	char buf[MSN_BUF_LEN];

	g_return_val_if_fail(servconn != NULL, FALSE);
	g_return_val_if_fail(command != NULL, FALSE);

	if (params == NULL)
		g_snprintf(buf, sizeof(buf), "%s %u\r\n", command,
				   servconn->session->trId++);
	else
		g_snprintf(buf, sizeof(buf), "%s %u %s\r\n",
				   command, servconn->session->trId++, params);

	return (msn_servconn_write(servconn, buf, strlen(buf)) > 0);
}

void
msn_servconn_register_command(MsnServConn *servconn, const char *command,
							  MsnServConnCommandCb cb)
{
	char *command_up;

	g_return_if_fail(servconn != NULL);
	g_return_if_fail(command != NULL);
	g_return_if_fail(cb != NULL);

	command_up = g_ascii_strup(command, -1);

	g_hash_table_insert(servconn->commands, command_up, cb);
}

void
msn_servconn_register_msg_type(MsnServConn *servconn,
							   const char *content_type,
							   MsnServConnMsgCb cb)
{
	g_return_if_fail(servconn != NULL);
	g_return_if_fail(content_type != NULL);
	g_return_if_fail(cb != NULL);

	g_hash_table_insert(servconn->msg_types, g_strdup(content_type), cb);
}

void
msn_servconn_parse_data(gpointer data, gint source, GaimInputCondition cond)
{
	MsnServConn *servconn = (MsnServConn *)data;
	char buf[MSN_BUF_LEN];
	gboolean cont = TRUE;
	int len;

	len = read(servconn->fd, buf, sizeof(buf));

	if (len <= 0) {
		if (servconn->failed_read_cb != NULL)
			servconn->failed_read_cb(data, source, cond);

		return;
	}

	servconn->rxqueue = g_realloc(servconn->rxqueue, len + servconn->rxlen);
	memcpy(servconn->rxqueue + servconn->rxlen, buf, len);
	servconn->rxlen += len;

	while (cont) {
		if (servconn->parsing_msg) {
			char *msg;

			if (servconn->rxlen == 0)
				break;

			if (servconn->msg_len > servconn->rxlen)
				break;

			msg = servconn->rxqueue;
			servconn->rxlen -= servconn->msg_len;

			if (servconn->rxlen) {
				servconn->rxqueue = g_memdup(msg + servconn->msg_len,
											 servconn->rxlen);
			}
			else {
				servconn->rxqueue = NULL;
				msg = g_realloc(msg, servconn->msg_len + 1);
			}

			msg[servconn->msg_len] = '\0';
			servconn->parsing_msg = FALSE;

			__process_multi_line(servconn, msg);

			servconn->msg_len = 0;
			g_free(servconn->msg_passport);
			g_free(servconn->msg_friendly);
			g_free(msg);
		}
		else {
			char *end = servconn->rxqueue;
			char *cmd;
			int cmdlen, i;

			if (!servconn->rxlen)
				return;

			for (i = 0; i < servconn->rxlen - 1; end++, i++) {
				if (*end == '\r' && end[1] == '\n')
					break;
			}

			if (i == servconn->rxlen - 1)
				return;

			cmdlen = end - servconn->rxqueue + 2;
			cmd = servconn->rxqueue;
			servconn->rxlen -= cmdlen;

			if (servconn->rxlen)
				servconn->rxqueue = g_memdup(cmd + cmdlen, servconn->rxlen);
			else {
				servconn->rxqueue = NULL;
				cmd = g_realloc(cmd, cmdlen + 1);
			}

			cmd[cmdlen] = '\0';

			gaim_debug(GAIM_DEBUG_MISC, "msn", "S: %s", cmd);

			g_strchomp(cmd);

			cont = __process_single_line(servconn, cmd);

			g_free(cmd);
		}
	}
}

