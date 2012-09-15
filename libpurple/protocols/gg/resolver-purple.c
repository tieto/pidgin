/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * Rewritten from scratch during Google Summer of Code 2012
 * by Tomek Wasilczyk (http://www.wasilczyk.pl).
 *
 * Previously implemented by:
 *  - Arkadiusz Miskiewicz <misiek@pld.org.pl> - first implementation (2001);
 *  - Bartosz Oler <bartosz@bzimage.us> - reimplemented during GSoC 2005;
 *  - Krzysztof Klinikowski <grommasher@gmail.com> - some parts (2009-2011).
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

#include <internal.h>
#include <debug.h>
#include <dnsquery.h>

#include <libgadu.h>
#include "resolver-purple.h"

static int ggp_resolver_purple_start(int *fd, void **private_data,
	const char *hostname);

static void ggp_resolver_purple_cleanup(void **private_data, int force);

static void ggp_resolver_purple_cb(GSList *hosts, gpointer cbdata,
	const char *error_message);

typedef struct
{
	PurpleDnsQueryData *purpleQuery;
	
	/**
	 * File descriptors:
	 *  pipes[0] - for reading
	 *  pipes[1] - for writing
	 */
	int pipes[2];
} ggp_resolver_purple_data;


extern void ggp_resolver_purple_setup(void)
{
	if (gg_global_set_custom_resolver(ggp_resolver_purple_start,
		ggp_resolver_purple_cleanup) != 0)
	{
		purple_debug_error("gg", "failed to set custom resolver\n");
	}
}

void ggp_resolver_purple_cb(GSList *hosts, gpointer cbdata,
	const char *error_message)
{
	ggp_resolver_purple_data *data = (ggp_resolver_purple_data*)cbdata;
	const int fd = data->pipes[1]; /* TODO: invalid read after logoff */
	int ipv4_count, all_count, write_size;
	struct in_addr *addresses;
	
	purple_debug_misc("gg", "ggp_resolver_purple_cb(%p, %p, \"%s\")\n",
		hosts, cbdata, error_message);
	
	if (error_message)
	{
		purple_debug_error("gg", "ggp_resolver_purple_cb failed: %s\n",
			error_message);
	}
	
	all_count = g_slist_length(hosts);
	g_assert(all_count % 2 == 0);
	all_count /= 2;
	addresses = malloc((all_count + 1) * sizeof(struct in_addr));
	
	ipv4_count = 0;
	while (hosts && (hosts = g_slist_delete_link(hosts, hosts)))
	{
		const struct sockaddr *addr = hosts->data;
		char dst[INET6_ADDRSTRLEN];
		
		if (addr->sa_family == AF_INET6)
		{
			inet_ntop(addr->sa_family,
				&((struct sockaddr_in6 *) addr)->sin6_addr,
				dst, sizeof(dst));
			purple_debug_misc("gg", "ggp_resolver_purple_cb "
				"ipv6 (ignore): %s\n", dst);
		}
		else if (addr->sa_family == AF_INET)
		{
			const struct in_addr addr_ipv4 =
				((struct sockaddr_in *) addr)->sin_addr;
			inet_ntop(addr->sa_family, &addr_ipv4,
				dst, sizeof(dst));
			purple_debug_misc("gg", "ggp_resolver_purple_cb "
				"ipv4: %s\n", dst);
			
			g_assert(ipv4_count < all_count);
			addresses[ipv4_count++] = addr_ipv4;
		}
		else
		{
			purple_debug_warning("gg", "ggp_resolver_purple_cb "
				"unexpected sa_family: %d\n", addr->sa_family);
		}
		
		g_free(hosts->data);
		hosts = g_slist_delete_link(hosts, hosts);
	}
	
	addresses[ipv4_count].s_addr = INADDR_NONE;
	
	write_size = (ipv4_count + 1) * sizeof(struct in_addr);
	if (write(fd, addresses, write_size) != write_size)
	{
		purple_debug_error("gg",
			"ggp_resolver_purple_cb write error\n");
	}
	free(addresses);
}

int ggp_resolver_purple_start(int *fd, void **private_data,
	const char *hostname)
{
	ggp_resolver_purple_data *data;
	purple_debug_misc("gg", "ggp_resolver_purple_start(%p, %p, \"%s\")\n",
		fd, private_data, hostname);
	
	data = malloc(sizeof(ggp_resolver_purple_data));
	*private_data = (void*)data;
	data->purpleQuery = NULL;
	data->pipes[0] = 0;
	data->pipes[1] = 0;
	
	if (purple_input_pipe(data->pipes) != 0)
	{
		purple_debug_error("gg", "ggp_resolver_purple_start: "
			"unable to create pipe\n");
		ggp_resolver_purple_cleanup(private_data, 0);
		return -1;
	}
	
	*fd = data->pipes[0];
	
	/* account and port is unknown in this context */
	data->purpleQuery = purple_dnsquery_a(NULL, hostname, 80,
		ggp_resolver_purple_cb, (gpointer)data);

	if (!data->purpleQuery)
	{
		purple_debug_error("gg", "ggp_resolver_purple_start: "
			"unable to call purple_dnsquery_a\n");
		ggp_resolver_purple_cleanup(private_data, 0);
		return -1;
	}
	
	return 0;
}

void ggp_resolver_purple_cleanup(void **private_data, int force)
{
	ggp_resolver_purple_data *data =
		(ggp_resolver_purple_data*)(*private_data);
	
	purple_debug_misc("gg", "ggp_resolver_purple_cleanup(%p, %d)\n",
		private_data, force);
	
	if (!data)
		return;
	*private_data = NULL;
	
	if (data->pipes[0])
		close(data->pipes[0]);
	if (data->pipes[1])
		close(data->pipes[1]);
	
	free(data);
}
