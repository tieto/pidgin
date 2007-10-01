/**
 * @file soap2.c
 * 	C file for SOAP connection related process
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "soap2.h"

#include "session.h"

#include "debug.h"
#include "xmlnode.h"

#include <glib.h>
#include <error.h>

static GHashTable *conn_table = NULL;

typedef struct _MsnSoapRequest {
	char *path;
	MsnSoapMessage *message;
	MsnSoapCallback cb;
	gpointer cb_data;
} MsnSoapRequest;

typedef struct _MsnSoapConnection {
	MsnSession *session;
	char *host;

	PurpleSslConnection *ssl;
	gboolean connected;

	guint event_handle;
	GString *buf;
	gsize handled_len;
	gsize body_len;
	int response_code;
	gboolean headers_done;
	gboolean close_when_done;

	MsnSoapMessage *message;

	GQueue *queue;
	MsnSoapRequest *current_request;
} MsnSoapConnection;

static void msn_soap_connection_destroy_foreach_cb(gpointer item, gpointer data);
static gboolean msn_soap_connection_run(gpointer data);

static MsnSoapConnection *msn_soap_connection_new(MsnSession *session,
	const char *host);
static void msn_soap_connection_handle_next(MsnSoapConnection *conn);
static void msn_soap_connection_destroy(MsnSoapConnection *conn);

static void msn_soap_message_send_internal(MsnSession *session,
	MsnSoapMessage *message, const char *host, const char *path,
	MsnSoapCallback cb, gpointer cb_data, gboolean first);

static void msn_soap_request_destroy(MsnSoapRequest *req);
static void msn_soap_connection_sanitize(MsnSoapConnection *conn, gboolean disconnect);


static MsnSoapConnection *
msn_soap_get_connection(MsnSession *session, const char *host)
{
	MsnSoapConnection *conn = NULL;

	if (session->soap_table) {
		conn = g_hash_table_lookup(conn_table, host);
	} else {
		session->soap_table = g_hash_table_new_full(g_str_hash, g_str_equal,
			NULL, (GDestroyNotify)msn_soap_connection_destroy);
	}

	if (conn == NULL) {
		conn = msn_soap_connection_new(session, host);
		g_hash_table_insert(session->soap_table, conn->host, conn);
	}

	return conn;
}

static MsnSoapConnection *
msn_soap_connection_new(MsnSession *session, const char *host)
{
	MsnSoapConnection *conn = g_new0(MsnSoapConnection, 1);
	conn->session = session;
	conn->host = g_strdup(host);
	conn->queue = g_queue_new();
	return conn;
}

static void
msn_soap_connected_cb(gpointer data, PurpleSslConnection *ssl,
		PurpleInputCondition cond)
{
	MsnSoapConnection *conn = data;

	conn->connected = TRUE;

	if (conn->event_handle == 0)
		conn->event_handle = g_idle_add(msn_soap_connection_run, conn);
}

static void
msn_soap_error_cb(PurpleSslConnection *ssl, PurpleSslErrorType error,
		gpointer data)
{
	MsnSoapConnection *conn = data;

	g_hash_table_remove(conn->session->soap_table, conn->host);
}

static gboolean
msn_soap_handle_redirect(MsnSoapConnection *conn, const char *url)
{
	char *c;

	/* Skip the http:// */
	if ((c = strchr(url, '/')) != NULL)
		url += 2;

	if ((c = strchr(url, '/')) != NULL) {
		char *host, *path;

		host = g_strndup(url, c - url);
		path = g_strdup(c);

		msn_soap_message_send_internal(conn->session,
			conn->current_request->message,	host, path,
			conn->current_request->cb, conn->current_request->cb_data, TRUE);

		msn_soap_request_destroy(conn->current_request);
		conn->current_request = NULL;

		g_free(host);
		g_free(path);

		return TRUE;
	}

	return FALSE;
}

static gboolean
msn_soap_handle_body(MsnSoapConnection *conn, MsnSoapMessage *response)
{
	xmlnode *node = response->xml;

	if (strcmp(node->name, "Envelope") == 0 &&
		node->child && strcmp(node->child->name, "Header") == 0 &&
		node->child->next) {
		xmlnode *body = node->child->next;
		MsnSoapRequest *request;

		if (strcmp(body->name, "Fault") == 0) {
			xmlnode *fault = xmlnode_get_child(body, "faultcode");

			if (fault != NULL) {
				char *faultdata = xmlnode_get_data(fault);

				if (strcmp(faultdata, "psf:Redirect") == 0) {
					xmlnode *url = xmlnode_get_child(body, "redirectUrl");

					if (url) {
						char *urldata = xmlnode_get_data(url);
						msn_soap_handle_redirect(conn, urldata);
						g_free(urldata);
					}

					g_free(faultdata);
					return TRUE;
				} else if (strcmp(faultdata, "wsse:FailedAuthentication") == 0) {
					xmlnode *reason = xmlnode_get_child(body, "faultstring");
					char *reasondata = xmlnode_get_data(reason);

					msn_soap_connection_sanitize(conn, TRUE);
					msn_session_set_error(conn->session, MSN_ERROR_AUTH,
						reasondata);

					g_free(reasondata);
					g_free(faultdata);
					return FALSE;
				}

				g_free(faultdata);
			}
		}

		request = conn->current_request;
		conn->current_request = NULL;
		request->cb(request->message, response,
			request->cb_data);
		msn_soap_request_destroy(request);
	}

	return TRUE;
}

static void
msn_soap_read_cb(gpointer data, gint fd, PurpleInputCondition cond)
{
    MsnSoapConnection *conn = data;
	int count;
	char buf[8192];
	char *linebreak;
	char *cursor;
	gboolean handled = FALSE;

	g_return_if_fail(cond == PURPLE_INPUT_READ);

	count = purple_ssl_read(conn->ssl, buf, sizeof(buf));
	purple_debug_info("soap", "read %d bytes\n", count);
	if (count < 0 && errno == EAGAIN)
		return;
	else if (count <= 0) {
		purple_debug_info("soap", "read: %s\n", strerror(errno));
		purple_ssl_close(conn->ssl);
		conn->ssl = NULL;
		msn_soap_connection_handle_next(conn);
		return;
	}

	if (conn->message == NULL) {
		conn->message = msn_soap_message_new(NULL, NULL);
	}

	if (conn->buf == NULL) {
		conn->buf = g_string_new_len(buf, count);
	} else {
		g_string_append_len(conn->buf, buf, count);
	}

	purple_debug_info("soap", "current %s\n", conn->buf->str);

	cursor = conn->buf->str + conn->handled_len;

	if (conn->headers_done) {
		if (conn->buf->len - conn->handled_len >= 
			conn->body_len) {
			xmlnode *node = xmlnode_from_str(cursor, conn->body_len);

			if (node == NULL) {
				purple_debug_info("soap", "Malformed SOAP response: %s\n",
					cursor);
			} else {
				MsnSoapMessage *message = conn->message;
				conn->message = NULL;
				message->xml = node;

				if (!msn_soap_handle_body(conn, message))
					return;
			}

			msn_soap_connection_handle_next(conn);
		}

		return;
	}

	while ((linebreak = strstr(cursor, "\r\n"))	!= NULL) {
		conn->handled_len = linebreak - conn->buf->str + 2;

		if (conn->response_code == 0) {
			if (sscanf(cursor, "HTTP/1.1 %d", &conn->response_code) != 1) {
				/* something horribly wrong */
				purple_ssl_close(conn->ssl);
				conn->ssl = NULL;
				msn_soap_connection_handle_next(conn);
				handled = TRUE;
				break;
			} else if (conn->response_code == 503) {
				msn_soap_connection_sanitize(conn, TRUE);
				msn_session_set_error(conn->session, MSN_ERROR_SERV_UNAVAILABLE, NULL);
				return;
			}
		} else if (cursor == linebreak) {
			/* blank line */
			conn->headers_done = TRUE;
		} else {
			char *line = g_strndup(cursor, linebreak - cursor);
			char *sep = strstr(line, ": ");
			char *key = line;
			char *value;

			if (sep == NULL) {
				purple_debug_info("soap", "ignoring malformed line: %s\n", line);
				g_free(line);
				goto loop_end;
			}

			value = sep + 2;
			*sep = '\0';
			msn_soap_message_add_header(conn->message, key, value);
			purple_debug_info("soap", "header %s: %s\n", key, value);

			if ((conn->response_code == 301 || conn->response_code == 300)
				&& strcmp(key, "Location") == 0) {

				msn_soap_handle_redirect(conn, value);

				handled = TRUE;
				g_free(line);
				break;
			} else if (conn->response_code == 401 &&
				strcmp(key, "WWW-Authenticate") == 0) {
				char *error = strstr(value, "cbtxt=");

				if (error) {
					error += strlen("cbtxt=");
				}

				msn_soap_connection_sanitize(conn, TRUE);
				msn_session_set_error(conn->session, MSN_ERROR_AUTH,
					error ? purple_url_decode(error) : NULL);

				g_free(line);
				return;
			} else if (strcmp(key, "Content-Length") == 0) {
				conn->body_len = atoi(value);
			} else if (strcmp(key, "Connection") == 0) {
				if (strcmp(value, "close") == 0) {
					conn->close_when_done = TRUE;
				}
			}
		}

	loop_end:
		cursor = conn->buf->str + conn->handled_len;
	}

	if (handled) {
		msn_soap_connection_handle_next(conn);
	}
}

static void
msn_soap_write_cb(gpointer data, gint fd, PurpleInputCondition cond)
{
	MsnSoapConnection *conn = data;
	int written;

	g_return_if_fail(cond == PURPLE_INPUT_WRITE);

	written = purple_ssl_write(conn->ssl, conn->buf->str + conn->handled_len,
		conn->buf->len - conn->handled_len);

	if (written < 0 && errno == EAGAIN)
		return;
	else if (written <= 0) {
		purple_ssl_close(conn->ssl);
		conn->ssl = NULL;
		msn_soap_connection_handle_next(conn);
		return;
	}

	conn->handled_len += written;

	if (conn->handled_len < conn->buf->len)
		return;

	/* we are done! */
	g_string_free(conn->buf, TRUE);
	conn->buf = NULL;
	conn->handled_len = 0;
	conn->body_len = 0;
	conn->response_code = 0;
	conn->headers_done = FALSE;
	conn->close_when_done = FALSE;

	purple_input_remove(conn->event_handle);
	conn->event_handle = purple_input_add(conn->ssl->fd, PURPLE_INPUT_READ,
		msn_soap_read_cb, conn);
}

static gboolean
msn_soap_connection_run(gpointer data)
{
	MsnSoapConnection *conn = data;
	MsnSoapRequest *req = g_queue_peek_head(conn->queue);

	conn->event_handle = 0;

	if (req) {
		if (conn->ssl == NULL) {
			conn->ssl = purple_ssl_connect(conn->session->account, conn->host,
				443, msn_soap_connected_cb, msn_soap_error_cb, conn);
		} else if (conn->connected) {
			int len = -1;
			char *body = xmlnode_to_str(req->message->xml, &len);
			GSList *iter;
			char *authstr = NULL;

			g_queue_pop_head(conn->queue);

			conn->buf = g_string_new("");

			if (conn->session->passport_info.mspauth)
				authstr = g_strdup_printf("Cookie: MSPAuth=%s\r\n",
					conn->session->passport_info.mspauth);


			g_string_append_printf(conn->buf,
				"POST %s HTTP/1.1\r\n"
				"SOAPAction: %s\r\n"
				"Content-Type:text/xml; charset=utf-8\r\n"
				"%s"
				"User-Agent: Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1)\r\n"
				"Accept: */*\r\n"
				"Host: %s\r\n"
				"Content-Length: %d\r\n"
				"Connection: Keep-Alive\r\n"
				"Cache-Control: no-cache\r\n",
				req->path, req->message->action ? req->message->action : "",
				authstr ? authstr : "",	conn->host, len);

			for (iter = req->message->headers; iter; iter = iter->next) {
				g_string_append(conn->buf, (char *)iter->data);
				g_string_append(conn->buf, "\r\n");
			}

			g_string_append(conn->buf, "\r\n");
			g_string_append(conn->buf, body);

			purple_debug_info("soap", "%s\n", conn->buf->str);

			conn->handled_len = 0;
			conn->current_request = req;

			conn->event_handle = purple_input_add(conn->ssl->fd,
				PURPLE_INPUT_WRITE, msn_soap_write_cb, conn);
			msn_soap_write_cb(conn, conn->ssl->fd, PURPLE_INPUT_WRITE);

			g_free(authstr);
		}		
	}

	return FALSE;
}

void
msn_soap_message_send(MsnSession *session, MsnSoapMessage *message,
	const char *host, const char *path,
	MsnSoapCallback cb, gpointer cb_data)
{
	msn_soap_message_send_internal(session, message, host, path, cb, cb_data,
		FALSE);
}

static void
msn_soap_message_send_internal(MsnSession *session,
	MsnSoapMessage *message, const char *host, const char *path,
	MsnSoapCallback cb, gpointer cb_data, gboolean first)
{
	MsnSoapConnection *conn = msn_soap_get_connection(session, host);
	MsnSoapRequest *req = g_new0(MsnSoapRequest, 1);

	req->path = g_strdup(path);
	req->message = message;
	req->cb = cb;
	req->cb_data = cb_data;

	if (first) {
		g_queue_push_head(conn->queue, req);
	} else {
		g_queue_push_tail(conn->queue, req);
	}

	if (conn->event_handle == 0)
		conn->event_handle = purple_timeout_add(0, msn_soap_connection_run,
			conn);
}

static void
msn_soap_connection_sanitize(MsnSoapConnection *conn, gboolean disconnect)
{
	if (conn->event_handle) {
		purple_input_remove(conn->event_handle);
		conn->event_handle = 0;
	}

	if (conn->message) {
		msn_soap_message_destroy(conn->message);
		conn->message = NULL;
	}

	if (conn->buf) {
		g_string_free(conn->buf, TRUE);
		conn->buf = NULL;
	}

	if (conn->ssl && (disconnect || conn->close_when_done)) {
		purple_ssl_close(conn->ssl);
		conn->ssl = NULL;
	}

	if (conn->current_request) {
		msn_soap_request_destroy(conn->current_request);
		conn->current_request = NULL;
	}
}

static void
msn_soap_connection_handle_next(MsnSoapConnection *conn)
{
	msn_soap_connection_sanitize(conn, FALSE);

	conn->event_handle = purple_timeout_add(0, msn_soap_connection_run,	conn);

	if (conn->current_request) {
		MsnSoapRequest *req = conn->current_request;
		conn->current_request = NULL;
		msn_soap_connection_destroy_foreach_cb(req, conn);
	}
}

static void
msn_soap_connection_destroy_foreach_cb(gpointer item, gpointer data)
{
	MsnSoapRequest *req = item;

	if (req->cb)
		req->cb(req->message, NULL, req->cb_data);

	msn_soap_request_destroy(req);
}

static void
msn_soap_connection_destroy(MsnSoapConnection *conn)
{
	if (conn->current_request) {
		MsnSoapRequest *req = conn->current_request;
		conn->current_request = NULL;
		msn_soap_connection_destroy_foreach_cb(req, conn);
	}

	msn_soap_connection_sanitize(conn, TRUE);
	g_queue_foreach(conn->queue, msn_soap_connection_destroy_foreach_cb, conn);
	g_queue_free(conn->queue);

	g_free(conn->host);
	g_free(conn);
}

MsnSoapMessage *
msn_soap_message_new(const char *action, xmlnode *xml)
{
	MsnSoapMessage *message = g_new0(MsnSoapMessage, 1);

	message->action = g_strdup(action);
	message->xml = xml;

	return message;
}

void
msn_soap_message_destroy(MsnSoapMessage *message)
{
	if (message) {
		g_slist_foreach(message->headers, (GFunc)g_free, NULL);
		g_slist_free(message->headers);
		g_free(message->action);
		if (message->xml)
			xmlnode_free(message->xml);
		g_free(message);
	}
}

void
msn_soap_message_add_header(MsnSoapMessage *req,
		const char *name, const char *value)
{
	char *header = g_strdup_printf("%s: %s\r\n", name, value);

	req->headers = g_slist_prepend(req->headers, header);
}

static void
msn_soap_request_destroy(MsnSoapRequest *req)
{
	g_free(req->path);
	msn_soap_message_destroy(req->message);
	g_free(req);
}

xmlnode *
msn_soap_xml_get(xmlnode *parent, const char *node)
{
	xmlnode *ret;
	char **tokens = g_strsplit(node, "/", -1);
	int i;

	for (i = 0; tokens[i]; i++) {
		if ((ret = xmlnode_get_child(parent, tokens[i])) != NULL)
			parent = ret;
		else
			break;
	}

	g_strfreev(tokens);
	return ret;
}

