/**
 * @file rendezvous.h The Gaim interface to mDNS and peer to peer Jabber.
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
 *
 */

#ifndef _RENDEZVOUS_H_
#define _RENDEZVOUS_H_

#include "internal.h"
#include "debug.h"

#include "mdns.h"

#define RENDEZVOUS_CONNECT_STEPS 2

#define UC_UNAVAILABLE 1
#define UC_IDLE 2

typedef struct _RendezvousData {
	int fd;
	GHashTable *buddies;
	GSList *mytxtdata;
	unsigned short listener_port;
	int listener;
	int listener_watcher;
} RendezvousData;

typedef struct _RendezvousBuddy {
#if 0
	guint ttltimer;
#endif
	gchar *firstandlast;
	gchar *aim;
	gchar *ipv4; /* String representation of an IPv4 address */
	gchar *ipv6; /* String representation of an IPv6 address */
	unsigned short p2pjport;
	int status;
	int idle;		/**< Current idle time in seconds since the epoch.	*/
	gchar *msg;		/**< Current status message of this buddy.			*/
	int fd;			/**< File descriptor of the P2PJ socket.			*/
	int watcher;	/**< Handle for the watcher of the P2PJ socket.		*/
} RendezvousBuddy;

#endif /* _RENDEZVOUS_H_ */
