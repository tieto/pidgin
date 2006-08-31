/*
 * Gaim's oscar protocol plugin
 * This file is the legal property of its developers.
 * Please see the AUTHORS file distributed alongside this file.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "oscar.h"

#include "eventloop.h"
#include "proxy.h"

#ifndef _WIN32
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#ifdef _WIN32
#include "win32dep.h"
#endif

/**
 * This sends a channel 1 SNAC containing the FLAP version.
 * The FLAP version is sent by itself at the beginning of every
 * connection to a FLAP server.  It is always the very first
 * packet sent by both the server and the client after the SYN,
 * SYN/ACK, ACK handshake.
 */
void
flap_connection_send_version(OscarData *od, FlapConnection *conn)
{
	FlapFrame *frame;

	frame = flap_frame_new(od, 0x01, 4);
	byte_stream_put32(&frame->data, 0x00000001);
	flap_connection_send(conn, frame);
}

/**
 * This sends a channel 1 SNAC containing the FLAP version and
 * the authentication cookie.  This is sent when connecting to
 * any FLAP server after the initial connection to the auth
 * server.  It is always the very first packet sent by both the
 * server and the client after the SYN, SYN/ACK, ACK handshake.
 */
void
flap_connection_send_version_with_cookie(OscarData *od, FlapConnection *conn, guint16 length, const guint8 *chipsahoy)
{
	FlapFrame *frame;
	aim_tlvlist_t *tl = NULL;

	frame = flap_frame_new(od, 0x01, 4 + 2 + 2 + length);
	byte_stream_put32(&frame->data, 0x00000001);
	aim_tlvlist_add_raw(&tl, 0x0006, length, chipsahoy);
	aim_tlvlist_write(&frame->data, &tl);
	aim_tlvlist_free(&tl);

	flap_connection_send(conn, frame);
}

/**
 * This sends an empty channel 4 SNAC.  This is sent to signify
 * that we're logging off.  This shouldn't really be necessary--
 * usually the AIM server will detect that the TCP connection has
 * been destroyed--but it's good practice.
 */
static void
flap_connection_send_close(OscarData *od, FlapConnection *conn)
{
	FlapFrame *frame;

	frame = flap_frame_new(od, 0x04, 0);
	flap_connection_send(conn, frame);
}

/**
 * This sends an empty channel 5 SNAC.  This is used as a keepalive
 * packet in FLAP connections.  WinAIM 4.x and higher send these
 * _every minute_ to keep the connection alive.
 */
void
flap_connection_send_keepalive(OscarData *od, FlapConnection *conn)
{
	FlapFrame *frame;

	frame = flap_frame_new(od, 0x05, 0);
	flap_connection_send(conn, frame);

	/* clean out SNACs over 60sec old */
	aim_cleansnacs(od, 60);
}

/**
 * Allocate a new empty connection structure.
 *
 * @param od The oscar session associated with this connection.
 * @param type Type of connection to create
 *
 * @return Returns the new connection structure.
 */
FlapConnection *
flap_connection_new(OscarData *od, int type)
{
	FlapConnection *conn;

	conn = g_new0(FlapConnection, 1);
	conn->od = od;
	conn->buffer_outgoing = gaim_circ_buffer_new(0);
	conn->fd = -1;
	conn->subtype = -1;
	conn->type = type;

	od->oscar_connections = g_slist_prepend(od->oscar_connections, conn);

	return conn;
}

/**
 * Close (but not free) a connection.
 *
 * This cancels any currently pending connection attempt,
 * closes any open fd and frees the auth cookie.
 *
 * @param conn The connection to close.
 */
void
flap_connection_close(OscarData *od, FlapConnection *conn)
{
	if (conn->connect_data != NULL)
	{
		gaim_proxy_connect_cancel(conn->connect_data);
		conn->connect_data = NULL;
	}

	if (conn->connect_data != NULL)
	{
		if (conn->type == SNAC_FAMILY_CHAT)
		{
			oscar_chat_destroy(conn->new_conn_data);
			conn->connect_data = NULL;
		}
	}

	if (conn->fd != -1)
	{
		if (conn->type == SNAC_FAMILY_LOCATE)
			flap_connection_send_close(od, conn);

		close(conn->fd);
		conn->fd = -1;
	}

	if (conn->watcher_incoming != 0)
	{
		gaim_input_remove(conn->watcher_incoming);
		conn->watcher_incoming = 0;
	}

	if (conn->watcher_outgoing != 0)
	{
		gaim_input_remove(conn->watcher_outgoing);
		conn->watcher_outgoing = 0;
	}

	g_free(conn->buffer_incoming.data.data);
	conn->buffer_incoming.data.data = NULL;

	gaim_circ_buffer_destroy(conn->buffer_outgoing);
	conn->buffer_outgoing = NULL;
}

static void
flap_connection_destroy_rates(struct rateclass *head)
{
	struct rateclass *rc;

	for (rc = head; rc; )
	{
		struct rateclass *tmp;
		struct snacpair *sp;

		tmp = rc->next;

		for (sp = rc->members; sp; ) {
			struct snacpair *tmpsp;

			tmpsp = sp->next;
			free(sp);
			sp = tmpsp;
		}
		free(rc);

		rc = tmp;
	}
}

static gboolean
flap_connection_destroy_cb(gpointer data)
{
	FlapConnection *conn;
	OscarData *od;
	GaimAccount *account;

	conn = data;
	od = conn->od;
	account = gaim_connection_get_account(od->gc);

	gaim_debug_info("oscar", "Destroying oscar connection of "
			"type 0x%04hx\n", conn->type);

	od->oscar_connections = g_slist_remove(od->oscar_connections, conn);

	/*
	 * TODO: If we don't have a SNAC_FAMILY_LOCATE connection then
	 * we should try to request one instead of disconnecting.
	 */
	if (!account->disconnecting && ((od->oscar_connections == NULL)
			|| (!flap_connection_getbytype(od, SNAC_FAMILY_LOCATE))))
	{
		/* No more FLAP connections!  Sign off this GaimConnection! */
		gchar *tmp;
		if (conn->disconnect_reason == OSCAR_DISCONNECT_REMOTE_CLOSED)
			tmp = g_strdup(_("Server closed the connection."));
		else if (conn->disconnect_reason == OSCAR_DISCONNECT_LOST_CONNECTION)
			tmp = g_strdup_printf(_("Lost connection with server:\n%s"),
					conn->error_message);
		else if (conn->disconnect_reason == OSCAR_DISCONNECT_INVALID_DATA)
			tmp = g_strdup(_("Received invalid data on connection with server."));
		else if (conn->disconnect_reason == OSCAR_DISCONNECT_COULD_NOT_CONNECT)
			tmp = g_strdup_printf(_("Could not establish a connection with the server:\n%s"),
					conn->error_message);
		else
			/*
			 * We shouldn't print a message for some disconnect_reasons.
			 * Like OSCAR_DISCONNECT_LOCAL_CLOSED.
			 */
			tmp = NULL;

		if (tmp != NULL)
		{
			gaim_connection_error(od->gc, tmp);
			g_free(tmp);
		}
	}

	flap_connection_close(od, conn);

	g_free(conn->error_message);
	g_free(conn->cookie);

	/*
	 * Free conn->internal, if necessary
	 */
	if (conn->type == SNAC_FAMILY_CHAT)
		flap_connection_destroy_chat(od, conn);

	g_slist_free(conn->groups);
	flap_connection_destroy_rates(conn->rates);

	g_free(conn);

	return FALSE;
}

/**
 * See the comments for the parameters of
 * flap_connection_schedule_destroy().
 */
void
flap_connection_destroy(FlapConnection *conn, OscarDisconnectReason reason, const gchar *error_message)
{
	if (conn->destroy_timeout != 0)
		gaim_timeout_remove(conn->destroy_timeout);
	conn->disconnect_reason = reason;
	g_free(conn->error_message);
	conn->error_message = g_strdup(error_message);
	flap_connection_destroy_cb(conn);
}

/**
 * Schedule Gaim to destroy the given FlapConnection as soon as we
 * return control back to the program's main loop.  We must do this
 * if we want to destroy the connection but we are still using it
 * for some reason.
 *
 * @param reason The reason for the disconnection.
 * @param error_message A brief error message that gives more detail
 *        regarding the reason for the disconnecting.  This should
 *        be NULL for everything except OSCAR_DISCONNECT_LOST_CONNECTION,
 *        in which case it should contain the value of strerror(errno),
 *        and OSCAR_DISCONNECT_COULD_NOT_CONNECT, in which case it
 *        should contain the error_message passed back from the call
 *        to gaim_proxy_connect().
 */
void
flap_connection_schedule_destroy(FlapConnection *conn, OscarDisconnectReason reason, const gchar *error_message)
{
	if (conn->destroy_timeout != 0)
		/* Already taken care of */
		return;

	gaim_debug_info("oscar", "Scheduling destruction of FLAP "
			"connection of type 0x%04hx\n", conn->type);
	conn->disconnect_reason = reason;
	conn->error_message = g_strdup(error_message);
	conn->destroy_timeout = gaim_timeout_add(0, flap_connection_destroy_cb, conn);
}

/**
 * In OSCAR, every connection has a set of SNAC groups associated
 * with it.  These are the groups that you can send over this connection
 * without being guaranteed a "Not supported" SNAC error.
 *
 * The grand theory of things says that these associations transcend
 * what libfaim calls "connection types" (conn->type).  You can probably
 * see the elegance here, but since I want to revel in it for a bit, you
 * get to hear it all spelled out.
 *
 * So let us say that you have your core BOS connection running.  One
 * of your modules has just given you a SNAC of the group 0x0004 to send
 * you.  Maybe an IM destined for some twit in Greenland.  So you start
 * at the top of your connection list, looking for a connection that
 * claims to support group 0x0004.  You find one.  Why, that neat BOS
 * connection of yours can do that.  So you send it on its way.
 *
 * Now, say, that fellow from Greenland has friends and they all want to
 * meet up with you in a lame chat room.  This has landed you a SNAC
 * in the family 0x000e and you have to admit you're a bit lost.  You've
 * searched your connection list for someone who wants to make your life
 * easy and deliver this SNAC for you, but there isn't one there.
 *
 * Here comes the good bit.  Without even letting anyone know, particularly
 * the module that decided to send this SNAC, and definitely not that twit
 * in Greenland, you send out a service request.  In this request, you have
 * marked the need for a connection supporting group 0x000e.  A few seconds
 * later, you receive a service redirect with an IP address and a cookie in
 * it.  Great, you say.  Now I have something to do.  Off you go, making
 * that connection.  One of the first things you get from this new server
 * is a message saying that indeed it does support the group you were looking
 * for.  So you continue and send rate confirmation and all that.
 *
 * Then you remember you had that SNAC to send, and now you have a means to
 * do it, and you do, and everyone is happy.  Except the Greenlander, who is
 * still stuck in the bitter cold.
 *
 * Oh, and this is useful for building the Migration SNACs, too.  In the
 * future, this may help convince me to implement rate limit mitigation
 * for real.  We'll see.
 *
 * Just to make me look better, I'll say that I've known about this great
 * scheme for quite some time now.  But I still haven't convinced myself
 * to make libfaim work that way.  It would take a fair amount of effort,
 * and probably some client API changes as well.  (Whenever I don't want
 * to do something, I just say it would change the client API.  Then I
 * instantly have a couple of supporters of not doing it.)
 *
 * Generally, addgroup is only called by the internal handling of the
 * server ready SNAC.  So if you want to do something before that, you'll
 * have to be more creative.  That is done rather early, though, so I don't
 * think you have to worry about it.  Unless you're me.  I care deeply
 * about such inane things.
 *
 */

/**
 * Find a FlapConnection that supports the given oscar
 * family.
 */
FlapConnection *
flap_connection_findbygroup(OscarData *od, guint16 group)
{
	GSList *cur;

	for (cur = od->oscar_connections; cur != NULL; cur = cur->next)
	{
		FlapConnection *conn;
		GSList *l;

		conn = cur->data;

		for (l = conn->groups; l != NULL; l = l->next)
		{
			if (GPOINTER_TO_UINT(l->data) == group)
				return conn;
		}
	}

	return NULL;
}

/**
 * Locates a connection of the specified type in the
 * specified session.
 *
 * TODO: Use flap_connection_findbygroup everywhere and get rid of this.
 *
 * @param od The session to search.
 * @param type The type of connection to look for.
 *
 * @return Returns the first connection found of the given target type,
 *         or NULL if none could be found.
 */
FlapConnection *
flap_connection_getbytype(OscarData *od, int type)
{
	GSList *cur;

	for (cur = od->oscar_connections; cur != NULL; cur = cur->next)
	{
		FlapConnection *conn;
		conn = cur->data;
		if ((conn->type == type) && (conn->connected))
			return conn;
	}

	return NULL;
}

FlapConnection *
flap_connection_getbytype_all(OscarData *od, int type)
{
	GSList *cur;

	for (cur = od->oscar_connections; cur; cur = cur->next)
	{
		FlapConnection *conn;
		conn = cur->data;
		if (conn->type == type)
			return conn;
	}

	return NULL;
}

/**
 * Allocate a new FLAP frame.
 *
 * @param channel The FLAP channel.  This is almost always 2.
 */
FlapFrame *
flap_frame_new(OscarData *od, guint16 channel, int datalen)
{
	FlapFrame *frame;

	frame = g_new0(FlapFrame, 1);
	frame->channel = channel;

	if (datalen > 0)
	{
		guint8 *data;
		data = g_malloc(datalen);
		byte_stream_init(&frame->data, data, datalen);
	}

	return frame;
}

/**
 * Free a FlapFrame
 *
 * @param frame The frame to free.
 * @return -1 on error; 0 on success.
 */
static void
flap_frame_destroy(FlapFrame *frame)
{
	free(frame->data.data);
	free(frame);

	return;
}

static void
parse_snac(OscarData *od, FlapConnection *conn, FlapFrame *frame)
{
	aim_module_t *cur;
	aim_modsnac_t snac;

	if (byte_stream_empty(&frame->data) < 10)
		return;

	snac.family = byte_stream_get16(&frame->data);
	snac.subtype = byte_stream_get16(&frame->data);
	snac.flags = byte_stream_get16(&frame->data);
	snac.id = byte_stream_get32(&frame->data);

	/* SNAC flags are apparently uniform across all SNACs, so we handle them here */
	if (snac.flags & 0x0001) {
		/*
		 * This means the SNAC will be followed by another SNAC with
		 * related information.  We don't need to do anything about
		 * this here.
		 */
	}
	if (snac.flags & 0x8000) {
		/*
		 * This packet contains the version of the family that this SNAC is
		 * in.  You get this when your SSI module is version 2 or higher.
		 * For now we have no need for this, but you could always save
		 * it as a part of aim_modnsac_t, or something.  The format is...
		 * 2 byte length of total mini-header (which is 6 bytes), then TLV
		 * of  type 0x0001, length 0x0002, value is the 2 byte version
		 * number
		 */
		byte_stream_advance(&frame->data, byte_stream_get16(&frame->data));
	}

	for (cur = (aim_module_t *)od->modlistv; cur; cur = cur->next) {

		if (!(cur->flags & AIM_MODFLAG_MULTIFAMILY) &&
				(cur->family != snac.family))
			continue;

		if (cur->snachandler(od, conn, cur, frame, &snac, &frame->data))
			return;
	}
}

static void
parse_fakesnac(OscarData *od, FlapConnection *conn, FlapFrame *frame, guint16 family, guint16 subtype)
{
	aim_module_t *cur;
	aim_modsnac_t snac;

	snac.family = family;
	snac.subtype = subtype;
	snac.flags = snac.id = 0;

	for (cur = (aim_module_t *)od->modlistv; cur; cur = cur->next) {

		if (!(cur->flags & AIM_MODFLAG_MULTIFAMILY) &&
				(cur->family != snac.family))
			continue;

		if (cur->snachandler(od, conn, cur, frame, &snac, &frame->data))
			return;
	}
}

static void
parse_flap_ch4(OscarData *od, FlapConnection *conn, FlapFrame *frame)
{
	aim_tlvlist_t *tlvlist;
	char *msg = NULL;
	guint16 code = 0;
	aim_rxcallback_t userfunc;

	if (byte_stream_empty(&frame->data) == 0) {
		/* XXX should do something with this */
		return;
	}

	/* An ICQ account is logging in */
	if (conn->type == SNAC_FAMILY_AUTH)
	{
		parse_fakesnac(od, conn, frame, 0x0017, 0x0003);
		return;
	}

	tlvlist = aim_tlvlist_read(&frame->data);

	if (aim_tlv_gettlv(tlvlist, 0x0009, 1))
		code = aim_tlv_get16(tlvlist, 0x0009, 1);

	if (aim_tlv_gettlv(tlvlist, 0x000b, 1))
		msg = aim_tlv_getstr(tlvlist, 0x000b, 1);

	if ((userfunc = aim_callhandler(od, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNERR)))
		userfunc(od, conn, frame, code, msg);

	aim_tlvlist_free(&tlvlist);

	free(msg);
}

/**
 * Takes a new incoming FLAP frame and sends it to the appropriate
 * handler function to be parsed.
 */
static void
parse_flap(OscarData *od, FlapConnection *conn, FlapFrame *frame)
{
	if (frame->channel == 0x01) {
		guint32 flap_version = byte_stream_get32(&frame->data);
		if (flap_version != 0x00000001)
		{
				/* Error! */
				gaim_debug_warning("oscar", "Expecting FLAP version "
					"0x00000001 but received FLAP version %08lx.  Closing connection.\n",
					flap_version);
				flap_connection_schedule_destroy(conn,
						OSCAR_DISCONNECT_INVALID_DATA, NULL);
		}
		else
			conn->connected = TRUE;

	} else if (frame->channel == 0x02) {
		parse_snac(od, conn, frame);

	} else if (frame->channel == 0x04) {
		parse_flap_ch4(od, conn, frame);

	} else if (frame->channel == 0x05) {
		/* TODO: Reset our keepalive watchdog? */

	}
}

/**
 * Read in all available data on the socket for a given connection.
 * All complete FLAPs handled immedate after they're received.
 * Incomplete FLAP data is stored locally and appended to the next
 * time this callback is triggered.
 */
void
flap_connection_recv_cb(gpointer data, gint source, GaimInputCondition cond)
{
	FlapConnection *conn;
	ssize_t read;
	guint8 header[6];

	conn = data;

	/* Read data until we run out of data and break out of the loop */
	while (TRUE)
	{
		/* Start reading a new FLAP */
		if (conn->buffer_incoming.data.data == NULL)
		{
			/* Peek at the first 6 bytes to get the length */
			read = recv(conn->fd, &header, 6, MSG_PEEK);

			/* Check if the FLAP server closed the connection */
			if (read == 0)
			{
				flap_connection_schedule_destroy(conn,
						OSCAR_DISCONNECT_REMOTE_CLOSED, NULL);
				break;
			}

			/* If there was an error then close the connection */
			if (read == -1)
			{
				if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
					/* No worries */
					break;

				/* Error! */
				flap_connection_schedule_destroy(conn,
						OSCAR_DISCONNECT_LOST_CONNECTION, strerror(errno));
				break;
			}

			/* If we don't even have a complete FLAP header then do nothing */
			if (read < 6)
				break;

			/* Read the first 6 bytes (the FLAP header) */
			read = recv(conn->fd, &header, 6, 0);

			/* All FLAP frames must start with the byte 0x2a */
			if (aimutil_get8(&header[0]) != 0x2a)
			{
				flap_connection_schedule_destroy(conn,
						OSCAR_DISCONNECT_INVALID_DATA, NULL);
				break;
			}

			/* Initialize a new temporary FlapFrame for incoming data */
			conn->buffer_incoming.channel = aimutil_get8(&header[1]);
			conn->buffer_incoming.seqnum = aimutil_get16(&header[2]);
			conn->buffer_incoming.data.len = aimutil_get16(&header[4]);
			conn->buffer_incoming.data.data = g_new(guint8, conn->buffer_incoming.data.len);
			conn->buffer_incoming.data.offset = 0;
		}

		if (conn->buffer_incoming.data.len - conn->buffer_incoming.data.offset)
		{
			/* Read data into the temporary FlapFrame until it is complete */
			read = recv(conn->fd,
						&conn->buffer_incoming.data.data[conn->buffer_incoming.data.offset],
						conn->buffer_incoming.data.len - conn->buffer_incoming.data.offset,
						0);

			/* Check if the FLAP server closed the connection */
			if (read == 0)
			{
				flap_connection_schedule_destroy(conn,
						OSCAR_DISCONNECT_REMOTE_CLOSED, NULL);
				break;
			}

			if (read == -1)
			{
				if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
					/* No worries */
					break;

				/* Error! */
				flap_connection_schedule_destroy(conn,
						OSCAR_DISCONNECT_LOST_CONNECTION, strerror(errno));
				break;
			}

			conn->buffer_incoming.data.offset += read;
			if (conn->buffer_incoming.data.offset < conn->buffer_incoming.data.len)
				/* Waiting for more data to arrive */
				break;
		}

		/* We have a complete FLAP!  Handle it and continue reading */
		byte_stream_rewind(&conn->buffer_incoming.data);
		parse_flap(conn->od, conn, &conn->buffer_incoming);
		conn->lastactivity = time(NULL);

		g_free(conn->buffer_incoming.data.data);
		conn->buffer_incoming.data.data = NULL;
	}
}

static void
send_cb(gpointer data, gint source, GaimInputCondition cond)
{
	FlapConnection *conn;
	int writelen, ret;

	conn = data;
	writelen = gaim_circ_buffer_get_max_read(conn->buffer_outgoing);

	if (writelen == 0)
	{
		gaim_input_remove(conn->watcher_outgoing);
		conn->watcher_outgoing = 0;
		return;
	}

	ret = send(conn->fd, conn->buffer_outgoing->outptr, writelen, 0);
	if (ret <= 0)
	{
		if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
			/* No worries */
			return;

		/* Error! */
		flap_connection_schedule_destroy(conn,
				OSCAR_DISCONNECT_LOST_CONNECTION, strerror(errno));
		return;
	}

	gaim_circ_buffer_mark_read(conn->buffer_outgoing, ret);
}

static void
flap_connection_send_byte_stream(ByteStream *bs, FlapConnection *conn, size_t count)
{
	if (conn == NULL)
		return;

	/* Make sure we don't send past the end of the bs */
	if (count > byte_stream_empty(bs))
		count = byte_stream_empty(bs); /* truncate to remaining space */

	if (count == 0)
		return;

	/* Add everything to our outgoing buffer */
	gaim_circ_buffer_append(conn->buffer_outgoing, bs->data, count);

	/* If we haven't already started writing stuff, then start the cycle */
	if (conn->watcher_outgoing == 0)
	{
		conn->watcher_outgoing = gaim_input_add(conn->fd,
				GAIM_INPUT_WRITE, send_cb, conn);
		send_cb(conn, conn->fd, 0);
	}
}

static void
sendframe_flap(FlapConnection *conn, FlapFrame *frame)
{
	ByteStream bs;
	int payloadlen, bslen;

	payloadlen = byte_stream_curpos(&frame->data);

	byte_stream_init(&bs, malloc(6 + payloadlen), 6 + payloadlen);

	/* FLAP header */
	byte_stream_put8(&bs, 0x2a);
	byte_stream_put8(&bs, frame->channel);
	byte_stream_put16(&bs, frame->seqnum);
	byte_stream_put16(&bs, payloadlen);

	/* Payload */
	byte_stream_rewind(&frame->data);
	byte_stream_putbs(&bs, &frame->data, payloadlen);

	bslen = byte_stream_curpos(&bs);
	byte_stream_rewind(&bs);
	flap_connection_send_byte_stream(&bs, conn, bslen);

	free(bs.data); /* XXX byte_stream_free */
}

void
flap_connection_send(FlapConnection *conn, FlapFrame *frame)
{
	frame->seqnum = ++(conn->seqnum);
	sendframe_flap(conn, frame);
	flap_frame_destroy(frame);
}

