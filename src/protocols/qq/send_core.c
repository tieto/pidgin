/**
* The QQ2003C protocol plugin
 *
 * for gaim
 *
 * Copyright (C) 2004 Puzzlebird
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

// START OF FILE
/*****************************************************************************/
#include "debug.h"		// gaim_debug
#include "internal.h"		// send, socket

#include "packet_parse.h"	// MAX_PACKET_SIZE
#include "crypt.h"		// qq_crypt
#include "header_info.h"	// cmd alias
#include "qq_proxy.h"		// qq_proxy_write
#include "send_core.h"
#include "sendqueue.h"		// qq_sendpacket
#include "qq.h"			// qq_data, QQ_DEBUG

/*****************************************************************************/
// create qq packet header with given sequence
// return the number of bytes in header if succeeds
// return -1 if there is any error
static gint _create_packet_head_seq(guint8 * buf, guint8 ** cursor,
			     GaimConnection * gc, guint16 cmd, gboolean is_auto_seq, guint16 * seq)
{
	qq_data *qd;
	gint bytes_expected, bytes_written;

	g_return_val_if_fail(gc != NULL &&
			     gc->proto_data != NULL && buf != NULL && cursor != NULL && *cursor != NULL, -1);

	qd = (qq_data *) gc->proto_data;
	if (is_auto_seq)
		*seq = ++(qd->send_seq);

	*cursor = buf;
	bytes_written = 0;
	bytes_expected = (qd->use_tcp) ? QQ_TCP_HEADER_LENGTH : QQ_UDP_HEADER_LENGTH;

	// QQ TCP packet has two bytes in the begining defines packet length
	// so I leave room here for size
	if (qd->use_tcp)
		bytes_written += create_packet_w(buf, cursor, 0x0000);

	// now comes the normal QQ packet as UDP
	bytes_written += create_packet_b(buf, cursor, QQ_PACKET_TAG);
	bytes_written += create_packet_w(buf, cursor, QQ_CLIENT);
	bytes_written += create_packet_w(buf, cursor, cmd);
	bytes_written += create_packet_w(buf, cursor, *seq);

	if (bytes_written != bytes_expected) {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ",
			   "Fail create qq header, expect %d bytes, written %d bytes\n", bytes_expected, bytes_written);
		bytes_written = -1;
	}
	return bytes_written;
}				// _create_packet_head_seq

/*****************************************************************************/
// for those need ack and resend no ack feed back from server
// return number of bytes written to the socket,
// return -1 if there is any error
static gint _qq_send_packet(GaimConnection * gc, guint8 * buf, gint len, guint16 cmd)
{
	qq_data *qd;
	qq_sendpacket *p;
	gint bytes_sent;
	guint8 *cursor;

	g_return_val_if_fail(gc != NULL && gc->proto_data != NULL, -1);

	qd = (qq_data *) gc->proto_data;

	if (qd->use_tcp) {
		if (len > MAX_PACKET_SIZE) {
			gaim_debug(GAIM_DEBUG_ERROR, "QQ",
				   "xxx [%05d] %s, %d bytes is too large, do not send\n",
				   qq_get_cmd_desc(cmd), qd->send_seq, len);
			return -1;
		} else {	// I update the len for TCP packet
			cursor = buf;
			create_packet_w(buf, &cursor, len);
		}		// if len
	}			// if use_tcp

	bytes_sent = qq_proxy_write(qd, buf, len);

	if (bytes_sent >= 0) {		//put to queue, for matching server ACK usage
		p = g_new0(qq_sendpacket, 1);
		p->fd = qd->fd;
		p->cmd = cmd;
		p->send_seq = qd->send_seq;
		p->resend_times = 0;
		p->sendtime = time(NULL);
		p->buf = g_memdup(buf, len);	// don't use g_strdup, may have 0x00
		p->len = len;
		qd->sendqueue = g_list_append(qd->sendqueue, p);
	}			// if bytes_sent

	return bytes_sent;

}				// _qq_send_packet

/*****************************************************************************/
// send the packet generated with the given cmd and data
// return the number of bytes sent to socket if succeeds
// return -1 if there is any error
gint qq_send_cmd(GaimConnection * gc, guint16 cmd,
		 gboolean is_auto_seq, guint16 seq, gboolean need_ack, guint8 * data, gint len)
{
	qq_data *qd;
	guint8 *buf, *cursor, *encrypted_data;
	guint16 seq_ret;
	gint encrypted_len, bytes_written, bytes_expected, bytes_sent;

	g_return_val_if_fail(gc != NULL && gc->proto_data != NULL, -1);

	qd = (qq_data *) gc->proto_data;
	g_return_val_if_fail(qd->session_key != NULL, -1);

	// use g_newa, so that the allocated memory will be freed
	// when the current stack frame is cleaned up
	buf = g_newa(guint8, MAX_PACKET_SIZE);
	encrypted_len = len + 16;	// at most 16 bytes more
	encrypted_data = g_newa(guint8, encrypted_len);
	cursor = buf;
	bytes_written = 0;

	qq_crypt(ENCRYPT, data, len, qd->session_key, encrypted_data, &encrypted_len);

	seq_ret = seq;
	if (_create_packet_head_seq(buf, &cursor, gc, cmd, is_auto_seq, &seq_ret) >= 0) {
		bytes_expected = 4 + encrypted_len + 1;
		bytes_written += create_packet_dw(buf, &cursor, (guint32) qd->uid);
		bytes_written += create_packet_data(buf, &cursor, encrypted_data, encrypted_len);
		bytes_written += create_packet_b(buf, &cursor, QQ_PACKET_TAIL);
		if (bytes_written == bytes_expected) {	// packet OK
			// if it does not need ACK, we send ACK manually several times
			if (need_ack)   // my request, send it
				bytes_sent = _qq_send_packet(gc, buf, cursor - buf, cmd);
			else		// server's request, send ACK
				bytes_sent = qq_proxy_write(qd, buf, cursor - buf);

			if (QQ_DEBUG)
				gaim_debug(GAIM_DEBUG_INFO, "QQ",
					   "<== [%05d] %s, %d bytes\n", seq_ret, qq_get_cmd_desc(cmd), bytes_sent);
			return bytes_sent;
		} else {	// bad packet
			gaim_debug(GAIM_DEBUG_ERROR, "QQ",
				   "Fail creating packet, expect %d bytes, written %d bytes\n",
				   bytes_expected, bytes_written);
			return -1;
		}		// if bytes_written
	}			// if create_packet_head_seq
	return -1;
}				// qq_send_cmd

/*****************************************************************************/
// END OF FILE
