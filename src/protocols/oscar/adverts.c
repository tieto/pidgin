/*
 *
 *
 */

#define FAIM_INTERNAL
#include <aim.h>

faim_export int aim_ads_clientready(aim_session_t *sess, aim_conn_t *conn)
{
	aim_frame_t *fr;
	aim_snacid_t snacid;
	
	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x0002, 0x1a)))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x0001, 0x0002, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x0001, 0x0002, 0x0000, snacid);

	aimbs_put16(&fr->data, 0x0001);
	aimbs_put16(&fr->data, 0x0002);

	aimbs_put16(&fr->data, 0x0001);
	aimbs_put16(&fr->data, 0x0013);

	aimbs_put16(&fr->data, 0x0005);
	aimbs_put16(&fr->data, 0x0001);
	aimbs_put16(&fr->data, 0x0001);
	aimbs_put16(&fr->data, 0x0001);

	aim_tx_enqueue(sess, fr);

	return 0;
}

faim_export int aim_ads_requestads(aim_session_t *sess, aim_conn_t *conn)
{
	return aim_genericreq_n(sess, conn, 0x0005, 0x0002);
}
