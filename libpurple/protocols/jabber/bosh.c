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

void jabber_bosh_connection_init(PurpleBOSHConnection *conn, PurpleAccount *account, char *url) {
	conn->pipelining = TRUE;
	conn->account = account;
	if (!purple_url_parse(url, &(conn->host), &(conn->port), &(conn->path), &(conn->user), &(conn->passwd))) {
		purple_debug_info("jabber", "Unable to parse given URL.\n");
		return;
	}
	if (conn->user || conn->passwd) {
		purple_debug_info("jabber", "Sorry, HTTP Authentication isn't supported yet. Username and password in the BOSH URL will be ignored.\n");
	}
	conn->conn_a = g_new0(PurpleHTTPConnection, 1);
	jabber_bosh_http_connection_init(conn->conn_a, conn->account, conn->host, conn->port);
	conn->conn_a->userdata = conn;
}

static void jabber_bosh_connection_connected(PurpleHTTPConnection *conn) {
	PurpleBOSHConnection *bosh_conn = conn->userdata;
	if (bosh_conn->connect_cb) bosh_conn->connect_cb(bosh_conn);
}

void jabber_bosh_connection_connect(PurpleBOSHConnection *conn) {
	conn->conn_a->connect_cb = jabber_bosh_connection_connected;
	jabber_bosh_http_connection_connect(conn->conn_a);	
}


void jabber_bosh_http_connection_init(PurpleHTTPConnection *conn, PurpleAccount *account, char *host, int port) {
	conn->account = account;
	conn->host = host;
	conn->port = port;
	conn->connect_cb = NULL;
}

static void jabber_bosh_http_connection_callback(gpointer data, gint source, const gchar *error) {
	PurpleHTTPConnection *conn = data;
	if (source < 0) {
		purple_debug_info("jabber", "Couldn't connect becasue of: %s\n", error);
		return;
	}
	conn->fd = source;
	if (conn->connect_cb) conn->connect_cb(conn);
}

void jabber_bosh_http_connection_connect(PurpleHTTPConnection *conn) {
	if((purple_proxy_connect(&(conn->handle), conn->account, conn->host, conn->port, jabber_bosh_http_connection_callback, conn)) == NULL) {
		purple_debug_info("jabber", "Unable to connect to %s.\n", conn->host);
	}
}