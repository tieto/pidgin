/**
 * @file sslconn.h SSL API
 * @ingroup core
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
#ifndef _GAIM_SSLCONN_H_
#define _GAIM_SSLCONN_H_

#include "proxy.h"

#define GAIM_SSL_DEFAULT_PORT 443

typedef enum
{
	GAIM_SSL_HANDSHAKE_FAILED = 1,
	GAIM_SSL_CONNECT_FAILED = 2
} GaimSslErrorType;

typedef struct _GaimSslConnection GaimSslConnection;

typedef void (*GaimSslInputFunction)(gpointer, GaimSslConnection *,
									 GaimInputCondition);
typedef void (*GaimSslErrorFunction)(GaimSslConnection *, GaimSslErrorType,
									 gpointer);

struct _GaimSslConnection
{
	char *host;
	int port;
	void *connect_cb_data;
	GaimSslInputFunction connect_cb;
	GaimSslErrorFunction error_cb;
	void *recv_cb_data;
	GaimSslInputFunction recv_cb;

	int fd;
	int inpa;
	GaimProxyConnectData *connect_data;

	void *private_data;
};

/**
 * SSL implementation operations structure.
 *
 * Every SSL implementation must provide all of these and register it.
 */
typedef struct
{
	gboolean (*init)(void);
	void (*uninit)(void);
	void (*connectfunc)(GaimSslConnection *gsc);
	void (*close)(GaimSslConnection *gsc);
	size_t (*read)(GaimSslConnection *gsc, void *data, size_t len);
	size_t (*write)(GaimSslConnection *gsc, const void *data, size_t len);

} GaimSslOps;

#ifdef __cplusplus
extern "C" {
#endif

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
 * Makes a SSL connection to the specified host and port.  The caller
 * should keep track of the returned value and use it to cancel the
 * connection, if needed.
 *
 * @param account    The account making the connection.
 * @param host       The destination host.
 * @param port       The destination port.
 * @param func       The SSL input handler function.
 * @param error_func The SSL error handler function.  This function
 *                   should NOT call gaim_ssl_close().  In the event
 *                   of an error the GaimSslConnection will be
 *                   destroyed for you.
 * @param data       User-defined data.
 *
 * @return The SSL connection handle.
 */
GaimSslConnection *gaim_ssl_connect(GaimAccount *account, const char *host,
									int port, GaimSslInputFunction func,
									GaimSslErrorFunction error_func,
									void *data);

/**
 * Makes a SSL connection using an already open file descriptor.
 *
 * @param account    The account making the connection.
 * @param fd         The file descriptor.
 * @param func       The SSL input handler function.
 * @param error_func The SSL error handler function.
 * @param data       User-defined data.
 *
 * @return The SSL connection handle.
 */
GaimSslConnection *gaim_ssl_connect_fd(GaimAccount *account, int fd,
									   GaimSslInputFunction func,
									   GaimSslErrorFunction error_func,
									   void *data);

/**
 * Adds an input watcher for the specified SSL connection.
 *
 * @param gsc   The SSL connection handle.
 * @param func  The callback function.
 * @param data  User-defined data.
 */
void gaim_ssl_input_add(GaimSslConnection *gsc, GaimSslInputFunction func,
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
 * Sets the current SSL operations structure.
 *
 * @param ops The SSL operations structure to assign.
 */
void gaim_ssl_set_ops(GaimSslOps *ops);

/**
 * Returns the current SSL operations structure.
 *
 * @return The SSL operations structure.
 */
GaimSslOps *gaim_ssl_get_ops(void);

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

#endif /* _GAIM_SSLCONN_H_ */
