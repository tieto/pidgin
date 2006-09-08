/**
 * @file proxy.c Proxy API
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
 *
 */

/* this is a little piece of code to handle proxy connection */
/* it is intended to : 1st handle http proxy, using the CONNECT command
 , 2nd provide an easy way to add socks support
 , 3rd draw women to it like flies to honey */

#include "internal.h"
#include "cipher.h"
#include "debug.h"
#include "dnsquery.h"
#include "notify.h"
#include "ntlm.h"
#include "prefs.h"
#include "proxy.h"
#include "util.h"

struct _GaimProxyConnectData {
	GaimProxyConnectFunction connect_cb;
	gpointer data;
	gchar *host;
	int port;
	int fd;
	guint inpa;
	GaimProxyInfo *gpi;
	GaimDnsQueryData *query_data;

	/**
	 * This contains alternating length/char* values.  The char*
	 * values need to be freed when removed from the linked list.
	 */
	GSList *hosts;

	/*
	 * All of the following variables are used when establishing a
	 * connection through a proxy.
	 */
	guchar *write_buffer;
	gsize write_buf_len;
	gsize written_len;
	GaimInputFunction read_cb;
	guchar *read_buffer;
	gsize read_buf_len;
	gsize read_len;
};

static const char *socks5errors[] = {
	"succeeded\n",
	"general SOCKS server failure\n",
	"connection not allowed by ruleset\n",
	"Network unreachable\n",
	"Host unreachable\n",
	"Connection refused\n",
	"TTL expired\n",
	"Command not supported\n",
	"Address type not supported\n"
};

static GaimProxyInfo *global_proxy_info = NULL;

/*
 * TODO: Once all callers of gaim_proxy_connect() are keeping track
 *       of the return value from that function this linked list
 *       will no longer be needed.
 */
static GSList *connect_datas = NULL;

static void try_connect(GaimProxyConnectData *connect_data);

/**************************************************************************
 * Proxy structure API
 **************************************************************************/
GaimProxyInfo *
gaim_proxy_info_new(void)
{
	return g_new0(GaimProxyInfo, 1);
}

void
gaim_proxy_info_destroy(GaimProxyInfo *info)
{
	g_return_if_fail(info != NULL);

	g_free(info->host);
	g_free(info->username);
	g_free(info->password);

	g_free(info);
}

void
gaim_proxy_info_set_type(GaimProxyInfo *info, GaimProxyType type)
{
	g_return_if_fail(info != NULL);

	info->type = type;
}

void
gaim_proxy_info_set_host(GaimProxyInfo *info, const char *host)
{
	g_return_if_fail(info != NULL);

	g_free(info->host);
	info->host = g_strdup(host);
}

void
gaim_proxy_info_set_port(GaimProxyInfo *info, int port)
{
	g_return_if_fail(info != NULL);

	info->port = port;
}

void
gaim_proxy_info_set_username(GaimProxyInfo *info, const char *username)
{
	g_return_if_fail(info != NULL);

	g_free(info->username);
	info->username = g_strdup(username);
}

void
gaim_proxy_info_set_password(GaimProxyInfo *info, const char *password)
{
	g_return_if_fail(info != NULL);

	g_free(info->password);
	info->password = g_strdup(password);
}

GaimProxyType
gaim_proxy_info_get_type(const GaimProxyInfo *info)
{
	g_return_val_if_fail(info != NULL, GAIM_PROXY_NONE);

	return info->type;
}

const char *
gaim_proxy_info_get_host(const GaimProxyInfo *info)
{
	g_return_val_if_fail(info != NULL, NULL);

	return info->host;
}

int
gaim_proxy_info_get_port(const GaimProxyInfo *info)
{
	g_return_val_if_fail(info != NULL, 0);

	return info->port;
}

const char *
gaim_proxy_info_get_username(const GaimProxyInfo *info)
{
	g_return_val_if_fail(info != NULL, NULL);

	return info->username;
}

const char *
gaim_proxy_info_get_password(const GaimProxyInfo *info)
{
	g_return_val_if_fail(info != NULL, NULL);

	return info->password;
}

/**************************************************************************
 * Global Proxy API
 **************************************************************************/
GaimProxyInfo *
gaim_global_proxy_get_info(void)
{
	return global_proxy_info;
}

static GaimProxyInfo *
gaim_gnome_proxy_get_info(void)
{
	static GaimProxyInfo info = {0, NULL, 0, NULL, NULL};
	gchar *path;
	if ((path = g_find_program_in_path("gconftool-2"))) {
		gchar *tmp;

		g_free(path);

		/* See whether to use a proxy. */
		if (!g_spawn_command_line_sync("gconftool-2 -g /system/http_proxy/use_http_proxy", &tmp,
					       NULL, NULL, NULL))
			return gaim_global_proxy_get_info();
		if (!strcmp(tmp, "false\n")) {
			info.type = GAIM_PROXY_NONE;
			g_free(tmp);
			return &info;
		} else if (strcmp(tmp, "true\n")) {
			g_free(tmp);
			return gaim_global_proxy_get_info();
		}

		g_free(tmp);
		info.type = GAIM_PROXY_HTTP;

		/* Free the old fields */
		if (info.host) {
			g_free(info.host);
			info.host = NULL;
		}
		if (info.username) {
			g_free(info.username);
			info.username = NULL;
		}
		if (info.password) {
			g_free(info.password);
			info.password = NULL;
		}

		/* Get the new ones */
		if (!g_spawn_command_line_sync("gconftool-2 -g /system/http_proxy/host", &info.host,
					       NULL, NULL, NULL))
			return gaim_global_proxy_get_info();
		g_strchomp(info.host);

		if (!g_spawn_command_line_sync("gconftool-2 -g /system/http_proxy/authentication_user", &info.username,
					       NULL, NULL, NULL))
			return gaim_global_proxy_get_info();
		g_strchomp(info.username);

		if (!g_spawn_command_line_sync("gconftool-2 -g /system/http_proxy/authentication_password", &info.password,
					       NULL, NULL, NULL))
			return gaim_global_proxy_get_info();
		g_strchomp(info.password);

		if (!g_spawn_command_line_sync("gconftool-2 -g /system/http_proxy/port", &tmp,
					       NULL, NULL, NULL))
			return gaim_global_proxy_get_info();
		info.port = atoi(tmp);
		g_free(tmp);

		return &info;
	}
	return gaim_global_proxy_get_info();
}
/**************************************************************************
 * Proxy API
 **************************************************************************/

/**
 * Whoever calls this needs to have called
 * gaim_proxy_connect_data_disconnect() beforehand.
 */
static void
gaim_proxy_connect_data_destroy(GaimProxyConnectData *connect_data)
{
	connect_datas = g_slist_remove(connect_datas, connect_data);

	if (connect_data->query_data != NULL)
		gaim_dnsquery_destroy(connect_data->query_data);

	while (connect_data->hosts != NULL)
	{
		/* Discard the length... */
		connect_data->hosts = g_slist_remove(connect_data->hosts, connect_data->hosts->data);
		/* Free the address... */
		g_free(connect_data->hosts->data);
		connect_data->hosts = g_slist_remove(connect_data->hosts, connect_data->hosts->data);
	}

	g_free(connect_data->host);
	g_free(connect_data);
}

/**
 * Free all information dealing with a connection attempt and
 * reset the connect_data to prepare for it to try to connect
 * to another IP address.
 *
 * If an error message is passed in, then we know the connection
 * attempt failed.  If the connection attempt failed and
 * connect_data->hosts is not empty then we try the next IP address.
 * If the connection attempt failed and we have no more hosts
 * try try then we call the callback with the given error message,
 * then destroy the connect_data.
 *
 * @param error_message An error message explaining why the connection
 *        failed.  This will be passed to the callback function
 *        specified in the call to gaim_proxy_connect().  If the
 *        connection was successful then pass in null.
 */
static void
gaim_proxy_connect_data_disconnect(GaimProxyConnectData *connect_data, const gchar *error_message)
{
	if (connect_data->inpa > 0)
	{
		gaim_input_remove(connect_data->inpa);
		connect_data->inpa = 0;
	}

	if (connect_data->fd >= 0)
	{
		close(connect_data->fd);
		connect_data->fd = -1;
	}

	g_free(connect_data->write_buffer);
	connect_data->write_buffer = NULL;

	g_free(connect_data->read_buffer);
	connect_data->read_buffer = NULL;

	if (error_message != NULL)
	{
		gaim_debug_info("proxy", "Connection attempt failed: %s\n",
				error_message);
		if (connect_data->hosts != NULL)
			try_connect(connect_data);
		else
		{
			/* Everything failed!  Tell the originator of the request. */
			connect_data->connect_cb(connect_data->data, -1, error_message);
			gaim_proxy_connect_data_destroy(connect_data);
		}
	}
}

/**
 * This calls gaim_proxy_connect_data_disconnect(), but it lets you
 * specify the error_message using a printf()-like syntax.
 */
static void
gaim_proxy_connect_data_disconnect_formatted(GaimProxyConnectData *connect_data, const char *format, ...)
{
	va_list args;
	gchar *tmp;

	va_start(args, format);
	tmp = g_strdup_vprintf(format, args);
	va_end(args);

	gaim_proxy_connect_data_disconnect(connect_data, tmp);
	g_free(tmp);
}

static void
gaim_proxy_connect_data_connected(GaimProxyConnectData *connect_data)
{
	connect_data->connect_cb(connect_data->data, connect_data->fd, NULL);

	/*
	 * We've passed the file descriptor to the protocol, so it's no longer
	 * our responsibility, and we should be careful not to free it when
	 * we destroy the connect_data.
	 */
	connect_data->fd = -1;

	gaim_proxy_connect_data_disconnect(connect_data, NULL);
	gaim_proxy_connect_data_destroy(connect_data);
}

static void
socket_ready_cb(gpointer data, gint source, GaimInputCondition cond)
{
	GaimProxyConnectData *connect_data = data;
	socklen_t len;
	int error = 0;
	int ret;

	gaim_debug_info("proxy", "Connected.\n");

	/*
	 * getsockopt after a non-blocking connect returns -1 if something is
	 * really messed up (bad descriptor, usually). Otherwise, it returns 0 and
	 * error holds what connect would have returned if it blocked until now.
	 * Thus, error == 0 is success, error == EINPROGRESS means "try again",
	 * and anything else is a real error.
	 *
	 * (error == EINPROGRESS can happen after a select because the kernel can
	 * be overly optimistic sometimes. select is just a hint that you might be
	 * able to do something.)
	 */
	len = sizeof(error);
	ret = getsockopt(connect_data->fd, SOL_SOCKET, SO_ERROR, &error, &len);

	if (ret == 0 && error == EINPROGRESS)
		/* No worries - we'll be called again later */
		/* TODO: Does this ever happen? */
		return;

	if (ret != 0 || error != 0) {
		if (ret != 0)
			error = errno;
		gaim_proxy_connect_data_disconnect(connect_data, strerror(error));
		return;
	}

	gaim_proxy_connect_data_connected(connect_data);
}

static gboolean
clean_connect(gpointer data)
{
	gaim_proxy_connect_data_connected(data);

	return FALSE;
}

static void
proxy_connect_none(GaimProxyConnectData *connect_data, struct sockaddr *addr, socklen_t addrlen)
{
	gaim_debug_info("proxy", "Connecting to %s:%d with no proxy\n",
			connect_data->host, connect_data->port);

	connect_data->fd = socket(addr->sa_family, SOCK_STREAM, 0);
	if (connect_data->fd < 0)
	{
		gaim_proxy_connect_data_disconnect_formatted(connect_data,
				_("Unable to create socket:\n%s"), strerror(errno));
		return;
	}

	fcntl(connect_data->fd, F_SETFL, O_NONBLOCK);
#ifndef _WIN32
	fcntl(connect_data->fd, F_SETFD, FD_CLOEXEC);
#endif

	if (connect(connect_data->fd, addr, addrlen) != 0)
	{
		if ((errno == EINPROGRESS) || (errno == EINTR))
		{
			gaim_debug_info("proxy", "Connection in progress\n");
			connect_data->inpa = gaim_input_add(connect_data->fd,
					GAIM_INPUT_WRITE, socket_ready_cb, connect_data);
		}
		else
		{
			gaim_proxy_connect_data_disconnect(connect_data, strerror(errno));
		}
	}
	else
	{
		/*
		 * The connection happened IMMEDIATELY... strange, but whatever.
		 */
		socklen_t len;
		int error = ETIMEDOUT;
		int ret;

		gaim_debug_info("proxy", "Connected immediately.\n");

		len = sizeof(error);
		ret = getsockopt(connect_data->fd, SOL_SOCKET, SO_ERROR, &error, &len);
		if ((ret != 0) || (error != 0))
		{
			if (ret != 0)
				error = errno;
			gaim_proxy_connect_data_disconnect(connect_data, strerror(error));
			return;
		}

		/*
		 * We want to call the "connected" callback eventually, but we
		 * don't want to call it before we return, just in case.
		 */
		gaim_timeout_add(10, clean_connect, connect_data);
	}
}

/**
 * This is a utility function used by the HTTP, SOCKS4 and SOCKS5
 * connect functions.  It writes data from a buffer to a socket.
 * When all the data is written it sets up a watcher to read a
 * response and call a specified function.
 */
static void
proxy_do_write(gpointer data, gint source, GaimInputCondition cond)
{
	GaimProxyConnectData *connect_data;
	const guchar *request;
	gsize request_len;
	int ret;

	connect_data = data;
	request = connect_data->write_buffer + connect_data->written_len;
	request_len = connect_data->write_buf_len - connect_data->written_len;

	ret = write(connect_data->fd, request, request_len);
	if (ret <= 0)
	{
		if (errno == EAGAIN)
			/* No worries */
			return;

		/* Error! */
		gaim_proxy_connect_data_disconnect(connect_data, strerror(errno));
		return;
	}
	if (ret < request_len) {
		connect_data->written_len += ret;
		return;
	}

	/* We're done writing data!  Wait for a response. */
	g_free(connect_data->write_buffer);
	connect_data->write_buffer = NULL;
	gaim_input_remove(connect_data->inpa);
	connect_data->inpa = gaim_input_add(connect_data->fd,
			GAIM_INPUT_READ, connect_data->read_cb, connect_data);
}

#define HTTP_GOODSTRING "HTTP/1.0 200"
#define HTTP_GOODSTRING2 "HTTP/1.1 200"

/**
 * We're using an HTTP proxy for a non-port 80 tunnel.  Read the
 * response to the CONNECT request.
 */
static void
http_canread(gpointer data, gint source, GaimInputCondition cond)
{
	int len, headers_len, status = 0;
	gboolean error;
	GaimProxyConnectData *connect_data = data;
	guchar *p;
	gsize max_read;

	if (connect_data->read_buffer == NULL)
	{
		connect_data->read_buf_len = 8192;
		connect_data->read_buffer = g_malloc(connect_data->read_buf_len);
		connect_data->read_len = 0;
	}

	p = connect_data->read_buffer + connect_data->read_len;
	max_read = connect_data->read_buf_len - connect_data->read_len - 1;

	len = read(connect_data->fd, p, max_read);

	if (len == 0)
	{
		gaim_proxy_connect_data_disconnect(connect_data,
				_("Server closed the connection."));
		return;
	}

	if (len < 0)
	{
		if (errno == EAGAIN)
			/* No worries */
			return;

		/* Error! */
		gaim_proxy_connect_data_disconnect_formatted(connect_data,
				_("Lost connection with server:\n%s"), strerror(errno));
		return;
	}

	connect_data->read_len += len;
	p[len] = '\0';

	p = (guchar *)g_strstr_len((const gchar *)connect_data->read_buffer,
			connect_data->read_len, "\r\n\r\n");
	if (p != NULL) {
		*p = '\0';
		headers_len = (p - connect_data->read_buffer) + 4;
	} else if(len == max_read)
		headers_len = len;
	else
		return;

	error = strncmp((const char *)connect_data->read_buffer, "HTTP/", 5) != 0;
	if (!error)
	{
		int major;
		p = connect_data->read_buffer + 5;
		major = strtol((const char *)p, (char **)&p, 10);
		error = (major == 0) || (*p != '.');
		if(!error) {
			int minor;
			p++;
			minor = strtol((const char *)p, (char **)&p, 10);
			error = (*p != ' ');
			if(!error) {
				p++;
				status = strtol((const char *)p, (char **)&p, 10);
				error = (*p != ' ');
			}
		}
	}

	/* Read the contents */
	p = (guchar *)g_strrstr((const gchar *)connect_data->read_buffer, "Content-Length: ");
	if (p != NULL)
	{
		gchar *tmp;
		int len = 0;
		char tmpc;
		p += strlen("Content-Length: ");
		tmp = strchr((const char *)p, '\r');
		if(tmp)
			*tmp = '\0';
		len = atoi((const char *)p);
		if(tmp)
			*tmp = '\r';

		/* Compensate for what has already been read */
		len -= connect_data->read_len - headers_len;
		/* I'm assuming that we're doing this to prevent the server from
		   complaining / breaking since we don't read the whole page */
		while (len--) {
			/* TODO: deal with EAGAIN (and other errors) better */
			if (read(connect_data->fd, &tmpc, 1) < 0 && errno != EAGAIN)
				break;
		}
	}

	if (error)
	{
		gaim_proxy_connect_data_disconnect_formatted(connect_data,
				_("Unable to parse response from HTTP proxy: %s\n"),
				connect_data->read_buffer);
		return;
	}
	else if (status != 200)
	{
		gaim_debug_error("proxy",
				"Proxy server replied with:\n%s\n",
				connect_data->read_buffer);

		if (status == 407 /* Proxy Auth */)
		{
			gchar *ntlm;
			ntlm = g_strrstr((const gchar *)connect_data->read_buffer,
					"Proxy-Authenticate: NTLM ");
			if (ntlm != NULL)
			{
				/* Check for Type-2 */
				gchar *tmp = ntlm;
				guint8 *nonce;
				gchar *domain = (gchar*)gaim_proxy_info_get_username(connect_data->gpi);
				gchar *username;
				gchar *request;
				gchar *response;
				username = strchr(domain, '\\');
				if (username == NULL)
				{
					gaim_proxy_connect_data_disconnect_formatted(connect_data,
							_("HTTP proxy connection error %d"), status);
					return;
				}
				*username = '\0';
				username++;
				ntlm += strlen("Proxy-Authenticate: NTLM ");
				while(*tmp != '\r' && *tmp != '\0') tmp++;
				*tmp = '\0';
				nonce = gaim_ntlm_parse_type2(ntlm, NULL);
				response = gaim_ntlm_gen_type3(username,
					(gchar*) gaim_proxy_info_get_password(connect_data->gpi),
					(gchar*) gaim_proxy_info_get_host(connect_data->gpi),
					domain, nonce, NULL);
				username--;
				*username = '\\';
				request = g_strdup_printf(
					"CONNECT %s:%d HTTP/1.1\r\n"
					"Host: %s:%d\r\n"
					"Proxy-Authorization: NTLM %s\r\n"
					"Proxy-Connection: Keep-Alive\r\n\r\n",
					connect_data->host, connect_data->port, connect_data->host,
					connect_data->port, response);
				g_free(response);

				g_free(connect_data->read_buffer);
				connect_data->read_buffer = NULL;

				connect_data->write_buffer = (guchar *)request;
				connect_data->write_buf_len = strlen(request);
				connect_data->written_len = 0;
				connect_data->read_cb = http_canread;

				gaim_input_remove(connect_data->inpa);
				connect_data->inpa = gaim_input_add(connect_data->fd,
					GAIM_INPUT_WRITE, proxy_do_write, connect_data);
				proxy_do_write(connect_data, connect_data->fd, cond);
				return;
			} else if((ntlm = g_strrstr((const char *)connect_data->read_buffer, "Proxy-Authenticate: NTLM"))) { /* Empty message */
				gchar request[2048];
				gchar *domain = (gchar*) gaim_proxy_info_get_username(connect_data->gpi);
				gchar *username;
				int request_len;
				username = strchr(domain, '\\');
				if (username == NULL)
				{
					gaim_proxy_connect_data_disconnect_formatted(connect_data,
							_("HTTP proxy connection error %d"), status);
					return;
				}
				*username = '\0';

				request_len = g_snprintf(request, sizeof(request),
						"CONNECT %s:%d HTTP/1.1\r\n"
						"Host: %s:%d\r\n",
						connect_data->host, connect_data->port,
						connect_data->host, connect_data->port);

				g_return_if_fail(request_len < sizeof(request));
				request_len += g_snprintf(request + request_len,
					sizeof(request) - request_len,
					"Proxy-Authorization: NTLM %s\r\n"
					"Proxy-Connection: Keep-Alive\r\n\r\n",
					gaim_ntlm_gen_type1(
						(gchar*) gaim_proxy_info_get_host(connect_data->gpi),
						domain));
				*username = '\\';

				gaim_input_remove(connect_data->inpa);
				g_free(connect_data->read_buffer);
				connect_data->read_buffer = NULL;

				connect_data->write_buffer = g_memdup(request, request_len);
				connect_data->write_buf_len = request_len;
				connect_data->written_len = 0;

				connect_data->read_cb = http_canread;

				connect_data->inpa = gaim_input_add(connect_data->fd,
					GAIM_INPUT_WRITE, proxy_do_write, connect_data);

				proxy_do_write(connect_data, connect_data->fd, cond);
				return;
			} else {
				gaim_proxy_connect_data_disconnect_formatted(connect_data,
						_("HTTP proxy connection error %d"), status);
				return;
			}
		}
		if (status == 403)
		{
			/* Forbidden */
			gaim_proxy_connect_data_disconnect_formatted(connect_data,
					_("Access denied: HTTP proxy server forbids port %d tunneling."),
					connect_data->port);
		} else {
			gaim_proxy_connect_data_disconnect_formatted(connect_data,
					_("HTTP proxy connection error %d"), status);
		}
	} else {
		gaim_input_remove(connect_data->inpa);
		connect_data->inpa = 0;
		g_free(connect_data->read_buffer);
		connect_data->read_buffer = NULL;
		gaim_debug_info("proxy", "HTTP proxy connection established\n");
		gaim_proxy_connect_data_connected(connect_data);
		return;
	}
}

static void
http_canwrite(gpointer data, gint source, GaimInputCondition cond)
{
	GString *request;
	GaimProxyConnectData *connect_data;
	socklen_t len;
	int error = ETIMEDOUT;
	int ret;

	gaim_debug_info("proxy", "Connected.\n");

	connect_data = data;

	if (connect_data->inpa > 0)
	{
		gaim_input_remove(connect_data->inpa);
		connect_data->inpa = 0;
	}

	len = sizeof(error);
	ret = getsockopt(connect_data->fd, SOL_SOCKET, SO_ERROR, &error, &len);
	if ((ret != 0) || (error != 0))
	{
		if (ret != 0)
			error = errno;
		gaim_proxy_connect_data_disconnect(connect_data, strerror(error));
		return;
	}

	gaim_debug_info("proxy", "Using CONNECT tunneling for %s:%d\n",
		connect_data->host, connect_data->port);

	request = g_string_sized_new(4096);
	g_string_append_printf(request,
			"CONNECT %s:%d HTTP/1.1\r\nHost: %s:%d\r\n",
			connect_data->host, connect_data->port,
			connect_data->host, connect_data->port);

	if (gaim_proxy_info_get_username(connect_data->gpi) != NULL)
	{
		char *t1, *t2;

		t1 = g_strdup_printf("%s:%s",
			gaim_proxy_info_get_username(connect_data->gpi),
			gaim_proxy_info_get_password(connect_data->gpi) ?
				gaim_proxy_info_get_password(connect_data->gpi) : "");
		t2 = gaim_base64_encode((const guchar *)t1, strlen(t1));
		g_free(t1);

		g_string_append_printf(request,
			"Proxy-Authorization: Basic %s\r\n"
			"Proxy-Authorization: NTLM %s\r\n"
			"Proxy-Connection: Keep-Alive\r\n",
			t2, gaim_ntlm_gen_type1(
					gaim_proxy_info_get_host(connect_data->gpi), ""));
		g_free(t2);
	}

	g_string_append(request, "\r\n");

	connect_data->write_buf_len = request->len;
	connect_data->write_buffer = (guchar *)g_string_free(request, FALSE);
	connect_data->written_len = 0;
	connect_data->read_cb = http_canread;

	connect_data->inpa = gaim_input_add(connect_data->fd,
			GAIM_INPUT_WRITE, proxy_do_write, connect_data);
	proxy_do_write(connect_data, connect_data->fd, cond);
}

static void
proxy_connect_http(GaimProxyConnectData *connect_data, struct sockaddr *addr, socklen_t addrlen)
{
	gaim_debug_info("proxy",
			   "Connecting to %s:%d via %s:%d using HTTP\n",
			   connect_data->host, connect_data->port,
			   (gaim_proxy_info_get_host(connect_data->gpi) ? gaim_proxy_info_get_host(connect_data->gpi) : "(null)"),
			   gaim_proxy_info_get_port(connect_data->gpi));

	connect_data->fd = socket(addr->sa_family, SOCK_STREAM, 0);
	if (connect_data->fd < 0)
	{
		gaim_proxy_connect_data_disconnect_formatted(connect_data,
				_("Unable to create socket:\n%s"), strerror(errno));
		return;
	}

	fcntl(connect_data->fd, F_SETFL, O_NONBLOCK);
#ifndef _WIN32
	fcntl(connect_data->fd, F_SETFD, FD_CLOEXEC);
#endif

	if (connect(connect_data->fd, addr, addrlen) != 0)
	{
		if ((errno == EINPROGRESS) || (errno == EINTR)) {
			gaim_debug_info("proxy", "Connection in progress\n");

			if (connect_data->port != 80)
			{
				/* we need to do CONNECT first */
				connect_data->inpa = gaim_input_add(connect_data->fd,
						GAIM_INPUT_WRITE, http_canwrite, connect_data);
			}
			else
			{
				/*
				 * If we're trying to connect to something running on
				 * port 80 then we assume the traffic using this
				 * connection is going to be HTTP traffic.  If it's
				 * not then this will fail (uglily).  But it's good
				 * to avoid using the CONNECT method because it's
				 * not always allowed.
				 */
				gaim_debug_info("proxy", "HTTP proxy connection established\n");
				gaim_proxy_connect_data_connected(connect_data);
			}
		}
		else
		{
			gaim_proxy_connect_data_disconnect(connect_data, strerror(errno));
		}
	}
	else
	{
		gaim_debug_info("proxy", "Connected immediately.\n");

		http_canwrite(connect_data, connect_data->fd, GAIM_INPUT_WRITE);
	}
}

static void
s4_canread(gpointer data, gint source, GaimInputCondition cond)
{
	GaimProxyConnectData *connect_data = data;
	guchar *buf;
	int len, max_read;

	/* This is really not going to block under normal circumstances, but to
	 * be correct, we deal with the unlikely scenario */

	if (connect_data->read_buffer == NULL) {
		connect_data->read_buf_len = 12;
		connect_data->read_buffer = g_malloc(connect_data->read_buf_len);
		connect_data->read_len = 0;
	}

	buf = connect_data->read_buffer + connect_data->read_len;
	max_read = connect_data->read_buf_len - connect_data->read_len;

	len = read(connect_data->fd, buf, max_read);

	if ((len < 0 && errno == EAGAIN) || (len > 0 && len + connect_data->read_len < 4))
		return;
	else if (len + connect_data->read_len >= 4) {
		if (connect_data->read_buffer[1] == 90) {
			gaim_proxy_connect_data_connected(connect_data);
			return;
		}
	}

	gaim_proxy_connect_data_disconnect(connect_data, strerror(errno));
}

static void
s4_canwrite(gpointer data, gint source, GaimInputCondition cond)
{
	unsigned char packet[9];
	struct hostent *hp;
	GaimProxyConnectData *connect_data = data;
	socklen_t len;
	int error = ETIMEDOUT;
	int ret;

	gaim_debug_info("socks4 proxy", "Connected.\n");

	if (connect_data->inpa > 0)
	{
		gaim_input_remove(connect_data->inpa);
		connect_data->inpa = 0;
	}

	len = sizeof(error);
	ret = getsockopt(connect_data->fd, SOL_SOCKET, SO_ERROR, &error, &len);
	if ((ret != 0) || (error != 0))
	{
		if (ret != 0)
			error = errno;
		gaim_proxy_connect_data_disconnect(connect_data, strerror(error));
		return;
	}

	/*
	 * The socks4 spec doesn't include support for doing host name
	 * lookups by the proxy.  Some socks4 servers do this via
	 * extensions to the protocol.  Since we don't know if a
	 * server supports this, it would need to be implemented
	 * with an option, or some detection mechanism - in the
	 * meantime, stick with plain old SOCKS4.
	 */
	/* TODO: Use gaim_dnsquery_a() */
	hp = gethostbyname(connect_data->host);
	if (hp == NULL) {
		gaim_proxy_connect_data_disconnect_formatted(connect_data,
				_("Error resolving %s"), connect_data->host);
		return;
	}

	packet[0] = 4;
	packet[1] = 1;
	packet[2] = connect_data->port >> 8;
	packet[3] = connect_data->port & 0xff;
	packet[4] = (unsigned char)(hp->h_addr_list[0])[0];
	packet[5] = (unsigned char)(hp->h_addr_list[0])[1];
	packet[6] = (unsigned char)(hp->h_addr_list[0])[2];
	packet[7] = (unsigned char)(hp->h_addr_list[0])[3];
	packet[8] = 0;

	connect_data->write_buffer = g_memdup(packet, sizeof(packet));
	connect_data->write_buf_len = sizeof(packet);
	connect_data->written_len = 0;
	connect_data->read_cb = s4_canread;

	connect_data->inpa = gaim_input_add(connect_data->fd, GAIM_INPUT_WRITE, proxy_do_write, connect_data);

	proxy_do_write(connect_data, connect_data->fd, cond);
}

static void
proxy_connect_socks4(GaimProxyConnectData *connect_data, struct sockaddr *addr, socklen_t addrlen)
{
	gaim_debug_info("proxy",
			   "Connecting to %s:%d via %s:%d using SOCKS4\n",
			   connect_data->host, connect_data->port,
			   gaim_proxy_info_get_host(connect_data->gpi),
			   gaim_proxy_info_get_port(connect_data->gpi));

	connect_data->fd = socket(addr->sa_family, SOCK_STREAM, 0);
	if (connect_data->fd < 0)
	{
		gaim_proxy_connect_data_disconnect_formatted(connect_data,
				_("Unable to create socket:\n%s"), strerror(errno));
		return;
	}

	fcntl(connect_data->fd, F_SETFL, O_NONBLOCK);
#ifndef _WIN32
	fcntl(connect_data->fd, F_SETFD, FD_CLOEXEC);
#endif

	if (connect(connect_data->fd, addr, addrlen) != 0)
	{
		if ((errno == EINPROGRESS) || (errno == EINTR))
		{
			gaim_debug_info("proxy", "Connection in progress.\n");
			connect_data->inpa = gaim_input_add(connect_data->fd,
					GAIM_INPUT_WRITE, s4_canwrite, connect_data);
		}
		else
		{
			gaim_proxy_connect_data_disconnect(connect_data, strerror(errno));
		}
	}
	else
	{
		gaim_debug_info("proxy", "Connected immediately.\n");

		s4_canwrite(connect_data, connect_data->fd, GAIM_INPUT_WRITE);
	}
}

static void
s5_canread_again(gpointer data, gint source, GaimInputCondition cond)
{
	guchar *dest, *buf;
	GaimProxyConnectData *connect_data = data;
	int len;

	if (connect_data->read_buffer == NULL) {
		connect_data->read_buf_len = 512;
		connect_data->read_buffer = g_malloc(connect_data->read_buf_len);
		connect_data->read_len = 0;
	}

	dest = connect_data->read_buffer + connect_data->read_len;
	buf = connect_data->read_buffer;

	gaim_debug_info("socks5 proxy", "Able to read again.\n");

	len = read(connect_data->fd, dest, (connect_data->read_buf_len - connect_data->read_len));

	if (len == 0)
	{
		gaim_proxy_connect_data_disconnect(connect_data,
				_("Server closed the connection."));
		return;
	}

	if (len < 0)
	{
		if (errno == EAGAIN)
			/* No worries */
			return;

		/* Error! */
		gaim_proxy_connect_data_disconnect_formatted(connect_data,
				_("Lost connection with server:\n%s"), strerror(errno));
		return;
	}

	connect_data->read_len += len;

	if(connect_data->read_len < 4)
		return;

	if ((buf[0] != 0x05) || (buf[1] != 0x00)) {
		if ((buf[0] == 0x05) && (buf[1] < 0x09)) {
			gaim_debug_error("socks5 proxy", socks5errors[buf[1]]);
			gaim_proxy_connect_data_disconnect(connect_data,
					socks5errors[buf[1]]);
		} else {
			gaim_debug_error("socks5 proxy", "Bad data.\n");
			gaim_proxy_connect_data_disconnect(connect_data,
					_("Received invalid data on connection with server."));
		}
		return;
	}

	/* Skip past BND.ADDR */
	switch(buf[3]) {
		case 0x01: /* the address is a version-4 IP address, with a length of 4 octets */
			if(connect_data->read_len < 4 + 4)
				return;
			buf += 4 + 4;
			break;
		case 0x03: /* the address field contains a fully-qualified domain name.  The first
					  octet of the address field contains the number of octets of name that
					  follow, there is no terminating NUL octet. */
			if(connect_data->read_len < 4 + 1)
				return;
			buf += 4 + 1;
			if(connect_data->read_len < 4 + 1 + buf[0])
				return;
			buf += buf[0];
			break;
		case 0x04: /* the address is a version-6 IP address, with a length of 16 octets */
			if(connect_data->read_len < 4 + 16)
				return;
			buf += 4 + 16;
			break;
	}

	if(connect_data->read_len < (buf - connect_data->read_buffer) + 2)
		return;

	/* Skip past BND.PORT */
	buf += 2;

	gaim_proxy_connect_data_connected(connect_data);
}

static void
s5_sendconnect(gpointer data, int source)
{
	GaimProxyConnectData *connect_data = data;
	int hlen = strlen(connect_data->host);
	connect_data->write_buf_len = 5 + hlen + 2;
	connect_data->write_buffer = g_malloc(connect_data->write_buf_len);
	connect_data->written_len = 0;

	connect_data->write_buffer[0] = 0x05;
	connect_data->write_buffer[1] = 0x01;		/* CONNECT */
	connect_data->write_buffer[2] = 0x00;		/* reserved */
	connect_data->write_buffer[3] = 0x03;		/* address type -- host name */
	connect_data->write_buffer[4] = hlen;
	memcpy(connect_data->write_buffer + 5, connect_data->host, hlen);
	connect_data->write_buffer[5 + hlen] = connect_data->port >> 8;
	connect_data->write_buffer[5 + hlen + 1] = connect_data->port & 0xff;

	connect_data->read_cb = s5_canread_again;

	connect_data->inpa = gaim_input_add(connect_data->fd, GAIM_INPUT_WRITE, proxy_do_write, connect_data);
	proxy_do_write(connect_data, connect_data->fd, GAIM_INPUT_WRITE);
}

static void
s5_readauth(gpointer data, gint source, GaimInputCondition cond)
{
	GaimProxyConnectData *connect_data = data;
	int len;

	if (connect_data->read_buffer == NULL) {
		connect_data->read_buf_len = 2;
		connect_data->read_buffer = g_malloc(connect_data->read_buf_len);
		connect_data->read_len = 0;
	}

	gaim_debug_info("socks5 proxy", "Got auth response.\n");

	len = read(connect_data->fd, connect_data->read_buffer + connect_data->read_len,
		connect_data->read_buf_len - connect_data->read_len);

	if (len == 0)
	{
		gaim_proxy_connect_data_disconnect(connect_data,
				_("Server closed the connection."));
		return;
	}

	if (len < 0)
	{
		if (errno == EAGAIN)
			/* No worries */
			return;

		/* Error! */
		gaim_proxy_connect_data_disconnect_formatted(connect_data,
				_("Lost connection with server:\n%s"), strerror(errno));
		return;
	}

	connect_data->read_len += len;
	if (connect_data->read_len < 2)
		return;

	gaim_input_remove(connect_data->inpa);
	connect_data->inpa = 0;

	if ((connect_data->read_buffer[0] != 0x01) || (connect_data->read_buffer[1] != 0x00)) {
		gaim_proxy_connect_data_disconnect(connect_data,
				_("Received invalid data on connection with server."));
		return;
	}

	g_free(connect_data->read_buffer);
	connect_data->read_buffer = NULL;

	s5_sendconnect(connect_data, connect_data->fd);
}

static void
hmacmd5_chap(const unsigned char * challenge, int challen, const char * passwd, unsigned char * response)
{
	GaimCipher *cipher;
	GaimCipherContext *ctx;
	int i;
	unsigned char Kxoripad[65];
	unsigned char Kxoropad[65];
	int pwlen;

	cipher = gaim_ciphers_find_cipher("md5");
	ctx = gaim_cipher_context_new(cipher, NULL);

	memset(Kxoripad,0,sizeof(Kxoripad));
	memset(Kxoropad,0,sizeof(Kxoropad));

	pwlen=strlen(passwd);
	if (pwlen>64) {
		gaim_cipher_context_append(ctx, (const guchar *)passwd, strlen(passwd));
		gaim_cipher_context_digest(ctx, sizeof(Kxoripad), Kxoripad, NULL);
		pwlen=16;
	} else {
		memcpy(Kxoripad, passwd, pwlen);
	}
	memcpy(Kxoropad,Kxoripad,pwlen);

	for (i=0;i<64;i++) {
		Kxoripad[i]^=0x36;
		Kxoropad[i]^=0x5c;
	}

	gaim_cipher_context_reset(ctx, NULL);
	gaim_cipher_context_append(ctx, Kxoripad, 64);
	gaim_cipher_context_append(ctx, challenge, challen);
	gaim_cipher_context_digest(ctx, sizeof(Kxoripad), Kxoripad, NULL);

	gaim_cipher_context_reset(ctx, NULL);
	gaim_cipher_context_append(ctx, Kxoropad, 64);
	gaim_cipher_context_append(ctx, Kxoripad, 16);
	gaim_cipher_context_digest(ctx, 16, response, NULL);

	gaim_cipher_context_destroy(ctx);
}

static void
s5_readchap(gpointer data, gint source, GaimInputCondition cond)
{
	guchar *cmdbuf, *buf;
	GaimProxyConnectData *connect_data = data;
	int len, navas, currentav;

	gaim_debug(GAIM_DEBUG_INFO, "socks5 proxy", "Got CHAP response.\n");

	if (connect_data->read_buffer == NULL) {
		connect_data->read_buf_len = 20;
		connect_data->read_buffer = g_malloc(connect_data->read_buf_len);
		connect_data->read_len = 0;
	}

	len = read(connect_data->fd, connect_data->read_buffer + connect_data->read_len,
		connect_data->read_buf_len - connect_data->read_len);

	if (len == 0)
	{
		gaim_proxy_connect_data_disconnect(connect_data,
				_("Server closed the connection."));
		return;
	}

	if (len < 0)
	{
		if (errno == EAGAIN)
			/* No worries */
			return;

		/* Error! */
		gaim_proxy_connect_data_disconnect_formatted(connect_data,
				_("Lost connection with server:\n%s"), strerror(errno));
		return;
	}

	connect_data->read_len += len;
	if (connect_data->read_len < 2)
		return;

	cmdbuf = connect_data->read_buffer;

	if (*cmdbuf != 0x01) {
		gaim_proxy_connect_data_disconnect(connect_data,
				_("Received invalid data on connection with server."));
		return;
	}
	cmdbuf++;

	navas = *cmdbuf;
	cmdbuf++;

	for (currentav = 0; currentav < navas; currentav++) {
		if (connect_data->read_len - (cmdbuf - connect_data->read_buffer) < 2)
			return;
		if (connect_data->read_len - (cmdbuf - connect_data->read_buffer) < cmdbuf[1])
			return;
		buf = cmdbuf + 2;
		switch (cmdbuf[0]) {
			case 0x00:
				/* Did auth work? */
				if (buf[0] == 0x00) {
					gaim_input_remove(connect_data->inpa);
					connect_data->inpa = 0;
					g_free(connect_data->read_buffer);
					connect_data->read_buffer = NULL;
					/* Success */
					s5_sendconnect(connect_data, connect_data->fd);
					return;
				} else {
					/* Failure */
					gaim_debug_warning("proxy",
						"socks5 CHAP authentication "
						"failed.  Disconnecting...");
					gaim_proxy_connect_data_disconnect(connect_data,
							_("Authentication failed"));
					return;
				}
				break;
			case 0x03:
				/* Server wants our credentials */

				connect_data->write_buf_len = 16 + 4;
				connect_data->write_buffer = g_malloc(connect_data->write_buf_len);
				connect_data->written_len = 0;

				hmacmd5_chap(buf, cmdbuf[1],
					gaim_proxy_info_get_password(connect_data->gpi),
					connect_data->write_buffer + 4);
				connect_data->write_buffer[0] = 0x01;
				connect_data->write_buffer[1] = 0x01;
				connect_data->write_buffer[2] = 0x04;
				connect_data->write_buffer[3] = 0x10;

				gaim_input_remove(connect_data->inpa);
				g_free(connect_data->read_buffer);
				connect_data->read_buffer = NULL;

				connect_data->read_cb = s5_readchap;

				connect_data->inpa = gaim_input_add(connect_data->fd,
					GAIM_INPUT_WRITE, proxy_do_write, connect_data);

				proxy_do_write(connect_data, connect_data->fd, GAIM_INPUT_WRITE);
				break;
			case 0x11:
				/* Server wants to select an algorithm */
				if (buf[0] != 0x85) {
					/* Only currently support HMAC-MD5 */
					gaim_debug_warning("proxy",
						"Server tried to select an "
						"algorithm that we did not advertise "
						"as supporting.  This is a violation "
						"of the socks5 CHAP specification.  "
						"Disconnecting...");
					gaim_proxy_connect_data_disconnect(connect_data,
							_("Received invalid data on connection with server."));
					return;
				}
				break;
		}
		cmdbuf = buf + cmdbuf[1];
	}
	/* Fell through.  We ran out of CHAP events to process, but haven't
	 * succeeded or failed authentication - there may be more to come.
	 * If this is the case, come straight back here. */
}

static void
s5_canread(gpointer data, gint source, GaimInputCondition cond)
{
	GaimProxyConnectData *connect_data = data;
	int len;

	if (connect_data->read_buffer == NULL) {
		connect_data->read_buf_len = 2;
		connect_data->read_buffer = g_malloc(connect_data->read_buf_len);
		connect_data->read_len = 0;
	}

	gaim_debug_info("socks5 proxy", "Able to read.\n");

	len = read(connect_data->fd, connect_data->read_buffer + connect_data->read_len,
		connect_data->read_buf_len - connect_data->read_len);

	if (len == 0)
	{
		gaim_proxy_connect_data_disconnect(connect_data,
				_("Server closed the connection."));
		return;
	}

	if (len < 0)
	{
		if (errno == EAGAIN)
			/* No worries */
			return;

		/* Error! */
		gaim_proxy_connect_data_disconnect_formatted(connect_data,
				_("Lost connection with server:\n%s"), strerror(errno));
		return;
	}

	connect_data->read_len += len;
	if (connect_data->read_len < 2)
		return;

	gaim_input_remove(connect_data->inpa);
	connect_data->inpa = 0;

	if ((connect_data->read_buffer[0] != 0x05) || (connect_data->read_buffer[1] == 0xff)) {
		gaim_proxy_connect_data_disconnect(connect_data,
				_("Received invalid data on connection with server."));
		return;
	}

	if (connect_data->read_buffer[1] == 0x02) {
		gsize i, j;
		const char *u, *p;

		u = gaim_proxy_info_get_username(connect_data->gpi);
		p = gaim_proxy_info_get_password(connect_data->gpi);

		i = (u == NULL) ? 0 : strlen(u);
		j = (p == NULL) ? 0 : strlen(p);

		connect_data->write_buf_len = 1 + 1 + i + 1 + j;
		connect_data->write_buffer = g_malloc(connect_data->write_buf_len);
		connect_data->written_len = 0;

		connect_data->write_buffer[0] = 0x01;	/* version 1 */
		connect_data->write_buffer[1] = i;
		if (u != NULL)
			memcpy(connect_data->write_buffer + 2, u, i);
		connect_data->write_buffer[2 + i] = j;
		if (p != NULL)
			memcpy(connect_data->write_buffer + 2 + i + 1, p, j);

		g_free(connect_data->read_buffer);
		connect_data->read_buffer = NULL;

		connect_data->read_cb = s5_readauth;

		connect_data->inpa = gaim_input_add(connect_data->fd, GAIM_INPUT_WRITE,
			proxy_do_write, connect_data);

		proxy_do_write(connect_data, connect_data->fd, GAIM_INPUT_WRITE);

		return;
	} else if (connect_data->read_buffer[1] == 0x03) {
		gsize userlen;
		userlen = strlen(gaim_proxy_info_get_username(connect_data->gpi));

		connect_data->write_buf_len = 7 + userlen;
		connect_data->write_buffer = g_malloc(connect_data->write_buf_len);
		connect_data->written_len = 0;

		connect_data->write_buffer[0] = 0x01;
		connect_data->write_buffer[1] = 0x02;
		connect_data->write_buffer[2] = 0x11;
		connect_data->write_buffer[3] = 0x01;
		connect_data->write_buffer[4] = 0x85;
		connect_data->write_buffer[5] = 0x02;
		connect_data->write_buffer[6] = userlen;
		memcpy(connect_data->write_buffer + 7,
			gaim_proxy_info_get_username(connect_data->gpi), userlen);

		g_free(connect_data->read_buffer);
		connect_data->read_buffer = NULL;

		connect_data->read_cb = s5_readchap;

		connect_data->inpa = gaim_input_add(connect_data->fd, GAIM_INPUT_WRITE,
			proxy_do_write, connect_data);

		proxy_do_write(connect_data, connect_data->fd, GAIM_INPUT_WRITE);

		return;
	} else {
		g_free(connect_data->read_buffer);
		connect_data->read_buffer = NULL;

		s5_sendconnect(connect_data, connect_data->fd);
	}
}

static void
s5_canwrite(gpointer data, gint source, GaimInputCondition cond)
{
	unsigned char buf[5];
	int i;
	GaimProxyConnectData *connect_data = data;
	socklen_t len;
	int error = ETIMEDOUT;
	int ret;

	gaim_debug_info("socks5 proxy", "Connected.\n");

	if (connect_data->inpa > 0)
	{
		gaim_input_remove(connect_data->inpa);
		connect_data->inpa = 0;
	}

	len = sizeof(error);
	ret = getsockopt(connect_data->fd, SOL_SOCKET, SO_ERROR, &error, &len);
	if ((ret != 0) || (error != 0))
	{
		if (ret != 0)
			error = errno;
		gaim_proxy_connect_data_disconnect(connect_data, strerror(error));
		return;
	}

	i = 0;
	buf[0] = 0x05;		/* SOCKS version 5 */

	if (gaim_proxy_info_get_username(connect_data->gpi) != NULL) {
		buf[1] = 0x03;	/* three methods */
		buf[2] = 0x00;	/* no authentication */
		buf[3] = 0x03;	/* CHAP authentication */
		buf[4] = 0x02;	/* username/password authentication */
		i = 5;
	}
	else {
		buf[1] = 0x01;
		buf[2] = 0x00;
		i = 3;
	}

	connect_data->write_buf_len = i;
	connect_data->write_buffer = g_malloc(connect_data->write_buf_len);
	memcpy(connect_data->write_buffer, buf, i);

	connect_data->read_cb = s5_canread;

	connect_data->inpa = gaim_input_add(connect_data->fd, GAIM_INPUT_WRITE, proxy_do_write, connect_data);
	proxy_do_write(connect_data, connect_data->fd, GAIM_INPUT_WRITE);
}

static void
proxy_connect_socks5(GaimProxyConnectData *connect_data, struct sockaddr *addr, socklen_t addrlen)
{
	gaim_debug_info("proxy",
			   "Connecting to %s:%d via %s:%d using SOCKS5\n",
			   connect_data->host, connect_data->port,
			   gaim_proxy_info_get_host(connect_data->gpi),
			   gaim_proxy_info_get_port(connect_data->gpi));

	connect_data->fd = socket(addr->sa_family, SOCK_STREAM, 0);
	if (connect_data->fd < 0)
	{
		gaim_proxy_connect_data_disconnect_formatted(connect_data,
				_("Unable to create socket:\n%s"), strerror(errno));
		return;
	}

	fcntl(connect_data->fd, F_SETFL, O_NONBLOCK);
#ifndef _WIN32
	fcntl(connect_data->fd, F_SETFD, FD_CLOEXEC);
#endif

	if (connect(connect_data->fd, addr, addrlen) != 0)
	{
		if ((errno == EINPROGRESS) || (errno == EINTR))
		{
			gaim_debug_info("socks5 proxy", "Connection in progress\n");
			connect_data->inpa = gaim_input_add(connect_data->fd,
					GAIM_INPUT_WRITE, s5_canwrite, connect_data);
		}
		else
		{
			gaim_proxy_connect_data_disconnect(connect_data, strerror(errno));
		}
	}
	else
	{
		gaim_debug_info("proxy", "Connected immediately.\n");

		s5_canwrite(connect_data, connect_data->fd, GAIM_INPUT_WRITE);
	}
}

/**
 * This function attempts to connect to the next IP address in the list
 * of IP addresses returned to us by gaim_dnsquery_a() and attemps
 * to connect to each one.  This is called after the hostname is
 * resolved, and each time a connection attempt fails (assuming there
 * is another IP address to try).
 */
static void try_connect(GaimProxyConnectData *connect_data)
{
	size_t addrlen;
	struct sockaddr *addr;

	addrlen = GPOINTER_TO_INT(connect_data->hosts->data);
	connect_data->hosts = g_slist_remove(connect_data->hosts, connect_data->hosts->data);
	addr = connect_data->hosts->data;
	connect_data->hosts = g_slist_remove(connect_data->hosts, connect_data->hosts->data);

	gaim_debug_info("proxy", "Attempting connection to %s\n", "TODO");

	switch (gaim_proxy_info_get_type(connect_data->gpi)) {
		case GAIM_PROXY_NONE:
			proxy_connect_none(connect_data, addr, addrlen);
			break;

		case GAIM_PROXY_HTTP:
			proxy_connect_http(connect_data, addr, addrlen);
			break;

		case GAIM_PROXY_SOCKS4:
			proxy_connect_socks4(connect_data, addr, addrlen);
			break;

		case GAIM_PROXY_SOCKS5:
			proxy_connect_socks5(connect_data, addr, addrlen);
			break;

		case GAIM_PROXY_USE_ENVVAR:
			proxy_connect_http(connect_data, addr, addrlen);
			break;

		default:
			break;
	}

	g_free(addr);
}

static void
connection_host_resolved(GSList *hosts, gpointer data,
						 const char *error_message)
{
	GaimProxyConnectData *connect_data;

	connect_data = data;
	connect_data->query_data = NULL;

	if (error_message != NULL)
	{
		gaim_proxy_connect_data_disconnect(connect_data, error_message);
		return;
	}

	if (hosts == NULL)
	{
		gaim_proxy_connect_data_disconnect(connect_data, _("Could not resolve host name"));
		return;
	}

	connect_data->hosts = hosts;

	try_connect(connect_data);
}

GaimProxyInfo *
gaim_proxy_get_setup(GaimAccount *account)
{
	GaimProxyInfo *gpi;
	const gchar *tmp;

	if (account && gaim_account_get_proxy_info(account) != NULL)
		gpi = gaim_account_get_proxy_info(account);
	else if (gaim_running_gnome())
		gpi = gaim_gnome_proxy_get_info();
	else
		gpi = gaim_global_proxy_get_info();

	if (gaim_proxy_info_get_type(gpi) == GAIM_PROXY_USE_ENVVAR) {
		if ((tmp = g_getenv("HTTP_PROXY")) != NULL ||
			(tmp = g_getenv("http_proxy")) != NULL ||
			(tmp = g_getenv("HTTPPROXY")) != NULL) {
			char *proxyhost,*proxypath,*proxyuser,*proxypasswd;
			int proxyport;

			/* http_proxy-format:
			 * export http_proxy="http://user:passwd@your.proxy.server:port/"
			 */
			if(gaim_url_parse(tmp, &proxyhost, &proxyport, &proxypath, &proxyuser, &proxypasswd)) {
				gaim_proxy_info_set_host(gpi, proxyhost);
				g_free(proxyhost);
				g_free(proxypath);
				if (proxyuser != NULL) {
					gaim_proxy_info_set_username(gpi, proxyuser);
					g_free(proxyuser);
				}
				if (proxypasswd != NULL) {
					gaim_proxy_info_set_password(gpi, proxypasswd);
					g_free(proxypasswd);
				}

				/* only for backward compatibility */
				if (proxyport == 80 &&
				    ((tmp = g_getenv("HTTP_PROXY_PORT")) != NULL ||
				     (tmp = g_getenv("http_proxy_port")) != NULL ||
				     (tmp = g_getenv("HTTPPROXYPORT")) != NULL))
				    proxyport = atoi(tmp);

				gaim_proxy_info_set_port(gpi, proxyport);
			}
		} else {
			/* no proxy environment variable found, don't use a proxy */
			gaim_debug_info("proxy", "No environment settings found, not using a proxy\n");
			gaim_proxy_info_set_type(gpi, GAIM_PROXY_NONE);
		}

		/* XXX: Do we want to skip this step if user/password were part of url? */
		if ((tmp = g_getenv("HTTP_PROXY_USER")) != NULL ||
			(tmp = g_getenv("http_proxy_user")) != NULL ||
			(tmp = g_getenv("HTTPPROXYUSER")) != NULL)
			gaim_proxy_info_set_username(gpi, tmp);

		if ((tmp = g_getenv("HTTP_PROXY_PASS")) != NULL ||
			(tmp = g_getenv("http_proxy_pass")) != NULL ||
			(tmp = g_getenv("HTTPPROXYPASS")) != NULL)
			gaim_proxy_info_set_password(gpi, tmp);
	}

	return gpi;
}

GaimProxyConnectData *
gaim_proxy_connect(GaimAccount *account, const char *host, int port,
				   GaimProxyConnectFunction connect_cb, gpointer data)
{
	const char *connecthost = host;
	int connectport = port;
	GaimProxyConnectData *connect_data;

	g_return_val_if_fail(host       != NULL, NULL);
	g_return_val_if_fail(port       >  0,    NULL);
	g_return_val_if_fail(connect_cb != NULL, NULL);

	connect_data = g_new0(GaimProxyConnectData, 1);
	connect_data->fd = -1;
	connect_data->connect_cb = connect_cb;
	connect_data->data = data;
	connect_data->host = g_strdup(host);
	connect_data->port = port;
	connect_data->gpi = gaim_proxy_get_setup(account);

	if ((gaim_proxy_info_get_type(connect_data->gpi) != GAIM_PROXY_NONE) &&
		(gaim_proxy_info_get_host(connect_data->gpi) == NULL ||
		 gaim_proxy_info_get_port(connect_data->gpi) <= 0)) {

		gaim_notify_error(NULL, NULL, _("Invalid proxy settings"), _("Either the host name or port number specified for your given proxy type is invalid."));
		gaim_proxy_connect_data_destroy(connect_data);
		return NULL;
	}

	switch (gaim_proxy_info_get_type(connect_data->gpi))
	{
		case GAIM_PROXY_NONE:
			break;

		case GAIM_PROXY_HTTP:
		case GAIM_PROXY_SOCKS4:
		case GAIM_PROXY_SOCKS5:
		case GAIM_PROXY_USE_ENVVAR:
			connecthost = gaim_proxy_info_get_host(connect_data->gpi);
			connectport = gaim_proxy_info_get_port(connect_data->gpi);
			break;

		default:
			gaim_proxy_connect_data_destroy(connect_data);
			return NULL;
	}

	connect_data->query_data = gaim_dnsquery_a(connecthost,
			connectport, connection_host_resolved, connect_data);
	if (connect_data->query_data == NULL)
	{
		gaim_proxy_connect_data_destroy(connect_data);
		return NULL;
	}

	connect_datas = g_slist_prepend(connect_datas, connect_data);

	return connect_data;
}

/*
 * Combine some of this code with gaim_proxy_connect()
 */
GaimProxyConnectData *
gaim_proxy_connect_socks5(GaimProxyInfo *gpi, const char *host, int port,
				   GaimProxyConnectFunction connect_cb, gpointer data)
{
	GaimProxyConnectData *connect_data;

	g_return_val_if_fail(host       != NULL, NULL);
	g_return_val_if_fail(port       >  0,    NULL);
	g_return_val_if_fail(connect_cb != NULL, NULL);

	connect_data = g_new0(GaimProxyConnectData, 1);
	connect_data->fd = -1;
	connect_data->connect_cb = connect_cb;
	connect_data->data = data;
	connect_data->host = g_strdup(host);
	connect_data->port = port;
	connect_data->gpi = gpi;

	connect_data->query_data =
			gaim_dnsquery_a(gaim_proxy_info_get_host(gpi),
					gaim_proxy_info_get_port(gpi),
					connection_host_resolved, connect_data);
	if (connect_data->query_data == NULL)
	{
		gaim_proxy_connect_data_destroy(connect_data);
		return NULL;
	}

	connect_datas = g_slist_prepend(connect_datas, connect_data);

	return connect_data;
}

void
gaim_proxy_connect_cancel(GaimProxyConnectData *connect_data)
{
	gaim_proxy_connect_data_disconnect(connect_data, NULL);
	gaim_proxy_connect_data_destroy(connect_data);
}

static void
proxy_pref_cb(const char *name, GaimPrefType type,
			  gconstpointer value, gpointer data)
{
	GaimProxyInfo *info = gaim_global_proxy_get_info();

	if (!strcmp(name, "/core/proxy/type")) {
		int proxytype;
		const char *type = value;

		if (!strcmp(type, "none"))
			proxytype = GAIM_PROXY_NONE;
		else if (!strcmp(type, "http"))
			proxytype = GAIM_PROXY_HTTP;
		else if (!strcmp(type, "socks4"))
			proxytype = GAIM_PROXY_SOCKS4;
		else if (!strcmp(type, "socks5"))
			proxytype = GAIM_PROXY_SOCKS5;
		else if (!strcmp(type, "envvar"))
			proxytype = GAIM_PROXY_USE_ENVVAR;
		else
			proxytype = -1;

		gaim_proxy_info_set_type(info, proxytype);
	} else if (!strcmp(name, "/core/proxy/host"))
		gaim_proxy_info_set_host(info, value);
	else if (!strcmp(name, "/core/proxy/port"))
		gaim_proxy_info_set_port(info, GPOINTER_TO_INT(value));
	else if (!strcmp(name, "/core/proxy/username"))
		gaim_proxy_info_set_username(info, value);
	else if (!strcmp(name, "/core/proxy/password"))
		gaim_proxy_info_set_password(info, value);
}

void *
gaim_proxy_get_handle()
{
	static int handle;

	return &handle;
}

void
gaim_proxy_init(void)
{
	void *handle;

	/* Initialize a default proxy info struct. */
	global_proxy_info = gaim_proxy_info_new();

	/* Proxy */
	gaim_prefs_add_none("/core/proxy");
	gaim_prefs_add_string("/core/proxy/type", "none");
	gaim_prefs_add_string("/core/proxy/host", "");
	gaim_prefs_add_int("/core/proxy/port", 0);
	gaim_prefs_add_string("/core/proxy/username", "");
	gaim_prefs_add_string("/core/proxy/password", "");

	/* Setup callbacks for the preferences. */
	handle = gaim_proxy_get_handle();
	gaim_prefs_connect_callback(handle, "/core/proxy/type", proxy_pref_cb,
		NULL);
	gaim_prefs_connect_callback(handle, "/core/proxy/host", proxy_pref_cb,
		NULL);
	gaim_prefs_connect_callback(handle, "/core/proxy/port", proxy_pref_cb,
		NULL);
	gaim_prefs_connect_callback(handle, "/core/proxy/username",
		proxy_pref_cb, NULL);
	gaim_prefs_connect_callback(handle, "/core/proxy/password",
		proxy_pref_cb, NULL);
}

void
gaim_proxy_uninit(void)
{
	while (connect_datas != NULL)
	{
		gaim_proxy_connect_data_disconnect(connect_datas->data, NULL);
		gaim_proxy_connect_data_destroy(connect_datas->data);
	}
}
