/*
 * gaim - Rendezvous Protocol Plugin
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

#include "connection.h"
#include "network.h"

#include "direct.h"
#include "rendezvous.h"

#if 0
gchar *
gaim_network_convert_ipv4_to_string(void *ip)
{
	gchar *ret;
	unsigned char *ipv4 = (unsigned char *)ip;

	ret = g_strdup_printf("::ffff:%02hhx%02hhx:%02hhx%02hhx", ipv4[0], ipv4[1], ipv4[2], ipv4[3]);

	return ret;
}

gchar *
gaim_network_convert_ipv6_to_string(void *ip)
{
	gchar *ret;

	//ret = g_strdup_printf("%02hhx%02hhx:%02hhx%02hhx:%02hhx%02hhx:%02hhx%02hhx:%02hhx%02hhx:%02hhx%02hhx:%02hhx%02hhx:%02hhx%02hhx", ip[0], ip[1], ip[2], ip[3], ip[4], ip[5], ip[6], ip[7], ip[8], ip[9], ip[10], ip[11], ip[12], ip[13], ip[14], ip[15]);
	ret = g_malloc0(INET6_ADDRSTRLEN + 1);
	inet_ntop(AF_INET6, ip, ret, sizeof(ret));

	return ret;
}

static gboolean
rendezvous_find_buddy_by_ip(gpointer key, gpointer value, gpointer user_data)
{
	RendezvousBuddy *rb = value;

	if (rb->ipv4 == NULL)
		return FALSE;

printf("looking at ip=%hu.%hu.%hu.%hu\n", rb->ipv4[0], rb->ipv4[1], rb->ipv4[2], rb->ipv4[3]);
	return !memcmp(rb->ipv4, user_data, 4);
}
#endif

void
rendezvous_direct_acceptconnection(gpointer data, gint source, GaimInputCondition condition)
{
	GaimConnection *gc = (GaimConnection *)data;
	RendezvousData *rd = gc->proto_data;
	int fd;
	struct sockaddr_in6 addr;
	socklen_t addrlen = sizeof(addr);
#if 0
	gchar *ip;
#endif
	RendezvousBuddy *rb;

	fd = accept(rd->listener, (struct sockaddr *)&addr, &addrlen);
	if (fd == -1) {
		gaim_debug_warning("rendezvous", "accept: %s\n", strerror(errno));
		return;
	}

#if 0
	printf("\nsa_family=%d\n\n", ((struct sockaddr *)&addr)->sa_family);
	if (((struct sockaddr *)&addr)->sa_family == AF_INET)
		ip = gaim_network_convert_ipv4_to_string((unsigned char *)&ip);
	else if (((struct sockaddr *)&addr)->sa_family == AF_INET6)
		ip = gaim_network_convert_ipv6_to_string((unsigned char *)&(addr.sin6_addr));
	printf("\nip=%s\n", ip);

	rb = g_hash_table_find(rd->buddies, rendezvous_find_buddy_by_ip, ip);
	g_free(ip);
#endif

	if (rb == NULL) {
		/* We don't want to talk to people that don't advertise themselves */
printf("\ndid not find rb\n\n");
		close(fd);
		return;
	}
printf("\nip belongs to=%s\n\n", rb->aim);

	rb->fd = fd;
}
