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
#include "cipher.h"
#include "debug.h"
#include "imgstore.h"
#include "prpl.h"
#include "notify.h"
#include "request.h"
#include "util.h"
#include "xmlnode.h"

#include "buddy.h"
#include "chat.h"
#include "jabber.h"
#include "iq.h"
#include "presence.h"
#include "xdata.h"
#include "pep.h"
#include "adhoccommands.h"
#include "connection.h"

void jabber_bosh_connection_init(PurpleBOSHConnection *conn, PurpleAccount *account, JabberStream *js, char *url) {
	conn->pipelining = TRUE;
	conn->account = account;
	if (!purple_url_parse(url, &(conn->host), &(conn->port), &(conn->path), &(conn->user), &(conn->passwd))) {
		purple_debug_info("jabber", "Unable to parse given URL.\n");
		return;
	}
	if (conn->user || conn->passwd) {
		purple_debug_info("jabber", "Sorry, HTTP Authentication isn't supported yet. Username and password in the BOSH URL will be ignored.\n");
	}
	conn->js = js;
	conn->rid = rand() % 100000 + 1728679472;
	
	conn->conn_a = g_new0(PurpleHTTPConnection, 1);
	jabber_bosh_http_connection_init(conn->conn_a, conn->account, conn->host, conn->port);
	conn->conn_a->userdata = conn;
}

void jabber_bosh_connection_stream_restart(PurpleBOSHConnection *conn) {
	/*
	<body rid='1573741824'
	      sid='SomeSID'
	      to='jabber.org'
	      xml:lang='en'
	      xmpp:restart='true'
	      xmlns='http://jabber.org/protocol/httpbind'
	      xmlns:xmpp='urn:xmpp:xbosh'/>
	*/
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
}

void jabber_bosh_connection_received(PurpleBOSHConnection *conn, xmlnode *node) {
	
	xmlnode_free(node);
}

void jabber_bosh_connection_auth_response(PurpleBOSHConnection *conn, xmlnode *node) {
	xmlnode *child = node->child;
	
	while(child != NULL && child->type != XMLNODE_TYPE_TAG) {
		child = child->next;	
	}
	
	if (child != NULL && child->type == XMLNODE_TYPE_TAG) {
		JabberStream *js = conn->js;
		if (!strcmp(child->name, "success")) {
			jabber_bosh_connection_stream_restart(conn);
			jabber_process_packet(js, &child);
		} else {
			js->state = JABBER_STREAM_AUTHENTICATING;
			conn->receive_cb = jabber_bosh_connection_received;
			jabber_process_packet(js, &child);
		}
	} else printf("\n!! no child!!\n");
}

void jabber_bosh_connection_boot_response(PurpleBOSHConnection *conn, xmlnode *node) {
	char *version;
	if (xmlnode_get_attrib(node, "sid")) {
		conn->sid = g_strdup(xmlnode_get_attrib(node, "sid"));
	} else {
		purple_debug_info("jabber", "Connection manager doesn't behave BOSH-like!\n");
	}
	
	if (version = xmlnode_get_attrib(node, "ver")) {
		version[1] = 0;
		if (!(atoi(version) >= 1 && atoi(&version[2]) >= 6)) purple_debug_info("jabber", 	"Unsupported version of BOSH protocol. The connection manager must at least support version 1.6!\n");
		else {
			xmlnode *packet = xmlnode_get_child(node, "features");
			conn->js->use_bosh = TRUE;
			conn->receive_cb = jabber_bosh_connection_auth_response;
			jabber_stream_features_parse(conn->js, packet);
		}
		version[1] = '.';
	} else {
		purple_debug_info("jabber", "Missing version in session creation response!\n");	
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
}

void jabber_bosh_connection_http_received_cb(PurpleHTTPRequest *req, PurpleHTTPResponse *res, void *userdata) {
	PurpleBOSHConnection *conn = userdata;
	if (conn->receive_cb) {
		xmlnode *node = xmlnode_from_str(res->data, res->data_len);
		char *txt = xmlnode_to_formatted_str(node, NULL);
		printf("\njabber_bosh_connection_http_received_cb\n%s\n", txt);
		g_free(txt);
		conn->receive_cb(conn, node);
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
	
	xmlnode_insert_child(packet, node);
	
	jabber_bosh_connection_send_native(conn, packet);
}

void jabber_bosh_connection_send_native(PurpleBOSHConnection *conn, xmlnode *node) {
	PurpleHTTPRequest *request = g_new0(PurpleHTTPRequest, 1);
	
	char *txt = xmlnode_to_formatted_str(node, NULL);
	printf("\njabber_bosh_connection_send\n%s\n", txt);
	g_free(txt);
	
	jabber_bosh_http_request_init(request, "POST", g_strdup_printf("/%s", conn->path), jabber_bosh_connection_http_received_cb, conn);
	jabber_bosh_http_request_add_to_header(request, "Content-Encoding", "text/xml; charset=utf-8");
	request->data = xmlnode_to_str(node, &(request->data_len));
	jabber_bosh_http_request_add_to_header(request, "Content-Length", g_strdup_printf("%d", (int)strlen(request->data)));
	jabber_bosh_http_connection_send_request(conn->conn_a, request);
}

static void jabber_bosh_connection_connected(PurpleHTTPConnection *conn) {
	PurpleBOSHConnection *bosh_conn = conn->userdata;
	bosh_conn->receive_cb = jabber_bosh_connection_received;
	if (bosh_conn->ready && bosh_conn->connect_cb) bosh_conn->connect_cb(bosh_conn);
	else jabber_bosh_connection_boot(bosh_conn);
}

void jabber_bosh_connection_connect(PurpleBOSHConnection *conn) {
	conn->conn_a->connect_cb = jabber_bosh_connection_connected;
	jabber_bosh_http_connection_connect(conn->conn_a);
}

static void jabber_bosh_http_connection_receive(gpointer data, gint source, PurpleInputCondition condition) {
	char buffer[1024];
	int len;
	PurpleHTTPConnection *conn = data;
	PurpleHTTPResponse *response = conn->current_response;
	
	purple_debug_info("jabber", "jabber_bosh_http_connection_receive\n");
	if (!response) {
		// new response
		response = conn->current_response = g_new0(PurpleHTTPResponse, 1);
		jabber_bosh_http_response_init(response);
	}
	
	len = read(source, buffer, 1024);
	if (len > 0) {
		char *found = NULL;
		if (found = g_strstr_len(buffer, len, "\r\n\r\n")) {
			char *beginning = buffer;
			char *field = NULL;
			char *value = NULL;
			char *cl = NULL;
			
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
					g_hash_table_replace(response->header, g_strdup(field), g_strdup(value));
					value = field = 0;
					++beginning;
				}
			}
			++found; ++found; ++found; ++found;
			
			cl = g_hash_table_lookup(response->header, "Content-Length");
			if (cl) {
				PurpleHTTPRequest *request = NULL;
				response->data_len = atoi(cl);
				if (response->data <= len - (found - buffer)) response->data = g_memdup(found, response->data_len);
				else printf("\nDidn't receive complete content");
				
				request = g_queue_pop_head(conn->requests);
				if (request) {
					request->cb(request, response, conn->userdata);
					jabber_bosh_http_request_clean(request);
					jabber_bosh_http_response_clean(response);
					conn->current_response = NULL;
				} else {
					purple_debug_info("jabber", "received HTTP response but haven't requested anything yet.\n");
				}
			} else {
				printf("\ndidn't receive length.\n");
			}
		} else {
			printf("\nDid not receive complete HTTP header!\n");
		}
	} else {
		purple_debug_info("jabber", "jabber_bosh_http_connection_receive: problem receiving data\n");
	}
}

void jabber_bosh_http_connection_init(PurpleHTTPConnection *conn, PurpleAccount *account, char *host, int port) {
	conn->account = account;
	conn->host = host;
	conn->port = port;
	conn->connect_cb = NULL;
	conn->current_response = NULL;
	conn->current_data = NULL;
	conn->requests = g_queue_new();
}

void jabber_bosh_http_connection_clean(PurpleHTTPConnection *conn) {
	g_queue_free(conn->requests);
}

static void jabber_bosh_http_connection_callback(gpointer data, gint source, const gchar *error) {
	PurpleHTTPConnection *conn = data;
	if (source < 0) {
		purple_debug_info("jabber", "Couldn't connect becasue of: %s\n", error);
		return;
	}
	conn->fd = source;
	conn->ie_handle = purple_input_add(conn->fd, PURPLE_INPUT_READ, jabber_bosh_http_connection_receive, conn);
	if (conn->connect_cb) conn->connect_cb(conn);
}

void jabber_bosh_http_connection_connect(PurpleHTTPConnection *conn) {
	if((purple_proxy_connect(&(conn->handle), conn->account, conn->host, conn->port, jabber_bosh_http_connection_callback, conn)) == NULL) {
		purple_debug_info("jabber", "Unable to connect to %s.\n", conn->host);
	} 
}

static void jabber_bosh_http_connection_send_request_add_field_to_string(gpointer key, gpointer value, gpointer user_data) {
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
	jabber_bosh_http_request_add_to_header(req, "User-Agent", "libpurple");
	packet = tmp = g_strdup_printf("%s %s HTTP/1.1\r\n", req->method, req->path);
	g_hash_table_foreach(req->header, jabber_bosh_http_connection_send_request_add_field_to_string, &packet);
	tmp = packet;
	packet = g_strdup_printf("%s\r\n%s", tmp, req->data);
	g_free(tmp);
	if (write(conn->fd, packet, strlen(packet)) == -1) purple_debug_info("jabber", "send error\n");
	g_queue_push_tail(conn->requests, req);
}

void jabber_bosh_http_request_init(PurpleHTTPRequest *req, const char *method, const char *path, PurpleHTTPRequestCallback cb, void *userdata) {
	req->method = g_strdup(method);
	req->path = g_strdup(path);
	req->cb = cb;
	req->userdata = userdata;
	req->header = g_hash_table_new(g_str_hash, g_str_equal);
}

void jabber_bosh_http_request_add_to_header(PurpleHTTPRequest *req, const char *field, const char *value) {
	g_hash_table_replace(req->header, field, value);
}

void jabber_bosh_http_request_set_data(PurpleHTTPRequest *req, char *data, int len) {
	req->data = data;
	req->data_len = len;
}

void jabber_bosh_http_request_clean(PurpleHTTPRequest *req) {
	g_hash_table_destroy(req->header);
	g_free(req->method);
	g_free(req->path);
}

void jabber_bosh_http_response_init(PurpleHTTPResponse *res) {
	res->status = 0;
	res->header = g_hash_table_new(g_str_hash, g_str_equal);
}


void jabber_bosh_http_response_clean(PurpleHTTPResponse *res) {
	g_hash_table_destroy(res->header);
	g_free(res->data);
}
