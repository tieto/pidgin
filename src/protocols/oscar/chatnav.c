/*
 * Handle ChatNav.
 *
 * [The ChatNav(igation) service does various things to keep chat
 *  alive.  It provides room information, room searching and creating, 
 *  as well as giving users the right ("permission") to use chat.]
 *
 */

#define FAIM_INTERNAL
#include <aim.h>

/*
 * conn must be a chatnav connection!
 */
faim_export int aim_chatnav_reqrights(aim_session_t *sess, aim_conn_t *conn)
{
	return aim_genericreq_n_snacid(sess, conn, 0x000d, 0x0002);
}

faim_export int aim_chatnav_clientready(aim_session_t *sess, aim_conn_t *conn)
{
	aim_frame_t *fr;
	aim_snacid_t snacid;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 0x20)))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x0001, 0x0002, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x0001, 0x0002, 0x0000, snacid);

	aimbs_put16(&fr->data, 0x000d);
	aimbs_put16(&fr->data, 0x0001);

	aimbs_put16(&fr->data, 0x0004);
	aimbs_put16(&fr->data, 0x0001);

	aimbs_put16(&fr->data, 0x0001);
	aimbs_put16(&fr->data, 0x0003);

	aimbs_put16(&fr->data, 0x0004);
	aimbs_put16(&fr->data, 0x0686);

	aim_tx_enqueue(sess, fr);

	return 0;
}

faim_export int aim_chatnav_createroom(aim_session_t *sess, aim_conn_t *conn, const char *name, fu16_t exchange)
{
	aim_frame_t *fr;
	aim_snacid_t snacid;
	aim_tlvlist_t *tl = NULL;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10+12+strlen("invite")+strlen(name))))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x000d, 0x0008, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x000d, 0x0008, 0x0000, snacid);

	/* exchange */
	aimbs_put16(&fr->data, exchange);

	/* room cookie */
	aimbs_put8(&fr->data, strlen("invite"));
	aimbs_putraw(&fr->data, "invite", strlen("invite"));

	/* 
	 * instance
	 * 
	 * Setting this to 0xffff apparently assigns the last instance.
	 *
	 */
	aimbs_put16(&fr->data, 0xffff);

	/* detail level */
	aimbs_put8(&fr->data, 0x01);

	/* room name */
	aim_addtlvtochain_raw(&tl, 0x00d3, strlen(name), name);

	/* tlvcount */
	aimbs_put16(&fr->data, aim_counttlvchain(&tl));
	aim_writetlvchain(&fr->data, &tl);

	aim_freetlvchain(&tl);

	aim_tx_enqueue(sess, fr);

	return 0;
}

static int parseinfo_perms(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs, aim_snac_t *snac2)
{
	aim_rxcallback_t userfunc;
	int ret = 0;
	struct aim_chat_exchangeinfo *exchanges = NULL;
	int curexchange;
	aim_tlv_t *exchangetlv;
	fu8_t maxrooms = 0;
	aim_tlvlist_t *tlvlist, *innerlist;

	tlvlist = aim_readtlvchain(bs);

	/* 
	 * Type 0x0002: Maximum concurrent rooms.
	 */ 
	if (aim_gettlv(tlvlist, 0x0002, 1))
		maxrooms = aim_gettlv8(tlvlist, 0x0002, 1);

	/* 
	 * Type 0x0003: Exchange information
	 *
	 * There can be any number of these, each one
	 * representing another exchange.  
	 * 
	 */
	for (curexchange = 0; ((exchangetlv = aim_gettlv(tlvlist, 0x0003, curexchange+1))); ) {
		aim_bstream_t tbs;

		aim_bstream_init(&tbs, exchangetlv->value, exchangetlv->length);

		curexchange++;
	
		exchanges = realloc(exchanges, curexchange * sizeof(struct aim_chat_exchangeinfo));

		/* exchange number */
		exchanges[curexchange-1].number = aimbs_get16(&tbs);
		innerlist = aim_readtlvchain(&tbs);

		/* 
		 * Type 0x000d: Unknown.
		 */
		if (aim_gettlv(innerlist, 0x000d, 1))
			;

		/* 
		 * Type 0x0004: Unknown
		 */
		if (aim_gettlv(innerlist, 0x0004, 1))
			;

		/* 
		 * Type 0x0002: Unknown
		 */
		if (aim_gettlv(innerlist, 0x0002, 1)) {
			fu16_t classperms;

			classperms = aim_gettlv16(innerlist, 0x0002, 1);
			
			faimdprintf(sess, 1, "faim: class permissions %x\n", classperms);
		}

		/*
		 * Type 0x00c9: Unknown
		 */ 
		if (aim_gettlv(innerlist, 0x00c9, 1))
			;
		      
		/*
		 * Type 0x00ca: Creation Date 
		 */
		if (aim_gettlv(innerlist, 0x00ca, 1))
			;
		      
		/*
		 * Type 0x00d0: Mandatory Channels?
		 */
		if (aim_gettlv(innerlist, 0x00d0, 1))
			;

		/*
		 * Type 0x00d1: Maximum Message length
		 */
		if (aim_gettlv(innerlist, 0x00d1, 1))
			;

		/*
		 * Type 0x00d2: Maximum Occupancy?
		 */
		if (aim_gettlv(innerlist, 0x00d2, 1))	
			;

		/*
		 * Type 0x00d3: Exchange Name
		 */
		if (aim_gettlv(innerlist, 0x00d3, 1))	
			exchanges[curexchange-1].name = aim_gettlv_str(innerlist, 0x00d3, 1);
		else
			exchanges[curexchange-1].name = NULL;

		/*
		 * Type 0x00d5: Creation Permissions
		 *
		 * 0  Creation not allowed
		 * 1  Room creation allowed
		 * 2  Exchange creation allowed
		 * 
		 */
		if (aim_gettlv(innerlist, 0x00d5, 1)) {
			fu8_t createperms;

			createperms = aim_gettlv8(innerlist, 0x00d5, 1);
		}

		/*
		 * Type 0x00d6: Character Set (First Time)
		 */	      
		if (aim_gettlv(innerlist, 0x00d6, 1))	
			exchanges[curexchange-1].charset1 = aim_gettlv_str(innerlist, 0x00d6, 1);
		else
			exchanges[curexchange-1].charset1 = NULL;
		      
		/*
		 * Type 0x00d7: Language (First Time)
		 */	      
		if (aim_gettlv(innerlist, 0x00d7, 1))	
			exchanges[curexchange-1].lang1 = aim_gettlv_str(innerlist, 0x00d7, 1);
		else
			exchanges[curexchange-1].lang1 = NULL;

		/*
		 * Type 0x00d8: Character Set (Second Time)
		 */	      
		if (aim_gettlv(innerlist, 0x00d8, 1))	
			exchanges[curexchange-1].charset2 = aim_gettlv_str(innerlist, 0x00d8, 1);
		else
			exchanges[curexchange-1].charset2 = NULL;

		/*
		 * Type 0x00d9: Language (Second Time)
		 */	      
		if (aim_gettlv(innerlist, 0x00d9, 1))	
			exchanges[curexchange-1].lang2 = aim_gettlv_str(innerlist, 0x00d9, 1);
		else
			exchanges[curexchange-1].lang2 = NULL;
		      
		aim_freetlvchain(&innerlist);
	}

	/*
	 * Call client.
	 */
	if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
		ret = userfunc(sess, rx, snac2->type, maxrooms, curexchange, exchanges);

	for (curexchange--; curexchange >= 0; curexchange--) {
		free(exchanges[curexchange].name);
		free(exchanges[curexchange].charset1);
		free(exchanges[curexchange].lang1);
		free(exchanges[curexchange].charset2);
		free(exchanges[curexchange].lang2);
	}
	free(exchanges);
	aim_freetlvchain(&tlvlist);

	return ret;
}

static int parseinfo_create(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs, aim_snac_t *snac2)
{
	aim_rxcallback_t userfunc;
	aim_tlvlist_t *tlvlist, *innerlist;
	char *ck = NULL, *fqcn = NULL, *name = NULL;
	fu16_t exchange = 0, instance = 0, unknown = 0, flags = 0, maxmsglen = 0, maxoccupancy = 0;
	fu32_t createtime = 0;
	fu8_t createperms = 0, detaillevel;
	int cklen;
	aim_tlv_t *bigblock;
	int ret = 0;
	aim_bstream_t bbbs;

	tlvlist = aim_readtlvchain(bs);

	if (!(bigblock = aim_gettlv(tlvlist, 0x0004, 1))) {
		faimdprintf(sess, 0, "no bigblock in top tlv in create room response\n");
		aim_freetlvchain(&tlvlist);
		return 0;
	}

	aim_bstream_init(&bbbs, bigblock->value, bigblock->length);

	exchange = aimbs_get16(&bbbs);
	cklen = aimbs_get8(&bbbs);
	ck = aimbs_getstr(&bbbs, cklen);
	instance = aimbs_get16(&bbbs);
	detaillevel = aimbs_get8(&bbbs);

	if (detaillevel != 0x02) {
		faimdprintf(sess, 0, "unknown detaillevel in create room response (0x%02x)\n", detaillevel);
		aim_freetlvchain(&tlvlist);
		free(ck);
		return 0;
	}

	unknown = aimbs_get16(&bbbs);

	innerlist = aim_readtlvchain(&bbbs);

	if (aim_gettlv(innerlist, 0x006a, 1))
		fqcn = aim_gettlv_str(innerlist, 0x006a, 1);

	if (aim_gettlv(innerlist, 0x00c9, 1))
		flags = aim_gettlv16(innerlist, 0x00c9, 1);

	if (aim_gettlv(innerlist, 0x00ca, 1))
		createtime = aim_gettlv32(innerlist, 0x00ca, 1);

	if (aim_gettlv(innerlist, 0x00d1, 1))
		maxmsglen = aim_gettlv16(innerlist, 0x00d1, 1);

	if (aim_gettlv(innerlist, 0x00d2, 1))
		maxoccupancy = aim_gettlv16(innerlist, 0x00d2, 1);

	if (aim_gettlv(innerlist, 0x00d3, 1))
		name = aim_gettlv_str(innerlist, 0x00d3, 1);

	if (aim_gettlv(innerlist, 0x00d5, 1))
		createperms = aim_gettlv8(innerlist, 0x00d5, 1);

	if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype))) {
		ret = userfunc(sess, rx, snac2->type, fqcn, instance, exchange, flags, createtime, maxmsglen, maxoccupancy, createperms, unknown, name, ck);
	}

	free(ck);
	free(name);
	free(fqcn);
	aim_freetlvchain(&innerlist);
	aim_freetlvchain(&tlvlist);

	return ret;
}

/*
 * Since multiple things can trigger this callback, we must lookup the 
 * snacid to determine the original snac subtype that was called.
 */
static int parseinfo(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
{
	aim_snac_t *snac2;
	int ret = 0;

	if (!(snac2 = aim_remsnac(sess, snac->id))) {
		faimdprintf(sess, 0, "faim: chatnav_parse_info: received response to unknown request! (%08lx)\n", snac->id);
		return 0;
	}

	if (snac2->family != 0x000d) {
		faimdprintf(sess, 0, "faim: chatnav_parse_info: recieved response that maps to corrupt request! (fam=%04x)\n", snac2->family);
		return 0;
	}

	/*
	 * We now know what the original SNAC subtype was.
	 */
	if (snac2->type == 0x0002) /* request chat rights */
		ret = parseinfo_perms(sess, mod, rx, snac, bs, snac2);
	else if (snac2->type == 0x0003) /* request exchange info */
		faimdprintf(sess, 0, "chatnav_parse_info: resposne to exchange info\n");
	else if (snac2->type == 0x0004) /* request room info */
		faimdprintf(sess, 0, "chatnav_parse_info: response to room info\n");
	else if (snac2->type == 0x0005) /* request more room info */
		faimdprintf(sess, 0, "chatnav_parse_info: response to more room info\n");
	else if (snac2->type == 0x0006) /* request occupant list */
		faimdprintf(sess, 0, "chatnav_parse_info: response to occupant info\n");
	else if (snac2->type == 0x0007) /* search for a room */
		faimdprintf(sess, 0, "chatnav_parse_info: search results\n");
	else if (snac2->type == 0x0008) /* create room */
		ret = parseinfo_create(sess, mod, rx, snac, bs, snac2);
	else
		faimdprintf(sess, 0, "chatnav_parse_info: unknown request subtype (%04x)\n", snac2->type);

	if (snac2)
		free(snac2->data);
	free(snac2);

	return ret;
}

static int snachandler(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
{

	if (snac->subtype == 0x0009)
		return parseinfo(sess, mod, rx, snac, bs);

	return 0;
}

faim_internal int chatnav_modfirst(aim_session_t *sess, aim_module_t *mod)
{

	mod->family = 0x000d;
	mod->version = 0x0000;
	mod->flags = 0;
	strncpy(mod->name, "chatnav", sizeof(mod->name));
	mod->snachandler = snachandler;

	return 0;
}
