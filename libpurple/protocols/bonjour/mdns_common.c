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
#include "cipher.h"
#include "debug.h"

#include "mdns_common.h"
#include "mdns_interface.h"
#include "bonjour.h"
#include "buddy.h"


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
bonjour_dns_sd_send_status(BonjourDnsSd *data, const char *status, const char *status_message) {
	g_free(data->status);
	g_free(data->msg);

	data->status = g_strdup(status);
	data->msg = g_strdup(status_message);

	/* Update our text record with the new status */
	_mdns_publish(data, PUBLISH_UPDATE); /* <--We must control the errors */
}

void
bonjour_dns_sd_buddy_icon_data_set(BonjourDnsSd *data) {
	PurpleStoredImage *img = purple_buddy_icons_find_account_icon(data->account);
	gconstpointer avatar_data;
	gsize avatar_len;
	gchar *enc;
	int i;
	unsigned char hashval[20];
	char *p, hash[41];

	g_return_if_fail(img != NULL);

	avatar_data = purple_imgstore_get_data(img);
	avatar_len = purple_imgstore_get_size(img);

	enc = purple_base64_encode(avatar_data, avatar_len);

	purple_cipher_digest_region("sha1", avatar_data,
				    avatar_len, sizeof(hashval),
				    hashval, NULL);

	purple_imgstore_unref(img);

	p = hash;
	for(i=0; i<20; i++, p+=2)
		snprintf(p, 3, "%02x", hashval[i]);

	g_free(data->phsh);
	data->phsh = g_strdup(hash);

	g_free(enc);

	/* Update our TXT record */
	_mdns_publish(data, PUBLISH_UPDATE);
}

void
bonjour_dns_sd_update_buddy_icon(BonjourDnsSd *data) {
	PurpleStoredImage *img;

	if ((img = purple_buddy_icons_find_account_icon(data->account))) {
		gconstpointer avatar_data;
		gsize avatar_len;

		avatar_data = purple_imgstore_get_data(img);
		avatar_len = purple_imgstore_get_size(img);

		_mdns_set_buddy_icon_data(data, avatar_data, avatar_len);

		purple_imgstore_unref(img);
	} else {
		/* We need to do this regardless of whether data->phsh is set so that we
		 * cancel any icons that are currently in the process of being set */
		_mdns_set_buddy_icon_data(data, NULL, 0);
		if (data->phsh != NULL) {
			/* Clear the buddy icon */
			g_free(data->phsh);
			data->phsh = NULL;
			/* Update our TXT record */
			_mdns_publish(data, PUBLISH_UPDATE);
		}
	}
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

	return TRUE;
}

/**
 * Unregister the "_presence._tcp" service at the mDNS daemon.
 */

void
bonjour_dns_sd_stop(BonjourDnsSd *data) {
	_mdns_stop(data);
}
