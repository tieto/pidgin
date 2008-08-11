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
	xmlnode_set_attrib(init, "ack", "0");
	xmlnode_set_attrib(init, "xmlns", "http://jabber.org/protocol/httpbind");
	xmlnode_set_attrib(init, "hold", "1");
	
	jabber_bosh_connection_send(conn, init);
}

void jabber_bosh_connection_login_cb(PurpleHTTPRequest *req, PurpleHTTPResponse *res, void *userdata) {
	purple_debug_info("jabber", "RECEIVED FIRST HTTP RESPONSE\n");
	printf("\nDATA\n\n%s\n", res->data);
}

void jabber_bosh_connection_send(PurpleBOSHConnection *conn, xmlnode *node) {
	PurpleHTTPRequest *request = g_new0(PurpleHTTPRequest, 1);
	jabber_bosh_http_request_init(request, "POST", g_strdup_printf("/%s", conn->path), jabber_bosh_connection_login_cb, conn);
	jabber_bosh_http_request_add_to_header(request, "Content-Encoding", "text/xml; charset=utf-8");
	request->data = xmlnode_to_str(node, &(request->data_len));
	jabber_bosh_http_request_add_to_header(request, "Content-Length", g_strdup_printf("%d", (int)strlen(request->data)));
	jabber_bosh_http_connection_send_request(conn->conn_a, request);
}

static void jabber_bosh_connection_connected(PurpleHTTPConnection *conn) {
	PurpleBOSHConnection *bosh_conn = conn->userdata;
	
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
