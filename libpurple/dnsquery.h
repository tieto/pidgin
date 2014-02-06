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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
/**
 * SECTION:dnsquery
 * @section_id: libpurple-dnsquery
 * @short_description: <filename>dnsquery.h</filename>
 * @title: DNS Query API
 */

#ifndef _PURPLE_DNSQUERY_H_
#define _PURPLE_DNSQUERY_H_

#include <glib.h>
#include "eventloop.h"
#include "account.h"

/**
 * PurpleDnsQueryData:
 *
 * An opaque structure representing a DNS query.  The hostname and port
 * associated with the query can be retrieved using
 * purple_dnsquery_get_host() and purple_dnsquery_get_port().
 */
typedef struct _PurpleDnsQueryData PurpleDnsQueryData;
typedef struct _PurpleDnsQueryUiOps PurpleDnsQueryUiOps;

/**
 * PurpleDnsQueryConnectFunction:
 *
 * The "hosts" parameter is a linked list containing pairs of
 * one size_t addrlen and one struct sockaddr *addr.  It should
 * be free'd by the callback function.
 */
typedef void (*PurpleDnsQueryConnectFunction)(GSList *hosts, gpointer data, const char *error_message);

/**
 * PurpleDnsQueryResolvedCallback:
 *
 * DNS query resolved callback used by the UI if it handles resolving DNS
 */
typedef void  (*PurpleDnsQueryResolvedCallback) (PurpleDnsQueryData *query_data, GSList *hosts);

/**
 * PurpleDnsQueryFailedCallback:
 *
 * DNS query failed callback used by the UI if it handles resolving DNS
 */
typedef void  (*PurpleDnsQueryFailedCallback) (PurpleDnsQueryData *query_data, const gchar *error_message);

/**
 * PurpleDnsQueryUiOps:
 * @resolve_host: If implemented, return %TRUE if the UI takes responsibility
 *                for DNS queries. When returning %FALSE, the standard
 *                implementation is used.
 * @destroy:      Called just before @query_data is freed; this should cancel
 *                any further use of @query_data the UI would make. Unneeded if
 *                @resolve_host is not implemented.
 *
 * DNS Request UI operations;  UIs should implement this if they want to do DNS
 * lookups themselves, rather than relying on the core.
 *
 * See <link linkend="chapter-ui-ops">List of <literal>UiOps</literal>
 *     Structures</link>
 */
struct _PurpleDnsQueryUiOps
{
	gboolean (*resolve_host)(PurpleDnsQueryData *query_data,
	                         PurpleDnsQueryResolvedCallback resolved_cb,
	                         PurpleDnsQueryFailedCallback failed_cb);

	void (*destroy)(PurpleDnsQueryData *query_data);

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

/**************************************************************************/
/* DNS query API                                                          */
/**************************************************************************/
/*@{*/

/**
 * purple_dnsquery_a:
 * @account:  The account that the query is being done for (or NULL)
 * @hostname: The hostname to resolve.
 * @port:     A port number which is stored in the struct sockaddr.
 * @callback: The callback function to call after resolving.
 * @data:     Extra data to pass to the callback function.
 *
 * Perform an asynchronous DNS query.
 *
 * Returns: NULL if there was an error, otherwise return a reference to
 *         a data structure that can be used to cancel the pending
 *         DNS query, if needed.
 *
 */
PurpleDnsQueryData *purple_dnsquery_a(PurpleAccount *account, const char *hostname, int port, PurpleDnsQueryConnectFunction callback, gpointer data);

/**
 * purple_dnsquery_destroy:
 * @query_data: The DNS query to cancel.  This data structure
 *        is freed by this function.
 *
 * Cancel a DNS query and destroy the associated data structure.
 */
void purple_dnsquery_destroy(PurpleDnsQueryData *query_data);

/**
 * purple_dnsquery_set_ui_ops:
 * @ops: The UI operations structure.
 *
 * Sets the UI operations structure to be used when doing a DNS
 * resolve.  The UI operations need only be set if the UI wants to
 * handle the resolve itself; otherwise, leave it as NULL.
 */
void purple_dnsquery_set_ui_ops(PurpleDnsQueryUiOps *ops);

/**
 * purple_dnsquery_get_ui_ops:
 *
 * Returns the UI operations structure to be used when doing a DNS
 * resolve.
 *
 * Returns: The UI operations structure.
 */
PurpleDnsQueryUiOps *purple_dnsquery_get_ui_ops(void);

/**
 * purple_dnsquery_get_host:
 * @query_data: The DNS query
 *
 * Get the host associated with a PurpleDnsQueryData
 *
 * Returns: The host.
 */
char *purple_dnsquery_get_host(PurpleDnsQueryData *query_data);

/**
 * purple_dnsquery_get_port:
 * @query_data: The DNS query
 *
 * Get the port associated with a PurpleDnsQueryData
 *
 * Returns: The port.
 */
unsigned short purple_dnsquery_get_port(PurpleDnsQueryData *query_data);

/**
 * purple_dnsquery_init:
 *
 * Initializes the DNS query subsystem.
 */
void purple_dnsquery_init(void);

/**
 * purple_dnsquery_uninit:
 *
 * Uninitializes the DNS query subsystem.
 */
void purple_dnsquery_uninit(void);

/*@}*/

G_END_DECLS

#endif /* _PURPLE_DNSQUERY_H_ */
