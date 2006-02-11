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

faim_internal int aim_bstream_init(aim_bstream_t *bs, guint8 *data, int len)
{

	if (!bs)
		return -1;

	bs->data = data;
	bs->len = len;
	bs->offset = 0;

	return 0;
}

faim_internal int aim_bstream_empty(aim_bstream_t *bs)
{
	return bs->len - bs->offset;
}

faim_internal int aim_bstream_curpos(aim_bstream_t *bs)
{
	return bs->offset;
}

faim_internal int aim_bstream_setpos(aim_bstream_t *bs, unsigned int off)
{

	if (off > bs->len)
		return -1;

	bs->offset = off;

	return off;
}

faim_internal void aim_bstream_rewind(aim_bstream_t *bs)
{

	aim_bstream_setpos(bs, 0);

	return;
}

/*
 * N can be negative, which can be used for going backwards
 * in a bstream.  I'm not sure if libfaim actually does
 * this anywhere...
 */
faim_internal int aim_bstream_advance(aim_bstream_t *bs, int n)
{

	if ((aim_bstream_curpos(bs) + n < 0) || (aim_bstream_empty(bs) < n))
		return 0; /* XXX throw an exception */

	bs->offset += n;

	return n;
}

faim_internal guint8 aimbs_get8(aim_bstream_t *bs)
{

	if (aim_bstream_empty(bs) < 1)
		return 0; /* XXX throw an exception */

	bs->offset++;

	return aimutil_get8(bs->data + bs->offset - 1);
}

faim_internal guint16 aimbs_get16(aim_bstream_t *bs)
{

	if (aim_bstream_empty(bs) < 2)
		return 0; /* XXX throw an exception */

	bs->offset += 2;

	return aimutil_get16(bs->data + bs->offset - 2);
}

faim_internal guint32 aimbs_get32(aim_bstream_t *bs)
{

	if (aim_bstream_empty(bs) < 4)
		return 0; /* XXX throw an exception */

	bs->offset += 4;

	return aimutil_get32(bs->data + bs->offset - 4);
}

faim_internal guint8 aimbs_getle8(aim_bstream_t *bs)
{

	if (aim_bstream_empty(bs) < 1)
		return 0; /* XXX throw an exception */

	bs->offset++;

	return aimutil_getle8(bs->data + bs->offset - 1);
}

faim_internal guint16 aimbs_getle16(aim_bstream_t *bs)
{

	if (aim_bstream_empty(bs) < 2)
		return 0; /* XXX throw an exception */

	bs->offset += 2;

	return aimutil_getle16(bs->data + bs->offset - 2);
}

faim_internal guint32 aimbs_getle32(aim_bstream_t *bs)
{

	if (aim_bstream_empty(bs) < 4)
		return 0; /* XXX throw an exception */

	bs->offset += 4;

	return aimutil_getle32(bs->data + bs->offset - 4);
}

faim_internal int aimbs_getrawbuf(aim_bstream_t *bs, guint8 *buf, int len)
{

	if (aim_bstream_empty(bs) < len)
		return 0;

	memcpy(buf, bs->data + bs->offset, len);
	bs->offset += len;

	return len;
}

faim_internal guint8 *aimbs_getraw(aim_bstream_t *bs, int len)
{
	guint8 *ob;

	if (!(ob = malloc(len)))
		return NULL;

	if (aimbs_getrawbuf(bs, ob, len) < len) {
		free(ob);
		return NULL;
	}

	return ob;
}

faim_internal char *aimbs_getstr(aim_bstream_t *bs, int len)
{
	char *ob;

	if (!(ob = malloc(len + 1)))
		return NULL;

	if (aimbs_getrawbuf(bs, (guint8 *)ob, len) < len) {
		free(ob);
		return NULL;
	}

	ob[len] = '\0';

	return ob;
}

faim_internal int aimbs_put8(aim_bstream_t *bs, guint8 v)
{

	if (aim_bstream_empty(bs) < 1)
		return 0; /* XXX throw an exception */

	bs->offset += aimutil_put8(bs->data + bs->offset, v);

	return 1;
}

faim_internal int aimbs_put16(aim_bstream_t *bs, guint16 v)
{

	if (aim_bstream_empty(bs) < 2)
		return 0; /* XXX throw an exception */

	bs->offset += aimutil_put16(bs->data + bs->offset, v);

	return 2;
}

faim_internal int aimbs_put32(aim_bstream_t *bs, guint32 v)
{

	if (aim_bstream_empty(bs) < 4)
		return 0; /* XXX throw an exception */

	bs->offset += aimutil_put32(bs->data + bs->offset, v);

	return 1;
}

faim_internal int aimbs_putle8(aim_bstream_t *bs, guint8 v)
{

	if (aim_bstream_empty(bs) < 1)
		return 0; /* XXX throw an exception */

	bs->offset += aimutil_putle8(bs->data + bs->offset, v);

	return 1;
}

faim_internal int aimbs_putle16(aim_bstream_t *bs, guint16 v)
{

	if (aim_bstream_empty(bs) < 2)
		return 0; /* XXX throw an exception */

	bs->offset += aimutil_putle16(bs->data + bs->offset, v);

	return 2;
}

faim_internal int aimbs_putle32(aim_bstream_t *bs, guint32 v)
{

	if (aim_bstream_empty(bs) < 4)
		return 0; /* XXX throw an exception */

	bs->offset += aimutil_putle32(bs->data + bs->offset, v);

	return 1;
}


faim_internal int aimbs_putraw(aim_bstream_t *bs, const guint8 *v, int len)
{

	if (aim_bstream_empty(bs) < len)
		return 0; /* XXX throw an exception */

	memcpy(bs->data + bs->offset, v, len);
	bs->offset += len;

	return len;
}

faim_internal int aimbs_putstr(aim_bstream_t *bs, const char *str)
{
	return aimbs_putraw(bs, (guint8 *)str, strlen(str));
}

faim_internal int aimbs_putbs(aim_bstream_t *bs, aim_bstream_t *srcbs, int len)
{

	if (aim_bstream_empty(srcbs) < len)
		return 0; /* XXX throw exception (underrun) */

	if (aim_bstream_empty(bs) < len)
		return 0; /* XXX throw exception (overflow) */

	memcpy(bs->data + bs->offset, srcbs->data + srcbs->offset, len);
	bs->offset += len;
	srcbs->offset += len;

	return len;
}
