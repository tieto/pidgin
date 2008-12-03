/**
 * @file network.c Network Implementation
 * @ingroup core
 */

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

#include "internal.h"

#ifndef _WIN32
#include <arpa/nameser.h>
#include <resolv.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/ioctl.h>
#else
#include <nspapi.h>
#endif

/* Solaris */
#if defined (__SVR4) && defined (__sun)
#include <sys/sockio.h>
#endif

#include "debug.h"
#include "account.h"
#include "nat-pmp.h"
#include "network.h"
#include "prefs.h"
#include "stun.h"
#include "upnp.h"

/*
 * Calling sizeof(struct ifreq) isn't always correct on
 * Mac OS X (and maybe others).
 */
#ifdef _SIZEOF_ADDR_IFREQ
#  define HX_SIZE_OF_IFREQ(a) _SIZEOF_ADDR_IFREQ(a)
#else
#  define HX_SIZE_OF_IFREQ(a) sizeof(a)
#endif

#ifdef HAVE_NETWORKMANAGER
#include <dbus/dbus-glib.h>
#include <NetworkManager.h>

static DBusGConnection *nm_conn = NULL;
static DBusGProxy *nm_proxy = NULL;
static DBusGProxy *dbus_proxy = NULL;
static NMState nm_state = NM_STATE_UNKNOWN;
static gboolean have_nm_state = FALSE;

#elif defined _WIN32
static int current_network_count;
#endif

struct _PurpleNetworkListenData {
	int listenfd;
	int socket_type;
	gboolean retry;
	gboolean adding;
	PurpleNetworkListenCallback cb;
	gpointer cb_data;
	UPnPMappingAddRemove *mapping_data;
};

#ifdef HAVE_NETWORKMANAGER
static NMState nm_get_network_state(void);
#endif

const unsigned char *
purple_network_ip_atoi(const char *ip)
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
purple_network_set_public_ip(const char *ip)
{
	g_return_if_fail(ip != NULL);

	/* XXX - Ensure the IP address is valid */

	purple_prefs_set_string("/purple/network/public_ip", ip);
}

const char *
purple_network_get_public_ip(void)
{
	return purple_prefs_get_string("/purple/network/public_ip");
}

const char *
purple_network_get_local_system_ip(int fd)
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
		tmp += HX_SIZE_OF_IFREQ(*ifr);

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
purple_network_get_my_ip(int fd)
{
	const char *ip = NULL;
	PurpleStunNatDiscovery *stun;

	/* Check if the user specified an IP manually */
	if (!purple_prefs_get_bool("/purple/network/auto_ip")) {
		ip = purple_network_get_public_ip();
		/* Make sure the IP address entered by the user is valid */
		if ((ip != NULL) && (purple_network_ip_atoi(ip) != NULL))
			return ip;
	} else {
		/* Check if STUN discovery was already done */
		stun = purple_stun_discover(NULL);
		if ((stun != NULL) && (stun->status == PURPLE_STUN_STATUS_DISCOVERED))
			return stun->publicip;

		/* Attempt to get the IP from a NAT device using UPnP */
		ip = purple_upnp_get_public_ip();
		if (ip != NULL)
			return ip;

		/* Attempt to get the IP from a NAT device using NAT-PMP */
		ip = purple_pmp_get_public_ip();
		if (ip != NULL)
			return ip;
	}

	/* Just fetch the IP of the local system */
	return purple_network_get_local_system_ip(fd);
}


static void
purple_network_set_upnp_port_mapping_cb(gboolean success, gpointer data)
{
	PurpleNetworkListenData *listen_data;

	listen_data = data;
	/* TODO: Once we're keeping track of upnp requests... */
	/* listen_data->pnp_data = NULL; */

	if (!success) {
		purple_debug_info("network", "Couldn't create UPnP mapping\n");
		if (listen_data->retry) {
			listen_data->retry = FALSE;
			listen_data->adding = FALSE;
			listen_data->mapping_data = purple_upnp_remove_port_mapping(
						purple_network_get_port_from_fd(listen_data->listenfd),
						(listen_data->socket_type == SOCK_STREAM) ? "TCP" : "UDP",
						purple_network_set_upnp_port_mapping_cb, listen_data);
			return;
		}
	} else if (!listen_data->adding) {
		/* We've tried successfully to remove the port mapping.
		 * Try to add it again */
		listen_data->adding = TRUE;
		listen_data->mapping_data = purple_upnp_set_port_mapping(
					purple_network_get_port_from_fd(listen_data->listenfd),
					(listen_data->socket_type == SOCK_STREAM) ? "TCP" : "UDP",
					purple_network_set_upnp_port_mapping_cb, listen_data);
		return;
	}

	if (listen_data->cb)
		listen_data->cb(listen_data->listenfd, listen_data->cb_data);

	/* Clear the UPnP mapping data, since it's complete and purple_netweork_listen_cancel() will try to cancel
	 * it otherwise. */
	listen_data->mapping_data = NULL;
	purple_network_listen_cancel(listen_data);
}

static gboolean
purple_network_finish_pmp_map_cb(gpointer data)
{
	PurpleNetworkListenData *listen_data;

	listen_data = data;

	if (listen_data->cb)
		listen_data->cb(listen_data->listenfd, listen_data->cb_data);

	purple_network_listen_cancel(listen_data);

	return FALSE;
}

static gboolean listen_map_external = TRUE;
void purple_network_listen_map_external(gboolean map_external)
{
	listen_map_external = map_external;
}

static PurpleNetworkListenData *
purple_network_do_listen(unsigned short port, int socket_type, PurpleNetworkListenCallback cb, gpointer cb_data)
{
	int listenfd = -1;
	int flags;
	const int on = 1;
	PurpleNetworkListenData *listen_data;
	unsigned short actual_port;
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
		purple_debug_warning("network", "getaddrinfo: %s\n", purple_gai_strerror(errnum));
		if (errnum == EAI_SYSTEM)
			purple_debug_warning("network", "getaddrinfo: system error: %s\n", g_strerror(errno));
#else
		purple_debug_warning("network", "getaddrinfo: Error Code = %d\n", errnum);
#endif
		return NULL;
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
			purple_debug_warning("network", "setsockopt: %s\n", g_strerror(errno));
		if (bind(listenfd, next->ai_addr, next->ai_addrlen) == 0)
			break; /* success */
		/* XXX - It is unclear to me (datallah) whether we need to be
		   using a new socket each time */
		close(listenfd);
	}

	freeaddrinfo(res);

	if (next == NULL)
		return NULL;
#else
	struct sockaddr_in sockin;

	if ((listenfd = socket(AF_INET, socket_type, 0)) < 0) {
		purple_debug_warning("network", "socket: %s\n", g_strerror(errno));
		return NULL;
	}

	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) != 0)
		purple_debug_warning("network", "setsockopt: %s\n", g_strerror(errno));

	memset(&sockin, 0, sizeof(struct sockaddr_in));
	sockin.sin_family = PF_INET;
	sockin.sin_port = htons(port);

	if (bind(listenfd, (struct sockaddr *)&sockin, sizeof(struct sockaddr_in)) != 0) {
		purple_debug_warning("network", "bind: %s\n", g_strerror(errno));
		close(listenfd);
		return NULL;
	}
#endif

	if (socket_type == SOCK_STREAM && listen(listenfd, 4) != 0) {
		purple_debug_warning("network", "listen: %s\n", g_strerror(errno));
		close(listenfd);
		return NULL;
	}
	flags = fcntl(listenfd, F_GETFL);
	fcntl(listenfd, F_SETFL, flags | O_NONBLOCK);
#ifndef _WIN32
	fcntl(listenfd, F_SETFD, FD_CLOEXEC);
#endif
	actual_port = purple_network_get_port_from_fd(listenfd);

	purple_debug_info("network", "Listening on port: %hu\n", actual_port);

	listen_data = g_new0(PurpleNetworkListenData, 1);
	listen_data->listenfd = listenfd;
	listen_data->adding = TRUE;
	listen_data->retry = TRUE;
	listen_data->cb = cb;
	listen_data->cb_data = cb_data;
	listen_data->socket_type = socket_type;

	if (!listen_map_external || !purple_prefs_get_bool("/purple/network/map_ports"))
	{
		purple_debug_info("network", "Skipping external port mapping.\n");
		/* The pmp_map_cb does what we want to do */
		purple_timeout_add(0, purple_network_finish_pmp_map_cb, listen_data);
	}
	/* Attempt a NAT-PMP Mapping, which will return immediately */
	else if (purple_pmp_create_map(((socket_type == SOCK_STREAM) ? PURPLE_PMP_TYPE_TCP : PURPLE_PMP_TYPE_UDP),
							  actual_port, actual_port, PURPLE_PMP_LIFETIME))
	{
		purple_debug_info("network", "Created NAT-PMP mapping on port %i\n", actual_port);
		/* We want to return listen_data now, and on the next run loop trigger the cb and destroy listen_data */
		purple_timeout_add(0, purple_network_finish_pmp_map_cb, listen_data);
	}
	else
	{
		/* Attempt a UPnP Mapping */
		listen_data->mapping_data = purple_upnp_set_port_mapping(
						 actual_port,
						 (socket_type == SOCK_STREAM) ? "TCP" : "UDP",
						 purple_network_set_upnp_port_mapping_cb, listen_data);
	}

	return listen_data;
}

PurpleNetworkListenData *
purple_network_listen(unsigned short port, int socket_type,
		PurpleNetworkListenCallback cb, gpointer cb_data)
{
	g_return_val_if_fail(port != 0, NULL);

	return purple_network_do_listen(port, socket_type, cb, cb_data);
}

PurpleNetworkListenData *
purple_network_listen_range(unsigned short start, unsigned short end,
		int socket_type, PurpleNetworkListenCallback cb, gpointer cb_data)
{
	PurpleNetworkListenData *ret = NULL;

	if (purple_prefs_get_bool("/purple/network/ports_range_use")) {
		start = purple_prefs_get_int("/purple/network/ports_range_start");
		end = purple_prefs_get_int("/purple/network/ports_range_end");
	} else {
		if (end < start)
			end = start;
	}

	for (; start <= end; start++) {
		ret = purple_network_do_listen(start, socket_type, cb, cb_data);
		if (ret != NULL)
			break;
	}

	return ret;
}

void purple_network_listen_cancel(PurpleNetworkListenData *listen_data)
{
	if (listen_data->mapping_data != NULL)
		purple_upnp_cancel_port_mapping(listen_data->mapping_data);

	g_free(listen_data);
}

unsigned short
purple_network_get_port_from_fd(int fd)
{
	struct sockaddr_in addr;
	socklen_t len;

	g_return_val_if_fail(fd >= 0, 0);

	len = sizeof(addr);
	if (getsockname(fd, (struct sockaddr *) &addr, &len) == -1) {
		purple_debug_warning("network", "getsockname: %s\n", g_strerror(errno));
		return 0;
	}

	return ntohs(addr.sin_port);
}

#ifdef _WIN32
#ifndef NS_NLA
#define NS_NLA 15
#endif
static gint
wpurple_get_connected_network_count(void)
{
	gint net_cnt = 0;

	WSAQUERYSET qs;
	HANDLE h;
	gint retval;
	int errorid;

	memset(&qs, 0, sizeof(WSAQUERYSET));
	qs.dwSize = sizeof(WSAQUERYSET);
	qs.dwNameSpace = NS_NLA;

	retval = WSALookupServiceBegin(&qs, LUP_RETURN_ALL, &h);
	if (retval != ERROR_SUCCESS) {
		gchar *msg;
		errorid = WSAGetLastError();
		msg = g_win32_error_message(errorid);
		purple_debug_warning("network", "Couldn't retrieve NLA SP lookup handle. "
						"NLA service is probably not running. Message: %s (%d).\n",
						msg, errorid);
		g_free(msg);

		return -1;
	} else {
		char buf[4096];
		WSAQUERYSET *res = (LPWSAQUERYSET) buf;
		DWORD size = sizeof(buf);
		while ((retval = WSALookupServiceNext(h, 0, &size, res)) == ERROR_SUCCESS) {
			net_cnt++;
			purple_debug_info("network", "found network '%s'\n",
					res->lpszServiceInstanceName ? res->lpszServiceInstanceName : "(NULL)");
			size = sizeof(buf);
		}

		errorid = WSAGetLastError();
		if (!(errorid == WSA_E_NO_MORE || errorid == WSAENOMORE)) {
			gchar *msg = g_win32_error_message(errorid);
			purple_debug_error("network", "got unexpected NLA response %s (%d)\n", msg, errorid);
			g_free(msg);

			net_cnt = -1;
		}

		retval = WSALookupServiceEnd(h);
	}

	return net_cnt;

}

static gboolean wpurple_network_change_thread_cb(gpointer data)
{
	gint new_count;
	PurpleConnectionUiOps *ui_ops = purple_connections_get_ui_ops();

	new_count = wpurple_get_connected_network_count();

	if (new_count < 0)
		return FALSE;

	purple_debug_info("network", "Received Network Change Notification. Current network count is %d, previous count was %d.\n", new_count, current_network_count);

	purple_signal_emit(purple_network_get_handle(), "network-configuration-changed", NULL);

	if (new_count > 0 && ui_ops != NULL && ui_ops->network_connected != NULL) {
		ui_ops->network_connected();
	} else if (new_count == 0 && current_network_count > 0 &&
			   ui_ops != NULL && ui_ops->network_disconnected != NULL) {
		ui_ops->network_disconnected();
	}

	current_network_count = new_count;

	return FALSE;
}

static gpointer wpurple_network_change_thread(gpointer data)
{
	HANDLE h;
	WSAQUERYSET qs;
	time_t last_trigger = time(NULL);

	int (WSAAPI *MyWSANSPIoctl) (
		HANDLE hLookup, DWORD dwControlCode, LPVOID lpvInBuffer,
		DWORD cbInBuffer, LPVOID lpvOutBuffer, DWORD cbOutBuffer,
		LPDWORD lpcbBytesReturned, LPWSACOMPLETION lpCompletion) = NULL;

	if (!(MyWSANSPIoctl = (void*) wpurple_find_and_loadproc("ws2_32.dll", "WSANSPIoctl"))) {
		g_thread_exit(NULL);
		return NULL;
	}

	while (TRUE) {
		int retval;
		DWORD retLen = 0;

		memset(&qs, 0, sizeof(WSAQUERYSET));
		qs.dwSize = sizeof(WSAQUERYSET);
		qs.dwNameSpace = NS_NLA;
		if (WSALookupServiceBegin(&qs, 0, &h) == SOCKET_ERROR) {
			int errorid = WSAGetLastError();
			gchar *msg = g_win32_error_message(errorid);
			purple_debug_warning("network", "Couldn't retrieve NLA SP lookup handle. "
				"NLA service is probably not running. Message: %s (%d).\n",
				msg, errorid);
			g_free(msg);
			g_thread_exit(NULL);
			return NULL;
		}

		/* Make sure at least 30 seconds have elapsed since the last
		 * notification so we don't peg the cpu if this keeps changing. */
		if ((time(NULL) - last_trigger) < 30)
			Sleep(30000);

		last_trigger = time(NULL);

		/* This will block until there is a network change */
		if (MyWSANSPIoctl(h, SIO_NSP_NOTIFY_CHANGE, NULL, 0, NULL, 0, &retLen, NULL) == SOCKET_ERROR) {
			int errorid = WSAGetLastError();
			gchar *msg = g_win32_error_message(errorid);
			purple_debug_warning("network", "Unable to wait for changes. Message: %s (%d).\n",
				msg, errorid);
			g_free(msg);
		}

		retval = WSALookupServiceEnd(h);

		purple_timeout_add(0, wpurple_network_change_thread_cb, NULL);

	}

	g_thread_exit(NULL);
	return NULL;
}
#endif

gboolean
purple_network_is_available(void)
{
#ifdef HAVE_NETWORKMANAGER
	if (!have_nm_state)
	{
		have_nm_state = TRUE;
		nm_state = nm_get_network_state();
		if (nm_state == NM_STATE_UNKNOWN)
			purple_debug_warning("network", "NetworkManager not active. Assuming connection exists.\n");
	}

	if (nm_state == NM_STATE_UNKNOWN || nm_state == NM_STATE_CONNECTED)
		return TRUE;

	return FALSE;

#elif defined _WIN32
	return (current_network_count > 0);
#else
	return TRUE;
#endif
}

#ifdef HAVE_NETWORKMANAGER
static void
nm_update_state(NMState state)
{
	NMState prev = nm_state;
	PurpleConnectionUiOps *ui_ops = purple_connections_get_ui_ops();

	have_nm_state = TRUE;
	nm_state = state;

	purple_signal_emit(purple_network_get_handle(), "network-configuration-changed", NULL);

	switch(state)
	{
		case NM_STATE_CONNECTED:
			/* Call res_init in case DNS servers have changed */
			res_init();
			if (ui_ops != NULL && ui_ops->network_connected != NULL)
				ui_ops->network_connected();
			break;
		case NM_STATE_ASLEEP:
		case NM_STATE_CONNECTING:
		case NM_STATE_DISCONNECTED:
			if (prev != NM_STATE_CONNECTED && prev != NM_STATE_UNKNOWN)
				break;
			if (ui_ops != NULL && ui_ops->network_disconnected != NULL)
				ui_ops->network_disconnected();
			break;
		case NM_STATE_UNKNOWN:
		default:
			break;
	}
}

static void
nm_state_change_cb(DBusGProxy *proxy, NMState state, gpointer user_data)
{
	purple_debug_info("network", "Got StateChange from NetworkManager: %d.\n", state);
	nm_update_state(state);
}

static NMState
nm_get_network_state(void)
{
	GError *err = NULL;
	NMState state = NM_STATE_UNKNOWN;

	if (!nm_proxy)
		return NM_STATE_UNKNOWN;

	if (!dbus_g_proxy_call(nm_proxy, "state", &err, G_TYPE_INVALID, G_TYPE_UINT, &state, G_TYPE_INVALID)) {
		g_error_free(err);
		return NM_STATE_UNKNOWN;
	}

	return state;
}

static void
nm_dbus_name_owner_changed_cb(DBusGProxy *proxy, char *service, char *old_owner, char *new_owner, gpointer user_data)
{
	if (g_str_equal(service, NM_DBUS_SERVICE)) {
		gboolean old_owner_good = old_owner && (old_owner[0] != '\0');
		gboolean new_owner_good = new_owner && (new_owner[0] != '\0');

		purple_debug_info("network", "Got NameOwnerChanged signal, service = '%s', old_owner = '%s', new_owner = '%s'\n", service, old_owner, new_owner);
		if (!old_owner_good && new_owner_good) {	/* Equivalent to old ServiceCreated signal */
			purple_debug_info("network", "NetworkManager has started.\n");
			nm_update_state(nm_get_network_state());
		} else if (old_owner_good && !new_owner_good) {	/* Equivalent to old ServiceDeleted signal */
			purple_debug_info("network", "NetworkManager has gone away.\n");
			nm_update_state(NM_STATE_UNKNOWN);
		}
	}
}

#endif

void *
purple_network_get_handle(void)
{
	static int handle;

	return &handle;
}

void
purple_network_init(void)
{
#ifdef HAVE_NETWORKMANAGER
	GError *error = NULL;
#endif
#ifdef _WIN32
	GError *err = NULL;
	gint cnt = wpurple_get_connected_network_count();

	if (cnt < 0) /* Assume there is a network */
		current_network_count = 1;
	/* Don't listen for network changes if we can't tell anyway */
	else
	{
		current_network_count = cnt;
		if (!g_thread_create(wpurple_network_change_thread, NULL, FALSE, &err))
			purple_debug_error("network", "Couldn't create Network Monitor thread: %s\n", err ? err->message : "");
	}
#endif

	purple_prefs_add_none  ("/purple/network");
	purple_prefs_add_bool  ("/purple/network/auto_ip", TRUE);
	purple_prefs_add_string("/purple/network/public_ip", "");
	purple_prefs_add_bool  ("/purple/network/map_ports", TRUE);
	purple_prefs_add_bool  ("/purple/network/ports_range_use", FALSE);
	purple_prefs_add_int   ("/purple/network/ports_range_start", 1024);
	purple_prefs_add_int   ("/purple/network/ports_range_end", 2048);

	if(purple_prefs_get_bool("/purple/network/map_ports") || purple_prefs_get_bool("/purple/network/auto_ip"))
		purple_upnp_discover(NULL, NULL);

#ifdef HAVE_NETWORKMANAGER
	nm_conn = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
	if (!nm_conn) {
		purple_debug_warning("network", "Error connecting to DBus System service: %s.\n", error->message);
	} else {
		nm_proxy = dbus_g_proxy_new_for_name(nm_conn,
		                                     NM_DBUS_SERVICE,
		                                     NM_DBUS_PATH,
		                                     NM_DBUS_INTERFACE);
		dbus_g_proxy_add_signal(nm_proxy, "StateChange", G_TYPE_UINT, G_TYPE_INVALID);
		dbus_g_proxy_connect_signal(nm_proxy, "StateChange",
		                            G_CALLBACK(nm_state_change_cb), NULL, NULL);

		dbus_proxy = dbus_g_proxy_new_for_name(nm_conn,
		                                       DBUS_SERVICE_DBUS,
		                                       DBUS_PATH_DBUS,
		                                       DBUS_INTERFACE_DBUS);
		dbus_g_proxy_add_signal(dbus_proxy, "NameOwnerChanged", G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
		dbus_g_proxy_connect_signal(dbus_proxy, "NameOwnerChanged",
		                            G_CALLBACK(nm_dbus_name_owner_changed_cb), NULL, NULL);
	}
#endif

	purple_signal_register(purple_network_get_handle(), "network-configuration-changed",
						   purple_marshal_VOID, NULL, 0);

	purple_pmp_init();
	purple_upnp_init();
}

void
purple_network_uninit(void)
{
#ifdef HAVE_NETWORKMANAGER
	if (nm_proxy) {
		dbus_g_proxy_disconnect_signal(nm_proxy, "StateChange", G_CALLBACK(nm_state_change_cb), NULL);
		g_object_unref(G_OBJECT(nm_proxy));
	}
	if (dbus_proxy) {
		dbus_g_proxy_disconnect_signal(dbus_proxy, "NameOwnerChanged", G_CALLBACK(nm_dbus_name_owner_changed_cb), NULL);
		g_object_unref(G_OBJECT(dbus_proxy));
	}
	if (nm_conn)
		dbus_g_connection_unref(nm_conn);
#endif

	purple_signal_unregister(purple_network_get_handle(),
	                         "network-configuration-changed");
}
