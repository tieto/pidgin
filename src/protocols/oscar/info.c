/*
 * aim_info.c
 *
 * The functions here are responsible for requesting and parsing information-
 * gathering SNACs.  Or something like that. 
 *
 */

#define FAIM_INTERNAL
#include <aim.h>

struct aim_priv_inforeq {
	char sn[MAXSNLEN+1];
	fu16_t infotype;
};

faim_export int aim_getinfo(aim_session_t *sess, aim_conn_t *conn, const char *sn, fu16_t infotype)
{
	struct aim_priv_inforeq privdata;
	aim_frame_t *fr;
	aim_snacid_t snacid;

	if (!sess || !conn || !sn)
		return -EINVAL;

	if ((infotype != AIM_GETINFO_GENERALINFO) && (infotype != AIM_GETINFO_AWAYMESSAGE))
		return -EINVAL;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 12+1+strlen(sn))))
		return -ENOMEM;

	strncpy(privdata.sn, sn, sizeof(privdata.sn));
	privdata.infotype = infotype;
	snacid = aim_cachesnac(sess, 0x0002, 0x0005, 0x0000, &privdata, sizeof(struct aim_priv_inforeq));
	
	aim_putsnac(&fr->data, 0x0002, 0x0005, 0x0000, snacid);
	aimbs_put16(&fr->data, infotype);
	aimbs_put8(&fr->data, strlen(sn));
	aimbs_putraw(&fr->data, sn, strlen(sn));

	aim_tx_enqueue(sess, fr);

	return 0;
}

faim_export const char *aim_userinfo_sn(aim_userinfo_t *ui)
{

	if (!ui)
		return NULL;

	return ui->sn;
}

faim_export fu16_t aim_userinfo_flags(aim_userinfo_t *ui)
{

	if (!ui)
		return 0;

	return ui->flags;
}

faim_export fu16_t aim_userinfo_idle(aim_userinfo_t *ui)
{

	if (!ui)
		return 0;

	return ui->idletime;
}

faim_export float aim_userinfo_warnlevel(aim_userinfo_t *ui)
{

	if (!ui)
		return 0.00;

	return (ui->warnlevel / 10);
}

faim_export time_t aim_userinfo_membersince(aim_userinfo_t *ui)
{

	if (!ui)
		return 0;

	return (time_t)ui->membersince;
}

faim_export time_t aim_userinfo_onlinesince(aim_userinfo_t *ui)
{

	if (!ui)
		return 0;

	return (time_t)ui->onlinesince;
}

faim_export fu32_t aim_userinfo_sessionlen(aim_userinfo_t *ui)
{

	if (!ui)
		return 0;

	return ui->sessionlen;
}

faim_export int aim_userinfo_hascap(aim_userinfo_t *ui, fu16_t cap)
{

	if (!ui || !ui->capspresent)
		return -1;

	return !!(ui->capabilities & cap);
}


/*
 * Capability blocks.  
 */
static const struct {
	unsigned short flag;
	unsigned char data[16];
} aim_caps[] = {

	/*
	 * Chat is oddball.
	 */
	{AIM_CAPS_CHAT,
	 {0x74, 0x8f, 0x24, 0x20, 0x62, 0x87, 0x11, 0xd1, 
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	/*
	 * These are mostly in order.
	 */
	{AIM_CAPS_VOICE,
	 {0x09, 0x46, 0x13, 0x41, 0x4c, 0x7f, 0x11, 0xd1, 
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{AIM_CAPS_SENDFILE,
	 {0x09, 0x46, 0x13, 0x43, 0x4c, 0x7f, 0x11, 0xd1, 
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	/*
	 * Advertised by the EveryBuddy client.
	 */
	{AIM_CAPS_EVERYBUDDY,
	 {0x09, 0x46, 0x13, 0x44, 0x4c, 0x7f, 0x11, 0xd1, 
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{AIM_CAPS_IMIMAGE,
	 {0x09, 0x46, 0x13, 0x45, 0x4c, 0x7f, 0x11, 0xd1, 
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{AIM_CAPS_BUDDYICON,
	 {0x09, 0x46, 0x13, 0x46, 0x4c, 0x7f, 0x11, 0xd1, 
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{AIM_CAPS_SAVESTOCKS,
	 {0x09, 0x46, 0x13, 0x47, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{AIM_CAPS_GETFILE,
	 {0x09, 0x46, 0x13, 0x48, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	/*
	 * Indeed, there are two of these.  The former appears to be correct, 
	 * but in some versions of winaim, the second one is set.  Either they 
	 * forgot to fix endianness, or they made a typo. It really doesn't 
	 * matter which.
	 */
	{AIM_CAPS_GAMES,
	 {0x09, 0x46, 0x13, 0x4a, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},
	{AIM_CAPS_GAMES2,
	 {0x09, 0x46, 0x13, 0x4a, 0x4c, 0x7f, 0x11, 0xd1,
	  0x22, 0x82, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{AIM_CAPS_SENDBUDDYLIST,
	 {0x09, 0x46, 0x13, 0x4b, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{AIM_CAPS_LAST}
};

/*
 * This still takes a length parameter even with a bstream because capabilities
 * are not naturally bounded.
 * 
 */
faim_internal fu16_t aim_getcap(aim_session_t *sess, aim_bstream_t *bs, int len)
{
	fu16_t flags = 0;
	int offset;

	for (offset = 0; aim_bstream_empty(bs) && (offset < len); offset += 0x10) {
		fu8_t *cap;
		int i, identified;

		cap = aimbs_getraw(bs, 0x10);

		for (i = 0, identified = 0; !(aim_caps[i].flag & AIM_CAPS_LAST); i++) {

			if (memcmp(&aim_caps[i].data, cap, 0x10) == 0) {
				flags |= aim_caps[i].flag;
				identified++;
				break; /* should only match once... */

			}
		}

		if (!identified)
			faimdprintf(sess, 0, "unknown capability!\n");

		free(cap);
	}

	return flags;
}

faim_internal int aim_putcap(aim_bstream_t *bs, fu16_t caps)
{
	int i;

	if (!bs)
		return -EINVAL;

	for (i = 0; aim_bstream_empty(bs); i++) {

		if (aim_caps[i].flag == AIM_CAPS_LAST)
			break;

		if (caps & aim_caps[i].flag)
			aimbs_putraw(bs, aim_caps[i].data, 0x10);

	}

	return 0;
}

/*
 * AIM is fairly regular about providing user info.  This is a generic 
 * routine to extract it in its standard form.
 */
faim_internal int aim_extractuserinfo(aim_session_t *sess, aim_bstream_t *bs, aim_userinfo_t *outinfo)
{
	int curtlv, tlvcnt;
	fu8_t snlen;

	if (!bs || !outinfo)
		return -EINVAL;

	/* Clear out old data first */
	memset(outinfo, 0x00, sizeof(aim_userinfo_t));

	/*
	 * Screen name.  Stored as an unterminated string prepended with a 
	 * byte containing its length.
	 */
	snlen = aimbs_get8(bs);
	aimbs_getrawbuf(bs, outinfo->sn, snlen);

	/*
	 * Warning Level.  Stored as an unsigned short.
	 */
	outinfo->warnlevel = aimbs_get16(bs);

	/*
	 * TLV Count. Unsigned short representing the number of 
	 * Type-Length-Value triples that follow.
	 */
	tlvcnt = aimbs_get16(bs);

	/* 
	 * Parse out the Type-Length-Value triples as they're found.
	 */
	for (curtlv = 0; curtlv < tlvcnt; curtlv++) {
		int endpos;
		fu16_t type, length;

		type = aimbs_get16(bs);
		length = aimbs_get16(bs);

		endpos = aim_bstream_curpos(bs) + length;

		if (type == 0x0001) {
			/*
			 * Type = 0x0001: User flags
			 * 
			 * Specified as any of the following ORed together:
			 *      0x0001  Trial (user less than 60days)
			 *      0x0002  Unknown bit 2
			 *      0x0004  AOL Main Service user
			 *      0x0008  Unknown bit 4
			 *      0x0010  Free (AIM) user 
			 *      0x0020  Away
			 *      0x0400  ActiveBuddy
			 *
			 */
			outinfo->flags = aimbs_get16(bs);

		} else if (type == 0x0002) {
			/*
			 * Type = 0x0002: Member-Since date. 
			 *
			 * The time/date that the user originally registered for
			 * the service, stored in time_t format.
			 */
			outinfo->membersince = aimbs_get32(bs);

		} else if (type == 0x0003) {
			/*
			 * Type = 0x0003: On-Since date.
			 *
			 * The time/date that the user started their current 
			 * session, stored in time_t format.
			 */
			outinfo->onlinesince = aimbs_get32(bs);

		} else if (type == 0x0004) {
			/*
			 * Type = 0x0004: Idle time.
			 *
			 * Number of seconds since the user actively used the 
			 * service.
			 *
			 * Note that the client tells the server when to start
			 * counting idle times, so this may or may not be 
			 * related to reality.
			 */
			outinfo->idletime = aimbs_get16(bs);

		} else if (type == 0x0006) {
			/*
			 * Type = 0x0006: ICQ Online Status
			 *
			 * ICQ's Away/DND/etc "enriched" status. Some decoding 
			 * of values done by Scott <darkagl@pcnet.com>
			 */
			aimbs_get16(bs);
			outinfo->icqinfo.status = aimbs_get16(bs);

		} else if (type == 0x000a) {
			/*
			 * Type = 0x000a
			 *
			 * ICQ User IP Address.
			 * Ahh, the joy of ICQ security.
			 */
			outinfo->icqinfo.ipaddr = aimbs_get32(bs);

		} else if (type == 0x000c) {
			/* 
			 * Type = 0x000c
			 *
			 * random crap containing the IP address,
			 * apparently a port number, and some Other Stuff.
			 *
			 */
			aimbs_getrawbuf(bs, outinfo->icqinfo.crap, 0x25);

		} else if (type == 0x000d) {
			/*
			 * Type = 0x000d
			 *
			 * Capability information.
			 *
			 */
			outinfo->capabilities = aim_getcap(sess, bs, length);
			outinfo->capspresent = 1;

		} else if (type == 0x000e) {
			/*
			 * Type = 0x000e
			 *
			 * Unknown.  Always of zero length, and always only
			 * on AOL users.
			 *
			 * Ignore.
			 *
			 */

		} else if ((type == 0x000f) || (type == 0x0010)) {
			/*
			 * Type = 0x000f: Session Length. (AIM)
			 * Type = 0x0010: Session Length. (AOL)
			 *
			 * The duration, in seconds, of the user's current 
			 * session.
			 *
			 * Which TLV type this comes in depends on the
			 * service the user is using (AIM or AOL).
			 *
			 */
			outinfo->sessionlen = aimbs_get32(bs);

		} else {

			/*
			 * Reaching here indicates that either AOL has
			 * added yet another TLV for us to deal with, 
			 * or the parsing has gone Terribly Wrong.
			 *
			 * Either way, inform the owner and attempt
			 * recovery.
			 *
			 */
			faimdprintf(sess, 0, "userinfo: **warning: unexpected TLV:\n");
			faimdprintf(sess, 0, "userinfo:   sn    =%s\n", outinfo->sn);
			faimdprintf(sess, 0, "userinfo:   type  =0x%04x\n",type);
			faimdprintf(sess, 0, "userinfo:   length=0x%04x\n", length);

		}

		/* Save ourselves. */
		aim_bstream_setpos(bs, endpos);
	}

	return 0;
}

/*
 * Inverse of aim_extractuserinfo()
 */
faim_internal int aim_putuserinfo(aim_bstream_t *bs, aim_userinfo_t *info)
{
	aim_tlvlist_t *tlvlist = NULL;

	if (!bs || !info)
		return -EINVAL;

	aimbs_put8(bs, strlen(info->sn));
	aimbs_putraw(bs, info->sn, strlen(info->sn));

	aimbs_put16(bs, info->warnlevel);


	aim_addtlvtochain16(&tlvlist, 0x0001, info->flags);
	aim_addtlvtochain32(&tlvlist, 0x0002, info->membersince);
	aim_addtlvtochain32(&tlvlist, 0x0003, info->onlinesince);
	aim_addtlvtochain16(&tlvlist, 0x0004, info->idletime);

#if ICQ_OSCAR_SUPPORT
	if (atoi(info->sn) != 0) {
		aim_addtlvtochain16(&tlvlist, 0x0006, info->icqinfo.status);
		aim_addtlvtochain32(&tlvlist, 0x000a, info->icqinfo.ipaddr);
	}
#endif

	aim_addtlvtochain_caps(&tlvlist, 0x000d, info->capabilities);

	aim_addtlvtochain32(&tlvlist, (fu16_t)((info->flags & AIM_FLAG_AOL) ? 0x0010 : 0x000f), info->sessionlen);

	aimbs_put16(bs, aim_counttlvchain(&tlvlist));
	aim_writetlvchain(bs, &tlvlist);
	aim_freetlvchain(&tlvlist);

	return 0;
}

faim_export int aim_sendbuddyoncoming(aim_session_t *sess, aim_conn_t *conn, aim_userinfo_t *info)
{
	aim_frame_t *fr;
	aim_snacid_t snacid;

	if (!sess || !conn || !info)
		return -EINVAL;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 1152)))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x0003, 0x000b, 0x0000, NULL, 0);
	
	aim_putsnac(&fr->data, 0x0003, 0x000b, 0x0000, snacid);
	aim_putuserinfo(&fr->data, info);

	aim_tx_enqueue(sess, fr);

	return 0;
}

faim_export int aim_sendbuddyoffgoing(aim_session_t *sess, aim_conn_t *conn, const char *sn)
{
	aim_frame_t *fr;
	aim_snacid_t snacid;

	if (!sess || !conn || !sn)
		return -EINVAL;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10+1+strlen(sn))))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x0003, 0x000c, 0x0000, NULL, 0);
	
	aim_putsnac(&fr->data, 0x0003, 0x000c, 0x0000, snacid);
	aimbs_put8(&fr->data, strlen(sn));
	aimbs_putraw(&fr->data, sn, strlen(sn));

	aim_tx_enqueue(sess, fr);

	return 0;
}

/*
 * Huh? What is this?
 */
faim_export int aim_0002_000b(aim_session_t *sess, aim_conn_t *conn, const char *sn)
{
	aim_frame_t *fr;
	aim_snacid_t snacid;

	if (!sess || !conn || !sn)
		return -EINVAL;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10+1+strlen(sn))))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x0002, 0x000b, 0x0000, NULL, 0);
	
	aim_putsnac(&fr->data, 0x0002, 0x000b, 0x0000, snacid);
	aimbs_put8(&fr->data, strlen(sn));
	aimbs_putraw(&fr->data, sn, strlen(sn));

	aim_tx_enqueue(sess, fr);

	return 0;
}

/*
 * Normally contains:
 *   t(0001)  - short containing max profile length (value = 1024)
 *   t(0002)  - short - unknown (value = 16) [max MIME type length?]
 *   t(0003)  - short - unknown (value = 10)
 */
static int rights(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
{
	aim_tlvlist_t *tlvlist;
	aim_rxcallback_t userfunc;
	int ret = 0;
	fu16_t maxsiglen = 0;

	tlvlist = aim_readtlvchain(bs);

	if (aim_gettlv(tlvlist, 0x0001, 1))
		maxsiglen = aim_gettlv16(tlvlist, 0x0001, 1);

	if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
		ret = userfunc(sess, rx, maxsiglen);

	aim_freetlvchain(&tlvlist);

	return ret;
}

static int userinfo(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
{
	aim_userinfo_t userinfo;
	char *text_encoding = NULL, *text = NULL;
	aim_rxcallback_t userfunc;
	aim_tlvlist_t *tlvlist;
	aim_snac_t *origsnac = NULL;
	struct aim_priv_inforeq *inforeq;
	int ret = 0;

	origsnac = aim_remsnac(sess, snac->id);

	if (!origsnac || !origsnac->data) {
		faimdprintf(sess, 0, "parse_userinfo_middle: major problem: no snac stored!\n");
		return 0;
	}

	inforeq = (struct aim_priv_inforeq *)origsnac->data;

	if ((inforeq->infotype != AIM_GETINFO_GENERALINFO) &&
			(inforeq->infotype != AIM_GETINFO_AWAYMESSAGE)) {
		faimdprintf(sess, 0, "parse_userinfo_middle: unknown infotype in request! (0x%04x)\n", inforeq->infotype);
		return 0;
	}

	aim_extractuserinfo(sess, bs, &userinfo);

	tlvlist = aim_readtlvchain(bs);

	/* 
	 * Depending on what informational text was requested, different
	 * TLVs will appear here.
	 *
	 * Profile will be 1 and 2, away message will be 3 and 4.
	 */
	if (aim_gettlv(tlvlist, 0x0001, 1)) {
		text_encoding = aim_gettlv_str(tlvlist, 0x0001, 1);
		text = aim_gettlv_str(tlvlist, 0x0002, 1);
	} else if (aim_gettlv(tlvlist, 0x0003, 1)) {
		text_encoding = aim_gettlv_str(tlvlist, 0x0003, 1);
		text = aim_gettlv_str(tlvlist, 0x0004, 1);
	}

	if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
		ret = userfunc(sess, rx, &userinfo, text_encoding, text, inforeq->infotype);

	free(text_encoding);
	free(text);

	aim_freetlvchain(&tlvlist);

	if (origsnac)
		free(origsnac->data);
	free(origsnac);

	return ret;
}

static int snachandler(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
{

	if (snac->subtype == 0x0003)
		return rights(sess, mod, rx, snac, bs);
	else if (snac->subtype == 0x0006)
		return userinfo(sess, mod, rx, snac, bs);

	return 0;
}

faim_internal int locate_modfirst(aim_session_t *sess, aim_module_t *mod)
{

	mod->family = 0x0002;
	mod->version = 0x0000;
	mod->flags = 0;
	strncpy(mod->name, "locate", sizeof(mod->name));
	mod->snachandler = snachandler;

	return 0;
}
