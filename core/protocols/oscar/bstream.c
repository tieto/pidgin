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

/*
 * This file contains all functions needed to use bstreams.
 */

#include "oscar.h"

int byte_stream_init(ByteStream *bs, guint8 *data, int len)
{

	if (!bs)
		return -1;

	bs->data = data;
	bs->len = len;
	bs->offset = 0;

	return 0;
}

int byte_stream_empty(ByteStream *bs)
{
	return bs->len - bs->offset;
}

int byte_stream_curpos(ByteStream *bs)
{
	return bs->offset;
}

int byte_stream_setpos(ByteStream *bs, unsigned int off)
{

	if (off > bs->len)
		return -1;

	bs->offset = off;

	return off;
}

void byte_stream_rewind(ByteStream *bs)
{

	byte_stream_setpos(bs, 0);

	return;
}

/*
 * N can be negative, which can be used for going backwards
 * in a bstream.  I'm not sure if libfaim actually does
 * this anywhere...
 */
int byte_stream_advance(ByteStream *bs, int n)
{

	if ((byte_stream_curpos(bs) + n < 0) || (byte_stream_empty(bs) < n))
		return 0; /* XXX throw an exception */

	bs->offset += n;

	return n;
}

guint8 byte_stream_get8(ByteStream *bs)
{

	if (byte_stream_empty(bs) < 1)
		return 0; /* XXX throw an exception */

	bs->offset++;

	return aimutil_get8(bs->data + bs->offset - 1);
}

guint16 byte_stream_get16(ByteStream *bs)
{

	if (byte_stream_empty(bs) < 2)
		return 0; /* XXX throw an exception */

	bs->offset += 2;

	return aimutil_get16(bs->data + bs->offset - 2);
}

guint32 byte_stream_get32(ByteStream *bs)
{

	if (byte_stream_empty(bs) < 4)
		return 0; /* XXX throw an exception */

	bs->offset += 4;

	return aimutil_get32(bs->data + bs->offset - 4);
}

guint8 byte_stream_getle8(ByteStream *bs)
{

	if (byte_stream_empty(bs) < 1)
		return 0; /* XXX throw an exception */

	bs->offset++;

	return aimutil_getle8(bs->data + bs->offset - 1);
}

guint16 byte_stream_getle16(ByteStream *bs)
{

	if (byte_stream_empty(bs) < 2)
		return 0; /* XXX throw an exception */

	bs->offset += 2;

	return aimutil_getle16(bs->data + bs->offset - 2);
}

guint32 byte_stream_getle32(ByteStream *bs)
{

	if (byte_stream_empty(bs) < 4)
		return 0; /* XXX throw an exception */

	bs->offset += 4;

	return aimutil_getle32(bs->data + bs->offset - 4);
}

int byte_stream_getrawbuf(ByteStream *bs, guint8 *buf, int len)
{

	if (byte_stream_empty(bs) < len)
		return 0;

	memcpy(buf, bs->data + bs->offset, len);
	bs->offset += len;

	return len;
}

guint8 *byte_stream_getraw(ByteStream *bs, int len)
{
	guint8 *ob;

	ob = malloc(len);

	if (byte_stream_getrawbuf(bs, ob, len) < len) {
		free(ob);
		return NULL;
	}

	return ob;
}

char *byte_stream_getstr(ByteStream *bs, int len)
{
	char *ob;

	ob = malloc(len + 1);

	if (byte_stream_getrawbuf(bs, (guint8 *)ob, len) < len) {
		free(ob);
		return NULL;
	}

	ob[len] = '\0';

	return ob;
}

int byte_stream_put8(ByteStream *bs, guint8 v)
{

	if (byte_stream_empty(bs) < 1)
		return 0; /* XXX throw an exception */

	bs->offset += aimutil_put8(bs->data + bs->offset, v);

	return 1;
}

int byte_stream_put16(ByteStream *bs, guint16 v)
{

	if (byte_stream_empty(bs) < 2)
		return 0; /* XXX throw an exception */

	bs->offset += aimutil_put16(bs->data + bs->offset, v);

	return 2;
}

int byte_stream_put32(ByteStream *bs, guint32 v)
{

	if (byte_stream_empty(bs) < 4)
		return 0; /* XXX throw an exception */

	bs->offset += aimutil_put32(bs->data + bs->offset, v);

	return 1;
}

int byte_stream_putle8(ByteStream *bs, guint8 v)
{

	if (byte_stream_empty(bs) < 1)
		return 0; /* XXX throw an exception */

	bs->offset += aimutil_putle8(bs->data + bs->offset, v);

	return 1;
}

int byte_stream_putle16(ByteStream *bs, guint16 v)
{

	if (byte_stream_empty(bs) < 2)
		return 0; /* XXX throw an exception */

	bs->offset += aimutil_putle16(bs->data + bs->offset, v);

	return 2;
}

int byte_stream_putle32(ByteStream *bs, guint32 v)
{

	if (byte_stream_empty(bs) < 4)
		return 0; /* XXX throw an exception */

	bs->offset += aimutil_putle32(bs->data + bs->offset, v);

	return 1;
}


int byte_stream_putraw(ByteStream *bs, const guint8 *v, int len)
{

	if (byte_stream_empty(bs) < len)
		return 0; /* XXX throw an exception */

	memcpy(bs->data + bs->offset, v, len);
	bs->offset += len;

	return len;
}

int byte_stream_putstr(ByteStream *bs, const char *str)
{
	return byte_stream_putraw(bs, (guint8 *)str, strlen(str));
}

int byte_stream_putbs(ByteStream *bs, ByteStream *srcbs, int len)
{

	if (byte_stream_empty(srcbs) < len)
		return 0; /* XXX throw exception (underrun) */

	if (byte_stream_empty(bs) < len)
		return 0; /* XXX throw exception (overflow) */

	memcpy(bs->data + bs->offset, srcbs->data + srcbs->offset, len);
	bs->offset += len;
	srcbs->offset += len;

	return len;
}
