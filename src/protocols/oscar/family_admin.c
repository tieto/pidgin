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
 * Family 0x0007 - Account Administration.
 *
 * Used for stuff like changing the formating of your screen name, changing your
 * email address, requesting an account confirmation email, getting account info,
 *
 */

#include "oscar.h"

/*
 * Subtype 0x0002 - Request a bit of account info.
 *
 * Info should be one of the following:
 * 0x0001 - Screen name formatting
 * 0x0011 - Email address
 * 0x0013 - Unknown
 *
 */
faim_export int aim_admin_getinfo(OscarSession *sess, OscarConnection *conn, guint16 info)
{
	FlapFrame *fr;
	aim_snacid_t snacid;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 14)))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x0007, 0x0002, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x0007, 0x0002, 0x0000, snacid);

	aimbs_put16(&fr->data, info);
	aimbs_put16(&fr->data, 0x0000);

	aim_tx_enqueue(sess, fr);

	return 0;
}

/*
 * Subtypes 0x0003 and 0x0005 - Parse account info.
 *
 * Called in reply to both an information request (subtype 0x0002) and
 * an information change (subtype 0x0004).
 *
 */
static int infochange(OscarSession *sess, aim_module_t *mod, FlapFrame *rx, aim_modsnac_t *snac, ByteStream *bs)
{
	aim_rxcallback_t userfunc;
	char *url=NULL, *sn=NULL, *email=NULL;
	guint16 perms, tlvcount, err=0;

	perms = aimbs_get16(bs);
	tlvcount = aimbs_get16(bs);

	while (tlvcount && aim_bstream_empty(bs)) {
		guint16 type, length;

		type = aimbs_get16(bs);
		length = aimbs_get16(bs);

		switch (type) {
			case 0x0001: {
				sn = aimbs_getstr(bs, length);
			} break;

			case 0x0004: {
				url = aimbs_getstr(bs, length);
			} break;

			case 0x0008: {
				err = aimbs_get16(bs);
			} break;

			case 0x0011: {
				if (length == 0) {
					email = (char*)malloc(13*sizeof(char));
					strcpy(email, "*suppressed*");
				} else
					email = aimbs_getstr(bs, length);
			} break;
		}

		tlvcount--;
	}

	if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
		userfunc(sess, rx, (snac->subtype == 0x0005) ? 1 : 0, perms, err, url, sn, email);

	free(sn);
	free(url);
	free(email);

	return 1;
}

/*
 * Subtype 0x0004 - Set screenname formatting.
 *
 */
faim_export int aim_admin_setnick(OscarSession *sess, OscarConnection *conn, const char *newnick)
{
	FlapFrame *fr;
	aim_snacid_t snacid;
	aim_tlvlist_t *tl = NULL;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10+2+2+strlen(newnick))))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x0007, 0x0004, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x0007, 0x0004, 0x0000, snacid);

	aim_tlvlist_add_str(&tl, 0x0001, newnick);

	aim_tlvlist_write(&fr->data, &tl);
	aim_tlvlist_free(&tl);

	aim_tx_enqueue(sess, fr);


	return 0;
}

/*
 * Subtype 0x0004 - Change password.
 *
 */
faim_export int aim_admin_changepasswd(OscarSession *sess, OscarConnection *conn, const char *newpw, const char *curpw)
{
	FlapFrame *fr;
	aim_tlvlist_t *tl = NULL;
	aim_snacid_t snacid;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10+4+strlen(curpw)+4+strlen(newpw))))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x0007, 0x0004, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x0007, 0x0004, 0x0000, snacid);

	/* new password TLV t(0002) */
	aim_tlvlist_add_str(&tl, 0x0002, newpw);

	/* current password TLV t(0012) */
	aim_tlvlist_add_str(&tl, 0x0012, curpw);

	aim_tlvlist_write(&fr->data, &tl);
	aim_tlvlist_free(&tl);

	aim_tx_enqueue(sess, fr);

	return 0;
}

/*
 * Subtype 0x0004 - Change email address.
 *
 */
faim_export int aim_admin_setemail(OscarSession *sess, OscarConnection *conn, const char *newemail)
{
	FlapFrame *fr;
	aim_snacid_t snacid;
	aim_tlvlist_t *tl = NULL;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10+2+2+strlen(newemail))))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x0007, 0x0004, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x0007, 0x0004, 0x0000, snacid);

	aim_tlvlist_add_str(&tl, 0x0011, newemail);

	aim_tlvlist_write(&fr->data, &tl);
	aim_tlvlist_free(&tl);

	aim_tx_enqueue(sess, fr);

	return 0;
}

/*
 * Subtype 0x0006 - Request account confirmation.
 *
 * This will cause an email to be sent to the address associated with
 * the account.  By following the instructions in the mail, you can
 * get the TRIAL flag removed from your account.
 *
 */
faim_export int aim_admin_reqconfirm(OscarSession *sess, OscarConnection *conn)
{
	return aim_genericreq_n(sess, conn, 0x0007, 0x0006);
}

/*
 * Subtype 0x0007 - Account confirmation request acknowledgement.
 *
 */
static int accountconfirm(OscarSession *sess, aim_module_t *mod, FlapFrame *rx, aim_modsnac_t *snac, ByteStream *bs)
{
	int ret = 0;
	aim_rxcallback_t userfunc;
	guint16 status;
	aim_tlvlist_t *tl;

	status = aimbs_get16(bs);
	/* This is 0x0013 if unable to confirm at this time */

	tl = aim_tlvlist_read(bs);

	if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
		ret = userfunc(sess, rx, status);

	return ret;
}

static int snachandler(OscarSession *sess, aim_module_t *mod, FlapFrame *rx, aim_modsnac_t *snac, ByteStream *bs)
{

	if ((snac->subtype == 0x0003) || (snac->subtype == 0x0005))
		return infochange(sess, mod, rx, snac, bs);
	else if (snac->subtype == 0x0007)
		return accountconfirm(sess, mod, rx, snac, bs);

	return 0;
}

faim_internal int admin_modfirst(OscarSession *sess, aim_module_t *mod)
{

	mod->family = 0x0007;
	mod->version = 0x0001;
	mod->toolid = 0x0010;
	mod->toolversion = 0x0629;
	mod->flags = 0;
	strncpy(mod->name, "admin", sizeof(mod->name));
	mod->snachandler = snachandler;

	return 0;
}
