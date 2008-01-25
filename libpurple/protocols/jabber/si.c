/*
 * purple - Jabber Protocol Plugin
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */

#include "internal.h"

#include "blist.h"
#include "cipher.h"
#include "debug.h"
#include "ft.h"
#include "request.h"
#include "network.h"
#include "notify.h"

#include "buddy.h"
#include "disco.h"
#include "jabber.h"
#include "iq.h"
#include "si.h"

#define STREAMHOST_CONNECT_TIMEOUT 15

typedef struct _JabberSIXfer {
	JabberStream *js;

	PurpleProxyConnectData *connect_data;
	PurpleNetworkListenData *listen_data;
	guint connect_timeout;

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
	PurpleProxyInfo *gpi;

	char *rxqueue;
	size_t rxlen;
	gsize rxmaxlen;
} JabberSIXfer;

static PurpleXfer*
jabber_si_xfer_find(JabberStream *js, const char *sid, const char *from)
{
	GList *xfers;

	if(!sid || !from)
		return NULL;

	for(xfers = js->file_transfers; xfers; xfers = xfers->next) {
		PurpleXfer *xfer = xfers->data;
		JabberSIXfer *jsx = xfer->data;
		if(jsx->stream_id && xfer->who &&
				!strcmp(jsx->stream_id, sid) && !strcmp(xfer->who, from))
			return xfer;
	}

	return NULL;
}


static void jabber_si_bytestreams_attempt_connect(PurpleXfer *xfer);

static void
jabber_si_bytestreams_connect_cb(gpointer data, gint source, const gchar *error_message)
{
	PurpleXfer *xfer = data;
	JabberSIXfer *jsx = xfer->data;
	JabberIq *iq;
	xmlnode *query, *su;
	JabberBytestreamsStreamhost *streamhost = jsx->streamhosts->data;

	purple_proxy_info_destroy(jsx->gpi);
	jsx->gpi = NULL;
	jsx->connect_data = NULL;

	if (jsx->connect_timeout > 0)
		purple_timeout_remove(jsx->connect_timeout);
	jsx->connect_timeout = 0;

	if(source < 0) {
		purple_debug_warning("jabber",
				"si connection failed, jid was %s, host was %s, error was %s\n",
				streamhost->jid, streamhost->host,
				error_message ? error_message : "(null)");
		jsx->streamhosts = g_list_remove(jsx->streamhosts, streamhost);
		g_free(streamhost->jid);
		g_free(streamhost->host);
		g_free(streamhost->zeroconf);
		g_free(streamhost);
		jabber_si_bytestreams_attempt_connect(xfer);
		return;
	}

	/* unknown file transfer type is assumed to be RECEIVE */
	if(xfer->type == PURPLE_XFER_SEND)
	{
		xmlnode *activate;
		iq = jabber_iq_new_query(jsx->js, JABBER_IQ_SET, "http://jabber.org/protocol/bytestreams");
		xmlnode_set_attrib(iq->node, "to", streamhost->jid);
		query = xmlnode_get_child(iq->node, "query");
		xmlnode_set_attrib(query, "sid", jsx->stream_id);
		activate = xmlnode_new_child(query, "activate");
		xmlnode_insert_data(activate, xfer->who, -1);

		/* TODO: We need to wait for an activation result before starting */
	}
	else
	{
		iq = jabber_iq_new_query(jsx->js, JABBER_IQ_RESULT, "http://jabber.org/protocol/bytestreams");
		xmlnode_set_attrib(iq->node, "to", xfer->who);
		jabber_iq_set_id(iq, jsx->iq_id);
		query = xmlnode_get_child(iq->node, "query");
		su = xmlnode_new_child(query, "streamhost-used");
		xmlnode_set_attrib(su, "jid", streamhost->jid);
	}

	jabber_iq_send(iq);

	purple_xfer_start(xfer, source, NULL, -1);
}

static gboolean
connect_timeout_cb(gpointer data)
{
	PurpleXfer *xfer = data;
	JabberSIXfer *jsx = xfer->data;

	purple_debug_info("jabber", "Streamhost connection timeout of %d seconds exceeded.\n", STREAMHOST_CONNECT_TIMEOUT);

	jsx->connect_timeout = 0;

	if (jsx->connect_data != NULL)
		purple_proxy_connect_cancel(jsx->connect_data);
	jsx->connect_data = NULL;

	/* Trigger the connect error manually */
	jabber_si_bytestreams_connect_cb(xfer, -1, "Timeout Exceeded.");

	return FALSE;
}

static void jabber_si_bytestreams_attempt_connect(PurpleXfer *xfer)
{
	JabberSIXfer *jsx = xfer->data;
	JabberBytestreamsStreamhost *streamhost;
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

		purple_xfer_cancel_local(xfer);

		return;
	}

	streamhost = jsx->streamhosts->data;

	jsx->connect_data = NULL;
	if (jsx->gpi != NULL)
		purple_proxy_info_destroy(jsx->gpi);
	jsx->gpi = NULL;

	dstjid = jabber_id_new(xfer->who);

	/* TODO: Deal with zeroconf */

	if(dstjid != NULL && streamhost->host && streamhost->port > 0) {
		jsx->gpi = purple_proxy_info_new();
		purple_proxy_info_set_type(jsx->gpi, PURPLE_PROXY_SOCKS5);
		purple_proxy_info_set_host(jsx->gpi, streamhost->host);
		purple_proxy_info_set_port(jsx->gpi, streamhost->port);

		/* unknown file transfer type is assumed to be RECEIVE */
		if(xfer->type == PURPLE_XFER_SEND)
			dstaddr = g_strdup_printf("%s%s@%s/%s%s@%s/%s", jsx->stream_id, jsx->js->user->node, jsx->js->user->domain,
				jsx->js->user->resource, dstjid->node, dstjid->domain, dstjid->resource);
		else
			dstaddr = g_strdup_printf("%s%s@%s/%s%s@%s/%s", jsx->stream_id, dstjid->node, dstjid->domain, dstjid->resource,
				jsx->js->user->node, jsx->js->user->domain, jsx->js->user->resource);

		purple_cipher_digest_region("sha1", (guchar *)dstaddr, strlen(dstaddr),
				sizeof(hashval), hashval, NULL);
		g_free(dstaddr);
		dstaddr = g_malloc(41);
		p = dstaddr;
		for(i=0; i<20; i++, p+=2)
			snprintf(p, 3, "%02x", hashval[i]);

		jsx->connect_data = purple_proxy_connect_socks5(NULL, jsx->gpi,
				dstaddr, 0,
				jabber_si_bytestreams_connect_cb, xfer);
		g_free(dstaddr);

		/* When selecting a streamhost, timeout after STREAMHOST_CONNECT_TIMEOUT seconds, otherwise it takes forever */
		if (xfer->type != PURPLE_XFER_SEND && jsx->connect_data != NULL)
			jsx->connect_timeout = purple_timeout_add_seconds(
				STREAMHOST_CONNECT_TIMEOUT, connect_timeout_cb, xfer);

		jabber_id_free(dstjid);
	}

	if (jsx->connect_data == NULL)
	{
		jsx->streamhosts = g_list_remove(jsx->streamhosts, streamhost);
		g_free(streamhost->jid);
		g_free(streamhost->host);
		g_free(streamhost->zeroconf);
		g_free(streamhost);
		jabber_si_bytestreams_attempt_connect(xfer);
	}
}

void jabber_bytestreams_parse(JabberStream *js, xmlnode *packet)
{
	PurpleXfer *xfer;
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
		const char *jid, *host = NULL, *port, *zeroconf;
		int portnum = 0;

		if((jid = xmlnode_get_attrib(streamhost, "jid")) &&
				((zeroconf = xmlnode_get_attrib(streamhost, "zeroconf")) ||
				((host = xmlnode_get_attrib(streamhost, "host")) &&
				(port = xmlnode_get_attrib(streamhost, "port")) &&
				(portnum = atoi(port))))) {
			JabberBytestreamsStreamhost *sh = g_new0(JabberBytestreamsStreamhost, 1);
			sh->jid = g_strdup(jid);
			sh->host = g_strdup(host);
			sh->port = portnum;
			sh->zeroconf = g_strdup(zeroconf);
			jsx->streamhosts = g_list_append(jsx->streamhosts, sh);
		}
	}

	jabber_si_bytestreams_attempt_connect(xfer);
}


static void
jabber_si_xfer_bytestreams_send_read_again_resp_cb(gpointer data, gint source,
		PurpleInputCondition cond)
{
	PurpleXfer *xfer = data;
	JabberSIXfer *jsx = xfer->data;
	int len;

	len = write(source, jsx->rxqueue + jsx->rxlen, jsx->rxmaxlen - jsx->rxlen);
	if (len < 0 && errno == EAGAIN)
		return;
	else if (len < 0) {
		purple_input_remove(xfer->watcher);
		xfer->watcher = 0;
		g_free(jsx->rxqueue);
		jsx->rxqueue = NULL;
		close(source);
		purple_xfer_cancel_remote(xfer);
		return;
	}
	jsx->rxlen += len;

	if (jsx->rxlen < jsx->rxmaxlen)
		return;

	purple_input_remove(xfer->watcher);
	xfer->watcher = 0;
	g_free(jsx->rxqueue);
	jsx->rxqueue = NULL;

	purple_xfer_start(xfer, source, NULL, -1);
}

static void
jabber_si_xfer_bytestreams_send_read_again_cb(gpointer data, gint source,
		PurpleInputCondition cond)
{
	PurpleXfer *xfer = data;
	JabberSIXfer *jsx = xfer->data;
	int i;
	char buffer[256];
	int len;
	char *dstaddr, *p;
	unsigned char hashval[20];
	const char *host;

	purple_debug_info("jabber", "in jabber_si_xfer_bytestreams_send_read_again_cb\n");

	if(jsx->rxlen < 5) {
		purple_debug_info("jabber", "reading the first 5 bytes\n");
		len = read(source, buffer, 5 - jsx->rxlen);
		if(len < 0 && errno == EAGAIN)
			return;
		else if(len <= 0) {
			purple_input_remove(xfer->watcher);
			xfer->watcher = 0;
			close(source);
			purple_xfer_cancel_remote(xfer);
			return;
		}
		jsx->rxqueue = g_realloc(jsx->rxqueue, len + jsx->rxlen);
		memcpy(jsx->rxqueue + jsx->rxlen, buffer, len);
		jsx->rxlen += len;
		return;
	} else if(jsx->rxqueue[0] != 0x05 || jsx->rxqueue[1] != 0x01 ||
			jsx->rxqueue[3] != 0x03) {
		purple_debug_info("jabber", "invalid socks5 stuff\n");
		purple_input_remove(xfer->watcher);
		xfer->watcher = 0;
		close(source);
		purple_xfer_cancel_remote(xfer);
		return;
	} else if(jsx->rxlen - 5 <  jsx->rxqueue[4] + 2) {
		purple_debug_info("jabber", "reading umpteen more bytes\n");
		len = read(source, buffer, jsx->rxqueue[4] + 5 + 2 - jsx->rxlen);
		if(len < 0 && errno == EAGAIN)
			return;
		else if(len <= 0) {
			purple_input_remove(xfer->watcher);
			xfer->watcher = 0;
			close(source);
			purple_xfer_cancel_remote(xfer);
			return;
		}
		jsx->rxqueue = g_realloc(jsx->rxqueue, len + jsx->rxlen);
		memcpy(jsx->rxqueue + jsx->rxlen, buffer, len);
		jsx->rxlen += len;
	}

	if(jsx->rxlen - 5 < jsx->rxqueue[4] + 2)
		return;

	purple_input_remove(xfer->watcher);
	xfer->watcher = 0;

	dstaddr = g_strdup_printf("%s%s@%s/%s%s", jsx->stream_id,
			jsx->js->user->node, jsx->js->user->domain,
			jsx->js->user->resource, xfer->who);

	purple_cipher_digest_region("sha1", (guchar *)dstaddr, strlen(dstaddr),
				    sizeof(hashval), hashval, NULL);
	g_free(dstaddr);
	dstaddr = g_malloc(41);
	p = dstaddr;
	for(i=0; i<20; i++, p+=2)
		snprintf(p, 3, "%02x", hashval[i]);

	if(jsx->rxqueue[4] != 40 || strncmp(dstaddr, jsx->rxqueue+5, 40) ||
			jsx->rxqueue[45] != 0x00 || jsx->rxqueue[46] != 0x00) {
		purple_debug_error("jabber", "someone connected with the wrong info!\n");
		close(source);
		purple_xfer_cancel_remote(xfer);
		g_free(dstaddr);
		return;
	}

	g_free(dstaddr);

	g_free(jsx->rxqueue);
	host = purple_network_get_my_ip(jsx->js->fd);

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

	xfer->watcher = purple_input_add(source, PURPLE_INPUT_WRITE,
		jabber_si_xfer_bytestreams_send_read_again_resp_cb, xfer);
	jabber_si_xfer_bytestreams_send_read_again_resp_cb(xfer, source,
		PURPLE_INPUT_WRITE);
}

static void
jabber_si_xfer_bytestreams_send_read_response_cb(gpointer data, gint source,
		PurpleInputCondition cond)
{
	PurpleXfer *xfer = data;
	JabberSIXfer *jsx = xfer->data;
	int len;

	len = write(source, jsx->rxqueue + jsx->rxlen, jsx->rxmaxlen - jsx->rxlen);
	if (len < 0 && errno == EAGAIN)
		return;
	else if (len < 0) {
		purple_input_remove(xfer->watcher);
		xfer->watcher = 0;
		g_free(jsx->rxqueue);
		jsx->rxqueue = NULL;
		close(source);
		purple_xfer_cancel_remote(xfer);
		return;
	}
	jsx->rxlen += len;

	if (jsx->rxlen < jsx->rxmaxlen)
		return;

	purple_input_remove(xfer->watcher);
	xfer->watcher = 0;

	if (jsx->rxqueue[1] == 0x00) {
		xfer->watcher = purple_input_add(source, PURPLE_INPUT_READ,
			jabber_si_xfer_bytestreams_send_read_again_cb, xfer);
		g_free(jsx->rxqueue);
		jsx->rxqueue = NULL;
	} else {
		close(source);
		purple_xfer_cancel_remote(xfer);
	}
}

static void
jabber_si_xfer_bytestreams_send_read_cb(gpointer data, gint source,
		PurpleInputCondition cond)
{
	PurpleXfer *xfer = data;
	JabberSIXfer *jsx = xfer->data;
	int i;
	int len;
	char buffer[256];

	purple_debug_info("jabber", "in jabber_si_xfer_bytestreams_send_read_cb\n");

	xfer->fd = source;

	if(jsx->rxlen < 2) {
		purple_debug_info("jabber", "reading those first two bytes\n");
		len = read(source, buffer, 2 - jsx->rxlen);
		if(len < 0 && errno == EAGAIN)
			return;
		else if(len <= 0) {
			purple_input_remove(xfer->watcher);
			xfer->watcher = 0;
			close(source);
			purple_xfer_cancel_remote(xfer);
			return;
		}
		jsx->rxqueue = g_realloc(jsx->rxqueue, len + jsx->rxlen);
		memcpy(jsx->rxqueue + jsx->rxlen, buffer, len);
		jsx->rxlen += len;
		return;
	} else if(jsx->rxlen - 2 <  jsx->rxqueue[1]) {
		purple_debug_info("jabber", "reading the next umpteen bytes\n");
		len = read(source, buffer, jsx->rxqueue[1] + 2 - jsx->rxlen);
		if(len < 0 && errno == EAGAIN)
			return;
		else if(len <= 0) {
			purple_input_remove(xfer->watcher);
			xfer->watcher = 0;
			close(source);
			purple_xfer_cancel_remote(xfer);
			return;
		}
		jsx->rxqueue = g_realloc(jsx->rxqueue, len + jsx->rxlen);
		memcpy(jsx->rxqueue + jsx->rxlen, buffer, len);
		jsx->rxlen += len;
	}

	if(jsx->rxlen -2 < jsx->rxqueue[1])
		return;

	purple_input_remove(xfer->watcher);
	xfer->watcher = 0;

	purple_debug_info("jabber", "checking to make sure we're socks FIVE\n");

	if(jsx->rxqueue[0] != 0x05) {
		close(source);
		purple_xfer_cancel_remote(xfer);
		return;
	}

	purple_debug_info("jabber", "going to test %hhu different methods\n", jsx->rxqueue[1]);

	for(i=0; i<jsx->rxqueue[1]; i++) {

		purple_debug_info("jabber", "testing %hhu\n", jsx->rxqueue[i+2]);
		if(jsx->rxqueue[i+2] == 0x00) {
			g_free(jsx->rxqueue);
			jsx->rxlen = 0;
			jsx->rxmaxlen = 2;
			jsx->rxqueue = g_malloc(jsx->rxmaxlen);
			jsx->rxqueue[0] = 0x05;
			jsx->rxqueue[1] = 0x00;
			xfer->watcher = purple_input_add(source, PURPLE_INPUT_WRITE,
				jabber_si_xfer_bytestreams_send_read_response_cb,
				xfer);
			jabber_si_xfer_bytestreams_send_read_response_cb(xfer,
				source, PURPLE_INPUT_WRITE);
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
	xfer->watcher = purple_input_add(source, PURPLE_INPUT_WRITE,
		jabber_si_xfer_bytestreams_send_read_response_cb, xfer);
	jabber_si_xfer_bytestreams_send_read_response_cb(xfer,
		source, PURPLE_INPUT_WRITE);
}

static gint
jabber_si_compare_jid(gconstpointer a, gconstpointer b)
{
	const JabberBytestreamsStreamhost *sh = a;

	if(!a)
		return -1;

	return strcmp(sh->jid, (char *)b);
}


static void
jabber_si_free_streamhost(gpointer data, gpointer user_data)
{
	JabberBytestreamsStreamhost *sh = data;

	if(!data)
		return;

	g_free(sh->jid);
	g_free(sh->host);
	g_free(sh->zeroconf);
	g_free(sh);
}

static void
jabber_si_xfer_bytestreams_send_connected_cb(gpointer data, gint source,
		PurpleInputCondition cond)
{
	PurpleXfer *xfer = data;
	int acceptfd;

	purple_debug_info("jabber", "in jabber_si_xfer_bytestreams_send_connected_cb\n");

	acceptfd = accept(source, NULL, 0);
	if(acceptfd == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
		return;
	else if(acceptfd == -1) {
		purple_debug_warning("jabber", "accept: %s\n", g_strerror(errno));
		/* TODO: This should cancel the ft */
		return;
	}

	purple_input_remove(xfer->watcher);
	close(source);

	xfer->watcher = purple_input_add(acceptfd, PURPLE_INPUT_READ,
					 jabber_si_xfer_bytestreams_send_read_cb, xfer);
}

static void
jabber_si_connect_proxy_cb(JabberStream *js, xmlnode *packet,
		gpointer data)
{
	PurpleXfer *xfer = data;
	JabberSIXfer *jsx = xfer->data;
	xmlnode *query, *streamhost_used;
	const char *from, *type, *jid;
	GList *matched;

	/* TODO: This need to send errors if we don't see what we're looking for */

	/* In the case of a direct file transfer, this is expected to return */
	if(!jsx)
		return;

	if(!(type = xmlnode_get_attrib(packet, "type")) || strcmp(type, "result"))
		return;

	if(!(from = xmlnode_get_attrib(packet, "from")))
		return;

	if(!(query = xmlnode_get_child(packet, "query")))
		return;

	if(!(streamhost_used = xmlnode_get_child(query, "streamhost-used")))
		return;

	if(!(jid = xmlnode_get_attrib(streamhost_used, "jid")))
		return;

	if(!(matched = g_list_find_custom(jsx->streamhosts, jid, jabber_si_compare_jid)))
	{
		gchar *my_jid = g_strdup_printf("%s@%s/%s", jsx->js->user->node,
			jsx->js->user->domain, jsx->js->user->resource);
		if (!strcmp(jid, my_jid))
			purple_debug_info("jabber", "Got local SOCKS5 streamhost-used.\n");
		else
			purple_debug_info("jabber", "streamhost-used does not match any proxy that was offered to target\n");
		g_free(my_jid);
		return;
	}

	/* TODO: Clean up the local SOCKS5 proxy - it isn't going to be used.*/

	jsx->streamhosts = g_list_remove_link(jsx->streamhosts, matched);
	g_list_foreach(jsx->streamhosts, jabber_si_free_streamhost, NULL);
	g_list_free(jsx->streamhosts);

	jsx->streamhosts = matched;

	jabber_si_bytestreams_attempt_connect(xfer);
}

static void
jabber_si_xfer_bytestreams_listen_cb(int sock, gpointer data)
{
	PurpleXfer *xfer = data;
	JabberSIXfer *jsx;
	JabberIq *iq;
	xmlnode *query, *streamhost;
	char *jid, port[6];
	const char *local_ip, *public_ip, *ft_proxies;
	GList *tmp;
	JabberBytestreamsStreamhost *sh, *sh2;

	jsx = xfer->data;
	jsx->listen_data = NULL;

	if (purple_xfer_get_status(xfer) == PURPLE_XFER_STATUS_CANCEL_LOCAL) {
		purple_xfer_unref(xfer);
		return;
	}

	purple_xfer_unref(xfer);

	if (sock < 0) {
		purple_xfer_cancel_local(xfer);
		return;
	}

	iq = jabber_iq_new_query(jsx->js, JABBER_IQ_SET,
			"http://jabber.org/protocol/bytestreams");
	xmlnode_set_attrib(iq->node, "to", xfer->who);
	query = xmlnode_get_child(iq->node, "query");

	xmlnode_set_attrib(query, "sid", jsx->stream_id);

	jid = g_strdup_printf("%s@%s/%s", jsx->js->user->node,
			jsx->js->user->domain, jsx->js->user->resource);
	xfer->local_port = purple_network_get_port_from_fd(sock);
	g_snprintf(port, sizeof(port), "%hu", xfer->local_port);

	/* TODO: Should there be an option to not use the local host as a ft proxy?
	 *       (to prevent revealing IP address, etc.) */

	/* Include the localhost's IP (for in-network transfers) */
	local_ip = purple_network_get_local_system_ip(jsx->js->fd);
	if (strcmp(local_ip, "0.0.0.0") != 0)
	{
		streamhost = xmlnode_new_child(query, "streamhost");
		xmlnode_set_attrib(streamhost, "jid", jid);
		xmlnode_set_attrib(streamhost, "host", local_ip);
		xmlnode_set_attrib(streamhost, "port", port);
	}

	/* Include the public IP (assuming that there is a port mapped somehow) */
	/* TODO: Check that it isn't the same as above and is a valid IP */
	public_ip = purple_network_get_my_ip(jsx->js->fd);
	if (strcmp(public_ip, local_ip) != 0)
	{
		streamhost = xmlnode_new_child(query, "streamhost");
		xmlnode_set_attrib(streamhost, "jid", jid);
		xmlnode_set_attrib(streamhost, "host", public_ip);
		xmlnode_set_attrib(streamhost, "port", port);
	}

	g_free(jid);

	/* The listener for the local proxy */
	xfer->watcher = purple_input_add(sock, PURPLE_INPUT_READ,
			jabber_si_xfer_bytestreams_send_connected_cb, xfer);

	/* insert proxies here */
	ft_proxies = purple_account_get_string(xfer->account, "ft_proxies", NULL);
	if (ft_proxies) {
		int i, portnum;
		char *tmp;
		gchar **ft_proxy_list = g_strsplit(ft_proxies, ",", 0);

		g_list_foreach(jsx->streamhosts, jabber_si_free_streamhost, NULL);
		g_list_free(jsx->streamhosts);
		jsx->streamhosts = NULL;

		for(i = 0; ft_proxy_list[i]; i++) {
			g_strstrip(ft_proxy_list[i]);
			if(!(*ft_proxy_list[i]))
				continue;

			if((tmp = strchr(ft_proxy_list[i], ':'))) {
				portnum = atoi(tmp + 1);
				*tmp = '\0';
			} else
				portnum = 7777;

			g_snprintf(port, sizeof(port), "%hu", portnum);

			if(g_list_find_custom(jsx->streamhosts, ft_proxy_list[i], jabber_si_compare_jid) != NULL)
				continue;

			streamhost = xmlnode_new_child(query, "streamhost");
			xmlnode_set_attrib(streamhost, "jid", ft_proxy_list[i]);
			xmlnode_set_attrib(streamhost, "host", ft_proxy_list[i]);
			xmlnode_set_attrib(streamhost, "port", port);

			sh = g_new0(JabberBytestreamsStreamhost, 1);
			sh->jid = g_strdup(ft_proxy_list[i]);
			sh->host = g_strdup(ft_proxy_list[i]);
			sh->port = portnum;

			jsx->streamhosts = g_list_prepend(jsx->streamhosts, sh);
		}

		g_strfreev(ft_proxy_list);
	}

	for (tmp = jsx->js->bs_proxies; tmp; tmp = tmp->next) {
		sh = tmp->data;

		/* TODO: deal with zeroconf proxies */

		if (!(sh->jid && sh->host && sh->port > 0))
			continue;

		if(g_list_find_custom(jsx->streamhosts, sh->jid, jabber_si_compare_jid) != NULL)
			continue;

		streamhost = xmlnode_new_child(query, "streamhost");
		xmlnode_set_attrib(streamhost, "jid", sh->jid);
		xmlnode_set_attrib(streamhost, "host", sh->host);
		g_snprintf(port, sizeof(port), "%hu", sh->port);
		xmlnode_set_attrib(streamhost, "port", port);

		sh2 = g_new0(JabberBytestreamsStreamhost, 1);
		sh2->jid = g_strdup(sh->jid);
		sh2->host = g_strdup(sh->host);
		sh2->zeroconf = g_strdup(sh->zeroconf);
		sh2->port = sh->port;

		jsx->streamhosts = g_list_prepend(jsx->streamhosts, sh2);
	}

	jabber_iq_set_callback(iq, jabber_si_connect_proxy_cb, xfer);

	jabber_iq_send(iq);

}

static void
jabber_si_xfer_bytestreams_send_init(PurpleXfer *xfer)
{
	JabberSIXfer *jsx;

	purple_xfer_ref(xfer);

	jsx = xfer->data;
	jsx->listen_data = purple_network_listen_range(0, 0, SOCK_STREAM,
				jabber_si_xfer_bytestreams_listen_cb, xfer);
	if (jsx->listen_data == NULL) {
		purple_xfer_unref(xfer);
		/* XXX: couldn't open a port, we're fscked */
		purple_xfer_cancel_local(xfer);
		return;
	}

}

static void jabber_si_xfer_send_method_cb(JabberStream *js, xmlnode *packet,
		gpointer data)
{
	PurpleXfer *xfer = data;
	xmlnode *si, *feature, *x, *field, *value;

	if(!(si = xmlnode_get_child_with_namespace(packet, "si", "http://jabber.org/protocol/si"))) {
		purple_xfer_cancel_remote(xfer);
		return;
	}

	if(!(feature = xmlnode_get_child_with_namespace(si, "feature", "http://jabber.org/protocol/feature-neg"))) {
		purple_xfer_cancel_remote(xfer);
		return;
	}

	if(!(x = xmlnode_get_child_with_namespace(feature, "x", "jabber:x:data"))) {
		purple_xfer_cancel_remote(xfer);
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
	purple_xfer_cancel_remote(xfer);
}

static void jabber_si_xfer_send_request(PurpleXfer *xfer)
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
	xmlnode_set_namespace(feature, "http://jabber.org/protocol/feature-neg");
	x = xmlnode_new_child(feature, "x");
	xmlnode_set_namespace(x, "jabber:x:data");
	xmlnode_set_attrib(x, "type", "form");
	field = xmlnode_new_child(x, "field");
	xmlnode_set_attrib(field, "var", "stream-method");
	xmlnode_set_attrib(field, "type", "list-single");
	option = xmlnode_new_child(field, "option");
	value = xmlnode_new_child(option, "value");
	xmlnode_insert_data(value, "http://jabber.org/protocol/bytestreams", -1);
	/*
	option = xmlnode_new_child(field, "option");
	value = xmlnode_new_child(option, "value");
	xmlnode_insert_data(value, "http://jabber.org/protocol/ibb", -1);
	*/

	jabber_iq_set_callback(iq, jabber_si_xfer_send_method_cb, xfer);

	/* Store the IQ id so that we can cancel the callback */
	g_free(jsx->iq_id);
	jsx->iq_id = g_strdup(iq->id);

	jabber_iq_send(iq);
}

static void jabber_si_xfer_free(PurpleXfer *xfer)
{
	JabberSIXfer *jsx = xfer->data;
	JabberStream *js = jsx->js;

	js->file_transfers = g_list_remove(js->file_transfers, xfer);

	if (jsx->connect_data != NULL)
		purple_proxy_connect_cancel(jsx->connect_data);
	if (jsx->listen_data != NULL)
		purple_network_listen_cancel(jsx->listen_data);
	if (jsx->iq_id != NULL)
		jabber_iq_remove_callback_by_id(js, jsx->iq_id);

	if (jsx->connect_timeout > 0)
		purple_timeout_remove(jsx->connect_timeout);

	if (jsx->streamhosts) {
		g_list_foreach(jsx->streamhosts, jabber_si_free_streamhost, NULL);
		g_list_free(jsx->streamhosts);
	}

	g_free(jsx->stream_id);
	g_free(jsx->iq_id);
	/* XXX: free other stuff */
	g_free(jsx->rxqueue);
	g_free(jsx);
	xfer->data = NULL;
}

static void jabber_si_xfer_cancel_send(PurpleXfer *xfer)
{
	jabber_si_xfer_free(xfer);
	purple_debug(PURPLE_DEBUG_INFO, "jabber", "in jabber_si_xfer_cancel_send\n");
}


static void jabber_si_xfer_request_denied(PurpleXfer *xfer)
{
	jabber_si_xfer_free(xfer);
	purple_debug(PURPLE_DEBUG_INFO, "jabber", "in jabber_si_xfer_request_denied\n");
}


static void jabber_si_xfer_cancel_recv(PurpleXfer *xfer)
{
	jabber_si_xfer_free(xfer);
	purple_debug(PURPLE_DEBUG_INFO, "jabber", "in jabber_si_xfer_cancel_recv\n");
}


static void jabber_si_xfer_end(PurpleXfer *xfer)
{
	jabber_si_xfer_free(xfer);
}


static void jabber_si_xfer_send_disco_cb(JabberStream *js, const char *who,
		JabberCapabilities capabilities, gpointer data)
{
	PurpleXfer *xfer = data;

	if(capabilities & JABBER_CAP_SI_FILE_XFER) {
		jabber_si_xfer_send_request(xfer);
	} else {
		char *msg = g_strdup_printf(_("Unable to send file to %s, user does not support file transfers"), who);
		purple_notify_error(js->gc, _("File Send Failed"),
				_("File Send Failed"), msg);
		g_free(msg);
	}
}

static void resource_select_cancel_cb(PurpleXfer *xfer, PurpleRequestFields *fields)
{
	purple_xfer_cancel_local(xfer);
}

static void do_transfer_send(PurpleXfer *xfer, const char *resource)
{
	JabberSIXfer *jsx = xfer->data;
	char **who_v = g_strsplit(xfer->who, "/", 2);
	char *who;

	who = g_strdup_printf("%s/%s", who_v[0], resource);
	g_strfreev(who_v);
	g_free(xfer->who);
	xfer->who = who;
	jabber_disco_info_do(jsx->js, who,
			jabber_si_xfer_send_disco_cb, xfer);
}

static void resource_select_ok_cb(PurpleXfer *xfer, PurpleRequestFields *fields)
{
	PurpleRequestField *field = purple_request_fields_get_field(fields, "resource");
	int selected_id = purple_request_field_choice_get_value(field);
	GList *labels = purple_request_field_choice_get_labels(field);

	const char *selected_label = g_list_nth_data(labels, selected_id);

	do_transfer_send(xfer, selected_label);
}

static void jabber_si_xfer_init(PurpleXfer *xfer)
{
	JabberSIXfer *jsx = xfer->data;
	JabberIq *iq;
	if(purple_xfer_get_type(xfer) == PURPLE_XFER_SEND) {
		JabberBuddy *jb;
		JabberBuddyResource *jbr = NULL;
		char *resource;

		if(NULL != (resource = jabber_get_resource(xfer->who))) {
			/* they've specified a resource, no need to ask or
			 * default or anything, just do it */

			do_transfer_send(xfer, resource);
			g_free(resource);
			return;
		}

		jb = jabber_buddy_find(jsx->js, xfer->who, TRUE);

		if(!jb || !jb->resources) {
			/* no resources online, we're trying to send to someone
			 * whose presence we're not subscribed to, or
			 * someone who is offline.  Let's inform the user */
			char *msg;

			if(!jb) {
				msg = g_strdup_printf(_("Unable to send file to %s, invalid JID"), xfer->who);
			} else if(jb->subscription & JABBER_SUB_TO) {
				msg = g_strdup_printf(_("Unable to send file to %s, user is not online"), xfer->who);
			} else {
				msg = g_strdup_printf(_("Unable to send file to %s, not subscribed to user presence"), xfer->who);
			}

			purple_notify_error(jsx->js->gc, _("File Send Failed"), _("File Send Failed"), msg);
			g_free(msg);
		} else if(g_list_length(jb->resources) == 1) {
			/* only 1 resource online (probably our most common case)
			 * so no need to ask who to send to */
			jbr = jb->resources->data;

			do_transfer_send(xfer, jbr->name);

		} else {
			/* we've got multiple resources, we need to pick one to send to */
			GList *l;
			char *msg = g_strdup_printf(_("Please select the resource of %s to which you would like to send a file"), xfer->who);
			PurpleRequestFields *fields = purple_request_fields_new();
			PurpleRequestField *field = purple_request_field_choice_new("resource", _("Resource"), 0);
			PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);

			for(l = jb->resources; l; l = l->next)
			{
				jbr = l->data;

				purple_request_field_choice_add(field, jbr->name);
			}

			purple_request_field_group_add_field(group, field);

			purple_request_fields_add_group(fields, group);

			purple_request_fields(jsx->js->gc, _("Select a Resource"), msg, NULL, fields,
					_("Send File"), G_CALLBACK(resource_select_ok_cb), _("Cancel"), G_CALLBACK(resource_select_cancel_cb),
					jsx->js->gc->account, xfer->who, NULL, xfer);

			g_free(msg);
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

PurpleXfer *jabber_si_new_xfer(PurpleConnection *gc, const char *who)
{
	JabberStream *js;

	PurpleXfer *xfer;
	JabberSIXfer *jsx;

	js = gc->proto_data;

	xfer = purple_xfer_new(gc->account, PURPLE_XFER_SEND, who);
	if (xfer)
	{
		xfer->data = jsx = g_new0(JabberSIXfer, 1);
		jsx->js = js;

		purple_xfer_set_init_fnc(xfer, jabber_si_xfer_init);
		purple_xfer_set_cancel_send_fnc(xfer, jabber_si_xfer_cancel_send);
		purple_xfer_set_end_fnc(xfer, jabber_si_xfer_end);

		js->file_transfers = g_list_append(js->file_transfers, xfer);
	}

	return xfer;
}

void jabber_si_xfer_send(PurpleConnection *gc, const char *who, const char *file)
{
	JabberStream *js;

	PurpleXfer *xfer;

	js = gc->proto_data;

	if(!purple_find_buddy(gc->account, who) || !jabber_buddy_find(js, who, FALSE))
		return;

	xfer = jabber_si_new_xfer(gc, who);

	if (file)
		purple_xfer_request_accepted(xfer, file);
	else
		purple_xfer_request(xfer);
}

void jabber_si_parse(JabberStream *js, xmlnode *packet)
{
	JabberSIXfer *jsx;
	PurpleXfer *xfer;
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

	xfer = purple_xfer_new(js->gc->account, PURPLE_XFER_RECEIVE, from);
	if (xfer)
	{
		xfer->data = jsx;

		purple_xfer_set_filename(xfer, filename);
		if(filesize > 0)
			purple_xfer_set_size(xfer, filesize);

		purple_xfer_set_init_fnc(xfer, jabber_si_xfer_init);
		purple_xfer_set_request_denied_fnc(xfer, jabber_si_xfer_request_denied);
		purple_xfer_set_cancel_recv_fnc(xfer, jabber_si_xfer_cancel_recv);
		purple_xfer_set_end_fnc(xfer, jabber_si_xfer_end);

		js->file_transfers = g_list_append(js->file_transfers, xfer);

		purple_xfer_request(xfer);
	}
}


