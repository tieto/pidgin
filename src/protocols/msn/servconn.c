/**
 * @file servconn.c Server connection functions
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
#include "servconn.h"
#include "error.h"

static void read_cb(gpointer data, gint source, GaimInputCondition cond);

MsnServConn *
msn_servconn_new(MsnSession *session, MsnServConnType type)
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

	return servconn;
}

void
msn_servconn_destroy(MsnServConn *servconn)
{
	g_return_if_fail(servconn != NULL);

	if (servconn->processing)
	{
		servconn->wasted = TRUE;
		return;
	}

	if (servconn->connected)
		msn_servconn_disconnect(servconn);

	if (servconn->http_data != NULL)
		g_free(servconn->http_data);

	msn_cmdproc_destroy(servconn->cmdproc);
	g_free(servconn);
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
			tmp = g_strdup_printf(_("Error reading from %s server"), name);
			gaim_debug_info("msn", "Last command was: %s\n", cmd);
			break;
		default:
			tmp = g_strdup_printf(_("Unknown error from %s server"), name);
			break;
	}

	if (servconn->type != MSN_SERVER_SB)
	{
		gaim_connection_error(gc, tmp);
	}
	else
	{
		MsnSwitchBoard *swboard;
		swboard = servconn->data;
		swboard->error = MSN_SB_ERROR_CONNECTION;
	}

	g_free(tmp);
}

static void
connect_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnServConn *servconn = data;

	servconn->processing = FALSE;

	if (servconn->wasted)
	{
		msn_servconn_destroy(servconn);
		return;
	}

	servconn->fd = source;

	if (source > 0)
	{
		servconn->connected = TRUE;
		servconn->cmdproc->ready = TRUE;

		/* Someone wants to know we connected. */
		servconn->connect_cb(servconn);
		servconn->inpa = gaim_input_add(servconn->fd, GAIM_INPUT_READ,
										read_cb, data);
	}
	else
	{
		servconn->cmdproc->error = MSN_ERROR_CONNECT;
		show_error(servconn);
	}
}

gboolean
msn_servconn_connect(MsnServConn *servconn, const char *host, int port)
{
	MsnSession *session;
	int r;

	g_return_val_if_fail(servconn != NULL, FALSE);
	g_return_val_if_fail(host     != NULL, FALSE);
	g_return_val_if_fail(port      > 0,    FALSE);

	session = servconn->session;

	if (servconn->connected)
		msn_servconn_disconnect(servconn);

	if (session->http_method)
	{
		if (servconn->http_data->gateway_host != NULL)
			g_free(servconn->http_data->gateway_host);

		servconn->http_data->gateway_host = g_strdup(host);
	}

	r = gaim_proxy_connect(session->account, host, port, connect_cb,
						   servconn);

	if (r == 0)
	{
		servconn->processing = TRUE;
		return TRUE;
	}
	else
		return FALSE;
}

void
msn_servconn_disconnect(MsnServConn *servconn)
{
	g_return_if_fail(servconn != NULL);

	if (!servconn->connected)
	{
		if (servconn->disconnect_cb != NULL)
			servconn->disconnect_cb(servconn);

		return;
	}

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

		if (servconn->http_data->old_gateway_host != NULL)
			g_free(servconn->http_data->old_gateway_host);

		if (servconn->http_data->gateway_host != NULL)
			g_free(servconn->http_data->gateway_host);

		if (servconn->http_data->timer)
			gaim_timeout_remove(servconn->http_data->timer);
	}

	servconn->rx_buf = NULL;
	servconn->rx_len = 0;
	servconn->payload_len = 0;

	servconn->connected = FALSE;
	servconn->cmdproc->ready = FALSE;

	if (servconn->disconnect_cb != NULL)
		servconn->disconnect_cb(servconn);
}

void
msn_servconn_set_connect_cb(MsnServConn *servconn, void (*connect_cb)(MsnServConn *))
{
	g_return_if_fail(servconn != NULL);
	servconn->connect_cb = connect_cb;
}

void
msn_servconn_set_disconnect_cb(MsnServConn *servconn, void (*disconnect_cb)(MsnServConn *))
{
	g_return_if_fail(servconn != NULL);

	servconn->disconnect_cb = disconnect_cb;
}

static void
failed_io(MsnServConn *servconn)
{
	g_return_if_fail(servconn != NULL);

	show_error(servconn);

	msn_servconn_disconnect(servconn);
}

size_t
msn_servconn_write(MsnServConn *servconn, const char *buf, size_t len)
{
	size_t ret = FALSE;

	g_return_val_if_fail(servconn != NULL, 0);

	if (servconn->http_data == NULL)
	{
		switch (servconn->type)
		{
			case MSN_SERVER_NS:
			case MSN_SERVER_SB:
				ret = write(servconn->fd, buf, len);
				break;
			case MSN_SERVER_DC:
				ret = write(servconn->fd, &buf, sizeof(len));
				ret = write(servconn->fd, buf, len);
				break;
			default:
				ret = write(servconn->fd, buf, len);
				break;
		}
	}
	else
	{
		ret = msn_http_servconn_write(servconn, buf, len,
									  servconn->http_data->server_type);
	}

	if (ret == -1)
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

	buf[len] = '\0';

	servconn->rx_buf = g_realloc(servconn->rx_buf, len + servconn->rx_len + 1);
	memcpy(servconn->rx_buf + servconn->rx_len, buf, len + 1);
	servconn->rx_len += len;

	if (session->http_method)
	{
		char *result_msg = NULL;
		size_t result_len = 0;
		gboolean error;
		char *tmp;

		tmp = g_strndup(servconn->rx_buf, servconn->rx_len);

		if (!msn_http_servconn_parse_data(servconn, tmp, servconn->rx_len,
										  &result_msg, &result_len,
										  &error))
		{
			g_free(tmp);
			return;
		}

		g_free(tmp);

		if (error)
		{
			gaim_connection_error(gaim_account_get_connection(session->account),
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
								   servconn->port, read_cb, servconn);

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

			if (end == NULL)
				/* The command is still not complete. */
				break;

			*end = '\0';
			end += 2;
			cur_len = end - cur;
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
	} while (servconn->connected && servconn->rx_len > 0);

	if (servconn->connected)
	{
		if (servconn->rx_len > 0)
			servconn->rx_buf = g_memdup(cur, servconn->rx_len);
		else
			servconn->rx_buf = NULL;
	}

	servconn->processing = FALSE;

	if (servconn->wasted)
		msn_servconn_destroy(servconn);

	g_free(old_rx_buf);
}

#if 0
static int
create_listener(int port)
{
	int fd;
	const int on = 1;

#if 0
	struct addrinfo hints;
	struct addrinfo *c, *res;
	char port_str[5];

	snprintf(port_str, sizeof(port_str), "%d", port);

	memset(&hints, 0, sizeof(hints));

	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if (getaddrinfo(NULL, port_str, &hints, &res) != 0)
	{
		gaim_debug_error("msn", "Could not get address info: %s.\n",
						 port_str);
		return -1;
	}

	for (c = res; c != NULL; c = c->ai_next)
	{
		fd = socket(c->ai_family, c->ai_socktype, c->ai_protocol);

		if (fd < 0)
			continue;

		setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

		if (bind(fd, c->ai_addr, c->ai_addrlen) == 0)
			break;

		close(fd);
	}

	if (c == NULL)
	{
		gaim_debug_error("msn", "Could not find socket: %s.\n", port_str);
		return -1;
	}

	freeaddrinfo(res);
#else
	struct sockaddr_in sockin;

	fd = socket(AF_INET, SOCK_STREAM, 0);

	if (fd < 0)
		return -1;

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) != 0)
	{
		close(fd);
		return -1;
	}

	memset(&sockin, 0, sizeof(struct sockaddr_in));
	sockin.sin_family = AF_INET;
	sockin.sin_port = htons(port);

	if (bind(fd, (struct sockaddr *)&sockin, sizeof(struct sockaddr_in)) != 0)
	{
		close(fd);
		return -1;
	}
#endif

	if (listen (fd, 4) != 0)
	{
		close (fd);
		return -1;
	}

	fcntl(fd, F_SETFL, O_NONBLOCK);

	return fd;
}
#endif
