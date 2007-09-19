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

static void
msn_soap_read_cb(gpointer data, gint fd, PurpleInputCondition cond)
{
    MsnSoapConnection2 *conn = data;
	int count;
	char buf[8192];
	int linebreak;

	g_return_if_fail(cond == PURPLE_INPUT_READ);

	count = purple_ssl_read(conn->ssl, buf, sizeof(buf));
	if (count < 0 && errno == EAGAIN)
		return;
	else if (count <= 0) {
		msn_soap_connection2_cleanup(conn);
		return;
	}

	if (conn->buf == NULL) {
		conn->buf = g_memdup(buf, count);
		conn->buf_len = len;
	} else {
		conn->buf = g_realloc(conn->buf, conn->buf_len + count);
		memcpy(conn->buf + conn->buf_len, buf, count);
		conn->buf_len += count;
	}

	if (conn->response == NULL) {
		conn->response = msn_soap_message_new(conn->current->action, NULL);
	}

	while ((linebreak = strstr(conn->buf + conn->buf_count, "\r\n")) != NULL) {
		
	}
}

static void
msn_soap_write_cb(gpointer data, gint fd, PurpleInputCondition cond)
{
	MsnSoapConnection2 *conn = data;
	int written;

	g_return_if_fail(cond == PURPLE_INPUT_WRITE);

	written = purple_ssl_write(conn->ssl, conn->buf + conn->buf_count,
		conn->buf_len - conn->buf_count);

	if (written < 0 && errno == EAGAIN)
		return;
	else if (written <= 0) {
		msn_soap_connection2_cleanup(conn);
		return;
	}

	conn->buf_count += written;

	if (conn->buf_count < conn->buf_len)
		return;

	/* we are done! */
	g_free(conn->buf);
	conn->buf_len = 0;
	conn->buf_count = 0;

	purple_input_remove(conn->io_handle);
	conn->io_handle = purple_input_add(conn->ssl->fd, PURPLE_INPUT_READ,
		msn_soap_read_cb, conn);
}

static gboolean
msn_soap_connection2_run(gpointer data)
{
	MsnSoapConnection2 *conn = data;
	MsnSoapMessage *req = g_queue_peek_head(conn->queue);

	if (req) {
		if (conn->ssl) {
			if (strcmp(conn->ssl->host, req->host) != 0 ||
				strcmp(conn->path, req->path) != 0) {
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
			char *body = xmlnode_to_str(req->message, &len);
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

			for (iter = req->headers; iter; iter = iter->next) {
				g_string_append(str, (char *)iter->data);
				g_string_append(str, "\r\n");
			}

			g_string_append(str, "\r\n");
			g_string_append(str, body);

			conn->buf_len = str->len;
			conn->buf = g_string_free(str, FALSE);
			conn->buf_count = 0;
			conn->current = req;

			conn->io_handle = purple_input_add(conn->ssl->fd,
				PURPLE_INPUT_WRITE, msn_soap_write_cb, conn);
			msn_soap_write_cb(conn, conn->ssl->fd, PURPLE_INPUT_WRITE);
		}		
	}

	conn->idle_handle = 0;
	return FALSE;
}

void
msn_soap_connection2_post(MsnSoapConnection2 *conn, MsnSoapMessage *req,
		const char *host, const char *path, MsnSoapCallback cb, gpointer data)
{
	req->cb = cb;
	req->data = data;
	req->host = g_strdup(host);
	req->path = g_strdup(path);

	g_queue_push_tail(conn->queue, req);

	if (conn->idle_handle == 0)
		conn->idle_handle = g_idle_add(msn_soap_connection2_run, conn);
}

static void
msn_soap_connection2_destroy_foreach_cb(gpointer item, gpointer data)
{
	MsnSoapMessage *req = item;
	MsnSoapConnection2 *conn = data;

	if (req->cb)
		req->cb(conn, req, NULL, req->data);

	msn_soap_message_destroy(req);
}

static void
msn_soap_connection2_cleanup(MsnSoapConnection2 *conn)
{
	g_queue_foreach(conn->queue, msn_soap_connection2_destroy_foreach_cb, conn);
	if (conn->current) {
		msn_soap_connection2_destroy_foreach_cb(conn->current, conn);
		conn->current = NULL;
	}

	purple_input_remove(conn->io_handle);
	conn->io_handle = 0;
	g_source_remove(conn->idle_handle);
	conn->idle_handle = 0;

	g_free(conn->buf);
	conn->buf_len = 0;
	conn->buf_count = 0;

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
msn_soap_message_new(const char *action, xmlnode *message)
{
	MsnSoapMessage *req = g_new0(MsnSoapMessage, 1);

	req->action = g_strdup(action);
	req->message = message;

	return req;
}

void
msn_soap_message_destroy(MsnSoapMessage *req)
{
	g_free(req->action);
	g_slist_foreach(req->headers, (GFunc)g_free, NULL);
	g_free(req->host);
	g_free(req->path);
	g_free(req);
}

void
msn_soap_message_add_header(MsnSoapMessage *req,
		const char *name, const char *value)
{
	char *header = g_strdup_printf("%s: %s\r\n", name, value);

	req->headers = g_slist_prepend(req->headers, header);
}
