/**
 * @file httpmethod.c HTTP connection method
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
#include "debug.h"
#include "httpconn.h"

typedef struct
{
	MsnHttpConn *httpconn;
	char *body;
	size_t body_len;
} MsnHttpQueueData;

static void
msn_httpconn_process_queue(MsnHttpConn *httpconn)
{
	httpconn->waiting_response = FALSE;

	if (httpconn->queue != NULL)
	{
		MsnHttpQueueData *queue_data;

		queue_data = (MsnHttpQueueData *)httpconn->queue->data;

		httpconn->queue = g_list_remove(httpconn->queue, queue_data);

		msn_httpconn_write(queue_data->httpconn,
						   queue_data->body,
						   queue_data->body_len);

		g_free(queue_data->body);
		g_free(queue_data);
	}
}

static gboolean
msn_httpconn_parse_data(MsnHttpConn *httpconn, const char *buf,
						size_t size, char **ret_buf, size_t *ret_size,
						gboolean *error)
{
	const char *s, *c;
	char *header, *body;
	const char *body_start;
	char *tmp;
	size_t body_len = 0;
	gboolean wasted = FALSE;

	g_return_val_if_fail(httpconn != NULL, FALSE);
	g_return_val_if_fail(buf      != NULL, FALSE);
	g_return_val_if_fail(size      > 0,    FALSE);
	g_return_val_if_fail(ret_buf  != NULL, FALSE);
	g_return_val_if_fail(ret_size != NULL, FALSE);
	g_return_val_if_fail(error    != NULL, FALSE);

#if 0
	gaim_debug_info("msn", "HTTP: parsing data {%s}\n", buf);
#endif

	/* Healthy defaults. */
	body = NULL;

	*ret_buf  = NULL;
	*ret_size = 0;
	*error    = FALSE;

	/* First, some tests to see if we have a full block of stuff. */
	if (((strncmp(buf, "HTTP/1.1 200 OK\r\n", 17) != 0) &&
		 (strncmp(buf, "HTTP/1.1 100 Continue\r\n", 23) != 0)) &&
		((strncmp(buf, "HTTP/1.0 200 OK\r\n", 17) != 0) &&
		 (strncmp(buf, "HTTP/1.0 100 Continue\r\n", 23) != 0)))
	{
		*error = TRUE;

		return FALSE;
	}

	if (strncmp(buf, "HTTP/1.1 100 Continue\r\n", 23) == 0)
	{
		if ((s = strstr(buf, "\r\n\r\n")) == NULL)
			return FALSE;

		s += 4;

		if (*s == '\0')
		{
			*ret_buf = g_strdup("");
			*ret_size = 0;

			msn_httpconn_process_queue(httpconn);

			return TRUE;
		}

		buf = s;
		size -= (s - buf);
	}

	if ((s = strstr(buf, "\r\n\r\n")) == NULL)
		/* Need to wait for the full HTTP header to arrive */
		return FALSE;

	s += 4; /* Skip \r\n */
	header = g_strndup(buf, s - buf);
	body_start = s;
	body_len = size - (body_start - buf);

	if ((s = gaim_strcasestr(header, "Content-Length: ")) != NULL)
	{
		int tmp_len;

		s += strlen("Content-Length: ");

		if ((c = strchr(s, '\r')) == NULL)
		{
			g_free(header);

			return FALSE;
		}

		tmp = g_strndup(s, c - s);
		tmp_len = atoi(tmp);
		g_free(tmp);

		if (body_len != tmp_len)
		{
			/* Need to wait for the full packet to arrive */

			g_free(header);

#if 0
			gaim_debug_warning("msn",
							   "body length (%d) != content length (%d)\n",
							   body_len, tmp_len);
#endif

			return FALSE;
		}
	}

	body = g_malloc0(body_len + 1);
	memcpy(body, body_start, body_len);

#ifdef MSN_DEBUG_HTTP
	gaim_debug_misc("msn", "Incoming HTTP buffer (header): {%s\r\n}\n",
					header);
#endif

	/* Now we should be able to process the data. */
	if ((s = gaim_strcasestr(header, "X-MSN-Messenger: ")) != NULL)
	{
		char *full_session_id, *gw_ip, *session_action;
		char *t, *session_id;
		char **elems, **cur, **tokens;

		full_session_id = gw_ip = session_action = NULL;

		s += strlen("X-MSN-Messenger: ");

		if ((c = strchr(s, '\r')) == NULL)
		{
			msn_session_set_error(httpconn->session,
								  MSN_ERROR_HTTP_MALFORMED, NULL);
			gaim_debug_error("msn", "Malformed X-MSN-Messenger field.\n{%s}",
							 buf);

			g_free(body);
			return FALSE;
		}

		tmp = g_strndup(s, c - s);

		elems = g_strsplit(tmp, "; ", 0);

		for (cur = elems; *cur != NULL; cur++)
		{
			tokens = g_strsplit(*cur, "=", 2);

			if (strcmp(tokens[0], "SessionID") == 0)
				full_session_id = tokens[1];
			else if (strcmp(tokens[0], "GW-IP") == 0)
				gw_ip = tokens[1];
			else if (strcmp(tokens[0], "Session") == 0)
				session_action = tokens[1];
			else
				g_free(tokens[1]);

			g_free(tokens[0]);
			/* Don't free each of the tokens, only the array. */
			g_free(tokens);
		}

		g_strfreev(elems);

		g_free(tmp);

		if ((session_action != NULL) && (strcmp(session_action, "close") == 0))
			wasted = TRUE;

		g_free(session_action);

		t = strchr(full_session_id, '.');
		session_id = g_strndup(full_session_id, t - full_session_id);

		if (!wasted)
		{
			g_free(httpconn->full_session_id);
			httpconn->full_session_id = full_session_id;

			g_free(httpconn->session_id);
			httpconn->session_id = session_id;

			g_free(httpconn->host);
			httpconn->host = gw_ip;
		}
		else
		{
			MsnServConn *servconn;

			/* It's going to die. */
			/* poor thing */

			servconn = httpconn->servconn;

			/* I'll be honest, I don't fully understand all this, but this
			 * causes crashes, Stu. */
			/* if (servconn != NULL)
				servconn->wasted = TRUE; */

			g_free(full_session_id);
			g_free(session_id);
			g_free(gw_ip);
		}
	}

	g_free(header);

	*ret_buf  = body;
	*ret_size = body_len;

	msn_httpconn_process_queue(httpconn);

	return TRUE;
}

static void
read_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnHttpConn *httpconn;
	MsnServConn *servconn;
	MsnSession *session;
	char buf[MSN_BUF_LEN];
	char *cur, *end, *old_rx_buf;
	int len, cur_len;
	char *result_msg = NULL;
	size_t result_len = 0;
	gboolean error = FALSE;

	httpconn = data;
	servconn = NULL;
	session = httpconn->session;

	len = read(httpconn->fd, buf, sizeof(buf) - 1);

	if (len < 0 && errno == EAGAIN)
		return;
	else if (len <= 0)
	{
		gaim_debug_error("msn", "HTTP: Read error\n");
		msn_servconn_got_error(httpconn->servconn, MSN_SERVCONN_ERROR_READ);

		return;
	}

	buf[len] = '\0';

	httpconn->rx_buf = g_realloc(httpconn->rx_buf, len + httpconn->rx_len + 1);
	memcpy(httpconn->rx_buf + httpconn->rx_len, buf, len + 1);
	httpconn->rx_len += len;

	if (!msn_httpconn_parse_data(httpconn, httpconn->rx_buf, httpconn->rx_len,
								 &result_msg, &result_len, &error))
	{
		/* Either we must wait for more input, or something went wrong */
		if (error)
			msn_servconn_got_error(httpconn->servconn, MSN_SERVCONN_ERROR_READ);

		return;
	}

	httpconn->servconn->processing = FALSE;

	servconn = httpconn->servconn;

	if (error)
	{
		gaim_debug_error("msn", "HTTP: Special error\n");
		msn_servconn_got_error(httpconn->servconn, MSN_SERVCONN_ERROR_READ);

		return;
	}

	g_free(httpconn->rx_buf);
	httpconn->rx_buf = NULL;
	httpconn->rx_len = 0;

	if (result_len == 0)
	{
		/* Nothing to do here */
#if 0
		gaim_debug_info("msn", "HTTP: nothing to do here\n");
#endif
		g_free(result_msg);
		return;
	}

	g_free(servconn->rx_buf);
	servconn->rx_buf = result_msg;
	servconn->rx_len = result_len;

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

static void
httpconn_write_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnHttpConn *httpconn;
	int ret, writelen;

	httpconn = data;
	writelen = gaim_circ_buffer_get_max_read(httpconn->tx_buf);

	if (writelen == 0)
	{
		gaim_input_remove(httpconn->tx_handler);
		httpconn->tx_handler = 0;
		return;
	}

	ret = write(httpconn->fd, httpconn->tx_buf->outptr, writelen);
	if (ret <= 0)
	{
		if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
			/* No worries */
			return;

		/* Error! */
		msn_servconn_got_error(httpconn->servconn, MSN_SERVCONN_ERROR_WRITE);
		return;
	}

	gaim_circ_buffer_mark_read(httpconn->tx_buf, ret);

	/* TODO: I don't think these 2 lines are needed.  Remove them? */
	if (ret == writelen)
		httpconn_write_cb(data, source, cond);
}

static gboolean
write_raw(MsnHttpConn *httpconn, const char *data, size_t data_len)
{
	ssize_t res; /* result of the write operation */

	if (httpconn->tx_handler == 0)
		res = write(httpconn->fd, data, data_len);
	else
	{
		res = -1;
		errno = EAGAIN;
	}

	if ((res <= 0) && ((errno != EAGAIN) && (errno != EWOULDBLOCK)))
	{
		msn_servconn_got_error(httpconn->servconn, MSN_SERVCONN_ERROR_WRITE);
		return FALSE;
	}

	if (res < 0 || res < data_len)
	{
		if (res < 0)
			res = 0;
		if (httpconn->tx_handler == 0 && httpconn->fd)
			httpconn->tx_handler = gaim_input_add(httpconn->fd,
				GAIM_INPUT_WRITE, httpconn_write_cb, httpconn);
		gaim_circ_buffer_append(httpconn->tx_buf, data + res,
			data_len - res);
	}

	return TRUE;
}

static char *
msn_httpconn_proxy_auth(MsnHttpConn *httpconn)
{
	GaimAccount *account;
	GaimProxyInfo *gpi;
	const char *username, *password;
	char *auth = NULL;

	account = httpconn->session->account;

	if (gaim_account_get_proxy_info(account) == NULL)
		gpi = gaim_global_proxy_get_info();
	else
		gpi = gaim_account_get_proxy_info(account);

	if (gpi == NULL || !(gaim_proxy_info_get_type(gpi) == GAIM_PROXY_HTTP ||
						 gaim_proxy_info_get_type(gpi) == GAIM_PROXY_USE_ENVVAR))
		return NULL;

	username = gaim_proxy_info_get_username(gpi);
	password = gaim_proxy_info_get_password(gpi);

	if (username != NULL) {
		char *tmp;
		auth = g_strdup_printf("%s:%s", username, password ? password : "");
		tmp = gaim_base64_encode((const guchar *)auth, strlen(auth));
		g_free(auth);
		auth = g_strdup_printf("Proxy-Authorization: Basic %s\r\n", tmp);
		g_free(tmp);
	}

	return auth;
}

static gboolean
msn_httpconn_poll(gpointer data)
{
	MsnHttpConn *httpconn;
	char *header;
	char *auth;

	httpconn = data;

	g_return_val_if_fail(httpconn != NULL, FALSE);

	if ((httpconn->host == NULL) || (httpconn->full_session_id == NULL))
	{
		/* There's no need to poll if the session is not fully established */
		return TRUE;
	}

	if (httpconn->waiting_response)
	{
		/* There's no need to poll if we're already waiting for a response */
		return TRUE;
	}

	auth = msn_httpconn_proxy_auth(httpconn);

	header = g_strdup_printf(
		"POST http://%s/gateway/gateway.dll?Action=poll&SessionID=%s HTTP/1.1\r\n"
		"Accept: */*\r\n"
		"Accept-Language: en-us\r\n"
		"User-Agent: MSMSGS\r\n"
		"Host: %s\r\n"
		"Proxy-Connection: Keep-Alive\r\n"
		"%s" /* Proxy auth */
		"Connection: Keep-Alive\r\n"
		"Pragma: no-cache\r\n"
		"Content-Type: application/x-msn-messenger\r\n"
		"Content-Length: 0\r\n\r\n",
		httpconn->host,
		httpconn->full_session_id,
		httpconn->host,
		auth ? auth : "");

	g_free(auth);

	if (write_raw(httpconn, header, strlen(header)))
		httpconn->waiting_response = TRUE;

	g_free(header);

	return TRUE;
}

ssize_t
msn_httpconn_write(MsnHttpConn *httpconn, const char *body, size_t body_len)
{
	char *params;
	char *data;
	int header_len;
	char *auth;
	const char *server_types[] = { "NS", "SB" };
	const char *server_type;
	char *host;
	MsnServConn *servconn;

	/* TODO: remove http data from servconn */

	g_return_val_if_fail(httpconn != NULL, 0);
	g_return_val_if_fail(body != NULL, 0);
	g_return_val_if_fail(body_len > 0, 0);

	servconn = httpconn->servconn;

	if (httpconn->waiting_response)
	{
		MsnHttpQueueData *queue_data = g_new0(MsnHttpQueueData, 1);

		queue_data->httpconn = httpconn;
		queue_data->body     = g_memdup(body, body_len);
		queue_data->body_len = body_len;

		httpconn->queue = g_list_append(httpconn->queue, queue_data);

		return body_len;
	}

	server_type = server_types[servconn->type];

	if (httpconn->virgin)
	{
		host = "gateway.messenger.hotmail.com";

		/* The first time servconn->host is the host we should connect to. */
		params = g_strdup_printf("Action=open&Server=%s&IP=%s",
								 server_type,
								 servconn->host);
		httpconn->virgin = FALSE;
	}
	else
	{
		/* The rest of the times servconn->host is the gateway host. */
		host = httpconn->host;

		if (host == NULL || httpconn->full_session_id == NULL)
		{
			gaim_debug_warning("msn", "Attempted HTTP write before session is established\n");
			return -1;
		}

		params = g_strdup_printf("SessionID=%s",
			httpconn->full_session_id);
	}

	auth = msn_httpconn_proxy_auth(httpconn);

	data = g_strdup_printf(
		"POST http://%s/gateway/gateway.dll?%s HTTP/1.1\r\n"
		"Accept: */*\r\n"
		"Accept-Language: en-us\r\n"
		"User-Agent: MSMSGS\r\n"
		"Host: %s\r\n"
		"Proxy-Connection: Keep-Alive\r\n"
		"%s" /* Proxy auth */
		"Connection: Keep-Alive\r\n"
		"Pragma: no-cache\r\n"
		"Content-Type: application/x-msn-messenger\r\n"
		"Content-Length: %d\r\n\r\n",
		host,
		params,
		host,
		auth ? auth : "",
		(int) body_len);

	g_free(params);

	g_free(auth);

	header_len = strlen(data);
	data = g_realloc(data, header_len + body_len);
	memcpy(data + header_len, body, body_len);

	if (write_raw(httpconn, data, header_len + body_len))
		httpconn->waiting_response = TRUE;

	g_free(data);

	return body_len;
}

MsnHttpConn *
msn_httpconn_new(MsnServConn *servconn)
{
	MsnHttpConn *httpconn;

	g_return_val_if_fail(servconn != NULL, NULL);

	httpconn = g_new0(MsnHttpConn, 1);

	gaim_debug_info("msn", "new httpconn (%p)\n", httpconn);

	/* TODO: Remove this */
	httpconn->session = servconn->session;

	httpconn->servconn = servconn;

	httpconn->tx_buf = gaim_circ_buffer_new(MSN_BUF_LEN);
	httpconn->tx_handler = 0;

	return httpconn;
}

void
msn_httpconn_destroy(MsnHttpConn *httpconn)
{
	g_return_if_fail(httpconn != NULL);

	gaim_debug_info("msn", "destroy httpconn (%p)\n", httpconn);

	if (httpconn->connected)
		msn_httpconn_disconnect(httpconn);

	g_free(httpconn->full_session_id);

	g_free(httpconn->session_id);

	g_free(httpconn->host);

	gaim_circ_buffer_destroy(httpconn->tx_buf);
	if (httpconn->tx_handler > 0)
		gaim_input_remove(httpconn->tx_handler);

	g_free(httpconn);
}

static void
connect_cb(gpointer data, gint source)
{
	MsnHttpConn *httpconn = data;

	/*
	TODO: Need to do this in case the account is disabled while connecting
	if (!g_list_find(gaim_connections_get_all(), gc))
	{
		if (source >= 0)
			close(source);
		destroy_new_conn_data(new_conn_data);
		return;
	}
	*/

	httpconn->fd = source;

	if (source >= 0)
	{
		httpconn->inpa = gaim_input_add(httpconn->fd, GAIM_INPUT_READ,
			read_cb, data);

		httpconn->timer = gaim_timeout_add(2000, msn_httpconn_poll, httpconn);

		msn_httpconn_process_queue(httpconn);
	}
	else
	{
		gaim_debug_error("msn", "HTTP: Connection error\n");
		msn_servconn_got_error(httpconn->servconn, MSN_SERVCONN_ERROR_CONNECT);
	}
}

gboolean
msn_httpconn_connect(MsnHttpConn *httpconn, const char *host, int port)
{
	GaimProxyConnectInfo *connect_info;

	g_return_val_if_fail(httpconn != NULL, FALSE);
	g_return_val_if_fail(host     != NULL, FALSE);
	g_return_val_if_fail(port      > 0,    FALSE);

	if (httpconn->connected)
		msn_httpconn_disconnect(httpconn);

	connect_info = gaim_proxy_connect(httpconn->session->account,
		"gateway.messenger.hotmail.com", 80, connect_cb, NULL, httpconn);

	if (connect_info != NULL)
	{
		httpconn->waiting_response = TRUE;
		httpconn->connected = TRUE;
	}

	return httpconn->connected;
}

void
msn_httpconn_disconnect(MsnHttpConn *httpconn)
{
	g_return_if_fail(httpconn != NULL);

	if (!httpconn->connected)
		return;

	if (httpconn->timer)
		gaim_timeout_remove(httpconn->timer);

	httpconn->timer = 0;

	if (httpconn->inpa > 0)
	{
		gaim_input_remove(httpconn->inpa);
		httpconn->inpa = 0;
	}

	close(httpconn->fd);

	g_free(httpconn->rx_buf);
	httpconn->rx_buf = NULL;
	httpconn->rx_len = 0;

	httpconn->connected = FALSE;

	/* msn_servconn_disconnect(httpconn->servconn); */
}
