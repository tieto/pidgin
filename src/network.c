/**
 * @file network.c Network Implementation
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

#include "internal.h"

#ifndef _WIN32
#include <net/if.h>
#include <sys/ioctl.h>
#endif

/* Solaris */
#if defined (__SVR4) && defined (__sun)
#include <sys/sockio.h>
#endif

#include "debug.h"
#include "account.h"
#include "network.h"
#include "prefs.h"
#include "stun.h"
#include "upnp.h"

typedef struct {
	int listenfd;
	int socket_type;
	gboolean retry;
	gboolean adding;
	GaimNetworkListenCallback cb;
	gpointer cb_data;
} ListenUPnPData;

const unsigned char *
gaim_network_ip_atoi(const char *ip)
{
	static unsigned char ret[4];
	gchar *delimiter = ".";
	gchar **split;
	int i;

	g_return_val_if_fail(ip != NULL, NULL);

	split = g_strsplit(ip, delimiter, 4);
	for (i = 0; split[i] != NULL; i++)
		ret[i] = atoi(split[i]);
	g_strfreev(split);

	/* i should always be 4 */
	if (i != 4)
		return NULL;

	return ret;
}

void
gaim_network_set_public_ip(const char *ip)
{
	g_return_if_fail(ip != NULL);

	/* XXX - Ensure the IP address is valid */

	gaim_prefs_set_string("/core/network/public_ip", ip);
}

const char *
gaim_network_get_public_ip(void)
{
	return gaim_prefs_get_string("/core/network/public_ip");
}

const char *
gaim_network_get_local_system_ip(int fd)
{
	char buffer[1024];
	static char ip[16];
	char *tmp;
	struct ifconf ifc;
	struct ifreq *ifr;
	struct sockaddr_in *sinptr;
	guint32 lhost = htonl(127 * 256 * 256 * 256 + 1);
	long unsigned int add;
	int source = fd;

	if (fd < 0)
		source = socket(PF_INET,SOCK_STREAM, 0);

	ifc.ifc_len = sizeof(buffer);
	ifc.ifc_req = (struct ifreq *)buffer;
	ioctl(source, SIOCGIFCONF, &ifc);

	if (fd < 0)
		close(source);

	tmp = buffer;
	while (tmp < buffer + ifc.ifc_len)
	{
		ifr = (struct ifreq *)tmp;
		tmp += sizeof(struct ifreq);

		if (ifr->ifr_addr.sa_family == AF_INET)
		{
			sinptr = (struct sockaddr_in *)&ifr->ifr_addr;
			if (sinptr->sin_addr.s_addr != lhost)
			{
				add = ntohl(sinptr->sin_addr.s_addr);
				g_snprintf(ip, 16, "%lu.%lu.%lu.%lu",
					((add >> 24) & 255),
					((add >> 16) & 255),
					((add >> 8) & 255),
					add & 255);

				return ip;
			}
		}
	}

	return "0.0.0.0";
}

const char *
gaim_network_get_my_ip(int fd)
{
	const char *ip = NULL;
	GaimStunNatDiscovery *stun;

	/* Check if the user specified an IP manually */
	if (!gaim_prefs_get_bool("/core/network/auto_ip")) {
		ip = gaim_network_get_public_ip();
		if ((ip != NULL) && (*ip != '\0'))
			return ip;
	}

	/* Check if STUN discovery was already done */
	stun = gaim_stun_discover(NULL);
	if ((stun != NULL) && (stun->status == GAIM_STUN_STATUS_DISCOVERED))
		return stun->publicip;

	/* Attempt to get the IP from a NAT device using UPnP */
	ip = gaim_upnp_get_public_ip();
	if (ip != NULL)
	  return ip;

	/* Just fetch the IP of the local system */
	return gaim_network_get_local_system_ip(fd);
}


static void
gaim_network_set_upnp_port_mapping_cb(gboolean success, gpointer data)
{
	ListenUPnPData *ldata = data;

	if (!success) {
		gaim_debug_info("network", "Couldn't create UPnP mapping\n");
		if (ldata->retry) {
			ldata->retry = FALSE;
			ldata->adding = FALSE;
			gaim_upnp_remove_port_mapping(
				gaim_network_get_port_from_fd(ldata->listenfd),
				(ldata->socket_type == SOCK_STREAM) ? "TCP" : "UDP",
				gaim_network_set_upnp_port_mapping_cb, ldata);
			return;
		}
	} else if (!ldata->adding) {
		/* We've tried successfully to remove the port mapping.
		 * Try to add it again */
		ldata->adding = TRUE;
		gaim_upnp_set_port_mapping(
			gaim_network_get_port_from_fd(ldata->listenfd),
			(ldata->socket_type == SOCK_STREAM) ? "TCP" : "UDP",
			gaim_network_set_upnp_port_mapping_cb, ldata);
		return;
	}

	if (ldata->cb)
		ldata->cb(ldata->listenfd, ldata->cb_data);

	g_free(ldata);
}


static gboolean
gaim_network_do_listen(unsigned short port, int socket_type, GaimNetworkListenCallback cb, gpointer cb_data)
{
	int listenfd = -1;
	const int on = 1;
	ListenUPnPData *ld;
#ifdef HAVE_GETADDRINFO
	int errnum;
	struct addrinfo hints, *res, *next;
	char serv[6];

	/*
	 * Get a list of addresses on this machine.
	 */
	snprintf(serv, sizeof(serv), "%hu", port);
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = socket_type;
	errnum = getaddrinfo(NULL /* any IP */, serv, &hints, &res);
	if (errnum != 0) {
#ifndef _WIN32
		gaim_debug_warning("network", "getaddrinfo: %s\n", gai_strerror(errnum));
		if (errnum == EAI_SYSTEM)
			gaim_debug_warning("network", "getaddrinfo: system error: %s\n", strerror(errno));
#else
		gaim_debug_warning("network", "getaddrinfo: Error Code = %d\n", errnum);
#endif
		return FALSE;
	}

	/*
	 * Go through the list of addresses and attempt to listen on
	 * one of them.
	 * XXX - Try IPv6 addresses first?
	 */
	for (next = res; next != NULL; next = next->ai_next) {
		listenfd = socket(next->ai_family, next->ai_socktype, next->ai_protocol);
		if (listenfd < 0)
			continue;
		if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) != 0)
			gaim_debug_warning("network", "setsockopt: %s\n", strerror(errno));
		if (bind(listenfd, next->ai_addr, next->ai_addrlen) == 0)
			break; /* success */
		/* XXX - It is unclear to me (datallah) whether we need to be
		   using a new socket each time */
		close(listenfd);
	}

	freeaddrinfo(res);

	if (next == NULL)
		return FALSE;
#else
	struct sockaddr_in sockin;

	if ((listenfd = socket(AF_INET, socket_type, 0)) < 0) {
		gaim_debug_warning("network", "socket: %s\n", strerror(errno));
		return FALSE;
	}

	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) != 0)
		gaim_debug_warning("network", "setsockopt: %s\n", strerror(errno));

	memset(&sockin, 0, sizeof(struct sockaddr_in));
	sockin.sin_family = PF_INET;
	sockin.sin_port = htons(port);

	if (bind(listenfd, (struct sockaddr *)&sockin, sizeof(struct sockaddr_in)) != 0) {
		gaim_debug_warning("network", "bind: %s\n", strerror(errno));
		close(listenfd);
		return FALSE;
	}
#endif

	if (socket_type == SOCK_STREAM && listen(listenfd, 4) != 0) {
		gaim_debug_warning("network", "listen: %s\n", strerror(errno));
		close(listenfd);
		return FALSE;
	}
	fcntl(listenfd, F_SETFL, O_NONBLOCK);

	gaim_debug_info("network", "Listening on port: %hu\n", gaim_network_get_port_from_fd(listenfd));

	ld = g_new0(ListenUPnPData, 1);
	ld->listenfd = listenfd;
	ld->adding = TRUE;
	ld->retry = TRUE;
	ld->cb = cb;
	ld->cb_data = cb_data;

	gaim_upnp_set_port_mapping(
			gaim_network_get_port_from_fd(listenfd),
			(socket_type == SOCK_STREAM) ? "TCP" : "UDP",
			gaim_network_set_upnp_port_mapping_cb, ld);

	return TRUE;
}

gboolean
gaim_network_listen(unsigned short port, int socket_type,
		GaimNetworkListenCallback cb, gpointer cb_data)
{
	g_return_val_if_fail(port != 0, -1);

	return gaim_network_do_listen(port, socket_type, cb, cb_data);
}

gboolean
gaim_network_listen_range(unsigned short start, unsigned short end,
		int socket_type, GaimNetworkListenCallback cb, gpointer cb_data)
{
	gboolean ret = FALSE;

	if (gaim_prefs_get_bool("/core/network/ports_range_use")) {
		start = gaim_prefs_get_int("/core/network/ports_range_start");
		end = gaim_prefs_get_int("/core/network/ports_range_end");
	} else {
		if (end < start)
			end = start;
	}

	for (; start <= end; start++) {
		ret = gaim_network_do_listen(start, socket_type, cb, cb_data);
		if (ret)
			break;
	}

	return ret;
}

unsigned short
gaim_network_get_port_from_fd(int fd)
{
	struct sockaddr_in addr;
	socklen_t len;

	g_return_val_if_fail(fd >= 0, 0);

	len = sizeof(addr);
	if (getsockname(fd, (struct sockaddr *) &addr, &len) == -1) {
		gaim_debug_warning("network", "getsockname: %s\n", strerror(errno));
		return 0;
	}

	return ntohs(addr.sin_port);
}

void
gaim_network_init(void)
{
	gaim_prefs_add_none  ("/core/network");
	gaim_prefs_add_bool  ("/core/network/auto_ip", TRUE);
	gaim_prefs_add_string("/core/network/public_ip", "");
	gaim_prefs_add_bool  ("/core/network/ports_range_use", FALSE);
	gaim_prefs_add_int   ("/core/network/ports_range_start", 1024);
	gaim_prefs_add_int   ("/core/network/ports_range_end", 2048);

	gaim_upnp_discover(NULL, NULL);
}
