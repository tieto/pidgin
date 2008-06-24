/**
 * @file packet_parse.c
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

#include <string.h>

#include "packet_parse.h"
#include "debug.h"


/*------------------------------------------------PUT------------------------------------------------*/

/* note:
 * 1, in these functions, 'b' stands for byte, 'w' stands for word, 'dw' stands for double word.
 * 2, we use '*cursor' and 'buf' as two addresses to calculate the length.
 * 3, fixed obscure bugs, thanks ccpaging.
 * 4, change '0' to '1', if want to get more info about the packet parsing.
 * by s3e, 20070717 */

#if 0
#define PARSER_DEBUG
#endif

/* read one byte from buf, 
 * return the number of bytes read if succeeds, otherwise return -1 */
/*
gint read_packet_b(guint8 *buf, guint8 **cursor, gint buflen, guint8 *b)
{
	guint8 *b_ship = NULL;
#ifdef PARSER_DEBUG
	purple_debug(PURPLE_DEBUG_INFO, "QQ_DEBUGGER",
			"[read_b] buf addr: 0x%x\n", (gpointer)buf);
#endif
	if (*cursor <= buf + buflen - sizeof(*b)) {
#ifdef PARSER_DEBUG
		purple_debug(PURPLE_DEBUG_INFO, "QQ_DEBUGGER",
			"[read_b] *cursor addr: 0x%x, buf expected addr: 0x%x\n",
			(gpointer)*cursor, (gpointer)(buf + buflen - sizeof(*b)));
#endif
		b_ship = g_new0(guint8, sizeof(guint8));
		g_memmove(b_ship, *cursor, sizeof(guint8));
		*b = *b_ship;
#ifdef PARSER_DEBUG
		purple_debug(PURPLE_DEBUG_INFO, "QQ_DEBUGGER",
			"[read_b] data: 0x%02x->0x%02x\n",
			**(guint8 **)cursor, *b);
#endif
		*cursor += sizeof(*b);
		// free
		g_free(b_ship);
		b_ship = NULL;

		return sizeof(*b);
	} else {
		return -1;
	}
}
*/
gint qq_get8(guint8 *b, guint8 *buf)
{
	guint8 b_dest;
	memcpy(&b_dest, buf, sizeof(b_dest));
	*b = b_dest;
        purple_debug(PURPLE_DEBUG_ERROR, "QQ", "[DBG][get8] buf %d\n", (void *)buf);
        purple_debug(PURPLE_DEBUG_ERROR, "QQ", "[DBG][get8] b_dest 0x%2x, *b 0x%02x\n", b_dest, *b);
	return sizeof(b_dest);
}


/* read two bytes as "guint16" from buf, 
 * return the number of bytes read if succeeds, otherwise return -1 */
/*
gint read_packet_w(guint8 *buf, guint8 **cursor, gint buflen, guint16 *w)
{
	guint8 *w_ship = NULL;
	guint16 w_dest;
#ifdef PARSER_DEBUG
	purple_debug(PURPLE_DEBUG_INFO, "QQ_DEBUGGER",
			"[read_w] buf addr: 0x%x\n", (gpointer)buf);
#endif
	if (*cursor <= buf + buflen - sizeof(*w)) {
#ifdef PARSER_DEBUG
		purple_debug(PURPLE_DEBUG_INFO, "QQ_DEBUGGER",
			"[read_w] *cursor addr: 0x%x, buf expected addr: 0x%x\n",
			(gpointer)*cursor, (gpointer)(buf + buflen - sizeof(*w)));
#endif
		// type should match memory buffer
		w_ship = (guint8 *)g_new0(guint16, 1);
		// copy bytes into temporary buffer
		g_memmove(w_ship, *cursor, sizeof(guint16));
		// type convert and assign value
		w_dest = *(guint16 *)w_ship;
		// ntohs
		*w = g_ntohs(w_dest);
#ifdef PARSER_DEBUG
		purple_debug(PURPLE_DEBUG_INFO, "QQ_DEBUGGER",
			"[read_w] data: 0x%04x->0x%04x-g_ntohs->0x%04x\n",
			**(guint16 **)cursor, w_dest, *w);
#endif
		// *cursor goes on
		*cursor += sizeof(*w);
		
		// free mem
		g_free(w_ship);
		w_ship = NULL;

		return sizeof(*w);
	} else {
		return -1;
	}
}
*/
gint qq_get16(guint16 *w, guint8 *buf)
{
	guint16 w_dest;
	memcpy(&w_dest, buf, sizeof(w_dest));
	*w = g_ntohs(w_dest);
        purple_debug(PURPLE_DEBUG_ERROR, "QQ", "[DBG][get16] buf %d\n", (void *)buf);
        purple_debug(PURPLE_DEBUG_ERROR, "QQ", "[DBG][get16] w_dest 0x%04x, *w 0x%04x\n", w_dest, *w);
	return sizeof(w_dest);
}


/* read four bytes as "guint32" from buf, 
 * return the number of bytes read if succeeds, otherwise return -1 */
/*
gint read_packet_dw(guint8 *buf, guint8 **cursor, gint buflen, guint32 *dw)
{
	guint8 *dw_ship = NULL;
	guint32 dw_dest;
#ifdef PARSER_DEBUG
	purple_debug(PURPLE_DEBUG_INFO, "QQ_DEBUGGER",
			"[read_dw] buf addr: 0x%x\n", (gpointer)buf);
#endif
	if (*cursor <= buf + buflen - sizeof(*dw)) {
#ifdef PARSER_DEBUG
		purple_debug(PURPLE_DEBUG_INFO, "QQ_DEBUGGER",
			"[read_dw] *cursor addr: 0x%x, buf expected addr: 0x%x\n",
			(gpointer)*cursor, (gpointer)(buf + buflen - sizeof(*dw)));
#endif
		dw_ship = (guint8 *)g_new0(guint32, 1);
		g_memmove(dw_ship, *cursor, sizeof(guint32));
		dw_dest = *(guint32 *)dw_ship;
		*dw = g_ntohl(dw_dest);
#ifdef PARSER_DEBUG
		purple_debug(PURPLE_DEBUG_INFO, "QQ_DEBUGGER",
			"[read_dw] data: 0x%08x->0x%08x-g_ntohl->0x%08x\n",
			**(guint32 **)cursor, dw_dest, *dw);
#endif
		*cursor += sizeof(*dw);

		g_free(dw_ship);
		dw_ship = NULL;

		return sizeof(*dw);
	} else {
		return -1;
	}
}
*/
gint qq_get32(guint32 *dw, guint8 *buf)
{
	guint32 dw_dest;
	memcpy(&dw_dest, buf, sizeof(dw_dest));
	*dw = g_ntohl(dw_dest);
        purple_debug(PURPLE_DEBUG_ERROR, "QQ", "[DBG][get32] buf %d\n", (void *)buf);
        purple_debug(PURPLE_DEBUG_ERROR, "QQ", "[DBG][get32] dw_dest 0x%08x, *dw 0x%08x\n", dw_dest, *dw);
	return sizeof(dw_dest);
}


/* read datalen bytes from buf, 
 * return the number of bytes read if succeeds, otherwise return -1 */
/*
gint read_packet_data(guint8 *buf, guint8 **cursor, gint buflen, guint8 *data, gint datalen) {
#ifdef PARSER_DEBUG
	purple_debug(PURPLE_DEBUG_INFO, "QQ_DEBUGGER",
			"[read_data] buf addr: 0x%x\n", (gpointer)buf);
#endif
	if (*cursor <= buf + buflen - datalen) {
#ifdef PARSER_DEBUG
		purple_debug(PURPLE_DEBUG_INFO, "QQ_DEBUGGER",
			"[read_data] *cursor addr: 0x%x, buf expected addr: 0x%x\n",
			(gpointer)*cursor, (gpointer)(buf + buflen - datalen));
#endif
		g_memmove(data, *cursor, datalen);
		*cursor += datalen;
		return datalen;
	} else {
		return -1;
	}
}
*/
gint qq_getdata(guint8 *data, gint datalen, guint8 *buf)
{
    memcpy(data, buf, datalen);
        purple_debug(PURPLE_DEBUG_ERROR, "QQ", "[DBG][getdata] buf %d\n", (void *)buf);
    return datalen;
}


/* read four bytes as "time_t" from buf,
 * return the number of bytes read if succeeds, otherwise return -1
 * This function is a wrapper around read_packet_dw() to avoid casting. */
/*
gint read_packet_time(guint8 *buf, guint8 **cursor, gint buflen, time_t *t)
{
	guint32 time;
	gint ret = read_packet_dw(buf, cursor, buflen, &time);
	if (ret != -1 ) {
		*t = time;
	}
	return ret;
}
*/
gint qq_getime(time_t *t, guint8 *buf)
{
	guint32 dw_dest;
	memcpy(&dw_dest, buf, sizeof(dw_dest));
        purple_debug(PURPLE_DEBUG_ERROR, "QQ", "[DBG][getime] buf %d\n", (void *)buf);
        purple_debug(PURPLE_DEBUG_ERROR, "QQ", "[DBG][getime] dw_dest before 0x%08x\n", dw_dest);
	dw_dest = g_ntohl(dw_dest);
        purple_debug(PURPLE_DEBUG_ERROR, "QQ", "[DBG][getime] dw_dest after 0x%08x\n", dw_dest);
	memcpy(t, &dw_dest, sizeof(dw_dest));
	return sizeof(dw_dest);
}

/*------------------------------------------------PUT------------------------------------------------*/
/* pack one byte into buf
 * return the number of bytes packed, otherwise return -1 */
/*
gint create_packet_b(guint8 *buf, guint8 **cursor, guint8 b)
{
	guint8 b_dest;
#ifdef PARSER_DEBUG
	// show me the address!
	purple_debug(PURPLE_DEBUG_INFO, "QQ_DEBUGGER",
			"[create_b] buf addr: 0x%x\n", (gpointer)buf);
#endif
	// using gpointer is more safe, s3e, 20070704
	if ((gpointer)*cursor <= (gpointer)(buf + MAX_PACKET_SIZE - sizeof(guint8))) {
#ifdef PARSER_DEBUG
		purple_debug(PURPLE_DEBUG_INFO, "QQ_DEBUGGER",
			"[create_b] *cursor addr: 0x%x, buf expected addr: 0x%x\n",
			(gpointer)*cursor,
			(gpointer)(buf + MAX_PACKET_SIZE - sizeof(guint8)));
#endif
		b_dest = b;
		g_memmove(*cursor, &b_dest, sizeof(guint8));
#ifdef PARSER_DEBUG
		// show data
		purple_debug(PURPLE_DEBUG_INFO, "QQ_DEBUGGER",
			"[create_b] data: 0x%02x->0x%02x\n", b, **(guint8 **)cursor);
#endif
		*cursor += sizeof(guint8);
		return sizeof(guint8);
	} else {
		return -1;
	}
}
*/
gint qq_put8(guint8 *buf, guint8 b)
{
    memcpy(buf, &b, sizeof(b));
        purple_debug(PURPLE_DEBUG_ERROR, "QQ", "[DBG][put8] buf %d\n", (void *)buf);
        purple_debug(PURPLE_DEBUG_ERROR, "QQ", "[DBG][put8] b 0x%02x\n", b);
    return sizeof(b);
}


/* pack two bytes as "guint16" into buf
 * return the number of bytes packed, otherwise return -1 */
/*
gint create_packet_w(guint8 *buf, guint8 **cursor, guint16 w)
{
	guint16 w_dest;
	guint8 *w_ship = NULL;
#ifdef PARSER_DEBUG
	purple_debug(PURPLE_DEBUG_INFO, "QQ_DEBUGGER",
			"[create_w] buf addr: 0x%x\n", (gpointer)buf);
#endif
	if ((gpointer)*cursor <= (gpointer)(buf + MAX_PACKET_SIZE - sizeof(guint16))) {
#ifdef PARSER_DEBUG
		purple_debug(PURPLE_DEBUG_INFO, "QQ_DEBUGGER",
			"[create_w] *cursor addr: 0x%x, buf expected addr: 0x%x\n",
			(gpointer)*cursor,
			(gpointer)(buf + MAX_PACKET_SIZE - sizeof(guint16)));
#endif
		// obscure bugs found by ccpaging, patches from him.
		// similar bugs have been fixed, s3e, 20070710
		w_dest = g_htons(w);
		w_ship = (guint8 *)&w_dest;
		g_memmove(*cursor, w_ship, sizeof(guint16));
#ifdef PARSER_DEBUG
		purple_debug(PURPLE_DEBUG_INFO, "QQ_DEBUGGER",
			"[create_w] data: 0x%04x-g_htons->0x%04x->0x%04x\n",
			w, w_dest, **(guint16 **)cursor);
#endif
		*cursor += sizeof(guint16);
		return sizeof(guint16);
	} else {
		return -1;
	}
}
*/
gint qq_put16(guint8 *buf, guint16 w)
{
    guint16 w_porter;
    w_porter = g_htons(w);
        purple_debug(PURPLE_DEBUG_ERROR, "QQ", "[DBG][put16] buf %d\n", (void *)buf);
        purple_debug(PURPLE_DEBUG_ERROR, "QQ", "[DBG][put16] w 0x%04x, w_porter 0x%04x\n", w, w_porter);
    memcpy(buf, &w_porter, sizeof(w_porter));
    return sizeof(w_porter);
}


/* pack four bytes as "guint32" into buf
 * return the number of bytes packed, otherwise return -1 */
/*
gint create_packet_dw(guint8 *buf, guint8 **cursor, guint32 dw)
{
	guint32 dw_dest;
	guint8 *dw_ship = NULL;
#ifdef PARSER_DEBUG
	purple_debug(PURPLE_DEBUG_INFO, "QQ_DEBUGGER", "[create_dw] buf addr: 0x%x\n", (gpointer)buf);
#endif
	if ((gpointer)*cursor <= (gpointer)(buf + MAX_PACKET_SIZE - sizeof(guint32))) {
#ifdef PARSER_DEBUG
		purple_debug(PURPLE_DEBUG_INFO, "QQ_DEBUGGER",
			"[create_dw] *cursor addr: 0x%x, buf expected addr: 0x%x\n",
			(gpointer)*cursor,
			(gpointer)(buf + MAX_PACKET_SIZE -sizeof(guint32)));
#endif
		dw_dest = g_htonl(dw);
		dw_ship = (guint8 *)&dw_dest;
		g_memmove(*cursor, dw_ship, sizeof(guint32));
#ifdef PARSER_DEBUG
		purple_debug(PURPLE_DEBUG_INFO, "QQ_DEBUGGER",
			"[create_dw] data: 0x%08x-g_htonl->0x%08x->0x%08x\n",
			dw, dw_dest, **(guint32 **)cursor);
#endif
		*cursor += sizeof(guint32);
		return sizeof(guint32);
	} else {
		return -1;
	}
}
*/
gint qq_put32(guint8 *buf, guint32 dw)
{
    guint32 dw_porter;
    dw_porter = g_htonl(dw);
        purple_debug(PURPLE_DEBUG_ERROR, "QQ", "[DBG][put32] buf %d\n", (void *)buf);
        purple_debug(PURPLE_DEBUG_ERROR, "QQ", "[DBG][put32] dw 0x%08x, dw_porter 0x%08x\n", dw, dw_porter);
    memcpy(buf, &dw_porter, sizeof(dw_porter));
    return sizeof(dw_porter);
}


/* pack datalen bytes into buf
 * return the number of bytes packed, otherwise return -1 */
/*
gint create_packet_data(guint8 *buf, guint8 **cursor, guint8 *data, gint datalen)
{
#ifdef PARSER_DEBUG
	purple_debug(PURPLE_DEBUG_INFO, "QQ_DEBUGGER",
			"[create_data] buf addr: 0x%x\n", (gpointer)buf);
#endif
	if ((gpointer)*cursor <= (gpointer)(buf + MAX_PACKET_SIZE - datalen)) {
#ifdef PARSER_DEBUG
		purple_debug(PURPLE_DEBUG_INFO, "QQ_DEBUGGER",
			"[create_data] *cursor addr: 0x%x, buf expected addr: 0x%x\n",
			(gpointer)*cursor,
			(gpointer)(buf + MAX_PACKET_SIZE - datalen));
#endif
		g_memmove(*cursor, data, datalen);
		*cursor += datalen;
		return datalen;
	} else {
		return -1;
	}
}
*/
gint qq_putdata(guint8 *buf, guint8 *data, const int datalen)
{
    memcpy(buf, data, datalen);
        purple_debug(PURPLE_DEBUG_ERROR, "QQ", "[DBG][putdata] buf %d\n", (void *)buf);
    return datalen;
}


