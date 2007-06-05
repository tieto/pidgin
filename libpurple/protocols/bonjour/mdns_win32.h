/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _BONJOUR_MDNS_WIN32
#define _BONJOUR_MDNS_WIN32

#include "config.h"

#ifdef USE_BONJOUR_APPLE 

#include <glib.h>
#include "mdns_types.h"
#include "buddy.h"
#include "dnsquery.h"
#include "dns_sd_proxy.h"

/* data structure for the resolve callback */
typedef struct _ResolveCallbackArgs
{
	DNSServiceRef resolver;
	int resolver_fd;
	
	PurpleDnsQueryData *query;
	gchar *fqn; 
	
	BonjourBuddy* buddy;
} ResolveCallbackArgs;


/* Bonjour async callbacks */

void _mdns_resolve_host_callback(GSList *hosts, gpointer data, const char *error_message);

void DNSSD_API _mdns_text_record_query_callback(DNSServiceRef DNSServiceRef, DNSServiceFlags flags, uint32_t interfaceIndex,
    DNSServiceErrorType errorCode, const char *fullname, uint16_t rrtype, uint16_t rrclass, uint16_t rdlen,
    const void *rdata, uint32_t ttl, void *context);

void DNSSD_API _mdns_service_resolve_callback(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex, DNSServiceErrorType errorCode, 
    const char *fullname, const char *hosttarget, uint16_t port, uint16_t txtLen, const char *txtRecord, void *context);

void DNSSD_API _mdns_service_register_callback(DNSServiceRef sdRef, DNSServiceFlags flags, DNSServiceErrorType errorCode,
    const char *name, const char *regtype, const char *domain, void *context);

void DNSSD_API _mdns_service_browse_callback(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
    DNSServiceErrorType errorCode, const char *serviceName, const char *regtype, const char *replyDomain, void *context);


/* utility functions */
void _mdns_parse_text_record(BonjourBuddy* buddy, const char* record, uint16_t record_len);

/* interface functions */

int _mdns_publish(BonjourDnsSd *data, PublishType type);
void _mdns_handle_event(gpointer data, gint source, PurpleInputCondition condition);

#endif

#endif
