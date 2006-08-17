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

typedef struct _GaimDnsqueryData GaimDnsqueryData;

/**
 * The "hosts" parameter is a linked list containing pairs of
 * one size_t addrlen and one struct sockaddr *addr.
 */
typedef void (*GaimDnsqueryConnectFunction)(GSList *hosts, gpointer data, const char *error_message);


#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************/
/** @name DNS query API                                                       */
/**************************************************************************/
/*@{*/

/**
 * Do an asynchronous DNS query.
 *
 * @param hostname The hostname to resolve
 * @param port A portnumber which is stored in the struct sockaddr
 * @param callback Callback to call after resolving
 * @param data Extra data for the callback function
 *
 * @return NULL if there was an error, or a reference to a data
 *         structure that can be used to cancel the pending
 *         connection, if needed.
 */
GaimDnsqueryData *gaim_dnsquery_a(const char *hostname, int port, GaimDnsqueryConnectFunction callback, gpointer data);

/**
 * Cancel a DNS query.
 *
 * @param query_data A pointer to the DNS query data that you want
 *        to cancel.
 */
void gaim_dnsquery_destroy(GaimDnsqueryData *query_data);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* _GAIM_DNSQUERY_H_ */
