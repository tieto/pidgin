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
	char *data;
	size_t size;

} MsnHttpQueueData;

static void read_cb(gpointer data, gint source, GaimInputCondition cond);
void msn_httpconn_process_queue(MsnHttpConn *httpconn);
gboolean msn_httpconn_parse_data(MsnHttpConn *httpconn, const char *buf,
								 size_t size, char **ret_buf, size_t *ret_size,
								 gboolean *error);

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

	return httpconn;
}

void
msn_httpconn_destroy(MsnHttpConn *httpconn)
{
	g_return_if_fail(httpconn != NULL);

	gaim_debug_info("msn", "destroy httpconn (%p)\n", httpconn);

	if (httpconn->connected)
		msn_httpconn_disconnect(httpconn);

	g_free(httpconn);
}

static ssize_t
write_raw(MsnHttpConn *httpconn, const char *header,
		  const char *body, size_t body_len)
{
	char *buf;
	size_t buf_len;

	ssize_t s;
	ssize_t res; /* result of the write operation */

#ifdef MSN_DEBUG_HTTP
	gaim_debug_misc("msn", "Writing HTTP (header): {%s}\n", header);
#endif

	buf = g_strdup_printf("%s\r\n", header);
	buf_len = strlen(buf);

	if (body != NULL)
	{
		buf = g_realloc(buf, buf_len + body_len);
		memcpy(buf + buf_len, body, body_len);
		buf_len += body_len;
	}

	s = 0;

	do
	{
		res = write(httpconn->fd, buf, buf_len);
		if (res >= 0)
		{
			s += res;
		}
		else if (errno != EAGAIN)
		{
			msn_servconn_got_error(httpconn->servconn, MSN_SERVCONN_ERROR_WRITE);
			return -1;
		}
	} while (s < buf_len);

	g_free(buf);

	return s;
}

void
msn_httpconn_poll(MsnHttpConn *httpconn)
{
	char *header;
	int r;

	g_return_if_fail(httpconn != NULL);

	if (httpconn->waiting_response ||
		httpconn->queue != NULL)
	{
		return;
	}

	header = g_strdup_printf(
		"POST http://%s/gateway/gateway.dll?Action=poll&SessionID=%s HTTP/1.1\r\n"
		"Accept: */*\r\n"
		"Accept-Language: en-us\r\n"
		"User-Agent: MSMSGS\r\n"
		"Host: %s\r\n"
		"Proxy-Connection: Keep-Alive\r\n"
		"Connection: Keep-Alive\r\n"
		"Pragma: no-cache\r\n"
		"Content-Type: application/x-msn-messenger\r\n"
		"Content-Length: 0\r\n",
		httpconn->host,
		httpconn->full_session_id,
		httpconn->host);

	r = write_raw(httpconn, header, NULL, -1);

	g_free(header);

	if (r > 0)
	{
		httpconn->waiting_response = TRUE;
		httpconn->dirty = FALSE;
	}
}

static gboolean
poll(gpointer data)
{
	MsnHttpConn *httpconn;

	httpconn = data;

#if 0
	gaim_debug_info("msn", "polling from %s\n", httpconn->session_id);
#endif

	if (httpconn->dirty)
		msn_httpconn_poll(httpconn);

	return TRUE;
}

static void
connect_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnHttpConn *httpconn = data;

	httpconn->fd = source;

	if (source > 0)
	{
		httpconn->inpa = gaim_input_add(httpconn->fd, GAIM_INPUT_READ,
										read_cb, data);

		httpconn->timer = gaim_timeout_add(2000, poll, httpconn);

		httpconn->waiting_response = FALSE;
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
	int r;

	g_return_val_if_fail(httpconn != NULL, FALSE);
	g_return_val_if_fail(host     != NULL, FALSE);
	g_return_val_if_fail(port      > 0,    FALSE);

	if (httpconn->connected)
		msn_httpconn_disconnect(httpconn);

	r = gaim_proxy_connect(httpconn->session->account,
						   "gateway.messenger.hotmail.com", 80, connect_cb,
						   httpconn);

	if (r == 0)
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

	httpconn->rx_buf = NULL;
	httpconn->rx_len = 0;

	httpconn->connected = FALSE;

	/* msn_servconn_disconnect(httpconn->servconn); */
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
	gboolean error;

	httpconn = data;
	servconn = NULL;
	session = httpconn->session;

	len = read(httpconn->fd, buf, sizeof(buf) - 1);

	if (len <= 0)
	{
		gaim_debug_error("msn", "HTTP: Read error\n");
		msn_servconn_got_error(httpconn->servconn, MSN_SERVCONN_ERROR_READ);
		msn_httpconn_disconnect(httpconn);

		return;
	}

	buf[len] = '\0';

	httpconn->rx_buf = g_realloc(httpconn->rx_buf, len + httpconn->rx_len + 1);
	memcpy(httpconn->rx_buf + httpconn->rx_len, buf, len + 1);
	httpconn->rx_len += len;

	if (!msn_httpconn_parse_data(httpconn, httpconn->rx_buf, httpconn->rx_len,
								 &result_msg, &result_len, &error))
	{
		/* We must wait for more input */

		return;
	}

	httpconn->servconn->processing = FALSE;

	servconn = httpconn->servconn;

	if (error)
	{
		gaim_debug_error("msn", "HTTP: Special error\n");
		msn_servconn_got_error(httpconn->servconn, MSN_SERVCONN_ERROR_READ);
		msn_httpconn_disconnect(httpconn);

		return;
	}

	if (result_len == 0)
	{
		/* Nothing to do here */
#if 0
		gaim_debug_info("msn", "HTTP: nothing to do here\n");
#endif
		g_free(result_msg);
		g_free(httpconn->rx_buf);
		httpconn->rx_buf = NULL;
		httpconn->rx_len = 0;
		return;
	}

	g_free(httpconn->rx_buf);
	httpconn->rx_buf = NULL;
	httpconn->rx_len = 0;

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

void
msn_httpconn_process_queue(MsnHttpConn *httpconn)
{
	if (httpconn->queue != NULL)
	{
		MsnHttpQueueData *queue_data;

		queue_data = (MsnHttpQueueData *)httpconn->queue->data;

		httpconn->queue = g_list_remove(httpconn->queue, queue_data);

		msn_httpconn_write(queue_data->httpconn,
						   queue_data->data,
						   queue_data->size);

		g_free(queue_data->data);
		g_free(queue_data);
	}
	else
	{
		httpconn->dirty = TRUE;
	}
}

size_t
msn_httpconn_write(MsnHttpConn *httpconn, const char *data, size_t size)
{
	char *params;
	char *header;
	gboolean first;
	const char *server_types[] = { "NS", "SB" };
	const char *server_type;
	size_t r; /* result of the write operation */
	char *host;
	MsnServConn *servconn;

	/* TODO: remove http data from servconn */

	g_return_val_if_fail(httpconn != NULL, 0);
	g_return_val_if_fail(data     != NULL, 0);
	g_return_val_if_fail(size      > 0,    0);

	servconn = httpconn->servconn;

	if (httpconn->waiting_response)
	{
		MsnHttpQueueData *queue_data = g_new0(MsnHttpQueueData, 1);

		queue_data->httpconn = httpconn;
		queue_data->data     = g_memdup(data, size);
		queue_data->size     = size;

		httpconn->queue = g_list_append(httpconn->queue, queue_data);
		/* httpconn->dirty = TRUE; */

		/* servconn->processing = TRUE; */

		return size;
	}

	first = httpconn->virgin;
	server_type = server_types[servconn->type];

	if (first)
	{
		host = "gateway.messenger.hotmail.com";

		/* The first time servconn->host is the host we should connect to. */
		params = g_strdup_printf("Action=open&Server=%s&IP=%s",
								 server_type,
								 servconn->host);
	}
	else
	{
		/* The rest of the times servconn->host is the gateway host. */
		host = httpconn->host;

		params = g_strdup_printf("SessionID=%s",
								 httpconn->full_session_id);
	}

	header = g_strdup_printf(
		"POST http://%s/gateway/gateway.dll?%s HTTP/1.1\r\n"
		"Accept: */*\r\n"
		"Accept-Language: en-us\r\n"
		"User-Agent: MSMSGS\r\n"
		"Host: %s\r\n"
		"Proxy-Connection: Keep-Alive\r\n"
		"Connection: Keep-Alive\r\n"
		"Pragma: no-cache\r\n"
		"Content-Type: application/x-msn-messenger\r\n"
		"Content-Length: %d\r\n",
		host,
		params,
		host,
		(int)size);

	g_free(params);

	r = write_raw(httpconn, header, data, size);

	g_free(header);

	if (r > 0)
	{
		httpconn->virgin = FALSE;
		httpconn->waiting_response = TRUE;
		httpconn->dirty = FALSE;
	}

	return r;
}

gboolean
msn_httpconn_parse_data(MsnHttpConn *httpconn, const char *buf,
						size_t size, char **ret_buf, size_t *ret_size,
						gboolean *error)
{
	GaimConnection *gc;
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

	httpconn->waiting_response = FALSE;

	gc = gaim_account_get_connection(httpconn->session->account);

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
		return FALSE;

	header = g_strndup(buf, s - buf);
	s += 4; /* Skip \r\n */
	body_start = s;
	body_len = size - (body_start - buf);

	if ((s = strstr(header, "Content-Length: ")) != NULL)
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
	if ((s = strstr(header, "X-MSN-Messenger: ")) != NULL)
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
			if (httpconn->full_session_id != NULL);
				g_free(httpconn->full_session_id);

			httpconn->full_session_id = full_session_id;

			if (httpconn->session_id != NULL);
				g_free(httpconn->session_id);

			httpconn->session_id = session_id;

			if (httpconn->host != NULL);
				g_free(httpconn->host);

			httpconn->host = gw_ip;
		}
		else
		{
			MsnServConn *servconn;

			/* It's going to die. */

			servconn = httpconn->servconn;

			if (servconn != NULL)
				servconn->wasted = TRUE;

			g_free(full_session_id);
			g_free(gw_ip);
		}
	}

	g_free(header);

	*ret_buf  = body;
	*ret_size = body_len;

	msn_httpconn_process_queue(httpconn);

	return TRUE;
}
