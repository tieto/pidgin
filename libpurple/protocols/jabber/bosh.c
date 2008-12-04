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
typedef void (*PurpleHTTPRequestCallback)(PurpleHTTPRequest *req, PurpleHTTPResponse *res, void *userdata);
typedef void (*PurpleBOSHConnectionConnectFunction)(PurpleBOSHConnection *conn);
typedef void (*PurpleBOSHConnectionReceiveFunction)(PurpleBOSHConnection *conn, xmlnode *node);

static char *bosh_useragent = NULL;

struct _PurpleBOSHConnection {
    /* decoded URL */
    char *host;
    int port;
    char *path; 
    
    int rid;
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
    GQueue *requests;
    
    PurpleHTTPResponse *current_response;
    char *current_data;
    int current_len;
    
    int pih; /* what? */
    PurpleHTTPConnectionConnectFunction connect_cb;
    PurpleHTTPConnectionConnectFunction disconnect_cb;
    void *userdata;
};

struct _PurpleHTTPRequest {
    PurpleHTTPRequestCallback cb;
    char *method;
    char *path;
    GHashTable *header;
    char *data;
    int data_len;
    void *userdata;
};

struct _PurpleHTTPResponse {
    int status;
    GHashTable *header;
    char *data;
    int data_len;
};

static void jabber_bosh_connection_stream_restart(PurpleBOSHConnection *conn);
static gboolean jabber_bosh_connection_error_check(PurpleBOSHConnection *conn, xmlnode *node);
static void jabber_bosh_connection_received(PurpleBOSHConnection *conn, xmlnode *node);
static void jabber_bosh_connection_auth_response(PurpleBOSHConnection *conn, xmlnode *node);
static void jabber_bosh_connection_boot_response(PurpleBOSHConnection *conn, xmlnode *node);
static void jabber_bosh_connection_http_received_cb(PurpleHTTPRequest *req, PurpleHTTPResponse *res, void *userdata);
static void jabber_bosh_connection_send_native(PurpleBOSHConnection *conn, xmlnode *node);

static void jabber_bosh_http_connection_receive_parse_header(PurpleHTTPResponse *response, char **data, int *len);
static PurpleHTTPConnection* jabber_bosh_http_connection_init(const char *host, int port);
static void jabber_bosh_http_connection_connect(PurpleHTTPConnection *conn);
static void jabber_bosh_http_connection_send_request(PurpleHTTPConnection *conn, PurpleHTTPRequest *req);
static void jabber_bosh_http_connection_destroy(PurpleHTTPConnection *conn);

static PurpleHTTPRequest* jabber_bosh_http_request_init(const char *method, const char *path, PurpleHTTPRequestCallback cb, void *userdata);
static void jabber_bosh_http_request_add_to_header(PurpleHTTPRequest *req, const char *field, const char *value);
static void jabber_bosh_http_request_set_data(PurpleHTTPRequest *req, char *data, int len);
static void jabber_bosh_http_request_destroy(PurpleHTTPRequest *req);

static PurpleHTTPResponse* jabber_bosh_http_response_init(void);
static void jabber_bosh_http_response_destroy(PurpleHTTPResponse *res);

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

PurpleBOSHConnection* jabber_bosh_connection_init(JabberStream *js, const char *url)
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
	conn->path = path;
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

void jabber_bosh_connection_destroy(PurpleBOSHConnection *conn)
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
	char *tmp;

	xmlnode_set_attrib(packet, "type", "terminate");
	xmlnode_set_attrib(packet, "xmlns", "http://jabber.org/protocol/httpbind");
	xmlnode_set_attrib(packet, "sid", conn->sid);
	tmp = g_strdup_printf("%d", ++conn->rid);
	xmlnode_set_attrib(packet, "rid", tmp);
	g_free(tmp);

	jabber_bosh_connection_send_native(conn, packet);
	xmlnode_free(packet);
}

static void jabber_bosh_connection_stream_restart(PurpleBOSHConnection *conn) {
	xmlnode *restart = xmlnode_new("body");
	char *tmp = NULL;
	conn->rid++;
	xmlnode_set_attrib(restart, "rid", tmp = g_strdup_printf("%d", conn->rid));
	g_free(tmp);
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
	
	if (!node) return FALSE;
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
	
	if (node == NULL) return;
	
	if (jabber_bosh_connection_error_check(conn, node) == TRUE) return;
	
	child = node->child;
	while (child != 0) {
		if (child->type == XMLNODE_TYPE_TAG) {
			xmlnode *session = NULL;
			if (!strcmp(child->name, "iq")) session = xmlnode_get_child(child, "session");
			if (session) {
				conn->ready = TRUE;
			}
			jabber_process_packet(js, &child);
		}
		child = child->next;
	}
}

static void jabber_bosh_connection_auth_response(PurpleBOSHConnection *conn, xmlnode *node) {
	xmlnode *child = node->child;
	
	if (jabber_bosh_connection_error_check(conn, node) == TRUE) return;
	
	while(child != NULL && child->type != XMLNODE_TYPE_TAG) {
		child = child->next;	
	}
	
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
	} else printf("\n!! no child!!\n");
}

static void jabber_bosh_connection_boot_response(PurpleBOSHConnection *conn, xmlnode *node) {
	char *version;
	
	if (jabber_bosh_connection_error_check(conn, node) == TRUE) return;
	
	if (xmlnode_get_attrib(node, "sid")) {
		conn->sid = g_strdup(xmlnode_get_attrib(node, "sid"));
	} else {
		purple_debug_info("jabber", "Connection manager doesn't behave BOSH-like!\n");
	}

	if ((version = g_strdup(xmlnode_get_attrib(node, "ver")))) {
		char *dot = strstr(version, ".");
		int major = atoi(version);
		int minor = atoi(dot + 1);

		purple_debug_info("jabber", "BOSH connection manager version %s\n", version);

		if (major > 1 || (major == 1 && minor >= 6)) {
			xmlnode *packet = xmlnode_get_child(node, "features");
			conn->js->use_bosh = TRUE;
			conn->receive_cb = jabber_bosh_connection_auth_response;
			jabber_stream_features_parse(conn->js, packet);		
		} else {
			purple_connection_error_reason(conn->js->gc,
			                  PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			                  _("Unsupported version of BOSH protocol"));
		}

		g_free(version);
	} else {
		purple_debug_info("jabber", "Missing version in BOSH initiation\n");
	}
}

static void jabber_bosh_connection_boot(PurpleBOSHConnection *conn) {
	char *tmp;
	xmlnode *init = xmlnode_new("body");
	xmlnode_set_attrib(init, "content", "text/xml; charset=utf-8");
	xmlnode_set_attrib(init, "secure", "true");
	//xmlnode_set_attrib(init, "route", tmp = g_strdup_printf("xmpp:%s:5222", conn->js->user->domain));
	//g_free(tmp);
	xmlnode_set_attrib(init, "to", conn->js->user->domain);
	xmlnode_set_attrib(init, "xml:lang", "en");
	xmlnode_set_attrib(init, "xmpp:version", "1.0");
	xmlnode_set_attrib(init, "ver", "1.6");
	xmlnode_set_attrib(init, "xmlns:xmpp", "urn:xmpp:xbosh"); 
	xmlnode_set_attrib(init, "rid", tmp = g_strdup_printf("%d", conn->rid));
	g_free(tmp);
	xmlnode_set_attrib(init, "wait", "60"); /* this should be adjusted automatically according to real time network behavior */
	xmlnode_set_attrib(init, "xmlns", "http://jabber.org/protocol/httpbind");
	xmlnode_set_attrib(init, "hold", "1");
	
	conn->receive_cb = jabber_bosh_connection_boot_response;
	jabber_bosh_connection_send_native(conn, init);
	xmlnode_free(init);
}

static void jabber_bosh_connection_http_received_cb(PurpleHTTPRequest *req, PurpleHTTPResponse *res, void *userdata) {
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
	char *tmp;
	conn->rid++;
	xmlnode_set_attrib(packet, "xmlns", "http://jabber.org/protocol/httpbind");
	xmlnode_set_attrib(packet, "sid", conn->sid);
	tmp = g_strdup_printf("%d", conn->rid);
	xmlnode_set_attrib(packet, "rid", tmp);
	g_free(tmp);
	
	if (node) {
		xmlnode_insert_child(packet, node);
		if (conn->ready == TRUE) xmlnode_set_attrib(node, "xmlns", "jabber:client");
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
	
	char *txt = xmlnode_to_formatted_str(node, NULL);
	printf("\njabber_bosh_connection_send\n%s\n", txt);
	g_free(txt);
	
	request = jabber_bosh_http_request_init("POST", g_strdup_printf("/%s", conn->path), jabber_bosh_connection_http_received_cb, conn);
	jabber_bosh_http_request_add_to_header(request, "Content-Encoding", "text/xml; charset=utf-8");
	request->data = xmlnode_to_str(node, &(request->data_len));
	jabber_bosh_http_request_add_to_header(request, "Content-Length", g_strdup_printf("%d", (int)strlen(request->data)));
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

static void jabber_bosh_connection_refresh(PurpleHTTPConnection *conn) {
	PurpleBOSHConnection *bosh_conn = conn->userdata;
	jabber_bosh_connection_send(bosh_conn, NULL);
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

static void jabber_bosh_http_connection_receive_parse_header(PurpleHTTPResponse *response, char **data, int *len) {
	GHashTable *header = response->header;
	char *beginning = *data;
	char *found = g_strstr_len(*data, *len, "\r\n\r\n");
	char *field = NULL;
	char *value = NULL;
	char *old_data = *data;
	
	while (*beginning != 'H') ++beginning;
	beginning[12] = 0;
	response->status = atoi(&beginning[9]);
	beginning = &beginning[13];
	do {
		++beginning;
	} while (*beginning != '\n');
	++beginning;
	/* parse HTTP response header */
	for (;beginning != found; ++beginning) {
		if (!field) field = beginning;
		if (*beginning == ':') {
			*beginning = 0;
			value = beginning + 1;
		} else if (*beginning == '\r') {
			*beginning = 0;
			g_hash_table_replace(header, g_ascii_strdown(field, -1), g_strdup(value));
			value = field = 0;
			++beginning;
		}
	}
	++beginning; ++beginning; ++beginning; ++beginning;
	/* remove the header from data buffer */
	*data = g_strdup(beginning);
	*len = *len - (beginning - old_data);
	g_free(old_data);	
}

static void jabber_bosh_http_connection_receive(gpointer data, gint source, PurpleInputCondition condition) {
	char buffer[1025];
	int len;
	PurpleHTTPConnection *conn = data;
	PurpleBOSHConnection *bosh_conn = conn->userdata;
	PurpleHTTPResponse *response = conn->current_response;
	
	purple_debug_info("jabber", "jabber_bosh_http_connection_receive\n");
	
	len = read(source, buffer, 1024);
	if (len > 0) {
		buffer[len] = 0;
		if (conn->current_data == NULL) {
			conn->current_len = len;	
			conn->current_data = g_strdup_printf("%s", buffer);
		} else {
			char *old_data = conn->current_data;
			conn->current_len += len;	
			conn->current_data = g_strdup_printf("%s%s", conn->current_data, buffer);
			g_free(old_data);
		}
		
		if (!response) {
			/* check for header footer */
			char *found = g_strstr_len(conn->current_data, conn->current_len, "\r\n\r\n");
			if (found) {
				/* New response */
				response = conn->current_response = jabber_bosh_http_response_init();
				jabber_bosh_http_connection_receive_parse_header(response, &(conn->current_data), &(conn->current_len));
				/* XXX: Crash if there is no Content-Length header */
				response->data_len = atoi(g_hash_table_lookup(response->header, "content-length"));
			} else {
				printf("\nDid not receive HTTP header\n");
			}
		} 
		
		if (response) {
			if (conn->current_len >= response->data_len) {
				PurpleHTTPRequest *request = g_queue_pop_head(conn->requests);
				
#warning For a pure HTTP 1.1 stack, this would need to be handled elsewhere.
				if (bosh_conn->ready == TRUE && g_queue_is_empty(conn->requests) == TRUE) {
					jabber_bosh_connection_send(bosh_conn, NULL); 
					printf("\n SEND AN EMPTY REQUEST \n");
				}
				
				if (request) {
					char *old_data = conn->current_data;
					response->data = g_memdup(conn->current_data, response->data_len);
					conn->current_data = g_strdup(&conn->current_data[response->data_len]);
					conn->current_len -= response->data_len;
					g_free(old_data);
					
					if (request->cb) request->cb(request, response, conn->userdata);
					else purple_debug_info("jabber", "missing request callback!\n");
					
					jabber_bosh_http_request_destroy(request);
					jabber_bosh_http_response_destroy(response);
					conn->current_response = NULL;
				} else {
					purple_debug_info("jabber", "received HTTP response but haven't requested anything yet.\n");
				}
			}
		}
	} else if (len == 0) {
		if (conn->ie_handle) {
			purple_input_remove(conn->ie_handle);
			conn->ie_handle = 0;
		}
		if (conn->disconnect_cb)
			conn->disconnect_cb(conn);
	} else {
		purple_debug_info("jabber", "jabber_bosh_http_connection_receive: problem receiving data (%d)\n", len);
	}
}

static PurpleHTTPConnection* jabber_bosh_http_connection_init(const char *host, int port)
{
	PurpleHTTPConnection *conn = g_new0(PurpleHTTPConnection, 1);
	conn->host = g_strdup(host);
	conn->port = port;
	conn->fd = -1;
	conn->requests = g_queue_new();

	return conn;
}

static void jabber_bosh_http_connection_destroy(PurpleHTTPConnection *conn)
{
	g_free(conn->current_data);
	g_free(conn->host);

	if (conn->requests) {
		g_queue_foreach(conn->requests, (GFunc)jabber_bosh_http_request_destroy, NULL);
		g_queue_free(conn->requests);
	}

	if (conn->current_response)
		jabber_bosh_http_response_destroy(conn->current_response);

	if (conn->ie_handle)
			purple_input_remove(conn->ie_handle);
	if (conn->fd > 0)
			close(conn->fd);

	g_free(conn);
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
	        jabber_bosh_http_connection_receive, conn);
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

static void jabber_bosh_http_connection_send_request_add_field_to_string(gpointer key, gpointer value, gpointer user_data)
{
	char **ppacket = user_data;
	char *tmp = *ppacket;
	char *field = key;
	char *val = value;
	*ppacket = g_strdup_printf("%s%s: %s\r\n", tmp, field, val);
	g_free(tmp);
}

void jabber_bosh_http_connection_send_request(PurpleHTTPConnection *conn, PurpleHTTPRequest *req) {
	char *packet;
	char *tmp;
	jabber_bosh_http_request_add_to_header(req, "Host", conn->host);
	jabber_bosh_http_request_add_to_header(req, "User-Agent", bosh_useragent);

	packet = tmp = g_strdup_printf("%s %s HTTP/1.1\r\n", req->method, req->path);
	g_hash_table_foreach(req->header, jabber_bosh_http_connection_send_request_add_field_to_string, &packet);
	tmp = packet;
	packet = g_strdup_printf("%s\r\n%s", tmp, req->data);
	g_free(tmp);
	if (write(conn->fd, packet, strlen(packet)) == -1) purple_debug_info("jabber", "send error\n");
	g_queue_push_tail(conn->requests, req);
}

static PurpleHTTPRequest* jabber_bosh_http_request_init(const char *method,
        const char *path, PurpleHTTPRequestCallback cb, void *userdata)
{
	PurpleHTTPRequest *req = g_new(PurpleHTTPRequest, 1);
	req->method = g_strdup(method);
	req->path = g_strdup(path);
	req->cb = cb;
	req->userdata = userdata;
	req->header = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	return req;
}

static void jabber_bosh_http_request_add_to_header(PurpleHTTPRequest *req, const char *field, const char *value) {
	char *f = g_strdup(field);
	char *v = g_strdup(value);
	g_hash_table_replace(req->header, f, v);
}

void jabber_bosh_http_request_set_data(PurpleHTTPRequest *req, char *data, int len) {
	req->data = data;
	req->data_len = len;
}

static void jabber_bosh_http_request_destroy(PurpleHTTPRequest *req)
{
	g_hash_table_destroy(req->header);
	g_free(req->method);
	g_free(req->path);
	g_free(req->data);
	g_free(req);
}

static PurpleHTTPResponse* jabber_bosh_http_response_init(void)
{
	PurpleHTTPResponse *res = g_new0(PurpleHTTPResponse, 1);
	res->header = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	return res;
}

static void jabber_bosh_http_response_destroy(PurpleHTTPResponse *res)
{
	g_hash_table_destroy(res->header);
	g_free(res->data);
	g_free(res);
}
