/**
 * @file httpmethod.c HTTP connection method
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
#include "debug.h"
#include "httpmethod.h"

#define GET_NEXT(tmp) \
	while (*(tmp) && *(tmp) != ' ' && *(tmp) != '\r') \
		(tmp)++; \
	if (*(tmp) != '\0') *(tmp)++ = '\0'; \
	if (*(tmp) == '\n') (tmp)++; \
	while (*(tmp) && *(tmp) == ' ') \
		(tmp)++

#define GET_NEXT_LINE(tmp) \
	while (*(tmp) && *(tmp) != '\r') \
		(tmp)++; \
	if (*(tmp) != '\0') *(tmp)++ = '\0'; \
	if (*(tmp) == '\n') (tmp)++

typedef struct
{
	MsnServConn *servconn;
	char *buffer;
	size_t size;
	const char *server_type;

} MsnHttpQueueData;

static gboolean
http_poll(gpointer data)
{
	MsnServConn *servconn = data;

	gaim_debug_info("msn", "Polling server %s.\n",
					servconn->http_data->gateway_ip);
	msn_http_servconn_poll(servconn);

	servconn->http_data->timer = 0;

	gaim_debug(GAIM_DEBUG_INFO, "msn", "Returning from http_poll\n");

	return FALSE;
}

static void
stop_timer(MsnServConn *servconn)
{
	if (servconn->http_data->timer)
	{
		gaim_debug(GAIM_DEBUG_INFO, "msn", "Stopping timer\n");
		gaim_timeout_remove(servconn->http_data->timer);
		servconn->http_data->timer = 0;
	}
}

static void
start_timer(MsnServConn *servconn)
{
	stop_timer(servconn);

	gaim_debug(GAIM_DEBUG_INFO, "msn", "Starting timer\n");
	servconn->http_data->timer = gaim_timeout_add(5000, http_poll, servconn);
}

size_t
msn_http_servconn_write(MsnServConn *servconn, const char *buf, size_t size,
						const char *server_type)
{
	size_t s, needed;
	char *params;
	char *temp;
	gboolean first;
	int res; /* result of the write operation */

	g_return_val_if_fail(servconn != NULL, 0);
	g_return_val_if_fail(buf      != NULL, 0);
	g_return_val_if_fail(size      > 0,    0);
	g_return_val_if_fail(servconn->http_data != NULL, 0);

	if (servconn->http_data->waiting_response ||
		servconn->http_data->queue != NULL)
	{
		MsnHttpQueueData *queue_data = g_new0(MsnHttpQueueData, 1);

		queue_data->servconn    = servconn;
		queue_data->buffer      = g_strdup(buf);
		queue_data->size        = size;
		queue_data->server_type = server_type;

		servconn->http_data->queue =
			g_list_append(servconn->http_data->queue, queue_data);

		return size;
	}

	first = servconn->http_data->virgin;

	if (first)
	{
		if (server_type)
		{
			params = g_strdup_printf("Action=open&Server=%s&IP=%s",
									 server_type,
									 servconn->http_data->gateway_ip);
		}
		else
		{
			params = g_strdup_printf("Action=open&IP=%s",
									 servconn->http_data->gateway_ip);
		}
	}
	else
	{
		params = g_strdup_printf("SessionID=%s",
								 servconn->http_data->session_id);
	}

	temp = g_strdup_printf(
		"POST http://%s/gateway/gateway.dll?%s HTTP/1.1\r\n"
		"Accept: */*\r\n"
		"Accept-Language: en-us\r\n"
		"User-Agent: MSMSGS\r\n"
		"Host: %s\r\n"
		"Proxy-Connection: Keep-Alive\r\n"
		"Connection: Keep-Alive\r\n"
		"Pragma: no-cache\r\n"
		"Content-Type: application/x-msn-messenger\r\n"
		"Content-Length: %d\r\n"
		"\r\n"
		"%s",
		((strcmp(server_type, "SB") == 0) && first
		 ? "gateway.messenger.hotmail.com"
		 : servconn->http_data->gateway_ip),
		params,
		servconn->http_data->gateway_ip,
		(int)size,
		buf);

	g_free(params);

#if 1
	gaim_debug_misc("msn", "Writing HTTP to fd %d: {%s}\n",
					servconn->fd, temp);
#endif

	s = 0;
	needed = strlen(temp);

	do {
		res = write(servconn->fd, temp, needed);
		if (res >= 0)
			s += res;
		else if (errno != EAGAIN) {
			char *msg = g_strdup_printf("Unable to write to MSN server via HTTP (error %d)", errno);
			gaim_connection_error(servconn->session->account->gc, msg);
			g_free(msg);
			return -1;
		}
	} while (s < needed);

	g_free(temp);

	servconn->http_data->waiting_response = TRUE;

	servconn->http_data->virgin = FALSE;

	stop_timer(servconn);

	return s;
}

void
msn_http_servconn_poll(MsnServConn *servconn)
{
	size_t s;
	char *temp;

	g_return_if_fail(servconn != NULL);
	g_return_if_fail(servconn->http_data != NULL);

	if (servconn->http_data->waiting_response ||
		servconn->http_data->queue != NULL)
	{
		return;
	}

	temp = g_strdup_printf(
		"POST http://%s/gateway/gateway.dll?Action=poll&SessionID=%s HTTP/1.1\r\n"
		"Accept: */*\r\n"
		"Accept-Language: en-us\r\n"
		"User-Agent: MSMSGS\r\n"
		"Host: %s\r\n"
		"Proxy-Connection: Keep-Alive\r\n"
		"Connection: Keep-Alive\r\n"
		"Pragma: no-cache\r\n"
		"Content-Type: application/x-msn-messenger\r\n"
		"Content-Length: 0\r\n"
		"\r\n",
		servconn->http_data->gateway_ip,
		servconn->http_data->session_id,
		servconn->http_data->gateway_ip);

	gaim_debug_misc("msn", "Writing to HTTP: {%s}\n", temp);

	s = write(servconn->fd, temp, strlen(temp));

	g_free(temp);

	servconn->http_data->waiting_response = TRUE;

	stop_timer(servconn);

	if (s <= 0)
		gaim_connection_error(servconn->session->account->gc,
							  _("Write error"));
}

gboolean
msn_http_servconn_parse_data(MsnServConn *servconn, const char *buf,
							 size_t size, char **ret_buf, size_t *ret_size,
							 gboolean *error)
{
	GaimConnection *gc;
	const char *s, *c;
	char *headers, *body;
	char *tmp;
	size_t len = 0;

	g_return_val_if_fail(servconn != NULL, FALSE);
	g_return_val_if_fail(buf      != NULL, FALSE);
	g_return_val_if_fail(size      > 0,    FALSE);
	g_return_val_if_fail(ret_buf  != NULL, FALSE);
	g_return_val_if_fail(ret_size != NULL, FALSE);
	g_return_val_if_fail(error    != NULL, FALSE);

	gaim_debug_info("msn", "parsing data {%s} from fd %d\n", buf, servconn->fd);
	servconn->http_data->waiting_response = FALSE;

	gc = gaim_account_get_connection(servconn->session->account);

	/* Healthy defaults. */
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

			return TRUE;
		}

		buf = s;
		size -= (s - buf);
	}

	if ((s = strstr(buf, "\r\n\r\n")) == NULL)
		return FALSE;

	headers = g_strndup(buf, s - buf);
	s += 4; /* Skip \r\n */
	body = g_strndup(s, size - (s - buf));

	gaim_debug_misc("msn", "Incoming HTTP buffer: {%s\r\n%s}", headers, body);

	if ((s = strstr(headers, "Content-Length: ")) != NULL)
	{
		s += strlen("Content-Length: ");

		if ((c = strchr(s, '\r')) == NULL)
		{
			g_free(headers);
			g_free(body);

			return FALSE;
		}

		tmp = g_strndup(s, c - s);
		len = atoi(tmp);
		g_free(tmp);

		if (strlen(body) != len)
		{
			g_free(headers);
			g_free(body);

			gaim_debug_warning("msn",
							   "body length (%d) != content length (%d)\n",
							   strlen(body), len);
			return FALSE;
		}
	}

	/* Now we should be able to process the data. */
	if ((s = strstr(headers, "X-MSN-Messenger: ")) != NULL)
	{
		char *session_id, *gw_ip;
		char *c2, *s2;

		s += strlen("X-MSN-Messenger: ");

		if ((c = strchr(s, '\r')) == NULL)
		{
			gaim_connection_error(gc, "Malformed X-MSN-Messenger field.");
			return FALSE;
		}

		tmp = g_strndup(s, c - s);

		/* Find the value for the Session ID */
		if ((s2 = strchr(tmp, '=')) == NULL)
		{
			gaim_connection_error(gc, "Malformed X-MSN-Messenger field.");
			return FALSE;
		}

		s2++;

		/* Terminate the ; so we can g_strdup it. */
		if ((c2 = strchr(s2, ';')) == NULL)
		{
			gaim_connection_error(gc, "Malformed X-MSN-Messenger field.");
			return FALSE;
		}

		*c2 = '\0';
		c2++;

		/* Now grab that session ID. */
		session_id = g_strdup(s2);

		/* Continue to the gateway IP */
		if ((s2 = strchr(c2, '=')) == NULL)
		{
			gaim_connection_error(gc, "Malformed X-MSN-Messenger field.");
			return FALSE;
		}

		s2++;

		/* Grab the gateway IP */
		gw_ip = g_strdup(s2);

		g_free(tmp);

		/* Set the new data. */
		if (servconn->http_data->session_id != NULL)
			g_free(servconn->http_data->session_id);

		if (servconn->http_data->old_gateway_ip != NULL)
			g_free(servconn->http_data->old_gateway_ip);

		servconn->http_data->old_gateway_ip = servconn->http_data->gateway_ip;

		servconn->http_data->session_id = session_id;
		servconn->http_data->gateway_ip = gw_ip;
	}

	g_free(headers);

	*ret_buf  = body;
	*ret_size = len;

	if (servconn->http_data->queue != NULL)
	{
		MsnHttpQueueData *queue_data;

		queue_data = (MsnHttpQueueData *)servconn->http_data->queue->data;

		servconn->http_data->queue =
			g_list_remove(servconn->http_data->queue, queue_data);

		msn_http_servconn_write(queue_data->servconn,
								queue_data->buffer,
								queue_data->size,
								queue_data->server_type);

		g_free(queue_data->buffer);
		g_free(queue_data);
	}
	else
		start_timer(servconn);

	return TRUE;
}

