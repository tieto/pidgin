/*
 * gaim - Jabber Protocol Plugin
 *
 * Copyright (C) 2003, Nathan Walp <faceprint@faceprint.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "blist.h"

#include "internal.h"
#include "cipher.h"
#include "debug.h"
#include "ft.h"
#include "network.h"
#include "notify.h"

#include "buddy.h"
#include "disco.h"
#include "jabber.h"
#include "iq.h"
#include "si.h"

#include "si.h"

struct bytestreams_streamhost {
	char *jid;
	char *host;
	int port;
};

typedef struct _JabberSIXfer {
	JabberStream *js;

	GaimProxyConnectData *connect_data;
	GaimNetworkListenData *listen_data;

	gboolean accepted;

	char *stream_id;
	char *iq_id;

	enum {
		STREAM_METHOD_UNKNOWN = 0,
		STREAM_METHOD_BYTESTREAMS = 2 << 1,
		STREAM_METHOD_IBB = 2 << 2,
		STREAM_METHOD_UNSUPPORTED = 2 << 31
	} stream_method;

	GList *streamhosts;
	GaimProxyInfo *gpi;

	char *rxqueue;
	size_t rxlen;
	gsize rxmaxlen;
} JabberSIXfer;

static GaimXfer*
jabber_si_xfer_find(JabberStream *js, const char *sid, const char *from)
{
	GList *xfers;

	if(!sid || !from)
		return NULL;

	for(xfers = js->file_transfers; xfers; xfers = xfers->next) {
		GaimXfer *xfer = xfers->data;
		JabberSIXfer *jsx = xfer->data;
		if(jsx->stream_id && xfer->who &&
				!strcmp(jsx->stream_id, sid) && !strcmp(xfer->who, from))
			return xfer;
	}

	return NULL;
}


static void jabber_si_bytestreams_attempt_connect(GaimXfer *xfer);

static void
jabber_si_bytestreams_connect_cb(gpointer data, gint source, const gchar *error_message)
{
	GaimXfer *xfer = data;
	JabberSIXfer *jsx = xfer->data;
	JabberIq *iq;
	xmlnode *query, *su;
	struct bytestreams_streamhost *streamhost = jsx->streamhosts->data;

	gaim_proxy_info_destroy(jsx->gpi);
	jsx->connect_data = NULL;

	if(source < 0) {
		jsx->streamhosts = g_list_remove(jsx->streamhosts, streamhost);
		g_free(streamhost->jid);
		g_free(streamhost->host);
		g_free(streamhost);
		jabber_si_bytestreams_attempt_connect(xfer);
		return;
	}

	iq = jabber_iq_new_query(jsx->js, JABBER_IQ_RESULT, "http://jabber.org/protocol/bytestreams");
	xmlnode_set_attrib(iq->node, "to", xfer->who);
	jabber_iq_set_id(iq, jsx->iq_id);
	query = xmlnode_get_child(iq->node, "query");
	su = xmlnode_new_child(query, "streamhost-used");
	xmlnode_set_attrib(su, "jid", streamhost->jid);

	jabber_iq_send(iq);

	gaim_xfer_start(xfer, source, NULL, -1);
}

static void jabber_si_bytestreams_attempt_connect(GaimXfer *xfer)
{
	JabberSIXfer *jsx = xfer->data;
	struct bytestreams_streamhost *streamhost;
	char *dstaddr, *p;
	int i;
	unsigned char hashval[20];
	JabberID *dstjid;

	if(!jsx->streamhosts) {
		JabberIq *iq = jabber_iq_new(jsx->js, JABBER_IQ_ERROR);
		xmlnode *error, *inf;

		if(jsx->iq_id)
			jabber_iq_set_id(iq, jsx->iq_id);

		xmlnode_set_attrib(iq->node, "to", xfer->who);
		error = xmlnode_new_child(iq->node, "error");
		xmlnode_set_attrib(error, "code", "404");
		xmlnode_set_attrib(error, "type", "cancel");
		inf = xmlnode_new_child(error, "item-not-found");
		xmlnode_set_namespace(inf, "urn:ietf:params:xml:ns:xmpp-stanzas");

		jabber_iq_send(iq);

		gaim_xfer_cancel_local(xfer);

		return;
	}

	streamhost = jsx->streamhosts->data;

	dstjid = jabber_id_new(xfer->who);

	if(dstjid != NULL) {
		jsx->gpi = gaim_proxy_info_new();
		gaim_proxy_info_set_type(jsx->gpi, GAIM_PROXY_SOCKS5);
		gaim_proxy_info_set_host(jsx->gpi, streamhost->host);
		gaim_proxy_info_set_port(jsx->gpi, streamhost->port);



		dstaddr = g_strdup_printf("%s%s@%s/%s%s@%s/%s", jsx->stream_id, dstjid->node, dstjid->domain, dstjid->resource, jsx->js->user->node,
				jsx->js->user->domain, jsx->js->user->resource);

		gaim_cipher_digest_region("sha1", (guchar *)dstaddr, strlen(dstaddr),
				sizeof(hashval), hashval, NULL);
		g_free(dstaddr);
		dstaddr = g_malloc(41);
		p = dstaddr;
		for(i=0; i<20; i++, p+=2)
			snprintf(p, 3, "%02x", hashval[i]);

		jsx->connect_data = gaim_proxy_connect_socks5(NULL, jsx->gpi,
				dstaddr, 0,
				jabber_si_bytestreams_connect_cb, xfer);
		g_free(dstaddr);

		jabber_id_free(dstjid);
	}

	if (jsx->connect_data == NULL)
	{
		jsx->streamhosts = g_list_remove(jsx->streamhosts, streamhost);
		g_free(streamhost->jid);
		g_free(streamhost->host);
		g_free(streamhost);
		jabber_si_bytestreams_attempt_connect(xfer);
	}
}

void jabber_bytestreams_parse(JabberStream *js, xmlnode *packet)
{
	GaimXfer *xfer;
	JabberSIXfer *jsx;
	xmlnode *query, *streamhost;
	const char *sid, *from, *type;

	if(!(type = xmlnode_get_attrib(packet, "type")) || strcmp(type, "set"))
		return;

	if(!(from = xmlnode_get_attrib(packet, "from")))
		return;

	if(!(query = xmlnode_get_child(packet, "query")))
		return;

	if(!(sid = xmlnode_get_attrib(query, "sid")))
		return;

	if(!(xfer = jabber_si_xfer_find(js, sid, from)))
		return;

	jsx = xfer->data;

	if(!jsx->accepted)
		return;

	if(jsx->iq_id)
		g_free(jsx->iq_id);
	jsx->iq_id = g_strdup(xmlnode_get_attrib(packet, "id"));

	for(streamhost = xmlnode_get_child(query, "streamhost"); streamhost;
			streamhost = xmlnode_get_next_twin(streamhost)) {
		const char *jid, *host, *port;
		int portnum;

		if((jid = xmlnode_get_attrib(streamhost, "jid")) &&
				(host = xmlnode_get_attrib(streamhost, "host")) &&
				(port = xmlnode_get_attrib(streamhost, "port")) &&
				(portnum = atoi(port))) {
			struct bytestreams_streamhost *sh = g_new0(struct bytestreams_streamhost, 1);
			sh->jid = g_strdup(jid);
			sh->host = g_strdup(host);
			sh->port = portnum;
			jsx->streamhosts = g_list_append(jsx->streamhosts, sh);
		}
	}

	jabber_si_bytestreams_attempt_connect(xfer);
}


static void
jabber_si_xfer_bytestreams_send_read_again_resp_cb(gpointer data, gint source,
		GaimInputCondition cond)
{
	GaimXfer *xfer = data;
	JabberSIXfer *jsx = xfer->data;
	int len;

	len = write(source, jsx->rxqueue + jsx->rxlen, jsx->rxmaxlen - jsx->rxlen);
	if (len < 0 && errno == EAGAIN)
		return;
	else if (len < 0) {
		gaim_input_remove(xfer->watcher);
		xfer->watcher = 0;
		g_free(jsx->rxqueue);
		jsx->rxqueue = NULL;
		close(source);
		gaim_xfer_cancel_remote(xfer);
		return;
	}
	jsx->rxlen += len;

	if (jsx->rxlen < jsx->rxmaxlen)
		return;

	gaim_input_remove(xfer->watcher);
	xfer->watcher = 0;
	g_free(jsx->rxqueue);
	jsx->rxqueue = NULL;

	gaim_xfer_start(xfer, source, NULL, -1);
}

static void
jabber_si_xfer_bytestreams_send_read_again_cb(gpointer data, gint source,
		GaimInputCondition cond)
{
	GaimXfer *xfer = data;
	JabberSIXfer *jsx = xfer->data;
	int i;
	char buffer[256];
	int len;
	char *dstaddr, *p;
	unsigned char hashval[20];
	const char *host;

	gaim_debug_info("jabber", "in jabber_si_xfer_bytestreams_send_read_again_cb\n");

	if(jsx->rxlen < 5) {
		gaim_debug_info("jabber", "reading the first 5 bytes\n");
		len = read(source, buffer, 5 - jsx->rxlen);
		if(len < 0 && errno == EAGAIN)
			return;
		else if(len <= 0) {
			gaim_input_remove(xfer->watcher);
			xfer->watcher = 0;
			close(source);
			gaim_xfer_cancel_remote(xfer);
			return;
		}
		jsx->rxqueue = g_realloc(jsx->rxqueue, len + jsx->rxlen);
		memcpy(jsx->rxqueue + jsx->rxlen, buffer, len);
		jsx->rxlen += len;
		return;
	} else if(jsx->rxqueue[0] != 0x05 || jsx->rxqueue[1] != 0x01 ||
			jsx->rxqueue[3] != 0x03) {
		gaim_debug_info("jabber", "invalid socks5 stuff\n");
		gaim_input_remove(xfer->watcher);
		xfer->watcher = 0;
		close(source);
		gaim_xfer_cancel_remote(xfer);
		return;
	} else if(jsx->rxlen - 5 <  jsx->rxqueue[4] + 2) {
		gaim_debug_info("jabber", "reading umpteen more bytes\n");
		len = read(source, buffer, jsx->rxqueue[4] + 5 + 2 - jsx->rxlen);
		if(len < 0 && errno == EAGAIN)
			return;
		else if(len <= 0) {
			gaim_input_remove(xfer->watcher);
			xfer->watcher = 0;
			close(source);
			gaim_xfer_cancel_remote(xfer);
			return;
		}
		jsx->rxqueue = g_realloc(jsx->rxqueue, len + jsx->rxlen);
		memcpy(jsx->rxqueue + jsx->rxlen, buffer, len);
		jsx->rxlen += len;
	}

	if(jsx->rxlen - 5 < jsx->rxqueue[4] + 2)
		return;

	gaim_input_remove(xfer->watcher);
	xfer->watcher = 0;

	dstaddr = g_strdup_printf("%s%s@%s/%s%s", jsx->stream_id,
			jsx->js->user->node, jsx->js->user->domain,
			jsx->js->user->resource, xfer->who);

	gaim_cipher_digest_region("sha1", (guchar *)dstaddr, strlen(dstaddr),
							  sizeof(hashval), hashval, NULL);
	g_free(dstaddr);
	dstaddr = g_malloc(41);
	p = dstaddr;
	for(i=0; i<20; i++, p+=2)
		snprintf(p, 3, "%02x", hashval[i]);

	if(jsx->rxqueue[4] != 40 || strncmp(dstaddr, jsx->rxqueue+5, 40) ||
			jsx->rxqueue[45] != 0x00 || jsx->rxqueue[46] != 0x00) {
		gaim_debug_error("jabber", "someone connected with the wrong info!\n");
		close(source);
		gaim_xfer_cancel_remote(xfer);
		return;
	}

	g_free(jsx->rxqueue);
	host = gaim_network_get_my_ip(jsx->js->fd);

	jsx->rxmaxlen = 5 + strlen(host) + 2;
	jsx->rxqueue = g_malloc(jsx->rxmaxlen);
	jsx->rxlen = 0;

	jsx->rxqueue[0] = 0x05;
	jsx->rxqueue[1] = 0x00;
	jsx->rxqueue[2] = 0x00;
	jsx->rxqueue[3] = 0x03;
	jsx->rxqueue[4] = strlen(host);
	memcpy(jsx->rxqueue + 5, host, strlen(host));
	jsx->rxqueue[5+strlen(host)] = 0x00;
	jsx->rxqueue[6+strlen(host)] = 0x00;

	xfer->watcher = gaim_input_add(source, GAIM_INPUT_WRITE,
		jabber_si_xfer_bytestreams_send_read_again_resp_cb, xfer);
	jabber_si_xfer_bytestreams_send_read_again_resp_cb(xfer, source,
		GAIM_INPUT_WRITE);
}

static void
jabber_si_xfer_bytestreams_send_read_response_cb(gpointer data, gint source,
		GaimInputCondition cond)
{
	GaimXfer *xfer = data;
	JabberSIXfer *jsx = xfer->data;
	int len;

	len = write(source, jsx->rxqueue + jsx->rxlen, jsx->rxmaxlen - jsx->rxlen);
	if (len < 0 && errno == EAGAIN)
		return;
	else if (len < 0) {
		gaim_input_remove(xfer->watcher);
		xfer->watcher = 0;
		g_free(jsx->rxqueue);
		jsx->rxqueue = NULL;
		close(source);
		gaim_xfer_cancel_remote(xfer);
		return;
	}
	jsx->rxlen += len;

	if (jsx->rxlen < jsx->rxmaxlen)
		return;

	gaim_input_remove(xfer->watcher);
	xfer->watcher = 0;

	if (jsx->rxqueue[1] == 0x00) {
		xfer->watcher = gaim_input_add(source, GAIM_INPUT_READ,
			jabber_si_xfer_bytestreams_send_read_again_cb, xfer);
		g_free(jsx->rxqueue);
		jsx->rxqueue = NULL;
	} else {
		close(source);
		gaim_xfer_cancel_remote(xfer);
	}
}

static void
jabber_si_xfer_bytestreams_send_read_cb(gpointer data, gint source,
		GaimInputCondition cond)
{
	GaimXfer *xfer = data;
	JabberSIXfer *jsx = xfer->data;
	int i;
	int len;
	char buffer[256];

	gaim_debug_info("jabber", "in jabber_si_xfer_bytestreams_send_read_cb\n");

	xfer->fd = source;

	if(jsx->rxlen < 2) {
		gaim_debug_info("jabber", "reading those first two bytes\n");
		len = read(source, buffer, 2 - jsx->rxlen);
		if(len < 0 && errno == EAGAIN)
			return;
		else if(len <= 0) {
			gaim_input_remove(xfer->watcher);
			xfer->watcher = 0;
			close(source);
			gaim_xfer_cancel_remote(xfer);
			return;
		}
		jsx->rxqueue = g_realloc(jsx->rxqueue, len + jsx->rxlen);
		memcpy(jsx->rxqueue + jsx->rxlen, buffer, len);
		jsx->rxlen += len;
		return;
	} else if(jsx->rxlen - 2 <  jsx->rxqueue[1]) {
		gaim_debug_info("jabber", "reading the next umpteen bytes\n");
		len = read(source, buffer, jsx->rxqueue[1] + 2 - jsx->rxlen);
		if(len < 0 && errno == EAGAIN)
			return;
		else if(len <= 0) {
			gaim_input_remove(xfer->watcher);
			xfer->watcher = 0;
			close(source);
			gaim_xfer_cancel_remote(xfer);
			return;
		}
		jsx->rxqueue = g_realloc(jsx->rxqueue, len + jsx->rxlen);
		memcpy(jsx->rxqueue + jsx->rxlen, buffer, len);
		jsx->rxlen += len;
	}

	if(jsx->rxlen -2 < jsx->rxqueue[1])
		return;

	gaim_input_remove(xfer->watcher);
	xfer->watcher = 0;

	gaim_debug_info("jabber", "checking to make sure we're socks FIVE\n");

	if(jsx->rxqueue[0] != 0x05) {
		close(source);
		gaim_xfer_cancel_remote(xfer);
		return;
	}

	gaim_debug_info("jabber", "going to test %hhu different methods\n", jsx->rxqueue[1]);

	for(i=0; i<jsx->rxqueue[1]; i++) {

		gaim_debug_info("jabber", "testing %hhu\n", jsx->rxqueue[i+2]);
		if(jsx->rxqueue[i+2] == 0x00) {
			g_free(jsx->rxqueue);
			jsx->rxlen = 0;
			jsx->rxmaxlen = 2;
			jsx->rxqueue = g_malloc(jsx->rxmaxlen);
			jsx->rxqueue[0] = 0x05;
			jsx->rxqueue[1] = 0x00;
			xfer->watcher = gaim_input_add(source, GAIM_INPUT_WRITE,
				jabber_si_xfer_bytestreams_send_read_response_cb,
				xfer);
			jabber_si_xfer_bytestreams_send_read_response_cb(xfer,
				source, GAIM_INPUT_WRITE);
			jsx->rxqueue = NULL;
			jsx->rxlen = 0;
			return;
		}
	}

	g_free(jsx->rxqueue);
	jsx->rxlen = 0;
	jsx->rxmaxlen = 2;
	jsx->rxqueue = g_malloc(jsx->rxmaxlen);
	jsx->rxqueue[0] = 0x05;
	jsx->rxqueue[1] = 0xFF;
	xfer->watcher = gaim_input_add(source, GAIM_INPUT_WRITE,
		jabber_si_xfer_bytestreams_send_read_response_cb, xfer);
	jabber_si_xfer_bytestreams_send_read_response_cb(xfer,
		source, GAIM_INPUT_WRITE);
}

static void
jabber_si_xfer_bytestreams_send_connected_cb(gpointer data, gint source,
		GaimInputCondition cond)
{
	GaimXfer *xfer = data;
	int acceptfd;

	gaim_debug_info("jabber", "in jabber_si_xfer_bytestreams_send_connected_cb\n");

	acceptfd = accept(source, NULL, 0);
	if(acceptfd == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
		return;
	else if(acceptfd == -1) {
		gaim_debug_warning("jabber", "accept: %s\n", strerror(errno));
		return;
	}

	gaim_input_remove(xfer->watcher);
	close(source);

	xfer->watcher = gaim_input_add(acceptfd, GAIM_INPUT_READ,
			jabber_si_xfer_bytestreams_send_read_cb, xfer);
}

static void
jabber_si_xfer_bytestreams_listen_cb(int sock, gpointer data)
{
	GaimXfer *xfer = data;
	JabberSIXfer *jsx;
	JabberIq *iq;
	xmlnode *query, *streamhost;
	char *jid, *port;

	jsx = xfer->data;
	jsx->listen_data = NULL;

	if (gaim_xfer_get_status(xfer) == GAIM_XFER_STATUS_CANCEL_LOCAL) {
		gaim_xfer_unref(xfer);
		return;
	}

	gaim_xfer_unref(xfer);

	if (sock < 0) {
		gaim_xfer_cancel_local(xfer);
		return;
	}

	iq = jabber_iq_new_query(jsx->js, JABBER_IQ_SET,
			"http://jabber.org/protocol/bytestreams");
	xmlnode_set_attrib(iq->node, "to", xfer->who);
	query = xmlnode_get_child(iq->node, "query");

	xmlnode_set_attrib(query, "sid", jsx->stream_id);

	streamhost = xmlnode_new_child(query, "streamhost");
	jid = g_strdup_printf("%s@%s/%s", jsx->js->user->node,
			jsx->js->user->domain, jsx->js->user->resource);
	xmlnode_set_attrib(streamhost, "jid", jid);
	g_free(jid);

	/* XXX: shouldn't we use the public IP or something? here */
	xmlnode_set_attrib(streamhost, "host",
			gaim_network_get_my_ip(jsx->js->fd));
	xfer->local_port = gaim_network_get_port_from_fd(sock);
	port = g_strdup_printf("%hu", xfer->local_port);
	xmlnode_set_attrib(streamhost, "port", port);
	g_free(port);

	xfer->watcher = gaim_input_add(sock, GAIM_INPUT_READ,
			jabber_si_xfer_bytestreams_send_connected_cb, xfer);

	/* XXX: insert proxies here */

	/* XXX: callback to find out which streamhost they used, or see if they
	 * screwed it up */
	jabber_iq_send(iq);

}

static void
jabber_si_xfer_bytestreams_send_init(GaimXfer *xfer)
{
	JabberSIXfer *jsx;

	gaim_xfer_ref(xfer);

	jsx = xfer->data;
	jsx->listen_data = gaim_network_listen_range(0, 0, SOCK_STREAM,
				jabber_si_xfer_bytestreams_listen_cb, xfer);
	if (jsx->listen_data == NULL) {
		gaim_xfer_unref(xfer);
		/* XXX: couldn't open a port, we're fscked */
		gaim_xfer_cancel_local(xfer);
		return;
	}

}

static void jabber_si_xfer_send_method_cb(JabberStream *js, xmlnode *packet,
		gpointer data)
{
	GaimXfer *xfer = data;
	xmlnode *si, *feature, *x, *field, *value;

	if(!(si = xmlnode_get_child_with_namespace(packet, "si", "http://jabber.org/protocol/si"))) {
		gaim_xfer_cancel_remote(xfer);
		return;
	}

	if(!(feature = xmlnode_get_child_with_namespace(si, "feature", "http://jabber.org/protocol/feature-neg"))) {
		gaim_xfer_cancel_remote(xfer);
		return;
	}

	if(!(x = xmlnode_get_child_with_namespace(feature, "x", "jabber:x:data"))) {
		gaim_xfer_cancel_remote(xfer);
		return;
	}

	for(field = xmlnode_get_child(x, "field"); field; field = xmlnode_get_next_twin(field)) {
		const char *var = xmlnode_get_attrib(field, "var");

		if(var && !strcmp(var, "stream-method")) {
			if((value = xmlnode_get_child(field, "value"))) {
				char *val = xmlnode_get_data(value);
				if(val && !strcmp(val, "http://jabber.org/protocol/bytestreams")) {
					jabber_si_xfer_bytestreams_send_init(xfer);
					g_free(val);
					return;
				}
				g_free(val);
			}
		}
	}
	gaim_xfer_cancel_remote(xfer);
}

static void jabber_si_xfer_send_request(GaimXfer *xfer)
{
	JabberSIXfer *jsx = xfer->data;
	JabberIq *iq;
	xmlnode *si, *file, *feature, *x, *field, *option, *value;
	char buf[32];

	xfer->filename = g_path_get_basename(xfer->local_filename);

	iq = jabber_iq_new(jsx->js, JABBER_IQ_SET);
	xmlnode_set_attrib(iq->node, "to", xfer->who);
	si = xmlnode_new_child(iq->node, "si");
	xmlnode_set_namespace(si, "http://jabber.org/protocol/si");
	jsx->stream_id = jabber_get_next_id(jsx->js);
	xmlnode_set_attrib(si, "id", jsx->stream_id);
	xmlnode_set_attrib(si, "profile",
			"http://jabber.org/protocol/si/profile/file-transfer");

	file = xmlnode_new_child(si, "file");
	xmlnode_set_namespace(file,
			"http://jabber.org/protocol/si/profile/file-transfer");
	xmlnode_set_attrib(file, "name", xfer->filename);
	g_snprintf(buf, sizeof(buf), "%" G_GSIZE_FORMAT, xfer->size);
	xmlnode_set_attrib(file, "size", buf);
	/* maybe later we'll do hash and date attribs */

	feature = xmlnode_new_child(si, "feature");
	xmlnode_set_namespace(feature,
			"http://jabber.org/protocol/feature-neg");
	x = xmlnode_new_child(feature, "x");
	xmlnode_set_namespace(x, "jabber:x:data");
	xmlnode_set_attrib(x, "type", "form");
	field = xmlnode_new_child(x, "field");
	xmlnode_set_attrib(field, "var", "stream-method");
	xmlnode_set_attrib(field, "type", "list-single");
	option = xmlnode_new_child(field, "option");
	value = xmlnode_new_child(option, "value");
	xmlnode_insert_data(value, "http://jabber.org/protocol/bytestreams",
			-1);
	/*
	option = xmlnode_new_child(field, "option");
	value = xmlnode_new_child(option, "value");
	xmlnode_insert_data(value, "http://jabber.org/protocol/ibb", -1);
	*/

	jabber_iq_set_callback(iq, jabber_si_xfer_send_method_cb, xfer);

	jabber_iq_send(iq);
}

static void jabber_si_xfer_free(GaimXfer *xfer)
{
	JabberSIXfer *jsx = xfer->data;
	JabberStream *js = jsx->js;

	js->file_transfers = g_list_remove(js->file_transfers, xfer);

	if (jsx->connect_data != NULL)
		gaim_proxy_connect_cancel(jsx->connect_data);
	if (jsx->listen_data != NULL)
		gaim_network_listen_cancel(jsx->listen_data);

	g_free(jsx->stream_id);
	g_free(jsx->iq_id);
	/* XXX: free other stuff */
	g_free(jsx->rxqueue);
	g_free(jsx);
	xfer->data = NULL;
}

static void jabber_si_xfer_cancel_send(GaimXfer *xfer)
{
	jabber_si_xfer_free(xfer);
	gaim_debug(GAIM_DEBUG_INFO, "jabber", "in jabber_si_xfer_cancel_send\n");
}


static void jabber_si_xfer_request_denied(GaimXfer *xfer)
{
	jabber_si_xfer_free(xfer);
	gaim_debug(GAIM_DEBUG_INFO, "jabber", "in jabber_si_xfer_request_denied\n");
}


static void jabber_si_xfer_cancel_recv(GaimXfer *xfer)
{
	jabber_si_xfer_free(xfer);
	gaim_debug(GAIM_DEBUG_INFO, "jabber", "in jabber_si_xfer_cancel_recv\n");
}


static void jabber_si_xfer_end(GaimXfer *xfer)
{
	jabber_si_xfer_free(xfer);
}


static void jabber_si_xfer_send_disco_cb(JabberStream *js, const char *who,
		JabberCapabilities capabilities, gpointer data)
{
	GaimXfer *xfer = data;

	if(capabilities & JABBER_CAP_SI_FILE_XFER) {
		jabber_si_xfer_send_request(xfer);
	} else {
		char *msg = g_strdup_printf(_("Unable to send file to %s, user does not support file transfers"), who);
		gaim_notify_error(js->gc, _("File Send Failed"),
				_("File Send Failed"), msg);
		g_free(msg);
	}
}

static void jabber_si_xfer_init(GaimXfer *xfer)
{
	JabberSIXfer *jsx = xfer->data;
	JabberIq *iq;
	if(gaim_xfer_get_type(xfer) == GAIM_XFER_SEND) {
		JabberBuddy *jb;
		JabberBuddyResource *jbr = NULL;

		jb = jabber_buddy_find(jsx->js, xfer->who, TRUE);
		/* XXX */
		if(!jb)
			return;

		/* XXX: for now, send to the first resource available */
		if(jb->resources != NULL) {
			char **who_v = g_strsplit(xfer->who, "/", 2);
			char *who;

			jbr = jabber_buddy_find_resource(jb, NULL);
			who = g_strdup_printf("%s/%s", who_v[0], jbr->name);
			g_strfreev(who_v);
			g_free(xfer->who);
			xfer->who = who;
			jabber_disco_info_do(jsx->js, who,
					jabber_si_xfer_send_disco_cb, xfer);
		} else {
			return; /* XXX: ick */
		}
	} else {
		xmlnode *si, *feature, *x, *field, *value;

		iq = jabber_iq_new(jsx->js, JABBER_IQ_RESULT);
		xmlnode_set_attrib(iq->node, "to", xfer->who);
		if(jsx->iq_id)
			jabber_iq_set_id(iq, jsx->iq_id);

		jsx->accepted = TRUE;

		si = xmlnode_new_child(iq->node, "si");
		xmlnode_set_namespace(si, "http://jabber.org/protocol/si");

		feature = xmlnode_new_child(si, "feature");
		xmlnode_set_namespace(feature, "http://jabber.org/protocol/feature-neg");

		x = xmlnode_new_child(feature, "x");
		xmlnode_set_namespace(x, "jabber:x:data");
		xmlnode_set_attrib(x, "type", "submit");

		field = xmlnode_new_child(x, "field");
		xmlnode_set_attrib(field, "var", "stream-method");

		value = xmlnode_new_child(field, "value");
		if(jsx->stream_method & STREAM_METHOD_BYTESTREAMS)
			xmlnode_insert_data(value, "http://jabber.org/protocol/bytestreams", -1);
		/*
		else if(jsx->stream_method & STREAM_METHOD_IBB)
		xmlnode_insert_data(value, "http://jabber.org/protocol/ibb", -1);
		*/

		jabber_iq_send(iq);
	}
}

GaimXfer *jabber_si_new_xfer(GaimConnection *gc, const char *who)
{
	JabberStream *js;

	GaimXfer *xfer;
	JabberSIXfer *jsx;

	js = gc->proto_data;

	xfer = gaim_xfer_new(gc->account, GAIM_XFER_SEND, who);
	if (xfer)
	{
		xfer->data = jsx = g_new0(JabberSIXfer, 1);
		jsx->js = js;

		gaim_xfer_set_init_fnc(xfer, jabber_si_xfer_init);
		gaim_xfer_set_cancel_send_fnc(xfer, jabber_si_xfer_cancel_send);
		gaim_xfer_set_end_fnc(xfer, jabber_si_xfer_end);

		js->file_transfers = g_list_append(js->file_transfers, xfer);
	}

	return xfer;
}

void jabber_si_xfer_send(GaimConnection *gc, const char *who, const char *file)
{
	JabberStream *js;

	GaimXfer *xfer;

	js = gc->proto_data;

	if(!gaim_find_buddy(gc->account, who) || !jabber_buddy_find(js, who, FALSE))
		return;

	xfer = jabber_si_new_xfer(gc, who);

	if (file)
		gaim_xfer_request_accepted(xfer, file);
	else
		gaim_xfer_request(xfer);
}

void jabber_si_parse(JabberStream *js, xmlnode *packet)
{
	JabberSIXfer *jsx;
	GaimXfer *xfer;
	xmlnode *si, *file, *feature, *x, *field, *option, *value;
	const char *stream_id, *filename, *filesize_c, *profile, *from;
	size_t filesize = 0;

	if(!(si = xmlnode_get_child(packet, "si")))
		return;

	if(!(profile = xmlnode_get_attrib(si, "profile")) ||
			strcmp(profile, "http://jabber.org/protocol/si/profile/file-transfer"))
		return;

	if(!(stream_id = xmlnode_get_attrib(si, "id")))
		return;

	if(!(file = xmlnode_get_child(si, "file")))
		return;

	if(!(filename = xmlnode_get_attrib(file, "name")))
		return;

	if((filesize_c = xmlnode_get_attrib(file, "size")))
		filesize = atoi(filesize_c);

	if(!(feature = xmlnode_get_child(si, "feature")))
		return;

	if(!(x = xmlnode_get_child_with_namespace(feature, "x", "jabber:x:data")))
		return;

	if(!(from = xmlnode_get_attrib(packet, "from")))
		return;

	/* if they've already sent us this file transfer with the same damn id
	 * then we're gonna ignore it, until I think of something better to do
	 * with it */
	if((xfer = jabber_si_xfer_find(js, stream_id, from)))
		return;

	jsx = g_new0(JabberSIXfer, 1);

	for(field = xmlnode_get_child(x, "field"); field; field = xmlnode_get_next_twin(field)) {
		const char *var = xmlnode_get_attrib(field, "var");
		if(var && !strcmp(var, "stream-method")) {
			for(option = xmlnode_get_child(field, "option"); option;
					option = xmlnode_get_next_twin(option)) {
				if((value = xmlnode_get_child(option, "value"))) {
					char *val;
					if((val = xmlnode_get_data(value))) {
						if(!strcmp(val, "http://jabber.org/protocol/bytestreams")) {
							jsx->stream_method |= STREAM_METHOD_BYTESTREAMS;
							/*
						} else if(!strcmp(val, "http://jabber.org/protocol/ibb")) {
							jsx->stream_method |= STREAM_METHOD_IBB;
							*/
						}
						g_free(val);
					}
				}
			}
		}
	}

	if(jsx->stream_method == STREAM_METHOD_UNKNOWN) {
		g_free(jsx);
		return;
	}

	jsx->js = js;
	jsx->stream_id = g_strdup(stream_id);
	jsx->iq_id = g_strdup(xmlnode_get_attrib(packet, "id"));

	xfer = gaim_xfer_new(js->gc->account, GAIM_XFER_RECEIVE, from);
	if (xfer)
	{
		xfer->data = jsx;

		gaim_xfer_set_filename(xfer, filename);
		if(filesize > 0)
			gaim_xfer_set_size(xfer, filesize);

		gaim_xfer_set_init_fnc(xfer, jabber_si_xfer_init);
		gaim_xfer_set_request_denied_fnc(xfer, jabber_si_xfer_request_denied);
		gaim_xfer_set_cancel_recv_fnc(xfer, jabber_si_xfer_cancel_recv);
		gaim_xfer_set_end_fnc(xfer, jabber_si_xfer_end);

		js->file_transfers = g_list_append(js->file_transfers, xfer);

		gaim_xfer_request(xfer);
	}
}


