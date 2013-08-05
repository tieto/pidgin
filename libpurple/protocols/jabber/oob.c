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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */
#include "internal.h"
#include "debug.h"
#include "ft.h"
#include "http.h"
#include "util.h"

#include "jabber.h"
#include "iq.h"
#include "oob.h"

typedef struct _JabberOOBXfer {
	JabberStream *js;
	gchar *iq_id;
	gchar *url;
	PurpleHttpConnection *hc;
} JabberOOBXfer;

static void jabber_oob_xfer_init(PurpleXfer *xfer)
{
	purple_xfer_start(xfer, -1, NULL, 0);
}

static void jabber_oob_xfer_free(PurpleXfer *xfer)
{
	JabberOOBXfer *jox = purple_xfer_get_protocol_data(xfer);

	purple_xfer_set_protocol_data(xfer, NULL);
	jox->js->oob_file_transfers = g_list_remove(jox->js->oob_file_transfers,
			xfer);

	g_free(jox->iq_id);
	g_free(jox->url);
	g_free(jox);
}

static void jabber_oob_xfer_end(PurpleXfer *xfer)
{
	JabberOOBXfer *jox = purple_xfer_get_protocol_data(xfer);
	JabberIq *iq;

	iq = jabber_iq_new(jox->js, JABBER_IQ_RESULT);
	xmlnode_set_attrib(iq->node, "to", purple_xfer_get_remote_user(xfer));
	jabber_iq_set_id(iq, jox->iq_id);

	jabber_iq_send(iq);

	jabber_oob_xfer_free(xfer);
}

static void
jabber_oob_xfer_got(PurpleHttpConnection *hc, PurpleHttpResponse *response,
	gpointer _xfer)
{
	PurpleXfer *xfer = _xfer;
	JabberOOBXfer *jox;

	if (purple_xfer_is_cancelled(xfer))
		return;

	jox = purple_xfer_get_protocol_data(xfer);
	jox->hc = NULL;

	if (!purple_http_response_is_successfull(response) ||
		purple_xfer_get_bytes_remaining(xfer) > 0)
	{
		purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_CANCEL_REMOTE);
		purple_xfer_end(xfer);
	} else {
		purple_xfer_set_completed(xfer, TRUE);
		purple_xfer_end(xfer);
	}
}

static void
jabber_oob_xfer_progress_watcher(PurpleHttpConnection *hc,
	gboolean reading_state, int processed, int total, gpointer _xfer)
{
	PurpleXfer *xfer = _xfer;

	if (!reading_state)
		return;

	purple_xfer_set_size(xfer, total);
	purple_xfer_update_progress(xfer);
}

static gboolean
jabber_oob_xfer_writer(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, const gchar *buffer, size_t offset,
	size_t length, gpointer _xfer)
{
	PurpleXfer *xfer = _xfer;

	return purple_xfer_write_file(xfer, (const guchar*)buffer, length);
}

static void jabber_oob_xfer_start(PurpleXfer *xfer)
{
	PurpleHttpRequest *req;
	JabberOOBXfer *jox = purple_xfer_get_protocol_data(xfer);

	req = purple_http_request_new(jox->url);
	purple_http_request_set_max_len(req, -1);
	purple_http_request_set_response_writer(req, jabber_oob_xfer_writer,
		xfer);
	jox->hc = purple_http_request(jox->js->gc, req, jabber_oob_xfer_got,
		xfer);

	purple_http_conn_set_progress_watcher(jox->hc,
		jabber_oob_xfer_progress_watcher, xfer, -1);
}

static void jabber_oob_xfer_recv_error(PurpleXfer *xfer, const char *code) {
	JabberOOBXfer *jox = purple_xfer_get_protocol_data(xfer);
	JabberIq *iq;
	xmlnode *y, *z;

	iq = jabber_iq_new(jox->js, JABBER_IQ_ERROR);
	xmlnode_set_attrib(iq->node, "to", purple_xfer_get_remote_user(xfer));
	jabber_iq_set_id(iq, jox->iq_id);
	y = xmlnode_new_child(iq->node, "error");
	xmlnode_set_attrib(y, "code", code);
	if(!strcmp(code, "406")) {
		z = xmlnode_new_child(y, "not-acceptable");
		xmlnode_set_attrib(y, "type", "modify");
		xmlnode_set_namespace(z, NS_XMPP_STANZAS);
	} else if(!strcmp(code, "404")) {
		z = xmlnode_new_child(y, "not-found");
		xmlnode_set_attrib(y, "type", "cancel");
		xmlnode_set_namespace(z, NS_XMPP_STANZAS);
	}
	jabber_iq_send(iq);

	jabber_oob_xfer_free(xfer);
}

static void jabber_oob_xfer_recv_denied(PurpleXfer *xfer) {
	jabber_oob_xfer_recv_error(xfer, "406");
}

static void jabber_oob_xfer_recv_cancelled(PurpleXfer *xfer) {
	JabberOOBXfer *jox = purple_xfer_get_protocol_data(xfer);

	purple_http_conn_cancel(jox->hc);
	jabber_oob_xfer_recv_error(xfer, "404");
}

void jabber_oob_parse(JabberStream *js, const char *from, JabberIqType type,
	const char *id, xmlnode *querynode) {
	JabberOOBXfer *jox;
	PurpleXfer *xfer;
	const gchar *filename, *slash;
	gchar *url;
	xmlnode *urlnode;

	if(type != JABBER_IQ_SET)
		return;

	if(!from)
		return;

	if(!(urlnode = xmlnode_get_child(querynode, "url")))
		return;

	url = xmlnode_get_data(urlnode);
	if (!url)
		return;

	xfer = purple_xfer_new(purple_connection_get_account(js->gc),
		PURPLE_XFER_RECEIVE, from);
	if (!xfer) {
		g_free(url);
		return;
	}

	jox = g_new0(JabberOOBXfer, 1);
	jox->iq_id = g_strdup(id);
	jox->js = js;
	jox->url = url;
	purple_xfer_set_protocol_data(xfer, jox);

	slash = strrchr(url, '/');
	if (slash == NULL)
		filename = url;
	else
		filename = slash + 1;
	purple_xfer_set_filename(xfer, filename);

	purple_xfer_set_init_fnc(xfer, jabber_oob_xfer_init);
	purple_xfer_set_end_fnc(xfer, jabber_oob_xfer_end);
	purple_xfer_set_request_denied_fnc(xfer, jabber_oob_xfer_recv_denied);
	purple_xfer_set_cancel_recv_fnc(xfer, jabber_oob_xfer_recv_cancelled);
	purple_xfer_set_start_fnc(xfer, jabber_oob_xfer_start);

	js->oob_file_transfers = g_list_append(js->oob_file_transfers, xfer);

	purple_xfer_request(xfer);
}
