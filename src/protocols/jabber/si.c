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
#include "util.h"

#include "buddy.h"
#include "jabber.h"
#include "iq.h"
#include "si.h"

#include "si.h"

static GaimXfer *jabber_si_xfer_find_by_id(JabberStream *js, const char *id)
{
	GList *xfers;

	if(!id)
		return NULL;

	for(xfers = js->file_transfers; xfers; xfers = xfers->next) {
		GaimXfer *xfer = xfers->data;
		JabberSIXfer *jsx = xfer->data;

		if(!strcmp(jsx->id, id))
			return xfer;
	}

	return NULL;
}

static void
jabber_si_xfer_ibb_start(JabberStream *js, xmlnode *packet, gpointer data) {
	GaimXfer *xfer = data;
	JabberSIXfer *jsx = xfer->data;

	/* Make sure we didn't get an error back */

	/* XXX: OK, here we need to set up a g_idle thing to send messages
	 * until our eyes bleed, but do it without interfering with normal
	 * gaim operations.  When we're done, we have to send a <close> like
	 * we sent the <open> to start this damn thing.  If we're really
	 * fortunate, Exodus or someone else will implement something to test
	 * against soon */
}

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

