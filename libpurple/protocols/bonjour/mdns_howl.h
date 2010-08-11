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

#ifndef _BONJOUR_MDNS_HOWL
#define _BONJOUR_MDNS_HOWL

#include "config.h"

#ifdef USE_BONJOUR_HOWL

#include <howl.h>
#include <glib.h>
#include "mdns_types.h"

/* callback functions */

sw_result HOWL_API _publish_reply(sw_discovery discovery, sw_discovery_oid oid, sw_discovery_publish_status status, sw_opaque extra);

sw_result HOWL_API _resolve_reply(sw_discovery discovery, sw_discovery_oid oid, sw_uint32 interface_index, sw_const_string name,
	sw_const_string type, sw_const_string domain, sw_ipv4_address address, sw_port port, sw_octets text_record, 
	sw_ulong text_record_len, sw_opaque extra);

sw_result HOWL_API _browser_reply(sw_discovery discovery, sw_discovery_oid oid, sw_discovery_browse_status status,
	sw_uint32 interface_index, sw_const_string name, sw_const_string type, sw_const_string domain, sw_opaque_t extra);


/* interface functions */

int _mdns_publish(BonjourDnsSd *data, PublishType type);
void _mdns_handle_event(gpointer data, gint source, PurpleInputCondition condition);

#endif

#endif
