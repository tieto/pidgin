/**
 * @file dnssrv.h
 *
 * gaim
 *
 * Copyright (C) 2005, Thomas Butter <butter@uni-mannheim.de>
 *i
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

#ifndef _GAIM_DNSSRV_H
#define _GAIM_DNSSRV_H

typedef struct _GaimSrvResponse GaimSrvResponse;
typedef struct _GaimSrvQueryData GaimSrvQueryData;

struct _GaimSrvResponse {
	char hostname[256];
	int port;
	int weight;
	int pref;
};

typedef void (*GaimSrvCallback)(GaimSrvResponse *resp, int results, gpointer data);

/**
 * Queries an SRV record.
 *
 * @param protocol Name of the protocol (e.g. "sip")
 * @param transport Name of the transport ("tcp" or "udp")
 * @param domain Domain name to query (e.g. "blubb.com")
 * @param cb A callback which will be called with the results
 * @param extradata Extra data to be passed to the callback
 */
GaimSrvQueryData *gaim_srv_resolve(const char *protocol, const char *transport, const char *domain, GaimSrvCallback cb, gpointer extradata);

/**
 * Cancel an SRV DNS query.
 *
 * @param query_data The request to cancel.
 */
void gaim_srv_cancel(GaimSrvQueryData *query_data);

#endif /* _GAIM_DNSSRV_H */
