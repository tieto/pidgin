/**
 * @file obsolete.h Obsolete code, to be removed
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301 USA
 */
#ifndef _PURPLE_OBSOLETE_H_
#define _PURPLE_OBSOLETE_H_

#include <glib.h>
#include "account.h"

/**************************************************************************/
/** @name URI/URL Functions                                               */
/**************************************************************************/
/*@{*/

/**
  * An opaque structure representing a URL request. Can be used to cancel
  * the request.
  */
typedef struct _PurpleUtilFetchUrlData PurpleUtilFetchUrlData;

/**
 * This is the signature used for functions that act as the callback
 * to purple_util_fetch_url() or purple_util_fetch_url_request().
 *
 * @param url_data      The same value that was returned when you called
 *                      purple_fetch_url() or purple_fetch_url_request().
 * @param user_data     The user data that your code passed into either
 *                      purple_util_fetch_url() or purple_util_fetch_url_request().
 * @param url_text      This will be NULL on error.  Otherwise this
 *                      will contain the contents of the URL.
 * @param len           0 on error, otherwise this is the length of buf.
 * @param error_message If something went wrong then this will contain
 *                      a descriptive error message, and buf will be
 *                      NULL and len will be 0.
 */
typedef void (*PurpleUtilFetchUrlCallback)(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message);

/**
 * Fetches the data from a URL, and passes it to a callback function.
 *
 * @param url        The URL.
 * @param full       TRUE if this is the full URL, or FALSE if it's a
 *                   partial URL.
 * @param user_agent The user agent field to use, or NULL.
 * @param http11     TRUE if HTTP/1.1 should be used to download the file.
 * @param max_len    The maximum number of bytes to retrieve (-1 for unlimited)
 * @param cb         The callback function.
 * @param data       The user data to pass to the callback function.
 */
PurpleUtilFetchUrlData * purple_util_fetch_url(const gchar *url, gboolean full,
	const gchar *user_agent, gboolean http11, gssize max_len,
	PurpleUtilFetchUrlCallback cb, gpointer data);

/**
 * Fetches the data from a URL, and passes it to a callback function.
 *
 * @param account    The account for which the request is needed, or NULL.
 * @param url        The URL.
 * @param full       TRUE if this is the full URL, or FALSE if it's a
 *                   partial URL.
 * @param user_agent The user agent field to use, or NULL.
 * @param http11     TRUE if HTTP/1.1 should be used to download the file.
 * @param request    A HTTP request to send to the server instead of the
 *                   standard GET
 * @param include_headers
 *                   If TRUE, include the HTTP headers in the response.
 * @param max_len    The maximum number of bytes to retrieve (-1 for unlimited)
 * @param callback   The callback function.
 * @param data       The user data to pass to the callback function.
 */
PurpleUtilFetchUrlData *purple_util_fetch_url_request(
		PurpleAccount *account, const gchar *url,
		gboolean full, const gchar *user_agent, gboolean http11,
		const gchar *request, gboolean include_headers, gssize max_len,
		PurpleUtilFetchUrlCallback callback, gpointer data);

/**
 * Cancel a pending URL request started with either
 * purple_util_fetch_url_request() or purple_util_fetch_url().
 *
 * @param url_data The data returned when you initiated the URL fetch.
 */
void purple_util_fetch_url_cancel(PurpleUtilFetchUrlData *url_data);


/*@}*/

#endif /* _PURPLE_OBSOLETE_H_ */
