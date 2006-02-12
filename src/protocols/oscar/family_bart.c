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
 * Family 0x0010 - Server stored buddy art
 *
 * Used for storing and retrieving your cute little buddy icon
 * from the AIM servers.
 *
 */

#include "oscar.h"

/**
 * Subtype 0x0002 - Upload your icon.
 *
 * @param sess The oscar session.
 * @param conn The icon connection for this session.
 * @param icon The raw data of the icon image file.
 * @param iconlen Length of the raw data of the icon image file.
 * @return Return 0 if no errors, otherwise return the error number.
 */
faim_export int aim_bart_upload(OscarSession *sess, const guint8 *icon, guint16 iconlen)
{
	OscarConnection *conn;
	FlapFrame *fr;
	aim_snacid_t snacid;

	if (!sess || !(conn = aim_conn_findbygroup(sess, 0x0010)) || !icon || !iconlen)
		return -EINVAL;

	if (!(fr = flap_frame_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10 + 2 + 2+iconlen)))
		return -ENOMEM;
	snacid = aim_cachesnac(sess, 0x0010, 0x0002, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x0010, 0x0002, 0x0000, snacid);

	/* The reference number for the icon */
	aimbs_put16(&fr->data, 1);

	/* The icon */
	aimbs_put16(&fr->data, iconlen);
	aimbs_putraw(&fr->data, icon, iconlen);

	aim_tx_enqueue(sess, fr);

	return 0;
}

/**
 * Subtype 0x0003 - Acknowledgement for uploading a buddy icon.
 *
 * You get this honky after you upload a buddy icon.
 */
static int uploadack(OscarSession *sess, aim_module_t *mod, FlapFrame *rx, aim_modsnac_t *snac, ByteStream *bs)
{
	int ret = 0;
	aim_rxcallback_t userfunc;
	guint16 something, somethingelse;
	guint8 onemorething;

	something = aimbs_get16(bs);
	somethingelse = aimbs_get16(bs);
	onemorething = aimbs_get8(bs);

	if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
		ret = userfunc(sess, rx);

	return ret;
}

/**
 * Subtype 0x0004 - Request someone's icon.
 *
 * @param sess The oscar session.
 * @param conn The icon connection for this session.
 * @param sn The screen name of the person who's icon you are requesting.
 * @param iconcsum The MD5 checksum of the icon you are requesting.
 * @param iconcsumlen Length of the MD5 checksum given above.  Should be 10 bytes.
 * @return Return 0 if no errors, otherwise return the error number.
 */
faim_export int aim_bart_request(OscarSession *sess, const char *sn, guint8 iconcsumtype, const guint8 *iconcsum, guint16 iconcsumlen)
{
	OscarConnection *conn;
	FlapFrame *fr;
	aim_snacid_t snacid;

	if (!sess || !(conn = aim_conn_findbygroup(sess, 0x0010)) || !sn || !strlen(sn) || !iconcsum || !iconcsumlen)
		return -EINVAL;

	if (!(fr = flap_frame_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10 + 1+strlen(sn) + 4 + 1+iconcsumlen)))
		return -ENOMEM;
	snacid = aim_cachesnac(sess, 0x0010, 0x0004, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x0010, 0x0004, 0x0000, snacid);

	/* Screen name */
	aimbs_put8(&fr->data, strlen(sn));
	aimbs_putstr(&fr->data, sn);

	/* Some numbers.  You like numbers, right? */
	aimbs_put8(&fr->data, 0x01);
	aimbs_put16(&fr->data, 0x0001);
	aimbs_put8(&fr->data, iconcsumtype);

	/* Icon string */
	aimbs_put8(&fr->data, iconcsumlen);
	aimbs_putraw(&fr->data, iconcsum, iconcsumlen);

	aim_tx_enqueue(sess, fr);

	return 0;
}

/**
 * Subtype 0x0005 - Receive a buddy icon.
 *
 * This is sent in response to a buddy icon request.
 */
static int parseicon(OscarSession *sess, aim_module_t *mod, FlapFrame *rx, aim_modsnac_t *snac, ByteStream *bs)
{
	int ret = 0;
	aim_rxcallback_t userfunc;
	char *sn;
	guint16 flags, iconlen;
	guint8 iconcsumtype, iconcsumlen, *iconcsum, *icon;

	sn = aimbs_getstr(bs, aimbs_get8(bs));
	flags = aimbs_get16(bs);
	iconcsumtype = aimbs_get8(bs);
	iconcsumlen = aimbs_get8(bs);
	iconcsum = aimbs_getraw(bs, iconcsumlen);
	iconlen = aimbs_get16(bs);
	icon = aimbs_getraw(bs, iconlen);

	if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
		ret = userfunc(sess, rx, sn, iconcsumtype, iconcsum, iconcsumlen, icon, iconlen);

	free(sn);
	free(iconcsum);
	free(icon);

	return ret;
}

static int snachandler(OscarSession *sess, aim_module_t *mod, FlapFrame *rx, aim_modsnac_t *snac, ByteStream *bs)
{

	if (snac->subtype == 0x0003)
		return uploadack(sess, mod, rx, snac, bs);
	else if (snac->subtype == 0x0005)
		return parseicon(sess, mod, rx, snac, bs);

	return 0;
}

faim_internal int bart_modfirst(OscarSession *sess, aim_module_t *mod)
{

	mod->family = 0x0010;
	mod->version = 0x0001;
	mod->toolid = 0x0010;
	mod->toolversion = 0x0629;
	mod->flags = 0;
	strncpy(mod->name, "bart", sizeof(mod->name));
	mod->snachandler = snachandler;

	return 0;
}
