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
typedef struct _PurpleHTTPHeaderField PurpleHTTPHeaderField;

typedef void (*PurpleHTTPRequestCallback)(PurpleHTTPRequest *req, PurpleHTTPResponse *res, void *userdata);

typedef struct {
    int fd;
    PurpleConnection *conn;
    GQueue *requests;
    void *userdata;
} PurpleHTTPConnection;

typedef struct {
    char *url;
    gboolean pipelining;
    PurpleHTTPConnection *conn_a;
    PurpleHTTPConnection *conn_b;
} PurpleBOSHConnection;

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
    GList *header;
    char *data;
};

struct _PurpleHTTPHeaderField {
    char *name;
    char *value;
};

PurpleHTTPHeaderField *jabber_bosh_http_header_field(const char *name, const char *value);

void jabber_bosh_http_connection_connect(PurpleHTTPConnection *conn);
void jabber_bosh_http_send_request(PurpleHTTPConnection *conn, PurpleHTTPRequest *req);
void jabber_bosh_http_connection_clean(PurpleHTTPConnection *conn);

void jabber_bosh_http_request_init(PurpleHTTPRequest *req, const char *method, const char *url, PurpleHTTPRequestCallback cb, void *userdata);
void jabber_bosh_http_request_clean(PurpleHTTPRequest *req);
#endif /* _PURPLE_JABBER_BOSH_H_ */
