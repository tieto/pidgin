/**
 * @file dnssrv.h
 */

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

typedef struct _PurpleSrvTxtQueryData PurpleSrvTxtQueryData;
typedef struct _PurpleSrvResponse PurpleSrvResponse;
typedef struct _PurpleTxtResponse PurpleTxtResponse;

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
 * SRV Request UI operations;  UIs should implement this if they want to do SRV
 * lookups themselves, rather than relying on the core.
 *
 * @see @ref ui-ops
 */
typedef struct
{
	/** If implemented, return TRUE if the UI takes responsibility for SRV
	  * queries. When returning FALSE, the standard implementation is used. 
	  * These callbacks MUST be called asynchronously. */
	gboolean (*resolve)(PurpleSrvTxtQueryData *query_data,
	                    PurpleSrvTxtQueryResolvedCallback resolved_cb,
	                    PurpleSrvTxtQueryFailedCallback failed_cb);

	/** Called just before @a query_data is freed; this should cancel any
	 *  further use of @a query_data the UI would make. Unneeded if
	 *  #resolve is not implemented.
	 */
	void (*destroy)(PurpleSrvTxtQueryData *query_data);

	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
} PurpleSrvTxtQueryUiOps;

/**
 * @param resp An array of PurpleSrvResponse of size results.  The array
 *        is sorted based on the order described in the DNS SRV RFC.
 *        Users of this API should try each record in resp in order,
 *        starting at the beginning.
 */
typedef void (*PurpleSrvCallback)(PurpleSrvResponse *resp, int results, gpointer data);

/**
 * Callback that returns the data retrieved from a DNS TXT lookup.
 *
 * @param responses   A GList of PurpleTxtResponse objects.
 * @param data        The extra data passed to purple_txt_resolve.
 */
typedef void (*PurpleTxtCallback)(GList *responses, gpointer data);

G_BEGIN_DECLS

/**
 * Queries an SRV record.
 *
 * @param account   The account that the query is being done for (or NULL)
 * @param protocol  Name of the protocol (e.g. "sip")
 * @param transport Name of the transport ("tcp" or "udp")
 * @param domain    Domain name to query (e.g. "blubb.com")
 * @param cb        A callback which will be called with the results
 * @param extradata Extra data to be passed to the callback
 *
 * @return NULL if there was an error, otherwise return a reference to
 *         a data structure that can be used to cancel the pending
 *         DNS query, if needed.
 */
PurpleSrvTxtQueryData *purple_srv_resolve(PurpleAccount *account, const char *protocol, const char *transport, const char *domain, PurpleSrvCallback cb, gpointer extradata);

/**
 * Queries an TXT record.
 *
 * @param account   The account that the query is being done for (or NULL)
 * @param owner     Name of the protocol (e.g. "_xmppconnect")
 * @param domain    Domain name to query (e.g. "blubb.com")
 * @param cb        A callback which will be called with the results
 * @param extradata Extra data to be passed to the callback
 *
 * @return NULL if there was an error, otherwise return a reference to
 *         a data structure that can be used to cancel the pending
 *         DNS query, if needed.
 */
PurpleSrvTxtQueryData *purple_txt_resolve(PurpleAccount *account, const char *owner, const char *domain, PurpleTxtCallback cb, gpointer extradata);

/**
 * Get the value of the current TXT record.
 *
 * @param response  The TXT response record
 *
 * @return The value of the current TXT record.
 */
const gchar *purple_txt_response_get_content(PurpleTxtResponse *response);

/**
 * Destroy a TXT DNS response object.
 *
 * @param response The PurpleTxtResponse to destroy.
 */
void purple_txt_response_destroy(PurpleTxtResponse *response);

/**
 * Cancel a SRV/TXT query and destroy the associated data structure.
 *
 * @param query_data The SRV/TXT query to cancel.  This data structure
 *        is freed by this function.
 */
void purple_srv_txt_query_destroy(PurpleSrvTxtQueryData *query_data);

/**
 * Sets the UI operations structure to be used when doing a SRV/TXT
 * resolve.  The UI operations need only be set if the UI wants to
 * handle the resolve itself; otherwise, leave it as NULL.
 *
 * @param ops The UI operations structure.
 */
void purple_srv_txt_query_set_ui_ops(PurpleSrvTxtQueryUiOps *ops);

/**
 * Returns the UI operations structure to be used when doing a SRV/TXT
 * resolve.
 *
 * @return The UI operations structure.
 */
PurpleSrvTxtQueryUiOps *purple_srv_txt_query_get_ui_ops(void);

/**
 * Get the query from a PurpleSrvTxtQueryData
 *
 * @param query_data The SRV/TXT query
 * @return The query.
 */
char *purple_srv_txt_query_get_query(PurpleSrvTxtQueryData *query_data);

/**
 * Get the type from a PurpleSrvTxtQueryData (TXT or SRV)
 *
 * @param query_data The query
 * @return The query.
 */
int purple_srv_txt_query_get_type(PurpleSrvTxtQueryData *query_data);

G_END_DECLS

#endif /* _PURPLE_DNSSRV_H */

