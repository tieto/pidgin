/**
 * @file sslconn.h SSL API
 * @ingroup core
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
#ifndef _GAIM_SSL_H_
#define _GAIM_SSL_H_

#include "proxy.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GAIM_SSL_DEFAULT_PORT 443

typedef void *GaimSslConnection;
typedef void (*GaimSslInputFunction)(gpointer, GaimSslConnection *,
									 GaimInputCondition);

/**************************************************************************/
/** @name SSL API                                                         */
/**************************************************************************/
/*@{*/

/**
 * Returns whether or not SSL is currently supported.
 *
 * @return TRUE if SSL is supported, or FALSE otherwise.
 */
gboolean gaim_ssl_is_supported(void);

/**
 * Makes a SSL connection to the specified host and port.
 *
 * @param account The account making the connection.
 * @param host    The destination host.
 * @param port    The destination port.
 * @param func    The SSL input handler function.
 * @param data    User-defined data.
 *
 * @return The SSL connection handle.
 */
GaimSslConnection *gaim_ssl_connect(GaimAccount *account, const char *host,
									int port, GaimSslInputFunction func,
									void *data);

/**
 * Closes a SSL connection.
 *
 * @param gsc The SSL connection to close.
 */
void gaim_ssl_close(GaimSslConnection *gsc);

/**
 * Reads data from an SSL connection.
 *
 * @param gsc    The SSL connection handle.
 * @param buffer The destination buffer.
 * @param len    The maximum number of bytes to read.
 *
 * @return The number of bytes read.
 */
size_t gaim_ssl_read(GaimSslConnection *gsc, void *buffer, size_t len);

/**
 * Writes data to an SSL connection.
 *
 * @param gsc    The SSL connection handle.
 * @param buffer The buffer to write.
 * @param len    The length of the data to write.
 *
 * @return The number of bytes written.
 */
size_t gaim_ssl_write(GaimSslConnection *gsc, const void *buffer, size_t len);

/*@}*/

/**************************************************************************/
/** @name Subsystem API                                                   */
/**************************************************************************/
/*@{*/

/**
 * Initializes the SSL subsystem.
 */
void gaim_ssl_init(void);

/**
 * Uninitializes the SSL subsystem.
 */
void gaim_ssl_uninit(void);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* _GAIM_SSL_H_ */
