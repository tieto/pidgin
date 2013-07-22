/**
 * @file httpconn.h HTTP-tunnelled connections
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#include "httpconn.h"

#include "debug.h"
#include "http.h"

#include "msn.h"

typedef struct _MsnHttpConnWriteQueueElement MsnHttpConnWriteQueueElement;

struct _MsnHttpConn
{
	MsnServConn *servconn;

	gboolean connected;
	gboolean is_disconnecting;
	gboolean is_externally_disconnected;

	gchar *host_dest;
	gchar *host_gw;
	int port;

	gchar *session_id; /* changes after every request */

	PurpleHttpConnection *current_request;
	int polling_timer;
	int finish_timer;
	PurpleHttpKeepalivePool *keepalive_pool;
	GSList *write_queue;
};

struct _MsnHttpConnWriteQueueElement
{
	gchar *data;
	size_t len;
};

static MsnHttpConnWriteQueueElement *
msn_httpconn_writequeueelement_new(const gchar *data, size_t len)
{
	MsnHttpConnWriteQueueElement *wqe;

	g_return_val_if_fail(!(len > 0 && data == NULL), NULL);

	wqe = g_new(MsnHttpConnWriteQueueElement, 1);
	wqe->data = g_memdup(data, len);
	wqe->len = len;

	return wqe;
}

static void
msn_httpconn_writequeueelement_free(MsnHttpConnWriteQueueElement *wqe)
{
	g_free(wqe->data);
	g_free(wqe);
}

static void
msn_httpconn_writequeueelement_process(MsnHttpConn *httpconn)
{
	MsnHttpConnWriteQueueElement *wqe;

	g_return_if_fail(httpconn != NULL);

	if (httpconn->current_request != NULL || httpconn->finish_timer > 0)
	{
		if (purple_debug_is_verbose()) {
			purple_debug_misc("msn", "cannot process queue: "
				"there is a request already running\n");
		}
		return;
	}
	if (httpconn->write_queue == NULL)
		return;

	wqe = httpconn->write_queue->data;
	httpconn->write_queue = g_slist_remove(httpconn->write_queue, wqe);
	msn_httpconn_write(httpconn, wqe->data, wqe->len);
	msn_httpconn_writequeueelement_free(wqe);
}

MsnHttpConn *
msn_httpconn_new(MsnServConn *servconn)
{
	MsnHttpConn *httpconn;

	g_return_val_if_fail(servconn != NULL, NULL);

	httpconn = g_new0(MsnHttpConn, 1);

	httpconn->servconn = servconn;

	return httpconn;
}

void
msn_httpconn_destroy(MsnHttpConn *httpconn)
{
	if (httpconn == NULL)
		return;

	if (httpconn->connected)
		msn_httpconn_disconnect(httpconn);

	g_free(httpconn);
}

static gboolean
msn_httpconn_poll(gpointer _httpconn)
{
	MsnHttpConn *httpconn = _httpconn;

	if (!httpconn->connected) {
		purple_debug_error("msn", "msn_httpconn_poll: invalid state\n");
		return FALSE;
	}

	/* There's no need to poll if the session is not fully established. */
	if (!httpconn->session_id)
		return TRUE;

	/* There's no need to poll if we're already waiting for a response. */
	if (httpconn->current_request || httpconn->finish_timer > 0)
		return TRUE;

	if (httpconn->write_queue) {
		purple_debug_warning("msn", "There are pending packets, but no "
			"request running\n");
		return TRUE;
	}

	msn_httpconn_write(httpconn, NULL, 0);

	return TRUE;
}

void
msn_httpconn_connect(MsnHttpConn *httpconn, const gchar *host, int port)
{
	g_return_if_fail(httpconn != NULL);
	g_return_if_fail(host != NULL);

	if (httpconn->connected) {
		purple_debug_warning("msn", "http already (virtually) "
			"connected\n");
		return;
	}

	httpconn->host_dest = g_strdup(host);
	httpconn->port = port;
	httpconn->connected = TRUE;

	httpconn->polling_timer = purple_timeout_add_seconds(2,
		msn_httpconn_poll, httpconn);
	httpconn->keepalive_pool = purple_http_keepalive_pool_new();

	msn_httpconn_writequeueelement_process(httpconn);
}

void
msn_httpconn_disconnect(MsnHttpConn *httpconn)
{
	g_return_if_fail(httpconn != NULL);

	if (!httpconn->connected) {
		purple_debug_warning("msn", "http is not connected\n");
		return;
	}
	httpconn->connected = FALSE;
	httpconn->is_disconnecting = TRUE;

	g_slist_free_full(httpconn->write_queue,
		(GDestroyNotify)msn_httpconn_writequeueelement_free);
	httpconn->write_queue = NULL;

	if (httpconn->polling_timer > 0) {
		purple_timeout_remove(httpconn->polling_timer);
		httpconn->polling_timer = 0;
	}

	if (httpconn->finish_timer > 0) {
		purple_timeout_remove(httpconn->finish_timer);
		httpconn->finish_timer = 0;
	}

	if (httpconn->current_request != NULL) {
		purple_http_conn_cancel(httpconn->current_request);
		httpconn->current_request = NULL;
	}

	g_free(httpconn->host_dest);
	httpconn->host_dest = NULL;
	g_free(httpconn->host_gw);
	httpconn->host_gw = NULL;
	g_free(httpconn->session_id);
	httpconn->session_id = NULL;

	purple_http_keepalive_pool_unref(httpconn->keepalive_pool);
	httpconn->keepalive_pool = NULL;
	httpconn->is_disconnecting = FALSE;
}

static void
msn_httpconn_parse_header(MsnHttpConn *httpconn, const gchar *header)
{
	gchar *session_id = NULL, *gw_ip = NULL, *session_action = NULL;
	gchar **elems;
	int i;

	elems = g_strsplit(header, ";", 10);

	for (i = 0; elems[i] != NULL; i++) {
		gchar **tokens = g_strsplit(elems[i], "=", 2);
		if (tokens[0] == NULL || tokens[1] == NULL) {
			g_strfreev(tokens);
			continue;
		}

		g_strstrip(tokens[0]);
		g_strstrip(tokens[1]);

		if (g_strcmp0(tokens[0], "SessionID") == 0) {
			g_free(session_id);
			session_id = g_strdup(tokens[1]);
		} else if (g_strcmp0(tokens[0], "GW-IP") == 0) {
			g_free(gw_ip);
			gw_ip = g_strdup(tokens[1]);
		} else if (g_strcmp0(tokens[0], "Session") == 0) {
			g_free(session_action);
			session_action = g_strdup(tokens[1]);
		}

		g_strfreev(tokens);
	}

	g_strfreev(elems);

	if (session_id != NULL) {
		g_free(httpconn->session_id);
		httpconn->session_id = session_id;
	}

	if (gw_ip != NULL) {
		g_free(httpconn->host_gw);
		httpconn->host_gw = gw_ip;
	}

	if (g_strcmp0(session_action, "close") == 0) {
		g_free(httpconn->session_id);
		httpconn->session_id = NULL;
		g_free(httpconn->host_gw);
		httpconn->host_gw = NULL;
		httpconn->is_externally_disconnected = TRUE;
	}
	g_free(session_action);
}

static gboolean
msn_httpconn_read_finish(gpointer _httpconn)
{
	MsnHttpConn *httpconn = _httpconn;

	httpconn->finish_timer = 0;

	if (httpconn->servconn->rx_len > 0) {
		if (msn_servconn_process_data(httpconn->servconn) == NULL)
			return FALSE;
	}

	msn_httpconn_writequeueelement_process(httpconn);

	return FALSE;
}

static void
msn_httpconn_read(PurpleHttpConnection *phc, PurpleHttpResponse *response,
	gpointer _httpconn)
{
	MsnHttpConn *httpconn = _httpconn;
	const gchar *msn_header;
	const gchar *got_data;
	size_t got_len;

	if (!purple_http_response_is_successfull(response)) {
		httpconn->current_request = NULL;
		msn_servconn_got_error(httpconn->servconn,
			MSN_SERVCONN_ERROR_READ,
			purple_http_response_get_error(response));
		return;
	}

	if (httpconn->servconn->type == MSN_SERVCONN_NS) {
		purple_connection_update_last_received(
			purple_account_get_connection(httpconn->servconn->
				session->account));
	}

	msn_header = purple_http_response_get_header(response,
		"X-MSN-Messenger");
	if (msn_header != NULL)
		msn_httpconn_parse_header(httpconn, msn_header);

	got_data = purple_http_response_get_data(response, &got_len);

	g_free(httpconn->servconn->rx_buf);
	httpconn->servconn->rx_buf = g_memdup(got_data, got_len);
	httpconn->servconn->rx_len = got_len;

	httpconn->current_request = NULL;
	httpconn->finish_timer = purple_timeout_add(0, msn_httpconn_read_finish,
		httpconn);
}

ssize_t
msn_httpconn_write(MsnHttpConn *httpconn, const gchar *data, size_t data_len)
{
	PurpleAccount *account;
	PurpleHttpRequest *req;
	static const gchar *server_types[] = { "NS", "SB" };
	const gchar *server_type;
	const gchar *host;
	gchar *params;

	g_return_val_if_fail(httpconn != NULL, -1);
	g_return_val_if_fail(!(data == NULL && data_len > 0), -1);

	if (httpconn->is_disconnecting) {
		purple_debug_warning("msn", "http connection is in "
			"disconnecting state\n");
		return -1;
	}

	if (httpconn->is_externally_disconnected) {
		purple_debug_warning("msn", "http connection was externally "
			"disconnected\n");
		return -1;
	}

	if (httpconn->current_request != NULL) {
		httpconn->write_queue = g_slist_append(httpconn->write_queue,
			msn_httpconn_writequeueelement_new(data, data_len));
		return data_len;
	}

	server_type = server_types[httpconn->servconn->type];

	if (httpconn->session_id == NULL || httpconn->host_gw == NULL) {
		/* I'm not sure, if this shouldn't be MSN_HTTPCONN_SERVER */
		host = httpconn->host_dest;
		params = g_strdup_printf("Action=open&Server=%s&IP=%s",
			server_type, httpconn->host_dest);
	} else {
		host = httpconn->host_gw;
		params = g_strdup_printf("SessionID=%s", httpconn->session_id);
	}

	if (data_len == 0) {
		gchar *tmp = params;
		params = g_strdup_printf("Action=poll&%s", params);
		g_free(tmp);
	}

	account = httpconn->servconn->session->account;

	req = purple_http_request_new(NULL);
	purple_http_request_set_keepalive_pool(req, httpconn->keepalive_pool);
	purple_http_request_set_url_printf(req,
		"http://%s/gateway/gateway.dll?%s", host, params);
	purple_http_request_set_method(req, "POST");
	purple_http_request_header_set(req, "Accept-Language", "en-us");
	purple_http_request_header_set(req, "User-Agent", "MSMSGS");
	purple_http_request_header_set(req, "Pragma", "no-cache");
	purple_http_request_header_set(req, "Content-Type",
		"application/x-msn-messenger");
	purple_http_request_set_contents(req, data, data_len);
	httpconn->current_request = purple_http_request(
		purple_account_get_connection(account), req,
		msn_httpconn_read, httpconn);
	purple_http_request_unref(req);

	g_free(params);

	return data_len;
}
