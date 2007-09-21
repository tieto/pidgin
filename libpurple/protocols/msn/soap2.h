/**
 * @file soap2.h
 * 	header file for SOAP connection related process
 *
 * purple
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _MSN_SOAP2_H
#define _MSN_SOAP2_H

#include "session.h"
#include "sslconn.h"
#include "xmlnode.h"

#include <glib.h>

typedef struct _MsnSoapMessage MsnSoapMessage;
typedef struct _MsnSoapConnection2 MsnSoapConnection2;
typedef struct _MsnSoapRequest MsnSoapRequest;
typedef struct _MsnSoapResponse MsnSoapResponse;

typedef void (*MsnSoapCallback)(MsnSoapConnection2 *conn,
		MsnSoapRequest *req, MsnSoapResponse *resp, gpointer cb_data);

struct _MsnSoapMessage {
	xmlnode *xml;
	GSList *headers;

	char *buf;
	gsize buf_len;
	gsize buf_count;
};

struct _MsnSoapRequest {
	MsnSoapMessage *message;
	char *action;

	MsnSoapCallback cb;
	gpointer data;

	char *host;
	char *path;
};

struct _MsnSoapResponse {
	MsnSoapMessage *message;
	int code;
	gboolean seen_newline;
	int body_len;
};

struct _MsnSoapConnection2 {
	MsnSession *session;

	char *path;

	PurpleSslConnection *ssl;
	gboolean connected;

	guint idle_handle;
	guint io_handle;
	GQueue *queue;

	MsnSoapRequest *request;
	MsnSoapResponse *response;
};

MsnSoapConnection2 *msn_soap_connection2_new(MsnSession *session);

void msn_soap_connection2_post(MsnSoapConnection2 *conn, MsnSoapRequest *req,
		MsnSoapCallback cb, gpointer data);

void msn_soap_connection2_destroy(MsnSoapConnection2 *conn);

MsnSoapMessage *msn_soap_message_new(void);

void msn_soap_message_destroy(MsnSoapMessage *req);

void msn_soap_message_add_header(MsnSoapMessage *req,
	const char *name, const char *value);

MsnSoapRequest *msn_soap_request2_new(const char *host, const char *path,
		const char *action);

void msn_soap_request2_destroy(MsnSoapRequest *req);

MsnSoapResponse *msn_soap_response_new(void);

void msn_soap_response_destroy(MsnSoapResponse *resp);

#endif
