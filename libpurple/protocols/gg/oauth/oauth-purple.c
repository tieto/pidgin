#include "oauth-purple.h"

#include "oauth.h"
#include "../utils.h"
#include "../xml.h"

#include <debug.h>

#define GGP_OAUTH_RESPONSE_MAX 10240

typedef struct
{
	PurpleConnection *gc;
	ggp_oauth_request_cb callback;
	gpointer user_data;
	gchar *token;
	gchar *token_secret;
} ggp_oauth_data;

static void ggp_oauth_data_free(ggp_oauth_data *data);

static void ggp_oauth_request_token_got(PurpleUtilFetchUrlData *url_data,
	gpointer user_data, const gchar *url_text, gsize len,
	const gchar *error_message);

static void ggp_oauth_authorization_done(PurpleUtilFetchUrlData *url_data,
	gpointer user_data, const gchar *url_text, gsize len,
	const gchar *error_message);

static void ggp_oauth_access_token_got(PurpleUtilFetchUrlData *url_data,
	gpointer user_data, const gchar *url_text, gsize len,
	const gchar *error_message);

static void ggp_oauth_data_free(ggp_oauth_data *data)
{
	g_free(data->token);
	g_free(data->token_secret);
	g_free(data);
}

void ggp_oauth_request(PurpleConnection *gc, ggp_oauth_request_cb callback,
	gpointer user_data)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	char *auth;
	const char *method = "POST";
	const char *url = "http://api.gadu-gadu.pl/request_token";
	gchar *request;
	ggp_oauth_data *data;

	purple_debug_misc("gg", "ggp_oauth_request: requesting token...\n");

	auth = gg_oauth_generate_header(method, url,
		purple_account_get_username(account),
		purple_account_get_password(account), NULL, NULL);
	request = g_strdup_printf(
		"POST /request_token HTTP/1.1\r\n"
		"Host: api.gadu-gadu.pl\r\n"
		"%s\r\n"
		"Content-Length: 0\r\n"
		"\r\n",
		auth);
	free(auth);

	data = g_new0(ggp_oauth_data, 1);
	data->gc = gc;
	data->callback = callback;
	data->user_data = user_data;

	purple_util_fetch_url_request(account, url, FALSE, NULL, TRUE, request,
		FALSE, GGP_OAUTH_RESPONSE_MAX, ggp_oauth_request_token_got,
		data);

	g_free(request);
}

static void ggp_oauth_request_token_got(PurpleUtilFetchUrlData *url_data,
	gpointer user_data, const gchar *url_text, gsize len,
	const gchar *error_message)
{
	ggp_oauth_data *data = user_data;
	PurpleAccount *account;
	xmlnode *xml;
	gchar *request, *request_data;
	gboolean succ = TRUE;

	if (!PURPLE_CONNECTION_IS_VALID(data->gc))
	{
		ggp_oauth_data_free(data);
		return;
	}
	account = purple_connection_get_account(data->gc);

	if (len == 0)
	{
		purple_debug_error("gg", "ggp_oauth_request_token_got: "
			"requested token not received\n");
		ggp_oauth_data_free(data);
		return;
	}

	purple_debug_misc("gg", "ggp_oauth_request_token_got: "
		"got request token, doing authorization...\n");

	xml = xmlnode_from_str(url_text, -1);
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
		purple_account_get_password(account));
	request = g_strdup_printf(
		"POST /authorize HTTP/1.1\r\n"
		"Host: login.gadu-gadu.pl\r\n"
		"Content-Length: %d\r\n"
		"Content-Type: application/x-www-form-urlencoded\r\n"
		"\r\n%s",
		strlen(request_data), request_data);
	g_free(request_data);

	// we don't need any results, nor 302 redirection
	purple_util_fetch_url_request(account,
		"https://login.gadu-gadu.pl/authorize", FALSE, NULL, TRUE,
		request, FALSE, 0,
		ggp_oauth_authorization_done, data);

	g_free(request);
}

static void ggp_oauth_authorization_done(PurpleUtilFetchUrlData *url_data,
	gpointer user_data, const gchar *url_text, gsize len,
	const gchar *error_message)
{
	ggp_oauth_data *data = user_data;
	PurpleAccount *account;
	char *auth;
	const char *url = "http://api.gadu-gadu.pl/access_token";
	gchar *request;
	
	if (!PURPLE_CONNECTION_IS_VALID(data->gc))
	{
		ggp_oauth_data_free(data);
		return;
	}
	account = purple_connection_get_account(data->gc);

	purple_debug_misc("gg", "ggp_oauth_authorization_done: "
		"authorization done, requesting access token...\n");

	auth = gg_oauth_generate_header("POST", url,
		purple_account_get_username(account),
		purple_account_get_password(account),
		data->token, data->token_secret);

	request = g_strdup_printf(
		"POST /access_token HTTP/1.1\r\n"
		"Host: api.gadu-gadu.pl\r\n"
		"%s\r\n"
		"Content-Length: 0\r\n"
		"\r\n",
		auth);
	free(auth);
	
	purple_util_fetch_url_request(account, url, FALSE, NULL, TRUE, request,
		FALSE, GGP_OAUTH_RESPONSE_MAX, ggp_oauth_access_token_got,
		data);

	g_free(request);
}

static void ggp_oauth_access_token_got(PurpleUtilFetchUrlData *url_data,
	gpointer user_data, const gchar *url_text, gsize len,
	const gchar *error_message)
{
	ggp_oauth_data *data = user_data;
	gchar *token;
	xmlnode *xml;
	gboolean succ = TRUE;

	xml = xmlnode_from_str(url_text, -1);
	if (xml == NULL)
	{
		purple_debug_error("gg", "ggp_oauth_access_token_got: "
			"invalid xml\n");
		ggp_oauth_data_free(data);
		return;
	}

	succ &= ggp_xml_get_string(xml, "oauth_token", &token);
	xmlnode_free(xml);
	if (!succ || strlen(token) < 10)
	{
		purple_debug_error("gg", "ggp_oauth_access_token_got: "
			"invalid xml - token is not present\n");
		ggp_oauth_data_free(data);
		return;
	}

	purple_debug_misc("gg", "ggp_oauth_access_token_got: "
		"got access token\n");

	data->callback(data->gc, token, data->user_data);

	g_free(token);
	ggp_oauth_data_free(data);
}
