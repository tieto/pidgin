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

static gboolean
rendezvous_find_buddy_by_ipv4(gpointer key, gpointer value, gpointer user_data)
{
	RendezvousBuddy *rb = value;

	if (rb->ipv4 == NULL)
		return FALSE;

int *ipv4 = user_data;
printf("looking for ip=%hu.%hu.%hu.%hu\n", ipv4[0], ipv4[1], ipv4[2], ipv4[3]);
printf("looking at  ip=%hu.%hu.%hu.%hu, %s\n", rb->ipv4[0], rb->ipv4[1], rb->ipv4[2], rb->ipv4[3], rb->firstandlast);
	return !memcmp(rb->ipv4, user_data, 4);
}

static gboolean
rendezvous_find_buddy_by_ipv6(gpointer key, gpointer value, gpointer user_data)
{
	RendezvousBuddy *rb = value;

	if (rb->ipv6 == NULL)
		return FALSE;

int *ipv6 = user_data;
printf("looking for ip=%hx%hx:%hx%hx:%hx%hx:%hx%hx:%hx%hx:%hx%hx:%hx%hx:%hx%hx\n", ipv6[0], ipv6[1], ipv6[2], ipv6[3], ipv6[4], ipv6[5], ipv6[6], ipv6[7], ipv6[8], ipv6[9], ipv6[10], ipv6[11], ipv6[12], ipv6[13], ipv6[14], ipv6[15]);
printf("looking at  ip=%hx%hx:%hx%hx:%hx%hx:%hx%hx:%hx%hx:%hx%hx:%hx%hx:%hx%hx, %s\n", rb->ipv6[0], rb->ipv6[1], rb->ipv6[2], rb->ipv6[3], rb->ipv6[4], rb->ipv6[5], rb->ipv6[6], rb->ipv6[7], rb->ipv6[8], rb->ipv6[9], rb->ipv6[10], rb->ipv6[11], rb->ipv6[12], rb->ipv6[13], rb->ipv6[14], rb->ipv6[15], rb->firstandlast);
	return !memcmp(rb->ipv6, user_data, 16);
}

void
rendezvous_direct_acceptconnection(gpointer data, gint source, GaimInputCondition condition)
{
	GaimConnection *gc = (GaimConnection *)data;
	RendezvousData *rd = gc->proto_data;
	int fd;
	struct sockaddr_in6 addr;
	socklen_t addrlen = sizeof(addr);
	RendezvousBuddy *rb = NULL;

	fd = accept(rd->listener, (struct sockaddr *)&addr, &addrlen);
	if (fd == -1) {
		gaim_debug_warning("rendezvous", "accept: %s\n", strerror(errno));
		return;
	}

	if (((struct sockaddr *)&addr)->sa_family == AF_INET)
		rb = g_hash_table_find(rd->buddies, rendezvous_find_buddy_by_ipv4, &(((struct sockaddr_in *)&addr)->sin_addr));
	else if (((struct sockaddr *)&addr)->sa_family == AF_INET6)
		rb = g_hash_table_find(rd->buddies, rendezvous_find_buddy_by_ipv6, &(addr.sin6_addr.s6_addr));

	if (rb == NULL) {
		/* We don't want to talk to people that don't advertise themselves */
printf("\ndid not find rb\n\n");
		close(fd);
		return;
	}
printf("\nip belongs to=%s\n\n", rb->aim);

	rb->fd = fd;

	/* TODO: Add a watcher on the connection. */
}

static void
rendezvous_direct_connect(RendezvousBuddy *rb)
{
	struct sockaddr_in addr;

	/* If we already have a connection then do nothing */
	if (rb->fd != -1)
		return;

	if ((rb->ipv4 == NULL) && (rb->ipv6 == NULL))
	{
		gaim_debug_warning("rendezvous", "Could not connect: Unknown IP address.\n");
		/* TODO: Show an error message to the user. */
		return;
	}

	if ((rb->fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		gaim_debug_warning("rendezvous", "Could not connect: %s.\n", strerror(errno));
		/* TODO: Show an error message to the user. */
		return;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = rb->p2pjport;
	memcpy(&addr.sin_addr, rb->ipv4, 4);
	memset(&addr.sin_zero, 0, 8);

	if (connect(rb->fd, (struct sockaddr *)&addr, sizeof(struct sockaddr)) == -1)
	{
		gaim_debug_warning("rendezvous", "Could not connect: %s.\n", strerror(errno));
		/* TODO: Show an error message to the user. */
		return;
	}

	/* TODO: Connect a watcher */
}

static void
rendezvous_direct_write_message_to_socket(int fd, const char *message)
{

}

/*
 * TODO: Establish a direct connection, then send IM.  Will need to
 * queue the message somewhere, while the connection is established.
 */
void
rendezvous_direct_send_message(GaimConnection *gc, const char *who, const char *message)
{
	RendezvousData *rd = gc->proto_data;
	RendezvousBuddy *rb;

	rb = g_hash_table_lookup(rd->buddies, who);
	if (rb == NULL)
	{
		/* TODO: Should print an error to the user, here */
		gaim_debug_error("rendezvous", "Could not send message to %s: Could not find user information.\n", who);
		return;
	}

	if (rb->fd == -1)
	{
		rendezvous_direct_connect(rb);
		/* TODO: Queue message */
		//gaim_debug_warning("rendezvous", "Could not send message to %s: Unable to establish connection.\n", who);
	}
	else
	{
		rendezvous_direct_write_message_to_socket(rb->fd, message);
	}
}
