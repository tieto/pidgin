/**
 * @file send_core.c
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include "debug.h"
#include "internal.h"

#include "crypt.h"
#include "header_info.h"
#include "packet_parse.h"
#include "qq.h"
#include "qq_proxy.h"
#include "send_core.h"
#include "sendqueue.h"

/* create qq packet header with given sequence
 * return the number of bytes in header if succeeds
 * return -1 if there is any error */
gint _create_packet_head_seq(guint8 *buf, PurpleConnection *gc, 
		guint16 cmd, gboolean is_auto_seq, guint16 *seq)
{
	qq_data *qd;
	gint bytes_expected, bytes;

	g_return_val_if_fail(buf != NULL, -1);

	qd = (qq_data *) gc->proto_data;
	if (is_auto_seq)
		*seq = ++(qd->send_seq);

	bytes = 0;
	bytes_expected = (qd->use_tcp) ? QQ_TCP_HEADER_LENGTH : QQ_UDP_HEADER_LENGTH;

	/* QQ TCP packet has two bytes in the begining defines packet length
	 * so I leave room here for size */
	if (qd->use_tcp) {
		bytes += qq_put16(buf + bytes, 0x0000);
	}
	/* now comes the normal QQ packet as UDP */
	bytes += qq_put8(buf + bytes, QQ_PACKET_TAG);
	bytes += qq_put16(buf + bytes, QQ_CLIENT);
	bytes += qq_put16(buf + bytes, cmd);
	bytes += qq_put16(buf + bytes, *seq);

	if (bytes != bytes_expected) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ",
				"Fail create qq header, expect %d bytes, written %d bytes\n", bytes_expected, bytes);
		bytes = -1;
	}
	return bytes;
}

/* for those need ack and resend no ack feed back from server
 * return number of bytes written to the socket,
 * return -1 if there is any error */
gint _qq_send_packet(PurpleConnection *gc, guint8 *buf, gint len, guint16 cmd)
{
	qq_data *qd;
	qq_sendpacket *p;
	gint bytes = 0;

	qd = (qq_data *) gc->proto_data;

	if (qd->use_tcp) {
		if (len > MAX_PACKET_SIZE) {
			purple_debug(PURPLE_DEBUG_ERROR, "QQ",
					"xxx [%05d] %s, %d bytes is too large, do not send\n",
					qq_get_cmd_desc(cmd), qd->send_seq, len);
			return -1;
		} else {	/* I update the len for TCP packet */
			/* set TCP packet length
			 * _create_packet_head_seq has reserved two byte for storing pkt length, ccpaging */
			qq_put16(buf, len);
		}
	}

	/* bytes actually returned */
	bytes = qq_proxy_write(qd, buf, len);

	if (bytes >= 0) {		/* put to queue, for matching server ACK usage */
		p = g_new0(qq_sendpacket, 1);
		p->fd = qd->fd;
		p->cmd = cmd;
		p->send_seq = qd->send_seq;
		p->resend_times = 0;
		p->sendtime = time(NULL);
		p->buf = g_memdup(buf, len);	/* don't use g_strdup, may have 0x00 */
		p->len = len;
		qd->sendqueue = g_list_append(qd->sendqueue, p);
	}

	/* for debugging, s3e, 20070622 */
	_qq_show_packet("QQ_SEND_PACKET", p->buf, p->len);
	purple_debug(PURPLE_DEBUG_INFO, "QQ", "%d bytes written to the socket.\n", bytes);

	return bytes;
}

/* send the packet generated with the given cmd and data
 * return the number of bytes sent to socket if succeeds
 * return -1 if there is any error */
gint qq_send_cmd(PurpleConnection *gc, guint16 cmd,
		gboolean is_auto_seq, guint16 seq, gboolean need_ack, guint8 *data, gint len)
{
	qq_data *qd;
	guint8 *buf, *encrypted_data;
	guint16 seq_ret;
	gint encrypted_len, bytes, bytes_header, bytes_expected, bytes_sent;

	qd = (qq_data *) gc->proto_data;
	g_return_val_if_fail(qd->session_key != NULL, -1);

	buf = g_newa(guint8, MAX_PACKET_SIZE);
	encrypted_len = len + 16;	/* at most 16 bytes more */
	encrypted_data = g_newa(guint8, encrypted_len);

	qq_encrypt(data, len, qd->session_key, encrypted_data, &encrypted_len);

	seq_ret = seq;

	bytes = 0;
	bytes += _create_packet_head_seq(buf + bytes, gc, cmd, is_auto_seq, &seq_ret);
	if (bytes <= 0) {
		/* _create_packet_head_seq warned before */
		return -1;
	}
	
	bytes_header = bytes;
	bytes_expected = 4 + encrypted_len + 1;
	bytes += qq_put32(buf + bytes, (guint32) qd->uid);
	bytes += qq_putdata(buf + bytes, encrypted_data, encrypted_len);
	bytes += qq_put8(buf + bytes, QQ_PACKET_TAIL);

	if ((bytes - bytes_header) != bytes_expected) {	/* bad packet */
		purple_debug(PURPLE_DEBUG_ERROR, "QQ",
				"Fail creating packet, expect %d bytes, written %d bytes\n",
				bytes_expected, bytes - bytes_header);
		return -1;
	}

	/* if it does not need ACK, we send ACK manually several times */
	if (need_ack)   /* my request, send it */
		bytes_sent = _qq_send_packet(gc, buf, bytes, cmd);
	else		/* server's request, send ACK */
		bytes_sent = qq_proxy_write(qd, buf, bytes);

	if (QQ_DEBUG)
		purple_debug(PURPLE_DEBUG_INFO, "QQ",
				"<== [%05d] %s, %d bytes\n", seq_ret, qq_get_cmd_desc(cmd), bytes_sent);
	return bytes_sent;
}
