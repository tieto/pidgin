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
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include "internal.h"
#include "debug.h"
#include "ft.h"
#include "network.h"
#include "notify.h"
#include "sha.h"
#include "util.h"

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
		if(!strcmp(jsx->stream_id, sid) && !strcmp(xfer->who, from))
			return xfer;
	}

	return NULL;
}


static void jabber_si_bytestreams_attempt_connect(GaimXfer *xfer);

static void jabber_si_bytestreams_connect_cb(gpointer data, gint source, GaimInputCondition cond)
{
	GaimXfer *xfer = data;
	JabberSIXfer *jsx = xfer->data;
	JabberIq *iq;
	xmlnode *query, *su;
	struct bytestreams_streamhost *streamhost = jsx->streamhosts->data;

	gaim_proxy_info_destroy(jsx->gpi);

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

	if(!jsx->streamhosts) {
		JabberIq *iq = jabber_iq_new(jsx->js, JABBER_IQ_ERROR);
		xmlnode *error, *condition;

		if(jsx->iq_id)
			jabber_iq_set_id(iq, jsx->iq_id);

		xmlnode_set_attrib(iq->node, "to", xfer->who);
		error = xmlnode_new_child(iq->node, "error");
		xmlnode_set_attrib(error, "code", "404");
		xmlnode_set_attrib(error, "type", "cancel");
		condition = xmlnode_new_child(error, "condition");
		xmlnode_set_attrib(condition, "xmlns", "urn:ietf:params:xml:ns:xmpp-stanzas");
		xmlnode_new_child(condition, "item-not-found");

		jabber_iq_send(iq);

		gaim_xfer_cancel_local(xfer);

		return;
	}

	streamhost = jsx->streamhosts->data;

	jsx->gpi = gaim_proxy_info_new();
	gaim_proxy_info_set_type(jsx->gpi, GAIM_PROXY_SOCKS5);
	gaim_proxy_info_set_host(jsx->gpi, streamhost->host);
	gaim_proxy_info_set_port(jsx->gpi, streamhost->port);

	dstaddr = g_strdup_printf("%s%s%s@%s/%s", jsx->stream_id, xfer->who, jsx->js->user->node,
			jsx->js->user->domain, jsx->js->user->resource);
	shaBlock((unsigned char *)dstaddr, strlen(dstaddr), hashval);
	g_free(dstaddr);
	dstaddr = g_malloc(41);
	p = dstaddr;
	for(i=0; i<20; i++, p+=2)
		snprintf(p, 3, "%02x", hashval[i]);

	gaim_proxy_connect_socks5(jsx->gpi, dstaddr, 0, jabber_si_bytestreams_connect_cb, xfer);
	g_free(dstaddr);
}

void jabber_bytestreams_parse(JabberStream *js, xmlnode *packet)
{
	GaimXfer *xfer;
	JabberSIXfer *jsx;
	xmlnode *query, *streamhost;
	const char *sid, *from;

	if(!(from = xmlnode_get_attrib(packet, "from")))
		return;

	if(!(query = xmlnode_get_child(packet, "query")))
		return;

	if(!(sid = xmlnode_get_attrib(query, "sid")))
		return;

	if(!(xfer = jabber_si_xfer_find(js, sid, from)))
		return;

	jsx = xfer->data;
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
jabber_si_xfer_bytestreams_send_connected_cb(gpointer data, gint source,
		GaimInputCondition cond)
{
	GaimXfer *xfer = data;
	char ver, len, method;
	int i;
	char buf[2];

	xfer->fd = source;

	read(source, &ver, 1);

	if(ver != 0x05) {
		close(source);
		gaim_xfer_cancel_remote(xfer);
		return;
	}

	read(source, &len, 1);
	for(i=0; i<len; i++) {
		read(source, &method, 1);
		if(method == 0x00) {
			buf[0] = 0x05;
			buf[1] = 0x00;
			write(source, buf, 2);
			gaim_xfer_start(xfer, source, NULL, 0);
			return;
		}
	}

	buf[0] = 0x05;
	buf[1] = 0xFF;
	write(source, buf, 2);
	gaim_xfer_cancel_remote(xfer);
}

static void
jabber_si_xfer_bytestreams_send_init(GaimXfer *xfer)
{
	JabberSIXfer *jsx = xfer->data;
	JabberIq *iq;
	xmlnode *query, *streamhost;
	char *jid, *port;
	int fd;

	iq = jabber_iq_new_query(jsx->js, JABBER_IQ_SET,
			"http://jabber.org/protocol/bytestreams");
	xmlnode_set_attrib(iq->node, "to", xfer->who);
	query = xmlnode_get_child(iq->node, "query");

	xmlnode_set_attrib(query, "sid", jsx->stream_id);

	streamhost = xmlnode_new_child(query, "streamhost");
	jid = g_strdup_printf("%s@%s/%s", jsx->js->user->node, jsx->js->user->domain, jsx->js->user->resource);
	xmlnode_set_attrib(streamhost, "jid", jid);
	g_free(jid);

	if((fd = gaim_network_listen_range(0, 0)) < 0) {
		/* XXX: couldn't open a port, we're fscked */
		return;
	}

	xmlnode_set_attrib(streamhost, "host",  gaim_network_get_ip_for_account(jsx->js->gc->account, fd));
	xfer->local_port = gaim_network_get_port_from_fd(fd);
	port = g_strdup_printf("%d", xfer->local_port);
	xmlnode_set_attrib(streamhost, "port", port);
	g_free(port);

	xfer->watcher = gaim_input_add(fd, GAIM_INPUT_READ,
			jabber_si_xfer_bytestreams_send_connected_cb, xfer);

	/* XXX: insert proxies here */

	/* XXX: callback to find out which streamhost they used, or see if they
	 * screwed it up */
	jabber_iq_send(iq);
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
	xmlnode_set_attrib(si, "xmlns", "http://jabber.org/protocol/si");
	jsx->stream_id = jabber_get_next_id(jsx->js);
	xmlnode_set_attrib(si, "id", jsx->stream_id);
	xmlnode_set_attrib(si, "profile",
			"http://jabber.org/protocol/si/profile/file-transfer");

	file = xmlnode_new_child(si, "file");
	xmlnode_set_attrib(file, "xmlns",
			"http://jabber.org/protocol/si/profile/file-transfer");
	xmlnode_set_attrib(file, "name", xfer->filename);
	g_snprintf(buf, sizeof(buf), "%d", xfer->size);
	xmlnode_set_attrib(file, "size", buf);
	/* maybe later we'll do hash and date attribs */

	feature = xmlnode_new_child(si, "feature");
	xmlnode_set_attrib(feature, "xmlns",
			"http://jabber.org/protocol/feature-neg");
	x = xmlnode_new_child(feature, "x");
	xmlnode_set_attrib(x, "xmlns", "jabber:x:data");
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

void jabber_si_xfer_cancel_send(GaimXfer *xfer)
{
	gaim_debug(GAIM_DEBUG_INFO, "jabber", "in jabber_si_xfer_cancel_send\n");
}


void jabber_si_xfer_cancel_recv(GaimXfer *xfer)
{
	gaim_debug(GAIM_DEBUG_INFO, "jabber", "in jabber_si_xfer_cancel_recv\n");
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
		if(g_list_length(jb->resources) >= 1) {
			char *who;
			jbr = jb->resources->data;
			who = g_strdup_printf("%s/%s", xfer->who, jbr->name);
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

		si = xmlnode_new_child(iq->node, "si");
		xmlnode_set_attrib(si, "xmlns", "http://jabber.org/protocol/si");

		feature = xmlnode_new_child(si, "feature");
		xmlnode_set_attrib(feature, "xmlns", "http://jabber.org/protocol/feature-neg");

		x = xmlnode_new_child(feature, "x");
		xmlnode_set_attrib(x, "xmlns", "jabber:x:data");
		xmlnode_set_attrib(x, "type", "form");

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

void jabber_si_xfer_ask_send(GaimConnection *gc, const char *name)
{
	JabberStream *js = gc->proto_data;
	GaimXfer *xfer;
	JabberSIXfer *jsx;

	if(!gaim_find_buddy(gc->account, name) || !jabber_buddy_find(js, name, FALSE))
		return;

	xfer = gaim_xfer_new(gaim_connection_get_account(gc), GAIM_XFER_SEND, name);

	xfer->data = jsx = g_new0(JabberSIXfer, 1);
	jsx->js = js;

	gaim_xfer_set_init_fnc(xfer, jabber_si_xfer_init);
	gaim_xfer_set_cancel_send_fnc(xfer, jabber_si_xfer_cancel_send);

	js->file_transfers = g_list_append(js->file_transfers, xfer);

	gaim_xfer_request(xfer);
}

void jabber_si_parse(JabberStream *js, xmlnode *packet)
{
	JabberSIXfer *jsx;
	GaimXfer *xfer;
	xmlnode *si, *file, *feature, *x, *field, *option, *value;
	const char *stream_id, *filename, *filesize_c, *profile;
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

	xfer = gaim_xfer_new(js->gc->account, GAIM_XFER_RECEIVE,
			xmlnode_get_attrib(packet, "from"));
	xfer->data = jsx;

	gaim_xfer_set_filename(xfer, filename);
	if(filesize > 0)
		gaim_xfer_set_size(xfer, filesize);

	gaim_xfer_set_init_fnc(xfer, jabber_si_xfer_init);

	js->file_transfers = g_list_append(js->file_transfers, xfer);

	gaim_xfer_request(xfer);
}


