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
 *
 */

/* this is a little piece of code to handle proxy connection */
/* it is intended to : 1st handle http proxy, using the CONNECT command
 , 2nd provide an easy way to add socks support
 , 3rd draw women to it like flies to honey */
#define _PURPLE_PROXY_C_

#include "internal.h"
#include "ciphers/md5hash.h"
#include "debug.h"
#include "http.h"
#include "notify.h"
#include "ntlm.h"
#include "prefs.h"
#include "proxy.h"
#include "util.h"

#include <gio/gio.h>

struct _PurpleProxyInfo
{
	PurpleProxyType type; /* The proxy type.  */

	char *host;           /* The host.        */
	int   port;           /* The port number. */
	char *username;       /* The username.    */
	char *password;       /* The password.    */
};

struct _PurpleProxyConnectData {
	void *handle;
	PurpleProxyConnectFunction connect_cb;
	gpointer data;
	gchar *host;
	int port;
	int fd;
	PurpleProxyInfo *gpi;

	GCancellable *cancellable;
};

static PurpleProxyInfo *global_proxy_info = NULL;

static GSList *handles = NULL;

/*
 * TODO: Eventually (GObjectification) this bad boy will be removed, because it is
 *       a gross fix for a crashy problem.
 */
#define PURPLE_PROXY_CONNECT_DATA_IS_VALID(connect_data) g_slist_find(handles, connect_data)

/**************************************************************************
 * Proxy structure API
 **************************************************************************/
PurpleProxyInfo *
purple_proxy_info_new(void)
{
	return g_new0(PurpleProxyInfo, 1);
}

void
purple_proxy_info_destroy(PurpleProxyInfo *info)
{
	g_return_if_fail(info != NULL);

	g_free(info->host);
	g_free(info->username);
	g_free(info->password);

	g_free(info);
}

void
purple_proxy_info_set_proxy_type(PurpleProxyInfo *info, PurpleProxyType type)
{
	g_return_if_fail(info != NULL);

	info->type = type;
}

void
purple_proxy_info_set_host(PurpleProxyInfo *info, const char *host)
{
	g_return_if_fail(info != NULL);

	g_free(info->host);
	info->host = g_strdup(host);
}

void
purple_proxy_info_set_port(PurpleProxyInfo *info, int port)
{
	g_return_if_fail(info != NULL);

	info->port = port;
}

void
purple_proxy_info_set_username(PurpleProxyInfo *info, const char *username)
{
	g_return_if_fail(info != NULL);

	g_free(info->username);
	info->username = g_strdup(username);
}

void
purple_proxy_info_set_password(PurpleProxyInfo *info, const char *password)
{
	g_return_if_fail(info != NULL);

	g_free(info->password);
	info->password = g_strdup(password);
}

PurpleProxyType
purple_proxy_info_get_proxy_type(const PurpleProxyInfo *info)
{
	g_return_val_if_fail(info != NULL, PURPLE_PROXY_NONE);

	return info->type;
}

const char *
purple_proxy_info_get_host(const PurpleProxyInfo *info)
{
	g_return_val_if_fail(info != NULL, NULL);

	return info->host;
}

int
purple_proxy_info_get_port(const PurpleProxyInfo *info)
{
	g_return_val_if_fail(info != NULL, 0);

	return info->port;
}

const char *
purple_proxy_info_get_username(const PurpleProxyInfo *info)
{
	g_return_val_if_fail(info != NULL, NULL);

	return info->username;
}

const char *
purple_proxy_info_get_password(const PurpleProxyInfo *info)
{
	g_return_val_if_fail(info != NULL, NULL);

	return info->password;
}

/**************************************************************************
 * Global Proxy API
 **************************************************************************/
PurpleProxyInfo *
purple_global_proxy_get_info(void)
{
	return global_proxy_info;
}

void
purple_global_proxy_set_info(PurpleProxyInfo *info)
{
	g_return_if_fail(info != NULL);

	purple_proxy_info_destroy(global_proxy_info);

	global_proxy_info = info;
}


/* index in gproxycmds below, keep them in sync */
#define GNOME_PROXY_MODE 0
#define GNOME_PROXY_USE_SAME_PROXY 1
#define GNOME_PROXY_SOCKS_HOST 2
#define GNOME_PROXY_SOCKS_PORT 3
#define GNOME_PROXY_HTTP_HOST 4
#define GNOME_PROXY_HTTP_PORT 5
#define GNOME_PROXY_HTTP_USER 6
#define GNOME_PROXY_HTTP_PASS 7
#define GNOME2_CMDS 0
#define GNOME3_CMDS 1

/* detect proxy settings for gnome2/gnome3 */
static const char* gproxycmds[][2] = {
	{ "gconftool-2 -g /system/proxy/mode" , "gsettings get org.gnome.system.proxy mode" },
	{ "gconftool-2 -g /system/http_proxy/use_same_proxy", "gsettings get org.gnome.system.proxy use-same-proxy" },
	{ "gconftool-2 -g /system/proxy/socks_host", "gsettings get org.gnome.system.proxy.socks host" },
	{ "gconftool-2 -g /system/proxy/socks_port", "gsettings get org.gnome.system.proxy.socks port" },
	{ "gconftool-2 -g /system/http_proxy/host", "gsettings get org.gnome.system.proxy.http host" },
	{ "gconftool-2 -g /system/http_proxy/port", "gsettings get org.gnome.system.proxy.http port"},
	{ "gconftool-2 -g /system/http_proxy/authentication_user", "gsettings get org.gnome.system.proxy.http authentication-user" },
	{ "gconftool-2 -g /system/http_proxy/authentication_password", "gsettings get org.gnome.system.proxy.http authentication-password" },
};

/*
 * purple_gnome_proxy_get_parameter:
 * @parameter:     One of the GNOME_PROXY_x constants defined above
 * @gnome_version: GNOME2_CMDS or GNOME3_CMDS
 *
 * This is a utility function used to retrieve proxy parameter values from
 * GNOME 2/3 environment.
 *
 * Returns: The value of requested proxy parameter
 */
static char *
purple_gnome_proxy_get_parameter(guint8 parameter, guint8 gnome_version)
{
	gchar *param, *err;
	size_t param_len;

	if (parameter > GNOME_PROXY_HTTP_PASS)
		return NULL;
	if (gnome_version > GNOME3_CMDS)
		return NULL;

	if (!g_spawn_command_line_sync(gproxycmds[parameter][gnome_version],
			&param, &err, NULL, NULL))
		return NULL;
	g_free(err);

	g_strstrip(param);
	if (param[0] == '\'' || param[0] == '\"') {
		param_len = strlen(param);
		memmove(param, param + 1, param_len); /* copy last \0 too */
		--param_len;
		if (param_len > 0 && (param[param_len - 1] == '\'' || param[param_len - 1] == '\"'))
			param[param_len - 1] = '\0';
		g_strstrip(param);
	}

	return param;
}

static PurpleProxyInfo *
purple_gnome_proxy_get_info(void)
{
	static PurpleProxyInfo info = {0, NULL, 0, NULL, NULL};
	gboolean use_same_proxy = FALSE;
	gchar *tmp;
	guint8 gnome_version = GNOME3_CMDS;

	tmp = g_find_program_in_path("gsettings");
	if (tmp == NULL) {
		tmp = g_find_program_in_path("gconftool-2");
		gnome_version = GNOME2_CMDS;
	}
	if (tmp == NULL)
		return purple_global_proxy_get_info();

	g_free(tmp);

	/* Check whether to use a proxy. */
	tmp = purple_gnome_proxy_get_parameter(GNOME_PROXY_MODE, gnome_version);
	if (!tmp)
		return purple_global_proxy_get_info();

	if (purple_strequal(tmp, "none")) {
		info.type = PURPLE_PROXY_NONE;
		g_free(tmp);
		return &info;
	}

	if (!purple_strequal(tmp, "manual")) {
		/* Unknown setting.  Fallback to using our global proxy settings. */
		g_free(tmp);
		return purple_global_proxy_get_info();
	}

	g_free(tmp);

	/* Free the old fields */
	g_free(info.host);
	info.host = NULL;
	g_free(info.username);
	info.username = NULL;
	g_free(info.password);
	info.password = NULL;

	tmp = purple_gnome_proxy_get_parameter(GNOME_PROXY_USE_SAME_PROXY, gnome_version);
	if (!tmp)
		return purple_global_proxy_get_info();

	if (purple_strequal(tmp, "true"))
		use_same_proxy = TRUE;

	g_free(tmp);

	if (!use_same_proxy) {
		info.host = purple_gnome_proxy_get_parameter(GNOME_PROXY_SOCKS_HOST, gnome_version);
		if (!info.host)
			return purple_global_proxy_get_info();
	}

	if (!use_same_proxy && (info.host != NULL) && (*info.host != '\0')) {
		info.type = PURPLE_PROXY_SOCKS5;
		tmp = purple_gnome_proxy_get_parameter(GNOME_PROXY_SOCKS_PORT, gnome_version);
		if (!tmp) {
			g_free(info.host);
			info.host = NULL;
			return purple_global_proxy_get_info();
		}
		info.port = atoi(tmp);
		g_free(tmp);
	} else {
		g_free(info.host);
		info.host = purple_gnome_proxy_get_parameter(GNOME_PROXY_HTTP_HOST, gnome_version);
		if (!info.host)
			return purple_global_proxy_get_info();

		/* If we get this far then we know we're using an HTTP proxy */
		info.type = PURPLE_PROXY_HTTP;

		if (*info.host == '\0')
		{
			purple_debug_info("proxy", "Gnome proxy settings are set to "
					"'manual' but no suitable proxy server is specified.  Using "
					"Pidgin's proxy settings instead.\n");
			g_free(info.host);
			info.host = NULL;
			return purple_global_proxy_get_info();
		}

		info.username = purple_gnome_proxy_get_parameter(GNOME_PROXY_HTTP_USER, gnome_version);
		if (!info.username)
		{
			g_free(info.host);
			info.host = NULL;
			return purple_global_proxy_get_info();
		}

		info.password = purple_gnome_proxy_get_parameter(GNOME_PROXY_HTTP_PASS, gnome_version);
		if (!info.password)
		{
			g_free(info.host);
			info.host = NULL;
			g_free(info.username);
			info.username = NULL;
			return purple_global_proxy_get_info();
		}

		tmp = purple_gnome_proxy_get_parameter(GNOME_PROXY_HTTP_PORT, gnome_version);
		if (!tmp)
		{
			g_free(info.host);
			info.host = NULL;
			g_free(info.username);
			info.username = NULL;
			g_free(info.password);
			info.password = NULL;
			return purple_global_proxy_get_info();
		}
		info.port = atoi(tmp);
		g_free(tmp);
	}

	return &info;
}

#ifdef _WIN32

typedef BOOL (CALLBACK* LPFNWINHTTPGETIEPROXYCONFIG)(/*IN OUT*/ WINHTTP_CURRENT_USER_IE_PROXY_CONFIG* pProxyConfig);

/* This modifies "host" in-place evilly */
static void
_proxy_fill_hostinfo(PurpleProxyInfo *info, char *host, int default_port)
{
	int port = default_port;
	char *d;

	d = g_strrstr(host, ":");
	if (d) {
		*d = '\0';

		d++;
		if (*d)
			sscanf(d, "%d", &port);

		if (port == 0)
			port = default_port;
	}

	purple_proxy_info_set_host(info, host);
	purple_proxy_info_set_port(info, port);
}

static PurpleProxyInfo *
purple_win32_proxy_get_info(void)
{
	static LPFNWINHTTPGETIEPROXYCONFIG MyWinHttpGetIEProxyConfig = NULL;
	static gboolean loaded = FALSE;
	static PurpleProxyInfo info = {0, NULL, 0, NULL, NULL};

	WINHTTP_CURRENT_USER_IE_PROXY_CONFIG ie_proxy_config;

	if (!loaded) {
		loaded = TRUE;
		MyWinHttpGetIEProxyConfig = (LPFNWINHTTPGETIEPROXYCONFIG)
			wpurple_find_and_loadproc("winhttp.dll", "WinHttpGetIEProxyConfigForCurrentUser");
		if (!MyWinHttpGetIEProxyConfig)
			purple_debug_warning("proxy", "Unable to read Windows Proxy Settings.\n");
	}

	if (!MyWinHttpGetIEProxyConfig)
		return NULL;

	ZeroMemory(&ie_proxy_config, sizeof(ie_proxy_config));
	if (!MyWinHttpGetIEProxyConfig(&ie_proxy_config)) {
		purple_debug_error("proxy", "Error reading Windows Proxy Settings(%lu).\n", GetLastError());
		return NULL;
	}

	/* We can't do much if it is autodetect*/
	if (ie_proxy_config.fAutoDetect) {
		purple_debug_error("proxy", "Windows Proxy Settings set to autodetect (not supported).\n");

		/* TODO: For 3.0.0 we'll revisit this (maybe)*/

		return NULL;

	} else if (ie_proxy_config.lpszProxy) {
		gchar *proxy_list = g_utf16_to_utf8(ie_proxy_config.lpszProxy, -1,
									 NULL, NULL, NULL);

		/* We can't do anything about the bypass list, as we don't have the url */
		/* TODO: For 3.0.0 we'll revisit this*/

		/* There are proxy settings for several protocols */
		if (proxy_list && *proxy_list) {
			char *specific = NULL, *tmp;

			/* If there is only a global proxy, which  means "HTTP" */
			if (!strchr(proxy_list, ';') || (specific = g_strstr_len(proxy_list, -1, "http=")) != NULL) {

				if (specific) {
					specific += strlen("http=");
					tmp = strchr(specific, ';');
					if (tmp)
						*tmp = '\0';
					/* specific now points the proxy server (and port) */
				} else
					specific = proxy_list;

				purple_proxy_info_set_proxy_type(&info, PURPLE_PROXY_HTTP);
				_proxy_fill_hostinfo(&info, specific, 80);
				/* TODO: is there a way to set the username/password? */
				purple_proxy_info_set_username(&info, NULL);
				purple_proxy_info_set_password(&info, NULL);

				purple_debug_info("proxy", "Windows Proxy Settings: HTTP proxy: '%s:%d'.\n",
								  purple_proxy_info_get_host(&info),
								  purple_proxy_info_get_port(&info));

			} else if ((specific = g_strstr_len(proxy_list, -1, "socks=")) != NULL) {

				specific += strlen("socks=");
				tmp = strchr(specific, ';');
				if (tmp)
					*tmp = '\0';
				/* specific now points the proxy server (and port) */

				purple_proxy_info_set_proxy_type(&info, PURPLE_PROXY_SOCKS5);
				_proxy_fill_hostinfo(&info, specific, 1080);
				/* TODO: is there a way to set the username/password? */
				purple_proxy_info_set_username(&info, NULL);
				purple_proxy_info_set_password(&info, NULL);

				purple_debug_info("proxy", "Windows Proxy Settings: SOCKS5 proxy: '%s:%d'.\n",
								  purple_proxy_info_get_host(&info),
								  purple_proxy_info_get_port(&info));

			} else {

				purple_debug_info("proxy", "Windows Proxy Settings: No supported proxy specified.\n");

				purple_proxy_info_set_proxy_type(&info, PURPLE_PROXY_NONE);

			}
		}

		/* TODO: Fix API to be able look at proxy bypass settings */

		g_free(proxy_list);
	} else {
		purple_debug_info("proxy", "No Windows proxy set.\n");
		purple_proxy_info_set_proxy_type(&info, PURPLE_PROXY_NONE);
	}

	if (ie_proxy_config.lpszAutoConfigUrl)
		GlobalFree(ie_proxy_config.lpszAutoConfigUrl);
	if (ie_proxy_config.lpszProxy)
		GlobalFree(ie_proxy_config.lpszProxy);
	if (ie_proxy_config.lpszProxyBypass)
		GlobalFree(ie_proxy_config.lpszProxyBypass);

	return &info;
}
#endif


/**************************************************************************
 * Proxy API
 **************************************************************************/

/*
 * Whoever calls this needs to have called
 * purple_proxy_connect_data_disconnect() beforehand.
 */
static void
purple_proxy_connect_data_destroy(PurpleProxyConnectData *connect_data)
{
	if (!PURPLE_PROXY_CONNECT_DATA_IS_VALID(connect_data))
		return;

	handles = g_slist_remove(handles, connect_data);

	if(G_IS_CANCELLABLE(connect_data->cancellable)) {
		g_cancellable_cancel(connect_data->cancellable);

		g_object_unref(G_OBJECT(connect_data->cancellable));

		connect_data->cancellable = NULL;
	}

	g_free(connect_data->host);
	g_free(connect_data);
}

/*
 * purple_proxy_connect_data_disconnect:
 * @error_message: An error message explaining why the connection
 *        failed.  This will be passed to the callback function
 *        specified in the call to purple_proxy_connect().  If the
 *        connection was successful then pass in null.
 *
 * Free all information dealing with a connection attempt and
 * reset the connect_data to prepare for it to try to connect
 * to another IP address.
 *
 * If an error message is passed in, then we know the connection
 * attempt failed. If so, we call the callback with the given
 * error message, then destroy the connect_data.
 */
static void
purple_proxy_connect_data_disconnect(PurpleProxyConnectData *connect_data, const gchar *error_message)
{
	if (connect_data->fd >= 0)
	{
		close(connect_data->fd);
		connect_data->fd = -1;
	}

	if (error_message != NULL)
	{
		purple_debug_error("proxy", "Connection attempt failed: %s\n",
				error_message);

		/* Everything failed!  Tell the originator of the request. */
		connect_data->connect_cb(connect_data->data, -1, error_message);
		purple_proxy_connect_data_destroy(connect_data);
	}
}

static void
purple_proxy_connect_data_connected(PurpleProxyConnectData *connect_data)
{
	purple_debug_info("proxy", "Connected to %s:%d.\n",
	                  connect_data->host, connect_data->port);

	connect_data->connect_cb(connect_data->data, connect_data->fd, NULL);

	/*
	 * We've passed the file descriptor to the protocol, so it's no longer
	 * our responsibility, and we should be careful not to free it when
	 * we destroy the connect_data.
	 */
	connect_data->fd = -1;

	purple_proxy_connect_data_disconnect(connect_data, NULL);
	purple_proxy_connect_data_destroy(connect_data);
}

PurpleProxyInfo *
purple_proxy_get_setup(PurpleAccount *account)
{
	PurpleProxyInfo *gpi = NULL;
	const gchar *tmp;

	/* This is used as a fallback so we don't overwrite the selected proxy type */
	static PurpleProxyInfo *tmp_none_proxy_info = NULL;
	if (!tmp_none_proxy_info) {
		tmp_none_proxy_info = purple_proxy_info_new();
		purple_proxy_info_set_proxy_type(tmp_none_proxy_info, PURPLE_PROXY_NONE);
	}

	if (account && purple_account_get_proxy_info(account) != NULL) {
		gpi = purple_account_get_proxy_info(account);
		if (purple_proxy_info_get_proxy_type(gpi) == PURPLE_PROXY_USE_GLOBAL)
			gpi = NULL;
	}
	if (gpi == NULL) {
		if (purple_running_gnome())
			gpi = purple_gnome_proxy_get_info();
		else
			gpi = purple_global_proxy_get_info();
	}

	if (purple_proxy_info_get_proxy_type(gpi) == PURPLE_PROXY_USE_ENVVAR) {
		if ((tmp = g_getenv("HTTP_PROXY")) != NULL ||
			(tmp = g_getenv("http_proxy")) != NULL ||
			(tmp = g_getenv("HTTPPROXY")) != NULL)
		{
			PurpleHttpURL *url;

			/* http_proxy-format:
			 * export http_proxy="http://user:passwd@your.proxy.server:port/"
			 */
			url = purple_http_url_parse(tmp);
			if (!url) {
				purple_debug_warning("proxy", "Couldn't parse URL\n");
				return gpi;
			}

			purple_proxy_info_set_host(gpi, purple_http_url_get_host(url));
			purple_proxy_info_set_username(gpi, purple_http_url_get_username(url));
			purple_proxy_info_set_password(gpi, purple_http_url_get_password(url));
			purple_proxy_info_set_port(gpi, purple_http_url_get_port(url));

			/* XXX: Do we want to skip this step if user/password/port were part of url? */
			if ((tmp = g_getenv("HTTP_PROXY_USER")) != NULL ||
				(tmp = g_getenv("http_proxy_user")) != NULL ||
				(tmp = g_getenv("HTTPPROXYUSER")) != NULL)
				purple_proxy_info_set_username(gpi, tmp);

			if ((tmp = g_getenv("HTTP_PROXY_PASS")) != NULL ||
				(tmp = g_getenv("http_proxy_pass")) != NULL ||
				(tmp = g_getenv("HTTPPROXYPASS")) != NULL)
				purple_proxy_info_set_password(gpi, tmp);

			if ((tmp = g_getenv("HTTP_PROXY_PORT")) != NULL ||
				(tmp = g_getenv("http_proxy_port")) != NULL ||
				(tmp = g_getenv("HTTPPROXYPORT")) != NULL)
				purple_proxy_info_set_port(gpi, atoi(tmp));
		} else {
#ifdef _WIN32
			PurpleProxyInfo *wgpi;
			if ((wgpi = purple_win32_proxy_get_info()) != NULL)
				return wgpi;
#endif
			/* no proxy environment variable found, don't use a proxy */
			purple_debug_info("proxy", "No environment settings found, not using a proxy\n");
			gpi = tmp_none_proxy_info;
		}

	}

	return gpi;
}

/* Grabbed duplicate_fd() from GLib's testcases (gio/tests/socket.c).
 * Can be dropped once this API has been converted to Gio.
 */
static int
duplicate_fd (int fd)
{
#ifdef G_OS_WIN32
  HANDLE newfd;

  if (!DuplicateHandle (GetCurrentProcess (),
                        (HANDLE)fd,
                        GetCurrentProcess (),
                        &newfd,
                        0,
                        FALSE,
                        DUPLICATE_SAME_ACCESS))
    {
      return -1;
    }

  return (int)newfd;
#else
  return dup (fd);
#endif
}
/* End function grabbed from GLib */

static void
connect_to_host_cb(GObject *source, GAsyncResult *res, gpointer user_data)
{
	PurpleProxyConnectData *connect_data = user_data;
	GSocketConnection *conn;
	GError *error = NULL;
	GSocket *socket;

	conn = g_socket_client_connect_to_host_finish(G_SOCKET_CLIENT(source),
			res, &error);
	if (conn == NULL) {
		/* Ignore cancelled error as that signifies connect_data has
		 * been freed
		 */
		if (!g_error_matches(error, G_IO_ERROR,
				G_IO_ERROR_CANCELLED)) {
			purple_debug_error("proxy", "Unable to connect to "
					"destination host: %s\n",
					error->message);
			purple_proxy_connect_data_disconnect(connect_data,
					"Unable to connect to destination "
					"host.\n");
		}

		g_clear_error(&error);
		return;
	}

	socket = g_socket_connection_get_socket(conn);
	g_assert(socket != NULL);

	/* Duplicate the file descriptor, and then free the connection.
	 * libpurple's proxy code doesn't keep an object around for the
	 * lifetime of the connection. Therefore, in order to not leak
	 * memory, the GSocketConnection must be freed here. In order
	 * to avoid the double close/free of the file descriptor, the 
	 * file descriptor is duplicated.
	 */
	connect_data->fd = duplicate_fd(g_socket_get_fd(socket));
	g_object_unref(conn);

	purple_proxy_connect_data_connected(connect_data);
}

PurpleProxyConnectData *
purple_proxy_connect(void *handle, PurpleAccount *account,
				   const char *host, int port,
				   PurpleProxyConnectFunction connect_cb, gpointer data)
{
	PurpleProxyConnectData *connect_data;
	GProxyResolver *resolver;
	GSocketClient *client;

	g_return_val_if_fail(host       != NULL, NULL);
	g_return_val_if_fail(port       >  0,    NULL);
	g_return_val_if_fail(connect_cb != NULL, NULL);

	connect_data = g_new0(PurpleProxyConnectData, 1);
	connect_data->fd = -1;
	connect_data->handle = handle;
	connect_data->connect_cb = connect_cb;
	connect_data->data = data;
	connect_data->host = g_strdup(host);
	connect_data->port = port;
	connect_data->gpi = purple_proxy_get_setup(account);

	resolver = purple_proxy_get_proxy_resolver(account);

	if (resolver == NULL) {
		/* purple_proxy_get_proxy_resolver already has debug output */
		purple_proxy_connect_data_destroy(connect_data);
		return NULL;
	}

	connect_data->cancellable = g_cancellable_new();

	client = g_socket_client_new();
	g_socket_client_set_proxy_resolver(client, resolver);
	g_object_unref(resolver);

	purple_debug_info("proxy", "Attempting connection to %s:%u\n",
			host, port);

	g_socket_client_connect_to_host_async(client, host, port,
			connect_data->cancellable, connect_to_host_cb,
			connect_data);
	g_object_unref(client);

	handles = g_slist_prepend(handles, connect_data);

	return connect_data;
}

static void
socks5_proxy_connect_cb(GObject *source, GAsyncResult *res, gpointer user_data)
{
	PurpleProxyConnectData *connect_data = user_data;
	GIOStream *stream;
	GError *error = NULL;
	GSocket *socket;

	stream = g_proxy_connect_finish(G_PROXY(source), res, &error);

	if (stream == NULL) {
		/* Ignore cancelled error as that signifies connect_data has
		 * been freed
		 */
		if (!g_error_matches(error, G_IO_ERROR,
				G_IO_ERROR_CANCELLED)) {
			purple_debug_error("proxy", "Unable to connect to "
					"destination host: %s\n",
					error->message);
			purple_proxy_connect_data_disconnect(connect_data,
					"Unable to connect to destination "
					"host.\n");
		}

		g_clear_error(&error);
		return;
	}

	if (!G_IS_SOCKET_CONNECTION(stream)) {
		purple_debug_error("proxy",
				"GProxy didn't return a GSocketConnection.\n");
		purple_proxy_connect_data_disconnect(connect_data,
				"GProxy didn't return a GSocketConnection.\n");
		g_object_unref(stream);
		return;
	}

	socket = g_socket_connection_get_socket(G_SOCKET_CONNECTION(stream));

	/* Duplicate the file descriptor, and then free the connection.
	 * libpurple's proxy code doesn't keep an object around for the
	 * lifetime of the connection. Therefore, in order to not leak
	 * memory, the GSocketConnection (aka GIOStream here) must be
	 * freed here. In order to avoid the double close/free of the
	 * file descriptor, the file descriptor is duplicated.
	 */
	connect_data->fd = duplicate_fd(g_socket_get_fd(socket));
	g_object_unref(stream);

	purple_proxy_connect_data_connected(connect_data);
}

/* This is called when we connect to the SOCKS5 proxy server (through any
 * relevant account proxy)
 */
static void
socks5_connect_to_host_cb(GObject *source, GAsyncResult *res,
		gpointer user_data)
{
	PurpleProxyConnectData *connect_data = user_data;
	GSocketConnection *conn;
	GError *error = NULL;
	GProxy *proxy;
	PurpleProxyInfo *info;
	GSocketAddress *addr;
	GInetSocketAddress *inet_addr;
	GSocketAddress *proxy_addr;

	conn = g_socket_client_connect_to_host_finish(G_SOCKET_CLIENT(source),
			res, &error);
	if (conn == NULL) {
		/* Ignore cancelled error as that signifies connect_data has
		 * been freed
		 */
		if (!g_error_matches(error, G_IO_ERROR,
				G_IO_ERROR_CANCELLED)) {
			purple_debug_error("proxy", "Unable to connect to "
					"SOCKS5 host: %s\n", error->message);
			purple_proxy_connect_data_disconnect(connect_data,
					"Unable to connect to SOCKS5 host.\n");
		}

		g_clear_error(&error);
		return;
	}

	proxy = g_proxy_get_default_for_protocol("socks5");
	if (proxy == NULL) {
		purple_debug_error("proxy", "SOCKS5 proxy backend missing.\n");
		purple_proxy_connect_data_disconnect(connect_data,
				"SOCKS5 proxy backend missing.\n");
		g_object_unref(conn);
		return;
	}

	info = connect_data->gpi;

	addr = g_socket_connection_get_remote_address(conn, &error);
	if (addr == NULL) {
		purple_debug_error("proxy", "Unable to retrieve SOCKS5 host "
				"address from connection: %s\n",
				error->message);
		purple_proxy_connect_data_disconnect(connect_data,
				"Unable to retrieve SOCKS5 host address from "
				"connection");
		g_object_unref(conn);
		g_object_unref(proxy);
		g_clear_error(&error);
		return;
	}

	inet_addr = G_INET_SOCKET_ADDRESS(addr);

	proxy_addr = g_proxy_address_new(
			g_inet_socket_address_get_address(inet_addr),
			g_inet_socket_address_get_port(inet_addr),
			"socks5", connect_data->host, connect_data->port,
			purple_proxy_info_get_username(info),
			purple_proxy_info_get_password(info));
	g_object_unref(inet_addr);

	purple_debug_info("proxy", "Initiating SOCKS5 negotiation.\n");

	purple_debug_info("proxy",
			   "Connecting to %s:%d via %s:%d using SOCKS5\n",
			   connect_data->host, connect_data->port,
			   purple_proxy_info_get_host(connect_data->gpi),
			   purple_proxy_info_get_port(connect_data->gpi));

	g_proxy_connect_async(proxy, G_IO_STREAM(conn),
			G_PROXY_ADDRESS(proxy_addr),
			connect_data->cancellable,
			socks5_proxy_connect_cb, connect_data);
	g_object_unref(proxy_addr);
	g_object_unref(conn);
	g_object_unref(proxy);
}

/*
 * Combine some of this code with purple_proxy_connect()
 */
PurpleProxyConnectData *
purple_proxy_connect_socks5_account(void *handle, PurpleAccount *account,
						  PurpleProxyInfo *gpi,
						  const char *host, int port,
						  PurpleProxyConnectFunction connect_cb,
						  gpointer data)
{
	PurpleProxyConnectData *connect_data;
	GProxyResolver *resolver;
	GSocketClient *client;

	g_return_val_if_fail(host       != NULL, NULL);
	g_return_val_if_fail(port       >= 0,    NULL);
	g_return_val_if_fail(connect_cb != NULL, NULL);

	connect_data = g_new0(PurpleProxyConnectData, 1);
	connect_data->fd = -1;
	connect_data->handle = handle;
	connect_data->connect_cb = connect_cb;
	connect_data->data = data;
	connect_data->host = g_strdup(host);
	connect_data->port = port;
	connect_data->gpi = gpi;

	/* If there is an account proxy, use it to connect to the desired SOCKS5
	 * proxy.
	 */
	resolver = purple_proxy_get_proxy_resolver(account);

	if (resolver == NULL) {
		/* purple_proxy_get_proxy_resolver already has debug output */
		purple_proxy_connect_data_destroy(connect_data);
		return NULL;
	}

	connect_data->cancellable = g_cancellable_new();

	client = g_socket_client_new();
	g_socket_client_set_proxy_resolver(client, resolver);
	g_object_unref(resolver);

	purple_debug_info("proxy",
			   "Connecting to %s:%d via %s:%d using SOCKS5\n",
			   connect_data->host, connect_data->port,
			   purple_proxy_info_get_host(connect_data->gpi),
			   purple_proxy_info_get_port(connect_data->gpi));

	g_socket_client_connect_to_host_async(client,
			purple_proxy_info_get_host(connect_data->gpi),
			purple_proxy_info_get_port(connect_data->gpi),
			connect_data->cancellable, socks5_connect_to_host_cb,
			connect_data);
	g_object_unref(client);

	handles = g_slist_prepend(handles, connect_data);

	return connect_data;
}

void
purple_proxy_connect_cancel(PurpleProxyConnectData *connect_data)
{
	g_return_if_fail(connect_data != NULL);

	purple_proxy_connect_data_disconnect(connect_data, NULL);
	purple_proxy_connect_data_destroy(connect_data);
}

void
purple_proxy_connect_cancel_with_handle(void *handle)
{
	GSList *l, *l_next;

	for (l = handles; l != NULL; l = l_next) {
		PurpleProxyConnectData *connect_data = l->data;

		l_next = l->next;

		if (connect_data->handle == handle)
			purple_proxy_connect_cancel(connect_data);
	}
}

GProxyResolver *
purple_proxy_get_proxy_resolver(PurpleAccount *account)
{
	PurpleProxyInfo *info = purple_proxy_get_setup(account);
	const gchar *protocol;
	const gchar *username;
	const gchar *password;
	gchar *auth;
	gchar *proxy;
	GProxyResolver *resolver;

	if (purple_proxy_info_get_proxy_type(info) == PURPLE_PROXY_NONE) {
		/* Return an empty simple resolver, which will resolve on direct
		 * connection. */
		return g_simple_proxy_resolver_new(NULL, NULL);
	}

	switch (purple_proxy_info_get_proxy_type(info))
	{
		/* PURPLE_PROXY_NONE already handled above */

		case PURPLE_PROXY_USE_ENVVAR:
			/* Intentional passthrough */
		case PURPLE_PROXY_HTTP:
			protocol = "http";
			break;
		case PURPLE_PROXY_SOCKS4:
			protocol = "socks4";
			break;
		case PURPLE_PROXY_SOCKS5:
			/* Intentional passthrough */
		case PURPLE_PROXY_TOR:
			protocol = "socks5";
			break;

		default:
			purple_debug_error("proxy",
					"Invalid Proxy type (%d) specified.\n",
					purple_proxy_info_get_proxy_type(info));
			return NULL;
	}


	if (purple_proxy_info_get_host(info) == NULL ||
			purple_proxy_info_get_port(info) <= 0) {
		purple_notify_error(NULL, NULL,
				_("Invalid proxy settings"),
				_("Either the host name or port number "
				"specified for your given proxy type is "
				"invalid."),
				purple_request_cpar_from_account( account));
		return NULL;
	}

	/* Everything checks out. Create and return the GProxyResolver */

	username = purple_proxy_info_get_username(info);
	password = purple_proxy_info_get_password(info);

	/* Username and password are optional */
	if (username != NULL && password != NULL) {
		auth = g_strdup_printf("%s:%s@", username, password);
	} else if (username != NULL) {
		auth = g_strdup_printf("%s@", username);
	} else {
		auth = NULL;
	}

	proxy = g_strdup_printf("%s://%s%s:%i", protocol,
			auth != NULL ? auth : "",
			purple_proxy_info_get_host(info),
			purple_proxy_info_get_port(info));
	g_free(auth);

	resolver = g_simple_proxy_resolver_new(proxy, NULL);
	g_free(proxy);

	return resolver;
}

static void
proxy_pref_cb(const char *name, PurplePrefType type,
			  gconstpointer value, gpointer data)
{
	PurpleProxyInfo *info = purple_global_proxy_get_info();

	if (purple_strequal(name, "/purple/proxy/type")) {
		int proxytype;
		const char *type = value;

		if (purple_strequal(type, "none"))
			proxytype = PURPLE_PROXY_NONE;
		else if (purple_strequal(type, "http"))
			proxytype = PURPLE_PROXY_HTTP;
		else if (purple_strequal(type, "socks4"))
			proxytype = PURPLE_PROXY_SOCKS4;
		else if (purple_strequal(type, "socks5"))
			proxytype = PURPLE_PROXY_SOCKS5;
		else if (purple_strequal(type, "tor"))
			proxytype = PURPLE_PROXY_TOR;
		else if (purple_strequal(type, "envvar"))
			proxytype = PURPLE_PROXY_USE_ENVVAR;
		else
			proxytype = -1;

		purple_proxy_info_set_proxy_type(info, proxytype);
	} else if (purple_strequal(name, "/purple/proxy/host"))
		purple_proxy_info_set_host(info, value);
	else if (purple_strequal(name, "/purple/proxy/port"))
		purple_proxy_info_set_port(info, GPOINTER_TO_INT(value));
	else if (purple_strequal(name, "/purple/proxy/username"))
		purple_proxy_info_set_username(info, value);
	else if (purple_strequal(name, "/purple/proxy/password"))
		purple_proxy_info_set_password(info, value);
}

void *
purple_proxy_get_handle()
{
	static int handle;

	return &handle;
}

void
purple_proxy_init(void)
{
	void *handle;

	/* Initialize a default proxy info struct. */
	global_proxy_info = purple_proxy_info_new();

	/* Proxy */
	purple_prefs_add_none("/purple/proxy");
	purple_prefs_add_string("/purple/proxy/type", "none");
	purple_prefs_add_string("/purple/proxy/host", "");
	purple_prefs_add_int("/purple/proxy/port", 0);
	purple_prefs_add_string("/purple/proxy/username", "");
	purple_prefs_add_string("/purple/proxy/password", "");
	purple_prefs_add_bool("/purple/proxy/socks4_remotedns", FALSE);

	/* Setup callbacks for the preferences. */
	handle = purple_proxy_get_handle();
	purple_prefs_connect_callback(handle, "/purple/proxy/type", proxy_pref_cb,
		NULL);
	purple_prefs_connect_callback(handle, "/purple/proxy/host", proxy_pref_cb,
		NULL);
	purple_prefs_connect_callback(handle, "/purple/proxy/port", proxy_pref_cb,
		NULL);
	purple_prefs_connect_callback(handle, "/purple/proxy/username",
		proxy_pref_cb, NULL);
	purple_prefs_connect_callback(handle, "/purple/proxy/password",
		proxy_pref_cb, NULL);

	/* Load the initial proxy settings */
	purple_prefs_trigger_callback("/purple/proxy/type");
	purple_prefs_trigger_callback("/purple/proxy/host");
	purple_prefs_trigger_callback("/purple/proxy/port");
	purple_prefs_trigger_callback("/purple/proxy/username");
	purple_prefs_trigger_callback("/purple/proxy/password");
}

void
purple_proxy_uninit(void)
{
	while (handles != NULL)
	{
		purple_proxy_connect_data_disconnect(handles->data, NULL);
		purple_proxy_connect_data_destroy(handles->data);
	}

	purple_prefs_disconnect_by_handle(purple_proxy_get_handle());

	purple_proxy_info_destroy(global_proxy_info);
	global_proxy_info = NULL;
}
