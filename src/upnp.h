/**
 * @file upnp.h Universal Plug N Play API
 * @ingroup core
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _GAIM_UPNP_H_
#define _GAIM_UPNP_H_


typedef struct
{
  gchar* controlURL;
  gchar* serviceType;
} GaimUPnPControlInfo;


#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************/
/** @name UPnP API                                                        */
/**************************************************************************/
/*@{*/

/**
 * Sends a discovery request to search for a UPnP enabled IGD that
 * contains the WANIPConnection service that will allow us to recieve the
 * public IP address of the IGD, and control it for forwarding ports.
 *
 * @param void
 *
 * @return The control URL for the IGD we'll use to use the IGD services
 */
GaimUPnPControlInfo* gaim_upnp_discover(void);



/**
 * Gets the IP address from a UPnP enabled IGD that sits on the local
 * network, so when getting the network IP, instead of returning the
 * local network IP, the public IP is retrieved.
 *
 * @param controlURL The control URL retrieved from gaim_upnp_discover.
 *
 * @return The IP address of the network, or NULL if something went wrong
 */
gchar* gaim_upnp_get_public_ip(const GaimUPnPControlInfo* controlInfo);


/**
 * Maps Ports in a UPnP enabled IGD that sits on the local network to
 * this gaim client. Essentially, this function takes care of the port
 * forwarding so things like file transfers can work behind NAT firewalls
 *
 * @param controlURL The control URL retrieved from gaim_upnp_discover.
 * @param portMap The port to map to this client
 * @param protocol The protocol to map, either "TCP" or "UDP"
 *
 * @return TRUE if success, FALSE if something went wrong.
 */
gboolean gaim_upnp_set_port_mapping(const GaimUPnPControlInfo* controlInfo, 
                                    unsigned short portMap,
                                    const gchar* protocol);

/**
 * Deletes a port mapping in a UPnP enabled IGD that sits on the local network
 * to this gaim client. Essentially, this function takes care of deleting the 
 * port forwarding after they have completed a connection so another client on
 * the local network can take advantage of the port forwarding
 *
 * @param controlURL The control URL retrieved from gaim_upnp_discover.
 * @param portMap The port to delete the mapping for
 * @param protocol The protocol to map to. Either "TCP" or "UDP"
 *
 * @return TRUE if success, FALSE if something went wrong.
 */
gboolean 
gaim_upnp_remove_port_mapping(const GaimUPnPControlInfo* controlURL, 
                              unsigned short portMap,
                              const gchar* protocol);
/*@}*/


#ifdef __cplusplus
}
#endif

#endif /* _GAIM_UPNP_H_ */
