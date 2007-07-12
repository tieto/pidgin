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

#ifdef USE_BONJOUR_APPLE

#include <glib.h>
#include "mdns_types.h"
#include "buddy.h"
#include "dnsquery.h"
#include "dns_sd_proxy.h"

/* Bonjour async callbacks */

void DNSSD_API _mdns_service_browse_callback(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
    DNSServiceErrorType errorCode, const char *serviceName, const char *regtype, const char *replyDomain, void *context);

/* interface functions */

int _mdns_publish(BonjourDnsSd *data, PublishType type);
void _mdns_handle_event(gpointer data, gint source, PurpleInputCondition condition);

#endif

#endif
