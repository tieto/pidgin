/**
 * @file httpconn.h HTTP connection
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
#ifndef _MSN_HTTPCONN_H_
#define _MSN_HTTPCONN_H_

typedef struct _MsnHttpConn MsnHttpConn;

#include "servconn.h"

struct _MsnHttpConn
{
	MsnSession *session;
	MsnServConn *servconn;

	char *full_session_id;
	char *session_id;

	int timer;

	gboolean waiting_response;
	gboolean dirty; /**< The flag that states if we should poll. */
	gboolean connected;

	char *host;
	GList *queue;

	int fd;
	int inpa;

	char *rx_buf;
	int rx_len;

#if 0
	GQueue *servconn_queue;
#endif

	gboolean virgin;
};

MsnHttpConn *msn_httpconn_new(MsnServConn *servconn);
void msn_httpconn_destroy(MsnHttpConn *httpconn);
size_t msn_httpconn_write(MsnHttpConn *httpconn, const char *buf, size_t size);

gboolean msn_httpconn_connect(MsnHttpConn *httpconn,
							  const char *host, int port);
void msn_httpconn_disconnect(MsnHttpConn *httpconn);

#if 0
void msn_httpconn_queue_servconn(MsnHttpConn *httpconn, MsnServConn *servconn);
#endif

#if 0
/**
 * Initializes the HTTP data for a session.
 *
 * @param session The session.
 */
void msn_http_session_init(MsnSession *session);

/**
 * Uninitializes the HTTP data for a session.
 *
 * @param session The session.
 */
void msn_http_session_uninit(MsnSession *session);

/**
 * Writes data to the server using the HTTP connection method.
 *
 * @param servconn    The server connection.
 * @param buf         The data to write.
 * @param size        The size of the data to write.
 * @param server_type The optional server type.
 *
 * @return The number of bytes written.
 */
size_t msn_http_servconn_write(MsnServConn *servconn, const char *buf,
							   size_t size, const char *server_type);

/**
 * Polls the server for data.
 *
 * @param servconn The server connection.
 */
void msn_http_servconn_poll(MsnServConn *servconn);

/**
 * Processes an incoming message and returns a string the rest of MSN
 * can deal with.
 *
 * @param servconn The server connection.
 * @param buf      The incoming buffer.
 * @param size     The incoming size.
 * @param ret_buf  The returned buffer.
 * @param ret_len  The returned length.
 * @param error    TRUE if there was an HTTP error.
 *
 * @return TRUE if the returned buffer is ready to be processed.
 *         FALSE otherwise.
 */
gboolean msn_http_servconn_parse_data(MsnServConn *servconn,
									  const char *buf, size_t size,
									  char **ret_buf, size_t *ret_size,
									  gboolean *error);
#endif

#endif /* _MSN_HTTPCONN_H_ */
