/**
 * @file servconn.c Server connection functions
 *
 * gaim
 *
 * Copyright (C) 2003-2004 Christian Hammond <chipx86@gnupdate.org>
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
#include "error.h"

static void read_cb(gpointer data, gint source, GaimInputCondition cond);

static void
connect_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnServConn *servconn = data;

	gaim_debug_info("msn", "In servconn's connect_cb\n");

	servconn->fd = source;

	if (source > 0)
	{
		/* Someone wants to know we connected. */
		servconn->connect_cb(servconn);
		servconn->inpa = gaim_input_add(servconn->fd, GAIM_INPUT_READ,
										read_cb, data);
	}
	else
	{
		GaimConnection *gc = servconn->session->account->gc;
		gaim_connection_error(gc, _("Unable to connect."));
	}
}

MsnServConn *
msn_servconn_new(MsnSession *session, MsnServerType type)
{
	MsnServConn *servconn;

	g_return_val_if_fail(session != NULL, NULL);

	servconn = g_new0(MsnServConn, 1);

	servconn->type = type;

	servconn->session = session;
	servconn->cmdproc = msn_cmdproc_new(session);
	servconn->cmdproc->servconn = servconn;

	if (session->http_method)
	{
		servconn->http_data = g_new0(MsnHttpMethodData, 1);
		servconn->http_data->virgin = TRUE;
	}

	servconn->num = session->servconns_count++;
	session->servconns = g_list_append(session->servconns, servconn);

	return servconn;
}

gboolean
msn_servconn_connect(MsnServConn *servconn, const char *host, int port)
{
	MsnSession *session;
	int i;

	g_return_val_if_fail(servconn != NULL, FALSE);
	g_return_val_if_fail(host     != NULL, FALSE);
	g_return_val_if_fail(port      > 0,    FALSE);

	session = servconn->session;

	if (servconn->connected)
		msn_servconn_disconnect(servconn);

	if (session->http_method)
	{
		servconn->http_data->gateway_ip = g_strdup(host);
	}

	i = gaim_proxy_connect(session->account, host, port, connect_cb,
						   servconn);

	if (i == 0)
		servconn->connected = TRUE;

	return servconn->connected;
}

void
msn_servconn_disconnect(MsnServConn *servconn)
{
	MsnSession *session;

	g_return_if_fail(servconn != NULL);
	g_return_if_fail(servconn->connected);

	session = servconn->session;

	if (servconn->inpa > 0)
	{
		gaim_input_remove(servconn->inpa);
		servconn->inpa = 0;
	}

	close(servconn->fd);

	if (servconn->http_data != NULL)
	{
		if (servconn->http_data->session_id != NULL)
			g_free(servconn->http_data->session_id);

		if (servconn->http_data->old_gateway_ip != NULL)
			g_free(servconn->http_data->old_gateway_ip);

		if (servconn->http_data->gateway_ip != NULL)
			g_free(servconn->http_data->gateway_ip);

		if (servconn->http_data->timer)
			gaim_timeout_remove(servconn->http_data->timer);

		g_free(servconn->http_data);
	}

	servconn->rx_len = 0;
	servconn->payload_len = 0;

	if (servconn->disconnect_cb != NULL)
		servconn->disconnect_cb(servconn);

	servconn->connected = FALSE;
}

void
msn_servconn_destroy(MsnServConn *servconn)
{
	MsnSession *session;

	g_return_if_fail(servconn != NULL);

	if (servconn->processing)
	{
		servconn->wasted = TRUE;
		return;
	}

	session = servconn->session;

	session->servconns = g_list_remove(session->servconns, servconn);

	if (servconn->connected)
		msn_servconn_disconnect(servconn);

	msn_cmdproc_destroy(servconn->cmdproc);
	g_free(servconn);
}

void
msn_servconn_set_connect_cb(MsnServConn *servconn,
							gboolean (*connect_cb)(MsnServConn *servconn))
{
	g_return_if_fail(servconn != NULL);

	servconn->connect_cb = connect_cb;
}

void
msn_servconn_set_disconnect_cb(MsnServConn *servconn,
							   void (*disconnect_cb)(MsnServConn
													 *servconn))
{
	g_return_if_fail(servconn != NULL);

	servconn->disconnect_cb = disconnect_cb;
}

static void
show_error(MsnServConn *servconn)
{
	GaimConnection *gc;
	char *tmp;
	char *cmd;

	const char *names[] = { "Notification", "Switchboard" };
	const char *name;

	gc = gaim_account_get_connection(servconn->session->account);
	name = names[servconn->type];

	switch (servconn->cmdproc->error)
	{
		case MSN_ERROR_CONNECT:
			tmp = g_strdup_printf(_("Unable to connect to %s server"),
								  name);
			break;
		case MSN_ERROR_WRITE:
			tmp = g_strdup_printf(_("Error writing to %s server"), name);
			break;
		case MSN_ERROR_READ:
			cmd = servconn->cmdproc->last_trans;
			tmp = g_strdup_printf(_("Error reading from %s server. Last"
									"command was:\n %s"), name, cmd);
			break;
		default:
			tmp = g_strdup_printf(_("Unknown error from %s server"), name);
			break;
	}

	if (servconn->type != MSN_SERVER_SB)
		gaim_connection_error(gc, tmp);
	else
		gaim_notify_error(gc, NULL, tmp, NULL);

	g_free(tmp);
}

static void
failed_io(MsnServConn *servconn)
{
	show_error(servconn);

	msn_servconn_disconnect(servconn);
}

size_t
msn_servconn_write(MsnServConn *servconn, const char *buf, size_t size)
{
	size_t ret = FALSE;

	g_return_val_if_fail(servconn != NULL, 0);

	if (servconn->session->http_method)
	{
		ret = msn_http_servconn_write(servconn, buf, size,
									   servconn->http_data->server_type);
	}
	else
	{
		ret = write(servconn->fd, buf, size);
	}

	if (ret < 0)
	{
		servconn->cmdproc->error = MSN_ERROR_WRITE;
		failed_io(servconn);
	}

	return ret;
}

static void
read_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnServConn *servconn;
	MsnSession *session;
	char buf[MSN_BUF_LEN];
	char *cur, *end, *old_rx_buf;
	int len, cur_len;

	servconn = data;
	session = servconn->session;

	len = read(servconn->fd, buf, sizeof(buf) - 1);

	if (len <= 0)
	{
		servconn->cmdproc->error = MSN_ERROR_READ;

		failed_io(servconn);

		return;
	}

	servconn->rx_buf = g_realloc(servconn->rx_buf, len + servconn->rx_len);
	memcpy(servconn->rx_buf + servconn->rx_len, buf, len);
	servconn->rx_len += len;

	if (session->http_method)
	{
		char *result_msg = NULL;
		size_t result_len = 0;
		gboolean error;
		char *tmp;

		tmp = g_strndup(servconn->rx_buf, servconn->rx_len);

		if (!msn_http_servconn_parse_data(servconn, tmp,
										  servconn->rx_len, &result_msg,
										  &result_len, &error))
		{
			g_free(tmp);
			return;
		}

		g_free(tmp);

		if (error)
		{
			gaim_connection_error(
				gaim_account_get_connection(session->account),
				_("Received HTTP error. Please report this."));

			return;
		}

		if (servconn->http_data->session_id != NULL &&
			!strcmp(servconn->http_data->session_id, "close"))
		{
			msn_servconn_destroy(servconn);

			return;
		}

#if 0
		if (strcmp(servconn->http_data->gateway_ip,
				   msn_servconn_get_server(servconn)) != 0)
		{
			int i;

			/* Evil hackery. I promise to remove it, even though I can't. */

			servconn->connected = FALSE;

			if (servconn->inpa)
				gaim_input_remove(servconn->inpa);

			close(servconn->fd);

			i = gaim_proxy_connect(session->account, servconn->host,
								   servconn->port, servconn->login_cb,
								   servconn);

			if (i == 0)
				servconn->connected = TRUE;
		}
#endif

		g_free(servconn->rx_buf);
		servconn->rx_buf = result_msg;
		servconn->rx_len = result_len;
	}

	end = old_rx_buf = servconn->rx_buf;

	servconn->processing = TRUE;

	do
	{
		cur = end;

		if (servconn->payload_len)
		{
			if (servconn->payload_len > servconn->rx_len)
				/* The payload is still not complete. */
				break;

			cur_len = servconn->payload_len;
			end += cur_len;
		}
		else
		{
			end = strstr(cur, "\r\n");

			if (!end)
				/* The command is still not complete. */
				break;

			*end = '\0';
			cur_len = end - cur + 2;
			end += 2;
		}

		servconn->rx_len -= cur_len;

		if (servconn->payload_len)
		{
			msn_cmdproc_process_payload(servconn->cmdproc, cur, cur_len);
			servconn->payload_len = 0;
		}
		else
		{
			msn_cmdproc_process_cmd_text(servconn->cmdproc, cur);
		}
	} while (servconn->connected && servconn->rx_len);

	if (servconn->connected)
	{
		if (servconn->rx_len)
			servconn->rx_buf = g_memdup(cur, servconn->rx_len);
		else
			servconn->rx_buf = NULL;
	}

	servconn->processing = FALSE;

	if (servconn->wasted)
		msn_servconn_destroy(servconn);

	g_free(old_rx_buf);
}
