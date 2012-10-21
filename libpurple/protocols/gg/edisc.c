#include "edisc.h"

#include <debug.h>

#include "gg.h"
#include "libgaduw.h"
#include <http.h>

#define GGP_EDISC_HOSTNAME "MyComputer"
#define GGP_EDISC_OS "WINNT x86-msvc"
#define GGP_EDISC_TYPE "desktop"

#define GGP_EDISC_RESPONSE_MAX 10240

typedef struct _ggp_edisc_auth_data ggp_edisc_auth_data;

typedef struct _ggp_edisc_xfer ggp_edisc_xfer;

struct _ggp_edisc_session_data
{
	PurpleHttpCookieJar *cookies;
	gchar *security_token;

	PurpleHttpConnection *auth_request;
	gboolean auth_done;
	GList *auth_pending;
};

struct _ggp_edisc_xfer
{
	PurpleConnection *gc;
};

typedef void (*ggp_ggdrive_auth_cb)(PurpleConnection *gc, gboolean success,
	gpointer user_data);

static void ggp_ggdrive_auth(PurpleConnection *gc, ggp_ggdrive_auth_cb cb,
	gpointer user_data);

static void ggp_edisc_xfer_free(PurpleXfer *xfer);

/*******************************************************************************
 * Setting up.
 ******************************************************************************/

static inline ggp_edisc_session_data *
ggp_edisc_get_sdata(PurpleConnection *gc)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	return accdata->edisc_data;
}

void ggp_edisc_setup(PurpleConnection *gc)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	ggp_edisc_session_data *sdata = g_new0(ggp_edisc_session_data, 1);

	accdata->edisc_data = sdata;

	sdata->cookies = purple_http_cookie_jar_new();
}

void ggp_edisc_cleanup(PurpleConnection *gc)
{
	ggp_edisc_session_data *sdata = ggp_edisc_get_sdata(gc);

	purple_http_conn_cancel(sdata->auth_request);
	g_list_free_full(sdata->auth_pending, g_free);
	g_free(sdata->security_token);

	purple_http_cookie_jar_unref(sdata->cookies);

	g_free(sdata);
}

/*******************************************************************************
 * Sending a file.
 ******************************************************************************/

static void ggp_edisc_xfer_init(PurpleXfer *xfer);
static void ggp_edisc_xfer_init_authenticated(PurpleConnection *gc,
	gboolean success, gpointer _xfer);

static void ggp_edisc_xfer_error(PurpleXfer *xfer, const gchar *msg);

static void ggp_edisc_xfer_free(PurpleXfer *xfer)
{
	ggp_edisc_xfer *edisc_xfer = purple_xfer_get_protocol_data(xfer);

	if (edisc_xfer == NULL)
		return;

	g_free(edisc_xfer);
	purple_xfer_set_protocol_data(xfer, NULL);
}

static void ggp_edisc_xfer_error(PurpleXfer *xfer, const gchar *msg)
{
	purple_xfer_error(
		purple_xfer_get_type(xfer),
		purple_xfer_get_account(xfer),
		purple_xfer_get_remote_user(xfer),
		msg);
	ggp_edisc_xfer_free(xfer);
}

gboolean ggp_edisc_xfer_can_receive_file(PurpleConnection *gc, const char *who)
{
	return TRUE; /* TODO: only online, buddies (?) */
}

static void ggp_edisc_xfer_init(PurpleXfer *xfer)
{
	ggp_edisc_xfer *edisc_xfer = purple_xfer_get_protocol_data(xfer);

	if (purple_xfer_get_type(xfer) != PURPLE_XFER_SEND) {
		purple_debug_error("gg", "ggp_edisc_xfer_init: "
			"Not yet implemented\n");
		return;
	}

	ggp_ggdrive_auth(edisc_xfer->gc, ggp_edisc_xfer_init_authenticated, xfer);
}

static void ggp_edisc_xfer_init_authenticated(PurpleConnection *gc,
	gboolean success, gpointer _xfer)
{
	PurpleXfer *xfer = _xfer;

	if (!success) {
		ggp_edisc_xfer_error(xfer, _("Authentication failed"));
		return;
	}

	/* TODO: requesting a ticket */

	purple_xfer_start(xfer, -1, NULL, 0);
}

static void ggp_edisc_xfer_start(PurpleXfer *xfer)
{
	g_return_if_fail(xfer != NULL);

	if (purple_xfer_get_type(xfer) != PURPLE_XFER_SEND) {
		purple_debug_error("gg", "ggp_edisc_xfer_start: "
			"Not yet implemented\n");
		return;
	}

	purple_debug_info("gg", "Starting a file transfer...\n");
}

PurpleXfer * ggp_edisc_xfer_new(PurpleConnection *gc, const char *who)
{
	PurpleXfer *xfer;
	ggp_edisc_xfer *edisc_xfer;

	g_return_val_if_fail(gc != NULL, NULL);
	g_return_val_if_fail(who != NULL, NULL);

	xfer = purple_xfer_new(purple_connection_get_account(gc),
		PURPLE_XFER_SEND, who);
	edisc_xfer = g_new0(ggp_edisc_xfer, 1);
	purple_xfer_set_protocol_data(xfer, edisc_xfer);

	edisc_xfer->gc = gc;

	purple_xfer_set_init_fnc(xfer, ggp_edisc_xfer_init);
	purple_xfer_set_start_fnc(xfer, ggp_edisc_xfer_start);

	//purple_xfer_set_cancel_send_fnc(xfer, bonjour_xfer_cancel_send);
	//purple_xfer_set_end_fnc(xfer, bonjour_xfer_end);

	return xfer;
}

void ggp_edisc_xfer_send_file(PurpleConnection *gc, const char *who,
	const char *filename)
{
	PurpleXfer *xfer;

	g_return_if_fail(gc != NULL);
	g_return_if_fail(who != NULL);

	/* Nothing interesting here, this code is common among prpls.
	 * See ggp_edisc_new_xfer. */

	xfer = ggp_edisc_xfer_new(gc, who);
	if (filename)
		purple_xfer_request_accepted(xfer, filename);
	else
		purple_xfer_request(xfer);
}

/*******************************************************************************
 * Authentication
 ******************************************************************************/

struct _ggp_edisc_auth_data
{
	ggp_ggdrive_auth_cb cb;
	gpointer user_data;
};

static void ggp_ggdrive_auth_done(PurpleHttpConnection *hc,
	PurpleHttpResponse *response, gpointer user_data);

static void ggp_ggdrive_auth(PurpleConnection *gc, ggp_ggdrive_auth_cb cb,
	gpointer user_data)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	ggp_edisc_session_data *sdata = ggp_edisc_get_sdata(gc);
	ggp_edisc_auth_data *auth;
	const gchar *imtoken;
	gchar *metadata;
	PurpleHttpRequest *req;

	imtoken = ggp_get_imtoken(gc);
	if (!imtoken)
	{
		cb(gc, FALSE, user_data);
		return;
	}

	if (sdata->auth_done)
	{
		cb(gc, sdata->security_token != NULL, user_data);
		return;
	}

	auth = g_new0(ggp_edisc_auth_data, 1);
	auth->cb = cb;
	auth->user_data = user_data;
	sdata->auth_pending = g_list_prepend(sdata->auth_pending, auth);

	if (sdata->auth_request)
		return;

	purple_debug_info("gg", "ggp_ggdrive_auth(gc=%p)\n", gc);

	req = purple_http_request_new("https://drive.mpa.gg.pl/signin");
	purple_http_request_set_method(req, "PUT");

	/* TODO: defaults (browser name, etc) */
	purple_http_request_set_max_len(req, GGP_EDISC_RESPONSE_MAX);
	purple_http_request_set_cookie_jar(req, sdata->cookies);

	metadata = g_strdup_printf("{"
		"\"id\": \"%032x\", "
		"\"name\": \"" GGP_EDISC_HOSTNAME "\", "
		"\"os_version\": \"" GGP_EDISC_OS "\", "
		"\"client_version\": \"%s\", "
		"\"type\": \"" GGP_EDISC_TYPE "\"}",
		g_random_int_range(1, 1 << 16),
		ggp_libgaduw_version(gc));

	purple_http_request_header_set_printf(req, "Authorization",
		"IMToken %s", imtoken);
	purple_http_request_header_set_printf(req, "X-gged-user",
		"gg/pl:%u", accdata->session->uin);
	purple_http_request_header_set(req, "X-gged-client-metadata", metadata);
	purple_http_request_header_set(req, "X-gged-api-version", "6");
	g_free(metadata);

	sdata->auth_request = purple_http_request(gc, req,
		ggp_ggdrive_auth_done, NULL);
	purple_http_request_unref(req);
}

static void ggp_ggdrive_auth_results(PurpleConnection *gc, gboolean success)
{
	ggp_edisc_session_data *sdata = ggp_edisc_get_sdata(gc);
	GList *it;

	purple_debug_info("gg", "ggp_ggdrive_auth_results(gc=%p): %d\n",
		gc, success);

	it = g_list_first(sdata->auth_pending);
	while (it) {
		ggp_edisc_auth_data *auth = it->data;
		it = g_list_next(it);

		auth->cb(gc, success, auth->user_data);
		g_free(auth);
	}
	g_list_free(sdata->auth_pending);
	sdata->auth_pending = NULL;
	sdata->auth_done = TRUE;
}

static void ggp_ggdrive_auth_done(PurpleHttpConnection *hc,
	PurpleHttpResponse *response, gpointer user_data)
{
	PurpleConnection *gc = purple_http_conn_get_purple_connection(hc);
	ggp_edisc_session_data *sdata = ggp_edisc_get_sdata(gc);

	sdata->auth_request = NULL;

	if (!purple_http_response_is_successfull(response) ||
		0 != strcmp(purple_http_response_get_data(response),
		"{\"result\":{\"status\":0}}")) {
		ggp_ggdrive_auth_results(gc, FALSE);
		return;
	}

	sdata->security_token = g_strdup(purple_http_response_get_header(
		response, "X-gged-security-token"));
	if (!sdata->security_token) {
		ggp_ggdrive_auth_results(gc, FALSE);
		return;
	}

	if (purple_debug_is_unsafe())
		purple_debug_misc("gg", "ggp_ggdrive_auth_done: "
			"security_token=%s\n", sdata->security_token);
	ggp_ggdrive_auth_results(gc, TRUE);
}
