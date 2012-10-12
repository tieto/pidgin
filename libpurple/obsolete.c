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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301 USA
 */

#include "obsolete.h"

#include "internal.h"
#include "debug.h"
#include "ntlm.h"

struct _PurpleUtilFetchUrlData
{
	PurpleUtilFetchUrlCallback callback;
	void *user_data;

	struct
	{
		char *user;
		char *passwd;
		char *address;
		int port;
		char *page;

	} website;

	char *url;
	int num_times_redirected;
	gboolean full;
	char *user_agent;
	gboolean http11;
	char *request;
	gsize request_written;
	gboolean include_headers;

	gboolean is_ssl;
	PurpleSslConnection *ssl_connection;
	PurpleProxyConnectData *connect_data;
	int fd;
	guint inpa;

	gboolean got_headers;
	gboolean has_explicit_data_len;
	char *webdata;
	gsize len;
	unsigned long data_len;
	gssize max_len;
	gboolean chunked;
	PurpleAccount *account;
};

/**
 * The arguments to this function are similar to printf.
 */
static void
purple_util_fetch_url_error(PurpleUtilFetchUrlData *gfud, const char *format, ...)
{
	gchar *error_message;
	va_list args;

	va_start(args, format);
	error_message = g_strdup_vprintf(format, args);
	va_end(args);

	gfud->callback(gfud, gfud->user_data, NULL, 0, error_message);
	g_free(error_message);
	purple_util_fetch_url_cancel(gfud);
}

static void url_fetch_connect_cb(gpointer url_data, gint source, const gchar *error_message);
static void ssl_url_fetch_connect_cb(gpointer data, PurpleSslConnection *ssl_connection, PurpleInputCondition cond);
static void ssl_url_fetch_error_cb(PurpleSslConnection *ssl_connection, PurpleSslErrorType error, gpointer data);

static gboolean
parse_redirect(const char *data, size_t data_len,
			   PurpleUtilFetchUrlData *gfud)
{
	gchar *s;
	gchar *new_url, *temp_url, *end;
	gboolean full;
	int len;

	if ((s = g_strstr_len(data, data_len, "\nLocation: ")) == NULL)
		/* We're not being redirected */
		return FALSE;

	s += strlen("Location: ");
	end = strchr(s, '\r');

	/* Just in case :) */
	if (end == NULL)
		end = strchr(s, '\n');

	if (end == NULL)
		return FALSE;

	len = end - s;

	new_url = g_malloc(len + 1);
	strncpy(new_url, s, len);
	new_url[len] = '\0';

	full = gfud->full;

	if (*new_url == '/' || g_strstr_len(new_url, len, "://") == NULL)
	{
		temp_url = new_url;

		new_url = g_strdup_printf("%s:%d%s", gfud->website.address,
								  gfud->website.port, temp_url);

		g_free(temp_url);

		full = FALSE;
	}

	purple_debug_info("util", "Redirecting to %s\n", new_url);

	gfud->num_times_redirected++;
	if (gfud->num_times_redirected >= 5)
	{
		purple_util_fetch_url_error(gfud,
				_("Could not open %s: Redirected too many times"),
				gfud->url);
		return TRUE;
	}

	/*
	 * Try again, with this new location.  This code is somewhat
	 * ugly, but we need to reuse the gfud because whoever called
	 * us is holding a reference to it.
	 */
	g_free(gfud->url);
	gfud->url = new_url;
	gfud->full = full;
	g_free(gfud->request);
	gfud->request = NULL;

	if (gfud->is_ssl) {
		gfud->is_ssl = FALSE;
		purple_ssl_close(gfud->ssl_connection);
		gfud->ssl_connection = NULL;
	} else {
		purple_input_remove(gfud->inpa);
		gfud->inpa = 0;
		close(gfud->fd);
		gfud->fd = -1;
	}
	gfud->request_written = 0;
	gfud->len = 0;
	gfud->data_len = 0;

	g_free(gfud->website.user);
	g_free(gfud->website.passwd);
	g_free(gfud->website.address);
	g_free(gfud->website.page);
	purple_url_parse(new_url, &gfud->website.address, &gfud->website.port,
				   &gfud->website.page, &gfud->website.user, &gfud->website.passwd);

	if (purple_strcasestr(new_url, "https://") != NULL) {
		gfud->is_ssl = TRUE;
		gfud->ssl_connection = purple_ssl_connect(gfud->account,
				gfud->website.address, gfud->website.port,
				ssl_url_fetch_connect_cb, ssl_url_fetch_error_cb, gfud);
	} else {
		gfud->connect_data = purple_proxy_connect(NULL, gfud->account,
				gfud->website.address, gfud->website.port,
				url_fetch_connect_cb, gfud);
	}

	if (gfud->ssl_connection == NULL && gfud->connect_data == NULL)
	{
		purple_util_fetch_url_error(gfud, _("Unable to connect to %s"),
				gfud->website.address);
	}

	return TRUE;
}

static const char *
find_header_content(const char *data, size_t data_len, const char *header, size_t header_len)
{
	const char *p = NULL;

	if (header_len <= 0)
		header_len = strlen(header);

	/* Note: data is _not_ nul-terminated.  */
	if (data_len > header_len) {
		if (header[0] == '\n')
			p = (g_ascii_strncasecmp(data, header + 1, header_len - 1) == 0) ? data : NULL;
		if (!p)
			p = purple_strcasestr(data, header);
		if (p)
			p += header_len;
	}

	/* If we can find the header at all, try to sscanf it.
	 * Response headers should end with at least \r\n, so sscanf is safe,
	 * if we make sure that there is indeed a \n in our header.
	 */
	if (p && g_strstr_len(p, data_len - (p - data), "\n")) {
		return p;
	}

	return NULL;
}

static size_t
parse_content_len(const char *data, size_t data_len)
{
	size_t content_len = 0;
	const char *p = NULL;

	p = find_header_content(data, data_len, "\nContent-Length: ", sizeof("\nContent-Length: ") - 1);
	if (p) {
		sscanf(p, "%" G_GSIZE_FORMAT, &content_len);
		purple_debug_misc("util", "parsed %" G_GSIZE_FORMAT "\n", content_len);
	}

	return content_len;
}

static gboolean
content_is_chunked(const char *data, size_t data_len)
{
	const char *p = find_header_content(data, data_len, "\nTransfer-Encoding: ", sizeof("\nTransfer-Encoding: ") - 1);
	if (p && g_ascii_strncasecmp(p, "chunked", 7) == 0)
		return TRUE;

	return FALSE;
}

/* Process in-place */
static void
process_chunked_data(char *data, gsize *len)
{
	gsize sz;
	gsize newlen = 0;
	char *p = data;
	char *s = data;

	while (*s) {
		/* Read the size of this chunk */
		if (sscanf(s, "%" G_GSIZE_MODIFIER "x", &sz) != 1)
		{
			purple_debug_error("util", "Error processing chunked data: "
					"Expected data length, found: %s\n", s);
			break;
		}
		if (sz == 0) {
			/* We've reached the last chunk */
			/*
			 * TODO: The spec allows "footers" to follow the last chunk.
			 *       If there is more data after this line then we should
			 *       treat it like a header.
			 */
			break;
		}

		/* Advance to the start of the data */
		s = strstr(s, "\r\n");
		if (s == NULL)
			break;
		s += 2;

		if (s + sz > data + *len) {
			purple_debug_error("util", "Error processing chunked data: "
					"Chunk size %" G_GSIZE_FORMAT " bytes was longer "
					"than the data remaining in the buffer (%"
					G_GSIZE_FORMAT " bytes)\n", sz, data + *len - s);
		}

		/* Move all data overtop of the chunk length that we read in earlier */
		g_memmove(p, s, sz);
		p += sz;
		s += sz;
		newlen += sz;
		if (*s != '\r' && *(s + 1) != '\n') {
			purple_debug_error("util", "Error processing chunked data: "
					"Expected \\r\\n, found: %s\n", s);
			break;
		}
		s += 2;
	}

	/* NULL terminate the data */
	*p = 0;

	*len = newlen;
}

static void
url_fetch_recv_cb(gpointer url_data, gint source, PurpleInputCondition cond)
{
	PurpleUtilFetchUrlData *gfud = url_data;
	int len;
	char buf[4096];
	char *data_cursor;
	gboolean got_eof = FALSE;

	/*
	 * Read data in a loop until we can't read any more!  This is a
	 * little confusing because we read using a different function
	 * depending on whether the socket is ssl or cleartext.
	 */
	while ((gfud->is_ssl && ((len = purple_ssl_read(gfud->ssl_connection, buf, sizeof(buf))) > 0)) ||
			(!gfud->is_ssl && (len = read(source, buf, sizeof(buf))) > 0))
	{
		if(gfud->max_len != -1 && (gfud->len + len) > gfud->max_len) {
			purple_util_fetch_url_error(gfud, _("Error reading from %s: response too long (%d bytes limit)"),
						    gfud->website.address, gfud->max_len);
			return;
		}

		/* If we've filled up our buffer, make it bigger */
		if((gfud->len + len) >= gfud->data_len) {
			while((gfud->len + len) >= gfud->data_len)
				gfud->data_len += sizeof(buf);

			gfud->webdata = g_realloc(gfud->webdata, gfud->data_len);
		}

		data_cursor = gfud->webdata + gfud->len;

		gfud->len += len;

		memcpy(data_cursor, buf, len);

		gfud->webdata[gfud->len] = '\0';

		if(!gfud->got_headers) {
			char *end_of_headers;

			/* See if we've reached the end of the headers yet */
			end_of_headers = strstr(gfud->webdata, "\r\n\r\n");
			if (end_of_headers) {
				char *new_data;
				guint header_len = (end_of_headers + 4 - gfud->webdata);
				size_t content_len;

				purple_debug_misc("util", "Response headers: '%.*s'\n",
					header_len, gfud->webdata);

				/* See if we can find a redirect. */
				if(parse_redirect(gfud->webdata, header_len, gfud))
					return;

				gfud->got_headers = TRUE;

				/* No redirect. See if we can find a content length. */
				content_len = parse_content_len(gfud->webdata, header_len);
				gfud->chunked = content_is_chunked(gfud->webdata, header_len);

				if (content_len == 0) {
					/* We'll stick with an initial 8192 */
					content_len = 8192;
				} else {
					gfud->has_explicit_data_len = TRUE;
				}


				/* If we're returning the headers too, we don't need to clean them out */
				if (gfud->include_headers) {
					gfud->data_len = content_len + header_len;
					gfud->webdata = g_realloc(gfud->webdata, gfud->data_len);
				} else {
					size_t body_len = gfud->len - header_len;

					content_len = MAX(content_len, body_len);

					new_data = g_try_malloc(content_len);
					if (new_data == NULL) {
						purple_debug_error("util",
								"Failed to allocate %" G_GSIZE_FORMAT " bytes: %s\n",
								content_len, g_strerror(errno));
						purple_util_fetch_url_error(gfud,
								_("Unable to allocate enough memory to hold "
								  "the contents from %s.  The web server may "
								  "be trying something malicious."),
								gfud->website.address);

						return;
					}

					/* We may have read part of the body when reading the headers, don't lose it */
					if (body_len > 0) {
						memcpy(new_data, end_of_headers + 4, body_len);
					}

					/* Out with the old... */
					g_free(gfud->webdata);

					/* In with the new. */
					gfud->len = body_len;
					gfud->data_len = content_len;
					gfud->webdata = new_data;
				}
			}
		}

		if(gfud->has_explicit_data_len && gfud->len >= gfud->data_len) {
			got_eof = TRUE;
			break;
		}
	}

	if(len < 0) {
		if(errno == EAGAIN) {
			return;
		} else {
			purple_util_fetch_url_error(gfud, _("Error reading from %s: %s"),
					gfud->website.address, g_strerror(errno));
			return;
		}
	}

	if((len == 0) || got_eof) {
		gfud->webdata = g_realloc(gfud->webdata, gfud->len + 1);
		gfud->webdata[gfud->len] = '\0';

		if (!gfud->include_headers && gfud->chunked) {
			/* Process only if we don't want the headers. */
			process_chunked_data(gfud->webdata, &gfud->len);
		}

		gfud->callback(gfud, gfud->user_data, gfud->webdata, gfud->len, NULL);
		purple_util_fetch_url_cancel(gfud);
	}
}

static void ssl_url_fetch_recv_cb(gpointer data, PurpleSslConnection *ssl_connection, PurpleInputCondition cond)
{
	url_fetch_recv_cb(data, -1, cond);
}

/**
 * This function is called when the socket is available to be written
 * to.
 *
 * @param source The file descriptor that can be written to.  This can
 *        be an http connection or it can be the SSL connection of an
 *        https request.  So be careful what you use it for!  If it's
 *        an https request then use purple_ssl_write() instead of
 *        writing to it directly.
 */
static void
url_fetch_send_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	PurpleUtilFetchUrlData *gfud;
	int len, total_len;

	gfud = data;

	if (gfud->request == NULL) {

		PurpleProxyInfo *gpi = purple_proxy_get_setup(gfud->account);
		GString *request_str = g_string_new(NULL);

		g_string_append_printf(request_str, "GET %s%s HTTP/%s\r\n"
						    "Connection: close\r\n",
			(gfud->full ? "" : "/"),
			(gfud->full ? (gfud->url ? gfud->url : "") : (gfud->website.page ? gfud->website.page : "")),
			(gfud->http11 ? "1.1" : "1.0"));

		if (gfud->user_agent)
			g_string_append_printf(request_str, "User-Agent: %s\r\n", gfud->user_agent);

		/* Host header is not forbidden in HTTP/1.0 requests, and HTTP/1.1
		 * clients must know how to handle the "chunked" transfer encoding.
		 * Purple doesn't know how to handle "chunked", so should always send
		 * the Host header regardless, to get around some observed problems
		 */
		g_string_append_printf(request_str, "Accept: */*\r\n"
						    "Host: %s\r\n",
			(gfud->website.address ? gfud->website.address : ""));

		if (purple_proxy_info_get_username(gpi) != NULL
				&& (purple_proxy_info_get_type(gpi) == PURPLE_PROXY_USE_ENVVAR
					|| purple_proxy_info_get_type(gpi) == PURPLE_PROXY_HTTP)) {
			/* This chunk of code was copied from proxy.c http_start_connect_tunneling()
			 * This is really a temporary hack - we need a more complete proxy handling solution,
			 * so I didn't think it was worthwhile to refactor for reuse
			 */
			char *t1, *t2, *ntlm_type1;
			char hostname[256];
			int ret;

			ret = gethostname(hostname, sizeof(hostname));
			hostname[sizeof(hostname) - 1] = '\0';
			if (ret < 0 || hostname[0] == '\0') {
				purple_debug_warning("util", "proxy - gethostname() failed -- is your hostname set?");
				strcpy(hostname, "localhost");
			}

			t1 = g_strdup_printf("%s:%s",
				purple_proxy_info_get_username(gpi),
				purple_proxy_info_get_password(gpi) ?
					purple_proxy_info_get_password(gpi) : "");
			t2 = purple_base64_encode((const guchar *)t1, strlen(t1));
			g_free(t1);

			ntlm_type1 = purple_ntlm_gen_type1(hostname, "");

			g_string_append_printf(request_str,
				"Proxy-Authorization: Basic %s\r\n"
				"Proxy-Authorization: NTLM %s\r\n"
				"Proxy-Connection: Keep-Alive\r\n",
				t2, ntlm_type1);
			g_free(ntlm_type1);
			g_free(t2);
		}

		g_string_append(request_str, "\r\n");

		gfud->request = g_string_free(request_str, FALSE);
	}

	if(purple_debug_is_unsafe())
		purple_debug_misc("util", "Request: '%s'\n", gfud->request);
	else
		purple_debug_misc("util", "request constructed\n");

	total_len = strlen(gfud->request);

	if (gfud->is_ssl)
		len = purple_ssl_write(gfud->ssl_connection, gfud->request + gfud->request_written,
				total_len - gfud->request_written);
	else
		len = write(gfud->fd, gfud->request + gfud->request_written,
				total_len - gfud->request_written);

	if (len < 0 && errno == EAGAIN)
		return;
	else if (len < 0) {
		purple_util_fetch_url_error(gfud, _("Error writing to %s: %s"),
				gfud->website.address, g_strerror(errno));
		return;
	}
	gfud->request_written += len;

	if (gfud->request_written < total_len)
		return;

	/* We're done writing our request, now start reading the response */
	if (gfud->is_ssl) {
		purple_input_remove(gfud->inpa);
		gfud->inpa = 0;
		purple_ssl_input_add(gfud->ssl_connection, ssl_url_fetch_recv_cb, gfud);
	} else {
		purple_input_remove(gfud->inpa);
		gfud->inpa = purple_input_add(gfud->fd, PURPLE_INPUT_READ, url_fetch_recv_cb,
			gfud);
	}
}

static void
url_fetch_connect_cb(gpointer url_data, gint source, const gchar *error_message)
{
	PurpleUtilFetchUrlData *gfud;

	gfud = url_data;
	gfud->connect_data = NULL;

	if (source == -1)
	{
		purple_util_fetch_url_error(gfud, _("Unable to connect to %s: %s"),
				(gfud->website.address ? gfud->website.address : ""), error_message);
		return;
	}

	gfud->fd = source;

	gfud->inpa = purple_input_add(source, PURPLE_INPUT_WRITE,
								url_fetch_send_cb, gfud);
	url_fetch_send_cb(gfud, source, PURPLE_INPUT_WRITE);
}

static void ssl_url_fetch_connect_cb(gpointer data, PurpleSslConnection *ssl_connection, PurpleInputCondition cond)
{
	PurpleUtilFetchUrlData *gfud;

	gfud = data;

	gfud->inpa = purple_input_add(ssl_connection->fd, PURPLE_INPUT_WRITE,
			url_fetch_send_cb, gfud);
	url_fetch_send_cb(gfud, ssl_connection->fd, PURPLE_INPUT_WRITE);
}

static void ssl_url_fetch_error_cb(PurpleSslConnection *ssl_connection, PurpleSslErrorType error, gpointer data)
{
	PurpleUtilFetchUrlData *gfud;

	gfud = data;
	gfud->ssl_connection = NULL;

	purple_util_fetch_url_error(gfud, _("Unable to connect to %s: %s"),
			(gfud->website.address ? gfud->website.address : ""),
	purple_ssl_strerror(error));
}

PurpleUtilFetchUrlData *
purple_util_fetch_url_request(PurpleAccount *account,
		const char *url, gboolean full,	const char *user_agent, gboolean http11,
		const char *request, gboolean include_headers, gssize max_len,
		PurpleUtilFetchUrlCallback callback, void *user_data)
{
	PurpleUtilFetchUrlData *gfud;

	g_return_val_if_fail(url      != NULL, NULL);
	g_return_val_if_fail(callback != NULL, NULL);

	if(purple_debug_is_unsafe())
		purple_debug_info("util",
				 "requested to fetch (%s), full=%d, user_agent=(%s), http11=%d\n",
				 url, full, user_agent?user_agent:"(null)", http11);
	else
		purple_debug_info("util", "requesting to fetch a URL\n");

	gfud = g_new0(PurpleUtilFetchUrlData, 1);

	gfud->callback = callback;
	gfud->user_data  = user_data;
	gfud->url = g_strdup(url);
	gfud->user_agent = g_strdup(user_agent);
	gfud->http11 = http11;
	gfud->full = full;
	gfud->request = g_strdup(request);
	gfud->include_headers = include_headers;
	gfud->fd = -1;
	gfud->max_len = max_len;
	gfud->account = account;

	purple_url_parse(url, &gfud->website.address, &gfud->website.port,
				   &gfud->website.page, &gfud->website.user, &gfud->website.passwd);

	if (purple_strcasestr(url, "https://") != NULL) {
		if (!purple_ssl_is_supported()) {
			purple_util_fetch_url_error(gfud,
					_("Unable to connect to %s: %s"),
					gfud->website.address,
					_("Server requires TLS/SSL, but no TLS/SSL support was found."));
			return NULL;
		}

		gfud->is_ssl = TRUE;
		gfud->ssl_connection = purple_ssl_connect(account,
				gfud->website.address, gfud->website.port,
				ssl_url_fetch_connect_cb, ssl_url_fetch_error_cb, gfud);
	} else {
		gfud->connect_data = purple_proxy_connect(NULL, account,
				gfud->website.address, gfud->website.port,
				url_fetch_connect_cb, gfud);
	}

	if (gfud->ssl_connection == NULL && gfud->connect_data == NULL)
	{
		purple_util_fetch_url_error(gfud, _("Unable to connect to %s"),
				gfud->website.address);
		return NULL;
	}

	return gfud;
}

void
purple_util_fetch_url_cancel(PurpleUtilFetchUrlData *gfud)
{
	if (gfud->ssl_connection != NULL)
		purple_ssl_close(gfud->ssl_connection);

	if (gfud->connect_data != NULL)
		purple_proxy_connect_cancel(gfud->connect_data);

	if (gfud->inpa > 0)
		purple_input_remove(gfud->inpa);

	if (gfud->fd >= 0)
		close(gfud->fd);

	g_free(gfud->website.user);
	g_free(gfud->website.passwd);
	g_free(gfud->website.address);
	g_free(gfud->website.page);
	g_free(gfud->url);
	g_free(gfud->user_agent);
	g_free(gfud->request);
	g_free(gfud->webdata);

	g_free(gfud);
}
