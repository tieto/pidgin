#include "edisc.h"

#include <debug.h>

#include "gg.h"
#include "libgaduw.h"
#include "utils.h"
#include <http.h>

#include <json-glib/json-glib.h>

#define GGP_EDISC_OS "WINNT x86-msvc"
#define GGP_EDISC_TYPE "desktop"

#define GGP_EDISC_RESPONSE_MAX 10240
#define GGP_EDISC_FNAME_ALLOWED "1234567890" \
	"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ" \
	" [](){}-+=_;'<>,.&$!"

typedef struct _ggp_edisc_auth_data ggp_edisc_auth_data;

typedef struct _ggp_edisc_xfer ggp_edisc_xfer;

struct _ggp_edisc_session_data
{
	GHashTable *xfers_initialized;

	PurpleHttpCookieJar *cookies;
	gchar *security_token;

	PurpleHttpConnection *auth_request;
	gboolean auth_done;
	GList *auth_pending;
};

struct _ggp_edisc_xfer
{
	gchar *filename;
	gchar *ticket_id;

	gboolean allowed;

	PurpleConnection *gc;
	PurpleHttpConnection *hc;

	int already_read;
};

typedef void (*ggp_ggdrive_auth_cb)(PurpleConnection *gc, gboolean success,
	gpointer user_data);

static void ggp_ggdrive_auth(PurpleConnection *gc, ggp_ggdrive_auth_cb cb,
	gpointer user_data);

static void ggp_edisc_xfer_free(PurpleXfer *xfer);
static void ggp_edisc_set_defaults(PurpleHttpRequest *req);

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
	sdata->xfers_initialized = g_hash_table_new(g_str_hash, g_str_equal);
}

void ggp_edisc_cleanup(PurpleConnection *gc)
{
	ggp_edisc_session_data *sdata = ggp_edisc_get_sdata(gc);

	purple_http_conn_cancel(sdata->auth_request);
	g_list_free_full(sdata->auth_pending, g_free);
	g_free(sdata->security_token);

	purple_http_cookie_jar_unref(sdata->cookies);
	g_hash_table_destroy(sdata->xfers_initialized);

	g_free(sdata);
}

/*******************************************************************************
 * Misc.
 ******************************************************************************/

static void ggp_edisc_set_defaults(PurpleHttpRequest *req)
{
	purple_http_request_set_max_len(req, GGP_EDISC_RESPONSE_MAX);
	purple_http_request_header_set(req, "X-gged-api-version", "6");

	/* optional fields */
	purple_http_request_header_set(req, "User-Agent", "Mozilla/5.0 (Windows"
		" NT 6.1; rv:11.0) Gecko/20120613 GG/11.0.0.8169 (WINNT_x86-msv"
		"c; pl; beta; standard)");
	purple_http_request_header_set(req, "Accept", "text/html,application/xh"
		"tml+xml,application/xml;q=0.9,*/*;q=0.8");
	purple_http_request_header_set(req, "Accept-Language",
		"pl,en-us;q=0.7,en;q=0.3");
	/* purple_http_request_header_set(req, "Accept-Encoding",
	 * "gzip, deflate"); */
	purple_http_request_header_set(req, "Accept-Charset",
		"ISO-8859-2,utf-8;q=0.7,*;q=0.7");
	purple_http_request_header_set(req, "Connection", "keep-alive");
	purple_http_request_header_set(req, "Content-Type",
		"application/x-www-form-urlencoded; charset=UTF-8");
}

/*******************************************************************************
 * Sending a file.
 ******************************************************************************/

static void ggp_edisc_xfer_init(PurpleXfer *xfer);
static void ggp_edisc_xfer_init_authenticated(PurpleConnection *gc,
	gboolean success, gpointer _xfer);
static void ggp_edisc_xfer_init_ticket_sent(PurpleHttpConnection *hc,
	PurpleHttpResponse *response, gpointer _xfer);
static void ggp_edisc_xfer_sent(PurpleHttpConnection *hc,
	PurpleHttpResponse *response, gpointer _xfer);

static void ggp_edisc_xfer_error(PurpleXfer *xfer, const gchar *msg);

static void ggp_edisc_xfer_free(PurpleXfer *xfer)
{
	ggp_edisc_session_data *sdata;
	ggp_edisc_xfer *edisc_xfer = purple_xfer_get_protocol_data(xfer);

	if (edisc_xfer == NULL)
		return;

	g_free(edisc_xfer->filename);

	sdata = ggp_edisc_get_sdata(edisc_xfer->gc);
	if (edisc_xfer->ticket_id != NULL)
		g_hash_table_remove(sdata->xfers_initialized,
			edisc_xfer->ticket_id);

	g_free(edisc_xfer);
	purple_xfer_set_protocol_data(xfer, NULL);

}

static void ggp_edisc_xfer_error(PurpleXfer *xfer, const gchar *msg)
{
	purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_CANCEL_REMOTE);
	purple_xfer_conversation_write(xfer, msg, TRUE);
	purple_xfer_error(
		purple_xfer_get_type(xfer),
		purple_xfer_get_account(xfer),
		purple_xfer_get_remote_user(xfer),
		msg);
	ggp_edisc_xfer_free(xfer);
	purple_xfer_end(xfer);
}

static int ggp_edisc_parse_error(const gchar *data)
{
	JsonParser *parser;
	JsonObject *result;
	int error_id;

	parser = ggp_json_parse(data);
	result = json_node_get_object(json_parser_get_root(parser));
	result = json_object_get_object_member(result, "result");
	error_id = json_object_get_int_member(result, "appStatus");
	purple_debug_info("gg", "edisc error: %s (%d)\n",
		json_object_get_string_member(result, "errorMsg"),
		error_id);
	g_object_unref(parser);

	return error_id;
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

	purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_NOT_STARTED);

	edisc_xfer->filename = g_strdup(purple_xfer_get_filename(xfer));
	g_strcanon(edisc_xfer->filename, GGP_EDISC_FNAME_ALLOWED, '_');

	ggp_ggdrive_auth(edisc_xfer->gc, ggp_edisc_xfer_init_authenticated, xfer);
}

static void ggp_edisc_xfer_init_authenticated(PurpleConnection *gc,
	gboolean success, gpointer _xfer)
{
	ggp_edisc_session_data *sdata = ggp_edisc_get_sdata(gc);
	PurpleHttpRequest *req;
	PurpleXfer *xfer = _xfer;
	ggp_edisc_xfer *edisc_xfer = purple_xfer_get_protocol_data(xfer);
	gchar *data;

	if (!success) {
		ggp_edisc_xfer_error(xfer, _("Authentication failed"));
		return;
	}

	req = purple_http_request_new("https://drive.mpa.gg.pl/send_ticket");
	purple_http_request_set_method(req, "PUT");

	ggp_edisc_set_defaults(req);
	purple_http_request_set_cookie_jar(req, sdata->cookies);

	purple_http_request_header_set(req, "X-gged-security-token",
		sdata->security_token);

	data = g_strdup_printf("{\"send_ticket\":{"
		"\"recipient\":\"%s\","
		"\"file_name\":\"%s\","
		"\"file_size\":\"%u\""
		"}}",
		purple_xfer_get_remote_user(xfer),
		edisc_xfer->filename,
		(int)purple_xfer_get_size(xfer));
	purple_http_request_set_contents(req, data, -1);
	g_free(data);

	edisc_xfer->hc = purple_http_request(gc, req,
		ggp_edisc_xfer_init_ticket_sent, xfer);
	purple_http_request_unref(req);
}

static void ggp_edisc_xfer_init_ticket_sent(PurpleHttpConnection *hc,
	PurpleHttpResponse *response, gpointer _xfer)
{
	ggp_edisc_session_data *sdata = ggp_edisc_get_sdata(
		purple_http_conn_get_purple_connection(hc));
	PurpleXfer *xfer = _xfer;
	ggp_edisc_xfer *edisc_xfer = purple_xfer_get_protocol_data(xfer);
	const gchar *data = purple_http_response_get_data(response);
	JsonParser *parser;
	JsonObject *ticket;

	edisc_xfer->hc = NULL;

	if (!purple_http_response_is_successfull(response)) {
		int error_id = ggp_edisc_parse_error(data);
		if (error_id == 206) /* recipient not logged in */
			ggp_edisc_xfer_error(xfer, _("Recipient not logged in"));
		else
			ggp_edisc_xfer_error(xfer, _("Cannot offer sending a file"));
		return;
	}

	parser = ggp_json_parse(data);
	ticket = json_node_get_object(json_parser_get_root(parser));
	ticket = json_object_get_object_member(ticket, "result");
	ticket = json_object_get_object_member(ticket, "send_ticket");
	edisc_xfer->ticket_id = g_strdup(json_object_get_string_member(
		ticket, "id"));

	g_object_unref(parser);

	if (edisc_xfer->ticket_id == NULL) {
		purple_debug_error("gg",
			"ggp_edisc_xfer_init_ticket_sent: "
			"couldn't get ticket id\n");
		return;
	}

	purple_debug_info("gg", "ggp_edisc_xfer_init_ticket_sent: "
		"ticket \"%s\" created\n", edisc_xfer->ticket_id);

	g_hash_table_insert(sdata->xfers_initialized,
		edisc_xfer->ticket_id, xfer);
}

void ggp_edisc_event_send_ticket_changed(PurpleConnection *gc, const char *data)
{
	ggp_edisc_session_data *sdata = ggp_edisc_get_sdata(gc);
	PurpleXfer *xfer;
	ggp_edisc_xfer *edisc_xfer;
	JsonParser *parser;
	JsonObject *ticket;
	const gchar *ticket_id, *ack_status;
	gboolean is_allowed;

	parser = ggp_json_parse(data);
	ticket = json_node_get_object(json_parser_get_root(parser));
	ticket_id = json_object_get_string_member(ticket, "id");
	ack_status = json_object_get_string_member(ticket, "ack_status");

	if (g_strcmp0("unknown", ack_status) == 0) {
		g_object_unref(parser);
		return;
	} else if (g_strcmp0("rejected", ack_status) == 0)
		is_allowed = FALSE;
	else if (g_strcmp0("allowed", ack_status) == 0)
		is_allowed = TRUE;
	else {
		purple_debug_warning("gg", "ggp_edisc_event_send_ticket_changed: "
			"unknown ack_status=%s\n", ack_status);
		g_object_unref(parser);
		return;
	}

	xfer = g_hash_table_lookup(sdata->xfers_initialized, ticket_id);
	if (xfer == NULL) {
		purple_debug_warning("gg", "ggp_edisc_event_send_ticket_changed: "
			"ticket %s not found\n", ticket_id);
		g_object_unref(parser);
		return;
	}

	g_object_unref(parser);

	edisc_xfer = purple_xfer_get_protocol_data(xfer);
	if (!edisc_xfer) {
		purple_debug_fatal("gg", "ggp_edisc_event_send_ticket_changed: "
			"transfer %p already free'd\n", xfer);
		return;
	}

	if (!is_allowed) {
		purple_debug_info("gg", "ggp_edisc_event_send_ticket_changed: "
			"transfer %p rejected\n", xfer);
		purple_xfer_cancel_remote(xfer);
		ggp_edisc_xfer_free(xfer);
		return;
	}

	if (edisc_xfer->allowed) {
		purple_debug_misc("gg", "ggp_edisc_event_send_ticket_changed: "
			"transfer %p already allowed\n", xfer);
		return;
	}
	edisc_xfer->allowed = TRUE;

	purple_xfer_start(xfer, -1, NULL, 0);
}

static void ggp_edisc_xfer_reader(PurpleHttpConnection *hc,
	gchar *buffer, size_t offset, size_t length, gpointer _xfer,
	PurpleHttpContentReaderCb cb)
{
	PurpleXfer *xfer = _xfer;
	ggp_edisc_xfer *edisc_xfer;
	int stored;
	gboolean success, eof = FALSE;

	g_return_if_fail(xfer != NULL);
	edisc_xfer = purple_xfer_get_protocol_data(xfer);
	g_return_if_fail(edisc_xfer != NULL);

	if (edisc_xfer->already_read != offset) {
		purple_debug_error("gg", "ggp_edisc_xfer_reader: "
			"Invalid offset (%d != %d)\n",
			edisc_xfer->already_read, offset);
		ggp_edisc_xfer_error(xfer, _("Error while reading a file"));
		return;
	}

	stored = fread(buffer, 1, length, xfer->dest_fp);
	if (stored < 0)
		success = FALSE;
	else {
		success = TRUE;
		edisc_xfer->already_read += stored;
		eof = (edisc_xfer->already_read >= purple_xfer_get_size(xfer));
	}

	cb(hc, success, eof, stored);
}

static void ggp_edisc_xfer_progress_watcher(PurpleHttpConnection *hc,
	gboolean reading_state, int processed, int total, gpointer _xfer)
{
	PurpleXfer *xfer = _xfer;
	gboolean eof;
	int total_real;

	if (purple_xfer_get_type(xfer) != PURPLE_XFER_SEND) {
		purple_debug_error("gg", "ggp_edisc_xfer_start: "
			"Not yet implemented\n");
		return;
	}

	if (reading_state)
		return;

	eof = (processed >= total);
	total_real = purple_xfer_get_size(xfer);
	if (eof || processed > total_real)
		processed = total_real; /* just to be sure */

	purple_xfer_set_bytes_sent(xfer, processed);
	purple_xfer_update_progress(xfer);
}

static void ggp_edisc_xfer_start(PurpleXfer *xfer)
{
	ggp_edisc_session_data *sdata;
	ggp_edisc_xfer *edisc_xfer;
	gchar *upload_url, *filename_e;
	PurpleHttpRequest *req;

	g_return_if_fail(xfer != NULL);
	edisc_xfer = purple_xfer_get_protocol_data(xfer);
	g_return_if_fail(edisc_xfer != NULL);
	sdata = ggp_edisc_get_sdata(edisc_xfer->gc);

	if (purple_xfer_get_type(xfer) != PURPLE_XFER_SEND) {
		purple_debug_error("gg", "ggp_edisc_xfer_start: "
			"Not yet implemented\n");
		return;
	}

	filename_e = purple_strreplace(edisc_xfer->filename, " ", "%20");
	upload_url = g_strdup_printf("https://drive.mpa.gg.pl/me/file/outbox/"
		"%s%%2C%s", edisc_xfer->ticket_id, filename_e);
	g_free(filename_e);
	req = purple_http_request_new(upload_url);
	g_free(upload_url);

	purple_http_request_set_method(req, "PUT");
	purple_http_request_set_timeout(req, -1);

	ggp_edisc_set_defaults(req);
	purple_http_request_set_cookie_jar(req, sdata->cookies);

	purple_http_request_header_set(req, "X-gged-local-revision", "0");
	purple_http_request_header_set(req, "X-gged-security-token",
		sdata->security_token);
	purple_http_request_header_set(req, "X-gged-metadata", "{\"node_type\": \"file\"}");

	purple_http_request_set_contents_reader(req, ggp_edisc_xfer_reader,
		purple_xfer_get_size(xfer), xfer);

	edisc_xfer->hc = purple_http_request(edisc_xfer->gc, req,
		ggp_edisc_xfer_sent, xfer);
	purple_http_request_unref(req);

	purple_http_conn_set_progress_watcher(edisc_xfer->hc,
		ggp_edisc_xfer_progress_watcher, xfer, 250000);
}

static void ggp_edisc_xfer_sent(PurpleHttpConnection *hc,
	PurpleHttpResponse *response, gpointer _xfer)
{
	PurpleXfer *xfer = _xfer;
	ggp_edisc_xfer *edisc_xfer = purple_xfer_get_protocol_data(xfer);
	const gchar *data = purple_http_response_get_data(response);
	JsonParser *parser;
	JsonObject *result;
	int result_status;

	g_return_if_fail(edisc_xfer != NULL);

	edisc_xfer->hc = NULL;

	if (!purple_http_response_is_successfull(response)) {
		ggp_edisc_xfer_error(xfer, _("Error while sending a file"));
		return;
	}

	parser = ggp_json_parse(data);
	result = json_node_get_object(json_parser_get_root(parser));
	result = json_object_get_object_member(result, "result");
	if (json_object_has_member(result, "status"))
		result_status = json_object_get_int_member(result, "status");
	else
		result_status = -1;
	g_object_unref(parser);

	if (result_status == 0) {
		purple_xfer_set_completed(xfer, TRUE);
		purple_xfer_end(xfer);
		ggp_edisc_xfer_free(xfer);
	} else
		ggp_edisc_xfer_error(xfer, _("Error while sending a file"));
}

static void ggp_edisc_xfer_cancel(PurpleXfer *xfer)
{
	g_return_if_fail(xfer != NULL);

	if (purple_xfer_get_type(xfer) != PURPLE_XFER_SEND) {
		purple_debug_error("gg", "ggp_edisc_xfer_start: "
			"Not yet implemented\n");
		return;
	}

	purple_debug_info("gg", "ggp_edisc_xfer_cancel(%p)\n", xfer);
}

static void ggp_edisc_xfer_end(PurpleXfer *xfer)
{
	g_return_if_fail(xfer != NULL);

	if (purple_xfer_get_type(xfer) != PURPLE_XFER_SEND) {
		purple_debug_error("gg", "ggp_edisc_xfer_start: "
			"Not yet implemented\n");
		return;
	}

	purple_debug_info("gg", "ggp_edisc_xfer_end(%p)\n", xfer);
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

	purple_xfer_set_cancel_send_fnc(xfer, ggp_edisc_xfer_cancel);
	purple_xfer_set_end_fnc(xfer, ggp_edisc_xfer_end);

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

	ggp_edisc_set_defaults(req);
	purple_http_request_set_cookie_jar(req, sdata->cookies);

	metadata = g_strdup_printf("{"
		"\"id\": \"%032x\", "
		"\"name\": \"%s\", "
		"\"os_version\": \"" GGP_EDISC_OS "\", "
		"\"client_version\": \"%s\", "
		"\"type\": \"" GGP_EDISC_TYPE "\"}",
		g_random_int_range(1, 1 << 16),
		purple_get_host_name(),
		ggp_libgaduw_version(gc));

	purple_http_request_header_set_printf(req, "Authorization",
		"IMToken %s", imtoken);
	purple_http_request_header_set_printf(req, "X-gged-user",
		"gg/pl:%u", accdata->session->uin);
	purple_http_request_header_set(req, "X-gged-client-metadata", metadata);
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
