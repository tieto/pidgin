/*
 * purple - Jabber Protocol Plugin
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 *
 */
#include "internal.h"
#include "core.h"
#include "debug.h"
#include "http.h"

#include "bosh.h"

/*
TODO: test, what happens, if the http server (BOSH server) doesn't support
keep-alive (sends connection: close).
*/

#define JABBER_BOSH_SEND_DELAY 250

#define JABBER_BOSH_TIMEOUT 10

static gchar *jabber_bosh_useragent = NULL;

struct _PurpleJabberBOSHConnection {
	JabberStream *js;
	PurpleHttpKeepalivePool *kapool;
	PurpleHttpConnection *sc_req; /* Session Creation Request */
	PurpleHttpConnectionSet *payload_reqs;

	gchar *url;
	gboolean is_ssl;
	gboolean is_terminating;

	gchar *sid;
	guint64 rid; /* Must be big enough to hold 2^53 - 1 */

	GString *send_buff;
	guint send_timer;
};

static PurpleHttpRequest *
jabber_bosh_connection_http_request_new(PurpleJabberBOSHConnection *conn,
	const GString *data);
static void
jabber_bosh_connection_session_create(PurpleJabberBOSHConnection *conn);
static void
jabber_bosh_connection_send_now(PurpleJabberBOSHConnection *conn);

void
jabber_bosh_init(void)
{
	GHashTable *ui_info = purple_core_get_ui_info();
	const gchar *ui_name = NULL;
	const gchar *ui_version = NULL;

	if (ui_info) {
		ui_name = g_hash_table_lookup(ui_info, "name");
		ui_version = g_hash_table_lookup(ui_info, "version");
	}

	if (ui_name) {
		jabber_bosh_useragent = g_strdup_printf(
			"%s%s%s (libpurple " VERSION ")", ui_name,
			ui_version ? " " : "", ui_version ? ui_version : "");
	} else
		jabber_bosh_useragent = g_strdup("libpurple " VERSION);
}

void jabber_bosh_uninit(void)
{
	g_free(jabber_bosh_useragent);
	jabber_bosh_useragent = NULL;
}

PurpleJabberBOSHConnection*
jabber_bosh_connection_new(JabberStream *js, const gchar *url)
{
	PurpleJabberBOSHConnection *conn;
	PurpleHttpURL *url_p;

	url_p = purple_http_url_parse(url);
	if (!url_p) {
		purple_debug_error("jabber-bosh", "Unable to parse given URL.\n");
		return NULL;
	}

	conn = g_new0(PurpleJabberBOSHConnection, 1);
	conn->kapool = purple_http_keepalive_pool_new();
	conn->payload_reqs = purple_http_connection_set_new();
	purple_http_keepalive_pool_set_limit_per_host(conn->kapool, 2);
	conn->url = g_strdup(url);
	conn->js = js;
	conn->is_ssl = (g_ascii_strcasecmp("https",
		purple_http_url_get_protocol(url_p)) == 0);
	conn->send_buff = g_string_new(NULL);

	/*
	 * Random 64-bit integer masked off by 2^52 - 1.
	 *
	 * This should produce a random integer in the range [0, 2^52). It's
	 * unlikely we'll send enough packets in one session to overflow the
	 * rid.
	 */
	conn->rid = (((guint64)g_random_int() << 32) | g_random_int());
	conn->rid &= 0xFFFFFFFFFFFFFLL;

	if (purple_ip_address_is_valid(purple_http_url_get_host(url_p)))
		js->serverFQDN = g_strdup(js->user->domain);
	else
		js->serverFQDN = g_strdup(purple_http_url_get_host(url_p));

	purple_http_url_free(url_p);

	jabber_bosh_connection_session_create(conn);

	return conn;
}

void
jabber_bosh_connection_destroy(PurpleJabberBOSHConnection *conn)
{
	if (conn == NULL || conn->is_terminating)
		return;
	conn->is_terminating = TRUE;

	if (conn->sid != NULL) {
		purple_debug_info("jabber-bosh",
			"Terminating a session for %p\n", conn);
		jabber_bosh_connection_send_now(conn);
	}

	purple_http_connection_set_destroy(conn->payload_reqs);
	conn->payload_reqs = NULL;

	if (conn->send_timer)
		purple_timeout_remove(conn->send_timer);

	purple_http_conn_cancel(conn->sc_req);
	conn->sc_req = NULL;

	purple_http_keepalive_pool_unref(conn->kapool);
	conn->kapool = NULL;
	g_string_free(conn->send_buff, TRUE);
	conn->send_buff = NULL;

	g_free(conn->sid);
	conn->sid = NULL;
	g_free(conn->url);
	conn->url = NULL;

	g_free(conn);
}

gboolean
jabber_bosh_connection_is_ssl(const PurpleJabberBOSHConnection *conn)
{
	return conn->is_ssl;
}

static xmlnode *
jabber_bosh_connection_parse(PurpleJabberBOSHConnection *conn,
	PurpleHttpResponse *response)
{
	xmlnode *root;
	const gchar *data;
	size_t data_len;
	const gchar *type;

	g_return_val_if_fail(conn != NULL, NULL);
	g_return_val_if_fail(response != NULL, NULL);

	if (conn->is_terminating || purple_account_is_disconnecting(
		purple_connection_get_account(conn->js->gc)))
	{
		return NULL;
	}

	if (!purple_http_response_is_successful(response)) {
		purple_connection_error(conn->js->gc,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			_("Unable to connect"));
		return NULL;
	}

	data = purple_http_response_get_data(response, &data_len);
	root = xmlnode_from_str(data, data_len);

	type = xmlnode_get_attrib(root, "type");
	if (g_strcmp0(type, "terminate") == 0) {
		purple_connection_error(conn->js->gc,
			PURPLE_CONNECTION_ERROR_OTHER_ERROR, _("The BOSH "
			"connection manager terminated your session."));
		xmlnode_free(root);
		return NULL;
	}

	return root;
}

static void
jabber_bosh_connection_recv(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, gpointer _bosh_conn)
{
	PurpleJabberBOSHConnection *bosh_conn = _bosh_conn;
	xmlnode *node, *child;

	if (purple_debug_is_verbose() && purple_debug_is_unsafe()) {
		purple_debug_misc("jabber-bosh", "received: %s\n",
			purple_http_response_get_data(response, NULL));
	}

	node = jabber_bosh_connection_parse(bosh_conn, response);
	if (node == NULL)
		return;

	child = node->child;
	while (child != NULL) {
		/* jabber_process_packet might free child */
		xmlnode *next = child->next;
		const gchar *xmlns;

		if (child->type != XMLNODE_TYPE_TAG) {
			child = next;
			continue;
		}

		/* Workaround for non-compliant servers that don't stamp
		 * the right xmlns on these packets. See #11315.
		 */
		xmlns = xmlnode_get_namespace(child);
		if ((xmlns == NULL || g_strcmp0(xmlns, NS_BOSH) == 0) &&
			(g_strcmp0(child->name, "iq") == 0 ||
			g_strcmp0(child->name, "message") == 0 ||
			g_strcmp0(child->name, "presence") == 0))
		{
			xmlnode_set_namespace(child, NS_XMPP_CLIENT);
		}

		jabber_process_packet(bosh_conn->js, &child);

		child = next;
	}

	jabber_bosh_connection_send(bosh_conn, NULL);
}

static void
jabber_bosh_connection_send_now(PurpleJabberBOSHConnection *conn)
{
	PurpleHttpRequest *req;
	GString *data;

	g_return_if_fail(conn != NULL);

	if (conn->send_timer != 0) {
		purple_timeout_remove(conn->send_timer);
		conn->send_timer = 0;
	}

	if (conn->sid == NULL)
		return;

	data = g_string_new(NULL);

	/* missing parameters: route, from, ack */
	g_string_printf(data, "<body "
		"rid='%" G_GUINT64_FORMAT "' "
		"sid='%s' "
		"xmlns='" NS_BOSH "' "
		"xmlns:xmpp='" NS_XMPP_BOSH "' ",
		++conn->rid, conn->sid);

	if (conn->js->reinit && !conn->is_terminating) {
		g_string_append(data, "xmpp:restart='true'/>");
		conn->js->reinit = FALSE;
	} else {
		if (conn->is_terminating)
			g_string_append(data, "type='terminate' ");
		g_string_append_c(data, '>');
		g_string_append_len(data, conn->send_buff->str,
			conn->send_buff->len);
		g_string_append(data, "</body>");
		g_string_set_size(conn->send_buff, 0);
	}

	if (purple_debug_is_verbose() && purple_debug_is_unsafe())
		purple_debug_misc("jabber-bosh", "sending: %s\n", data->str);

	req = jabber_bosh_connection_http_request_new(conn, data);
	g_string_free(data, TRUE);

	if (conn->is_terminating) {
		purple_http_request(NULL, req, NULL, NULL);
		g_free(conn->sid);
		conn->sid = NULL;
	} else {
		purple_http_connection_set_add(conn->payload_reqs,
			purple_http_request(conn->js->gc, req,
				jabber_bosh_connection_recv, conn));
	}

	purple_http_request_unref(req);
}

static gboolean
jabber_bosh_connection_send_delayed(gpointer _conn)
{
	PurpleJabberBOSHConnection *conn = _conn;

	conn->send_timer = 0;
	jabber_bosh_connection_send_now(conn);

	return FALSE;
}

void
jabber_bosh_connection_send(PurpleJabberBOSHConnection *conn,
	const gchar *data)
{
	g_return_if_fail(conn != NULL);

	if (data)
		g_string_append(conn->send_buff, data);

	if (conn->send_timer == 0) {
		conn->send_timer = purple_timeout_add(
			JABBER_BOSH_SEND_DELAY,
			jabber_bosh_connection_send_delayed, conn);
	}
}

void
jabber_bosh_connection_send_keepalive(PurpleJabberBOSHConnection *conn)
{
	g_return_if_fail(conn != NULL);

	jabber_bosh_connection_send_now(conn);
}

static gboolean
jabber_bosh_version_check(const gchar *version, int major_req, int minor_min)
{
	const gchar *dot;
	int major, minor = 0;

	if (version == NULL)
		return FALSE;

	major = atoi(version);
	dot = strchr(version, '.');
	if (dot)
		minor = atoi(dot + 1);

	if (major != major_req)
		return FALSE;
	if (minor < minor_min)
		return FALSE;
	return TRUE;
}

static void
jabber_bosh_connection_session_created(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, gpointer _bosh_conn)
{
	PurpleJabberBOSHConnection *bosh_conn = _bosh_conn;
	xmlnode *node, *features;
	const gchar *sid, *ver, *inactivity_str;
	int inactivity = 0;

	bosh_conn->sc_req = NULL;

	if (purple_debug_is_verbose() && purple_debug_is_unsafe()) {
		purple_debug_misc("jabber-bosh",
			"received (session creation): %s\n",
			purple_http_response_get_data(response, NULL));
	}

	node = jabber_bosh_connection_parse(bosh_conn, response);
	if (node == NULL)
		return;

	sid = xmlnode_get_attrib(node, "sid");
	ver = xmlnode_get_attrib(node, "ver");
	inactivity_str = xmlnode_get_attrib(node, "inactivity");
	/* requests = xmlnode_get_attrib(node, "requests"); */

	if (!sid) {
		purple_connection_error(bosh_conn->js->gc,
			PURPLE_CONNECTION_ERROR_OTHER_ERROR,
			_("No BOSH session ID given"));
		xmlnode_free(node);
		return;
	}

	if (ver == NULL) {
		purple_debug_info("jabber-bosh", "Missing version in BOSH initiation\n");
	} else if (!jabber_bosh_version_check(ver, 1, 6)) {
		purple_debug_error("jabber-bosh",
			"Unsupported BOSH version: %s\n", ver);
		purple_connection_error(bosh_conn->js->gc,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			_("Unsupported version of BOSH protocol"));
		xmlnode_free(node);
		return;
	}

	purple_debug_misc("jabber-bosh", "Session created for %p\n", bosh_conn);

	bosh_conn->sid = g_strdup(sid);

	if (inactivity_str)
		inactivity = atoi(inactivity_str);
	if (inactivity < 0 || inactivity > 3600) {
		purple_debug_warning("jabber-bosh", "Ignoring invalid "
			"inactivity value: %s\n", inactivity_str);
		inactivity = 0;
	}
	if (inactivity > 0) {
		inactivity -= 5; /* rounding */
		if (inactivity <= 0)
			inactivity = 1;
		bosh_conn->js->max_inactivity = inactivity;
		if (bosh_conn->js->inactivity_timer == 0) {
			purple_debug_misc("jabber-bosh", "Starting inactivity "
				"timer for %d secs (compensating for "
				"rounding)\n", inactivity);
			jabber_stream_restart_inactivity_timer(bosh_conn->js);
		}
	}

	jabber_stream_set_state(bosh_conn->js, JABBER_STREAM_AUTHENTICATING);

	/* FIXME: Depending on receiving features might break with some hosts */
	features = xmlnode_get_child(node, "features");
	jabber_stream_features_parse(bosh_conn->js, features);

	xmlnode_free(node);

	jabber_bosh_connection_send(bosh_conn, NULL);
}

static void
jabber_bosh_connection_session_create(PurpleJabberBOSHConnection *conn)
{
	PurpleHttpRequest *req;
	GString *data;

	g_return_if_fail(conn != NULL);

	if (conn->sid || conn->sc_req)
		return;

	purple_debug_misc("jabber-bosh", "Requesting Session Create for %p\n",
		conn);

	data = g_string_new(NULL);

	/* missing optional parameters: route, from, ack */
	g_string_printf(data, "<body content='text/xml; charset=utf-8' "
		"rid='%" G_GUINT64_FORMAT "' "
		"to='%s' "
		"xml:lang='en' "
		"ver='1.10' "
		"wait='%d' "
		"hold='1' "
		"xmlns='" NS_BOSH "' "
		"xmpp:version='1.0' "
		"xmlns:xmpp='urn:xmpp:xbosh' "
		"/>",
		++conn->rid, conn->js->user->domain, JABBER_BOSH_TIMEOUT);

	req = jabber_bosh_connection_http_request_new(conn, data);
	g_string_free(data, TRUE);

	conn->sc_req = purple_http_request(conn->js->gc, req,
		jabber_bosh_connection_session_created, conn);

	purple_http_request_unref(req);
}

static PurpleHttpRequest *
jabber_bosh_connection_http_request_new(PurpleJabberBOSHConnection *conn,
	const GString *data)
{
	PurpleHttpRequest *req;

	jabber_stream_restart_inactivity_timer(conn->js);

	req = purple_http_request_new(conn->url);
	purple_http_request_set_keepalive_pool(req, conn->kapool);
	purple_http_request_set_method(req, "POST");
	purple_http_request_set_timeout(req, JABBER_BOSH_TIMEOUT + 2);
	purple_http_request_header_set(req, "User-Agent",
		jabber_bosh_useragent);
	purple_http_request_header_set(req, "Content-Encoding",
		"text/xml; charset=utf-8");
	purple_http_request_set_contents(req, data->str, data->len);

	return req;
}
