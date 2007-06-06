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

#include "mdns_howl.h"

#include "debug.h"
#include "buddy.h"

sw_result HOWL_API
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

sw_result HOWL_API
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
	bonjour_buddy_add_to_purple(buddy);

	return SW_OKAY;
}

sw_result HOWL_API
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
			gb = purple_find_buddy((PurpleAccount*)extra, name);
			if (gb != NULL)
			{
				bonjour_buddy_delete(gb->proto_data);
				purple_blist_remove_buddy(gb);
			}
			break;
		case SW_DISCOVERY_BROWSE_RESOLVED:
			purple_debug_info("bonjour", "_browse_reply --> Resolved\n");
			break;
		default:
			break;
	}

	return SW_OKAY;
}

int
_mdns_publish(BonjourDnsSd *data, PublishType type)
{
	sw_text_record dns_data;
	sw_result publish_result = SW_OKAY;
	char portstring[6];
	const char *jid, *aim, *email;

	/* Fill the data for the service */
	if (sw_text_record_init(&dns_data) != SW_OKAY)
	{
		purple_debug_error("bonjour", "Unable to initialize the data for the mDNS.\n");
		return -1;
	}

	/* Convert the port to a string */
	snprintf(portstring, sizeof(portstring), "%d", data->port_p2pj);

	jid = purple_account_get_string(data->account, "jid", NULL);
	aim = purple_account_get_string(data->account, "AIM", NULL);
	email = purple_account_get_string(data->account, "email", NULL);

	/* We should try to follow XEP-0174, but some clients have "issues", so we humor them.
	 * See http://telepathy.freedesktop.org/wiki/SalutInteroperability
	 */

	/* Needed by iChat */
	sw_text_record_add_key_and_string_value(dns_data, "txtvers", "1");
	/* Needed by Gaim/Pidgin <= 2.0.1 (remove at some point) */
	sw_text_record_add_key_and_string_value(dns_data, "1st", data->first);
	/* Needed by Gaim/Pidgin <= 2.0.1 (remove at some point) */
	sw_text_record_add_key_and_string_value(dns_data, "last", data->last);
	/* Needed by Adium */
	sw_text_record_add_key_and_string_value(dns_data, "port.p2pj", portstring);
	/* Needed by iChat, Gaim/Pidgin <= 2.0.1 */
	sw_text_record_add_key_and_string_value(dns_data, "status", data->status);
	/* Currently always set to "!" since we don't support AV and wont ever be in a conference */
	sw_text_record_add_key_and_string_value(dns_data, "vc", data->vc);
	sw_text_record_add_key_and_string_value(dns_data, "ver", VERSION);
	if (email != NULL && *email != '\0')
		sw_text_record_add_key_and_string_value(dns_data, "email", email);
	if (jid != NULL && *jid != '\0')
		sw_text_record_add_key_and_string_value(dns_data, "jid", jid);
	/* Nonstandard, but used by iChat */
	if (aim != NULL && *aim != '\0')
		sw_text_record_add_key_and_string_value(dns_data, "AIM", aim);
	if (data->msg != NULL && *data->msg != '\0')
		sw_text_record_add_key_and_string_value(dns_data, "msg", data->msg);
	if (data->phsh != NULL && *data->phsh != '\0')
		sw_text_record_add_key_and_string_value(dns_data, "phsh", data->phsh);

	/* TODO: ext, nick, node */

	/* Publish the service */
	switch (type)
	{
		case PUBLISH_START:
			publish_result = sw_discovery_publish(data->session, 0, purple_account_get_username(data->account), ICHAT_SERVICE, NULL,
								NULL, data->port_p2pj, sw_text_record_bytes(dns_data), sw_text_record_len(dns_data),
								_publish_reply, NULL, &data->session_id);
			break;
		case PUBLISH_UPDATE:
			publish_result = sw_discovery_publish_update(data->session, data->session_id,
								sw_text_record_bytes(dns_data), sw_text_record_len(dns_data));
			break;
	}
	if (publish_result != SW_OKAY)
	{
		purple_debug_error("bonjour", "Unable to publish or change the status of the _presence._tcp service.\n");
		return -1;
	}

	/* Free the memory used by temp data */
	sw_text_record_fina(dns_data);

	return 0;
}

void
_mdns_handle_event(gpointer data, gint source, PurpleInputCondition condition)
{
	sw_discovery_read_socket((sw_discovery)data);
}
