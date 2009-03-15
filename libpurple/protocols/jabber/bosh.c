/*
 * purple - Jabber Protocol Plugin
 *
 * Copyright (C) 2008, Tobias Markmann <tmarkmann@googlemail.com>
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
#include "internal.h"
#include "core.h"
#include "cipher.h"
#include "debug.h"
#include "prpl.h"
#include "util.h"
#include "xmlnode.h"

#include "bosh.h"

#define MAX_HTTP_CONNECTIONS      2

typedef struct _PurpleHTTPConnection PurpleHTTPConnection;

typedef void (*PurpleHTTPConnectionConnectFunction)(PurpleHTTPConnection *conn);
typedef void (*PurpleHTTPConnectionDisconnectFunction)(PurpleHTTPConnection *conn);
typedef void (*PurpleBOSHConnectionConnectFunction)(PurpleBOSHConnection *conn);
typedef void (*PurpleBOSHConnectionReceiveFunction)(PurpleBOSHConnection *conn, xmlnode *node);

static char *bosh_useragent = NULL;

typedef enum {
	PACKET_TERMINATE,
	PACKET_STREAM_RESTART,
	PACKET_NORMAL,
} PurpleBOSHPacketType;

struct _PurpleBOSHConnection {
    /* decoded URL */
    char *host;
    int port;
    char *path; 

    /* Must be big enough to hold 2^53 - 1 */
    guint64 rid;
    char *sid;
    int wait;

    JabberStream *js;
    gboolean pipelining;
	PurpleHTTPConnection *connections[MAX_HTTP_CONNECTIONS];

	int max_inactivity;
	int max_requests;
	int requests;
	GString *pending;

    gboolean ready;
    PurpleBOSHConnectionConnectFunction connect_cb;
    PurpleBOSHConnectionReceiveFunction receive_cb;
};

struct _PurpleHTTPConnection {
    int fd;
	gboolean ready;
    char *host;
    int port;
    int ie_handle;
	int requests; /* number of outstanding HTTP requests */

    GString *buf;
    gboolean headers_done;
    gsize handled_len;
    gsize body_len;

    int pih; /* what? */
    PurpleHTTPConnectionConnectFunction connect_cb;
    PurpleHTTPConnectionConnectFunction disconnect_cb;
    PurpleBOSHConnection *bosh;
};

static void jabber_bosh_connection_stream_restart(PurpleBOSHConnection *conn);
static gboolean jabber_bosh_connection_error_check(PurpleBOSHConnection *conn, xmlnode *node);
static void jabber_bosh_connection_received(PurpleBOSHConnection *conn, xmlnode *node);
static void jabber_bosh_connection_send_native(PurpleBOSHConnection *conn, PurpleBOSHPacketType, xmlnode *node);

static void jabber_bosh_http_connection_connect(PurpleHTTPConnection *conn);
static void jabber_bosh_connection_connected(PurpleHTTPConnection *conn);
static void jabber_bosh_http_connection_disconnected(PurpleHTTPConnection *conn);
static void jabber_bosh_http_connection_send_request(PurpleHTTPConnection *conn, const GString *req);

void jabber_bosh_init(void)
{
	GHashTable *ui_info = purple_core_get_ui_info();
	const char *ui_name = NULL;
	const char *ui_version = NULL;

	if (ui_info) {
		ui_name = g_hash_table_lookup(ui_info, "name");
		ui_version = g_hash_table_lookup(ui_info, "version");
	}

	if (ui_name)
		bosh_useragent = g_strdup_printf("%s%s%s (libpurple " VERSION ")",
		                                 ui_name, ui_version ? " " : "",
		                                 ui_version ? ui_version : "");
	else
		bosh_useragent = g_strdup("libpurple " VERSION);
}

void jabber_bosh_uninit(void)
{
	g_free(bosh_useragent);
	bosh_useragent = NULL;
}

static PurpleHTTPConnection*
jabber_bosh_http_connection_init(PurpleBOSHConnection *bosh)
{
	PurpleHTTPConnection *conn = g_new0(PurpleHTTPConnection, 1);
	conn->bosh = bosh;
	conn->host = g_strdup(bosh->host);
	conn->port = bosh->port;
	conn->fd = -1;
	conn->ready = FALSE;

	return conn;
}

static void
jabber_bosh_http_connection_destroy(PurpleHTTPConnection *conn)
{
	g_free(conn->host);

	if (conn->buf)
		g_string_free(conn->buf, TRUE);

	if (conn->ie_handle)
		purple_input_remove(conn->ie_handle);
	if (conn->fd >= 0)
		close(conn->fd);

	g_free(conn);
}

PurpleBOSHConnection*
jabber_bosh_connection_init(JabberStream *js, const char *url)
{
	PurpleBOSHConnection *conn;
	char *host, *path, *user, *passwd;
	int port;

	if (!purple_url_parse(url, &host, &port, &path, &user, &passwd)) {
		purple_debug_info("jabber", "Unable to parse given URL.\n");
		return NULL;
	}

	conn = g_new0(PurpleBOSHConnection, 1);
	conn->host = host;
	conn->port = port;
	conn->path = g_strdup_printf("/%s", path);
	g_free(path);
	conn->pipelining = TRUE;

	if ((user && user[0] != '\0') || (passwd && passwd[0] != '\0')) {
		purple_debug_info("jabber", "Ignoring unexpected username and password "
		                            "in BOSH URL.\n");
	}

	g_free(user);
	g_free(passwd);

	conn->js = js;
	/* FIXME: This doesn't seem very random */
	conn->rid = rand() % 100000 + 1728679472;

	conn->pending = g_string_new("");

	conn->ready = FALSE;

	conn->connections[0] = jabber_bosh_http_connection_init(conn);

	return conn;
}

void
jabber_bosh_connection_destroy(PurpleBOSHConnection *conn)
{
	int i;

	g_free(conn->host);
	g_free(conn->path);

	if (conn->pending)
		g_string_free(conn->pending, TRUE);

	for (i = 0; i < MAX_HTTP_CONNECTIONS; ++i) {
		if (conn->connections[i])
			jabber_bosh_http_connection_destroy(conn->connections[i]);
	}

	g_free(conn);
}

void jabber_bosh_connection_close(PurpleBOSHConnection *conn)
{
	jabber_bosh_connection_send_native(conn, PACKET_TERMINATE, NULL);
}

static void jabber_bosh_connection_stream_restart(PurpleBOSHConnection *conn) {
	jabber_bosh_connection_send_native(conn, PACKET_STREAM_RESTART, NULL);
}

static gboolean jabber_bosh_connection_error_check(PurpleBOSHConnection *conn, xmlnode *node) {
	const char *type;

	type = xmlnode_get_attrib(node, "type");
	
	if (type != NULL && !strcmp(type, "terminate")) {
		conn->ready = FALSE;
		purple_connection_error_reason (conn->js->gc,
			PURPLE_CONNECTION_ERROR_OTHER_ERROR,
			_("The BOSH connection manager terminated your session."));
		return TRUE;
	}
	return FALSE;
}

static void jabber_bosh_connection_received(PurpleBOSHConnection *conn, xmlnode *node) {
	xmlnode *child;
	JabberStream *js = conn->js;

	g_return_if_fail(node != NULL);
	if (jabber_bosh_connection_error_check(conn, node))
		return;

	child = node->child;
	while (child != NULL) {
		/* jabber_process_packet might free child */
		xmlnode *next = child->next;
		if (child->type == XMLNODE_TYPE_TAG) {
			if (!strcmp(child->name, "iq")) {
				if (xmlnode_get_child(child, "session"))
					conn->ready = TRUE;
			}

			jabber_process_packet(js, &child);
		}

		child = next;
	}
}

static void auth_response_cb(PurpleBOSHConnection *conn, xmlnode *node) {
	xmlnode *child;

	g_return_if_fail(node != NULL);
	if (jabber_bosh_connection_error_check(conn, node))
		return;

	child = node->child;
	while(child != NULL && child->type != XMLNODE_TYPE_TAG) {
		child = child->next;
	}

	/* We're only expecting one XML node here, so only process the first one */
	if (child != NULL && child->type == XMLNODE_TYPE_TAG) {
		JabberStream *js = conn->js;
		if (!strcmp(child->name, "success")) {
			jabber_bosh_connection_stream_restart(conn);
			jabber_process_packet(js, &child);
			conn->receive_cb = jabber_bosh_connection_received;
		} else {
			js->state = JABBER_STREAM_AUTHENTICATING;
			jabber_process_packet(js, &child);
		}
	} else {
		purple_debug_warning("jabber", "Received unexepcted empty BOSH packet.\n");	
	}
}

static void boot_response_cb(PurpleBOSHConnection *conn, xmlnode *node) {
	const char *sid, *version;
	const char *inactivity, *requests;
	xmlnode *packet;

	g_return_if_fail(node != NULL);
	if (jabber_bosh_connection_error_check(conn, node))
		return;

	sid = xmlnode_get_attrib(node, "sid");
	version = xmlnode_get_attrib(node, "ver");

	inactivity = xmlnode_get_attrib(node, "inactivity");
	requests = xmlnode_get_attrib(node, "requests");

	if (sid) {
		conn->sid = g_strdup(sid);
	} else {
		purple_connection_error_reason(conn->js->gc,
		        PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
		        _("No session ID given"));
		return;
	}

	if (version) {
		const char *dot = strstr(version, ".");
		int major = atoi(version);
		int minor = atoi(dot + 1);

		purple_debug_info("jabber", "BOSH connection manager version %s\n", version);

		if (major != 1 || minor < 6) {
			purple_connection_error_reason(conn->js->gc,
			        PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			        _("Unsupported version of BOSH protocol"));
			return;
		}
	} else {
		purple_debug_info("jabber", "Missing version in BOSH initiation\n");
	}

	if (inactivity)
		conn->max_inactivity = atoi(inactivity);

	if (requests)
		conn->max_requests = atoi(requests);

	/* FIXME: Depending on receiving features might break with some hosts */
	packet = xmlnode_get_child(node, "features");
	conn->js->use_bosh = TRUE;
	conn->receive_cb = auth_response_cb;
	jabber_stream_features_parse(conn->js, packet);		
}

static PurpleHTTPConnection *
find_available_http_connection(PurpleBOSHConnection *conn)
{
	int i;

	/* Easy solution: Does everyone involved support pipelining? Hooray! Just use
	 * one TCP connection! */
	if (conn->pipelining)
		return conn->connections[0];

	/* First loop, look for a connection that's ready */
	for (i = 0; i < MAX_HTTP_CONNECTIONS; ++i) {
		if (conn->connections[i] && conn->connections[i]->ready &&
		                            conn->connections[i]->requests == 0)
			return conn->connections[i];
	}

	/* Second loop, look for one that's NULL and create a new connection */
	for (i = 0; i < MAX_HTTP_CONNECTIONS; ++i) {
		if (!conn->connections[i]) {
			conn->connections[i] = jabber_bosh_http_connection_init(conn);

			conn->connections[i]->connect_cb = jabber_bosh_connection_connected;
			conn->connections[i]->disconnect_cb = jabber_bosh_http_connection_disconnected;
			jabber_bosh_http_connection_connect(conn->connections[i]);
			return NULL;
		}
	}

	/* None available. */
	return NULL;
}

static void jabber_bosh_connection_boot(PurpleBOSHConnection *conn) {
	GString *buf = g_string_new("");

	g_string_printf(buf, "<body content='text/xml; charset=utf-8' "
	                "secure='true' "
	                "to='%s' "
	                "xml:lang='en' "
	                "xmpp:version='1.0' "
	                "ver='1.6' "
	                "xmlns:xmpp='urn:xmpp:bosh' "
	                "rid='%" G_GUINT64_FORMAT "' "
/* TODO: This should be adjusted/adjustable automatically according to
 * realtime network behavior */
	                "wait='60' "
	                "hold='1' "
	                "xmlns='http://jabber.org/protocol/httpbind'/>",
	                conn->js->user->domain,
	                ++conn->rid);

	conn->receive_cb = boot_response_cb;
	jabber_bosh_http_connection_send_request(conn->connections[0], buf);
	g_string_free(buf, TRUE);
}

static void
http_received_cb(const char *data, int len, PurpleBOSHConnection *conn)
{
	if (conn->receive_cb) {
		xmlnode *node = xmlnode_from_str(data, len);
		if (node) {
			char *txt = xmlnode_to_formatted_str(node, NULL);
			printf("\nhttp_received_cb\n%s\n", txt);
			g_free(txt);
			conn->receive_cb(conn, node);
			xmlnode_free(node);
		} else {
			purple_debug_warning("jabber", "BOSH: Received invalid XML\n");
		}
	} else {
		g_return_if_reached();
	}
}

void jabber_bosh_connection_send(PurpleBOSHConnection *conn, xmlnode *node) {
	jabber_bosh_connection_send_native(conn, PACKET_NORMAL, node);
}

void jabber_bosh_connection_send_raw(PurpleBOSHConnection *conn,
        const char *data, int len)
{
	xmlnode *node = xmlnode_from_str(data, len);
	if (node) {
		jabber_bosh_connection_send_native(conn, PACKET_NORMAL, node);
		xmlnode_free(node);
	} else {
		/*
		 * This best emulates what a normal XMPP server would do
		 * if you send bad XML.
		 */
		purple_connection_error_reason(conn->js->gc,
		        PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
		        _("Cannot send malformed XML"));
	}
}

static void
jabber_bosh_connection_send_native(PurpleBOSHConnection *conn, PurpleBOSHPacketType type,
                                   xmlnode *node)
{
	PurpleHTTPConnection *chosen;
	GString *packet = NULL;
	char *buf = NULL;

	chosen = find_available_http_connection(conn);

	if (type != PACKET_NORMAL && !chosen) {
		/*
		 * For non-ordinary traffic, we don't want to 'buffer' it, so use the first
		 * connection.
		 */
		chosen = conn->connections[0];
	
		if (!chosen->ready)
			purple_debug_warning("jabber", "First BOSH connection wasn't ready. Bad "
					"things may happen.\n");
	}

	if (node)
		buf = xmlnode_to_str(node, NULL);

	if (type == PACKET_NORMAL && (!chosen ||
	        (conn->max_requests > 0 && conn->requests == conn->max_requests))) {
		/*
		 * For normal data, send up to max_requests requests at a time or there is no
		 * connection ready (likely, we're currently opening a second connection and
		 * will send these packets when connected).
		 */
		if (buf) {
			conn->pending = g_string_append(conn->pending, buf);
			g_free(buf);
		}
		return;
	}

	packet = g_string_new("");

	g_string_printf(packet, "<body "
	                "rid='%" G_GUINT64_FORMAT "' "
	                "sid='%s' "
	                "to='%s' "
	                "xml:lang='en' "
	                "xmlns='http://jabber.org/protocol/httpbind' "
	                "xmlns:xmpp='urn:xmpp:xbosh'",
	                ++conn->rid,
	                conn->sid,
	                conn->js->user->domain);

	if (type == PACKET_STREAM_RESTART)
		packet = g_string_append(packet, " xmpp:restart='true'/>");
	else {
		if (type == PACKET_TERMINATE)
			packet = g_string_append(packet, " type='terminate'");

		g_string_append_printf(packet, ">%s%s</body>", conn->pending->str,
		                       buf ? buf : "");
		g_string_truncate(conn->pending, 0);
	}

	g_free(buf);

	jabber_bosh_http_connection_send_request(chosen, packet);
}

static void jabber_bosh_connection_connected(PurpleHTTPConnection *conn) {
	conn->ready = TRUE;

	if (conn->bosh->ready) {
		purple_debug_info("jabber", "BOSH session already exists. Trying to reuse it.\n");
		if (conn->bosh->pending && conn->bosh->pending->len > 0) {
			/* Send the pending data */
			jabber_bosh_connection_send_native(conn->bosh, PACKET_NORMAL, NULL);
		}
#if 0
		conn->bosh->receive_cb = jabber_bosh_connection_received;
		if (conn->bosh->connect_cb)
			conn->bosh->connect_cb(conn->bosh);
#endif
	} else
		jabber_bosh_connection_boot(conn->bosh);
}

void jabber_bosh_connection_refresh(PurpleBOSHConnection *conn)
{
	jabber_bosh_connection_send(conn, NULL);
}

static void jabber_bosh_http_connection_disconnected(PurpleHTTPConnection *conn) {
	/*
	 * Well, then. Fine! I never liked you anyway, server! I was cheating on you
	 * with AIM!
	 */
	conn->ready = FALSE;

	if (conn->bosh->pipelining)
		/* Hmmmm, fall back to multiple connections */
		conn->bosh->pipelining = FALSE;

	/* No! Please! Take me back. It was me, not you! I was weak! */
	conn->connect_cb = jabber_bosh_connection_connected;
	jabber_bosh_http_connection_connect(conn);
}

void jabber_bosh_connection_connect(PurpleBOSHConnection *bosh) {
	PurpleHTTPConnection *conn = bosh->connections[0];
	conn->connect_cb = jabber_bosh_connection_connected;
	conn->disconnect_cb = jabber_bosh_http_connection_disconnected;
	jabber_bosh_http_connection_connect(conn);
}

static void
jabber_bosh_http_connection_process(PurpleHTTPConnection *conn)
{
	const char *cursor;

	cursor = conn->buf->str + conn->handled_len;

	if (!conn->headers_done) {
		const char *content_length = purple_strcasestr(cursor, "\r\nContent-Length");
		const char *end_of_headers = purple_strcasestr(cursor, "\r\n\r\n");

		/* Make sure Content-Length is in headers, not body */
		if (content_length && content_length < end_of_headers) {
			char *sep = strstr(content_length, ": ");
			int len = atoi(sep + 2);
			if (len == 0) 
				purple_debug_warning("jabber", "Found mangled Content-Length header.\n");

			conn->body_len = len;
		}

		if (end_of_headers) {
			conn->headers_done = TRUE;
			conn->handled_len = end_of_headers - conn->buf->str + 4;
			cursor = end_of_headers + 4;
		} else {
			conn->handled_len = conn->buf->len;
			return;
		}
	}

	/* Have we handled everything in the buffer? */
	if (conn->handled_len >= conn->buf->len)
		return;

	/* Have we read all that the Content-Length promised us? */
	if (conn->buf->len - conn->handled_len < conn->body_len)
		return;

	--conn->requests;
	--conn->bosh->requests;

#warning For a pure HTTP 1.1 stack, this would need to be handled elsewhere.
	if (conn->bosh->ready && conn->bosh->requests == 0) {
		jabber_bosh_connection_send(conn->bosh, NULL);
		purple_debug_misc("jabber", "BOSH: Sending an empty request\n");
	}

	http_received_cb(conn->buf->str + conn->handled_len, conn->body_len,
	                 conn->bosh);

	g_string_free(conn->buf, TRUE);
	conn->buf = NULL;
	conn->headers_done = FALSE;
	conn->handled_len = conn->body_len = 0;
}

static void
jabber_bosh_http_connection_read(gpointer data, gint fd,
                                 PurpleInputCondition condition)
{
	PurpleHTTPConnection *conn = data;
	char buffer[1025];
	int perrno;
	int cnt, count = 0;

	purple_debug_info("jabber", "jabber_bosh_http_connection_read\n");

	if (!conn->buf)
		conn->buf = g_string_new("");

	while ((cnt = read(fd, buffer, sizeof(buffer))) > 0) {
		purple_debug_info("jabber", "bosh read %d bytes\n", cnt);
		count += cnt;
		g_string_append_len(conn->buf, buffer, cnt);
	}

	perrno = errno;
	if (cnt == 0 && count) {
		/* TODO: process should know this response ended with a closed socket
		 * and throw an error if it's not a complete response. */
		jabber_bosh_http_connection_process(conn);
	}

	if (cnt == 0 || (cnt < 0 && perrno != EAGAIN)) {
		if (cnt < 0)
			purple_debug_info("jabber", "bosh read: %d\n", cnt);
		else
			purple_debug_info("jabber", "bosh socket closed\n");
	
		purple_input_remove(conn->ie_handle);
		conn->ie_handle = 0;

		if (conn->disconnect_cb)
			conn->disconnect_cb(conn);

		return;
	}

	jabber_bosh_http_connection_process(conn);
}

static void jabber_bosh_http_connection_callback(gpointer data, gint source, const gchar *error)
{
	PurpleHTTPConnection *conn = data;
	PurpleConnection *gc = conn->bosh->js->gc;

	if (source < 0) {
		gchar *tmp;
		tmp = g_strdup_printf(_("Could not establish a connection with the server:\n%s"),
		        error);
		purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, tmp);
		g_free(tmp);
		return;
	}

	conn->fd = source;

	if (conn->connect_cb)
		conn->connect_cb(conn);

	conn->ie_handle = purple_input_add(conn->fd, PURPLE_INPUT_READ,
	        jabber_bosh_http_connection_read, conn);
}

static void jabber_bosh_http_connection_connect(PurpleHTTPConnection *conn)
{
	PurpleConnection *gc = conn->bosh->js->gc;
	PurpleAccount *account = purple_connection_get_account(gc);

	if ((purple_proxy_connect(conn, account, conn->host, conn->port, jabber_bosh_http_connection_callback, conn)) == NULL) {
		purple_connection_error_reason(gc,
		    PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
		    _("Unable to create socket"));
	}
}

static void
jabber_bosh_http_connection_send_request(PurpleHTTPConnection *conn,
                                         const GString *req)
{
	GString *packet = g_string_new("");
	int ret;

	g_string_printf(packet, "POST %s HTTP/1.1\r\n"
	                       "Host: %s\r\n"
	                       "User-Agent: %s\r\n"
	                       "Content-Encoding: text/xml; charset=utf-8\r\n"
	                       "Content-Length: %" G_GSIZE_FORMAT "\r\n\r\n",
	                       conn->bosh->path, conn->host, bosh_useragent,
	                       req->len);

	packet = g_string_append(packet, req->str);

	purple_debug_misc("jabber", "BOSH out: %s\n", packet->str);
	/* TODO: Better error handling, circbuffer or possible integration with
	 * low-level code in jabber.c */
	ret = write(conn->fd, packet->str, packet->len);

	++conn->requests;
	++conn->bosh->requests;
	g_string_free(packet, TRUE);

	if (ret < 0 && errno == EAGAIN)
		purple_debug_warning("jabber", "BOSH write would have blocked\n");

	if (ret <= 0) {
		purple_connection_error_reason(conn->bosh->js->gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Write error"));
		return;
	}
}

