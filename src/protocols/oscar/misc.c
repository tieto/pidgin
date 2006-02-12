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
 * misc.c
 *
 * Random stuff.  Basically just a few functions for sending
 * simple SNACs, and then the generic error handler.
 *
 */

#include "oscar.h"

/*
 * Generic routine for sending commands.
 *
 * I know I can do this in a smarter way...but I'm not thinking straight
 * right now...
 *
 * I had one big function that handled all three cases, but then it broke
 * and I split it up into three.  But then I fixed it.  I just never went
 * back to the single.  I don't see any advantage to doing it either way.
 *
 */
faim_internal int aim_genericreq_n(OscarSession *sess, OscarConnection *conn, guint16 family, guint16 subtype)
{
	FlapFrame *fr;
	aim_snacid_t snacid = 0x00000000;

	if (!(fr = flap_frame_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10)))
		return -ENOMEM;

	aim_putsnac(&fr->data, family, subtype, 0x0000, snacid);

	aim_tx_enqueue(sess, fr);

	return 0;
}

faim_internal int aim_genericreq_n_snacid(OscarSession *sess, OscarConnection *conn, guint16 family, guint16 subtype)
{
	FlapFrame *fr;
	aim_snacid_t snacid;

	if (!(fr = flap_frame_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10)))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, family, subtype, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, family, subtype, 0x0000, snacid);

	aim_tx_enqueue(sess, fr);

	return 0;
}

faim_internal int aim_genericreq_l(OscarSession *sess, OscarConnection *conn, guint16 family, guint16 subtype, guint32 *longdata)
{
	FlapFrame *fr;
	aim_snacid_t snacid;

	if (!longdata)
		return aim_genericreq_n(sess, conn, family, subtype);

	if (!(fr = flap_frame_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10+4)))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, family, subtype, 0x0000, NULL, 0);

	aim_putsnac(&fr->data, family, subtype, 0x0000, snacid);
	aimbs_put32(&fr->data, *longdata);

	aim_tx_enqueue(sess, fr);

	return 0;
}

faim_internal int aim_genericreq_s(OscarSession *sess, OscarConnection *conn, guint16 family, guint16 subtype, guint16 *shortdata)
{
	FlapFrame *fr;
	aim_snacid_t snacid;

	if (!shortdata)
		return aim_genericreq_n(sess, conn, family, subtype);

	if (!(fr = flap_frame_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10+2)))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, family, subtype, 0x0000, NULL, 0);

	aim_putsnac(&fr->data, family, subtype, 0x0000, snacid);
	aimbs_put16(&fr->data, *shortdata);

	aim_tx_enqueue(sess, fr);

	return 0;
}

/*
 * Should be generic enough to handle the errors for all groups.
 *
 */
static int generror(OscarSession *sess, aim_module_t *mod, FlapFrame *rx, aim_modsnac_t *snac, ByteStream *bs)
{
	int ret = 0;
	int error = 0;
	aim_rxcallback_t userfunc;
	aim_snac_t *snac2;

	snac2 = aim_remsnac(sess, snac->id);

	if (aim_bstream_empty(bs))
		error = aimbs_get16(bs);

	if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
		ret = userfunc(sess, rx, error, snac2 ? snac2->data : NULL);

	if (snac2)
		free(snac2->data);
	free(snac2);

	return ret;
}

static int snachandler(OscarSession *sess, aim_module_t *mod, FlapFrame *rx, aim_modsnac_t *snac, ByteStream *bs)
{

	if (snac->subtype == 0x0001)
		return generror(sess, mod, rx, snac, bs);
	else if ((snac->family == 0xffff) && (snac->subtype == 0xffff)) {
		aim_rxcallback_t userfunc;

		if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
			return userfunc(sess, rx);
	}

	return 0;
}

faim_internal int misc_modfirst(OscarSession *sess, aim_module_t *mod)
{

	mod->family = 0xffff;
	mod->version = 0x0000;
	mod->flags = AIM_MODFLAG_MULTIFAMILY;
	strncpy(mod->name, "misc", sizeof(mod->name));
	mod->snachandler = snachandler;

	return 0;
}
