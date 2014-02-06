/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
/**
 * SECTION:upnp
 * @section_id: libpurple-upnp
 * @short_description: <filename>upnp.h</filename>
 * @title: Universal Plug N Play API
 */

#ifndef _PURPLE_UPNP_H_
#define _PURPLE_UPNP_H_

typedef struct _UPnPMappingAddRemove UPnPMappingAddRemove;

G_BEGIN_DECLS

/**************************************************************************/
/* UPnP API                                                               */
/**************************************************************************/
/*@{*/

/* typedef struct _PurpleUPnPRequestData PurpleUPnPRequestData; */

typedef void (*PurpleUPnPCallback) (gboolean success, gpointer data);


/**
 * purple_upnp_init:
 *
 * Initialize UPnP
 */
void purple_upnp_init(void);


/**
 * purple_upnp_discover:
 * @cb: an optional callback function to be notified when the UPnP
 *           discovery is complete
 * @cb_data: Extra data to be passed to the callback
 *
 * Sends a discovery request to search for a UPnP enabled IGD that
 * contains the WANIPConnection service that will allow us to recieve the
 * public IP address of the IGD, and control it for forwarding ports.
 * The result will be cached for further use.
 */
void purple_upnp_discover(PurpleUPnPCallback cb, gpointer cb_data);

#if 0
/**
 * purple_upnp_get_control_info:
 *
 * Retrieve the current UPnP control info, if there is any available.
 * This will only be filled in if purple_upnp_discover() had been called,
 * and finished discovering.
 *
 * Returns: The control URL for the IGD we'll use to use the IGD services
 */
const PurpleUPnPControlInfo* purple_upnp_get_control_info(void);
#endif

/**
 * purple_upnp_get_public_ip:
 *
 * Gets the IP address from a UPnP enabled IGD that sits on the local
 * network, so when getting the network IP, instead of returning the
 * local network IP, the public IP is retrieved.  This is a cached value from
 * the time of the UPnP discovery.
 *
 * Returns: The IP address of the network, or NULL if something went wrong
 */
const gchar* purple_upnp_get_public_ip(void);

/**
 * purple_upnp_cancel_port_mapping:
 * @mapping_data: The data returned when you initiated the UPnP mapping request.
 *
 * Cancel a pending port mapping request initiated with either
 * purple_upnp_set_port_mapping() or purple_upnp_remove_port_mapping().
 */
void purple_upnp_cancel_port_mapping(UPnPMappingAddRemove *mapping_data);

/**
 * purple_upnp_set_port_mapping:
 * @portmap: The port to map to this client
 * @protocol: The protocol to map, either "TCP" or "UDP"
 * @cb: an optional callback function to be notified when the mapping
 *           addition is complete
 * @cb_data: Extra data to be passed to the callback
 *
 * Maps Ports in a UPnP enabled IGD that sits on the local network to
 * this purple client. Essentially, this function takes care of the port
 * forwarding so things like file transfers can work behind NAT firewalls
 *
 * Returns: Data which can be passed to purple_upnp_cancel_port_mapping() to
 *          cancel
 */
UPnPMappingAddRemove *purple_upnp_set_port_mapping(unsigned short portmap, const gchar* protocol,
		PurpleUPnPCallback cb, gpointer cb_data);

/**
 * purple_upnp_remove_port_mapping:
 * @portmap: The port to delete the mapping for
 * @protocol: The protocol to map to. Either "TCP" or "UDP"
 * @cb: an optional callback function to be notified when the mapping
 *           removal is complete
 * @cb_data: Extra data to be passed to the callback
 *
 * Deletes a port mapping in a UPnP enabled IGD that sits on the local network
 * to this purple client. Essentially, this function takes care of deleting the
 * port forwarding after they have completed a connection so another client on
 * the local network can take advantage of the port forwarding
 *
 * Returns: Data which can be passed to purple_upnp_cancel_port_mapping() to
 *          cancel
 */
UPnPMappingAddRemove *purple_upnp_remove_port_mapping(unsigned short portmap,
		const gchar* protocol, PurpleUPnPCallback cb, gpointer cb_data);

/*@}*/

G_END_DECLS

#endif /* _PURPLE_UPNP_H_ */
