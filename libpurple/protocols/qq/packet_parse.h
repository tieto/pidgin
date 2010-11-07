/**
 * @file packet_parse.h
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

#ifndef _QQ_PACKET_PARSE_H_
#define _QQ_PACKET_PARSE_H_

#include <glib.h>
#include <time.h>

/* According to "UNIX Network Programming", all TCP/IP implementations
 * must support a minimum IP datagram size of 576 bytes, regardless of the MTU.
 * Assuming a 20 byte IP header and 8 byte UDP header, this leaves 548 bytes 
 * as a safe maximum size for UDP messages.
 *
 * TCP allows packet 64K
 */
#define MAX_PACKET_SIZE 65535

gint qq_get8(guint8 *b, guint8 *buf);
gint qq_get16(guint16 *w, guint8 *buf);
gint qq_get32(guint32 *dw,  guint8 *buf);
gint qq_getime(time_t *t, guint8 *buf);
gint qq_getdata(guint8 *data, gint datalen, guint8 *buf);

gint qq_put8(guint8 *buf, guint8 b);
gint qq_put16(guint8 *buf, guint16 w);
gint qq_put32(guint8 *buf, guint32 dw);
gint qq_putdata(guint8 *buf, const guint8 *data, const int datalen);

/*
gint read_packet_b(guint8 *buf, guint8 **cursor, gint buflen, guint8 *b);
gint read_packet_w(guint8 *buf, guint8 **cursor, gint buflen, guint16 *w);
gint read_packet_dw(guint8 *buf, guint8 **cursor, gint buflen, guint32 *dw);
gint read_packet_time(guint8 *buf, guint8 **cursor, gint buflen, time_t *t);
gint read_packet_data(guint8 *buf, guint8 **cursor, gint buflen, guint8 *data, gint datalen);

gint create_packet_b(guint8 *buf, guint8 **cursor, guint8 b);
gint create_packet_w(guint8 *buf, guint8 **cursor, guint16 w);
gint create_packet_dw(guint8 *buf, guint8 **cursor, guint32 dw);
gint create_packet_data(guint8 *buf, guint8 **cursor, guint8 *data, gint datalen);
*/

#endif
