/*
 * Family 0x0010 - Server stored buddy icons
 *
 * Used for storing and retrieving your cute little buddy icon 
 * from the AIM servers.
 *
 */

#define FAIM_INTERNAL
#include <aim.h>

/**
 * Subtype 0x0004 - Request someone's icon.
 *
 * @param sess The oscar session.
 * @param conn The icon connection for this session.
 * @return Return 0 if no errors, otherwise return the error number.
 */
faim_export int aim_icon_requesticon(aim_session_t *sess, const char *sn, const fu8_t *iconcsum, fu16_t iconcsumlen)
{
	aim_conn_t *conn;
	aim_frame_t *fr;
	aim_snacid_t snacid;

	if (!sess || !(conn = aim_conn_findbygroup(sess, 0x0010)) || !sn || !strlen(sn) || !iconcsum || !iconcsumlen)
		return -EINVAL;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10 + 1+strlen(sn) + 4 + 1+iconcsumlen)))
		return -ENOMEM;
	snacid = aim_cachesnac(sess, 0x0010, 0x0004, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x0010, 0x0004, 0x0000, snacid);

	/* Screen name */
	aimbs_put8(&fr->data, strlen(sn));
	aimbs_putraw(&fr->data, sn, strlen(sn));

	/* Some numbers.  You like numbers, right? */
	aimbs_put8(&fr->data, 0x01);
	aimbs_put16(&fr->data, 0x0001);
	aimbs_put8(&fr->data, 0x01);

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
static int parseicon(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
{
	aim_rxcallback_t userfunc;
	int i;
	char *sn;
	fu16_t flags, iconlen;
	fu8_t number, iconcsumlen, *iconcsum, *icon;

	sn = aimbs_getstr(bs, aimbs_get8(bs));
	flags = aimbs_get16(bs);
	number = aimbs_get8(bs);
	iconcsumlen = aimbs_get8(bs);
	iconcsum = aimbs_getraw(bs, iconcsumlen);
	iconlen = aimbs_get16(bs);
	icon = aimbs_getraw(bs, iconlen);

	if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
		return userfunc(sess, rx, sn, iconcsum, iconcsumlen, icon, iconlen);

	free(iconcsum);
	free(icon);

	return 0;
}

static int snachandler(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
{

	if (snac->subtype == 0x0005)
		return parseicon(sess, mod, rx, snac, bs);

	return 0;
}

faim_internal int icon_modfirst(aim_session_t *sess, aim_module_t *mod)
{

	mod->family = 0x0010;
	mod->version = 0x0001;
	mod->toolid = 0x0010;
	mod->toolversion = 0x0629;
	mod->flags = 0;
	strncpy(mod->name, "icon", sizeof(mod->name));
	mod->snachandler = snachandler;

	return 0;
}
