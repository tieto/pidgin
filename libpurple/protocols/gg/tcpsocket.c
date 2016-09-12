/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here. Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * Component written by Tomek Wasilczyk (http://www.wasilczyk.pl).
 *
 * This file is dual-licensed under the GPL2+ and the X11 (MIT) licences.
 * As a recipient of this file you may choose, which license to receive the
 * code under. As a contributor, you have to ensure the new code is
 * compatible with both.
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
#include "tcpsocket.h"

#include "gg.h"

#include "debug.h"
#include "purple-gio.h"

typedef struct {
	GSocketConnection *conn;
	GCancellable *cancellable;
	PurpleConnection *gc;
	gpointer priv_gg;
} GGPTcpSocketData;

static void
ggp_tcp_socket_data_free(GGPTcpSocketData *data)
{
	g_return_if_fail(data != NULL);

	if (data->cancellable != NULL) {
		g_cancellable_cancel(data->cancellable);
		g_clear_object(&data->cancellable);
	}

	if (data->conn != NULL) {
		purple_gio_graceful_close(G_IO_STREAM(data->conn), NULL, NULL);
		g_clear_object(&data->conn);
	}

	g_free(data);
}

static void
ggp_tcpsocket_connected(GObject *source, GAsyncResult *res, gpointer user_data)
{
	GGPTcpSocketData *data = user_data;
	GSocketConnection *conn;
	GSocket *socket;
	int fd = -1;
	GGPInfo *info;
	GError *error = NULL;

	conn = g_socket_client_connect_to_host_finish(G_SOCKET_CLIENT(source),
			res, &error);

	if (conn == NULL) {
		if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			/* The connection was already closed, return now */
			g_clear_error(&error);
			return;
		}

		purple_debug_error("gg", "socket failed to connect: %s",
				error->message);
		g_clear_error(&error);
	} else {
		data->conn = conn;

		socket = g_socket_connection_get_socket(data->conn);

		if (socket != NULL) {
			fd = g_socket_get_fd(socket);
		}
	}

	/* XXX: For some reason if you try to connect and then immediately
	 * disconnect, this gets into a state where ggp_tcpsocket_close()
	 * isn't called. The cancellable is therefore not cancelled, and
	 * the connection is never closed. Guard against that state here.
	 */
	if (data->gc == NULL ||
			!g_list_find(purple_connections_get_all(), data->gc)) {
		purple_debug_error("gg",
				"disconnected without closing connection: %p",
				data);
		ggp_tcp_socket_data_free(data);
		return;
	}

	if (!gg_socket_manager_connected(data, data->priv_gg, fd)) {
		purple_debug_error("gg", "socket not handled");
		ggp_tcp_socket_data_free(data);
		return;
	}

	info = purple_connection_get_protocol_data(data->gc);

	if (info->inpa > 0) {
		g_source_remove(info->inpa);
		info->inpa = 0;
	}

	if (info->session->fd < 0)
		return;

	/* XXX: This works, but not recommended to use GSocket FDs directly */
	info->inpa = purple_input_add(info->session->fd,
		ggp_tcpsocket_inputcond_gg_to_purple(info->session->check),
		ggp_async_login_handler, data->gc);
}

static void*
ggp_tcpsocket_connect(void *_gc, const char *host, int port, int is_tls,
	int is_async, void *priv)
{
	PurpleConnection *gc = _gc;
	GGPTcpSocketData *data;
	GSocketClient *client;
	GError *error = NULL;

	PURPLE_ASSERT_CONNECTION_IS_VALID(gc);
	g_return_val_if_fail(!purple_connection_is_disconnecting(gc), NULL);

	g_return_val_if_fail(host != NULL, NULL);
	g_return_val_if_fail(is_async, NULL);

	purple_debug_misc("gg", "ggp_tcpsocket_connect(%p, %s:%d, %s, %p)",
		gc, host, port, is_tls ? "tls" : "tcp", priv);

	client = purple_gio_socket_client_new(
			purple_connection_get_account(gc), &error);

	if (client == NULL) {
		purple_debug_error("gg", "unable to connect: %s",
				error->message);
		g_clear_error(&error);
		return NULL;
	}

	g_socket_client_set_tls(client, is_tls);

	data = g_new0(GGPTcpSocketData, 1);
	data->cancellable = g_cancellable_new();
	data->gc = gc;
	data->priv_gg = priv;

	g_socket_client_connect_to_host_async(client, host, port,
			data->cancellable, ggp_tcpsocket_connected, data);
	g_object_unref(client);

	return data;
}

static void
ggp_tcpsocket_close(void *_gc, void *_data)
{
	GGPTcpSocketData *data = _data;

	ggp_tcp_socket_data_free(data);
}

static ssize_t
ggp_tcpsocket_read(void *_gc, void *_data, unsigned char *buffer, size_t bufsize)
{
	GGPTcpSocketData *data = _data;
	GPollableInputStream *input;
	gssize ret;
	GError *error = NULL;

	if (data->conn == NULL) {
		return -1;
	}

	input = G_POLLABLE_INPUT_STREAM(
			g_io_stream_get_input_stream(G_IO_STREAM(data->conn)));
	ret = g_pollable_input_stream_read_nonblocking(input,
			buffer, bufsize, NULL, &error);

	if (ret < 0) {
		if (g_error_matches(error,
				G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK)) {
			errno = EAGAIN;
		} else {
			purple_debug_error("gg", "socket read error: %s",
					error->message);
		}

		g_clear_error(&error);
	}

	return ret;
}

static ssize_t
ggp_tcpsocket_write(void *_gc, void *_data, const unsigned char *data_buf, size_t len)
{
	GGPTcpSocketData *data = _data;
	GPollableOutputStream *output;
	gssize ret;
	GError *error = NULL;

	if (data->conn == NULL) {
		return -1;
	}

	output = G_POLLABLE_OUTPUT_STREAM(
			g_io_stream_get_output_stream(G_IO_STREAM(data->conn)));
	ret = g_pollable_output_stream_write_nonblocking(output,
			data_buf, len, NULL, &error);

	if (ret < 0) {
		if (g_error_matches(error,
				G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK)) {
			errno = EAGAIN;
		} else {
			purple_debug_error("gg", "socket write error: %s",
					error->message);
		}

		g_clear_error(&error);
	}

	return ret;
}

void
ggp_tcpsocket_setup(PurpleConnection *gc, struct gg_login_params *glp)
{
	glp->socket_manager_type = GG_SOCKET_MANAGER_TYPE_TLS;
	glp->socket_manager.cb_data = gc;
	glp->socket_manager.connect_cb = ggp_tcpsocket_connect;
	glp->socket_manager.close_cb = ggp_tcpsocket_close;
	glp->socket_manager.read_cb = ggp_tcpsocket_read;
	glp->socket_manager.write_cb = ggp_tcpsocket_write;
}

PurpleInputCondition
ggp_tcpsocket_inputcond_gg_to_purple(enum gg_check_t check)
{
	PurpleInputCondition cond = 0;

	if (check & GG_CHECK_READ)
		cond |= PURPLE_INPUT_READ;
	if (check & GG_CHECK_WRITE)
		cond |= PURPLE_INPUT_WRITE;

	return cond;
}
