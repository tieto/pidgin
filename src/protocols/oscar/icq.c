/*
 * Encapsulated ICQ.
 *
 */

#define FAIM_INTERNAL
#include <aim.h>

/*
 * Response to 15/2, contains an ICQ packet.
 */
static int icqresponse(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
{
	int ret = 0;
	aim_rxcallback_t userfunc;

	if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
		ret = userfunc(sess, rx);

	return ret;
}

static int snachandler(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
{

	if (snac->subtype == 0x0003)
		return icqresponse(sess, mod, rx, snac, bs);

	return 0;
}

faim_internal int icq_modfirst(aim_session_t *sess, aim_module_t *mod)
{

	mod->family = 0x0015;
	mod->version = 0x0001;
	mod->toolid = 0x0110;
	mod->toolversion = 0x047b;
	mod->flags = 0;
	strncpy(mod->name, "icq", sizeof(mod->name));
	mod->snachandler = snachandler;

	return 0;
}


