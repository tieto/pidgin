/**
 * @file dnsquery.h DNS query API
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
#ifndef _GAIM_DNSQUERY_H_
#define _GAIM_DNSQUERY_H_

#include <glib.h>
#include "eventloop.h"

/**
 * The "hosts" parameter is a linked list containing pairs of
 * one size_t addrlen and one struct sockaddr *addr.
 */
typedef void (*GaimProxyDnsConnectFunction)(GSList *hosts, gpointer data, const char *error_message);


#include "account.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************/
/** @name DNS query API                                                   */
/**************************************************************************/
/*@{*/

/**
 * Do an async dns query
 *
 * @param hostname The hostname to resolve
 * @param port A portnumber which is stored in the struct sockaddr
 * @param callback Callback to call after resolving
 * @param data Extra data for the callback function
 *
 * @return Zero indicates the connection is pending. Any other value indicates failure.
 */
int gaim_gethostbyname_async(const char *hostname, int port, GaimProxyDnsConnectFunction callback, gpointer data);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* _GAIM_DNSQUERY_H_ */
