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

static void jabber_si_xfer_init(GaimXfer *xfer)
{
	JabberSIXfer *jsx = xfer->data;
	JabberIq *iq;
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

static ssize_t jabber_si_xfer_read(char **buffer, GaimXfer *xfer) {
	char buf;

	if(read(xfer->fd, &buf, 1) == 1) {
		if(buf == 0x00)
			gaim_xfer_set_read_fnc(xfer, NULL);
	} else {
		gaim_debug_error("jabber", "Read error on bytestream transfer!\n");
		gaim_xfer_cancel_local(xfer);
	}

	return 0;
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
	gaim_xfer_set_read_fnc(xfer, jabber_si_xfer_read);

	js->file_transfers = g_list_append(js->file_transfers, xfer);

	gaim_xfer_request(xfer);
}

#if 0
void jabber_si_parse(JabberStream *js, xmlnode *packet)
{
	GaimXfer *xfer;
	JabberSIXfer *jsx;
	xmlnode *si, *feature, *x, *field, *value;
	GaimAccount *account = gaim_connection_get_account(js->gc);

	si = xmlnode_get_child(packet, "si");

	xfer = jabber_si_xfer_find_by_id(js, xmlnode_get_attrib(si, "id"));

	if(!xfer)
		return;

	jsx = xfer->data;

	if(!(feature = xmlnode_get_child(si, "feature")))
		return;

	for(x = xmlnode_get_child(feature, "x"); x; x = xmlnode_get_next_twin(x)) {
		const char *xmlns;
		if(!(xmlns = xmlnode_get_attrib(x, "xmlns")))
			continue;
		if(strcmp(xmlns, "jabber:x:data"))
			continue;
		for(field = xmlnode_get_child(x, "field"); field;
				field = xmlnode_get_next_twin(field)) {
			const char *var;
			if(!(var = xmlnode_get_attrib(field, "var")))
				continue;
			if(!strcmp(var, "stream-method")) {
				if((value = xmlnode_get_child(field, "value"))) {
					char *val_data = xmlnode_get_data(value);
					if(!val_data)
						jsx->stream_method = STREAM_METHOD_UNKNOWN;
					else if(!strcmp(val_data,
								"http://jabber.org/protocol/bytestreams"))
						jsx->stream_method = STREAM_METHOD_BYTESTREAMS;
					else if(!strcmp(val_data, "http://jabber.org/protocol/ibb"))
						jsx->stream_method = STREAM_METHOD_IBB;
					else
						jsx->stream_method = STREAM_METHOD_UNSUPPORTED;
					g_free(val_data);
				}
			}
		}
	}
	if(jsx->stream_method == STREAM_METHOD_UNKNOWN) {
		/* XXX */
	} else if(jsx->stream_method == STREAM_METHOD_UNSUPPORTED) {
		/* XXX */
	} else if(jsx->stream_method == STREAM_METHOD_BYTESTREAMS) {
		/* XXX: open the port and stuff */
		char *buf;
		xmlnode *query, *streamhost;
		JabberIq *iq = jabber_iq_new_query(js, JABBER_IQ_SET,
				"http://jabber.org/protocol/bytestreams");

		buf = g_strdup_printf("%s/%s", xfer->who, jsx->resource);
		xmlnode_set_attrib(iq->node, "to", buf);
		g_free(buf);

		query = xmlnode_get_child(iq->node, "query");
		xmlnode_set_attrib(query, "sid", jsx->id);
		streamhost = xmlnode_new_child(query, "streamhost");
		xmlnode_set_attrib(streamhost, "jid",
				gaim_account_get_username(js->gc->account));
		xmlnode_set_attrib(streamhost, "host", gaim_network_get_ip_for_account(account, js->fd));
		buf = g_strdup_printf("%d", xfer->local_port);
		xmlnode_set_attrib(streamhost, "port", buf);
		g_free(buf);
		jabber_iq_send(iq);
	} else if(jsx->stream_method == STREAM_METHOD_IBB) {
		char *buf;
		xmlnode *open;
		JabberIq *iq = jabber_iq_new(js, JABBER_IQ_SET);
		buf = g_strdup_printf("%s/%s", xfer->who, jsx->resource);
		xmlnode_set_attrib(iq->node, "to", buf);
		g_free(buf);

		open = xmlnode_new_child(iq->node, "open");
		xmlnode_set_attrib(open, "xmlns", "http://jabber.org/protocol/ibb");
		xmlnode_set_attrib(open, "sid", jsx->id);

		jabber_iq_set_callback(iq, jabber_si_xfer_ibb_start, xfer);

		jabber_iq_send(iq);

	}
}

static void jabber_si_xfer_send_request(GaimXfer *xfer)
{
	JabberSIXfer *jsx = xfer->data;
	JabberIq *iq;
	xmlnode *si, *file, *feature, *x, *field, *option, *value;
	char buf[32];
	char *to;

	xfer->filename = g_path_get_basename(xfer->local_filename);

	iq = jabber_iq_new(jsx->js, JABBER_IQ_SET);
	to = g_strdup_printf("%s/%s", xfer->who, jsx->resource);
	xmlnode_set_attrib(iq->node, "to", to);
	g_free(to);
	si = xmlnode_new_child(iq->node, "si");
	xmlnode_set_attrib(si, "xmlns", "http://jabber.org/protocol/si");
	jsx->id = jabber_get_next_id(jsx->js);
	xmlnode_set_attrib(si, "id", jsx->id);
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
	option = xmlnode_new_child(field, "option");
	value = xmlnode_new_child(option, "value");
	xmlnode_insert_data(value, "http://jabber.org/protocol/ibb", -1);

	jabber_iq_send(iq);
}

void jabber_si_xfer_init(GaimXfer *xfer)
{
	JabberSIXfer *jsx = xfer->data;
	if(gaim_xfer_get_type(xfer) == GAIM_XFER_SEND) {
		JabberBuddy *jb;
		JabberBuddyResource *jbr = NULL;
		GList *resources;
		GList *xfer_resources = NULL;

		jb = jabber_buddy_find(jsx->js, xfer->who, TRUE);
		if(!jb)
			return;

		for(resources = jb->resources; resources; resources = resources->next) {
			jbr = resources->data;
			if(jbr->capabilities & JABBER_CAP_SI_FILE_XFER)
				xfer_resources = g_list_append(xfer_resources, jbr);
		}

		if(g_list_length(xfer_resources) == 1) {
			jbr = xfer_resources->data;
			jsx->resource = g_strdup(jbr->name);
			jabber_si_xfer_send_request(xfer);
		} else if(g_list_length(xfer_resources) == 0) {
			char *buf = g_strdup_printf(_("Could not send %s to %s, protocol not supported."), xfer->filename, xfer->who);
			gaim_notify_error(jsx->js->gc, _("File Send Failed"),
					_("File Send Failed"), buf);
			g_free(buf);
		} else {
			/* XXX: ask which resource to send to! */
		}
		g_list_free(xfer_resources);
	}
}

void jabber_si_xfer_start(GaimXfer *xfer)
{
	gaim_debug(GAIM_DEBUG_INFO, "jabber", "in jabber_si_xfer_start\n");
}

void jabber_si_xfer_end(GaimXfer *xfer)
{
	gaim_debug(GAIM_DEBUG_INFO, "jabber", "in jabber_si_xfer_end\n");
}

void jabber_si_xfer_cancel_send(GaimXfer *xfer)
{
	gaim_debug(GAIM_DEBUG_INFO, "jabber", "in jabber_si_xfer_cancel_send\n");
}


void jabber_si_xfer_cancel_recv(GaimXfer *xfer)
{
	gaim_debug(GAIM_DEBUG_INFO, "jabber", "in jabber_si_xfer_cancel_recv\n");
}


void jabber_si_xfer_ack(GaimXfer *xfer, const char *buffer, size_t size)
{
	gaim_debug(GAIM_DEBUG_INFO, "jabber", "in jabber_si_xfer_ack\n");
}

#endif
