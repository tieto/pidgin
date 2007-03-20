/**
 * @file sslconn.h SSL API
 * @ingroup core
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
#ifndef _PURPLE_SSLCONN_H_
#define _PURPLE_SSLCONN_H_

#include "proxy.h"

#define PURPLE_SSL_DEFAULT_PORT 443

typedef enum
{
	PURPLE_SSL_HANDSHAKE_FAILED = 1,
	PURPLE_SSL_CONNECT_FAILED = 2
} PurpleSslErrorType;

typedef struct _PurpleSslConnection PurpleSslConnection;

typedef void (*PurpleSslInputFunction)(gpointer, PurpleSslConnection *,
									 PurpleInputCondition);
typedef void (*PurpleSslErrorFunction)(PurpleSslConnection *, PurpleSslErrorType,
									 gpointer);

struct _PurpleSslConnection
{
	char *host;
	int port;
	void *connect_cb_data;
	PurpleSslInputFunction connect_cb;
	PurpleSslErrorFunction error_cb;
	void *recv_cb_data;
	PurpleSslInputFunction recv_cb;

	int fd;
	int inpa;
	PurpleProxyConnectData *connect_data;

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
	void (*connectfunc)(PurpleSslConnection *gsc);
	void (*close)(PurpleSslConnection *gsc);
	size_t (*read)(PurpleSslConnection *gsc, void *data, size_t len);
	size_t (*write)(PurpleSslConnection *gsc, const void *data, size_t len);

} PurpleSslOps;

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
gboolean purple_ssl_is_supported(void);

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
 *                   should NOT call purple_ssl_close().  In the event
 *                   of an error the PurpleSslConnection will be
 *                   destroyed for you.
 * @param data       User-defined data.
 *
 * @return The SSL connection handle.
 */
PurpleSslConnection *purple_ssl_connect(PurpleAccount *account, const char *host,
									int port, PurpleSslInputFunction func,
									PurpleSslErrorFunction error_func,
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
PurpleSslConnection *purple_ssl_connect_fd(PurpleAccount *account, int fd,
									   PurpleSslInputFunction func,
									   PurpleSslErrorFunction error_func,
									   void *data);

/**
 * Adds an input watcher for the specified SSL connection.
 *
 * @param gsc   The SSL connection handle.
 * @param func  The callback function.
 * @param data  User-defined data.
 */
void purple_ssl_input_add(PurpleSslConnection *gsc, PurpleSslInputFunction func,
						void *data);

/**
 * Closes a SSL connection.
 *
 * @param gsc The SSL connection to close.
 */
void purple_ssl_close(PurpleSslConnection *gsc);

/**
 * Reads data from an SSL connection.
 *
 * @param gsc    The SSL connection handle.
 * @param buffer The destination buffer.
 * @param len    The maximum number of bytes to read.
 *
 * @return The number of bytes read.
 */
size_t purple_ssl_read(PurpleSslConnection *gsc, void *buffer, size_t len);

/**
 * Writes data to an SSL connection.
 *
 * @param gsc    The SSL connection handle.
 * @param buffer The buffer to write.
 * @param len    The length of the data to write.
 *
 * @return The number of bytes written.
 */
size_t purple_ssl_write(PurpleSslConnection *gsc, const void *buffer, size_t len);

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
void purple_ssl_set_ops(PurpleSslOps *ops);

/**
 * Returns the current SSL operations structure.
 *
 * @return The SSL operations structure.
 */
PurpleSslOps *purple_ssl_get_ops(void);

/**
 * Initializes the SSL subsystem.
 */
void purple_ssl_init(void);

/**
 * Uninitializes the SSL subsystem.
 */
void purple_ssl_uninit(void);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* _PURPLE_SSLCONN_H_ */
