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

#include <libgadu.h>
#include "resolver-purple.h"

#include <gio/gio.h>

static int ggp_resolver_purple_start(int *fd, void **private_data,
	const char *hostname);

static void ggp_resolver_purple_cleanup(void **private_data, int force);

static void ggp_resolver_purple_cb(GObject *sender, GAsyncResult *res, gpointer data);

typedef struct
{
	GCancellable *cancellable;

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

void ggp_resolver_purple_cb(GObject *sender, GAsyncResult *res, gpointer cbdata) {
	GList *addresses = NULL, *in_addrs = NULL, *l = NULL;
	GError *error = NULL;
	gsize native_size = 0; /* this is kind of dirty, but it'll be initialized before we use it */

	ggp_resolver_purple_data *data = (ggp_resolver_purple_data*)cbdata;
	const int fd = data->pipes[1];

	addresses = g_resolver_lookup_by_name_finish(g_resolver_get_default(), res, &error);
	if(addresses == NULL) {
		purple_debug_error("gg", "ggp_resolver_purple_cb failed: %s\n",
			error->message);

		g_error_free(error);
	} else {
		purple_debug_misc("gg", "ggp_resolver_purple_cb succeeded: (%p, %p)\n",
			addresses, cbdata);
	}

	g_object_unref(G_OBJECT(data->cancellable));
	data->cancellable = NULL;

	for(l = addresses; l; l = l->next) {
		GInetAddress *inet_address = G_INET_ADDRESS(l->data);
		GSocketFamily family = G_SOCKET_FAMILY_INVALID;
		gchar *ip_address = g_inet_address_to_string(inet_address);

		family = g_inet_address_get_family(inet_address);

		switch(family) {
			case G_SOCKET_FAMILY_IPV4:
				purple_debug_misc("gg", "ggp_resolver_purple_cb "
					"ipv4: %s\n", ip_address);

				native_size = g_inet_address_get_native_size(inet_address);
				in_addrs = g_list_append(in_addrs, g_memdup(g_inet_address_to_bytes(inet_address), native_size));

				break;
			case G_SOCKET_FAMILY_IPV6:
				purple_debug_misc("gg", "ggp_resolver_purple_cb "
					"ipv6 (ignore): %s\n", ip_address);

				break;
			default:
				purple_debug_warning("gg", "ggp_resolver_purple_cb "
					"unexpected sa_family: %d\n",
					family);

				break;
		}

		g_free(ip_address);
	}

	for(l = in_addrs; l; l = l->next) {
		gint write_size = native_size;
		if(write(fd, l->data, write_size) != write_size) {
			purple_debug_error("gg",
				"ggp_resolver_purple_cb write error on %p\n", l->data);
		}

		g_free(l->data);
	}

	g_list_free(in_addrs);
	g_resolver_free_addresses(addresses);
}

int ggp_resolver_purple_start(int *fd, void **private_data,
	const char *hostname)
{
	ggp_resolver_purple_data *data;
	purple_debug_misc("gg", "ggp_resolver_purple_start(%p, %p, \"%s\")\n",
		fd, private_data, hostname);

	data = malloc(sizeof(ggp_resolver_purple_data));
	*private_data = (void*)data;
	data->cancellable = NULL;
	data->pipes[0] = 0;
	data->pipes[1] = 0;

	if (purple_input_pipe(data->pipes) != 0) {
		purple_debug_error("gg", "ggp_resolver_purple_start: "
			"unable to create pipe\n");
		ggp_resolver_purple_cleanup(private_data, 0);
		return -1;
	}

	*fd = data->pipes[0];

	/* account and port is unknown in this context */
	data->cancellable = g_cancellable_new();

	g_resolver_lookup_by_name_async(g_resolver_get_default(),
	                                hostname,
	                                data->cancellable,
	                                ggp_resolver_purple_cb,
	                                (gpointer)data);

	if (!data->cancellable) {
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

	if (G_IS_CANCELLABLE(data->cancellable)) {
		g_cancellable_cancel(data->cancellable);

		g_object_unref(G_OBJECT(data->cancellable));
	}

	if (data->pipes[0])
		close(data->pipes[0]);
	if (data->pipes[1])
		close(data->pipes[1]);

	free(data);
}
