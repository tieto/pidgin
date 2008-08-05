/**
 * @file bosh.h Buddy handlers
 *
 * purple
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
 */
#ifndef _PURPLE_JABBER_BOSH_H_
#define _PURPLE_JABBER_BOSH_H_

#include <glib.h>

typedef struct _PurpleHTTPRequest PurpleHTTPRequest;
typedef struct _PurpleHTTPResponse PurpleHTTPResponse;
typedef struct _PurpleHTTPConnection PurpleHTTPConnection;
typedef struct _PurpleBOSHConnection PurpleBOSHConnection;

typedef void (*PurpleHTTPConnectionConnectFunction)(PurpleHTTPConnection *conn);
typedef void (*PurpleHTTPRequestCallback)(PurpleHTTPRequest *req, PurpleHTTPResponse *res, void *userdata);
typedef void (*PurpleBOSHConnectionConnectFunction)(PurpleBOSHConnection *conn);
typedef void (*PurpleBOSHConnectionReciveFunction)(PurpleBOSHConnection *conn, xmlnode *node);

struct _PurpleBOSHConnection {
    /* decoded URL */
    char *host;
    int port;
    char *path; 
    char *user;
    char *passwd;
    
    void *userdata;
    PurpleAccount *account;
    gboolean pipelining;
    PurpleHTTPConnection *conn_a;
    PurpleHTTPConnection *conn_b;
    
    PurpleBOSHConnectionConnectFunction connect_cb;
    PurpleBOSHConnectionReciveFunction receive_cb;
};

struct _PurpleHTTPConnection {
    int fd;
    char *host;
    int port;
    int handle;
    PurpleConnection *conn;
    PurpleAccount *account;
    GQueue *requests;
    
    PurpleHTTPConnectionConnectFunction connect_cb;
    void *userdata;
};

struct _PurpleHTTPRequest {
    PurpleHTTPRequestCallback cb;
    char *method;
    char *url;
    GList *header;
    char *data;
    void *userdata;
};

struct _PurpleHTTPResponse {
    int status;
    GHashTable *header;
    char *data;
};

void jabber_bosh_connection_init(PurpleBOSHConnection *conn, PurpleAccount *account, char *url);
void jabber_bosh_connection_connect(PurpleBOSHConnection *conn);

void jabber_bosh_http_connection_init(PurpleHTTPConnection *conn, PurpleAccount *account, char *host, int port);
void jabber_bosh_http_connection_connect(PurpleHTTPConnection *conn);
void jabber_bosh_http_send_request(PurpleHTTPConnection *conn, PurpleHTTPRequest *req);
void jabber_bosh_http_connection_clean(PurpleHTTPConnection *conn);

void jabber_bosh_http_request_init(PurpleHTTPRequest *req, const char *method, const char *path, PurpleHTTPRequestCallback cb, void *userdata);
void jabber_bosh_http_request_clean(PurpleHTTPRequest *req);
#endif /* _PURPLE_JABBER_BOSH_H_ */
