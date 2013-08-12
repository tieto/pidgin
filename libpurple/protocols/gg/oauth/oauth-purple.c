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

#include "oauth-purple.h"

#include "oauth.h"
#include "../utils.h"
#include "../xml.h"

#include <debug.h>
#include <http.h>

#define GGP_OAUTH_RESPONSE_MAX 10240

typedef struct
{
	PurpleConnection *gc;
	ggp_oauth_request_cb callback;
	gpointer user_data;
	gchar *token;
	gchar *token_secret;
	
	gchar *sign_method, *sign_url;
} ggp_oauth_data;

static void ggp_oauth_data_free(ggp_oauth_data *data);

static void ggp_oauth_request_token_got(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, gpointer user_data);

static void ggp_oauth_authorization_done(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, gpointer user_data);

static void ggp_oauth_access_token_got(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, gpointer user_data);

static void ggp_oauth_data_free(ggp_oauth_data *data)
{
	g_free(data->token);
	g_free(data->token_secret);
	g_free(data->sign_method);
	g_free(data->sign_url);
	g_free(data);
}

void ggp_oauth_request(PurpleConnection *gc, ggp_oauth_request_cb callback,
	gpointer user_data, const gchar *sign_method, const gchar *sign_url)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	PurpleHttpRequest *req;
	char *auth;
	const char *method = "POST";
	const char *url = "http://api.gadu-gadu.pl/request_token";
	ggp_oauth_data *data;

	g_return_if_fail((method == NULL) == (url == NULL));

	purple_debug_misc("gg", "ggp_oauth_request: requesting token...\n");

	auth = gg_oauth_generate_header(method, url,
		purple_account_get_username(account),
		purple_connection_get_password(gc), NULL, NULL);

	data = g_new0(ggp_oauth_data, 1);
	data->gc = gc;
	data->callback = callback;
	data->user_data = user_data;
	data->sign_method = g_strdup(sign_method);
	data->sign_url = g_strdup(sign_url);

	req = purple_http_request_new(url);
	purple_http_request_set_max_len(req, GGP_OAUTH_RESPONSE_MAX);
	purple_http_request_set_method(req, method);
	purple_http_request_header_set(req, "Authorization", auth);
	purple_http_request(gc, req, ggp_oauth_request_token_got, data);
	purple_http_request_unref(req);

	free(auth);
}

static void ggp_oauth_request_token_got(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, gpointer user_data)
{
	ggp_oauth_data *data = user_data;
	PurpleAccount *account;
	PurpleHttpRequest *req;
	xmlnode *xml;
	gchar *request_data;
	gboolean succ = TRUE;
	const gchar *xml_raw;

	if (!PURPLE_CONNECTION_IS_VALID(data->gc))
	{
		ggp_oauth_data_free(data);
		return;
	}
	account = purple_connection_get_account(data->gc);

	if (!purple_http_response_is_successful(response))
	{
		purple_debug_error("gg", "ggp_oauth_request_token_got: "
			"requested token not received\n");
		ggp_oauth_data_free(data);
		return;
	}

	purple_debug_misc("gg", "ggp_oauth_request_token_got: "
		"got request token, doing authorization...\n");

	xml_raw = purple_http_response_get_data(response, NULL);
	xml = xmlnode_from_str(xml_raw, -1);
	if (xml == NULL)
	{
		purple_debug_error("gg", "ggp_oauth_request_token_got: "
			"invalid xml\n");
		ggp_oauth_data_free(data);
		return;
	}

	succ &= ggp_xml_get_string(xml, "oauth_token", &data->token);
	succ &= ggp_xml_get_string(xml, "oauth_token_secret",
		&data->token_secret);
	xmlnode_free(xml);
	if (!succ)
	{
		purple_debug_error("gg", "ggp_oauth_request_token_got: "
			"invalid xml - token is not present\n");
		ggp_oauth_data_free(data);
		return;
	}

	request_data = g_strdup_printf(
		"callback_url=http://www.mojageneracja.pl&request_token=%s&"
		"uin=%s&password=%s", data->token,
		purple_account_get_username(account),
		purple_connection_get_password(data->gc));

	req = purple_http_request_new("https://login.gadu-gadu.pl/authorize");
	purple_http_request_set_max_len(req, GGP_OAUTH_RESPONSE_MAX);
	// we don't need any results, nor 302 redirection
	purple_http_request_set_max_redirects(req, 0);
	purple_http_request_set_method(req, "POST");
	purple_http_request_header_set(req, "Content-Type", "application/x-www-form-urlencoded");
	purple_http_request_set_contents(req, request_data, -1);
	purple_http_request(data->gc, req, ggp_oauth_authorization_done, data);
	purple_http_request_unref(req);

	g_free(request_data);
}

static void ggp_oauth_authorization_done(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, gpointer user_data)
{
	ggp_oauth_data *data = user_data;
	PurpleAccount *account;
	PurpleHttpRequest *req;
	char *auth;
	const char *method = "POST";
	const char *url = "http://api.gadu-gadu.pl/access_token";
	int response_code;

	if (!PURPLE_CONNECTION_IS_VALID(data->gc))
	{
		ggp_oauth_data_free(data);
		return;
	}
	account = purple_connection_get_account(data->gc);

	response_code = purple_http_response_get_code(response);
	if (response_code != 302)
	{
		purple_debug_error("gg", "ggp_oauth_authorization_done: "
			"failed (code = %d)", response_code);
		ggp_oauth_data_free(data);
		return;
	}

	purple_debug_misc("gg", "ggp_oauth_authorization_done: "
		"authorization done, requesting access token...\n");

	auth = gg_oauth_generate_header(method, url,
		purple_account_get_username(account),
		purple_connection_get_password(data->gc),
		data->token, data->token_secret);

	req = purple_http_request_new(url);
	purple_http_request_set_max_len(req, GGP_OAUTH_RESPONSE_MAX);
	purple_http_request_set_method(req, method);
	purple_http_request_header_set(req, "Authorization", auth);
	purple_http_request(data->gc, req, ggp_oauth_access_token_got, data);
	purple_http_request_unref(req);

	free(auth);
}

static void ggp_oauth_access_token_got(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, gpointer user_data)
{
	ggp_oauth_data *data = user_data;
	gchar *token, *token_secret;
	xmlnode *xml;
	const gchar *xml_raw;
	gboolean succ = TRUE;

	xml_raw = purple_http_response_get_data(response, NULL);
	xml = xmlnode_from_str(xml_raw, -1);
	if (xml == NULL)
	{
		purple_debug_error("gg", "ggp_oauth_access_token_got: "
			"invalid xml\n");
		ggp_oauth_data_free(data);
		return;
	}

	succ &= ggp_xml_get_string(xml, "oauth_token", &token);
	succ &= ggp_xml_get_string(xml, "oauth_token_secret",
		&token_secret);
	xmlnode_free(xml);
	if (!succ || strlen(token) < 10)
	{
		purple_debug_error("gg", "ggp_oauth_access_token_got: "
			"invalid xml - token is not present\n");
		ggp_oauth_data_free(data);
		return;
	}

	if (data->sign_url)
	{
		PurpleAccount *account;
		gchar *auth;
		
		purple_debug_misc("gg", "ggp_oauth_access_token_got: "
			"got access token, returning signed url\n");
		
		account = purple_connection_get_account(data->gc);
		auth = gg_oauth_generate_header(
			data->sign_method, data->sign_url,
			purple_account_get_username(account),
			purple_connection_get_password(data->gc),
			token, token_secret);
		data->callback(data->gc, auth, data->user_data);
	}
	else
	{
		purple_debug_misc("gg", "ggp_oauth_access_token_got: "
			"got access token, returning it\n");
		data->callback(data->gc, token, data->user_data);
	}

	g_free(token);
	g_free(token_secret);
	ggp_oauth_data_free(data);
}
