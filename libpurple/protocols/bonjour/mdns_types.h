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

#ifndef _BONJOUR_MDNS_TYPES
#define _BONJOUR_MDNS_TYPES

#include <glib.h>
#include "account.h"
#include "config.h"

#ifdef USE_BONJOUR_APPLE
#include "dns_sd_proxy.h"
#else /* USE_BONJOUR_HOWL */
#include <howl.h>
#endif

#define ICHAT_SERVICE "_presence._tcp."

/**
 * Data to be used by the dns-sd connection.
 */
typedef struct _BonjourDnsSd
{
#ifdef USE_BONJOUR_APPLE
	DNSServiceRef advertisement;
	DNSServiceRef browser;

	int advertisement_handler; /* hack... windows bonjour is broken, so we have to have this */
#else /* USE_BONJOUR_HOWL */
	sw_discovery session;
	sw_discovery_oid session_id;
#endif

	PurpleAccount *account;
	gchar *first;
	gchar *last;
	gint port_p2pj;
	gchar *phsh;
	gchar *status;
	gchar *vc;
	gchar *msg;
} BonjourDnsSd;

typedef enum _PublishType {
	PUBLISH_START,
	PUBLISH_UPDATE
} PublishType;


#endif
