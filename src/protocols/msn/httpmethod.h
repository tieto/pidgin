/**
 * @file httpmethod.h HTTP connection method
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
#ifndef _MSN_HTTP_METHOD_H_
#define _MSN_HTTP_METHOD_H_

typedef struct _MsnHttpMethodData MsnHttpMethodData;

#include "servconn.h"

struct _MsnHttpMethodData
{
	char *session_id;
	char *old_gateway_ip;
	char *gateway_ip;
	const char *server_type;

	int timer;

	gboolean virgin;
	gboolean waiting_response;

	GList *queue;
};

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

#endif /* _MSN_HTTP_METHOD_H_ */
