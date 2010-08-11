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
 * Family 0x0009 - Basic Oscar Service.
 *
 * The functionality of this family has been replaced by SSI.
 */

#include "oscar.h"

#include <string.h>

/* Subtype 0x0002 - Request BOS rights. */
void
aim_bos_reqrights(OscarData *od, FlapConnection *conn)
{
	aim_genericreq_n_snacid(od, conn, SNAC_FAMILY_BOS, 0x0002);
}

/* Subtype 0x0003 - BOS Rights. */
static int rights(OscarData *od, FlapConnection *conn, aim_module_t *mod, FlapFrame *frame, aim_modsnac_t *snac, ByteStream *bs)
{
	aim_rxcallback_t userfunc;
	GSList *tlvlist;
	guint16 maxpermits = 0, maxdenies = 0;
	int ret = 0;

	/*
	 * TLVs follow
	 */
	tlvlist = aim_tlvlist_read(bs);

	/*
	 * TLV type 0x0001: Maximum number of buddies on permit list.
	 */
	if (aim_tlv_gettlv(tlvlist, 0x0001, 1))
		maxpermits = aim_tlv_get16(tlvlist, 0x0001, 1);

	/*
	 * TLV type 0x0002: Maximum number of buddies on deny list.
	 */
	if (aim_tlv_gettlv(tlvlist, 0x0002, 1))
		maxdenies = aim_tlv_get16(tlvlist, 0x0002, 1);

	if ((userfunc = aim_callhandler(od, snac->family, snac->subtype)))
		ret = userfunc(od, conn, frame, maxpermits, maxdenies);

	aim_tlvlist_free(tlvlist);

	return ret;
}

/*
 * Subtype 0x0004 - Set group permission mask.
 *
 * Normally 0x1f (all classes).
 *
 * The group permission mask allows you to keep users of a certain
 * class or classes from talking to you.  The mask should be
 * a bitwise OR of all the user classes you want to see you.
 *
 */
void
aim_bos_setgroupperm(OscarData *od, FlapConnection *conn, guint32 mask)
{
	aim_genericreq_l(od, conn, SNAC_FAMILY_BOS, 0x0004, &mask);
}

/*
 * Stubtypes 0x0005, 0x0006, 0x0007, and 0x0008 - Modify permit/deny lists.
 *
 * Changes your visibility depending on changetype:
 *
 *  AIM_VISIBILITYCHANGE_PERMITADD: Lets provided list of names see you
 *  AIM_VISIBILITYCHANGE_PERMIDREMOVE: Removes listed names from permit list
 *  AIM_VISIBILITYCHANGE_DENYADD: Hides you from provided list of names
 *  AIM_VISIBILITYCHANGE_DENYREMOVE: Lets list see you again
 *
 * list should be a list of
 * screen names in the form "Screen Name One&ScreenNameTwo&" etc.
 *
 * Equivelents to options in WinAIM:
 *   - Allow all users to contact me: Send an AIM_VISIBILITYCHANGE_DENYADD
 *      with only your name on it.
 *   - Allow only users on my Buddy List: Send an
 *      AIM_VISIBILITYCHANGE_PERMITADD with the list the same as your
 *      buddy list
 *   - Allow only the uesrs below: Send an AIM_VISIBILITYCHANGE_PERMITADD
 *      with everyone listed that you want to see you.
 *   - Block all users: Send an AIM_VISIBILITYCHANGE_PERMITADD with only
 *      yourself in the list
 *   - Block the users below: Send an AIM_VISIBILITYCHANGE_DENYADD with
 *      the list of users to be blocked
 *
 * XXX ye gods.
 */
int aim_bos_changevisibility(OscarData *od, FlapConnection *conn, int changetype, const char *denylist)
{
	ByteStream bs;
	int packlen = 0;
	guint16 subtype;
	char *localcpy = NULL, *tmpptr = NULL;
	int i;
	int listcount;
	aim_snacid_t snacid;

	if (!denylist)
		return -EINVAL;

	if (changetype == AIM_VISIBILITYCHANGE_PERMITADD)
		subtype = 0x05;
	else if (changetype == AIM_VISIBILITYCHANGE_PERMITREMOVE)
		subtype = 0x06;
	else if (changetype == AIM_VISIBILITYCHANGE_DENYADD)
		subtype = 0x07;
	else if (changetype == AIM_VISIBILITYCHANGE_DENYREMOVE)
		subtype = 0x08;
	else
		return -EINVAL;

	localcpy = g_strdup(denylist);

	listcount = aimutil_itemcnt(localcpy, '&');
	packlen = aimutil_tokslen(localcpy, 99, '&') + listcount-1;

	byte_stream_new(&bs, packlen);

	for (i = 0; (i < (listcount - 1)) && (i < 99); i++) {
		tmpptr = aimutil_itemindex(localcpy, i, '&');

		byte_stream_put8(&bs, strlen(tmpptr));
		byte_stream_putstr(&bs, tmpptr);

		g_free(tmpptr);
	}
	g_free(localcpy);

	snacid = aim_cachesnac(od, SNAC_FAMILY_BOS, subtype, 0x0000, NULL, 0);
	flap_connection_send_snac(od, conn, SNAC_FAMILY_BOS, subtype, 0x0000, snacid, &bs);

	byte_stream_destroy(&bs);

	return 0;
}

static int
snachandler(OscarData *od, FlapConnection *conn, aim_module_t *mod, FlapFrame *frame, aim_modsnac_t *snac, ByteStream *bs)
{
	if (snac->subtype == 0x0003)
		return rights(od, conn, mod, frame, snac, bs);

	return 0;
}

int
bos_modfirst(OscarData *od, aim_module_t *mod)
{
	mod->family = SNAC_FAMILY_BOS;
	mod->version = 0x0001;
	mod->toolid = 0x0110;
	mod->toolversion = 0x0629;
	mod->flags = 0;
	strncpy(mod->name, "bos", sizeof(mod->name));
	mod->snachandler = snachandler;

	return 0;
}
