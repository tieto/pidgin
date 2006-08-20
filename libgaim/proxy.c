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

struct _GaimProxyConnectInfo {
	GaimProxyConnectFunction connect_cb;
	gpointer data;
	char *host;
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
static GSList *connect_infos = NULL;

static void try_connect(GaimProxyConnectInfo *connect_info);

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

/*
 * This is used when the connection attempt to one particular IP
 * address fails.  We close the socket, remove the watcher and get
 * rid of input and output buffers.  Normally try_connect() will
 * be called immediately after this.
 */
static void
gaim_proxy_connect_info_disconnect(GaimProxyConnectInfo *connect_info)
{
	if (connect_info->inpa > 0)
	{
		gaim_input_remove(connect_info->inpa);
		connect_info->inpa = 0;
	}

	if (connect_info->fd >= 0)
	{
		close(connect_info->fd);
		connect_info->fd = -1;
	}

	g_free(connect_info->write_buffer);
	connect_info->write_buffer = NULL;

	g_free(connect_info->read_buffer);
	connect_info->read_buffer = NULL;
}

static void
gaim_proxy_connect_info_destroy(GaimProxyConnectInfo *connect_info)
{
	gaim_proxy_connect_info_disconnect(connect_info);

	connect_infos = g_slist_remove(connect_infos, connect_info);

	if (connect_info->query_data != NULL)
		gaim_dnsquery_destroy(connect_info->query_data);

	while (connect_info->hosts != NULL)
	{
		/* Discard the length... */
		connect_info->hosts = g_slist_remove(connect_info->hosts, connect_info->hosts->data);
		/* Free the address... */
		g_free(connect_info->hosts->data);
		connect_info->hosts = g_slist_remove(connect_info->hosts, connect_info->hosts->data);
	}

	g_free(connect_info->host);
	g_free(connect_info);
}

static void
gaim_proxy_connect_info_connected(GaimProxyConnectInfo *connect_info)
{
	connect_info->connect_cb(connect_info->data, connect_info->fd, NULL);

	/*
	 * We've passed the file descriptor to the protocol, so it's no longer
	 * our responsibility, and we should be careful not to free it when
	 * we destroy the connect_info.
	 */
	connect_info->fd = -1;

	gaim_proxy_connect_info_destroy(connect_info);
}

/**
 * @param error An error message explaining why the connection
 *        failed.  This will be passed to the callback function
 *        specified in the call to gaim_proxy_connect().
 */
/*
 * TODO: Make sure all callers of this function pass a really really
 *       good error_message.
 */
static void
gaim_proxy_connect_info_error(GaimProxyConnectInfo *connect_info, const gchar *error_message)
{
	connect_info->connect_cb(connect_info->data, -1, error_message);
	gaim_proxy_connect_info_destroy(connect_info);
}

static void
no_one_calls(gpointer data, gint source, GaimInputCondition cond)
{
	GaimProxyConnectInfo *connect_info = data;
	socklen_t len;
	int error=0, ret;

	gaim_debug_info("proxy", "Connected.\n");

	len = sizeof(error);

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
	ret = getsockopt(connect_info->fd, SOL_SOCKET, SO_ERROR, &error, &len);
	if (ret == 0 && error == EINPROGRESS)
		return; /* we'll be called again later */
	if (ret < 0 || error != 0) {
		if (ret!=0)
			error = errno;

		gaim_debug_error("proxy",
			   "getsockopt SO_ERROR check: %s\n", strerror(error));

		gaim_proxy_connect_info_disconnect(connect_info);
		try_connect(connect_info);
		return;
	}

	gaim_input_remove(connect_info->inpa);
	connect_info->inpa = 0;

	gaim_proxy_connect_info_connected(connect_info);
}

static gboolean
clean_connect(gpointer data)
{
	GaimProxyConnectInfo *connect_info;

	connect_info = data;
	gaim_proxy_connect_info_connected(connect_info);

	return FALSE;
}

static int
proxy_connect_none(GaimProxyConnectInfo *connect_info, struct sockaddr *addr, socklen_t addrlen)
{
	gaim_debug_info("proxy", "Connecting to %s:%d with no proxy\n",
			connect_info->host, connect_info->port);

	connect_info->fd = socket(addr->sa_family, SOCK_STREAM, 0);
	if (connect_info->fd < 0)
	{
		gaim_debug_error("proxy",
				   "Unable to create socket: %s\n", strerror(errno));
		return -1;
	}
	fcntl(connect_info->fd, F_SETFL, O_NONBLOCK);
#ifndef _WIN32
	fcntl(connect_info->fd, F_SETFD, FD_CLOEXEC);
#endif

	if (connect(connect_info->fd, (struct sockaddr *)addr, addrlen) != 0)
	{
		if ((errno == EINPROGRESS) || (errno == EINTR)) {
			gaim_debug_info("proxy", "Connection in progress\n");
			connect_info->inpa = gaim_input_add(connect_info->fd, GAIM_INPUT_WRITE, no_one_calls, connect_info);
		}
		else {
			gaim_debug_error("proxy",
					   "Connect failed: %s\n", strerror(errno));
			close(connect_info->fd);
			connect_info->fd = -1;
			return -1;
		}
	}
	else
	{
		/*
		 * The connection happened IMMEDIATELY... strange, but whatever.
		 */
		socklen_t len;
		int error = ETIMEDOUT;
		gaim_debug_info("proxy", "Connected immediately.\n");
		len = sizeof(error);
		if (getsockopt(connect_info->fd, SOL_SOCKET, SO_ERROR, &error, &len) != 0)
		{
			gaim_debug_error("proxy", "getsockopt failed.\n");
			close(connect_info->fd);
			connect_info->fd = -1;
			return -1;
		}

		/*
		 * We want to call the "connected" callback eventually, but we
		 * don't want to call it before we return, just in case.
		 */
		gaim_timeout_add(10, clean_connect, connect_info);
	}

	return connect_info->fd;
}

static void
proxy_do_write(gpointer data, gint source, GaimInputCondition cond)
{
	GaimProxyConnectInfo *connect_info = data;
	const guchar *request = connect_info->write_buffer + connect_info->written_len;
	gsize request_len = connect_info->write_buf_len - connect_info->written_len;

	int ret = write(connect_info->fd, request, request_len);

	if(ret < 0 && errno == EAGAIN)
		return;
	else if(ret < 0) {
		gaim_proxy_connect_info_disconnect(connect_info);
		try_connect(connect_info);
		return;
	} else if (ret < request_len) {
		connect_info->written_len += ret;
		return;
	}

	gaim_input_remove(connect_info->inpa);
	g_free(connect_info->write_buffer);
	connect_info->write_buffer = NULL;

	/* register the response handler for the response */
	connect_info->inpa = gaim_input_add(connect_info->fd, GAIM_INPUT_READ, connect_info->read_cb, connect_info);
}

#define HTTP_GOODSTRING "HTTP/1.0 200"
#define HTTP_GOODSTRING2 "HTTP/1.1 200"

/* read the response to the CONNECT request, if we are requesting a non-port-80 tunnel */
static void
http_canread(gpointer data, gint source, GaimInputCondition cond)
{
	int len, headers_len, status = 0;
	gboolean error;
	GaimProxyConnectInfo *connect_info = data;
	guchar *p;
	gsize max_read;
	gchar *msg;

	if(connect_info->read_buffer == NULL) {
		connect_info->read_buf_len = 8192;
		connect_info->read_buffer = g_malloc(connect_info->read_buf_len);
		connect_info->read_len = 0;
	}

	p = connect_info->read_buffer + connect_info->read_len;
	max_read = connect_info->read_buf_len - connect_info->read_len - 1;

	len = read(connect_info->fd, p, max_read);
	if(len < 0 && errno == EAGAIN)
		return;
	else if(len <= 0) {
		gaim_proxy_connect_info_error(connect_info, _("Lost connection with server for an unknown reason."));
		return;
	} else {
		connect_info->read_len += len;
	}
	p[len] = '\0';

	if((p = (guchar *)g_strstr_len((const gchar *)connect_info->read_buffer, connect_info->read_len, "\r\n\r\n"))) {
		*p = '\0';
		headers_len = (p - connect_info->read_buffer) + 4;
	} else if(len == max_read)
		headers_len = len;
	else
		return;

	error = strncmp((const char *)connect_info->read_buffer, "HTTP/", 5) != 0;
	if (!error)
	{
		int major;
		p = connect_info->read_buffer + 5;
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
	p = (guchar *)g_strrstr((const gchar *)connect_info->read_buffer, "Content-Length: ");
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
		len -= connect_info->read_len - headers_len;
		/* I'm assuming that we're doing this to prevent the server from
		   complaining / breaking since we don't read the whole page */
		while(len--) {
			/* TODO: deal with EAGAIN (and other errors) better */
			if (read(connect_info->fd, &tmpc, 1) < 0 && errno != EAGAIN)
				break;
		}
	}

	if (error)
	{
		msg = g_strdup_printf("Unable to parse response from HTTP proxy: %s\n",
				connect_info->read_buffer);
		gaim_proxy_connect_info_error(connect_info, msg);
		g_free(msg);
		return;
	}
	else if (status != 200)
	{
		gaim_debug_error("proxy",
				"Proxy server replied with:\n%s\n",
				connect_info->read_buffer);


		if(status == 407 /* Proxy Auth */) {
			gchar *ntlm;
			if((ntlm = g_strrstr((const gchar *)connect_info->read_buffer, "Proxy-Authenticate: NTLM "))) { /* Check for Type-2 */
				gchar *tmp = ntlm;
				guint8 *nonce;
				gchar *domain = (gchar*)gaim_proxy_info_get_username(connect_info->gpi);
				gchar *username;
				gchar *request;
				gchar *response;
				username = strchr(domain, '\\');
				if (username == NULL)
				{
					msg = g_strdup_printf(_("HTTP proxy connection error %d"), status);
					gaim_proxy_connect_info_error(connect_info, msg);
					g_free(msg);
					return;
				}
				*username = '\0';
				username++;
				ntlm += strlen("Proxy-Authenticate: NTLM ");
				while(*tmp != '\r' && *tmp != '\0') tmp++;
				*tmp = '\0';
				nonce = gaim_ntlm_parse_type2(ntlm, NULL);
				response = gaim_ntlm_gen_type3(username,
					(gchar*) gaim_proxy_info_get_password(connect_info->gpi),
					(gchar*) gaim_proxy_info_get_host(connect_info->gpi),
					domain, nonce, NULL);
				username--;
				*username = '\\';
				request = g_strdup_printf(
					"CONNECT %s:%d HTTP/1.1\r\n"
					"Host: %s:%d\r\n"
					"Proxy-Authorization: NTLM %s\r\n"
					"Proxy-Connection: Keep-Alive\r\n\r\n",
					connect_info->host, connect_info->port, connect_info->host,
					connect_info->port, response);
				g_free(response);

				gaim_input_remove(connect_info->inpa);
				g_free(connect_info->read_buffer);
				connect_info->read_buffer = NULL;

				connect_info->write_buffer = (guchar *)request;
				connect_info->write_buf_len = strlen(request);
				connect_info->written_len = 0;

				connect_info->read_cb = http_canread;

				connect_info->inpa = gaim_input_add(connect_info->fd,
					GAIM_INPUT_WRITE, proxy_do_write, connect_info);

				proxy_do_write(connect_info, connect_info->fd, cond);
				return;
			} else if((ntlm = g_strrstr((const char *)connect_info->read_buffer, "Proxy-Authenticate: NTLM"))) { /* Empty message */
				gchar request[2048];
				gchar *domain = (gchar*) gaim_proxy_info_get_username(connect_info->gpi);
				gchar *username;
				int request_len;
				username = strchr(domain, '\\');
				if (username == NULL)
				{
					msg = g_strdup_printf(_("HTTP proxy connection error %d"), status);
					gaim_proxy_connect_info_error(connect_info, msg);
					g_free(msg);
					return;
				}
				*username = '\0';

				request_len = g_snprintf(request, sizeof(request),
						"CONNECT %s:%d HTTP/1.1\r\n"
						"Host: %s:%d\r\n",
						connect_info->host, connect_info->port,
						connect_info->host, connect_info->port);

				g_return_if_fail(request_len < sizeof(request));
				request_len += g_snprintf(request + request_len,
					sizeof(request) - request_len,
					"Proxy-Authorization: NTLM %s\r\n"
					"Proxy-Connection: Keep-Alive\r\n\r\n",
					gaim_ntlm_gen_type1(
						(gchar*) gaim_proxy_info_get_host(connect_info->gpi),
						domain));
				*username = '\\';

				gaim_input_remove(connect_info->inpa);
				g_free(connect_info->read_buffer);
				connect_info->read_buffer = NULL;

				connect_info->write_buffer = g_memdup(request, request_len);
				connect_info->write_buf_len = request_len;
				connect_info->written_len = 0;

				connect_info->read_cb = http_canread;

				connect_info->inpa = gaim_input_add(connect_info->fd,
					GAIM_INPUT_WRITE, proxy_do_write, connect_info);

				proxy_do_write(connect_info, connect_info->fd, cond);
				return;
			} else {
				msg = g_strdup_printf(_("HTTP proxy connection error %d"), status);
				gaim_proxy_connect_info_error(connect_info, msg);
				g_free(msg);
				return;
			}
		}
		if(status == 403 /* Forbidden */ ) {
			msg = g_strdup_printf(_("Access denied: HTTP proxy server forbids port %d tunnelling."), connect_info->port);
			gaim_proxy_connect_info_error(connect_info, msg);
			g_free(msg);
		} else {
			msg = g_strdup_printf(_("HTTP proxy connection error %d"), status);
			gaim_proxy_connect_info_error(connect_info, msg);
			g_free(msg);
		}
	} else {
		gaim_input_remove(connect_info->inpa);
		connect_info->inpa = 0;
		g_free(connect_info->read_buffer);
		connect_info->read_buffer = NULL;
		gaim_debug_info("proxy", "HTTP proxy connection established\n");
		gaim_proxy_connect_info_connected(connect_info);
		return;
	}
}



static void
http_canwrite(gpointer data, gint source, GaimInputCondition cond)
{
	char request[8192];
	int request_len = 0;
	GaimProxyConnectInfo *connect_info = data;
	socklen_t len;
	int error = ETIMEDOUT;

	gaim_debug_info("http proxy", "Connected.\n");

	if (connect_info->inpa > 0)
	{
		gaim_input_remove(connect_info->inpa);
		connect_info->inpa = 0;
	}

	len = sizeof(error);

	if (getsockopt(connect_info->fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
		gaim_proxy_connect_info_disconnect(connect_info);
		try_connect(connect_info);
		return;
	}

	gaim_debug_info("proxy", "using CONNECT tunnelling for %s:%d\n",
		connect_info->host, connect_info->port);
	request_len = g_snprintf(request, sizeof(request),
				 "CONNECT %s:%d HTTP/1.1\r\nHost: %s:%d\r\n",
				 connect_info->host, connect_info->port, connect_info->host, connect_info->port);

	if (gaim_proxy_info_get_username(connect_info->gpi) != NULL) {
		char *t1, *t2;
		t1 = g_strdup_printf("%s:%s",
			gaim_proxy_info_get_username(connect_info->gpi),
			gaim_proxy_info_get_password(connect_info->gpi) ?
				gaim_proxy_info_get_password(connect_info->gpi) : "");
		t2 = gaim_base64_encode((const guchar *)t1, strlen(t1));
		g_free(t1);
		g_return_if_fail(request_len < sizeof(request));

		request_len += g_snprintf(request + request_len,
			sizeof(request) - request_len,
			"Proxy-Authorization: Basic %s\r\n"
			"Proxy-Authorization: NTLM %s\r\n"
			"Proxy-Connection: Keep-Alive\r\n", t2,
			gaim_ntlm_gen_type1(
				(gchar*)gaim_proxy_info_get_host(connect_info->gpi),""));
		g_free(t2);
	}

	g_return_if_fail(request_len < sizeof(request));
	strcpy(request + request_len, "\r\n");
	request_len += 2;
	connect_info->write_buffer = g_memdup(request, request_len);
	connect_info->write_buf_len = request_len;
	connect_info->written_len = 0;

	connect_info->read_cb = http_canread;

	connect_info->inpa = gaim_input_add(connect_info->fd, GAIM_INPUT_WRITE, proxy_do_write,
		connect_info);

	proxy_do_write(connect_info, connect_info->fd, cond);
}

static int
proxy_connect_http(GaimProxyConnectInfo *connect_info, struct sockaddr *addr, socklen_t addrlen)
{
	gaim_debug_info("http proxy",
			   "Connecting to %s:%d via %s:%d using HTTP\n",
			   (connect_info->host ? connect_info->host : "(null)"), connect_info->port,
			   (gaim_proxy_info_get_host(connect_info->gpi) ? gaim_proxy_info_get_host(connect_info->gpi) : "(null)"),
			   gaim_proxy_info_get_port(connect_info->gpi));

	connect_info->fd = socket(addr->sa_family, SOCK_STREAM, 0);
	if (connect_info->fd < 0)
		return -1;

	fcntl(connect_info->fd, F_SETFL, O_NONBLOCK);
#ifndef _WIN32
	fcntl(connect_info->fd, F_SETFD, FD_CLOEXEC);
#endif

	if (connect(connect_info->fd, addr, addrlen) != 0)
	{
		if ((errno == EINPROGRESS) || (errno == EINTR)) {
			gaim_debug_info("http proxy", "Connection in progress\n");

			if (connect_info->port != 80) {
				/* we need to do CONNECT first */
				connect_info->inpa = gaim_input_add(connect_info->fd, GAIM_INPUT_WRITE,
							   http_canwrite, connect_info);
			} else {
				gaim_debug_info("proxy", "HTTP proxy connection established\n");
				gaim_proxy_connect_info_connected(connect_info);
			}
		} else {
			close(connect_info->fd);
			connect_info->fd = -1;
			return -1;
		}
	}
	else {
		socklen_t len;
		int error = ETIMEDOUT;

		gaim_debug_info("http proxy", "Connected immediately.\n");

		len = sizeof(error);
		if (getsockopt(connect_info->fd, SOL_SOCKET, SO_ERROR, &error, &len) != 0)
		{
			close(connect_info->fd);
			connect_info->fd = -1;
			return -1;
		}
		http_canwrite(connect_info, connect_info->fd, GAIM_INPUT_WRITE);
	}

	return connect_info->fd;
}


static void
s4_canread(gpointer data, gint source, GaimInputCondition cond)
{
	GaimProxyConnectInfo *connect_info = data;
	guchar *buf;
	int len, max_read;

	/* This is really not going to block under normal circumstances, but to
	 * be correct, we deal with the unlikely scenario */

	if (connect_info->read_buffer == NULL) {
		connect_info->read_buf_len = 12;
		connect_info->read_buffer = g_malloc(connect_info->read_buf_len);
		connect_info->read_len = 0;
	}

	buf = connect_info->read_buffer + connect_info->read_len;
	max_read = connect_info->read_buf_len - connect_info->read_len;

	len = read(connect_info->fd, buf, max_read);

	if ((len < 0 && errno == EAGAIN) || (len > 0 && len + connect_info->read_len < 4))
		return;
	else if (len + connect_info->read_len >= 4) {
		if (connect_info->read_buffer[1] == 90) {
			gaim_proxy_connect_info_connected(connect_info);
			return;
		}
	}

	gaim_proxy_connect_info_disconnect(connect_info);
	try_connect(connect_info);
}

static void
s4_canwrite(gpointer data, gint source, GaimInputCondition cond)
{
	unsigned char packet[9];
	struct hostent *hp;
	GaimProxyConnectInfo *connect_info = data;
	socklen_t len;
	int error = ETIMEDOUT;

	gaim_debug_info("socks4 proxy", "Connected.\n");

	if (connect_info->inpa > 0)
	{
		gaim_input_remove(connect_info->inpa);
		connect_info->inpa = 0;
	}

	len = sizeof(error);

	if (getsockopt(connect_info->fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
		gaim_proxy_connect_info_disconnect(connect_info);
		try_connect(connect_info);
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
	/* TODO: This needs to be non-blocking! */
	hp = gethostbyname(connect_info->host);
	if (hp == NULL) {
		gaim_proxy_connect_info_disconnect(connect_info);
		try_connect(connect_info);
		return;
	}

	packet[0] = 4;
	packet[1] = 1;
	packet[2] = connect_info->port >> 8;
	packet[3] = connect_info->port & 0xff;
	packet[4] = (unsigned char)(hp->h_addr_list[0])[0];
	packet[5] = (unsigned char)(hp->h_addr_list[0])[1];
	packet[6] = (unsigned char)(hp->h_addr_list[0])[2];
	packet[7] = (unsigned char)(hp->h_addr_list[0])[3];
	packet[8] = 0;

	connect_info->write_buffer = g_memdup(packet, sizeof(packet));
	connect_info->write_buf_len = sizeof(packet);
	connect_info->written_len = 0;
	connect_info->read_cb = s4_canread;

	connect_info->inpa = gaim_input_add(connect_info->fd, GAIM_INPUT_WRITE, proxy_do_write, connect_info);

	proxy_do_write(connect_info, connect_info->fd, cond);
}

static int
proxy_connect_socks4(GaimProxyConnectInfo *connect_info, struct sockaddr *addr, socklen_t addrlen)
{
	gaim_debug_info("socks4 proxy",
			   "Connecting to %s:%d via %s:%d using SOCKS4\n",
			   connect_info->host, connect_info->port,
			   gaim_proxy_info_get_host(connect_info->gpi),
			   gaim_proxy_info_get_port(connect_info->gpi));

	connect_info->fd = socket(addr->sa_family, SOCK_STREAM, 0);
	if (connect_info->fd < 0)
		return -1;

	fcntl(connect_info->fd, F_SETFL, O_NONBLOCK);
#ifndef _WIN32
	fcntl(connect_info->fd, F_SETFD, FD_CLOEXEC);
#endif

	if (connect(connect_info->fd, addr, addrlen) != 0)
	{
		if ((errno == EINPROGRESS) || (errno == EINTR)) {
			gaim_debug_info("socks4 proxy", "Connection in progress.\n");
			connect_info->inpa = gaim_input_add(connect_info->fd, GAIM_INPUT_WRITE, s4_canwrite, connect_info);
		}
		else {
			close(connect_info->fd);
			connect_info->fd = -1;
			return -1;
		}
	} else {
		socklen_t len;
		int error = ETIMEDOUT;

		gaim_debug_info("socks4 proxy", "Connected immediately.\n");

		len = sizeof(error);

		if (getsockopt(connect_info->fd, SOL_SOCKET, SO_ERROR, &error, &len) != 0)
		{
			close(connect_info->fd);
			connect_info->fd = -1;
			return -1;
		}

		s4_canwrite(connect_info, connect_info->fd, GAIM_INPUT_WRITE);
	}

	return connect_info->fd;
}

static void
s5_canread_again(gpointer data, gint source, GaimInputCondition cond)
{
	guchar *dest, *buf;
	GaimProxyConnectInfo *connect_info = data;
	int len;

	if (connect_info->read_buffer == NULL) {
		connect_info->read_buf_len = 512;
		connect_info->read_buffer = g_malloc(connect_info->read_buf_len);
		connect_info->read_len = 0;
	}

	dest = connect_info->read_buffer + connect_info->read_len;
	buf = connect_info->read_buffer;

	gaim_debug_info("socks5 proxy", "Able to read again.\n");

	len = read(connect_info->fd, dest, (connect_info->read_buf_len - connect_info->read_len));
	if(len < 0 && errno == EAGAIN)
		return;
	else if(len <= 0) {
		gaim_debug_warning("socks5 proxy", "or not...\n");
		gaim_proxy_connect_info_disconnect(connect_info);
		try_connect(connect_info);
		return;
	}
	connect_info->read_len += len;

	if(connect_info->read_len < 4)
		return;

	if ((buf[0] != 0x05) || (buf[1] != 0x00)) {
		if ((buf[0] == 0x05) && (buf[1] < 0x09))
			gaim_debug_error("socks5 proxy", socks5errors[buf[1]]);
		else
			gaim_debug_error("socks5 proxy", "Bad data.\n");
		gaim_proxy_connect_info_disconnect(connect_info);
		try_connect(connect_info);
		return;
	}

	/* Skip past BND.ADDR */
	switch(buf[3]) {
		case 0x01: /* the address is a version-4 IP address, with a length of 4 octets */
			if(connect_info->read_len < 4 + 4)
				return;
			buf += 4 + 4;
			break;
		case 0x03: /* the address field contains a fully-qualified domain name.  The first
					  octet of the address field contains the number of octets of name that
					  follow, there is no terminating NUL octet. */
			if(connect_info->read_len < 4 + 1)
				return;
			buf += 4 + 1;
			if(connect_info->read_len < 4 + 1 + buf[0])
				return;
			buf += buf[0];
			break;
		case 0x04: /* the address is a version-6 IP address, with a length of 16 octets */
			if(connect_info->read_len < 4 + 16)
				return;
			buf += 4 + 16;
			break;
	}

	if(connect_info->read_len < (buf - connect_info->read_buffer) + 2)
		return;

	/* Skip past BND.PORT */
	buf += 2;

	gaim_proxy_connect_info_connected(connect_info);
}

static void
s5_sendconnect(gpointer data, int source)
{
	GaimProxyConnectInfo *connect_info = data;
	int hlen = strlen(connect_info->host);
	connect_info->write_buf_len = 5 + hlen + 2;
	connect_info->write_buffer = g_malloc(connect_info->write_buf_len);
	connect_info->written_len = 0;

	connect_info->write_buffer[0] = 0x05;
	connect_info->write_buffer[1] = 0x01;		/* CONNECT */
	connect_info->write_buffer[2] = 0x00;		/* reserved */
	connect_info->write_buffer[3] = 0x03;		/* address type -- host name */
	connect_info->write_buffer[4] = hlen;
	memcpy(connect_info->write_buffer + 5, connect_info->host, hlen);
	connect_info->write_buffer[5 + hlen] = connect_info->port >> 8;
	connect_info->write_buffer[5 + hlen + 1] = connect_info->port & 0xff;

	connect_info->read_cb = s5_canread_again;

	connect_info->inpa = gaim_input_add(connect_info->fd, GAIM_INPUT_WRITE, proxy_do_write, connect_info);
	proxy_do_write(connect_info, connect_info->fd, GAIM_INPUT_WRITE);
}

static void
s5_readauth(gpointer data, gint source, GaimInputCondition cond)
{
	GaimProxyConnectInfo *connect_info = data;
	int len;

	if (connect_info->read_buffer == NULL) {
		connect_info->read_buf_len = 2;
		connect_info->read_buffer = g_malloc(connect_info->read_buf_len);
		connect_info->read_len = 0;
	}

	gaim_debug_info("socks5 proxy", "Got auth response.\n");

	len = read(connect_info->fd, connect_info->read_buffer + connect_info->read_len,
		connect_info->read_buf_len - connect_info->read_len);
	if(len < 0 && errno == EAGAIN)
		return;
	else if(len <= 0) {
		gaim_proxy_connect_info_disconnect(connect_info);
		try_connect(connect_info);
		return;
	}
	connect_info->read_len += len;

	if (connect_info->read_len < 2)
		return;

	gaim_input_remove(connect_info->inpa);
	connect_info->inpa = 0;

	if ((connect_info->read_buffer[0] != 0x01) || (connect_info->read_buffer[1] != 0x00)) {
		gaim_proxy_connect_info_disconnect(connect_info);
		try_connect(connect_info);
		return;
	}

	g_free(connect_info->read_buffer);
	connect_info->read_buffer = NULL;

	s5_sendconnect(connect_info, connect_info->fd);
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
	GaimProxyConnectInfo *connect_info = data;
	int len, navas, currentav;

	gaim_debug(GAIM_DEBUG_INFO, "socks5 proxy", "Got CHAP response.\n");

	if (connect_info->read_buffer == NULL) {
		connect_info->read_buf_len = 20;
		connect_info->read_buffer = g_malloc(connect_info->read_buf_len);
		connect_info->read_len = 0;
	}

	len = read(connect_info->fd, connect_info->read_buffer + connect_info->read_len,
		connect_info->read_buf_len - connect_info->read_len);

	if(len < 0 && errno == EAGAIN)
		return;
	else if(len <= 0) {
		gaim_proxy_connect_info_disconnect(connect_info);
		try_connect(connect_info);
		return;
	}
	connect_info->read_len += len;

	if (connect_info->read_len < 2)
		return;

	cmdbuf = connect_info->read_buffer;

	if (*cmdbuf != 0x01) {
		gaim_proxy_connect_info_disconnect(connect_info);
		try_connect(connect_info);
		return;
	}
	cmdbuf++;

	navas = *cmdbuf;
	cmdbuf++;

	for (currentav = 0; currentav < navas; currentav++) {
		if (connect_info->read_len - (cmdbuf - connect_info->read_buffer) < 2)
			return;
		if (connect_info->read_len - (cmdbuf - connect_info->read_buffer) < cmdbuf[1])
			return;
		buf = cmdbuf + 2;
		switch (cmdbuf[0]) {
			case 0x00:
				/* Did auth work? */
				if (buf[0] == 0x00) {
					gaim_input_remove(connect_info->inpa);
					connect_info->inpa = 0;
					g_free(connect_info->read_buffer);
					connect_info->read_buffer = NULL;
					/* Success */
					s5_sendconnect(connect_info, connect_info->fd);
					return;
				} else {
					/* Failure */
					gaim_debug_warning("proxy",
						"socks5 CHAP authentication "
						"failed.  Disconnecting...");
					gaim_proxy_connect_info_disconnect(connect_info);
					try_connect(connect_info);
					return;
				}
				break;
			case 0x03:
				/* Server wants our credentials */

				connect_info->write_buf_len = 16 + 4;
				connect_info->write_buffer = g_malloc(connect_info->write_buf_len);
				connect_info->written_len = 0;

				hmacmd5_chap(buf, cmdbuf[1],
					gaim_proxy_info_get_password(connect_info->gpi),
					connect_info->write_buffer + 4);
				connect_info->write_buffer[0] = 0x01;
				connect_info->write_buffer[1] = 0x01;
				connect_info->write_buffer[2] = 0x04;
				connect_info->write_buffer[3] = 0x10;

				gaim_input_remove(connect_info->inpa);
				g_free(connect_info->read_buffer);
				connect_info->read_buffer = NULL;

				connect_info->read_cb = s5_readchap;

				connect_info->inpa = gaim_input_add(connect_info->fd,
					GAIM_INPUT_WRITE, proxy_do_write, connect_info);

				proxy_do_write(connect_info, connect_info->fd, GAIM_INPUT_WRITE);
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
					gaim_proxy_connect_info_disconnect(connect_info);
					try_connect(connect_info);
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
	GaimProxyConnectInfo *connect_info = data;
	int len;

	if (connect_info->read_buffer == NULL) {
		connect_info->read_buf_len = 2;
		connect_info->read_buffer = g_malloc(connect_info->read_buf_len);
		connect_info->read_len = 0;
	}

	gaim_debug_info("socks5 proxy", "Able to read.\n");

	len = read(connect_info->fd, connect_info->read_buffer + connect_info->read_len,
		connect_info->read_buf_len - connect_info->read_len);
	if(len < 0 && errno == EAGAIN)
		return;
	else if(len <= 0) {
		gaim_proxy_connect_info_disconnect(connect_info);
		try_connect(connect_info);
		return;
	}
	connect_info->read_len += len;

	if (connect_info->read_len < 2)
		return;

	gaim_input_remove(connect_info->inpa);
	connect_info->inpa = 0;

	if ((connect_info->read_buffer[0] != 0x05) || (connect_info->read_buffer[1] == 0xff)) {
		gaim_proxy_connect_info_disconnect(connect_info);
		try_connect(connect_info);
		return;
	}

	if (connect_info->read_buffer[1] == 0x02) {
		gsize i, j;
		const char *u, *p;

		u = gaim_proxy_info_get_username(connect_info->gpi);
		p = gaim_proxy_info_get_password(connect_info->gpi);

		i = (u == NULL) ? 0 : strlen(u);
		j = (p == NULL) ? 0 : strlen(p);

		connect_info->write_buf_len = 1 + 1 + i + 1 + j;
		connect_info->write_buffer = g_malloc(connect_info->write_buf_len);
		connect_info->written_len = 0;

		connect_info->write_buffer[0] = 0x01;	/* version 1 */
		connect_info->write_buffer[1] = i;
		if (u != NULL)
			memcpy(connect_info->write_buffer + 2, u, i);
		connect_info->write_buffer[2 + i] = j;
		if (p != NULL)
			memcpy(connect_info->write_buffer + 2 + i + 1, p, j);

		g_free(connect_info->read_buffer);
		connect_info->read_buffer = NULL;

		connect_info->read_cb = s5_readauth;

		connect_info->inpa = gaim_input_add(connect_info->fd, GAIM_INPUT_WRITE,
			proxy_do_write, connect_info);

		proxy_do_write(connect_info, connect_info->fd, GAIM_INPUT_WRITE);

		return;
	} else if (connect_info->read_buffer[1] == 0x03) {
		gsize userlen;
		userlen = strlen(gaim_proxy_info_get_username(connect_info->gpi));

		connect_info->write_buf_len = 7 + userlen;
		connect_info->write_buffer = g_malloc(connect_info->write_buf_len);
		connect_info->written_len = 0;

		connect_info->write_buffer[0] = 0x01;
		connect_info->write_buffer[1] = 0x02;
		connect_info->write_buffer[2] = 0x11;
		connect_info->write_buffer[3] = 0x01;
		connect_info->write_buffer[4] = 0x85;
		connect_info->write_buffer[5] = 0x02;
		connect_info->write_buffer[6] = userlen;
		memcpy(connect_info->write_buffer + 7,
			gaim_proxy_info_get_username(connect_info->gpi), userlen);

		g_free(connect_info->read_buffer);
		connect_info->read_buffer = NULL;

		connect_info->read_cb = s5_readchap;

		connect_info->inpa = gaim_input_add(connect_info->fd, GAIM_INPUT_WRITE,
			proxy_do_write, connect_info);

		proxy_do_write(connect_info, connect_info->fd, GAIM_INPUT_WRITE);

		return;
	} else {
		g_free(connect_info->read_buffer);
		connect_info->read_buffer = NULL;

		s5_sendconnect(connect_info, connect_info->fd);
	}
}

static void
s5_canwrite(gpointer data, gint source, GaimInputCondition cond)
{
	unsigned char buf[5];
	int i;
	GaimProxyConnectInfo *connect_info = data;
	socklen_t len;
	int error = ETIMEDOUT;

	gaim_debug_info("socks5 proxy", "Connected.\n");

	if (connect_info->inpa > 0)
	{
		gaim_input_remove(connect_info->inpa);
		connect_info->inpa = 0;
	}

	len = sizeof(error);
	if (getsockopt(connect_info->fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
		gaim_proxy_connect_info_disconnect(connect_info);
		try_connect(connect_info);
		return;
	}

	i = 0;
	buf[0] = 0x05;		/* SOCKS version 5 */

	if (gaim_proxy_info_get_username(connect_info->gpi) != NULL) {
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

	connect_info->write_buf_len = i;
	connect_info->write_buffer = g_malloc(connect_info->write_buf_len);
	memcpy(connect_info->write_buffer, buf, i);

	connect_info->read_cb = s5_canread;

	connect_info->inpa = gaim_input_add(connect_info->fd, GAIM_INPUT_WRITE, proxy_do_write, connect_info);
	proxy_do_write(connect_info, connect_info->fd, GAIM_INPUT_WRITE);
}

static int
proxy_connect_socks5(GaimProxyConnectInfo *connect_info, struct sockaddr *addr, socklen_t addrlen)
{
	gaim_debug_info("socks5 proxy",
			   "Connecting to %s:%d via %s:%d using SOCKS5\n",
			   connect_info->host, connect_info->port,
			   gaim_proxy_info_get_host(connect_info->gpi),
			   gaim_proxy_info_get_port(connect_info->gpi));

	connect_info->fd = socket(addr->sa_family, SOCK_STREAM, 0);
	if (connect_info->fd < 0)
		return -1;

	fcntl(connect_info->fd, F_SETFL, O_NONBLOCK);
#ifndef _WIN32
	fcntl(connect_info->fd, F_SETFD, FD_CLOEXEC);
#endif

	if (connect(connect_info->fd, addr, addrlen) != 0)
	{
		if ((errno == EINPROGRESS) || (errno == EINTR)) {
			gaim_debug_info("socks5 proxy", "Connection in progress\n");
			connect_info->inpa = gaim_input_add(connect_info->fd, GAIM_INPUT_WRITE, s5_canwrite, connect_info);
		}
		else {
			close(connect_info->fd);
			connect_info->fd = -1;
			return -1;
		}
	}
	else {
		socklen_t len;
		int error = ETIMEDOUT;

		gaim_debug_info("socks5 proxy", "Connected immediately.\n");

		len = sizeof(error);

		if (getsockopt(connect_info->fd, SOL_SOCKET, SO_ERROR, &error, &len) != 0)
		{
			close(connect_info->fd);
			connect_info->fd = -1;
			return -1;
		}

		s5_canwrite(connect_info, connect_info->fd, GAIM_INPUT_WRITE);
	}

	return connect_info->fd;
}

/**
 * This function iterates through a list of IP addresses and attempts
 * to connect to each one.  This is called after the hostname is
 * resolved, and if a connection attempt fails.
 */
static void try_connect(GaimProxyConnectInfo *connect_info)
{
	size_t addrlen;
	struct sockaddr *addr;
	int ret = -1;

	if (connect_info->hosts == NULL)
	{
		gaim_proxy_connect_info_error(connect_info, _("Could not resolve host name"));
		return;
	}

	while (connect_info->hosts)
	{
		addrlen = GPOINTER_TO_INT(connect_info->hosts->data);
		connect_info->hosts = g_slist_remove(connect_info->hosts, connect_info->hosts->data);
		addr = connect_info->hosts->data;
		connect_info->hosts = g_slist_remove(connect_info->hosts, connect_info->hosts->data);

		switch (gaim_proxy_info_get_type(connect_info->gpi)) {
			case GAIM_PROXY_NONE:
				ret = proxy_connect_none(connect_info, addr, addrlen);
				break;

			case GAIM_PROXY_HTTP:
				ret = proxy_connect_http(connect_info, addr, addrlen);
				break;

			case GAIM_PROXY_SOCKS4:
				ret = proxy_connect_socks4(connect_info, addr, addrlen);
				break;

			case GAIM_PROXY_SOCKS5:
				ret = proxy_connect_socks5(connect_info, addr, addrlen);
				break;

			case GAIM_PROXY_USE_ENVVAR:
				ret = proxy_connect_http(connect_info, addr, addrlen);
				break;

			default:
				break;
		}

		g_free(addr);

		if (ret >= 0)
			break;
	}

	if (ret < 0) {
		gaim_proxy_connect_info_error(connect_info, _("Unable to establish a connection"));
	}
}

static void
connection_host_resolved(GSList *hosts, gpointer data,
						 const char *error_message)
{
	GaimProxyConnectInfo *connect_info;

	connect_info = data;
	connect_info->query_data = NULL;

	if (error_message != NULL)
	{
		gchar *tmp;
		tmp = g_strdup_printf("Error while resolving hostname: %s\n", error_message);
		gaim_proxy_connect_info_error(connect_info, tmp);
		g_free(tmp);
		return;
	}

	connect_info->hosts = hosts;

	try_connect(connect_info);
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

GaimProxyConnectInfo *
gaim_proxy_connect(GaimAccount *account, const char *host, int port,
				   GaimProxyConnectFunction connect_cb, gpointer data)
{
	const char *connecthost = host;
	int connectport = port;
	GaimProxyConnectInfo *connect_info;

	g_return_val_if_fail(host       != NULL, NULL);
	g_return_val_if_fail(port       >  0,    NULL);
	g_return_val_if_fail(connect_cb != NULL, NULL);

	connect_info = g_new0(GaimProxyConnectInfo, 1);
	connect_info->fd = -1;
	connect_info->connect_cb = connect_cb;
	connect_info->data = data;
	connect_info->host = g_strdup(host);
	connect_info->port = port;
	connect_info->gpi = gaim_proxy_get_setup(account);

	if ((gaim_proxy_info_get_type(connect_info->gpi) != GAIM_PROXY_NONE) &&
		(gaim_proxy_info_get_host(connect_info->gpi) == NULL ||
		 gaim_proxy_info_get_port(connect_info->gpi) <= 0)) {

		gaim_notify_error(NULL, NULL, _("Invalid proxy settings"), _("Either the host name or port number specified for your given proxy type is invalid."));
		gaim_proxy_connect_info_destroy(connect_info);
		return NULL;
	}

	switch (gaim_proxy_info_get_type(connect_info->gpi))
	{
		case GAIM_PROXY_NONE:
			break;

		case GAIM_PROXY_HTTP:
		case GAIM_PROXY_SOCKS4:
		case GAIM_PROXY_SOCKS5:
		case GAIM_PROXY_USE_ENVVAR:
			connecthost = gaim_proxy_info_get_host(connect_info->gpi);
			connectport = gaim_proxy_info_get_port(connect_info->gpi);
			break;

		default:
			gaim_proxy_connect_info_destroy(connect_info);
			return NULL;
	}

	connect_info->query_data = gaim_dnsquery_a(connecthost,
			connectport, connection_host_resolved, connect_info);
	if (connect_info->query_data == NULL)
	{
		gaim_proxy_connect_info_destroy(connect_info);
		return NULL;
	}

	connect_infos = g_slist_prepend(connect_infos, connect_info);

	return connect_info;
}

/*
 * Combine some of this code with gaim_proxy_connect()
 */
GaimProxyConnectInfo *
gaim_proxy_connect_socks5(GaimProxyInfo *gpi, const char *host, int port,
				   GaimProxyConnectFunction connect_cb, gpointer data)
{
	GaimProxyConnectInfo *connect_info;

	g_return_val_if_fail(host       != NULL, NULL);
	g_return_val_if_fail(port       >  0,    NULL);
	g_return_val_if_fail(connect_cb != NULL, NULL);

	connect_info = g_new0(GaimProxyConnectInfo, 1);
	connect_info->fd = -1;
	connect_info->connect_cb = connect_cb;
	connect_info->data = data;
	connect_info->host = g_strdup(host);
	connect_info->port = port;
	connect_info->gpi = gpi;

	connect_info->query_data =
			gaim_dnsquery_a(gaim_proxy_info_get_host(gpi),
					gaim_proxy_info_get_port(gpi),
					connection_host_resolved, connect_info);
	if (connect_info->query_data == NULL)
	{
		gaim_proxy_connect_info_destroy(connect_info);
		return NULL;
	}

	connect_infos = g_slist_prepend(connect_infos, connect_info);

	return connect_info;
}

void
gaim_proxy_connect_cancel(GaimProxyConnectInfo *connect_info)
{
	gaim_proxy_connect_info_destroy(connect_info);
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
	while (connect_infos != NULL)
		gaim_proxy_connect_info_destroy(connect_infos->data);
}
