/*
 * purple - Bonjour Protocol Plugin
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
#include "internal.h"
#include "util.h"
#include "debug.h"
#include "notify.h"
#include "proxy.h"
#include "ft.h"
#include "buddy.h"
#include "bonjour.h"
#include "bonjour_ft.h"
#include "cipher.h"

static void
bonjour_bytestreams_init(PurpleXfer *xfer);
static void
bonjour_bytestreams_connect(PurpleXfer *xfer, PurpleBuddy *pb);
static void
bonjour_xfer_init(PurpleXfer *xfer);
static void
bonjour_xfer_receive(PurpleConnection *pc, const char *id, const char *sid, const char *from,
		     const int filesize, const char *filename, int option);
static void bonjour_free_xfer(PurpleXfer *xfer);

/* Look for specific xfer handle */
static unsigned int next_id = 0;

static void
xep_ft_si_reject(BonjourData *bd, const char *id, const char *to, const char *error_code, const char *error_type)
{
	xmlnode *error_node;
	XepIq *iq;

	g_return_if_fail(error_code != NULL);
	g_return_if_fail(error_type != NULL);

	if(!to || !id)
		return;

	purple_debug_info("bonjour", "xep file transfer stream initialization error.\n");
	iq = xep_iq_new(bd, XEP_IQ_ERROR, to, purple_account_get_username(bd->jabber_data->account), id);
	if(iq == NULL)
		return;

	error_node = xmlnode_new_child(iq->node, "error");
	xmlnode_set_attrib(error_node, "code", error_code);
	xmlnode_set_attrib(error_node, "type", error_type);

	/* TODO: Make this better */
	if (!strcmp(error_code, "403")) {
		xmlnode *tmp_node = xmlnode_new_child(error_node, "forbidden");
		xmlnode_set_namespace(tmp_node, "urn:ietf:params:xml:ns:xmpp-stanzas");

		tmp_node = xmlnode_new_child(error_node, "text");
		xmlnode_set_namespace(tmp_node, "urn:ietf:params:xml:ns:xmpp-stanzas");
		xmlnode_insert_data(tmp_node, "Offer Declined", -1);
	} else if (!strcmp(error_code, "404")) {
		xmlnode *tmp_node = xmlnode_new_child(error_node, "item-not-found");
		xmlnode_set_namespace(tmp_node, "urn:ietf:params:xml:ns:xmpp-stanzas");
	}

	xep_iq_send_and_free(iq);
}

static void bonjour_xfer_cancel_send(PurpleXfer *xfer)
{
	purple_debug_info("bonjour", "Bonjour-xfer-cancel-send.\n");
	bonjour_free_xfer(xfer);
}

static void bonjour_xfer_request_denied(PurpleXfer *xfer)
{
	XepXfer *xf = xfer->data;

	purple_debug_info("bonjour", "Bonjour-xfer-request-denied.\n");

	if(xf)
		xep_ft_si_reject(xf->data, xf->sid, xfer->who, "403", "cancel");

	bonjour_free_xfer(xfer);
}

static void bonjour_xfer_cancel_recv(PurpleXfer *xfer)
{
	purple_debug_info("bonjour", "Bonjour-xfer-cancel-recv.\n");
	bonjour_free_xfer(xfer);
}

struct socket_cleanup {
	int fd;
	guint handle;
};

static void
_wait_for_socket_close(gpointer data, gint source, PurpleInputCondition cond)
{
	struct socket_cleanup *sc = data;
	char buf[1];
	int ret;

	ret = recv(source, buf, 1, 0);

	if (ret == 0 || (ret == -1 && !(errno == EAGAIN || errno == EWOULDBLOCK))) {
		purple_debug_info("bonjour", "Client completed recieving; closing server socket.\n");
		purple_input_remove(sc->handle);
		close(sc->fd);
		g_free(sc);
	}
}

static void bonjour_xfer_end(PurpleXfer *xfer)
{
	purple_debug_info("bonjour", "Bonjour-xfer-end.\n");

	/* We can't allow the server side to close the connection until the client is complete,
	 * otherwise there is a RST resulting in an error on the client side */
	if (purple_xfer_get_type(xfer) == PURPLE_XFER_SEND && purple_xfer_is_completed(xfer)) {
		struct socket_cleanup *sc = g_new0(struct socket_cleanup, 1);
		sc->fd = xfer->fd;
		xfer->fd = -1;
		sc->handle = purple_input_add(sc->fd, PURPLE_INPUT_READ,
						 _wait_for_socket_close, sc);
	}

	bonjour_free_xfer(xfer);
}

static PurpleXfer*
bonjour_si_xfer_find(BonjourData *bd, const char *sid, const char *from)
{
	GSList *xfers;
	PurpleXfer *xfer;
	XepXfer *xf;

	if(!sid || !from || !bd)
		return NULL;

	purple_debug_info("bonjour", "Look for sid=%s from=%s xferlists.\n",
			  sid, from);

	for(xfers = bd->xfer_lists; xfers; xfers = xfers->next) {
		xfer = xfers->data;
		if(xfer == NULL)
			break;
		xf = xfer->data;
		if(xf == NULL)
			break;
		if(xf->sid && xfer->who && !strcmp(xf->sid, sid) &&
				!strcmp(xfer->who, from))
			return xfer;
	}

	purple_debug_info("bonjour", "Look for xfer list fail\n");

	return NULL;
}

static void
xep_ft_si_offer(PurpleXfer *xfer, const gchar *to)
{
	xmlnode *si_node, *feature, *field, *file, *x;
	XepIq *iq;
	XepXfer *xf = xfer->data;
	BonjourData *bd = NULL;
	char buf[32];

	if(!xf)
		return;

	bd = xf->data;
	if(!bd)
		return;

	purple_debug_info("bonjour", "xep file transfer stream initialization offer-id=%d.\n", next_id);

	/* Assign stream id. */
	g_free(xf->iq_id);
	xf->iq_id = g_strdup_printf("%u", next_id++);
	iq = xep_iq_new(xf->data, XEP_IQ_SET, to, purple_account_get_username(bd->jabber_data->account), xf->iq_id);
	if(iq == NULL)
		return;

	/*Construct Stream initialization offer message.*/
	si_node = xmlnode_new_child(iq->node, "si");
	xmlnode_set_namespace(si_node, "http://jabber.org/protocol/si");
	xmlnode_set_attrib(si_node, "profile", "http://jabber.org/protocol/si/profile/file-transfer");
	g_free(xf->sid);
	xf->sid = g_strdup(xf->iq_id);
	xmlnode_set_attrib(si_node, "id", xf->sid);

	file = xmlnode_new_child(si_node, "file");
	xmlnode_set_namespace(file, "http://jabber.org/protocol/si/profile/file-transfer");
	xmlnode_set_attrib(file, "name", xfer->filename);
	g_snprintf(buf, sizeof(buf), "%" G_GSIZE_FORMAT, xfer->size);
	xmlnode_set_attrib(file, "size", buf);

	feature = xmlnode_new_child(si_node, "feature");
	xmlnode_set_namespace(feature, "http://jabber.org/protocol/feature-neg");

	x = xmlnode_new_child(feature, "x");
	xmlnode_set_namespace(x, "jabber:x:data");
	xmlnode_set_attrib(x, "type", "form");

	field = xmlnode_new_child(x, "field");
	xmlnode_set_attrib(field, "var", "stream-method");
	xmlnode_set_attrib(field, "type", "list-single");

	if (xf->mode & XEP_BYTESTREAMS) {
		xmlnode *option = xmlnode_new_child(field, "option");
		xmlnode *value = xmlnode_new_child(option, "value");
		xmlnode_insert_data(value, "http://jabber.org/protocol/bytestreams", -1);
	}
	if (xf->mode & XEP_IBB) {
		xmlnode *option = xmlnode_new_child(field, "option");
		xmlnode *value = xmlnode_new_child(option, "value");
		xmlnode_insert_data(value, "http://jabber.org/protocol/ibb", -1);
	}

	xep_iq_send_and_free(iq);
}

static void
xep_ft_si_result(PurpleXfer *xfer, char *to)
{
	xmlnode *si_node, *feature, *field, *value, *x;
	XepIq *iq;
	XepXfer *xf;
	BonjourData *bd;

	if(!to || !xfer)
		return;
	xf = xfer->data;
	if(!xf)
		return;

	bd = xf->data;

	purple_debug_info("bonjour", "xep file transfer stream initialization result.\n");
	iq = xep_iq_new(bd, XEP_IQ_RESULT, to, purple_account_get_username(bd->jabber_data->account), xf->iq_id);
	if(iq == NULL)
		return;

	si_node = xmlnode_new_child(iq->node, "si");
	xmlnode_set_namespace(si_node, "http://jabber.org/protocol/si");
	/*xmlnode_set_attrib(si_node, "profile", "http://jabber.org/protocol/si/profile/file-transfer");*/

	feature = xmlnode_new_child(si_node, "feature");
	xmlnode_set_namespace(feature, "http://jabber.org/protocol/feature-neg");

	x = xmlnode_new_child(feature, "x");
	xmlnode_set_namespace(x, "jabber:x:data");
	xmlnode_set_attrib(x, "type", "submit");

	field = xmlnode_new_child(x, "field");
	xmlnode_set_attrib(field, "var", "stream-method");

	value = xmlnode_new_child(field, "value");
	xmlnode_insert_data(value, "http://jabber.org/protocol/bytestreams", -1);

	xep_iq_send_and_free(iq);
}

static void
bonjour_free_xfer(PurpleXfer *xfer)
{
	XepXfer *xf;

	if(xfer == NULL) {
		purple_debug_info("bonjour", "bonjour-free-xfer-null.\n");
		return;
	}

	purple_debug_info("bonjour", "bonjour-free-xfer-%p.\n", xfer);

	xf = (XepXfer*)xfer->data;
	if(xf != NULL) {
		BonjourData *bd = (BonjourData*)xf->data;
		if(bd != NULL) {
			bd->xfer_lists = g_slist_remove(bd->xfer_lists, xfer);
			purple_debug_info("bonjour", "B free xfer from lists(%p).\n", bd->xfer_lists);
		}
		if (xf->proxy_connection != NULL)
			purple_proxy_connect_cancel(xf->proxy_connection);
		if (xf->proxy_info != NULL)
			purple_proxy_info_destroy(xf->proxy_info);
		if (xf->listen_data != NULL)
			purple_network_listen_cancel(xf->listen_data);
		g_free(xf->iq_id);
		g_free(xf->jid);
		g_free(xf->proxy_host);
		g_free(xf->buddy_ip);
		g_free(xf->sid);
		g_free(xf);
		xfer->data = NULL;
	}

	purple_debug_info("bonjour", "Need close socket=%d.\n", xfer->fd);
}

PurpleXfer *
bonjour_new_xfer(PurpleConnection *gc, const char *who)
{
	PurpleXfer *xfer;
	XepXfer *xep_xfer;
	BonjourData *bd;

	if(who == NULL || gc == NULL)
		return NULL;

	purple_debug_info("bonjour", "Bonjour-new-xfer to %s.\n", who);
	bd = (BonjourData*) gc->proto_data;
	if(bd == NULL)
		return NULL;

	/* Build the file transfer handle */
	xfer = purple_xfer_new(gc->account, PURPLE_XFER_SEND, who);
	xfer->data = xep_xfer = g_new0(XepXfer, 1);
	xep_xfer->data = bd;

	purple_debug_info("bonjour", "Bonjour-new-xfer bd=%p data=%p.\n", bd, xep_xfer->data);

	/* We don't support IBB yet */
	/*xep_xfer->mode = XEP_BYTESTREAMS | XEP_IBB;*/
	xep_xfer->mode = XEP_BYTESTREAMS;
	xep_xfer->sid = NULL;

	purple_xfer_set_init_fnc(xfer, bonjour_xfer_init);
	purple_xfer_set_cancel_send_fnc(xfer, bonjour_xfer_cancel_send);
	purple_xfer_set_end_fnc(xfer, bonjour_xfer_end);

	bd->xfer_lists = g_slist_append(bd->xfer_lists, xfer);

	return xfer;
}

void
bonjour_send_file(PurpleConnection *gc, const char *who, const char *file)
{
	PurpleXfer *xfer;

	g_return_if_fail(gc != NULL);
	g_return_if_fail(who != NULL);

	purple_debug_info("bonjour", "Bonjour-send-file to=%s.\n", who);

	xfer = bonjour_new_xfer(gc, who);

	if (file)
		purple_xfer_request_accepted(xfer, file);
	else
		purple_xfer_request(xfer);

}

static void
bonjour_xfer_init(PurpleXfer *xfer)
{
	PurpleBuddy *buddy;
	BonjourBuddy *bb;
	XepXfer *xf;

	xf = (XepXfer*)xfer->data;
	if(xf == NULL)
		return;

	purple_debug_info("bonjour", "Bonjour-xfer-init.\n");

	buddy = purple_find_buddy(xfer->account, xfer->who);
	/* this buddy is offline. */
	if (buddy == NULL || buddy->proto_data == NULL)
		return;

	bb = (BonjourBuddy *)buddy->proto_data;
	/* Assume it is the first IP. We could do something like keep track of which one is in use or something. */
	if (bb->ips)
		xf->buddy_ip = g_strdup(bb->ips->data);
	if (purple_xfer_get_type(xfer) == PURPLE_XFER_SEND) {
		/* initiate file transfer, send SI offer. */
		purple_debug_info("bonjour", "Bonjour xfer type is PURPLE_XFER_SEND.\n");
		xep_ft_si_offer(xfer, xfer->who);
	} else {
		/* accept file transfer request, send SI result. */
		xep_ft_si_result(xfer, xfer->who);
		purple_debug_info("bonjour", "Bonjour xfer type is PURPLE_XFER_RECEIVE.\n");
	}
}

void
xep_si_parse(PurpleConnection *pc, xmlnode *packet, PurpleBuddy *pb)
{
	const char *type, *id;
	BonjourData *bd;
	PurpleXfer *xfer;

	if(pc == NULL || packet == NULL || pb == NULL)
		return;
	bd = (BonjourData*) pc->proto_data;
	if(bd == NULL)
		return;

	purple_debug_info("bonjour", "xep-si-parse.\n");

	type = xmlnode_get_attrib(packet, "type");
	id = xmlnode_get_attrib(packet, "id");
	if(type) {
		if(!strcmp(type, "set")) {
			const char *profile;
			xmlnode *si;
			gboolean parsed_receive = FALSE;

			si = xmlnode_get_child(packet, "si");

			purple_debug_info("bonjour", "si offer Message type - SET.\n");
			if (si && (profile = xmlnode_get_attrib(si, "profile"))
					&& !strcmp(profile, "http://jabber.org/protocol/si/profile/file-transfer")) {
				const char *filename = NULL, *filesize_str = NULL;
				int filesize = 0;
				xmlnode *file;

				const char *sid = xmlnode_get_attrib(si, "id");

				if ((file = xmlnode_get_child(si, "file"))) {
					filename = xmlnode_get_attrib(file, "name");
					if((filesize_str = xmlnode_get_attrib(file, "size")))
						filesize = atoi(filesize_str);
				}

				/* TODO: Make sure that it is advertising a bytestreams transfer */

				bonjour_xfer_receive(pc, id, sid, pb->name, filesize, filename, XEP_BYTESTREAMS);

				parsed_receive = TRUE;
			}

			if (!parsed_receive) {
				purple_debug_info("bonjour", "rejecting unrecognized si SET offer.\n");
				xep_ft_si_reject((BonjourData *)pc->proto_data, id, pb->name, "403", "cancel");
				/*TODO: Send Cancel (501) */
			}
		} else if(!strcmp(type, "result")) {
			purple_debug_info("bonjour", "si offer Message type - RESULT.\n");

			xfer = bonjour_si_xfer_find(bd, id, pb->name);

			if(xfer == NULL) {
				purple_debug_info("bonjour", "xfer find fail.\n");
				xep_ft_si_reject((BonjourData *)pc->proto_data, id, pb->name, "403", "cancel");
			} else
				bonjour_bytestreams_init(xfer);

		} else if(!strcmp(type, "error")) {
			purple_debug_info("bonjour", "si offer Message type - ERROR.\n");

			xfer = bonjour_si_xfer_find(bd, id, pb->name);

			if(xfer == NULL)
				purple_debug_info("bonjour", "xfer find fail.\n");
			else
				purple_xfer_cancel_remote(xfer);
		} else
			purple_debug_info("bonjour", "si offer Message type - Unknown-%s.\n", type);
	}
}

void
xep_bytestreams_parse(PurpleConnection *pc, xmlnode *packet, PurpleBuddy *pb)
{
	const char *type, *from;
	xmlnode *query;
	BonjourData *bd;

	if(pc == NULL || packet == NULL || pb == NULL)
		return;

	bd = (BonjourData*) pc->proto_data;
	if(bd == NULL)
		return;

	purple_debug_info("bonjour", "xep-bytestreams-parse.\n");

	type = xmlnode_get_attrib(packet, "type");
	from = pb->name;
	query = xmlnode_get_child(packet,"query");
	if(type) {
		if(!strcmp(type, "set")) {
			const char *iq_id, *sid;
			gboolean found = FALSE;
			PurpleXfer *xfer;

			purple_debug_info("bonjour", "bytestream offer Message type - SET.\n");

			iq_id = xmlnode_get_attrib(packet, "id");

			sid = xmlnode_get_attrib(query, "sid");
			xfer = bonjour_si_xfer_find(bd, sid, from);

			if(xfer) {
				const char *jid, *host, *port;
				xmlnode *streamhost;
				int portnum;
				XepXfer *xf = NULL;

				xf = (XepXfer*)xfer->data;
				for(streamhost = xmlnode_get_child(query, "streamhost");
						streamhost;
						streamhost = xmlnode_get_next_twin(streamhost)) {

					if((jid = xmlnode_get_attrib(streamhost, "jid")) &&
					   (host = xmlnode_get_attrib(streamhost, "host")) &&
					   (port = xmlnode_get_attrib(streamhost, "port")) &&
					   (portnum = atoi(port))) {

						if(!strcmp(host, xf->buddy_ip)) {
							g_free(xf->iq_id);
							xf->iq_id = g_strdup(iq_id);
							xf->jid = g_strdup(jid);
							xf->proxy_host = g_strdup(host);
							xf->proxy_port = portnum;
							purple_debug_info("bonjour", "bytestream offer parse"
									  "jid=%s host=%s port=%d.\n", jid, host, portnum);
							bonjour_bytestreams_connect(xfer, pb);
							found = TRUE;
							break;
						}
					} else {
						purple_debug_info("bonjour", "bytestream offer Message parse error.\n");
					}
				}
			} else {

			}

			if (!found) {
				purple_debug_error("bonjour", "Didn't find an acceptable streamhost.\n");

				if (iq_id && xfer != NULL)
					xep_ft_si_reject(bd, iq_id, xfer->who, "404", "cancel");
			}

		} else {
			purple_debug_info("bonjour", "bytestream offer Message type - Unknown-%s.\n", type);
		}
	}
}

static void
bonjour_xfer_receive(PurpleConnection *pc, const char *id, const char *sid, const char *from,
		     const int filesize, const char *filename, int option)
{
	PurpleXfer *xfer;
	XepXfer *xf;
	BonjourData *bd;

	if(pc == NULL || id == NULL || from == NULL)
		return;

	bd = (BonjourData*) pc->proto_data;
	if(bd == NULL)
		return;

	purple_debug_info("bonjour", "bonjour-xfer-receive.\n");

	/* Build the file transfer handle */
	xfer = purple_xfer_new(pc->account, PURPLE_XFER_RECEIVE, from);
	xfer->data = xf = g_new0(XepXfer, 1);
	xf->data = bd;
	purple_xfer_set_filename(xfer, filename);
	xf->iq_id = g_strdup(id);
	xf->sid = g_strdup(sid);

	if(filesize > 0)
		purple_xfer_set_size(xfer, filesize);
	purple_xfer_set_init_fnc(xfer, bonjour_xfer_init);
	purple_xfer_set_request_denied_fnc(xfer, bonjour_xfer_request_denied);
	purple_xfer_set_cancel_recv_fnc(xfer, bonjour_xfer_cancel_recv);
	purple_xfer_set_end_fnc(xfer, bonjour_xfer_end);

	bd->xfer_lists = g_slist_append(bd->xfer_lists, xfer);

	purple_xfer_request(xfer);
}

static void
bonjour_sock5_request_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	PurpleXfer *xfer = data;
	XepXfer *xf = xfer->data;
	int acceptfd;
	int len = 0;

	if(xf == NULL)
		return;

	purple_debug_info("bonjour", "bonjour_sock5_request_cb - req_state = 0x%x\n", xf->sock5_req_state);

	switch(xf->sock5_req_state){
	case 0x00:
		acceptfd = accept(source, NULL, 0);
		if(acceptfd == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {

		} else if(acceptfd == -1) {
			/* This should cancel the ft */
			purple_debug_error("bonjour", "Error accepting incoming SOCKS5 connection. (%d)\n", errno);

			purple_input_remove(xfer->watcher);
			xfer->watcher = 0;
			close(source);
			purple_xfer_cancel_remote(xfer);
			return;
		} else {
			int flags;

			purple_debug_info("bonjour", "Accepted SOCKS5 ft connection - fd=%d\n", acceptfd);

			flags = fcntl(acceptfd, F_GETFL);
			fcntl(acceptfd, F_SETFL, flags | O_NONBLOCK);
#ifndef _WIN32
			fcntl(acceptfd, F_SETFD, FD_CLOEXEC);
#endif

			purple_input_remove(xfer->watcher);
			close(source);
			xfer->watcher = purple_input_add(acceptfd, PURPLE_INPUT_READ,
							 bonjour_sock5_request_cb, xfer);
			xf->sock5_req_state++;
			xf->rxlen = 0;
		}
		break;
	case 0x01:
		xfer->fd = source;
		len = read(source, xf->rx_buf + xf->rxlen, 3);
		if(len < 0 && errno == EAGAIN)
			return;
		else if(len <= 0){
			purple_input_remove(xfer->watcher);
			xfer->watcher = 0;
			close(source);
			purple_xfer_cancel_remote(xfer);
			return;
		} else {
			purple_input_remove(xfer->watcher);
			xfer->watcher = purple_input_add(source, PURPLE_INPUT_WRITE,
							 bonjour_sock5_request_cb, xfer);
			xf->sock5_req_state++;
			xf->rxlen = 0;
			bonjour_sock5_request_cb(xfer, source, PURPLE_INPUT_WRITE);
		}
		break;
	case 0x02:
		xf->tx_buf[0] = 0x05;
		xf->tx_buf[1] = 0x00;
		len = write(source, xf->tx_buf, 2);
		if (len < 0 && errno == EAGAIN)
			return;
		else if (len < 0) {
			purple_input_remove(xfer->watcher);
			xfer->watcher = 0;
			close(source);
			purple_xfer_cancel_remote(xfer);
			return;
		} else {
			purple_input_remove(xfer->watcher);
			xfer->watcher = purple_input_add(source, PURPLE_INPUT_READ,
							 bonjour_sock5_request_cb, xfer);
			xf->sock5_req_state++;
			xf->rxlen = 0;
		}
		break;
	case 0x03:
		len = read(source, xf->rx_buf + xf->rxlen, 20);
		if(len<=0){
		} else {
			purple_input_remove(xfer->watcher);
			xfer->watcher = purple_input_add(source, PURPLE_INPUT_WRITE,
							 bonjour_sock5_request_cb, xfer);
			xf->sock5_req_state++;
			xf->rxlen = 0;
			bonjour_sock5_request_cb(xfer, source, PURPLE_INPUT_WRITE);
		}
		break;
	case 0x04:
		xf->tx_buf[0] = 0x05;
		xf->tx_buf[1] = 0x00;
		xf->tx_buf[2] = 0x00;
		xf->tx_buf[3] = 0x03;
		xf->tx_buf[4] = strlen(xf->buddy_ip);
		memcpy(xf->tx_buf + 5, xf->buddy_ip, strlen(xf->buddy_ip));
		xf->tx_buf[5+strlen(xf->buddy_ip)] = 0x00;
		xf->tx_buf[6+strlen(xf->buddy_ip)] = 0x00;
		len = write(source, xf->tx_buf, 7 + strlen(xf->buddy_ip));
		if (len < 0 && errno == EAGAIN) {
			return;
		} else if (len < 0) {
			purple_input_remove(xfer->watcher);
			xfer->watcher = 0;
			close(source);
			purple_xfer_cancel_remote(xfer);
			return;
		} else {
			purple_input_remove(xfer->watcher);
			xfer->watcher = 0;
			xf->rxlen = 0;
			/*close(source);*/
			purple_xfer_start(xfer, source, NULL, -1);
		}
		break;
	default:
		break;
	}
	return;
}

static void
bonjour_bytestreams_listen(int sock, gpointer data)
{
	PurpleXfer *xfer = data;
	XepXfer *xf;
	XepIq *iq;
	xmlnode *query, *streamhost;
	gchar *port;
	const char *next_ip, *local_ip;
	const char token [] = ";";
	BonjourData *bd;

	purple_debug_info("bonjour", "Bonjour-bytestreams-listen. sock=%d.\n", sock);
	if (sock < 0 || xfer == NULL) {
		/*purple_xfer_cancel_local(xfer);*/
		return;
	}

	xfer->watcher = purple_input_add(sock, PURPLE_INPUT_READ,
					 bonjour_sock5_request_cb, xfer);
	xf = (XepXfer*)xfer->data;
	xf->listen_data = NULL;

	bd = xf->data;

	iq = xep_iq_new(bd, XEP_IQ_SET, xfer->who, purple_account_get_username(bd->jabber_data->account), xf->sid);

	query = xmlnode_new_child(iq->node, "query");
	xmlnode_set_namespace(query, "http://jabber.org/protocol/bytestreams");
	xmlnode_set_attrib(query, "sid", xf->sid);
	xmlnode_set_attrib(query, "mode", "tcp");

	xfer->local_port = purple_network_get_port_from_fd(sock);

	local_ip = purple_network_get_my_ip_ext2(sock);
	/* cheat a little here - the intent of the "const" attribute is to make it clear that the string doesn't need to be freed */
	next_ip = strtok((char *)local_ip, token);

	port = g_strdup_printf("%hu", xfer->local_port);
	while(next_ip != NULL) {
		streamhost = xmlnode_new_child(query, "streamhost");
		xmlnode_set_attrib(streamhost, "jid", xf->sid);
		xmlnode_set_attrib(streamhost, "host", next_ip);
		xmlnode_set_attrib(streamhost, "port", port);
		next_ip = strtok(NULL, token);
	}
	g_free(port);

	xep_iq_send_and_free(iq);
}

static void
bonjour_bytestreams_init(PurpleXfer *xfer)
{
	XepXfer *xf;
	if(xfer == NULL)
		return;
	purple_debug_info("bonjour", "Bonjour-bytestreams-init.\n");
	xf = xfer->data;
	purple_network_listen_map_external(FALSE);
	xf->listen_data = purple_network_listen_range(0, 0, SOCK_STREAM,
						      bonjour_bytestreams_listen, xfer);
	purple_network_listen_map_external(TRUE);
	if (xf->listen_data == NULL) {
		purple_xfer_cancel_local(xfer);
	}
	return;
}

static void
bonjour_bytestreams_connect_cb(gpointer data, gint source, const gchar *error_message)
{
	PurpleXfer *xfer = data;
	XepXfer *xf = xfer->data;
	XepIq *iq;
	xmlnode *q_node, *tmp_node;
	BonjourData *bd;

	xf->proxy_connection = NULL;

	if(source < 0) {
		purple_debug_error("bonjour", "Error connecting via SOCKS5 - %s\n",
			error_message ? error_message : "(null)");
		xep_ft_si_reject(xf->data, xf->iq_id, xfer->who, "404", "cancel");
		/* Cancel the connection */
		purple_xfer_cancel_local(xfer);
		return;
	}

	purple_debug_info("bonjour", "Connected successfully via SOCKS5, starting transfer.\n");

	bd = xf->data;

	/* Here, start the file transfer.*/

	/* Notify Initiator of Connection */
	iq = xep_iq_new(bd, XEP_IQ_RESULT, xfer->who, purple_account_get_username(bd->jabber_data->account), xf->iq_id);
	q_node = xmlnode_new_child(iq->node, "query");
	xmlnode_set_namespace(q_node, "http://jabber.org/protocol/bytestreams");
	tmp_node = xmlnode_new_child(q_node, "streamhost-used");
	xmlnode_set_attrib(tmp_node, "jid", xf->jid);
	xep_iq_send_and_free(iq);

	purple_xfer_start(xfer, source, NULL, -1);
}

static void
bonjour_bytestreams_connect(PurpleXfer *xfer, PurpleBuddy *pb)
{
	XepXfer *xf;
	char dstaddr[41];
	unsigned char hashval[20];
	char *p;
	int i;

	if(xfer == NULL)
		return;

	purple_debug_info("bonjour", "bonjour-bytestreams-connect.\n");

	xf = (XepXfer*)xfer->data;
	if(!xf)
		return;

	p = g_strdup_printf("%s%s%s", xf->sid, pb->name, purple_account_get_username(pb->account));
	purple_cipher_digest_region("sha1", (guchar *)p, strlen(p),
				    sizeof(hashval), hashval, NULL);
	g_free(p);

	memset(dstaddr, 0, 41);
	p = dstaddr;
	for(i = 0; i < 20; i++, p += 2)
		snprintf(p, 3, "%02x", hashval[i]);

	xf->proxy_info = purple_proxy_info_new();
	purple_proxy_info_set_type(xf->proxy_info, PURPLE_PROXY_SOCKS5);
	purple_proxy_info_set_host(xf->proxy_info, xf->proxy_host);
	purple_proxy_info_set_port(xf->proxy_info, xf->proxy_port);
	xf->proxy_connection = purple_proxy_connect_socks5(NULL, xf->proxy_info,
							   dstaddr, 0,
							   bonjour_bytestreams_connect_cb, xfer);

	if(xf->proxy_connection == NULL) {
		xep_ft_si_reject(xf->data, xf->iq_id, xfer->who, "404", "cancel");
		/* Cancel the connection */
		purple_xfer_cancel_local(xfer);
	}
}

