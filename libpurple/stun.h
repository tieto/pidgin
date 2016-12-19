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

#ifndef _PURPLE_STUN_H_
#define _PURPLE_STUN_H_
/**
 * SECTION:stun
 * @section_id: libpurple-stun
 * @short_description: <filename>stun.h</filename>
 * @title: STUN API
 */

/******************************************************************************
 * STUN API
 *****************************************************************************/

typedef struct _PurpleStunNatDiscovery PurpleStunNatDiscovery;

/**
 * PurpleStunStatus:
 * @PURPLE_STUN_STATUS_UNDISCOVERED: No request has been published
 * @PURPLE_STUN_STATUS_UNKNOWN: No STUN server reachable
 * @PURPLE_STUN_STATUS_DISCOVERING: The request has been sent to the server
 * @PURPLE_STUN_STATUS_DISCOVERED: The server has responded
 *
 * The status of a #PurpleStunNatDiscovery
 */
typedef enum {
	PURPLE_STUN_STATUS_UNDISCOVERED = -1,
	PURPLE_STUN_STATUS_UNKNOWN,
	PURPLE_STUN_STATUS_DISCOVERING,
	PURPLE_STUN_STATUS_DISCOVERED
} PurpleStunStatus;

/**
 * PurpleStunNatType:
 * @PURPLE_STUN_NAT_TYPE_PUBLIC_IP: No NAT
 * @PURPLE_STUN_NAT_TYPE_UNKNOWN_NAT: NAT is unknown
 * @PURPLE_STUN_NAT_TYPE_FULL_CONE: NAT is a full cone
 * @PURPLE_STUN_NAT_TYPE_RESTRICTED_CONE: NAT is a restricted cone
 * @PURPLE_STUN_NAT_TYPE_PORT_RESTRICTED_CONE: NAT is a port restricted cone
 * @PURPLE_STUN_NAT_TYPE_SYMMETRIC: NAT is symmetric
 *
 * The type of NAT that was discovered.
 */
typedef enum {
	PURPLE_STUN_NAT_TYPE_PUBLIC_IP,
	PURPLE_STUN_NAT_TYPE_UNKNOWN_NAT,
	PURPLE_STUN_NAT_TYPE_FULL_CONE,
	PURPLE_STUN_NAT_TYPE_RESTRICTED_CONE,
	PURPLE_STUN_NAT_TYPE_PORT_RESTRICTED_CONE,
	PURPLE_STUN_NAT_TYPE_SYMMETRIC
} PurpleStunNatType;

/**
 * PurpleStunNatDiscovery:
 * @status: The #PurpleStunStatus
 * @type: The #PurpleStunNatType
 * @publicip: The public ip
 * @servername: The name of the stun server
 * @lookup_time: The time when the lookup occurred
 *
 * A data type representing a STUN lookup.
 */
struct _PurpleStunNatDiscovery {
	PurpleStunStatus status;
	PurpleStunNatType type;
	char publicip[16];
	char *servername;
	time_t lookup_time;
};

typedef void (*PurpleStunCallback) (PurpleStunNatDiscovery *);

G_BEGIN_DECLS

/**
 * purple_stun_discover:
 * @cb: The callback to call when the STUN discovery is finished if the
 *           discovery would block.  If the discovery is done, this is NOT
 *           called.
 *
 * Starts a NAT discovery. It returns a PurpleStunNatDiscovery if the discovery
 * is already done. Otherwise the callback is called when the discovery is over
 * and NULL is returned.
 *
 * Returns: a #PurpleStunNatDiscovery which includes the public IP and the type
 *          of NAT or NULL if discovery would block
 */
PurpleStunNatDiscovery *purple_stun_discover(PurpleStunCallback cb);

void purple_stun_init(void);

G_END_DECLS

#endif /* _PURPLE_STUN_H_ */
