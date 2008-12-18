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
 * 3, change 'undef' to 'define' to get more info about the packet parsing. */

#undef PARSER_DEBUG

/* read one byte from buf,
 * return the number of bytes read if succeeds, otherwise return -1 */
gint qq_get8(guint8 *b, guint8 *buf)
{
	guint8 b_dest;
	memcpy(&b_dest, buf, sizeof(b_dest));
	*b = b_dest;
#ifdef PARSER_DEBUG
	purple_debug_info("QQ", "[DBG][get8] buf %p\n", (void *)buf);
	purple_debug_info("QQ", "[DBG][get8] b_dest 0x%2x, *b 0x%02x\n", b_dest, *b);
#endif
	return sizeof(b_dest);
}


/* read two bytes as "guint16" from buf,
 * return the number of bytes read if succeeds, otherwise return -1 */
gint qq_get16(guint16 *w, guint8 *buf)
{
	guint16 w_dest;
	memcpy(&w_dest, buf, sizeof(w_dest));
	*w = g_ntohs(w_dest);
#ifdef PARSER_DEBUG
	purple_debug_info("QQ", "[DBG][get16] buf %p\n", (void *)buf);
	purple_debug_info("QQ", "[DBG][get16] w_dest 0x%04x, *w 0x%04x\n", w_dest, *w);
#endif
	return sizeof(w_dest);
}

/* read four bytes as "guint32" from buf,
 * return the number of bytes read if succeeds, otherwise return -1 */
gint qq_get32(guint32 *dw, guint8 *buf)
{
	guint32 dw_dest;
	memcpy(&dw_dest, buf, sizeof(dw_dest));
	*dw = g_ntohl(dw_dest);
#ifdef PARSER_DEBUG
	purple_debug_info("QQ", "[DBG][get32] buf %p\n", (void *)buf);
	purple_debug_info("QQ", "[DBG][get32] dw_dest 0x%08x, *dw 0x%08x\n", dw_dest, *dw);
#endif
	return sizeof(dw_dest);
}

gint qq_getIP(struct in_addr *ip, guint8 *buf)
{
	memcpy(ip, buf, sizeof(struct in_addr));
	return sizeof(struct in_addr);
}

/* read datalen bytes from buf,
 * return the number of bytes read if succeeds, otherwise return -1 */
gint qq_getdata(guint8 *data, gint datalen, guint8 *buf)
{
    memcpy(data, buf, datalen);
#ifdef PARSER_DEBUG
	purple_debug_info("QQ", "[DBG][getdata] buf %p\n", (void *)buf);
#endif
    return datalen;
}


/* read four bytes as "time_t" from buf,
 * return the number of bytes read if succeeds, otherwise return -1
 * This function is a wrapper around read_packet_dw() to avoid casting. */
gint qq_getime(time_t *t, guint8 *buf)
{
	guint32 dw_dest;
	memcpy(&dw_dest, buf, sizeof(dw_dest));
#ifdef PARSER_DEBUG
	purple_debug_info("QQ", "[DBG][getime] buf %p\n", (void *)buf);
	purple_debug_info("QQ", "[DBG][getime] dw_dest before 0x%08x\n", dw_dest);
#endif
	dw_dest = g_ntohl(dw_dest);
#ifdef PARSER_DEBUG
	purple_debug_info("QQ", "[DBG][getime] dw_dest after 0x%08x\n", dw_dest);
#endif
	memcpy(t, &dw_dest, sizeof(dw_dest));
	return sizeof(dw_dest);
}

/*------------------------------------------------PUT------------------------------------------------*/
/* pack one byte into buf
 * return the number of bytes packed, otherwise return -1 */
gint qq_put8(guint8 *buf, guint8 b)
{
    memcpy(buf, &b, sizeof(b));
#ifdef PARSER_DEBUG
	purple_debug_info("QQ", "[DBG][put8] buf %p\n", (void *)buf);
	purple_debug_info("QQ", "[DBG][put8] b 0x%02x\n", b);
#endif
    return sizeof(b);
}


/* pack two bytes as "guint16" into buf
 * return the number of bytes packed, otherwise return -1 */
gint qq_put16(guint8 *buf, guint16 w)
{
    guint16 w_porter;
    w_porter = g_htons(w);
#ifdef PARSER_DEBUG
	purple_debug_info("QQ", "[DBG][put16] buf %p\n", (void *)buf);
	purple_debug_info("QQ", "[DBG][put16] w 0x%04x, w_porter 0x%04x\n", w, w_porter);
#endif
    memcpy(buf, &w_porter, sizeof(w_porter));
    return sizeof(w_porter);
}


/* pack four bytes as "guint32" into buf
 * return the number of bytes packed, otherwise return -1 */
gint qq_put32(guint8 *buf, guint32 dw)
{
	guint32 dw_porter;
    dw_porter = g_htonl(dw);
#ifdef PARSER_DEBUG
	purple_debug_info("QQ", "[DBG][put32] buf %p\n", (void *)buf);
	purple_debug_info("QQ", "[DBG][put32] dw 0x%08x, dw_porter 0x%08x\n", dw, dw_porter);
#endif
    memcpy(buf, &dw_porter, sizeof(dw_porter));
    return sizeof(dw_porter);
}

gint qq_putime(guint8 *buf, time_t *t)
{
	guint32 dw, dw_porter;
	memcpy(&dw, t, sizeof(dw));
    dw_porter = g_htonl(dw);
#ifdef PARSER_DEBUG
	purple_debug_info("QQ", "[DBG][put32] buf %p\n", (void *)buf);
	purple_debug_info("QQ", "[DBG][put32] dw 0x%08x, dw_porter 0x%08x\n", dw, dw_porter);
#endif
    memcpy(buf, &dw_porter, sizeof(dw_porter));
    return sizeof(dw_porter);
}

gint qq_putIP(guint8* buf, struct in_addr *ip)
{
    memcpy(buf, ip, sizeof(struct in_addr));
    return sizeof(struct in_addr);
}

/* pack datalen bytes into buf
 * return the number of bytes packed, otherwise return -1 */
gint qq_putdata(guint8 *buf, const guint8 *data, const int datalen)
{
   	memcpy(buf, data, datalen);
#ifdef PARSER_DEBUG
	purple_debug_info("QQ", "[DBG][putdata] buf %p\n", (void *)buf);
#endif
    return datalen;
}
