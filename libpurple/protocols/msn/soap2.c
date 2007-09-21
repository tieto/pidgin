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

#include "debug.h"
#include "xmlnode.h"

#include <glib.h>
#include <error.h>

static void msn_soap_connection2_destroy_foreach_cb(gpointer item, gpointer data);
static void msn_soap_connection2_cleanup(MsnSoapConnection2 *conn);
static gboolean msn_soap_connection2_run(gpointer data);


MsnSoapConnection2 *
msn_soap_connection2_new(MsnSession *session)
{
	MsnSoapConnection2 *conn = g_new0(MsnSoapConnection2, 1);
	conn->session = session;
	conn->queue = g_queue_new();
	return conn;
}

static void
msn_soap_connected_cb(gpointer data, PurpleSslConnection *ssl,
		PurpleInputCondition cond)
{
	MsnSoapConnection2 *conn = data;

	conn->connected = TRUE;

	if (conn->idle_handle == 0)
		conn->idle_handle = g_idle_add(msn_soap_connection2_run, conn);
}

static void
msn_soap_error_cb(PurpleSslConnection *ssl, PurpleSslErrorType error,
		gpointer data)
{
	MsnSoapConnection2 *conn = data;

	msn_soap_connection2_cleanup(conn);
}

static gboolean
msn_soap_handle_redirect(MsnSoapConnection2 *conn, const char *url)
{
	char *c;

	/* Skip the http:// */
	if ((c = strchr(url, '/')) != NULL)
		url += 2;

	if ((c = strchr(url, '/')) != NULL) {
		g_free(conn->request->host);
		g_free(conn->request->path);

		conn->request->host = g_strndup(url, c - url);
		conn->request->path = g_strdup(c);

		purple_input_remove(conn->io_handle);
		conn->io_handle = 0;

		msn_soap_connection2_post(conn, conn->request,
			conn->request->cb, conn->request->data);

		return TRUE;
	}

	return FALSE;
}

static gboolean
msn_soap_handle_body(MsnSoapConnection2 *conn, MsnSoapResponse *response)
{
	xmlnode *node = response->message->xml;

	if (strcmp(node->name, "Envelop") == 0 &&
		node->child && strcmp(node->child->name, "Header") == 0 &&
		node->child->next) {
		xmlnode *body = node->child->next;

		if (strcmp(body->name, "Fault")) {
			xmlnode *fault = xmlnode_get_child(body, "faultcode");

			if (fault != NULL) {
				if (strcmp(fault->data, "psf:Redirect") == 0) {
					xmlnode *url = xmlnode_get_child(fault, "redirectUrl");

					if (url && !msn_soap_handle_redirect(conn, url->data)) {
						return TRUE;
					}
				} else if (strcmp(fault->data, "wsse:FailedAuthentication")) {
					xmlnode *reason = xmlnode_get_child(fault, "faultstring");

					msn_session_set_error(conn->session, MSN_ERROR_AUTH,
						reason ? reason->data : NULL);

					return TRUE;
				}
			}

			conn->request->cb(conn, conn->request, conn->response,
				conn->request->data);

			return TRUE;
		}
	}

	return FALSE;
}

static void
msn_soap_read_cb(gpointer data, gint fd, PurpleInputCondition cond)
{
    MsnSoapConnection2 *conn = data;
	MsnSoapMessage *message;
	int count;
	char buf[8192];
	char *linebreak;

	g_return_if_fail(cond == PURPLE_INPUT_READ);

	count = purple_ssl_read(conn->ssl, buf, sizeof(buf));
	if (count < 0 && errno == EAGAIN)
		return;
	else if (count <= 0) {
		msn_soap_connection2_cleanup(conn);
		return;
	}

	if (conn->response == NULL) {
		conn->response = msn_soap_response_new();
		conn->response->message = msn_soap_message_new();
	}

	message = conn->response->message;

	if (message->buf == NULL) {
		message->buf = g_memdup(buf, count);
		message->buf_len = count;
	} else {
		message->buf = g_realloc(message->buf, message->buf_len + count);
		memcpy(message->buf + message->buf_len,	buf, count);
		message->buf_len += count;
	}

	if (conn->response->seen_newline) {
		if (message->buf_len - message->buf_count >=
			conn->response->body_len) {
			xmlnode *node = xmlnode_from_str(
				message->buf + message->buf_count, conn->response->body_len);

			if (node == NULL) {
				purple_debug_info("soap", "Malformed SOAP response: %s\n",
					message->buf + message->buf_count);
			} else {
				conn->response->message->xml = node;
				msn_soap_handle_body(conn, conn->response);
			}
		}

		return;
	}

	while ((linebreak = strstr(message->buf + message->buf_count, "\r\n"))
		!= NULL) {
		message->buf_count = linebreak - message->buf + 2;

		if (conn->response->code == -1) {
			if (sscanf(message->buf + message->buf_count, "HTTP/1.1 %d",
					&conn->response->code) != 1) {
				/* something horribly wrong */
				msn_soap_connection2_destroy_foreach_cb(conn->request, conn);
				conn->request = NULL;
			} else if (conn->response->code == 503) {
				msn_session_set_error(conn->session, MSN_ERROR_SERV_UNAVAILABLE, NULL);
			}
		} else if (message->buf + message->buf_count == linebreak) {
			/* blank line */
			conn->response->seen_newline = TRUE;
		} else {
			char *sep = strstr(message->buf + message->buf_count, ": ");
			char *key = message->buf + message->buf_count;
			char *value = sep + 2;

			*sep = '\0';
			*linebreak = '\0';
			msn_soap_message_add_header(message, key, value);

			if ((conn->response->code == 301 || conn->response->code == 300)
				&& strcmp(key, "Location") == 0) {

				if (msn_soap_handle_redirect(conn, value)) {

				} else if (conn->request->cb) {
					conn->request->cb(conn, conn->request, NULL,
						conn->request->data);
				}
			} else if (conn->response->code == 401 &&
				strcmp(key, "WWW-Authenticate") == 0) {
				char *error = strstr(value, "cbtxt=");

				if (error) {
					error += strlen("cbtxt=");
				}

				msn_session_set_error(conn->session, MSN_ERROR_AUTH,
					error ? purple_url_decode(error) : NULL);
			} else if (strcmp(key, "Content-Length") == 0) {
				conn->response->body_len = atoi(value);
			}
		}
	}
}

static void
msn_soap_write_cb(gpointer data, gint fd, PurpleInputCondition cond)
{
	MsnSoapConnection2 *conn = data;
	MsnSoapMessage *message = conn->request->message;
	int written;

	g_return_if_fail(cond == PURPLE_INPUT_WRITE);

	written = purple_ssl_write(conn->ssl, message->buf + message->buf_count,
		message->buf_len - message->buf_count);

	if (written < 0 && errno == EAGAIN)
		return;
	else if (written <= 0) {
		msn_soap_connection2_cleanup(conn);
		return;
	}

	message->buf_count += written;

	if (message->buf_count < message->buf_len)
		return;

	/* we are done! */
	purple_input_remove(conn->io_handle);
	conn->io_handle = purple_input_add(conn->ssl->fd, PURPLE_INPUT_READ,
		msn_soap_read_cb, conn);
}

static gboolean
msn_soap_connection2_run(gpointer data)
{
	MsnSoapConnection2 *conn = data;
	MsnSoapRequest *req = g_queue_peek_head(conn->queue);

	if (req) {
		if (conn->ssl) {
			if (strcmp(conn->ssl->host, req->host) != 0 ||
				strcmp(conn->path, req->path) != 0) {
				purple_input_remove(conn->io_handle);
				conn->io_handle = 0;
				purple_ssl_close(conn->ssl);
				conn->ssl = NULL;
				g_free(conn->path);
				conn->path = NULL;
			}
		}

		if (conn->ssl == NULL) {
			conn->ssl = purple_ssl_connect(conn->session->account, req->host,
				443, msn_soap_connected_cb, msn_soap_error_cb, conn);
			conn->path = g_strdup(req->path);
		} else {
			int len = -1;
			MsnSoapMessage *message = req->message;
			char *body = xmlnode_to_str(message->xml, &len);
			GString *str = g_string_new("");
			GSList *iter;

			g_queue_pop_head(conn->queue);

			g_string_append_printf(str,
				"POST %s HTTP/1.1\r\n"
				"SOAPAction: %s\r\n"
				"Content-Type:text/xml; charset=utf-8\r\n"
				"Cookie: MSPAuth=%s\r\n"
				"User-Agent: Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1)\r\n"
				"Accept: */*\r\n"
				"Host: %s\r\n"
				"Content-Length: %d\r\n"
				"Connection: Keep-Alive\r\n"
				"Cache-Control: no-cache\r\n",
				req->path, req->action,
				conn->session->passport_info.mspauth,
				req->host, len);

			for (iter = req->message->headers; iter; iter = iter->next) {
				g_string_append(str, (char *)iter->data);
				g_string_append(str, "\r\n");
			}

			g_string_append(str, "\r\n");
			g_string_append(str, body);

			message->buf_len = str->len;
			message->buf = g_string_free(str, FALSE);
			message->buf_count = 0;
			conn->request = req;

			conn->io_handle = purple_input_add(conn->ssl->fd,
				PURPLE_INPUT_WRITE, msn_soap_write_cb, conn);
			msn_soap_write_cb(conn, conn->ssl->fd, PURPLE_INPUT_WRITE);
		}		
	}

	conn->idle_handle = 0;
	return FALSE;
}

void
msn_soap_connection2_post(MsnSoapConnection2 *conn, MsnSoapRequest *req,
	MsnSoapCallback cb, gpointer data)
{
	req->cb = cb;
	req->data = data;

	g_queue_push_tail(conn->queue, req);

	if (conn->idle_handle == 0)
		conn->idle_handle = g_idle_add(msn_soap_connection2_run, conn);
}

static void
msn_soap_connection2_destroy_foreach_cb(gpointer item, gpointer data)
{
	MsnSoapRequest *req = item;
	MsnSoapConnection2 *conn = data;

	if (req->cb)
		req->cb(conn, req, NULL, req->data);

	msn_soap_request2_destroy(req);
}

static void
msn_soap_connection2_cleanup(MsnSoapConnection2 *conn)
{
	g_queue_foreach(conn->queue, msn_soap_connection2_destroy_foreach_cb, conn);
	if (conn->request) {
		msn_soap_connection2_destroy_foreach_cb(conn->request, conn);
		conn->request = NULL;
	}

	purple_input_remove(conn->io_handle);
	conn->io_handle = 0;
	g_source_remove(conn->idle_handle);
	conn->idle_handle = 0;

	if (conn->ssl) {
		purple_ssl_close(conn->ssl);
		conn->ssl = NULL;
	}

	while (g_queue_pop_head(conn->queue) != NULL);

	g_free(conn->path);
}

void
msn_soap_connection2_destroy(MsnSoapConnection2 *conn)
{
	msn_soap_connection2_cleanup(conn);
	g_queue_free(conn->queue);
	g_free(conn);
}

MsnSoapMessage *
msn_soap_message_new()
{
	MsnSoapMessage *req = g_new0(MsnSoapMessage, 1);

	return req;
}

void
msn_soap_message_destroy(MsnSoapMessage *message)
{
	g_slist_foreach(message->headers, (GFunc)g_free, NULL);
	g_free(message->buf);
	g_free(message);
}

void
msn_soap_message_add_header(MsnSoapMessage *req,
		const char *name, const char *value)
{
	char *header = g_strdup_printf("%s: %s\r\n", name, value);

	req->headers = g_slist_prepend(req->headers, header);
}

MsnSoapRequest *
msn_soap_request2_new(const char *host, const char *path, const char *action)
{
	MsnSoapRequest *req = g_new0(MsnSoapRequest, 1);

	req->host = g_strdup(host);
	req->path = g_strdup(path);
	req->action = g_strdup(action);

	return req;
}

void
msn_soap_request2_destroy(MsnSoapRequest *req)
{
	g_free(req->action);
	g_free(req->host);
	g_free(req->path);
	msn_soap_message_destroy(req->message);
	g_free(req);
}

MsnSoapResponse *
msn_soap_response_new(void)
{
	MsnSoapResponse *resp = g_new0(MsnSoapResponse, 1);

	return resp;
}

void
msn_soap_response_destroy(MsnSoapResponse *resp)
{
	msn_soap_message_destroy(resp->message);
	resp->code = -1;
	resp->body_len = -1;
	g_free(resp);
}
