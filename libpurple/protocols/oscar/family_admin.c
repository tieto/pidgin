/*
 * Purple's oscar protocol plugin
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
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
int
aim_admin_getinfo(OscarData *od, FlapConnection *conn, guint16 info)
{
	FlapFrame *fr;
	aim_snacid_t snacid;

	fr = flap_frame_new(od, 0x02, 14);

	snacid = aim_cachesnac(od, 0x0007, 0x0002, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x0007, 0x0002, 0x0000, snacid);

	byte_stream_put16(&fr->data, info);
	byte_stream_put16(&fr->data, 0x0000);

	flap_connection_send(conn, fr);

	return 0;
}

/*
 * Subtypes 0x0003 and 0x0005 - Parse account info.
 *
 * Called in reply to both an information request (subtype 0x0002) and
 * an information change (subtype 0x0004).
 *
 */
static int
infochange(OscarData *od, FlapConnection *conn, aim_module_t *mod, FlapFrame *frame, aim_modsnac_t *snac, ByteStream *bs)
{
	aim_rxcallback_t userfunc;
	char *url=NULL, *sn=NULL, *email=NULL;
	guint16 perms, tlvcount, err=0;

	perms = byte_stream_get16(bs);
	tlvcount = byte_stream_get16(bs);

	while (tlvcount && byte_stream_empty(bs)) {
		guint16 type, length;

		type = byte_stream_get16(bs);
		length = byte_stream_get16(bs);

		switch (type) {
			case 0x0001: {
				g_free(sn);
				sn = byte_stream_getstr(bs, length);
			} break;

			case 0x0004: {
				g_free(url);
				url = byte_stream_getstr(bs, length);
			} break;

			case 0x0008: {
				err = byte_stream_get16(bs);
			} break;

			case 0x0011: {
				g_free(email);
				if (length == 0)
					email = g_strdup("*suppressed");
				else
					email = byte_stream_getstr(bs, length);
			} break;
		}

		tlvcount--;
	}

	if ((userfunc = aim_callhandler(od, snac->family, snac->subtype)))
		userfunc(od, conn, frame, (snac->subtype == 0x0005) ? 1 : 0, perms, err, url, sn, email);

	g_free(sn);
	g_free(url);
	g_free(email);

	return 1;
}

/*
 * Subtype 0x0004 - Set screenname formatting.
 *
 */
int
aim_admin_setnick(OscarData *od, FlapConnection *conn, const char *newnick)
{
	FlapFrame *fr;
	aim_snacid_t snacid;
	GSList *tlvlist = NULL;

	fr = flap_frame_new(od, 0x02, 10+2+2+strlen(newnick));

	snacid = aim_cachesnac(od, 0x0007, 0x0004, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x0007, 0x0004, 0x0000, snacid);

	aim_tlvlist_add_str(&tlvlist, 0x0001, newnick);

	aim_tlvlist_write(&fr->data, &tlvlist);
	aim_tlvlist_free(tlvlist);

	flap_connection_send(conn, fr);


	return 0;
}

/*
 * Subtype 0x0004 - Change password.
 *
 */
int
aim_admin_changepasswd(OscarData *od, FlapConnection *conn, const char *newpw, const char *curpw)
{
	FlapFrame *fr;
	GSList *tlvlist = NULL;
	aim_snacid_t snacid;

	fr = flap_frame_new(od, 0x02, 10+4+strlen(curpw)+4+strlen(newpw));

	snacid = aim_cachesnac(od, 0x0007, 0x0004, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x0007, 0x0004, 0x0000, snacid);

	/* new password TLV t(0002) */
	aim_tlvlist_add_str(&tlvlist, 0x0002, newpw);

	/* current password TLV t(0012) */
	aim_tlvlist_add_str(&tlvlist, 0x0012, curpw);

	aim_tlvlist_write(&fr->data, &tlvlist);
	aim_tlvlist_free(tlvlist);

	flap_connection_send(conn, fr);

	return 0;
}

/*
 * Subtype 0x0004 - Change email address.
 *
 */
int
aim_admin_setemail(OscarData *od, FlapConnection *conn, const char *newemail)
{
	FlapFrame *fr;
	aim_snacid_t snacid;
	GSList *tlvlist = NULL;

	fr = flap_frame_new(od, 0x02, 10+2+2+strlen(newemail));

	snacid = aim_cachesnac(od, 0x0007, 0x0004, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x0007, 0x0004, 0x0000, snacid);

	aim_tlvlist_add_str(&tlvlist, 0x0011, newemail);

	aim_tlvlist_write(&fr->data, &tlvlist);
	aim_tlvlist_free(tlvlist);

	flap_connection_send(conn, fr);

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
void
aim_admin_reqconfirm(OscarData *od, FlapConnection *conn)
{
	aim_genericreq_n(od, conn, 0x0007, 0x0006);
}

/*
 * Subtype 0x0007 - Account confirmation request acknowledgement.
 *
 */
static int
accountconfirm(OscarData *od, FlapConnection *conn, aim_module_t *mod, FlapFrame *frame, aim_modsnac_t *snac, ByteStream *bs)
{
	int ret = 0;
	aim_rxcallback_t userfunc;
	guint16 status;
	/* GSList *tlvlist; */

	status = byte_stream_get16(bs);
	/* Status is 0x0013 if unable to confirm at this time */

	/* tlvlist = aim_tlvlist_read(bs); */

	if ((userfunc = aim_callhandler(od, snac->family, snac->subtype)))
		ret = userfunc(od, conn, frame, status);

	/* aim_tlvlist_free(tlvlist); */

	return ret;
}

static int
snachandler(OscarData *od, FlapConnection *conn, aim_module_t *mod, FlapFrame *frame, aim_modsnac_t *snac, ByteStream *bs)
{
	if ((snac->subtype == 0x0003) || (snac->subtype == 0x0005))
		return infochange(od, conn, mod, frame, snac, bs);
	else if (snac->subtype == 0x0007)
		return accountconfirm(od, conn, mod, frame, snac, bs);

	return 0;
}

int admin_modfirst(OscarData *od, aim_module_t *mod)
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
