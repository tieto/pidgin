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

#include <string.h>

#include "internal.h"
#include "mdns_common.h"
#include "mdns_interface.h"
#include "bonjour.h"
#include "buddy.h"
#include "debug.h"


/**
 * Allocate space for the dns-sd data.
 */
BonjourDnsSd *
bonjour_dns_sd_new()
{
	BonjourDnsSd *data = g_new0(BonjourDnsSd, 1);

	return data;
}

/**
 * Deallocate the space of the dns-sd data.
 */
void
bonjour_dns_sd_free(BonjourDnsSd *data)
{
	g_free(data->first);
	g_free(data->last);
	g_free(data->phsh);
	g_free(data->status);
	g_free(data->vc);
	g_free(data->msg);
	g_free(data);
}

/**
 * Send a new dns-sd packet updating our status.
 */
void
bonjour_dns_sd_send_status(BonjourDnsSd *data, const char *status, const char *status_message)
{
	g_free(data->status);
	g_free(data->msg);

	data->status = g_strdup(status);
	data->msg = g_strdup(status_message);

	/* Update our text record with the new status */
	_mdns_publish(data, PUBLISH_UPDATE); /* <--We must control the errors */
}

/**
 * Advertise our presence within the dns-sd daemon and start browsing
 * for other bonjour peers.
 */
gboolean
bonjour_dns_sd_start(BonjourDnsSd *data)
{
	PurpleConnection *gc;

	gc = purple_account_get_connection(data->account);

	/* Initialize the dns-sd data and session */
	if (!_mdns_init_session(data))
		return FALSE;

	/* Publish our bonjour IM client at the mDNS daemon */
	if (!_mdns_publish(data, PUBLISH_START))
		return FALSE;

	/* Advise the daemon that we are waiting for connections */
	if (!_mdns_browse(data)) {
		purple_debug_error("bonjour", "Unable to get service.");
		return FALSE;
	}


	/* Get the socket that communicates with the mDNS daemon and bind it to a */
	/* callback that will handle the dns_sd packets */
	gc->inpa = _mdns_register_to_mainloop(data);

	return TRUE;
}

/**
 * Unregister the "_presence._tcp" service at the mDNS daemon.
 */

void
bonjour_dns_sd_stop(BonjourDnsSd *data)
{
	PurpleConnection *gc;

	_mdns_stop(data);

	gc = purple_account_get_connection(data->account);
	if (gc->inpa > 0)
		purple_input_remove(gc->inpa);
}
