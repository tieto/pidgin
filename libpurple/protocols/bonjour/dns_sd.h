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

#ifndef _BONJOUR_DNS_SD
#define _BONJOUR_DNS_SD

#include <howl.h>
#include <glib.h>
#include "account.h"

#define ICHAT_SERVICE "_presence._tcp."

/**
 * Data to be used by the dns-sd connection.
 */
typedef struct _BonjourDnsSd
{
	sw_discovery session;
	sw_discovery_oid session_id;
	PurpleAccount *account;
	gchar *name;
	gchar *txtvers;
	gchar *version;
	gchar *first;
	gchar *last;
	gint port_p2pj;
	gchar *phsh;
	gchar *status;
	gchar *email;
	gchar *vc;
	gchar *jid;
	gchar *AIM;
	gchar *msg;
	GHashTable *buddies;
} BonjourDnsSd;

typedef enum _PublishType {
	PUBLISH_START,
	PUBLISH_UPDATE
} PublishType;

/**
 * Allocate space for the dns-sd data.
 */
BonjourDnsSd *bonjour_dns_sd_new(void);

/**
 * Deallocate the space of the dns-sd data.
 */
void bonjour_dns_sd_free(BonjourDnsSd *data);

/**
 * Send a new dns-sd packet updating our status.
 */
void bonjour_dns_sd_send_status(BonjourDnsSd *data, const char *status, const char *status_message);

/**
 * Advertise our presence within the dns-sd daemon and start
 * browsing for other bonjour peers.
 */
gboolean bonjour_dns_sd_start(BonjourDnsSd *data);

/**
 * Unregister the "_presence._tcp" service at the mDNS daemon.
 */
void bonjour_dns_sd_stop(BonjourDnsSd *data);

#endif
