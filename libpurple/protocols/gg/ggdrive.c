#include "ggdrive.h"

#include <debug.h>

#include "gg.h"
#include "libgaduw.h"

#define GGP_GGDRIVE_HOSTNAME "MyComputer"
#define GGP_GGDRIVE_OS "WINNT x86-msvc"
#define GGP_GGDRIVE_TYPE "desktop"

#define GGP_GGDRIVE_RESPONSE_MAX 1024

/*******************************************************************************
 * Authentication
 ******************************************************************************/

typedef void (*ggp_ggdrive_authentication_cb)(const gchar *security_token,
	gpointer user_data);

static void ggp_ggdrive_authenticate(PurpleConnection *gc,
	ggp_ggdrive_authentication_cb cb, gpointer user_data);

#if 0
static void ggp_ggdrive_authenticate_done(PurpleUtilFetchUrlData *url_data,
	gpointer _user_data, const gchar *url_text, gsize len,
	const gchar *error_message);
#endif

typedef struct
{
	PurpleConnection *gc;
	ggp_ggdrive_authentication_cb cb;
	gpointer user_data;
} ggp_ggdrive_authenticate_data;

/******************************************************************************/

static void ggp_ggdrive_authenticate(PurpleConnection *gc,
	ggp_ggdrive_authentication_cb cb, gpointer user_data)
{
#if 0
	PurpleAccount *account = purple_connection_get_account(gc);
	const gchar *imtoken;
	gchar *request, *metadata;
	ggp_ggdrive_authenticate_data *req_data;

	imtoken = ggp_get_imtoken(gc);
	if (!imtoken)
	{
		cb(NULL, user_data);
		return;
	}
	
	metadata = g_strdup_printf("{"
		"\"id\": \"%032x\", "
		"\"name\": \"" GGP_GGDRIVE_HOSTNAME "\", "
		"\"os_version\": \"" GGP_GGDRIVE_OS "\", "
		"\"client_version\": \"%s\", "
		"\"type\": \"" GGP_GGDRIVE_TYPE "\"}",
		g_random_int_range(1, 1 << 16),
		ggp_libgaduw_version(gc));
	
	request = g_strdup_printf(
		"PUT /signin HTTP/1.1\r\n"
		"Host: drive.mpa.gg.pl\r\n"
		"Authorization: IMToken %s\r\n"
		"X-gged-user: gg/pl:%u\r\n"
		"X-gged-client-metadata: %s\r\n"
		"X-gged-api-version: 6\r\n"
		"Content-Type: application/x-www-form-urlencoded\r\n"
		"Content-Length: 0\r\n"
		"\r\n",
		imtoken, ggp_own_uin(gc), metadata);

	req_data = g_new0(ggp_ggdrive_authenticate_data, 1);
	req_data->gc = gc;
	req_data->cb = cb;
	req_data->user_data = user_data;

	purple_util_fetch_url_request(account, "https://drive.mpa.gg.pl/signin",
		FALSE, NULL, TRUE, request, TRUE, GGP_GGDRIVE_RESPONSE_MAX,
		ggp_ggdrive_authenticate_done, req_data);

	g_free(metadata);
	g_free(request);
#endif
}

#if 0
static void ggp_ggdrive_authenticate_done(PurpleUtilFetchUrlData *url_data,
	gpointer _user_data, const gchar *url_text, gsize len,
	const gchar *error_message)
{
	ggp_ggdrive_authenticate_data *req_data = _user_data;
	PurpleConnection *gc = req_data->gc;
	ggp_ggdrive_authentication_cb cb = req_data->cb;
	gpointer user_data = req_data->user_data;

	g_free(req_data);

	if (!PURPLE_CONNECTION_IS_VALID(gc))
	{
		cb(NULL, user_data);
		return;
	}

	if (len == 0)
	{
		purple_debug_error("gg", "ggp_ggdrive_authenticate_done: failed\n");
		cb(NULL, user_data);
		return;
	}

	purple_debug_info("gg", "ggp_ggdrive_authenticate_done: [%s]\n", url_text);
}
#endif

/***/

static void ggp_ggdrive_test_cb(const gchar *security_token,
	gpointer user_data)
{
	if (!security_token)
		purple_debug_error("gg", "ggp_ggdrive_test_cb: didn't got security token\n");
	else
		purple_debug_info("gg", "ggp_ggdrive_test_cb: got security token [%s]\n", security_token);
}

void ggp_ggdrive_test(PurpleConnection *gc)
{
	ggp_ggdrive_authenticate(gc, ggp_ggdrive_test_cb, NULL);
}
