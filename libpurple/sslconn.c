/* purple
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#define _PURPLE_SSLCONN_C_

#include "internal.h"

#include "debug.h"
#include "plugins.h"
#include "request.h"
#include "sslconn.h"
#include "tls-certificate.h"

#define CONNECTION_CLOSE_TIMEOUT 15

static void
emit_error(PurpleSslConnection *gsc, int error_code)
{
	if (gsc->error_cb != NULL)
		gsc->error_cb(gsc, error_code, gsc->connect_cb_data);
}

static void
tls_handshake_cb(GObject *source, GAsyncResult *res, gpointer user_data)
{
	PurpleSslConnection *gsc = user_data;
	GError *error = NULL;

	if (!g_tls_connection_handshake_finish(G_TLS_CONNECTION(source), res,
			&error)) {
		if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			/* Connection already closed/freed. Escape. */
			return;
		} else if (g_error_matches(error, G_TLS_ERROR,
				G_TLS_ERROR_HANDSHAKE)) {
			/* In Gio, a handshake error is because of the cert */
			emit_error(gsc, PURPLE_SSL_CERTIFICATE_INVALID);
		} else {
			/* Report any other errors as handshake failing */
			emit_error(gsc, PURPLE_SSL_HANDSHAKE_FAILED);
		}

		purple_ssl_close(gsc);
		return;
	}

	gsc->connect_cb(gsc->connect_cb_data, gsc, PURPLE_INPUT_READ);
}

static gboolean
tls_connect(PurpleSslConnection *gsc)
{
	GSocket *socket;
	GSocketConnection *conn;
	GSocketConnectable *identity;
	GIOStream *tls_conn;
	GError *error = NULL;

	g_return_val_if_fail(gsc->conn == NULL, FALSE);

	socket = g_socket_new_from_fd(gsc->fd, &error);
	if (socket == NULL) {
		purple_debug_warning("sslconn",
				"Error creating socket from fd (%u): %s",
				gsc->fd, error->message);
		g_clear_error(&error);
		return FALSE;
	}

	conn = g_socket_connection_factory_create_connection(socket);
	g_object_unref(socket);

	identity = g_network_address_new(gsc->host, gsc->port);
	tls_conn = g_tls_client_connection_new(G_IO_STREAM(conn), identity,
			&error);
	g_object_unref(identity);
	g_object_unref(conn);

	if (tls_conn == NULL) {
		purple_debug_warning("sslconn",
				"Error creating TLS client connection: %s",
				error->message);
		g_clear_error(&error);
		return FALSE;
	}

	gsc->conn = G_TLS_CONNECTION(tls_conn);
	gsc->cancellable = g_cancellable_new();

	purple_tls_certificate_attach_to_tls_connection(gsc->conn);

	g_tls_connection_handshake_async(gsc->conn, G_PRIORITY_DEFAULT,
			gsc->cancellable, tls_handshake_cb, gsc);

	return TRUE;
}

static void
purple_ssl_connect_cb(gpointer data, gint source, const gchar *error_message)
{
	PurpleSslConnection *gsc;

	gsc = data;
	gsc->connect_data = NULL;

	if (source < 0)
	{
		emit_error(gsc, PURPLE_SSL_CONNECT_FAILED);
		purple_ssl_close(gsc);
		return;
	}

	gsc->fd = source;

	if (!tls_connect(gsc)) {
		emit_error(gsc, PURPLE_SSL_CONNECT_FAILED);
		purple_ssl_close(gsc);
	}
}

PurpleSslConnection *
purple_ssl_connect(PurpleAccount *account, const char *host, int port,
				 PurpleSslInputFunction func, PurpleSslErrorFunction error_func,
				 void *data)
{
	return purple_ssl_connect_with_ssl_cn(account, host, port, func, error_func,
	                                  NULL, data);
}

PurpleSslConnection *
purple_ssl_connect_with_ssl_cn(PurpleAccount *account, const char *host, int port,
				 PurpleSslInputFunction func, PurpleSslErrorFunction error_func,
				 const char *ssl_cn, void *data)
{
	PurpleSslConnection *gsc;

	g_return_val_if_fail(host != NULL,            NULL);
	g_return_val_if_fail(port != 0 && port != -1, NULL);
	g_return_val_if_fail(func != NULL,            NULL);

	gsc = g_new0(PurpleSslConnection, 1);

	gsc->fd              = -1;
	gsc->host            = ssl_cn ? g_strdup(ssl_cn) : g_strdup(host);
	gsc->port            = port;
	gsc->connect_cb_data = data;
	gsc->connect_cb      = func;
	gsc->error_cb        = error_func;

	gsc->connect_data = purple_proxy_connect(NULL, account, host, port, purple_ssl_connect_cb, gsc);

	if (gsc->connect_data == NULL)
	{
		g_free(gsc->host);
		g_free(gsc);

		return NULL;
	}

	return (PurpleSslConnection *)gsc;
}

static gboolean
recv_cb(GObject *source, gpointer data)
{
	PurpleSslConnection *gsc = data;

	gsc->recv_cb(gsc->recv_cb_data, gsc, PURPLE_INPUT_READ);

	return TRUE;
}

void
purple_ssl_input_add(PurpleSslConnection *gsc, PurpleSslInputFunction func,
				   void *data)
{
	GInputStream *input;
	GSource *source;

	g_return_if_fail(func != NULL);
	g_return_if_fail(gsc->conn != NULL);

	purple_ssl_input_remove(gsc);

	gsc->recv_cb_data = data;
	gsc->recv_cb      = func;

	input = g_io_stream_get_input_stream(G_IO_STREAM(gsc->conn));
	/* Pass NULL for cancellable as we don't want it notified on cancel */
	source = g_pollable_input_stream_create_source(
			G_POLLABLE_INPUT_STREAM(input), NULL);
	g_source_set_callback(source, (GSourceFunc)recv_cb, gsc, NULL);
	gsc->inpa = g_source_attach(source, NULL);
	g_source_unref(source);
}

void
purple_ssl_input_remove(PurpleSslConnection *gsc)
{
	if (gsc->inpa > 0) {
		g_source_remove(gsc->inpa);
		gsc->inpa = 0;
	}
}

const gchar *
purple_ssl_strerror(PurpleSslErrorType error)
{
	switch(error) {
		case PURPLE_SSL_CONNECT_FAILED:
			return _("SSL Connection Failed");
		case PURPLE_SSL_HANDSHAKE_FAILED:
			return _("SSL Handshake Failed");
		case PURPLE_SSL_CERTIFICATE_INVALID:
			return _("SSL peer presented an invalid certificate");
		default:
			purple_debug_warning("sslconn", "Unknown SSL error code %d\n", error);
			return _("Unknown SSL error");
	}
}

PurpleSslConnection *
purple_ssl_connect_with_host_fd(PurpleAccount *account, int fd,
                      PurpleSslInputFunction func,
                      PurpleSslErrorFunction error_func,
                      const char *host,
                      void *data)
{
	PurpleSslConnection *gsc;

	g_return_val_if_fail(fd != -1,                NULL);
	g_return_val_if_fail(func != NULL,            NULL);

	gsc = g_new0(PurpleSslConnection, 1);

	gsc->connect_cb_data = data;
	gsc->connect_cb      = func;
	gsc->error_cb        = error_func;
	gsc->fd              = fd;
    if(host)
        gsc->host            = g_strdup(host);
	gsc->cancellable     = g_cancellable_new();

	if (!tls_connect(gsc)) {
		emit_error(gsc, PURPLE_SSL_CONNECT_FAILED);
		g_clear_pointer(&gsc, purple_ssl_close);
	}

	return (PurpleSslConnection *)gsc;
}

static void
connection_closed_cb(GObject *stream, GAsyncResult *result,
		gpointer timeout_id)
{
	GError *error = NULL;

	purple_timeout_remove(GPOINTER_TO_UINT(timeout_id));

	g_io_stream_close_finish(G_IO_STREAM(stream), result, &error);

	if (error) {
		purple_debug_info("sslconn", "Connection close error: %s",
				error->message);
		g_clear_error(&error);
	} else {
		purple_debug_info("sslconn", "Connection closed.");
	}
}

static void
cleanup_cancellable_cb(gpointer data, GObject *where_the_object_was)
{
	g_object_unref(G_CANCELLABLE(data));
}

void
purple_ssl_close(PurpleSslConnection *gsc)
{
	g_return_if_fail(gsc != NULL);

	purple_request_close_with_handle(gsc);
	purple_notify_close_with_handle(gsc);

	if (gsc->connect_data != NULL)
		purple_proxy_connect_cancel(gsc->connect_data);

	if (gsc->inpa > 0)
		purple_input_remove(gsc->inpa);

	/* Stop any pending operations */
	if (G_IS_CANCELLABLE(gsc->cancellable)) {
		g_cancellable_cancel(gsc->cancellable);
		g_clear_object(&gsc->cancellable);
	}

	if (gsc->conn != NULL) {
		GCancellable *cancellable;
		guint timer_id;

		cancellable = g_cancellable_new();
		g_object_weak_ref(G_OBJECT(gsc->conn), cleanup_cancellable_cb,
				cancellable);

		timer_id = purple_timeout_add_seconds(CONNECTION_CLOSE_TIMEOUT,
				(GSourceFunc)g_cancellable_cancel, cancellable);

		g_io_stream_close_async(G_IO_STREAM(gsc->conn),
				G_PRIORITY_DEFAULT, cancellable,
				connection_closed_cb,
				GUINT_TO_POINTER(timer_id));
		g_clear_object(&gsc->conn);
	}

	g_free(gsc->host);
	g_free(gsc);
}

size_t
purple_ssl_read(PurpleSslConnection *gsc, void *data, size_t len)
{
	GInputStream *input;
	gssize outlen;
	GError *error = NULL;

	g_return_val_if_fail(gsc  != NULL, 0);
	g_return_val_if_fail(data != NULL, 0);
	g_return_val_if_fail(len  >  0,    0);
	g_return_val_if_fail(gsc->conn != NULL, 0);

	input = g_io_stream_get_input_stream(G_IO_STREAM(gsc->conn));
	outlen = g_pollable_input_stream_read_nonblocking(
			G_POLLABLE_INPUT_STREAM(input), data, len,
			gsc->cancellable, &error);

	if (outlen < 0) {
		if (g_error_matches(error, G_IO_ERROR,
				G_IO_ERROR_WOULD_BLOCK)) {
			errno = EAGAIN;
		}

		g_clear_error(&error);
	}

	return outlen;
}

size_t
purple_ssl_write(PurpleSslConnection *gsc, const void *data, size_t len)
{
	GOutputStream *output;
	gssize outlen;
	GError *error = NULL;

	g_return_val_if_fail(gsc  != NULL, 0);
	g_return_val_if_fail(data != NULL, 0);
	g_return_val_if_fail(len  >  0,    0);
	g_return_val_if_fail(gsc->conn != NULL, 0);

	output = g_io_stream_get_output_stream(G_IO_STREAM(gsc->conn));
	outlen = g_pollable_output_stream_write_nonblocking(
			G_POLLABLE_OUTPUT_STREAM(output), data, len,
			gsc->cancellable, &error);

	if (outlen < 0) {
		if (g_error_matches(error, G_IO_ERROR,
				G_IO_ERROR_WOULD_BLOCK)) {
			errno = EAGAIN;
		}

		g_clear_error(&error);
	}

	return outlen;
}

GList *
purple_ssl_get_peer_certificates(PurpleSslConnection *gsc)
{
	GTlsCertificate *certificate;

	g_return_val_if_fail(gsc != NULL, NULL);
	g_return_val_if_fail(gsc->conn != NULL, NULL);

	certificate = g_tls_connection_get_peer_certificate(gsc->conn);

	return certificate != NULL ? g_list_append(NULL, certificate) : NULL;
}

