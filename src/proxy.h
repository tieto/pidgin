/**
 * @file proxy.h Proxy API
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
#ifndef _GAIM_PROXY_H_
#define _GAIM_PROXY_H_

#include <glib.h>

/**
 * A type of proxy connection.
 */
typedef enum
{
	GAIM_PROXY_USE_GLOBAL = -1,  /**< Use the global proxy information. */
	GAIM_PROXY_NONE = 0,         /**< No proxy.                         */
	GAIM_PROXY_HTTP,             /**< HTTP proxy.                       */
	GAIM_PROXY_SOCKS4,           /**< SOCKS 4 proxy.                    */
	GAIM_PROXY_SOCKS5,           /**< SOCKS 5 proxy.                    */
	GAIM_PROXY_USE_ENVVAR        /**< Use environmental settings.       */

} GaimProxyType;

/**
 * An input condition.
 */
typedef enum
{
	GAIM_INPUT_READ  = 1 << 0,  /**< A read condition.  */
	GAIM_INPUT_WRITE = 1 << 1   /**< A write condition. */

} GaimInputCondition;

/**
 * Information on proxy settings.
 */
typedef struct
{
	GaimProxyType type;   /**< The proxy type.  */

	char *host;           /**< The host.        */
	int   port;           /**< The port number. */
	char *username;       /**< The username.    */
	char *password;       /**< The password.    */

} GaimProxyInfo;

typedef void (*GaimInputFunction)(gpointer, gint, GaimInputCondition);


#include "account.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************/
/** @name Proxy structure API                                             */
/**************************************************************************/
/*@{*/

/**
 * Creates a proxy information structure.
 *
 * @return The proxy information structure.
 */
GaimProxyInfo *gaim_proxy_info_new(void);

/**
 * Destroys a proxy information structure.
 *
 * @param info The proxy information structure to destroy.
 */
void gaim_proxy_info_destroy(GaimProxyInfo *info);

/**
 * Sets the type of proxy.
 *
 * @param info The proxy information.
 * @param type The proxy type.
 */
void gaim_proxy_info_set_type(GaimProxyInfo *info, GaimProxyType type);

/**
 * Sets the proxy host.
 *
 * @param info The proxy information.
 * @param host The host.
 */
void gaim_proxy_info_set_host(GaimProxyInfo *info, const char *host);

/**
 * Sets the proxy port.
 *
 * @param info The proxy information.
 * @param port The port.
 */
void gaim_proxy_info_set_port(GaimProxyInfo *info, int port);

/**
 * Sets the proxy username.
 *
 * @param info     The proxy information.
 * @param username The username.
 */
void gaim_proxy_info_set_username(GaimProxyInfo *info, const char *username);

/**
 * Sets the proxy password.
 *
 * @param info     The proxy information.
 * @param password The password.
 */
void gaim_proxy_info_set_password(GaimProxyInfo *info, const char *password);

/**
 * Returns the proxy's type.
 *
 * @param info The proxy information.
 *
 * @return The type.
 */
GaimProxyType gaim_proxy_info_get_type(const GaimProxyInfo *info);

/**
 * Returns the proxy's host.
 *
 * @param info The proxy information.
 *
 * @return The host.
 */
const char *gaim_proxy_info_get_host(const GaimProxyInfo *info);

/**
 * Returns the proxy's port.
 *
 * @param info The proxy information.
 *
 * @return The port.
 */
int gaim_proxy_info_get_port(const GaimProxyInfo *info);

/**
 * Returns the proxy's username.
 *
 * @param info The proxy information.
 *
 * @return The username.
 */
const char *gaim_proxy_info_get_username(const GaimProxyInfo *info);

/**
 * Returns the proxy's password.
 *
 * @param info The proxy information.
 *
 * @return The password.
 */
const char *gaim_proxy_info_get_password(const GaimProxyInfo *info);

/*@}*/

/**************************************************************************/
/** @name Global Proxy API                                                */
/**************************************************************************/
/*@{*/

/**
 * Returns gaim's global proxy information.
 *
 * @return The global proxy information.
 */
GaimProxyInfo *gaim_global_proxy_get_info(void);

/*@}*/

/**************************************************************************/
/** @name Proxy API                                                       */
/**************************************************************************/
/*@{*/

/**
 * Initializes the proxy subsystem.
 */
void gaim_proxy_init(void);

/**
 * Adds an input handler.
 *
 * @param source    The input source.
 * @param cond      The condition type.
 * @param func      The callback function for data.
 * @param user_data User-specified data.
 *
 * @return The resulting handle.
 */
gint gaim_input_add(int source, GaimInputCondition cond,
					GaimInputFunction func, gpointer user_data);

/**
 * Removes an input handler.
 *
 * @param handle The handle of the input handler.
 */
void gaim_input_remove(gint handle);

/**
 * Makes a connection to the specified host and port.
 *
 * @param account The account making the connection.
 * @param host    The destination host.
 * @param port    The destination port.
 * @param func    The input handler function.
 * @param data    User-defined data.
 *
 * @return The socket handle.
 */
int gaim_proxy_connect(GaimAccount *account, const char *host, int port,
					   GaimInputFunction func, gpointer data);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* _GAIM_PROXY_H_ */
