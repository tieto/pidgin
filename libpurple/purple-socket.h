/**
 * @file purple-socket.h Generic sockets
 * @ingroup core
 */

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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#ifndef _PURPLE_SOCKET_H_
#define _PURPLE_SOCKET_H_

#include "connection.h"

/**
 * A structure holding all resources needed for the TCP connection.
 */
typedef struct _PurpleSocket PurpleSocket;

/**
 * A callback fired after (successfully or not) establishing a connection.
 *
 * @ps:        The socket.
 * @error:     Error message, or NULL if connection was successful.
 * @user_data: The user data passed with callback function.
 */
typedef void (*PurpleSocketConnectCb)(PurpleSocket *ps, const gchar *error,
	gpointer user_data);

/**
 * Creates new, disconnected socket.
 *
 * Passing a PurpleConnection allows for proper proxy handling.
 *
 * @gs: The connection for which the socket is needed, or NULL.
 *
 * Returns:   The new socket struct.
 */
PurpleSocket *
purple_socket_new(PurpleConnection *gc);

/**
 * Gets PurpleConnection tied with specified socket.
 *
 * @ps: The socket.
 *
 * Returns:   The PurpleConnection object.
 */
PurpleConnection *
purple_socket_get_connection(PurpleSocket *ps);

/**
 * Determines, if socket should handle TLS.
 *
 * @ps:     The socket.
 * @is_tls: TRUE, if TLS should be handled transparently, FALSE otherwise.
 */
void
purple_socket_set_tls(PurpleSocket *ps, gboolean is_tls);

/**
 * Sets connection host.
 *
 * @ps:   The socket.
 * @host: The connection host.
 */
void
purple_socket_set_host(PurpleSocket *ps, const gchar *host);

/**
 * Sets connection port.
 *
 * @ps:   The socket.
 * @port: The connection port.
 */
void
purple_socket_set_port(PurpleSocket *ps, int port);

/**
 * Establishes a connection.
 *
 * @ps:        The socket.
 * @cb:        The function to call after establishing a connection, or on
 *                  error.
 * @user_data: The user data to be passed to callback function.
 *
 * Returns: TRUE on success (this doesn't mean it's connected yet), FALSE
 *         otherwise.
 */
gboolean
purple_socket_connect(PurpleSocket *ps, PurpleSocketConnectCb cb,
	gpointer user_data);

/**
 * Reads incoming data from socket.
 *
 * This function deals with TLS, if the socket is configured to do it.
 *
 * @ps:  The socket.
 * @buf: The buffer to write data to.
 * @len: The buffer size.
 *
 * Returns: Amount of data written, or -1 on error (errno will be also be set).
 */
gssize
purple_socket_read(PurpleSocket *ps, guchar *buf, size_t len);

/**
 * Sends data through socket.
 *
 * This function deals with TLS, if the socket is configured to do it.
 *
 * @ps:  The socket.
 * @buf: The buffer to read data from.
 * @len: The amount of data to read and send.
 *
 * @Amount: of data sent, or -1 on error (errno will albo be set).
 */
gssize
purple_socket_write(PurpleSocket *ps, const guchar *buf, size_t len);

/**
 * Adds an input handler for the socket.
 *
 * If the specified socket had input handler already registered, it will be
 * removed. To remove any input handlers, pass an NULL handler function.
 *
 * @ps:        The socket.
 * @cond:      The condition type.
 * @func:      The callback function for data, or NULL to remove any
 *                  existing callbacks.
 * @user_data: The user data to be passed to callback function.
 */
void
purple_socket_watch(PurpleSocket *ps, PurpleInputCondition cond,
	PurpleInputFunction func, gpointer user_data);

/**
 * Gets underlying file descriptor for socket.
 *
 * It's not meant to read/write data (use purple_socket_read/
 * purple_socket_write), rather for watching for changes with select().
 *
 * @ps: The socket
 *
 * Returns: The file descriptor, or -1 on error.
 */
int
purple_socket_get_fd(PurpleSocket *ps);

/**
 * Sets extra data for a socket.
 *
 * @ps:   The socket.
 * @key:  The unique key.
 * @data: The data to assign, or NULL to remove.
 */
void
purple_socket_set_data(PurpleSocket *ps, const gchar *key, gpointer data);

/**
 * Returns extra data in a socket.
 *
 * @ps:  The socket.
 * @key: The unqiue key.
 *
 * Returns: The data associated with the key.
 */
gpointer
purple_socket_get_data(PurpleSocket *ps, const gchar *key);

/**
 * Destroys the socket, closes connection and frees all resources.
 *
 * If file descriptor for the socket was extracted with purple_socket_get_fd and
 * added to event loop, it have to be removed prior this.
 *
 * @ps: The socket.
 */
void
purple_socket_destroy(PurpleSocket *ps);

#endif /* _PURPLE_SOCKET_H_ */
