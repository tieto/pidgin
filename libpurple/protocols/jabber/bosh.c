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

typedef struct _PurpleHTTPRequest PurpleHTTPRequest;
typedef struct _PurpleHTTPResponse PurpleHTTPResponse;
typedef struct _PurpleHTTPConnection PurpleHTTPConnection;

typedef void (*PurpleHTTPConnectionConnectFunction)(PurpleHTTPConnection *conn);
typedef void (*PurpleHTTPConnectionDisconnectFunction)(PurpleHTTPConnection *conn);
typedef void (*PurpleHTTPRequestCallback)(PurpleHTTPResponse *res, void *userdata);
typedef void (*PurpleBOSHConnectionConnectFunction)(PurpleBOSHConnection *conn);
typedef void (*PurpleBOSHConnectionReceiveFunction)(PurpleBOSHConnection *conn, xmlnode *node);

static char *bosh_useragent = NULL;

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
    PurpleHTTPConnection *conn_a;
    PurpleHTTPConnection *conn_b;

    gboolean ready;
    PurpleBOSHConnectionConnectFunction connect_cb;
    PurpleBOSHConnectionReceiveFunction receive_cb;
};

struct _PurpleHTTPConnection {
    int fd;
    char *host;
    int port;
    int ie_handle;
    GQueue *requests; /* Queue of PurpleHTTPRequestCallbacks */
    
    PurpleHTTPResponse *current_response;
    GString *buf;
    gboolean headers_done;
    gsize handled_len;
    gsize body_len;

    int pih; /* what? */
    PurpleHTTPConnectionConnectFunction connect_cb;
    PurpleHTTPConnectionConnectFunction disconnect_cb;
    void *userdata;
};

struct _PurpleHTTPRequest {
    PurpleHTTPRequestCallback cb;
    const char *path;
    char *data;
    int data_len;
    void *userdata;
};

struct _PurpleHTTPResponse {
    char *data;
    int data_len;
};

static void jabber_bosh_connection_stream_restart(PurpleBOSHConnection *conn);
static gboolean jabber_bosh_connection_error_check(PurpleBOSHConnection *conn, xmlnode *node);
static void jabber_bosh_connection_received(PurpleBOSHConnection *conn, xmlnode *node);
static void jabber_bosh_connection_http_received_cb(PurpleHTTPResponse *res, void *userdata);
static void jabber_bosh_connection_send_native(PurpleBOSHConnection *conn, xmlnode *node);

static void jabber_bosh_http_connection_connect(PurpleHTTPConnection *conn);
static void jabber_bosh_http_connection_send_request(PurpleHTTPConnection *conn, PurpleHTTPRequest *req);

void jabber_bosh_init(void)
{
	GHashTable *ui_info = purple_core_get_ui_info();
	const char *ui_version = NULL;

	if (ui_info)
		ui_version = g_hash_table_lookup(ui_info, "version");

	if (ui_version)
		bosh_useragent = g_strdup_printf("%s (libpurple " VERSION ")", ui_version);
	else
		bosh_useragent = g_strdup("libpurple " VERSION);
}

void jabber_bosh_uninit(void)
{
	g_free(bosh_useragent);
	bosh_useragent = NULL;
}

static void
jabber_bosh_http_request_destroy(PurpleHTTPRequest *req)
{
	g_free(req->data);
	g_free(req);
}

static void
jabber_bosh_http_response_destroy(PurpleHTTPResponse *res)
{
	g_free(res->data);
	g_free(res);
}

static PurpleHTTPConnection*
jabber_bosh_http_connection_init(const char *host, int port)
{
	PurpleHTTPConnection *conn = g_new0(PurpleHTTPConnection, 1);
	conn->host = g_strdup(host);
	conn->port = port;
	conn->fd = -1;
	conn->requests = g_queue_new();

	return conn;
}

static void
jabber_bosh_http_connection_destroy(PurpleHTTPConnection *conn)
{
	g_free(conn->host);

	if (conn->buf)
		g_string_free(conn->buf, TRUE);

	if (conn->requests)
		g_queue_free(conn->requests);

	if (conn->current_response)
		jabber_bosh_http_response_destroy(conn->current_response);

	if (conn->ie_handle)
		purple_input_remove(conn->ie_handle);
	if (conn->fd > 0)
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
	conn->ready = FALSE;
	conn->conn_a = jabber_bosh_http_connection_init(conn->host, conn->port);
	conn->conn_a->userdata = conn;

	return conn;
}

void
jabber_bosh_connection_destroy(PurpleBOSHConnection *conn)
{
	g_free(conn->host);
	g_free(conn->path);

	if (conn->conn_a)
		jabber_bosh_http_connection_destroy(conn->conn_a);
	if (conn->conn_b)
		jabber_bosh_http_connection_destroy(conn->conn_b);

	g_free(conn);
}

void jabber_bosh_connection_close(PurpleBOSHConnection *conn)
{
	xmlnode *packet = xmlnode_new("body");
	/* XEP-0124: The rid must not exceed 16 characters */
	char rid[17];
	g_snprintf(rid, sizeof(rid), "%" G_GUINT64_FORMAT, ++conn->rid);
	xmlnode_set_attrib(packet, "type", "terminate");
	xmlnode_set_attrib(packet, "xmlns", "http://jabber.org/protocol/httpbind");
	xmlnode_set_attrib(packet, "sid", conn->sid);
	xmlnode_set_attrib(packet, "rid", rid);

	jabber_bosh_connection_send_native(conn, packet);
	xmlnode_free(packet);
}

static void jabber_bosh_connection_stream_restart(PurpleBOSHConnection *conn) {
	xmlnode *restart = xmlnode_new("body");
	/* XEP-0124: The rid must not exceed 16 characters */
	char rid[17];
	g_snprintf(rid, sizeof(rid), "%" G_GUINT64_FORMAT, ++conn->rid);
	xmlnode_set_attrib(restart, "rid", rid);
	xmlnode_set_attrib(restart, "sid", conn->sid);
	xmlnode_set_attrib(restart, "to", conn->js->user->domain);
	xmlnode_set_attrib(restart, "xml:lang", "en");
	xmlnode_set_attrib(restart, "xmpp:restart", "true");
	xmlnode_set_attrib(restart, "xmlns", "http://jabber.org/protocol/httpbind");
	xmlnode_set_attrib(restart, "xmlns:xmpp", "urn:xmpp:xbosh"); 
	
	jabber_bosh_connection_send_native(conn, restart);
	xmlnode_free(restart);
}

static gboolean jabber_bosh_connection_error_check(PurpleBOSHConnection *conn, xmlnode *node) {
	const char *type;

	type = xmlnode_get_attrib(node, "type");
	
	if (type != NULL && !strcmp(type, "terminate")) {
		conn->ready = FALSE;
		purple_connection_error_reason (conn->js->gc,
			PURPLE_CONNECTION_ERROR_OTHER_ERROR,
			_("The BOSH conncetion manager suggested to terminate your session."));
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
	xmlnode *packet;

	g_return_if_fail(node != NULL);
	if (jabber_bosh_connection_error_check(conn, node))
		return;

	sid = xmlnode_get_attrib(node, "sid");
	version = xmlnode_get_attrib(node, "ver");

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

	/* FIXME: Depending on receiving features might break with some hosts */
	packet = xmlnode_get_child(node, "features");
	conn->js->use_bosh = TRUE;
	conn->receive_cb = auth_response_cb;
	jabber_stream_features_parse(conn->js, packet);		
}

static void jabber_bosh_connection_boot(PurpleBOSHConnection *conn) {
	xmlnode *init = xmlnode_new("body");
	/* XEP-0124: The rid must not exceed 16 characters */
	char rid[17];
	g_snprintf(rid, sizeof(rid), "%" G_GUINT64_FORMAT, ++conn->rid);
	xmlnode_set_attrib(init, "content", "text/xml; charset=utf-8");
	xmlnode_set_attrib(init, "secure", "true");
/*
	xmlnode_set_attrib(init, "route", tmp = g_strdup_printf("xmpp:%s:5222", conn->js->user->domain));
	g_free(tmp);
*/
	xmlnode_set_attrib(init, "to", conn->js->user->domain);
	xmlnode_set_attrib(init, "xml:lang", "en");
	xmlnode_set_attrib(init, "xmpp:version", "1.0");
	xmlnode_set_attrib(init, "ver", "1.6");
	xmlnode_set_attrib(init, "xmlns:xmpp", "urn:xmpp:xbosh"); 
	xmlnode_set_attrib(init, "rid", rid);
	xmlnode_set_attrib(init, "wait", "60"); /* this should be adjusted automatically according to real time network behavior */
	xmlnode_set_attrib(init, "xmlns", "http://jabber.org/protocol/httpbind");
	xmlnode_set_attrib(init, "hold", "1");
	
	conn->receive_cb = boot_response_cb;
	jabber_bosh_connection_send_native(conn, init);
	xmlnode_free(init);
}

static void jabber_bosh_connection_http_received_cb(PurpleHTTPResponse *res, void *userdata) {
	PurpleBOSHConnection *conn = userdata;
	if (conn->receive_cb) {
		xmlnode *node = xmlnode_from_str(res->data, res->data_len);
		if (node) {
			char *txt = xmlnode_to_formatted_str(node, NULL);
			printf("\njabber_bosh_connection_http_received_cb\n%s\n", txt);
			g_free(txt);
			conn->receive_cb(conn, node);
			xmlnode_free(node);
		} else {
			printf("\njabber_bosh_connection_http_received_cb: XML ERROR: %s\n", res->data); 
		}
	} else purple_debug_info("jabber", "missing receive_cb of PurpleBOSHConnection.\n");
}

void jabber_bosh_connection_send(PurpleBOSHConnection *conn, xmlnode *node) {
	xmlnode *packet = xmlnode_new("body");
	/* XEP-0124: The rid must not exceed 16 characters */
	char rid[17];
	g_snprintf(rid, sizeof(rid), "%" G_GUINT64_FORMAT, ++conn->rid);
	xmlnode_set_attrib(packet, "xmlns", "http://jabber.org/protocol/httpbind");
	xmlnode_set_attrib(packet, "sid", conn->sid);
	xmlnode_set_attrib(packet, "rid", rid);
	
	if (node) {
		xmlnode *copy = xmlnode_copy(node);
		xmlnode_insert_child(packet, copy);
		if (conn->ready == TRUE)
			xmlnode_set_attrib(copy, "xmlns", "jabber:client");
	}
	jabber_bosh_connection_send_native(conn, packet);
	xmlnode_free(packet);
}

void jabber_bosh_connection_send_raw(PurpleBOSHConnection *conn,
        const char *data, int len)
{
	xmlnode *node = xmlnode_from_str(data, len);
	if (node) {
		jabber_bosh_connection_send(conn, node);
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

static void jabber_bosh_connection_send_native(PurpleBOSHConnection *conn, xmlnode *node) {
	PurpleHTTPRequest *request;
	
	request = g_new0(PurpleHTTPRequest, 1);
	request->path     = conn->path;
	request->cb       = jabber_bosh_connection_http_received_cb;
	request->userdata = conn;

	request->data = xmlnode_to_str(node, &(request->data_len));

	jabber_bosh_http_connection_send_request(conn->conn_a, request);
}

static void jabber_bosh_connection_connected(PurpleHTTPConnection *conn) {
	PurpleBOSHConnection *bosh_conn = conn->userdata;
	if (bosh_conn->ready == TRUE && bosh_conn->connect_cb) {
		purple_debug_info("jabber", "BOSH session already exists. Trying to reuse it.\n");
		bosh_conn->receive_cb = jabber_bosh_connection_received;
		bosh_conn->connect_cb(bosh_conn);
	} else jabber_bosh_connection_boot(bosh_conn);
}

void jabber_bosh_connection_refresh(PurpleBOSHConnection *conn)
{
	jabber_bosh_connection_send(conn, NULL);
}

static void jabber_bosh_http_connection_disconnected(PurpleHTTPConnection *conn) {
	PurpleBOSHConnection *bosh_conn = conn->userdata;
	bosh_conn->conn_a->connect_cb = jabber_bosh_connection_connected;
	jabber_bosh_http_connection_connect(bosh_conn->conn_a);
}

void jabber_bosh_connection_connect(PurpleBOSHConnection *conn) {
	conn->conn_a->connect_cb = jabber_bosh_connection_connected;
	jabber_bosh_http_connection_connect(conn->conn_a);
}

static void
jabber_bosh_http_connection_process(PurpleHTTPConnection *conn)
{
	PurpleBOSHConnection *bosh_conn = conn->userdata;
	PurpleHTTPRequestCallback cb;
	const char *cursor;

	if (!conn->current_response)
		conn->current_response = g_new0(PurpleHTTPResponse, 1);

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

	cb = g_queue_pop_head(conn->requests);

#warning For a pure HTTP 1.1 stack, this would need to be handled elsewhere.
	if (bosh_conn->ready && g_queue_is_empty(conn->requests)) {
		jabber_bosh_connection_send(bosh_conn, NULL);
		printf("\n SEND AN EMPTY REQUEST \n");
	}

	if (cb) {
		conn->current_response->data_len = conn->body_len;
		conn->current_response->data = g_memdup(conn->buf->str + conn->handled_len, conn->body_len);

		cb(conn->current_response, conn->userdata);
	} else {
		purple_debug_warning("jabber", "Received HTTP response before POST\n");
	}

	g_string_free(conn->buf, TRUE);
	conn->buf = NULL;
	jabber_bosh_http_response_destroy(conn->current_response);
	conn->current_response = NULL;
	conn->headers_done = conn->handled_len = conn->body_len = 0;
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

	if (conn->buf == NULL)
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
	PurpleBOSHConnection *bosh_conn = conn->userdata;
	PurpleConnection *gc = bosh_conn->js->gc;

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
	PurpleBOSHConnection *bosh_conn = conn->userdata;
	PurpleConnection *gc = bosh_conn->js->gc;
	PurpleAccount *account = purple_connection_get_account(gc);

	if ((purple_proxy_connect(conn, account, conn->host, conn->port, jabber_bosh_http_connection_callback, conn)) == NULL) {
		purple_connection_error_reason(gc,
		    PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
		    _("Unable to create socket"));
	}
}

static void
jabber_bosh_http_connection_send_request(PurpleHTTPConnection *conn,
                                         PurpleHTTPRequest *req)
{
	PurpleBOSHConnection *bosh_conn = conn->userdata;
	GString *packet = g_string_new("");
	int ret;

	/* TODO: Should we lie about this HTTP/1.1 support? */
	g_string_append_printf(packet, "POST %s HTTP/1.1\r\n"
	                       "Host: %s\r\n"
	                       "User-Agent: %s\r\n"
	                       "Content-Encoding: text/xml; charset=utf-8\r\n"
	                       "Content-Length: %d\r\n\r\n",
	                       req->path, conn->host, bosh_useragent, req->data_len);

	packet = g_string_append(packet, req->data);

	printf("Sending %s\n", packet->str);
	/* TODO: Better error handling, circbuffer or possible integration with
	 * low-level code in jabber.c */
	ret = write(conn->fd, packet->str, packet->len);

	g_string_free(packet, TRUE);
	g_queue_push_tail(conn->requests, req->cb);
	jabber_bosh_http_request_destroy(req);

	if (ret < 0 && errno == EAGAIN)
		purple_debug_warning("jabber", "BOSH write would have blocked\n");

	if (ret <= 0) {
		purple_connection_error_reason(bosh_conn->js->gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Write error"));
		return;
	}
}

