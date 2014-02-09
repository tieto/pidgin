/* purple
 *
 * Copyright (C) 2005, Thomas Butter <butter@uni-mannheim.de>
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

#ifndef _PURPLE_DNSSRV_H
#define _PURPLE_DNSSRV_H
/**
 * SECTION:dnssrv
 * @section_id: libpurple-dnssrv
 * @short_description: <filename>dnssrv.h</filename>
 * @title: DNS SRV Utilities
 */

typedef struct _PurpleSrvTxtQueryData PurpleSrvTxtQueryData;
typedef struct _PurpleSrvResponse PurpleSrvResponse;
typedef struct _PurpleTxtResponse PurpleTxtResponse;
typedef struct _PurpleSrvTxtQueryUiOps PurpleSrvTxtQueryUiOps;

#include <glib.h>

enum PurpleDnsType {
	PurpleDnsTypeTxt = 16,
	PurpleDnsTypeSrv = 33
};

struct _PurpleSrvResponse {
	char hostname[256];
	int port;
	int weight;
	int pref;
};

struct _PurpleTxtResponse {
	char *content;
};

typedef void  (*PurpleSrvTxtQueryResolvedCallback) (PurpleSrvTxtQueryData *query_data, GList *records);
typedef void  (*PurpleSrvTxtQueryFailedCallback) (PurpleSrvTxtQueryData *query_data, const gchar *error_message);

/**
 * PurpleSrvTxtQueryUiOps:
 * @resolve: implemented, return %TRUE if the UI takes responsibility for SRV
 *           queries. When returning %FALSE, the standard implementation is
 *           used. These callbacks <emphasis>MUST</emphasis> be called
 *           asynchronously.
 * @destroy: Called just before @query_data is freed; this should cancel any
 *           further use of @query_data the UI would make. Unneeded if @resolve
 *           is not implemented.
 *
 * SRV Request UI operations;  UIs should implement this if they want to do SRV
 * lookups themselves, rather than relying on the core.
 *
 * See <link linkend="chapter-ui-ops">List of <literal>UiOps</literal> Structures</link>
 */
struct _PurpleSrvTxtQueryUiOps
{
	gboolean (*resolve)(PurpleSrvTxtQueryData *query_data,
	                    PurpleSrvTxtQueryResolvedCallback resolved_cb,
	                    PurpleSrvTxtQueryFailedCallback failed_cb);

	void (*destroy)(PurpleSrvTxtQueryData *query_data);

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * PurpleSrvCallback:
 * @resp: An array of PurpleSrvResponse of size results.  The array
 *        is sorted based on the order described in the DNS SRV RFC.
 *        Users of this API should try each record in resp in order,
 *        starting at the beginning.
 */
typedef void (*PurpleSrvCallback)(PurpleSrvResponse *resp, int results, gpointer data);

/**
 * PurpleTxtCallback:
 * @responses:   A GList of PurpleTxtResponse objects.
 * @data:        The extra data passed to purple_txt_resolve.
 *
 * Callback that returns the data retrieved from a DNS TXT lookup.
 */
typedef void (*PurpleTxtCallback)(GList *responses, gpointer data);

G_BEGIN_DECLS

/**
 * purple_srv_resolve:
 * @account:   The account that the query is being done for (or NULL)
 * @protocol:  Name of the protocol (e.g. "sip")
 * @transport: Name of the transport ("tcp" or "udp")
 * @domain:    Domain name to query (e.g. "blubb.com")
 * @cb:        A callback which will be called with the results
 * @extradata: Extra data to be passed to the callback
 *
 * Queries an SRV record.
 *
 * Returns: NULL if there was an error, otherwise return a reference to
 *         a data structure that can be used to cancel the pending
 *         DNS query, if needed.
 */
PurpleSrvTxtQueryData *purple_srv_resolve(PurpleAccount *account, const char *protocol, const char *transport, const char *domain, PurpleSrvCallback cb, gpointer extradata);

/**
 * purple_txt_resolve:
 * @account:   The account that the query is being done for (or NULL)
 * @owner:     Name of the protocol (e.g. "_xmppconnect")
 * @domain:    Domain name to query (e.g. "blubb.com")
 * @cb:        A callback which will be called with the results
 * @extradata: Extra data to be passed to the callback
 *
 * Queries an TXT record.
 *
 * Returns: NULL if there was an error, otherwise return a reference to
 *         a data structure that can be used to cancel the pending
 *         DNS query, if needed.
 */
PurpleSrvTxtQueryData *purple_txt_resolve(PurpleAccount *account, const char *owner, const char *domain, PurpleTxtCallback cb, gpointer extradata);

/**
 * purple_txt_response_get_content:
 * @response:  The TXT response record
 *
 * Get the value of the current TXT record.
 *
 * Returns: The value of the current TXT record.
 */
const gchar *purple_txt_response_get_content(PurpleTxtResponse *response);

/**
 * purple_txt_response_destroy:
 * @response: The PurpleTxtResponse to destroy.
 *
 * Destroy a TXT DNS response object.
 */
void purple_txt_response_destroy(PurpleTxtResponse *response);

/**
 * purple_srv_txt_query_destroy:
 * @query_data: The SRV/TXT query to cancel.  This data structure
 *        is freed by this function.
 *
 * Cancel a SRV/TXT query and destroy the associated data structure.
 */
void purple_srv_txt_query_destroy(PurpleSrvTxtQueryData *query_data);

/**
 * purple_srv_txt_query_set_ui_ops:
 * @ops: The UI operations structure.
 *
 * Sets the UI operations structure to be used when doing a SRV/TXT
 * resolve.  The UI operations need only be set if the UI wants to
 * handle the resolve itself; otherwise, leave it as NULL.
 */
void purple_srv_txt_query_set_ui_ops(PurpleSrvTxtQueryUiOps *ops);

/**
 * purple_srv_txt_query_get_ui_ops:
 *
 * Returns the UI operations structure to be used when doing a SRV/TXT
 * resolve.
 *
 * Returns: The UI operations structure.
 */
PurpleSrvTxtQueryUiOps *purple_srv_txt_query_get_ui_ops(void);

/**
 * purple_srv_txt_query_get_query:
 * @query_data: The SRV/TXT query
 *
 * Get the query from a PurpleSrvTxtQueryData
 *
 * Returns: The query.
 */
char *purple_srv_txt_query_get_query(PurpleSrvTxtQueryData *query_data);

/**
 * purple_srv_txt_query_get_query_type:
 * @query_data: The query
 *
 * Get the type from a PurpleSrvTxtQueryData (TXT or SRV)
 *
 * Returns: The query type.
 */
int purple_srv_txt_query_get_query_type(PurpleSrvTxtQueryData *query_data);

G_END_DECLS

#endif /* _PURPLE_DNSSRV_H */

