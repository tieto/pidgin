#include "edisc.h"

#include <debug.h>

#include "gg.h"
#include "libgaduw.h"
#include "utils.h"
#include <http.h>

#include <json-glib/json-glib.h>

#define GGP_EDISC_OS "WINNT x86-msvc"
#define GGP_EDISC_TYPE "desktop"
#define GGP_EDISC_API "6"

#define GGP_EDISC_RESPONSE_MAX 10240
#define GGP_EDISC_FNAME_ALLOWED "1234567890" \
	"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ" \
	" [](){}-+=_;'<>,.&$!"

typedef struct _ggp_edisc_auth_data ggp_edisc_auth_data;

typedef struct _ggp_edisc_xfer ggp_edisc_xfer;

struct _ggp_edisc_session_data
{
	GHashTable *xfers_initialized;
	GHashTable *xfers_history;

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

	gboolean allowed, ready;

	PurpleConnection *gc;
	PurpleHttpConnection *hc;

	gsize already_read;
};

typedef enum
{
	GGP_EDISC_XFER_ACK_STATUS_UNKNOWN,
	GGP_EDISC_XFER_ACK_STATUS_ALLOWED,
	GGP_EDISC_XFER_ACK_STATUS_REJECTED
} ggp_edisc_xfer_ack_status;

typedef void (*ggp_ggdrive_auth_cb)(PurpleConnection *gc, gboolean success,
	gpointer user_data);

/* Setting up. */
static inline ggp_edisc_session_data *
ggp_edisc_get_sdata(PurpleConnection *gc);

/* Misc. */
static void ggp_edisc_set_defaults(PurpleHttpRequest *req);
static int ggp_edisc_parse_error(const gchar *data);

/* General xfer functions. */
static void ggp_edisc_xfer_free(PurpleXfer *xfer);
static void ggp_edisc_xfer_error(PurpleXfer *xfer, const gchar *msg);
static void ggp_edisc_xfer_cancel(PurpleXfer *xfer);
static const gchar * ggp_edisc_xfer_ticket_url(const gchar *ticket_id);
static void ggp_edisc_xfer_progress_watcher(PurpleHttpConnection *hc,
	gboolean reading_state, int processed, int total, gpointer _xfer);
static ggp_edisc_xfer_ack_status
ggp_edisc_xfer_parse_ack_status(const gchar *str);


/* Sending a file. */
static void ggp_edisc_xfer_send_ticket_changed(PurpleConnection *gc,
	PurpleXfer *xfer, gboolean is_allowed);

/* Receiving a file. */
static void ggp_edisc_xfer_recv_reject(PurpleXfer *xfer);
static void ggp_edisc_xfer_recv_ticket_got(PurpleConnection *gc,
	const gchar *ticket_id);
static void ggp_edisc_xfer_recv_ticket_completed(PurpleXfer *xfer);

/* Authentication. */
static void ggp_ggdrive_auth(PurpleConnection *gc, ggp_ggdrive_auth_cb cb,
	gpointer user_data);

/*******************************************************************************
 * Setting up.
 ******************************************************************************/

static inline ggp_edisc_session_data *
ggp_edisc_get_sdata(PurpleConnection *gc)
{
	GGPInfo *accdata;

	g_return_val_if_fail(PURPLE_CONNECTION_IS_VALID(gc), NULL);

	accdata = purple_connection_get_protocol_data(gc);
	g_return_val_if_fail(accdata != NULL, NULL);

	return accdata->edisc_data;
}

void ggp_edisc_setup(PurpleConnection *gc)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	ggp_edisc_session_data *sdata = g_new0(ggp_edisc_session_data, 1);

	accdata->edisc_data = sdata;

	sdata->cookies = purple_http_cookie_jar_new();
	sdata->xfers_initialized = g_hash_table_new(g_str_hash, g_str_equal);
	sdata->xfers_history = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
}

void ggp_edisc_cleanup(PurpleConnection *gc)
{
	ggp_edisc_session_data *sdata = ggp_edisc_get_sdata(gc);

	purple_http_conn_cancel(sdata->auth_request);
	g_list_free_full(sdata->auth_pending, g_free);
	g_free(sdata->security_token);

	purple_http_cookie_jar_unref(sdata->cookies);
	g_hash_table_destroy(sdata->xfers_initialized);
	g_hash_table_destroy(sdata->xfers_history);

	g_free(sdata);
}

/*******************************************************************************
 * Misc.
 ******************************************************************************/

static void ggp_edisc_set_defaults(PurpleHttpRequest *req)
{
	purple_http_request_set_max_len(req, GGP_EDISC_RESPONSE_MAX);
	purple_http_request_header_set(req, "X-gged-api-version",
		GGP_EDISC_API);

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

/*******************************************************************************
 * General xfer functions.
 ******************************************************************************/

static void ggp_edisc_xfer_free(PurpleXfer *xfer)
{
	ggp_edisc_session_data *sdata;
	ggp_edisc_xfer *edisc_xfer = purple_xfer_get_protocol_data(xfer);

	if (edisc_xfer == NULL)
		return;

	g_free(edisc_xfer->filename);
	purple_http_conn_cancel(edisc_xfer->hc);

	sdata = ggp_edisc_get_sdata(edisc_xfer->gc);
	if (edisc_xfer->ticket_id != NULL)
		g_hash_table_remove(sdata->xfers_initialized,
			edisc_xfer->ticket_id);

	g_free(edisc_xfer);
	purple_xfer_set_protocol_data(xfer, NULL);

}

static void ggp_edisc_xfer_error(PurpleXfer *xfer, const gchar *msg)
{
	if (purple_xfer_is_cancelled(xfer))
		g_return_if_reached();
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

void ggp_edisc_xfer_ticket_changed(PurpleConnection *gc, const char *data)
{
	ggp_edisc_session_data *sdata = ggp_edisc_get_sdata(gc);
	PurpleXfer *xfer;
	JsonParser *parser;
	JsonObject *ticket;
	const gchar *ticket_id, *send_status;
	ggp_edisc_xfer_ack_status ack_status;
	gboolean is_completed;

	parser = ggp_json_parse(data);
	ticket = json_node_get_object(json_parser_get_root(parser));
	ticket_id = json_object_get_string_member(ticket, "id");
	ack_status = ggp_edisc_xfer_parse_ack_status(
		json_object_get_string_member(ticket, "ack_status"));
	send_status = json_object_get_string_member(ticket, "send_status");

	if (ticket_id == NULL)
		ticket_id = "";
	xfer = g_hash_table_lookup(sdata->xfers_initialized, ticket_id);
	if (xfer == NULL) {
		purple_debug_misc("gg", "ggp_edisc_event_ticket_changed: "
			"ticket %s not found, updating it...\n",
			purple_debug_is_unsafe() ? ticket_id : "");
		ggp_edisc_xfer_recv_ticket_got(gc, ticket_id);
		g_object_unref(parser);
		return;
	}

	is_completed = FALSE;
	if (g_strcmp0("in_progress", send_status) == 0) {
		/* do nothing */
	} else if (g_strcmp0("completed", send_status) == 0) {
		is_completed = TRUE;
	} else if (g_strcmp0("expired", send_status) == 0)
		ggp_edisc_xfer_error(xfer, _("File transfer expired."));
	else {
		purple_debug_warning("gg", "ggp_edisc_event_ticket_changed: "
			"unknown send_status=%s\n", send_status);
		g_object_unref(parser);
		return;
	}

	g_object_unref(parser);

	if (purple_xfer_get_type(xfer) == PURPLE_XFER_RECEIVE) {
		if (is_completed)
			ggp_edisc_xfer_recv_ticket_completed(xfer);
	} else {
		if (ack_status != GGP_EDISC_XFER_ACK_STATUS_UNKNOWN)
			ggp_edisc_xfer_send_ticket_changed(gc, xfer, ack_status
				== GGP_EDISC_XFER_ACK_STATUS_ALLOWED);
	}

}

static void ggp_edisc_xfer_cancel(PurpleXfer *xfer)
{
	g_return_if_fail(xfer != NULL);

	ggp_edisc_xfer_free(xfer);
}

static const gchar * ggp_edisc_xfer_ticket_url(const gchar *ticket_id)
{
	static gchar ticket_url[150];

	g_snprintf(ticket_url, sizeof(ticket_url),
		"https://drive.mpa.gg.pl/send_ticket/%s", ticket_id);

	return ticket_url;
}

static void ggp_edisc_xfer_progress_watcher(PurpleHttpConnection *hc,
	gboolean reading_state, int processed, int total, gpointer _xfer)
{
	PurpleXfer *xfer = _xfer;
	gboolean eof;
	int total_real;

	if (purple_xfer_get_type(xfer) == PURPLE_XFER_RECEIVE) {
		if (!reading_state)
			return;
	} else {
		if (reading_state)
			return;
	}

	eof = (processed >= total);
	total_real = purple_xfer_get_size(xfer);
	if (eof || processed > total_real)
		processed = total_real; /* just to be sure */

	purple_xfer_set_bytes_sent(xfer, processed);
	purple_xfer_update_progress(xfer);
}

static ggp_edisc_xfer_ack_status
ggp_edisc_xfer_parse_ack_status(const gchar *str)
{
	g_return_val_if_fail(str != NULL, GGP_EDISC_XFER_ACK_STATUS_UNKNOWN);

	if (g_strcmp0("unknown", str) == 0)
		return GGP_EDISC_XFER_ACK_STATUS_UNKNOWN;
	if (g_strcmp0("allowed", str) == 0)
		return GGP_EDISC_XFER_ACK_STATUS_ALLOWED;
	if (g_strcmp0("rejected", str) == 0)
		return GGP_EDISC_XFER_ACK_STATUS_REJECTED;

	purple_debug_warning("gg", "ggp_edisc_xfer_parse_ack_status: "
		"unknown status (%s)\n", str);
	return GGP_EDISC_XFER_ACK_STATUS_UNKNOWN;
}

/*******************************************************************************
 * Sending a file.
 ******************************************************************************/

static void ggp_edisc_xfer_send_init(PurpleXfer *xfer);
static void ggp_edisc_xfer_send_init_authenticated(PurpleConnection *gc,
	gboolean success, gpointer _xfer);
static void ggp_edisc_xfer_send_init_ticket_created(PurpleHttpConnection *hc,
	PurpleHttpResponse *response, gpointer _xfer);
static void ggp_edisc_xfer_send_reader(PurpleHttpConnection *hc,
	gchar *buffer, size_t offset, size_t length, gpointer _xfer,
	PurpleHttpContentReaderCb cb);
static void ggp_edisc_xfer_send_start(PurpleXfer *xfer);
static void ggp_edisc_xfer_send_done(PurpleHttpConnection *hc,
	PurpleHttpResponse *response, gpointer _xfer);

gboolean ggp_edisc_xfer_can_receive_file(PurpleConnection *gc,
	const char *who)
{
	PurpleBuddy *buddy;

	g_return_val_if_fail(gc != NULL, FALSE);
	g_return_val_if_fail(who != NULL, FALSE);

	buddy = purple_find_buddy(purple_connection_get_account(gc), who);
	if (buddy == NULL)
		return FALSE;

	/* TODO: check, if this buddy have us on his list */

	return PURPLE_BUDDY_IS_ONLINE(buddy);
}

static void ggp_edisc_xfer_send_init(PurpleXfer *xfer)
{
	ggp_edisc_xfer *edisc_xfer = purple_xfer_get_protocol_data(xfer);

	purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_NOT_STARTED);

	edisc_xfer->filename = g_strdup(purple_xfer_get_filename(xfer));
	g_strcanon(edisc_xfer->filename, GGP_EDISC_FNAME_ALLOWED, '_');

	ggp_ggdrive_auth(edisc_xfer->gc, ggp_edisc_xfer_send_init_authenticated,
		xfer);
}

static void ggp_edisc_xfer_send_init_authenticated(PurpleConnection *gc,
	gboolean success, gpointer _xfer)
{
	ggp_edisc_session_data *sdata = ggp_edisc_get_sdata(gc);
	PurpleHttpRequest *req;
	PurpleXfer *xfer = _xfer;
	ggp_edisc_xfer *edisc_xfer = purple_xfer_get_protocol_data(xfer);
	gchar *data;

	if (purple_xfer_is_cancelled(xfer))
		return;

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
		ggp_edisc_xfer_send_init_ticket_created, xfer);
	purple_http_request_unref(req);
}

static void ggp_edisc_xfer_send_init_ticket_created(PurpleHttpConnection *hc,
	PurpleHttpResponse *response, gpointer _xfer)
{
	ggp_edisc_session_data *sdata = ggp_edisc_get_sdata(
		purple_http_conn_get_purple_connection(hc));
	PurpleXfer *xfer = _xfer;
	ggp_edisc_xfer *edisc_xfer = purple_xfer_get_protocol_data(xfer);
	const gchar *data = purple_http_response_get_data(response, NULL);
	ggp_edisc_xfer_ack_status ack_status;
	JsonParser *parser;
	JsonObject *ticket;

	if (purple_xfer_is_cancelled(xfer))
		return;

	edisc_xfer->hc = NULL;

	if (!purple_http_response_is_successful(response)) {
		int error_id = ggp_edisc_parse_error(data);
		if (error_id == 206) /* recipient not logged in */
			ggp_edisc_xfer_error(xfer,
				_("Recipient not logged in"));
		else if (error_id == 207) /* bad sender recipient relation */
			ggp_edisc_xfer_error(xfer, _("Recipient didn't added "
				"you to his buddy list"));
		else
			ggp_edisc_xfer_error(xfer,
				_("Cannot offer sending a file"));
		return;
	}

	parser = ggp_json_parse(data);
	ticket = json_node_get_object(json_parser_get_root(parser));
	ticket = json_object_get_object_member(ticket, "result");
	ticket = json_object_get_object_member(ticket, "send_ticket");
	edisc_xfer->ticket_id = g_strdup(json_object_get_string_member(
		ticket, "id"));
	ack_status = ggp_edisc_xfer_parse_ack_status(
		json_object_get_string_member(ticket, "ack_status"));
	/* send_mode: "normal", "publink" (for legacy clients) */

	g_object_unref(parser);

	if (edisc_xfer->ticket_id == NULL) {
		purple_debug_error("gg",
			"ggp_edisc_xfer_send_init_ticket_created: "
			"couldn't get ticket id\n");
		return;
	}

	purple_debug_info("gg", "ggp_edisc_xfer_send_init_ticket_created: "
		"ticket \"%s\" created\n", edisc_xfer->ticket_id);

	g_hash_table_insert(sdata->xfers_initialized,
		edisc_xfer->ticket_id, xfer);
	g_hash_table_insert(sdata->xfers_history,
		g_strdup(edisc_xfer->ticket_id), GINT_TO_POINTER(1));

	if (ack_status != GGP_EDISC_XFER_ACK_STATUS_UNKNOWN)
		ggp_edisc_xfer_send_ticket_changed(edisc_xfer->gc, xfer,
			ack_status == GGP_EDISC_XFER_ACK_STATUS_ALLOWED);
}

void ggp_edisc_xfer_send_ticket_changed(PurpleConnection *gc, PurpleXfer *xfer,
	gboolean is_allowed)
{
	ggp_edisc_xfer *edisc_xfer = purple_xfer_get_protocol_data(xfer);
	if (!edisc_xfer) {
		purple_debug_fatal("gg", "ggp_edisc_event_ticket_changed: "
			"transfer %p already free'd\n", xfer);
		return;
	}

	if (!is_allowed) {
		purple_debug_info("gg", "ggp_edisc_event_ticket_changed: "
			"transfer %p rejected\n", xfer);
		purple_xfer_cancel_remote(xfer);
		ggp_edisc_xfer_free(xfer);
		return;
	}

	if (edisc_xfer->allowed) {
		purple_debug_misc("gg", "ggp_edisc_event_ticket_changed: "
			"transfer %p already allowed\n", xfer);
		return;
	}
	edisc_xfer->allowed = TRUE;

	purple_xfer_start(xfer, -1, NULL, 0);
}

static void ggp_edisc_xfer_send_reader(PurpleHttpConnection *hc,
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
		purple_debug_error("gg", "ggp_edisc_xfer_send_reader: "
			"Invalid offset (%d != %" G_GSIZE_FORMAT ")\n",
			edisc_xfer->already_read, offset);
		ggp_edisc_xfer_error(xfer, _("Error while reading a file"));
		return;
	}

	if (xfer->dest_fp == NULL)
		stored = -1;
	else
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

static void ggp_edisc_xfer_send_start(PurpleXfer *xfer)
{
	ggp_edisc_session_data *sdata;
	ggp_edisc_xfer *edisc_xfer;
	gchar *upload_url, *filename_e;
	PurpleHttpRequest *req;

	g_return_if_fail(xfer != NULL);
	edisc_xfer = purple_xfer_get_protocol_data(xfer);
	g_return_if_fail(edisc_xfer != NULL);
	sdata = ggp_edisc_get_sdata(edisc_xfer->gc);

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
	purple_http_request_header_set(req, "X-gged-metadata",
		"{\"node_type\": \"file\"}");

	purple_http_request_set_contents_reader(req, ggp_edisc_xfer_send_reader,
		purple_xfer_get_size(xfer), xfer);

	edisc_xfer->hc = purple_http_request(edisc_xfer->gc, req,
		ggp_edisc_xfer_send_done, xfer);
	purple_http_request_unref(req);

	purple_http_conn_set_progress_watcher(edisc_xfer->hc,
		ggp_edisc_xfer_progress_watcher, xfer, 250000);
}

static void ggp_edisc_xfer_send_done(PurpleHttpConnection *hc,
	PurpleHttpResponse *response, gpointer _xfer)
{
	PurpleXfer *xfer = _xfer;
	ggp_edisc_xfer *edisc_xfer = purple_xfer_get_protocol_data(xfer);
	const gchar *data = purple_http_response_get_data(response, NULL);
	JsonParser *parser;
	JsonObject *result;
	int result_status = -1;

	if (purple_xfer_is_cancelled(xfer))
		return;

	g_return_if_fail(edisc_xfer != NULL);

	edisc_xfer->hc = NULL;

	if (!purple_http_response_is_successful(response)) {
		ggp_edisc_xfer_error(xfer, _("Error while sending a file"));
		return;
	}

	parser = ggp_json_parse(data);
	result = json_node_get_object(json_parser_get_root(parser));
	result = json_object_get_object_member(result, "result");
	if (json_object_has_member(result, "status"))
		result_status = json_object_get_int_member(result, "status");
	g_object_unref(parser);

	if (result_status == 0) {
		purple_xfer_set_completed(xfer, TRUE);
		purple_xfer_end(xfer);
		ggp_edisc_xfer_free(xfer);
	} else
		ggp_edisc_xfer_error(xfer, _("Error while sending a file"));
}

PurpleXfer * ggp_edisc_xfer_send_new(PurpleConnection *gc, const char *who)
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

	purple_xfer_set_init_fnc(xfer, ggp_edisc_xfer_send_init);
	purple_xfer_set_start_fnc(xfer, ggp_edisc_xfer_send_start);
	purple_xfer_set_cancel_send_fnc(xfer, ggp_edisc_xfer_cancel);

	return xfer;
}

void ggp_edisc_xfer_send_file(PurpleConnection *gc, const char *who,
	const char *filename)
{
	PurpleXfer *xfer;

	g_return_if_fail(gc != NULL);
	g_return_if_fail(who != NULL);

	/* Nothing interesting here, this code is common among prpls.
	 * See ggp_edisc_xfer_send_new. */

	xfer = ggp_edisc_xfer_send_new(gc, who);
	if (filename)
		purple_xfer_request_accepted(xfer, filename);
	else
		purple_xfer_request(xfer);
}

/*******************************************************************************
 * Receiving a file.
 ******************************************************************************/

static void ggp_edisc_xfer_recv_ticket_update_authenticated(
	PurpleConnection *gc, gboolean success, gpointer _ticket);
static void ggp_edisc_xfer_recv_ticket_update_got(PurpleHttpConnection *hc,
	PurpleHttpResponse *response, gpointer user_data);
static PurpleXfer * ggp_edisc_xfer_recv_new(PurpleConnection *gc,
	const char *who);
static void ggp_edisc_xfer_recv_accept(PurpleXfer *xfer);
static void ggp_edisc_xfer_recv_start(PurpleXfer *xfer);
static void ggp_edisc_xfer_recv_ack(PurpleXfer *xfer, gboolean accept);
static void ggp_edisc_xfer_recv_ack_done(PurpleHttpConnection *hc,
	PurpleHttpResponse *response, gpointer _xfer);
static gboolean ggp_edisc_xfer_recv_writer(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, const gchar *buffer, size_t offset,
	size_t length, gpointer user_data);
static void ggp_edisc_xfer_recv_done(PurpleHttpConnection *hc,
	PurpleHttpResponse *response, gpointer _xfer);

static void ggp_edisc_xfer_recv_ticket_got(PurpleConnection *gc,
	const gchar *ticket_id)
{
	ggp_edisc_session_data *sdata = ggp_edisc_get_sdata(gc);

	if (g_hash_table_lookup(sdata->xfers_history, ticket_id))
		return;

	ggp_ggdrive_auth(gc, ggp_edisc_xfer_recv_ticket_update_authenticated,
		g_strdup(ticket_id));
}

static void ggp_edisc_xfer_recv_ticket_update_authenticated(
	PurpleConnection *gc, gboolean success, gpointer _ticket)
{
	ggp_edisc_session_data *sdata = ggp_edisc_get_sdata(gc);
	PurpleHttpRequest *req;
	gchar *ticket = _ticket;

	if (!success) {
		purple_debug_warning("gg",
			"ggp_edisc_xfer_recv_ticket_update_authenticated: "
			"update of ticket %s aborted due to authentication "
			"failure\n", ticket);
		g_free(ticket);
		return;
	}

	req = purple_http_request_new(ggp_edisc_xfer_ticket_url(ticket));
	g_free(ticket);

	ggp_edisc_set_defaults(req);
	purple_http_request_set_cookie_jar(req, sdata->cookies);

	purple_http_request_header_set(req, "X-gged-security-token",
		sdata->security_token);

	purple_http_request(gc, req, ggp_edisc_xfer_recv_ticket_update_got,
		NULL);
	purple_http_request_unref(req);
}

static void ggp_edisc_xfer_recv_ticket_update_got(PurpleHttpConnection *hc,
	PurpleHttpResponse *response, gpointer user_data)
{
	PurpleConnection *gc = purple_http_conn_get_purple_connection(hc);
	PurpleXfer *xfer;
	ggp_edisc_xfer *edisc_xfer;
	JsonParser *parser;
	JsonObject *result;
	int status = -1;
	ggp_edisc_session_data *sdata;

	const gchar *ticket_id, *file_name, *send_mode_str;
	uin_t sender, recipient;
	int file_size;

	if (!purple_http_response_is_successful(response)) {
		purple_debug_error("gg",
			"ggp_edisc_xfer_recv_ticket_update_got: "
			"cannot fetch update for ticket (code=%d)\n",
			purple_http_response_get_code(response));
		return;
	}

	sdata = ggp_edisc_get_sdata(gc);

	parser = ggp_json_parse(purple_http_response_get_data(response, NULL));
	result = json_node_get_object(json_parser_get_root(parser));
	result = json_object_get_object_member(result, "result");
	if (json_object_has_member(result, "status"))
		status = json_object_get_int_member(result, "status");
	result = json_object_get_object_member(result, "send_ticket");

	if (status != 0) {
		purple_debug_warning("gg",
			"ggp_edisc_xfer_recv_ticket_update_got: failed to get "
			"update (status=%d)\n", status);
		g_object_unref(parser);
		return;
	}

	ticket_id = json_object_get_string_member(result, "id");
	sender = ggp_str_to_uin(json_object_get_string_member(result,
		"sender"));
	recipient = ggp_str_to_uin(json_object_get_string_member(result,
		"recipient"));
	file_size = g_ascii_strtoll(json_object_get_string_member(result,
		"file_size"), NULL, 10);
	file_name = json_object_get_string_member(result, "file_name");

	/* GG11: normal
	 * AQQ 2.4.2.10: direct_inbox
	 */
	send_mode_str = json_object_get_string_member(result, "send_mode");

	/* more fields:
	 * send_progress (float), ack_status, send_status
	 */

	if (purple_debug_is_verbose() && purple_debug_is_unsafe())
		purple_debug_info("gg", "Got ticket update: id=%s, sender=%u, "
			"recipient=%u, file name=\"%s\", file size=%d, "
			"send mode=%s)\n",
			ticket_id,
			sender, recipient,
			file_name, file_size,
			send_mode_str);

	xfer = g_hash_table_lookup(sdata->xfers_initialized, ticket_id);
	if (xfer != NULL) {
		purple_debug_misc("gg", "ggp_edisc_xfer_recv_ticket_update_got:"
			" ticket %s already updated\n",
			purple_debug_is_unsafe() ? ticket_id : "");
		g_object_unref(parser);
		return;
	}

	if (recipient != ggp_get_my_uin(gc)) {
		purple_debug_misc("gg", "ggp_edisc_xfer_recv_ticket_update_got:"
			" ticket %s is not for incoming transfer "
			"(its from %u to %u)\n",
			purple_debug_is_unsafe() ? ticket_id : "",
			sender, recipient);
		g_object_unref(parser);
		return;
	}

	xfer = ggp_edisc_xfer_recv_new(gc, ggp_uin_to_str(sender));
	purple_xfer_set_filename(xfer, file_name);
	purple_xfer_set_size(xfer, file_size);
	purple_xfer_request(xfer);
	edisc_xfer = purple_xfer_get_protocol_data(xfer);
	edisc_xfer->ticket_id = g_strdup(ticket_id);
	g_hash_table_insert(sdata->xfers_initialized,
		edisc_xfer->ticket_id, xfer);
	g_hash_table_insert(sdata->xfers_history,
		g_strdup(ticket_id), GINT_TO_POINTER(1));

	g_object_unref(parser);
}

static PurpleXfer * ggp_edisc_xfer_recv_new(PurpleConnection *gc,
	const char *who)
{
	PurpleXfer *xfer;
	ggp_edisc_xfer *edisc_xfer;

	g_return_val_if_fail(gc != NULL, NULL);
	g_return_val_if_fail(who != NULL, NULL);

	xfer = purple_xfer_new(purple_connection_get_account(gc),
		PURPLE_XFER_RECEIVE, who);
	edisc_xfer = g_new0(ggp_edisc_xfer, 1);
	purple_xfer_set_protocol_data(xfer, edisc_xfer);

	edisc_xfer->gc = gc;

	purple_xfer_set_init_fnc(xfer, ggp_edisc_xfer_recv_accept);
	purple_xfer_set_request_denied_fnc(xfer, ggp_edisc_xfer_recv_reject);
	purple_xfer_set_start_fnc(xfer, ggp_edisc_xfer_recv_start);
	purple_xfer_set_cancel_recv_fnc(xfer, ggp_edisc_xfer_cancel);

	return xfer;
}

static void ggp_edisc_xfer_recv_reject(PurpleXfer *xfer)
{
	ggp_edisc_xfer_recv_ack(xfer, FALSE);
}

static void ggp_edisc_xfer_recv_accept(PurpleXfer *xfer)
{
	ggp_edisc_xfer_recv_ack(xfer, TRUE);
}

static void ggp_edisc_xfer_recv_ack(PurpleXfer *xfer, gboolean accept)
{
	ggp_edisc_xfer *edisc_xfer = purple_xfer_get_protocol_data(xfer);
	ggp_edisc_session_data *sdata = ggp_edisc_get_sdata(edisc_xfer->gc);
	PurpleHttpRequest *req;

	edisc_xfer->allowed = accept;

	req = purple_http_request_new(ggp_edisc_xfer_ticket_url(
		edisc_xfer->ticket_id));
	purple_http_request_set_method(req, "PUT");

	ggp_edisc_set_defaults(req);
	purple_http_request_set_cookie_jar(req, sdata->cookies);
	purple_http_request_header_set(req, "X-gged-security-token",
		sdata->security_token);

	purple_http_request_header_set(req, "X-gged-ack-status",
		accept ? "allow" : "reject");

	edisc_xfer->hc = purple_http_request(edisc_xfer->gc, req,
		accept ? ggp_edisc_xfer_recv_ack_done : NULL, xfer);
	purple_http_request_unref(req);

	if (!accept) {
		edisc_xfer->hc = NULL;
		ggp_edisc_xfer_free(xfer);
	}
}

static void ggp_edisc_xfer_recv_ack_done(PurpleHttpConnection *hc,
	PurpleHttpResponse *response, gpointer _xfer)
{
	PurpleXfer *xfer = _xfer;
	ggp_edisc_xfer *edisc_xfer;

	if (purple_xfer_is_cancelled(xfer))
		g_return_if_reached();

	edisc_xfer = purple_xfer_get_protocol_data(xfer);
	edisc_xfer->hc = NULL;

	if (!purple_http_response_is_successful(response)) {
		ggp_edisc_xfer_error(xfer, _("Cannot confirm file transfer."));
		return;
	}

	purple_debug_info("gg", "ggp_edisc_xfer_recv_ack_done: [%s]\n",
		purple_http_response_get_data(response, NULL));
}

static void ggp_edisc_xfer_recv_ticket_completed(PurpleXfer *xfer)
{
	ggp_edisc_xfer *edisc_xfer = purple_xfer_get_protocol_data(xfer);

	if (edisc_xfer->ready)
		return;
	edisc_xfer->ready = TRUE;

	purple_xfer_start(xfer, -1, NULL, 0);
}

static void ggp_edisc_xfer_recv_start(PurpleXfer *xfer)
{
	ggp_edisc_session_data *sdata;
	ggp_edisc_xfer *edisc_xfer;
	gchar *upload_url;
	PurpleHttpRequest *req;

	g_return_if_fail(xfer != NULL);
	edisc_xfer = purple_xfer_get_protocol_data(xfer);
	g_return_if_fail(edisc_xfer != NULL);
	sdata = ggp_edisc_get_sdata(edisc_xfer->gc);

	upload_url = g_strdup_printf("https://drive.mpa.gg.pl/me/file/inbox/"
		"%s,%s?api_version=%s&security_token=%s",
		edisc_xfer->ticket_id, purple_url_encode(purple_xfer_get_filename(xfer)),
		GGP_EDISC_API, sdata->security_token);
	req = purple_http_request_new(upload_url);
	g_free(upload_url);

	purple_http_request_set_timeout(req, -1);

	ggp_edisc_set_defaults(req);
	purple_http_request_set_max_len(req, purple_xfer_get_size(xfer) + 1);
	purple_http_request_set_cookie_jar(req, sdata->cookies);

	purple_http_request_set_response_writer(req, ggp_edisc_xfer_recv_writer,
		xfer);

	edisc_xfer->hc = purple_http_request(edisc_xfer->gc, req,
		ggp_edisc_xfer_recv_done, xfer);
	purple_http_request_unref(req);

	purple_http_conn_set_progress_watcher(edisc_xfer->hc,
		ggp_edisc_xfer_progress_watcher, xfer, 250000);
}

static gboolean ggp_edisc_xfer_recv_writer(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, const gchar *buffer, size_t offset,
	size_t length, gpointer _xfer)
{
	PurpleXfer *xfer = _xfer;
	ggp_edisc_xfer *edisc_xfer;
	gssize stored;

	g_return_val_if_fail(xfer != NULL, FALSE);
	edisc_xfer = purple_xfer_get_protocol_data(xfer);
	g_return_val_if_fail(edisc_xfer != NULL, FALSE);

	if (xfer->dest_fp == NULL)
		stored = -1;
	else
		stored = fwrite(buffer, 1, length, xfer->dest_fp);

	if (stored < 0 || (gsize)stored != length) {
		purple_debug_error("gg", "ggp_edisc_xfer_recv_writer: "
			"saved too less\n");
		return FALSE;
	}

	if (stored > purple_xfer_get_bytes_remaining(xfer)) {
		purple_debug_error("gg", "ggp_edisc_xfer_recv_writer: "
			"saved too much (%d > %d)\n",
			stored, (int)purple_xfer_get_bytes_remaining(xfer));
		return FALSE;
	}

	/* May look redundant with ggp_edisc_xfer_progress_watcher,
	 * but it isn't!
	 */
	purple_xfer_set_bytes_sent(xfer,
		purple_xfer_get_bytes_sent(xfer) + stored);

	return TRUE;
}

static void ggp_edisc_xfer_recv_done(PurpleHttpConnection *hc,
	PurpleHttpResponse *response, gpointer _xfer)
{
	PurpleXfer *xfer = _xfer;
	ggp_edisc_xfer *edisc_xfer = purple_xfer_get_protocol_data(xfer);

	if (purple_xfer_is_cancelled(xfer))
		return;

	g_return_if_fail(edisc_xfer != NULL);

	edisc_xfer->hc = NULL;

	if (!purple_http_response_is_successful(response)) {
		ggp_edisc_xfer_error(xfer, _("Error while receiving a file"));
		return;
	}

	if (purple_xfer_get_bytes_remaining(xfer) == 0) {
		purple_xfer_set_completed(xfer, TRUE);
		purple_xfer_end(xfer);
		ggp_edisc_xfer_free(xfer);
	} else {
		purple_debug_warning("gg", "ggp_edisc_xfer_recv_done: didn't "
			"received everything\n");
		ggp_edisc_xfer_error(xfer, _("Error while receiving a file"));
	}
}

/*******************************************************************************
 * Authentication.
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
	JsonParser *parser;
	JsonObject *result;
	int status = -1;

	sdata->auth_request = NULL;

	if (!purple_http_response_is_successful(response)) {
		purple_debug_misc("gg", "ggp_ggdrive_auth_done: authentication "
			"failed due to unsuccessful request (code = %d)\n",
			purple_http_response_get_code(response));
		ggp_ggdrive_auth_results(gc, FALSE);
		return;
	}

	parser = ggp_json_parse(purple_http_response_get_data(response, NULL));
	result = json_node_get_object(json_parser_get_root(parser));
	result = json_object_get_object_member(result, "result");
	if (json_object_has_member(result, "status"))
		status = json_object_get_int_member(result, "status");
	g_object_unref(parser);

	if (status != 0 ) {
		purple_debug_misc("gg", "ggp_ggdrive_auth_done: authentication "
			"failed due to bad result (status=%d)\n", status);
		if (purple_debug_is_verbose())
			purple_debug_misc("gg", "ggp_ggdrive_auth_done: "
				"result = %s\n",
				purple_http_response_get_data(response, NULL));
		ggp_ggdrive_auth_results(gc, FALSE);
		return;
	}

	sdata->security_token = g_strdup(purple_http_response_get_header(
		response, "X-gged-security-token"));
	if (!sdata->security_token) {
		purple_debug_misc("gg", "ggp_ggdrive_auth_done: authentication "
			"failed due to missing security token header\n");
		ggp_ggdrive_auth_results(gc, FALSE);
		return;
	}

	if (purple_debug_is_unsafe())
		purple_debug_misc("gg", "ggp_ggdrive_auth_done: "
			"security_token=%s\n", sdata->security_token);
	ggp_ggdrive_auth_results(gc, TRUE);
}
