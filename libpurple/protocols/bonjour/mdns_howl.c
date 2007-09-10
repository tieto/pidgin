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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301, USA.
 */

#include "internal.h"

#include "mdns_interface.h"
#include "debug.h"
#include "buddy.h"

#include <howl.h>

/* data used by howl bonjour implementation */
typedef struct _howl_impl_data {
	sw_discovery session;
	sw_discovery_oid session_id;
	guint session_handler;
} HowlSessionImplData;

static sw_result HOWL_API
_publish_reply(sw_discovery discovery, sw_discovery_oid oid,
			   sw_discovery_publish_status status, sw_opaque extra)
{
	purple_debug_warning("bonjour", "_publish_reply --> Start\n");

	/* Check the answer from the mDNS daemon */
	switch (status)
	{
		case SW_DISCOVERY_PUBLISH_STARTED :
			purple_debug_info("bonjour", "_publish_reply --> Service started\n");
			break;
		case SW_DISCOVERY_PUBLISH_STOPPED :
			purple_debug_info("bonjour", "_publish_reply --> Service stopped\n");
			break;
		case SW_DISCOVERY_PUBLISH_NAME_COLLISION :
			purple_debug_info("bonjour", "_publish_reply --> Name collision\n");
			break;
		case SW_DISCOVERY_PUBLISH_INVALID :
			purple_debug_info("bonjour", "_publish_reply --> Service invalid\n");
			break;
	}

	return SW_OKAY;
}

static sw_result HOWL_API
_resolve_reply(sw_discovery discovery, sw_discovery_oid oid,
			   sw_uint32 interface_index, sw_const_string name,
			   sw_const_string type, sw_const_string domain,
			   sw_ipv4_address address, sw_port port,
			   sw_octets text_record, sw_ulong text_record_len,
			   sw_opaque extra)
{
	BonjourBuddy *buddy;
	PurpleAccount *account = (PurpleAccount*)extra;
	gint address_length = 16;
	sw_text_record_iterator iterator;
	char key[SW_TEXT_RECORD_MAX_LEN];
	char value[SW_TEXT_RECORD_MAX_LEN];
	sw_uint32 value_length;

	/* TODO: We want to keep listening for updates*/
	sw_discovery_cancel(discovery, oid);

	/* create a buddy record */
	buddy = bonjour_buddy_new(name, account);

	/* Get the ip as a string */
	buddy->ip = g_malloc(address_length);
	sw_ipv4_address_name(address, buddy->ip, address_length);

	buddy->port_p2pj = port;

	/* Obtain the parameters from the text_record */
	if ((text_record_len > 0) && (text_record) && (*text_record != '\0'))
	{
		clear_bonjour_buddy_values(buddy);
		sw_text_record_iterator_init(&iterator, text_record, text_record_len);
		while (sw_text_record_iterator_next(iterator, key, (sw_octet *)value, &value_length) == SW_OKAY)
			set_bonjour_buddy_value(buddy, key, value, value_length);

		sw_text_record_iterator_fina(iterator);
	}

	if (!bonjour_buddy_check(buddy))
	{
		bonjour_buddy_delete(buddy);
		return SW_DISCOVERY_E_UNKNOWN;
	}

	/* Add or update the buddy in our buddy list */
	bonjour_buddy_add_to_purple(buddy, NULL);

	return SW_OKAY;
}

static sw_result HOWL_API
_browser_reply(sw_discovery discovery, sw_discovery_oid oid,
			   sw_discovery_browse_status status,
			   sw_uint32 interface_index, sw_const_string name,
			   sw_const_string type, sw_const_string domain,
			   sw_opaque_t extra)
{
	sw_discovery_resolve_id rid;
	PurpleAccount *account = (PurpleAccount*)extra;
	PurpleBuddy *gb = NULL;

	switch (status)
	{
		case SW_DISCOVERY_BROWSE_INVALID:
			purple_debug_warning("bonjour", "_browser_reply --> Invalid\n");
			break;
		case SW_DISCOVERY_BROWSE_RELEASE:
			purple_debug_warning("bonjour", "_browser_reply --> Release\n");
			break;
		case SW_DISCOVERY_BROWSE_ADD_DOMAIN:
			purple_debug_warning("bonjour", "_browser_reply --> Add domain\n");
			break;
		case SW_DISCOVERY_BROWSE_ADD_DEFAULT_DOMAIN:
			purple_debug_warning("bonjour", "_browser_reply --> Add default domain\n");
			break;
		case SW_DISCOVERY_BROWSE_REMOVE_DOMAIN:
			purple_debug_warning("bonjour", "_browser_reply --> Remove domain\n");
			break;
		case SW_DISCOVERY_BROWSE_ADD_SERVICE:
			/* A new peer has joined the network and uses iChat bonjour */
			purple_debug_info("bonjour", "_browser_reply --> Add service\n");
			if (g_ascii_strcasecmp(name, account->username) != 0)
			{
				if (sw_discovery_resolve(discovery, interface_index, name, type,
						domain, _resolve_reply, extra, &rid) != SW_OKAY)
				{
					purple_debug_warning("bonjour", "_browser_reply --> Cannot send resolve\n");
				}
			}
			break;
		case SW_DISCOVERY_BROWSE_REMOVE_SERVICE:
			purple_debug_info("bonjour", "_browser_reply --> Remove service\n");
			gb = purple_find_buddy(account, name);
			if (gb != NULL)
				purple_blist_remove_buddy(gb);
			break;
		case SW_DISCOVERY_BROWSE_RESOLVED:
			purple_debug_info("bonjour", "_browse_reply --> Resolved\n");
			break;
		default:
			break;
	}

	return SW_OKAY;
}

static void
_mdns_handle_event(gpointer data, gint source, PurpleInputCondition condition)
{
	sw_discovery_read_socket((sw_discovery)data);
}

/****************************
 * mdns_interface functions *
 ****************************/

gboolean _mdns_init_session(BonjourDnsSd *data) {
	HowlSessionImplData *idata = g_new0(HowlSessionImplData, 1);

	if (sw_discovery_init(&idata->session) != SW_OKAY) {
		purple_debug_error("bonjour", "Unable to initialize an mDNS session.\n");

		/* In Avahi, sw_discovery_init frees data->session but doesn't clear it */
		idata->session = NULL;

		g_free(idata);

		return FALSE;
	}

	data->mdns_impl_data = idata;

	return TRUE;
}


gboolean _mdns_publish(BonjourDnsSd *data, PublishType type, GSList *records) {
	sw_text_record dns_data;
	sw_result publish_result = SW_OKAY;
	HowlSessionImplData *idata = data->mdns_impl_data;

	g_return_val_if_fail(idata != NULL, FALSE);

	/* Fill the data for the service */
	if (sw_text_record_init(&dns_data) != SW_OKAY) {
		purple_debug_error("bonjour", "Unable to initialize the data for the mDNS.\n");
		return FALSE;
	}

	while (records) {
		PurpleKeyValuePair *kvp = records->data;
		sw_text_record_add_key_and_string_value(dns_data, kvp->key, kvp->value);
		records = records->next;
	}

	/* Publish the service */
	switch (type) {
		case PUBLISH_START:
			publish_result = sw_discovery_publish(idata->session, 0, purple_account_get_username(data->account), ICHAT_SERVICE, NULL,
								NULL, data->port_p2pj, sw_text_record_bytes(dns_data), sw_text_record_len(dns_data),
								_publish_reply, NULL, &idata->session_id);
			break;
		case PUBLISH_UPDATE:
			publish_result = sw_discovery_publish_update(idata->session, idata->session_id,
								sw_text_record_bytes(dns_data), sw_text_record_len(dns_data));
			break;
	}

	/* Free the memory used by temp data */
	sw_text_record_fina(dns_data);

	if (publish_result != SW_OKAY) {
		purple_debug_error("bonjour", "Unable to publish or change the status of the " ICHAT_SERVICE " service.\n");
		return FALSE;
	}

	return TRUE;
}

gboolean _mdns_browse(BonjourDnsSd *data) {
	HowlSessionImplData *idata = data->mdns_impl_data;
	/* TODO: don't we need to hang onto this to cancel later? */
	sw_discovery_oid session_id;

	g_return_val_if_fail(idata != NULL, FALSE);

	if (sw_discovery_browse(idata->session, 0, ICHAT_SERVICE, NULL, _browser_reply,
				    data->account, &session_id) == SW_OKAY) {
		idata->session_handler = purple_input_add(sw_discovery_socket(idata->session),
				PURPLE_INPUT_READ, _mdns_handle_event, idata->session);
		return TRUE;
	}

	return FALSE;
}

gboolean _mdns_set_buddy_icon_data(BonjourDnsSd *data, gconstpointer avatar_data, gsize avatar_len) {
	return FALSE;
}

void _mdns_stop(BonjourDnsSd *data) {
	HowlSessionImplData *idata = data->mdns_impl_data;

	if (idata == NULL || idata->session == NULL)
		return;

	sw_discovery_cancel(idata->session, idata->session_id);

	purple_input_remove(idata->session_handler);

	/* TODO: should this really be g_free()'d ??? */
	g_free(idata->session);

	g_free(idata);

	data->mdns_impl_data = NULL;
}

void _mdns_init_buddy(BonjourBuddy *buddy) {
}

void _mdns_delete_buddy(BonjourBuddy *buddy) {
}

void _mdns_retrieve_buddy_icon(BonjourBuddy* buddy) {
}


