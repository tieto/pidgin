/*
 * Purple's oscar protocol plugin
 * This file is the legal property of its developers.
 * Please see the AUTHORS file distributed alongside this file.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
*/

/**
 * This file implements AIM's clientLogin procedure for authenticating
 * users.  This replaces the older MD5-based and XOR-based
 * authentication methods that use SNAC family 0x0017.
 *
 * This doesn't use SNACs or FLAPs at all.  It makes http and https
 * POSTs to AOL to validate the user based on the password they
 * provided to us.  Upon successful authentication we request a
 * connection to the BOS server by calling startOSCARsession.  The
 * AOL server gives us the hostname and port number to use, as well
 * as the cookie to use to authenticate to the BOS server.  And then
 * everything else is the same as with BUCP.
 *
 * For details, see:
 * http://dev.aol.com/aim/oscar/#AUTH
 * http://dev.aol.com/authentication_for_clients
 */

#include "cipher.h"
#include "core.h"

#include "oscar.h"

#define URL_CLIENT_LOGIN "https://api.screenname.aol.com/auth/clientLogin"
#define URL_START_OSCAR_SESSION "http://api.oscar.aol.com/aim/startOSCARSession"

/*
 * Using clientLogin requires a developer ID.  This key is for libpurple.
 * It is the default key for all libpurple-based clients.  AOL encourages
 * UIs (especially ones with lots of users) to override this with their
 * own key.  This key is owned by the AIM account "markdoliner"
 *
 * Keys can be managed at http://developer.aim.com/manageKeys.jsp
 */
#define DEFAULT_CLIENT_KEY "ma15d7JTxbmVG-RP"

static const char *get_client_key(OscarData *od)
{
	return oscar_get_ui_info_string(
			od->icq ? "prpl-icq-clientkey" : "prpl-aim-clientkey",
			DEFAULT_CLIENT_KEY);
}

/**
 * This is similar to purple_url_encode() except that it follows
 * RFC3986 a little more closely by not encoding - . _ and ~
 * It also uses capital letters as hex characters because capital
 * letters are required by AOL.  The RFC says that capital letters
 * are a SHOULD and that URLs that use capital letters are
 * equivalent to URLs that use small letters.
 *
 * TODO: Check if purple_url_encode() can be replaced with this
 *       version without breaking anything.
 */
static const char *oscar_auth_url_encode(const char *str)
{
	const char *iter;
	static char buf[BUF_LEN];
	char utf_char[6];
	guint i, j = 0;

	g_return_val_if_fail(str != NULL, NULL);
	g_return_val_if_fail(g_utf8_validate(str, -1, NULL), NULL);

	iter = str;
	for (; *iter && j < (BUF_LEN - 1) ; iter = g_utf8_next_char(iter)) {
		gunichar c = g_utf8_get_char(iter);
		/* If the character is an ASCII character and is alphanumeric
		 * no need to escape */
		if ((c < 128 && isalnum(c)) || c =='-' || c == '.' || c == '_' || c == '~') {
			buf[j++] = c;
		} else {
			int bytes = g_unichar_to_utf8(c, utf_char);
			for (i = 0; i < bytes; i++) {
				if (j > (BUF_LEN - 4))
					break;
				sprintf(buf + j, "%%%02X", utf_char[i] & 0xff);
				j += 3;
			}
		}
	}

	buf[j] = '\0';

	return buf;
}

/**
 * @return A null-terminated base64 encoded version of the HMAC
 *         calculated using the given key and data.
 */
static gchar *hmac_sha256(const char *key, const char *message)
{
	PurpleCipherContext *context;
	guchar digest[32];

	context = purple_cipher_context_new_by_name("hmac", NULL);
	purple_cipher_context_set_option(context, "hash", "sha256");
	purple_cipher_context_set_key(context, (guchar *)key);
	purple_cipher_context_append(context, (guchar *)message, strlen(message));
	purple_cipher_context_digest(context, sizeof(digest), digest, NULL);
	purple_cipher_context_destroy(context);

	return purple_base64_encode(digest, sizeof(digest));
}

/**
 * @return A base-64 encoded HMAC-SHA256 signature created using the
 *         technique documented at
 *         http://dev.aol.com/authentication_for_clients#signing
 */
static gchar *generate_signature(const char *method, const char *url, const char *parameters, const char *session_key)
{
	char *encoded_url, *signature_base_string, *signature;
	const char *encoded_parameters;

	encoded_url = g_strdup(oscar_auth_url_encode(url));
	encoded_parameters = oscar_auth_url_encode(parameters);
	signature_base_string = g_strdup_printf("%s&%s&%s",
			method, encoded_url, encoded_parameters);
	g_free(encoded_url);

	signature = hmac_sha256(session_key, signature_base_string);
	g_free(signature_base_string);

	return signature;
}

static gboolean parse_start_oscar_session_response(PurpleConnection *gc, const gchar *response, gsize response_len, char **host, unsigned short *port, char **cookie)
{
	xmlnode *response_node, *tmp_node, *data_node;
	xmlnode *host_node = NULL, *port_node = NULL, *cookie_node = NULL;
	char *tmp;

	/* Parse the response as XML */
	response_node = xmlnode_from_str(response, response_len);
	if (response_node == NULL)
	{
		purple_debug_error("oscar", "startOSCARSession could not parse "
				"response as XML: %s\n", response);
		purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Received unexpected response from " URL_START_OSCAR_SESSION));
		return FALSE;
	}

	/* Grab the necessary XML nodes */
	tmp_node = xmlnode_get_child(response_node, "statusCode");
	data_node = xmlnode_get_child(response_node, "data");
	if (data_node != NULL) {
		host_node = xmlnode_get_child(data_node, "host");
		port_node = xmlnode_get_child(data_node, "port");
		cookie_node = xmlnode_get_child(data_node, "cookie");
	}

	/* Make sure we have a status code */
	if (tmp_node == NULL || (tmp = xmlnode_get_data_unescaped(tmp_node)) == NULL) {
		purple_debug_error("oscar", "startOSCARSession response was "
				"missing statusCode: %s\n", response);
		purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Received unexpected response from " URL_START_OSCAR_SESSION));
		xmlnode_free(response_node);
		return FALSE;
	}

	/* Make sure the status code was 200 */
	if (strcmp(tmp, "200") != 0)
	{
		purple_debug_error("oscar", "startOSCARSession response statusCode "
				"was %s: %s\n", tmp, response);

		if (strcmp(tmp, "401") == 0)
			purple_connection_error_reason(gc,
					PURPLE_CONNECTION_ERROR_OTHER_ERROR,
					_("You have been connecting and disconnecting too "
					  "frequently. Wait ten minutes and try again. If "
					  "you continue to try, you will need to wait even "
					  "longer."));
		else
			purple_connection_error_reason(gc,
					PURPLE_CONNECTION_ERROR_OTHER_ERROR,
					_("Received unexpected response from " URL_START_OSCAR_SESSION));

		g_free(tmp);
		xmlnode_free(response_node);
		return FALSE;
	}
	g_free(tmp);

	/* Make sure we have everything else */
	if (data_node == NULL || host_node == NULL ||
		port_node == NULL || cookie_node == NULL)
	{
		purple_debug_error("oscar", "startOSCARSession response was missing "
				"something: %s\n", response);
		purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Received unexpected response from " URL_START_OSCAR_SESSION));
		xmlnode_free(response_node);
		return FALSE;
	}

	/* Extract data from the XML */
	*host = xmlnode_get_data_unescaped(host_node);
	tmp = xmlnode_get_data_unescaped(port_node);
	*cookie = xmlnode_get_data_unescaped(cookie_node);
	if (*host == NULL || **host == '\0' || tmp == NULL || *tmp == '\0' || cookie == NULL || *cookie == '\0')
	{
		purple_debug_error("oscar", "startOSCARSession response was missing "
				"something: %s\n", response);
		purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Received unexpected response from " URL_START_OSCAR_SESSION));
		g_free(*host);
		g_free(tmp);
		g_free(*cookie);
		xmlnode_free(response_node);
		return FALSE;
	}

	*port = atoi(tmp);
	g_free(tmp);

	return TRUE;
}

static void start_oscar_session_cb(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
{
	OscarData *od;
	PurpleConnection *gc;
	char *host, *cookie;
	unsigned short port;
	guint8 *cookiedata;
	gsize cookiedata_len;

	od = user_data;
	gc = od->gc;

	od->url_data = NULL;

	if (error_message != NULL || len == 0) {
		gchar *tmp;
		tmp = g_strdup_printf(_("Error requesting " URL_START_OSCAR_SESSION
				": %s"), error_message);
		purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR, tmp);
		g_free(tmp);
		return;
	}

	if (!parse_start_oscar_session_response(gc, url_text, len, &host, &port, &cookie))
		return;

	cookiedata = purple_base64_decode(cookie, &cookiedata_len);
	oscar_connect_to_bos(gc, od, host, port, cookiedata, cookiedata_len);
	g_free(cookiedata);

	g_free(host);
	g_free(cookie);
}

static void send_start_oscar_session(OscarData *od, const char *token, const char *session_key, time_t hosttime)
{
	char *query_string, *signature, *url;

	/* Construct the GET parameters */
	query_string = g_strdup_printf("a=%s"
			"&f=xml"
			"&k=%s"
			"&ts=%" PURPLE_TIME_T_MODIFIER
			"&useTLS=0",
			oscar_auth_url_encode(token), get_client_key(od), hosttime);
	signature = generate_signature("GET", URL_START_OSCAR_SESSION,
			query_string, session_key);
	url = g_strdup_printf(URL_START_OSCAR_SESSION "?%s&sig_sha256=%s",
			query_string, signature);
	g_free(query_string);
	g_free(signature);

	/* Make the request */
	od->url_data = purple_util_fetch_url(url, TRUE, NULL, FALSE,
			start_oscar_session_cb, od);
	g_free(url);
}

/**
 * This function parses the given response from a clientLogin request
 * and extracts the useful information.
 *
 * @param gc           The PurpleConnection.  If the response data does
 *                     not indicate then purple_connection_error_reason()
 *                     will be called to close this connection.
 * @param response     The response data from the clientLogin request.
 * @param response_len The length of the above response, or -1 if
 *                     @response is NUL terminated.
 * @param token        If parsing was successful then this will be set to
 *                     a newly allocated string containing the token.  The
 *                     caller should g_free this string when it is finished
 *                     with it.  On failure this value will be untouched.
 * @param secret       If parsing was successful then this will be set to
 *                     a newly allocated string containing the secret.  The
 *                     caller should g_free this string when it is finished
 *                     with it.  On failure this value will be untouched.
 * @param hosttime     If parsing was successful then this will be set to
 *                     the time on the OpenAuth Server in seconds since the
 *                     Unix epoch.  On failure this value will be untouched.
 *
 * @return TRUE if the request was successful and we were able to
 *         extract all info we need.  Otherwise FALSE.
 */
static gboolean parse_client_login_response(PurpleConnection *gc, const gchar *response, gsize response_len, char **token, char **secret, time_t *hosttime)
{
	xmlnode *response_node, *tmp_node, *data_node;
	xmlnode *secret_node = NULL, *hosttime_node = NULL, *token_node = NULL, *tokena_node = NULL;
	char *tmp;

	/* Parse the response as XML */
	response_node = xmlnode_from_str(response, response_len);
	if (response_node == NULL)
	{
		purple_debug_error("oscar", "clientLogin could not parse "
				"response as XML: %s\n", response);
		purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Received unexpected response from " URL_CLIENT_LOGIN));
		return FALSE;
	}

	/* Grab the necessary XML nodes */
	tmp_node = xmlnode_get_child(response_node, "statusCode");
	data_node = xmlnode_get_child(response_node, "data");
	if (data_node != NULL) {
		secret_node = xmlnode_get_child(data_node, "sessionSecret");
		hosttime_node = xmlnode_get_child(data_node, "hostTime");
		token_node = xmlnode_get_child(data_node, "token");
		if (token_node != NULL)
			tokena_node = xmlnode_get_child(token_node, "a");
	}

	/* Make sure we have a status code */
	if (tmp_node == NULL || (tmp = xmlnode_get_data_unescaped(tmp_node)) == NULL) {
		purple_debug_error("oscar", "clientLogin response was "
				"missing statusCode: %s\n", response);
		purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Received unexpected response from " URL_CLIENT_LOGIN));
		xmlnode_free(response_node);
		return FALSE;
	}

	/* Make sure the status code was 200 */
	if (strcmp(tmp, "200") != 0)
	{
		int status_code, status_detail_code = 0;

		status_code = atoi(tmp);
		g_free(tmp);
		tmp_node = xmlnode_get_child(response_node, "statusDetailCode");
		if (tmp_node != NULL && (tmp = xmlnode_get_data_unescaped(tmp_node)) != NULL) {
			status_detail_code = atoi(tmp);
			g_free(tmp);
		}

		purple_debug_error("oscar", "clientLogin response statusCode "
				"was %d (%d): %s\n", status_code, status_detail_code, response);

		if (status_code == 330 && status_detail_code == 3011) {
			purple_connection_error_reason(gc,
					PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
					_("Incorrect password"));
		} else if (status_code == 401 && status_detail_code == 3019) {
			purple_connection_error_reason(gc,
					PURPLE_CONNECTION_ERROR_OTHER_ERROR,
					_("AOL does not allow your screen name to authenticate here"));
		} else
			purple_connection_error_reason(gc,
					PURPLE_CONNECTION_ERROR_OTHER_ERROR,
					_("Received unexpected response from " URL_CLIENT_LOGIN));

		xmlnode_free(response_node);
		return FALSE;
	}
	g_free(tmp);

	/* Make sure we have everything else */
	if (data_node == NULL || secret_node == NULL ||
		token_node == NULL || tokena_node == NULL)
	{
		purple_debug_error("oscar", "clientLogin response was missing "
				"something: %s\n", response);
		purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Received unexpected response from " URL_CLIENT_LOGIN));
		xmlnode_free(response_node);
		return FALSE;
	}

	/* Extract data from the XML */
	*token = xmlnode_get_data_unescaped(tokena_node);
	*secret = xmlnode_get_data_unescaped(secret_node);
	tmp = xmlnode_get_data_unescaped(hosttime_node);
	if (*token == NULL || **token == '\0' || *secret == NULL || **secret == '\0' || tmp == NULL || *tmp == '\0')
	{
		purple_debug_error("oscar", "clientLogin response was missing "
				"something: %s\n", response);
		purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Received unexpected response from " URL_CLIENT_LOGIN));
		g_free(*token);
		g_free(*secret);
		g_free(tmp);
		xmlnode_free(response_node);
		return FALSE;
	}

	*hosttime = strtol(tmp, NULL, 10);
	g_free(tmp);

	xmlnode_free(response_node);

	return TRUE;
}

static void client_login_cb(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
{
	OscarData *od;
	PurpleConnection *gc;
	char *token, *secret, *session_key;
	time_t hosttime;
	int password_len;
	char *password;

	od = user_data;
	gc = od->gc;

	od->url_data = NULL;

	if (error_message != NULL || len == 0) {
		gchar *tmp;
		tmp = g_strdup_printf(_("Error requesting " URL_CLIENT_LOGIN
				": %s"), error_message);
		purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR, tmp);
		g_free(tmp);
		return;
	}

	if (!parse_client_login_response(gc, url_text, len, &token, &secret, &hosttime))
		return;

	password_len = strlen(purple_connection_get_password(gc));
	password = g_strdup_printf("%.*s",
			od->icq ? MIN(password_len, MAXICQPASSLEN) : password_len,
			purple_connection_get_password(gc));
	session_key = hmac_sha256(password, secret);
	g_free(password);
	g_free(secret);

	send_start_oscar_session(od, token, session_key, hosttime);

	g_free(token);
	g_free(session_key);
}

/**
 * This function sends a request to
 * https://api.screenname.aol.com/auth/clientLogin with the user's
 * username and password and receives the user's session key, which is
 * used to request a connection to the BOSS server.
 */
void send_client_login(OscarData *od, const char *username)
{
	PurpleConnection *gc;
	GString *request, *body;
	const char *tmp;
	char *password;
	int password_len;

	gc = od->gc;

	/*
	 * We truncate ICQ passwords to 8 characters.  There is probably a
	 * limit for AIM passwords, too, but we really only need to do
	 * this for ICQ because older ICQ clients let you enter a password
	 * as long as you wanted and then they truncated it silently.
	 *
	 * And we can truncate based on the number of bytes and not the
	 * number of characters because passwords for AIM and ICQ are
	 * supposed to be plain ASCII (I don't know if this has always been
	 * the case, though).
	 */
	tmp = purple_connection_get_password(gc);
	password_len = strlen(tmp);
	password = g_strndup(tmp, od->icq ? MIN(password_len, MAXICQPASSLEN) : password_len);

	/* Construct the body of the HTTP POST request */
	body = g_string_new("");
	g_string_append_printf(body, "devId=%s", get_client_key(od));
	g_string_append_printf(body, "&f=xml");
	g_string_append_printf(body, "&pwd=%s", oscar_auth_url_encode(password));
	g_string_append_printf(body, "&s=%s", oscar_auth_url_encode(username));
	g_free(password);

	/* Construct an HTTP POST request */
	request = g_string_new("POST /auth/clientLogin HTTP/1.0\r\n"
			"Connection: close\r\n"
			"Accept: */*\r\n");

	/* Tack on the body */
	g_string_append_printf(request, "Content-Type: application/x-www-form-urlencoded; charset=UTF-8\r\n");
	g_string_append_printf(request, "Content-Length: %" G_GSIZE_FORMAT "\r\n\r\n", body->len);
	g_string_append_len(request, body->str, body->len);
	g_string_free(body, TRUE);

	/* Send the POST request  */
	od->url_data = purple_util_fetch_url_request(URL_CLIENT_LOGIN,
			TRUE, NULL, FALSE, request->str, FALSE,
			client_login_cb, od);
	g_string_free(request, TRUE);
}
