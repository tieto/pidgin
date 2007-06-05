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

#include "config.h"
#include "mdns_common.h"
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
	g_free(data->email);
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
	PurpleAccount *account;
	PurpleConnection *gc;
	gint dns_sd_socket;
	gpointer opaque_data;

#ifdef USE_BONJOUR_HOWL
	sw_discovery_oid session_id;
#endif

	account = data->account;
	gc = purple_account_get_connection(account);

	/* Initialize the dns-sd data and session */
	
#ifndef USE_BONJOUR_APPLE 
	if (sw_discovery_init(&data->session) != SW_OKAY)
	{
		purple_debug_error("bonjour", "Unable to initialize an mDNS session.\n");

		/* In Avahi, sw_discovery_init frees data->session but doesn't clear it */
		data->session = NULL;

		return FALSE;
	}
#endif

	/* Publish our bonjour IM client at the mDNS daemon */

	if (0 != _mdns_publish(data, PUBLISH_START))
	{
		return FALSE;
	}

	/* Advise the daemon that we are waiting for connections */
	
#ifdef USE_BONJOUR_APPLE
	printf("account pointer here is %x\n,", account);
	if (DNSServiceBrowse(&data->browser, 0, 0, ICHAT_SERVICE, NULL, _mdns_service_browse_callback, account) 
			!= kDNSServiceErr_NoError)
#else /* USE_BONJOUR_HOWL */
	if (sw_discovery_browse(data->session, 0, ICHAT_SERVICE, NULL, _browser_reply,
			account, &session_id) != SW_OKAY)
#endif
	{
		purple_debug_error("bonjour", "Unable to get service.");
		return FALSE;
	}

	/* Get the socket that communicates with the mDNS daemon and bind it to a */
	/* callback that will handle the dns_sd packets */

#ifdef USE_BONJOUR_APPLE 
	dns_sd_socket = DNSServiceRefSockFD(data->browser);
	opaque_data = data->browser;
#else /* USE_BONJOUR_HOWL */
	dns_sd_socket = sw_discovery_socket(data->session);
	opaque_data = data->session;
#endif
	
	gc->inpa = purple_input_add(dns_sd_socket, PURPLE_INPUT_READ,
								_mdns_handle_event,	opaque_data);

	return TRUE;
}

/**
 * Unregister the "_presence._tcp" service at the mDNS daemon.
 */

void
bonjour_dns_sd_stop(BonjourDnsSd *data)
{
	PurpleAccount *account;
	PurpleConnection *gc;

#ifdef USE_BONJOUR_APPLE 
	if (NULL == data->advertisement || NULL == data->browser)
#else /* USE_BONJOUR_HOWL */
	if (data->session == NULL)
#endif
		return;

#ifdef USE_BONJOUR_HOWL 
	sw_discovery_cancel(data->session, data->session_id);
#endif

	account = data->account;
	gc = purple_account_get_connection(account);
	purple_input_remove(gc->inpa);

#ifdef USE_BONJOUR_APPLE 
	/* hack: for win32, we need to stop listening to the advertisement pipe too */
	purple_input_remove(data->advertisement_fd);

	DNSServiceRefDeallocate(data->advertisement);
	DNSServiceRefDeallocate(data->browser);
	data->advertisement = NULL;
	data->browser = NULL;
#else /* USE_BONJOUR_HOWL */
	g_free(data->session);
	data->session = NULL;
#endif
}
