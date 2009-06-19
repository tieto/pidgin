/*
 * purple
 *
 * Some code copyright 2003 Tim Ringenbach <omarvo@hotmail.com>
 * (marv on irc.freenode.net)
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cipher.h"
#include "debug.h"
#include "internal.h"
#include "prpl.h"
#include "util.h"

#include "yahoo.h"

#include <string.h>

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
	unsigned long len;
	unsigned long data_len;
	gssize max_len;
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
		gfud->ssl_connection = purple_ssl_connect(NULL,
				gfud->website.address, gfud->website.port,
				ssl_url_fetch_connect_cb, ssl_url_fetch_error_cb, gfud);
	} else {
		gfud->connect_data = purple_proxy_connect(NULL, NULL,
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

static size_t
parse_content_len(const char *data, size_t data_len)
{
	size_t content_len = 0;
	const char *p = NULL;

	/* This is still technically wrong, since headers are case-insensitive
	 * [RFC 2616, section 4.2], though this ought to catch the normal case.
	 * Note: data is _not_ nul-terminated.
	 */
	if(data_len > 16) {
		p = (strncmp(data, "Content-Length: ", 16) == 0) ? data : NULL;
		if(!p)
			p = (strncmp(data, "CONTENT-LENGTH: ", 16) == 0)
				? data : NULL;
		if(!p) {
			p = g_strstr_len(data, data_len, "\nContent-Length: ");
			if (p)
				p++;
		}
		if(!p) {
			p = g_strstr_len(data, data_len, "\nCONTENT-LENGTH: ");
			if (p)
				p++;
		}

		if(p)
			p += 16;
	}

	/* If we can find a Content-Length header at all, try to sscanf it.
	 * Response headers should end with at least \r\n, so sscanf is safe,
	 * if we make sure that there is indeed a \n in our header.
	 */
	if (p && g_strstr_len(p, data_len - (p - data), "\n")) {
		sscanf(p, "%" G_GSIZE_FORMAT, &content_len);
		purple_debug_misc("util", "parsed %" G_GSIZE_FORMAT "\n", content_len);
	}

	return content_len;
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
			char *tmp;

			/* See if we've reached the end of the headers yet */
			if((tmp = strstr(gfud->webdata, "\r\n\r\n"))) {
				char * new_data;
				guint header_len = (tmp + 4 - gfud->webdata);
				size_t content_len;

				purple_debug_misc("util", "Response headers: '%.*s'\n",
					header_len, gfud->webdata);

				/* See if we can find a redirect. */
				if(parse_redirect(gfud->webdata, header_len, gfud))
					return;

				gfud->got_headers = TRUE;

				/* No redirect. See if we can find a content length. */
				content_len = parse_content_len(gfud->webdata, header_len);

				if(content_len == 0) {
					/* We'll stick with an initial 8192 */
					content_len = 8192;
				} else {
					gfud->has_explicit_data_len = TRUE;
				}


				/* If we're returning the headers too, we don't need to clean them out */
				if(gfud->include_headers) {
					gfud->data_len = content_len + header_len;
					gfud->webdata = g_realloc(gfud->webdata, gfud->data_len);
				} else {
					size_t body_len = 0;

					if(gfud->len > (header_len + 1))
						body_len = (gfud->len - header_len);

					content_len = MAX(content_len, body_len);

					new_data = g_try_malloc(content_len);
					if(new_data == NULL) {
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
					if(body_len > 0) {
						tmp += 4;
						memcpy(new_data, tmp, body_len);
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

		gfud->callback(gfud, gfud->user_data, gfud->webdata, gfud->len, NULL);
		purple_util_fetch_url_cancel(gfud);
	}
}

static void ssl_url_fetch_recv_cb(gpointer data, PurpleSslConnection *ssl_connection, PurpleInputCondition cond)
{
	url_fetch_recv_cb(data, -1, cond);
}

/*
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

	if (gfud->request == NULL)
	{
		/* Host header is not forbidden in HTTP/1.0 requests, and HTTP/1.1
		 * clients must know how to handle the "chunked" transfer encoding.
		 * Purple doesn't know how to handle "chunked", so should always send
		 * the Host header regardless, to get around some observed problems
		 */
		if (gfud->user_agent) {
			gfud->request = g_strdup_printf(
				"GET %s%s HTTP/%s\r\n"
				"Connection: close\r\n"
				"User-Agent: %s\r\n"
				"Accept: */*\r\n"
				"Host: %s\r\n\r\n",
				(gfud->full ? "" : "/"),
				(gfud->full ? (gfud->url ? gfud->url : "") : (gfud->website.page ? gfud->website.page : "")),
				(gfud->http11 ? "1.1" : "1.0"),
				(gfud->user_agent ? gfud->user_agent : ""),
				(gfud->website.address ? gfud->website.address : ""));
		} else {
			gfud->request = g_strdup_printf(
				"GET %s%s HTTP/%s\r\n"
				"Connection: close\r\n"
				"Accept: */*\r\n"
				"Host: %s\r\n\r\n",
				(gfud->full ? "" : "/"),
				(gfud->full ? (gfud->url ? gfud->url : "") : (gfud->website.page ? gfud->website.page : "")),
				(gfud->http11 ? "1.1" : "1.0"),
				(gfud->website.address ? gfud->website.address : ""));
		}
	}

	if(g_getenv("PURPLE_UNSAFE_DEBUG"))
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
purple_util_fetch_url_request_len_with_account(PurpleAccount *account,
		const char *url, gboolean full,	const char *user_agent, gboolean http11,
		const char *request, gboolean include_headers, gssize max_len,
		PurpleUtilFetchUrlCallback callback, void *user_data)
{
	PurpleUtilFetchUrlData *gfud;

	g_return_val_if_fail(url      != NULL, NULL);
	g_return_val_if_fail(callback != NULL, NULL);

	if(g_getenv("PURPLE_UNSAFE_DEBUG"))
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

	purple_url_parse(url, &gfud->website.address, &gfud->website.port,
				   &gfud->website.page, &gfud->website.user, &gfud->website.passwd);

	if (purple_strcasestr(url, "https://") != NULL) {
		if (!purple_ssl_is_supported()) {
			purple_util_fetch_url_error(gfud,
					_("Unable to connect to %s: Server requires TLS/SSL, but no TLS/SSL support was found."),
					gfud->website.address);
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

gboolean
yahoo_account_use_http_proxy(PurpleConnection *conn)
{
	PurpleProxyInfo *ppi = purple_proxy_get_setup(conn->account);
	return (ppi->type == PURPLE_PROXY_HTTP || ppi->type == PURPLE_PROXY_USE_ENVVAR);
}

/*
 * Returns cookies formatted as a null terminated string for the given connection.
 * Must g_free return value.
 * 
 * TODO:will work, but must test for strict correctness
 */
gchar* yahoo_get_cookies(PurpleConnection *gc)
{
	gchar *ans = NULL;
	gchar *cur;
	char firstflag = 1;
	gchar *t1,*t2,*t3;
	GSList *tmp;
	GSList *cookies;
	cookies = ((struct yahoo_data*)(gc->proto_data))->cookies;
	tmp = cookies;
	while(tmp)
	{
		cur = tmp->data;
		t1 = ans;
		t2 = g_strrstr(cur, ";expires=");
		if(t2 == NULL)
			t2 = g_strrstr(cur, "; expires=");
		if(t2 == NULL)
		{
			if(firstflag)
				ans = g_strdup_printf("%c=%s", cur[0], cur+2);
			else
				ans = g_strdup_printf("%s; %c=%s", t1, cur[0], cur+2);
		}
		else
		{
			t3 = strstr(t2+1, ";");
			if(t3 != NULL)
			{
				t2[0] = '\0';

				if(firstflag)
					ans = g_strdup_printf("%c=%s%s", cur[0], cur+2, t3);
				else
					ans = g_strdup_printf("%s; %c=%s%s", t1, cur[0], cur+2, t3);

				t2[0] = ';';
			}
			else
			{
				t2[0] = '\0';

				if(firstflag)
					ans = g_strdup_printf("%c=%s", cur[0], cur+2);
				else
					ans = g_strdup_printf("%s; %c=%s", t1, cur[0], cur+2);

				t2[0] = ';';
			}
		}
		if(firstflag)
			firstflag = 0;
		else
			g_free(t1);
		tmp = g_slist_next(tmp);
	}
	return ans;
}

/**
 * Encode some text to send to the yahoo server.
 *
 * @param gc The connection handle.
 * @param str The null terminated utf8 string to encode.
 * @param utf8 If not @c NULL, whether utf8 is okay or not.
 *             Even if it is okay, we may not use it. If we
 *             used it, we set this to @c TRUE, else to
 *             @c FALSE. If @c NULL, false is assumed, and
 *             it is not dereferenced.
 * @return The g_malloced string in the appropriate encoding.
 */
char *yahoo_string_encode(PurpleConnection *gc, const char *str, gboolean *utf8)
{
	struct yahoo_data *yd = gc->proto_data;
	char *ret;
	const char *to_codeset;

	if (yd->jp)
		return g_strdup(str);

	if (utf8 && *utf8) /* FIXME: maybe don't use utf8 if it'll fit in latin1 */
		return g_strdup(str);

	to_codeset = purple_account_get_string(purple_connection_get_account(gc), "local_charset",  "ISO-8859-1");
	ret = g_convert_with_fallback(str, -1, to_codeset, "UTF-8", "?", NULL, NULL, NULL);

	if (ret)
		return ret;
	else
		return g_strdup("");
}

/**
 * Decode some text received from the server.
 *
 * @param gc The gc handle.
 * @param str The null terminated string to decode.
 * @param utf8 Did the server tell us it was supposed to be utf8?
 * @return The decoded, utf-8 string, which must be g_free()'d.
 */
char *yahoo_string_decode(PurpleConnection *gc, const char *str, gboolean utf8)
{
	struct yahoo_data *yd = gc->proto_data;
	char *ret;
	const char *from_codeset;

	if (utf8) {
		if (g_utf8_validate(str, -1, NULL))
			return g_strdup(str);
	}

	if (yd->jp)
		from_codeset = "SHIFT_JIS";
	else
		from_codeset = purple_account_get_string(purple_connection_get_account(gc), "local_charset",  "ISO-8859-1");

	ret = g_convert_with_fallback(str, -1, "UTF-8", from_codeset, NULL, NULL, NULL, NULL);

	if (ret)
		return ret;
	else
		return g_strdup("");
}

char *yahoo_convert_to_numeric(const char *str)
{
	GString *gstr = NULL;
	char *retstr;
	const unsigned char *p;

	gstr = g_string_sized_new(strlen(str) * 6 + 1);

	for (p = (unsigned char *)str; *p; p++) {
		g_string_append_printf(gstr, "&#%u;", *p);
	}

	retstr = gstr->str;

	g_string_free(gstr, FALSE);

	return retstr;
}

/*
 * I found these on some website but i don't know that they actually
 * work (or are supposed to work). I didn't implement them yet.
 * 
     * [0;30m ---black
     * [1;37m ---white
     * [0;37m ---tan
     * [0;38m ---light black
     * [1;39m ---dark blue
     * [0;32m ---green
     * [0;33m ---yellow
     * [0;35m ---pink
     * [1;35m ---purple
     * [1;30m ---light blue
     * [0;31m ---red
     * [0;34m ---blue
     * [0;36m ---aqua
         * (shift+comma)lyellow(shift+period) ---light yellow
     * (shift+comma)lgreen(shift+period) ---light green
[2;30m <--white out
*/

static GHashTable *ht = NULL;

void yahoo_init_colorht()
{
	ht = g_hash_table_new(g_str_hash, g_str_equal);
/* the numbers in comments are what gyach uses, but i think they're incorrect */
	g_hash_table_insert(ht, "30", "<FONT COLOR=\"#000000\">"); /* black */
	g_hash_table_insert(ht, "31", "<FONT COLOR=\"#0000FF\">"); /* blue */
	g_hash_table_insert(ht, "32", "<FONT COLOR=\"#008080\">"); /* cyan */      /* 00b2b2 */
	g_hash_table_insert(ht, "33", "<FONT COLOR=\"#808080\">"); /* gray */      /* 808080 */
	g_hash_table_insert(ht, "34", "<FONT COLOR=\"#008000\">"); /* green */     /* 00c200 */
	g_hash_table_insert(ht, "35", "<FONT COLOR=\"#FF0080\">"); /* pink */      /* ffafaf */
	g_hash_table_insert(ht, "36", "<FONT COLOR=\"#800080\">"); /* purple */    /* b200b2 */
	g_hash_table_insert(ht, "37", "<FONT COLOR=\"#FF8000\">"); /* orange */    /* ffff00 */
	g_hash_table_insert(ht, "38", "<FONT COLOR=\"#FF0000\">"); /* red */
	g_hash_table_insert(ht, "39", "<FONT COLOR=\"#808000\">"); /* olive */     /* 546b50 */

	g_hash_table_insert(ht,  "1",  "<B>");
	g_hash_table_insert(ht, "x1", "</B>");
	g_hash_table_insert(ht,  "2",  "<I>");
	g_hash_table_insert(ht, "x2", "</I>");
	g_hash_table_insert(ht,  "4",  "<U>");
	g_hash_table_insert(ht, "x4", "</U>");

	/* these just tell us the text they surround is supposed
	 * to be a link. purple figures that out on its own so we
	 * just ignore it.
	 */
	g_hash_table_insert(ht, "l", ""); /* link start */
	g_hash_table_insert(ht, "xl", ""); /* link end */

	g_hash_table_insert(ht, "<black>",  "<FONT COLOR=\"#000000\">");
	g_hash_table_insert(ht, "<blue>",   "<FONT COLOR=\"#0000FF\">");
	g_hash_table_insert(ht, "<cyan>",   "<FONT COLOR=\"#008284\">");
	g_hash_table_insert(ht, "<gray>",   "<FONT COLOR=\"#848284\">");
	g_hash_table_insert(ht, "<green>",  "<FONT COLOR=\"#008200\">");
	g_hash_table_insert(ht, "<pink>",   "<FONT COLOR=\"#FF0084\">");
	g_hash_table_insert(ht, "<purple>", "<FONT COLOR=\"#840084\">");
	g_hash_table_insert(ht, "<orange>", "<FONT COLOR=\"#FF8000\">");
	g_hash_table_insert(ht, "<red>",    "<FONT COLOR=\"#FF0000\">");
	g_hash_table_insert(ht, "<yellow>", "<FONT COLOR=\"#848200\">");

	g_hash_table_insert(ht, "</black>",  "</FONT>");
	g_hash_table_insert(ht, "</blue>",   "</FONT>");
	g_hash_table_insert(ht, "</cyan>",   "</FONT>");
	g_hash_table_insert(ht, "</gray>",   "</FONT>");
	g_hash_table_insert(ht, "</green>",  "</FONT>");
	g_hash_table_insert(ht, "</pink>",   "</FONT>");
	g_hash_table_insert(ht, "</purple>", "</FONT>");
	g_hash_table_insert(ht, "</orange>", "</FONT>");
	g_hash_table_insert(ht, "</red>",    "</FONT>");
	g_hash_table_insert(ht, "</yellow>", "</FONT>");

	/* remove these once we have proper support for <FADE> and <ALT> */
	g_hash_table_insert(ht, "</fade>", "");
	g_hash_table_insert(ht, "</alt>", "");

	/* these are the normal html yahoo sends (besides <font>).
	 * anything else will get turned into &lt;tag&gt;, so if I forgot
	 * about something, please add it. Why Yahoo! has to send unescaped
	 * <'s and >'s that aren't supposed to be html is beyond me.
	 */
	g_hash_table_insert(ht, "<b>", "<b>");
	g_hash_table_insert(ht, "<i>", "<i>");
	g_hash_table_insert(ht, "<u>", "<u>");

	g_hash_table_insert(ht, "</b>", "</b>");
	g_hash_table_insert(ht, "</i>", "</i>");
	g_hash_table_insert(ht, "</u>", "</u>");
	g_hash_table_insert(ht, "</font>", "</font>");
}

void yahoo_dest_colorht()
{
	g_hash_table_destroy(ht);
}

static int point_to_html(int x)
{
	if (x < 9)
		return 1;
	if (x < 11)
		return 2;
	if (x < 13)
		return 3;
	if (x < 17)
		return 4;
	if (x < 25)
		return 5;
	if (x < 35)
		return 6;
	return 7;
}

/* The Yahoo size tag is actually an absz tag; convert it to an HTML size, and include both tags */
static void _font_tags_fix_size(GString *tag, GString *dest)
{
	char *x, *end;
	int size;

	if (((x = strstr(tag->str, "size"))) && ((x = strchr(x, '=')))) {
		while (*x && !g_ascii_isdigit(*x))
			x++;
		if (*x) {
			int htmlsize;

			size = strtol(x, &end, 10);
			htmlsize = point_to_html(size);
			g_string_append_len(dest, tag->str, x - tag->str);
			g_string_append_printf(dest, "%d", htmlsize);
			g_string_append_printf(dest, "\" absz=\"%d", size);
			g_string_append(dest, end);
		} else {
			g_string_append(dest, tag->str);
			return;
		}
	} else {
		g_string_append(dest, tag->str);
		return;
	}
}

char *yahoo_codes_to_html(const char *x)
{
	GString *s, *tmp;
	int i, j, xs, nomoreendtags = 0; /* s/endtags/closinganglebrackets */
	char *match, *ret;

	s = g_string_sized_new(strlen(x));

	for (i = 0, xs = strlen(x); i < xs; i++) {
		if ((x[i] == 0x1b) && (x[i+1] == '[')) {
			j = i + 1;

			while (j++ < xs) {
				if (x[j] != 'm')
					continue;
				else {
					tmp = g_string_new_len(x + i + 2, j - i - 2);
					if (tmp->str[0] == '#')
						g_string_append_printf(s, "<FONT COLOR=\"%s\">", tmp->str);
					else if ((match = (char *) g_hash_table_lookup(ht, tmp->str)))
						g_string_append(s, match);
					else {
						purple_debug(PURPLE_DEBUG_ERROR, "yahoo",
							"Unknown ansi code 'ESC[%sm'.\n", tmp->str);
						g_string_free(tmp, TRUE);
						break;
					}

					i = j;
					g_string_free(tmp, TRUE);
					break;
				}
			}

		} else if (!nomoreendtags && (x[i] == '<')) {
			j = i;

			while (j++ < xs) {
				if (x[j] != '>')
					if (j == xs) {
						g_string_append(s, "&lt;");
						nomoreendtags = 1;
					}
					else
						continue;
				else {
					tmp = g_string_new_len(x + i, j - i + 1);
					g_string_ascii_down(tmp);

					if ((match = (char *) g_hash_table_lookup(ht, tmp->str)))
						g_string_append(s, match);
					else if (!strncmp(tmp->str, "<fade ", 6) ||
						!strncmp(tmp->str, "<alt ", 5) ||
						!strncmp(tmp->str, "<snd ", 5)) {

						/* remove this if gtkimhtml ever supports any of these */
						i = j;
						g_string_free(tmp, TRUE);
						break;

					} else if (!strncmp(tmp->str, "<font ", 6)) {
						_font_tags_fix_size(tmp, s);
					} else {
						g_string_append(s, "&lt;");
						g_string_free(tmp, TRUE);
						break;
					}

					i = j;
					g_string_free(tmp, TRUE);
					break;
				}

			}

		} else {
			if (x[i] == '<')
				g_string_append(s, "&lt;");
			else if (x[i] == '>')
				g_string_append(s, "&gt;");
			else if (x[i] == '&')
				g_string_append(s, "&amp;");
			else if (x[i] == '"')
				g_string_append(s, "&quot;");
			else
				g_string_append_c(s, x[i]);
		}
	}

	ret = s->str;
	g_string_free(s, FALSE);
	purple_debug(PURPLE_DEBUG_MISC, "yahoo", "yahoo_codes_to_html:  Returning string: '%s'.\n", ret);
	return ret;
}

/* borrowed from gtkimhtml */
#define MAX_FONT_SIZE 7
#define POINT_SIZE(x) (_point_sizes [MIN ((x > 0 ? x : 1), MAX_FONT_SIZE) - 1])
static const gint _point_sizes [] = { 8, 10, 12, 14, 20, 30, 40 };

enum fatype { size, color, face, junk };
typedef struct {
	enum fatype type;
	union {
		int size;
		char *color;
		char *face;
		char *junk;
	} u;
} fontattr;

static void fontattr_free(fontattr *f)
{
	if (f->type == color)
		g_free(f->u.color);
	else if (f->type == face)
		g_free(f->u.face);
	g_free(f);
}

static void yahoo_htc_queue_cleanup(GQueue *q)
{
	char *tmp;

	while ((tmp = g_queue_pop_tail(q)))
		g_free(tmp);
	g_queue_free(q);
}

static void _parse_font_tag(const char *src, GString *dest, int *i, int *j,
				int len, GQueue *colors, GQueue *tags, GQueue *ftattr)
{

	int m, n, vstart;
	gboolean quote = 0, done = 0;

	m = *j;

	while (1) {
		m++;

		if (m >= len) {
			g_string_append(dest, &src[*i]);
			*i = len;
			break;
		}

		if (src[m] == '=') {
			n = vstart = m;
			while (1) {
				n++;

				if (n >= len) {
					m = n;
					break;
				}

				if (src[n] == '"') {
					if (!quote) {
						quote = 1;
						vstart = n;
						continue;
					} else {
						done = 1;
					}
				}

				if (!quote && ((src[n] == ' ') || (src[n] == '>')))
					done = 1;

				if (done) {
					if (!g_ascii_strncasecmp(&src[*j+1], "FACE", m - *j - 1)) {
						fontattr *f;

						f = g_new(fontattr, 1);
						f->type = face;
						f->u.face = g_strndup(&src[vstart+1], n-vstart-1);
						if (!ftattr)
							ftattr = g_queue_new();
						g_queue_push_tail(ftattr, f);
						m = n;
						break;
					} else if (!g_ascii_strncasecmp(&src[*j+1], "SIZE", m - *j - 1)) {
						fontattr *f;

						f = g_new(fontattr, 1);
						f->type = size;
						f->u.size = POINT_SIZE(strtol(&src[vstart+1], NULL, 10));
						if (!ftattr)
							ftattr = g_queue_new();
						g_queue_push_tail(ftattr, f);
						m = n;
						break;
					} else if (!g_ascii_strncasecmp(&src[*j+1], "COLOR", m - *j - 1)) {
						fontattr *f;

						f = g_new(fontattr, 1);
						f->type = color;
						f->u.color = g_strndup(&src[vstart+1], n-vstart-1);
						if (!ftattr)
							ftattr = g_queue_new();
						g_queue_push_head(ftattr, f);
						m = n;
						break;
					} else {
						fontattr *f;

						f = g_new(fontattr, 1);
						f->type = junk;
						f->u.junk = g_strndup(&src[*j+1], n-*j);
						if (!ftattr)
							ftattr = g_queue_new();
						g_queue_push_tail(ftattr, f);
						m = n;
						break;
					}

				}
			}
		}

		if (src[m] == ' ')
			*j = m;

		if (src[m] == '>') {
			gboolean needendtag = 0;
			fontattr *f;
			GString *tmp = g_string_new(NULL);
			char *colorstr;

			if (!g_queue_is_empty(ftattr)) {
				while ((f = g_queue_pop_tail(ftattr))) {
					switch (f->type) {
					case size:
						if (!needendtag) {
							needendtag = 1;
							g_string_append(dest, "<font ");
						}

						g_string_append_printf(dest, "size=\"%d\" ", f->u.size);
						fontattr_free(f);
						break;
					case face:
						if (!needendtag) {
							needendtag = 1;
							g_string_append(dest, "<font ");
						}

						g_string_append_printf(dest, "face=\"%s\" ", f->u.face);
						fontattr_free(f);
						break;
					case junk:
						if (!needendtag) {
							needendtag = 1;
							g_string_append(dest, "<font ");
						}

						g_string_append(dest, f->u.junk);
						fontattr_free(f);
						break;

					case color:
						if (needendtag) {
							g_string_append(tmp, "</font>");
							dest->str[dest->len-1] = '>';
							needendtag = 0;
						}

						colorstr = g_queue_peek_tail(colors);
						g_string_append(tmp, colorstr ? colorstr : "\033[#000000m");
						g_string_append_printf(dest, "\033[%sm", f->u.color);
						g_queue_push_tail(colors, g_strdup_printf("\033[%sm", f->u.color));
						fontattr_free(f);
						break;
					}
				}

				g_queue_free(ftattr);
				ftattr = NULL;

				if (needendtag) {
					dest->str[dest->len-1] = '>';
					g_queue_push_tail(tags, g_strdup("</font>"));
					g_string_free(tmp, TRUE);
				} else {
					g_queue_push_tail(tags, tmp->str);
					g_string_free(tmp, FALSE);
				}
			}

			*i = *j = m;
			break;
		}
	}

}

char *yahoo_html_to_codes(const char *src)
{
	int i, j, len;
	GString *dest;
	char *ret, *esc;
	GQueue *colors, *tags;
	GQueue *ftattr = NULL;
	gboolean no_more_specials = FALSE;

	colors = g_queue_new();
	tags = g_queue_new();
	dest = g_string_sized_new(strlen(src));

	for (i = 0, len = strlen(src); i < len; i++) {

		if (!no_more_specials && src[i] == '<') {
			j = i;

			while (1) {
				j++;

				if (j >= len) { /* no '>' */
					g_string_append_c(dest, src[i]);
					no_more_specials = TRUE;
					break;
				}

				if (src[j] == '<') {
					/* FIXME: This doesn't convert outgoing entities.
					 *        However, I suspect this case may never
					 *        happen anymore because of the entities.
					 */
					g_string_append_len(dest, &src[i], j - i);
					i = j - 1;
					if (ftattr) {
						fontattr *f;

						while ((f = g_queue_pop_head(ftattr)))
							fontattr_free(f);
						g_queue_free(ftattr);
						ftattr = NULL;
					}
					break;
				}

				if (src[j] == ' ') {
					if (!g_ascii_strncasecmp(&src[i+1], "BODY", j - i - 1)) {
						char *t = strchr(&src[j], '>');
						if (!t) {
							g_string_append(dest, &src[i]);
							i = len;
							break;
						} else {
							i = t - src;
							break;
						}
					} else if (!g_ascii_strncasecmp(&src[i+1], "A HREF=\"", j - i - 1)) {
						j += 7;
						g_string_append(dest, "\033[lm");
						while (1) {
							g_string_append_c(dest, src[j]);
							if (++j >= len) {
								i = len;
								break;
							}
							if (src[j] == '"') {
								g_string_append(dest, "\033[xlm");
								while (1) {
									if (++j >= len) {
										i = len;
										break;
									}
									if (!g_ascii_strncasecmp(&src[j], "</A>", 4)) {
										j += 3;
										break;
									}
								}
								i = j;
								break;
							}
						}
					} else if (!g_ascii_strncasecmp(&src[i+1], "SPAN", j - i - 1)) { /* drop span tags */
						while (1) {
							if (++j >= len) {
								g_string_append(dest, &src[i]);
								i = len;
								break;
							}
							if (src[j] == '>') {
								i = j;
								break;
							}
						}
					} else if (g_ascii_strncasecmp(&src[i+1], "FONT", j - i - 1)) { /* not interested! */
						while (1) {
							if (++j >= len) {
								g_string_append(dest, &src[i]);
								i = len;
								break;
							}
							if (src[j] == '>') {
								g_string_append_len(dest, &src[i], j - i + 1);
								i = j;
								break;
							}
						}
					} else { /* yay we have a font tag */
						_parse_font_tag(src, dest, &i, &j, len, colors, tags, ftattr);
					}

					break;
				}

				if (src[j] == '>') {
					/* This has some problems like the FIXME for the
					 * '<' case. and like that case, I suspect the case
					 * that this has problems is won't happen anymore anyway.
					 */
					int sublen = j - i - 1;

					if (sublen) {
						if (!g_ascii_strncasecmp(&src[i+1], "B", sublen)) {
							g_string_append(dest, "\033[1m");
						} else if (!g_ascii_strncasecmp(&src[i+1], "/B", sublen)) {
							g_string_append(dest, "\033[x1m");
						} else if (!g_ascii_strncasecmp(&src[i+1], "I", sublen)) {
							g_string_append(dest, "\033[2m");
						} else if (!g_ascii_strncasecmp(&src[i+1], "/I", sublen)) {
							g_string_append(dest, "\033[x2m");
						} else if (!g_ascii_strncasecmp(&src[i+1], "U", sublen)) {
							g_string_append(dest, "\033[4m");
						} else if (!g_ascii_strncasecmp(&src[i+1], "/U", sublen)) {
							g_string_append(dest, "\033[x4m");
						} else if (!g_ascii_strncasecmp(&src[i+1], "/A", sublen)) {
							g_string_append(dest, "\033[xlm");
						} else if (!g_ascii_strncasecmp(&src[i+1], "BR", sublen)) {
							g_string_append_c(dest, '\n');
						} else if (!g_ascii_strncasecmp(&src[i+1], "/BODY", sublen)) {
							/* mmm, </body> tags. *BURP* */
						} else if (!g_ascii_strncasecmp(&src[i+1], "/SPAN", sublen)) {
							/* </span> tags. dangerously close to </spam> */
						} else if (!g_ascii_strncasecmp(&src[i+1], "/FONT", sublen) && g_queue_peek_tail(tags)) {
							char *etag, *cl;

							etag = g_queue_pop_tail(tags);
							if (etag) {
								g_string_append(dest, etag);
								if (!strcmp(etag, "</font>")) {
									cl = g_queue_pop_tail(colors);
									if (cl)
										g_free(cl);
								}
								g_free(etag);
							}
						} else {
							g_string_append_len(dest, &src[i], j - i + 1);
						}
					} else {
						g_string_append_len(dest, &src[i], j - i + 1);
					}

					i = j;
					break;
				}

			}

		} else {
			if (((len - i) >= 4) && !strncmp(&src[i], "&lt;", 4)) {
				g_string_append_c(dest, '<');
				i += 3;
			} else if (((len - i) >= 4) && !strncmp(&src[i], "&gt;", 4)) {
				g_string_append_c(dest, '>');
				i += 3;
			} else if (((len - i) >= 5) && !strncmp(&src[i], "&amp;", 5)) {
				g_string_append_c(dest, '&');
				i += 4;
			} else if (((len - i) >= 6) && !strncmp(&src[i], "&quot;", 6)) {
				g_string_append_c(dest, '"');
				i += 5;
			} else if (((len - i) >= 6) && !strncmp(&src[i], "&apos;", 6)) {
				g_string_append_c(dest, '\'');
				i += 5;
			} else {
				g_string_append_c(dest, src[i]);
			}
		}
	}

	ret = dest->str;
	g_string_free(dest, FALSE);

	esc = g_strescape(ret, NULL);
	purple_debug(PURPLE_DEBUG_MISC, "yahoo", "yahoo_html_to_codes:  Returning string: '%s'.\n", esc);
	g_free(esc);

	yahoo_htc_queue_cleanup(colors);
	yahoo_htc_queue_cleanup(tags);

	return ret;
}
