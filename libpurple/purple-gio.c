/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include "internal.h"
#include "proxy.h"
#include "purple-gio.h"
#include "tls-certificate.h"

typedef struct {
	GIOStream *stream;
	GInputStream *input;
	GOutputStream *output;
} GracefulCloseData;

static gboolean
graceful_close_cb(gpointer user_data)
{
	GracefulCloseData *data = user_data;
	GError *error = NULL;

	if (g_input_stream_has_pending(data->input) ||
			g_output_stream_has_pending(data->output)) {
		/* Has pending operations. Not ready to close yet.
		 * Try again later.
		 */
		return G_SOURCE_CONTINUE;
	}

	/* Finally can gracefully close */

	/* Close input stream, from wrapper or GIOStream */
	if (!g_input_stream_close(data->input, NULL, &error)) {
		purple_debug_warning("gio",
				"Error closing input stream: %s",
				error->message);
		g_clear_error(&error);
	}

	g_clear_object(&data->input);

	/* Close output stream, from wrapper or GIOStream */
	if (!g_output_stream_close(data->output, NULL, &error)) {
		purple_debug_warning("gio",
				"Error closing output stream: %s",
				error->message);
		g_clear_error(&error);
	}

	g_clear_object(&data->output);

	/* Close io stream */
	if (!g_io_stream_close(data->stream, NULL, &error)) {
		purple_debug_warning("gio",
				"Error closing stream: %s",
				error->message);
		g_clear_error(&error);
	}

	g_clear_object(&data->stream);

	/* Clean up */
	g_free(data);
	return G_SOURCE_REMOVE;
}

void
purple_gio_graceful_close(GIOStream *stream,
		GInputStream *input, GOutputStream *output)
{
	GracefulCloseData *data;

	g_return_if_fail(G_IS_IO_STREAM(stream));
	g_return_if_fail(input == NULL || G_IS_INPUT_STREAM(input));
	g_return_if_fail(output == NULL || G_IS_OUTPUT_STREAM(output));

	data = g_new(GracefulCloseData, 1);
	data->stream = g_object_ref(stream);

	if (input == NULL)
		input = g_io_stream_get_input_stream(stream);
	data->input = g_object_ref(input);

	if (output == NULL)
		output = g_io_stream_get_output_stream(stream);
	data->output = g_object_ref(output);

	/* Try gracefully closing the stream synchronously */
	if (graceful_close_cb(data) == G_SOURCE_CONTINUE) {
		/* Has pending operations. Do so asynchronously */
		g_idle_add(graceful_close_cb, data);
	}
}

GSocketClient *
purple_gio_socket_client_new(PurpleAccount *account, GError **error)
{
	GProxyResolver *resolver;
	GSocketClient *client;

	resolver = purple_proxy_get_proxy_resolver(account, error);

	if (resolver == NULL) {
		return NULL;
	}

	client = g_socket_client_new();
	g_socket_client_set_proxy_resolver(client, resolver);
	g_object_unref(resolver);

	/* Attach purple's tls certificate handler in case tls is used */
	purple_tls_certificate_attach_to_socket_client(client);

	return client;
}

