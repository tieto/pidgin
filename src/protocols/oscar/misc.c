
/*
 * aim_misc.c
 *
 * TODO: Seperate a lot of this into an aim_bos.c.
 *
 * Other things...
 *
 *   - Idle setting 
 * 
 *
 */

#define FAIM_INTERNAL
#include <aim.h> 

/*
 * aim_bos_setidle()
 *
 *  Should set your current idle time in seconds.  Idealy, OSCAR should
 *  do this for us.  But, it doesn't.  The client must call this to set idle
 *  time.  
 *
 */
faim_export int aim_bos_setidle(aim_session_t *sess, aim_conn_t *conn, fu32_t idletime)
{
	return aim_genericreq_l(sess, conn, 0x0001, 0x0011, &idletime);
}


/*
 * aim_bos_changevisibility(conn, changtype, namelist)
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
faim_export int aim_bos_changevisibility(aim_session_t *sess, aim_conn_t *conn, int changetype, const char *denylist)
{
	aim_frame_t *fr;
	int packlen = 0;
	fu16_t subtype;
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

	localcpy = strdup(denylist);

	listcount = aimutil_itemcnt(localcpy, '&');
	packlen = aimutil_tokslen(localcpy, 99, '&') + listcount + 9;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, packlen))) {
		free(localcpy);
		return -ENOMEM;
	}

	snacid = aim_cachesnac(sess, 0x0009, subtype, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x0009, subtype, 0x00, snacid);

	for (i = 0; (i < (listcount - 1)) && (i < 99); i++) {
		tmpptr = aimutil_itemidx(localcpy, i, '&');

		aimbs_put8(&fr->data, strlen(tmpptr));
		aimbs_putraw(&fr->data, tmpptr, strlen(tmpptr));

		free(tmpptr);
	}
	free(localcpy);

	aim_tx_enqueue(sess, fr);

	return 0;
}



/*
 * aim_bos_setbuddylist(buddylist)
 *
 * This just builds the "set buddy list" command then queues it.
 *
 * buddy_list = "Screen Name One&ScreenNameTwo&";
 *
 * TODO: Clean this up.  
 *
 * XXX: I can't stress the TODO enough.
 *
 */
faim_export int aim_bos_setbuddylist(aim_session_t *sess, aim_conn_t *conn, const char *buddy_list)
{
	aim_frame_t *fr;
	aim_snacid_t snacid;
	int i, len = 0;
	char *localcpy = NULL;
	char *tmpptr = NULL;

	if (!buddy_list || !(localcpy = strdup(buddy_list))) 
		return -EINVAL;

	i = 0;
	tmpptr = strtok(localcpy, "&");
	while ((tmpptr != NULL) && (i < 150)) {
		faimdprintf(sess, 2, "---adding %d: %s (%d)\n", i, tmpptr, strlen(tmpptr));
		len += 1+strlen(tmpptr);
		i++;
		tmpptr = strtok(NULL, "&");
	}

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10+len)))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x0003, 0x0004, 0x0000, NULL, 0);
	
	aim_putsnac(&fr->data, 0x0003, 0x0004, 0x0000, snacid);

	strncpy(localcpy, buddy_list, strlen(buddy_list)+1);
	i = 0;
	tmpptr = strtok(localcpy, "&");
	while ((tmpptr != NULL) & (i < 150)) {
		faimdprintf(sess, 2, "---adding %d: %s (%d)\n", i, tmpptr, strlen(tmpptr));
		
		aimbs_put8(&fr->data, strlen(tmpptr));
		aimbs_putraw(&fr->data, tmpptr, strlen(tmpptr));
		i++;
		tmpptr = strtok(NULL, "&");
	}

	aim_tx_enqueue(sess, fr);

	free(localcpy);

	return 0;
}

/* 
 * aim_bos_setprofile(profile)
 *
 * Gives BOS your profile.
 *
 * 
 */
faim_export int aim_bos_setprofile(aim_session_t *sess, aim_conn_t *conn, const char *profile, const char *awaymsg, fu16_t caps)
{
	static const char defencoding[] = {"text/aolrtf; charset=\"us-ascii\""};
	aim_frame_t *fr;
	aim_tlvlist_t *tl = NULL;
	aim_snacid_t snacid;

	/* Build to packet first to get real length */
	if (profile) {
		aim_addtlvtochain_raw(&tl, 0x0001, strlen(defencoding), defencoding);
		aim_addtlvtochain_raw(&tl, 0x0002, strlen(profile), profile);
	}
	
	if (awaymsg) {
		aim_addtlvtochain_raw(&tl, 0x0003, strlen(defencoding), defencoding);
		aim_addtlvtochain_raw(&tl, 0x0004, strlen(awaymsg), awaymsg);
	}

	aim_addtlvtochain_caps(&tl, 0x0005, caps);

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10 + aim_sizetlvchain(&tl))))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x0002, 0x0004, 0x0000, NULL, 0);
	
	aim_putsnac(&fr->data, 0x0002, 0x004, 0x0000, snacid);
	aim_writetlvchain(&fr->data, &tl);
	aim_freetlvchain(&tl);

	aim_tx_enqueue(sess, fr);

	return 0;
}

/*
 * aim_bos_clientready()
 * 
 * Send Client Ready.  
 *
 */
faim_export int aim_clientready(aim_session_t *sess, aim_conn_t *conn)
{
	aim_conn_inside_t *ins = (aim_conn_inside_t *)conn->inside;
	struct snacgroup *sg;
	aim_frame_t *fr;
	aim_snacid_t snacid;

	if (!ins)
		return -EINVAL;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 1152)))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x0001, 0x0002, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x0001, 0x0002, 0x0000, snacid);

	/*
	 * Send only the tool versions that the server cares about (that it
	 * marked as supporting in the server ready SNAC).  
	 */
	for (sg = ins->groups; sg; sg = sg->next) {
		aim_module_t *mod;

		if ((mod = aim__findmodulebygroup(sess, sg->group))) {
			aimbs_put16(&fr->data, mod->family);
			aimbs_put16(&fr->data, mod->version);
			aimbs_put16(&fr->data, mod->toolid);
			aimbs_put16(&fr->data, mod->toolversion);
		} else
			faimdprintf(sess, 1, "aim_clientready: server supports group 0x%04x but we don't!\n", sg->group);
	}

	aim_tx_enqueue(sess, fr);

	return 0;
}

/* 
 *  Request Rate Information.
 * 
 */
faim_export int aim_reqrates(aim_session_t *sess, aim_conn_t *conn)
{
	return aim_genericreq_n(sess, conn, 0x0001, 0x0006);
}

/* 
 *  Rate Information Response Acknowledge.
 *
 */
faim_export int aim_ratesack(aim_session_t *sess, aim_conn_t *conn)
{
	aim_frame_t *fr;	
	aim_snacid_t snacid;

	if(!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10+10)))
		return -ENOMEM; 

	snacid = aim_cachesnac(sess, 0x0001, 0x0008, 0x0000, NULL, 0);
	
	aim_putsnac(&fr->data, 0x0001, 0x0008, 0x0000, snacid);

	/* XXX store the rate info in the inside struct, make this dynamic */
	aimbs_put16(&fr->data, 0x0001); 
	aimbs_put16(&fr->data, 0x0002);
	aimbs_put16(&fr->data, 0x0003);
	aimbs_put16(&fr->data, 0x0004);
	aimbs_put16(&fr->data, 0x0005);

	aim_tx_enqueue(sess, fr);

	return 0;
}

/* 
 * aim_bos_setprivacyflags()
 *
 * Sets privacy flags. Normally 0x03.
 *
 *  Bit 1:  Allows other AIM users to see how long you've been idle.
 *  Bit 2:  Allows other AIM users to see how long you've been a member.
 *
 */
faim_export int aim_bos_setprivacyflags(aim_session_t *sess, aim_conn_t *conn, fu32_t flags)
{
	return aim_genericreq_l(sess, conn, 0x0001, 0x0014, &flags);
}

/*
 * aim_bos_reqpersonalinfo()
 *
 */
faim_export int aim_bos_reqpersonalinfo(aim_session_t *sess, aim_conn_t *conn)
{
	return aim_genericreq_n(sess, conn, 0x0001, 0x000e);
}

faim_export int aim_setversions(aim_session_t *sess, aim_conn_t *conn)
{
	aim_conn_inside_t *ins = (aim_conn_inside_t *)conn->inside;
	struct snacgroup *sg;
	aim_frame_t *fr;
	aim_snacid_t snacid;

	if (!ins)
		return -EINVAL;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 1152)))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x0001, 0x0017, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x0001, 0x0017, 0x0000, snacid);

	/*
	 * Send only the versions that the server cares about (that it
	 * marked as supporting in the server ready SNAC).  
	 */
	for (sg = ins->groups; sg; sg = sg->next) {
		aim_module_t *mod;

		if ((mod = aim__findmodulebygroup(sess, sg->group))) {
			aimbs_put16(&fr->data, mod->family);
			aimbs_put16(&fr->data, mod->version);
		} else
			faimdprintf(sess, 1, "aim_setversions: server supports group 0x%04x but we don't!\n", sg->group);
	}

	aim_tx_enqueue(sess, fr);

	return 0;
}


/*
 * aim_bos_reqservice(serviceid)
 *
 * Service request. 
 *
 */
faim_export int aim_bos_reqservice(aim_session_t *sess, aim_conn_t *conn, fu16_t serviceid)
{
	return aim_genericreq_s(sess, conn, 0x0001, 0x0004, &serviceid);
}

/*
 * aim_bos_nop()
 *
 * No-op.  WinAIM sends these every 4min or so to keep
 * the connection alive.  Its not real necessary.
 *
 */
faim_export int aim_bos_nop(aim_session_t *sess, aim_conn_t *conn)
{
	return aim_genericreq_n(sess, conn, 0x0001, 0x0016);
}

/*
 * aim_flap_nop()
 *
 * No-op.  WinAIM 4.x sends these _every minute_ to keep
 * the connection alive.  
 */
faim_export int aim_flap_nop(aim_session_t *sess, aim_conn_t *conn)
{
	aim_frame_t *fr;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x05, 0)))
		return -ENOMEM;

	aim_tx_enqueue(sess, fr);

	return 0;
}

/*
 * aim_bos_reqrights()
 *
 * Request BOS rights.
 *
 */
faim_export int aim_bos_reqrights(aim_session_t *sess, aim_conn_t *conn)
{
	return aim_genericreq_n(sess, conn, 0x0009, 0x0002);
}

/*
 * aim_bos_reqbuddyrights()
 *
 * Request Buddy List rights.
 *
 */
faim_export int aim_bos_reqbuddyrights(aim_session_t *sess, aim_conn_t *conn)
{
	return aim_genericreq_n(sess, conn, 0x0003, 0x0002);
}

/*
 * Send a warning to destsn.
 * 
 * Flags:
 *  AIM_WARN_ANON  Send as an anonymous (doesn't count as much)
 *
 * returns -1 on error (couldn't alloc packet), 0 on success. 
 *
 */
faim_export int aim_send_warning(aim_session_t *sess, aim_conn_t *conn, const char *destsn, fu32_t flags)
{
	aim_frame_t *fr;
	aim_snacid_t snacid;
	fu16_t outflags = 0x0000;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, strlen(destsn)+13)))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x0004, 0x0008, 0x0000, destsn, strlen(destsn)+1);

	aim_putsnac(&fr->data, 0x0004, 0x0008, 0x0000, snacid);

	if (flags & AIM_WARN_ANON)
		outflags |= 0x0001;

	aimbs_put16(&fr->data, outflags); 
	aimbs_put8(&fr->data, strlen(destsn));
	aimbs_putraw(&fr->data, destsn, strlen(destsn));

	aim_tx_enqueue(sess, fr);

	return 0;
}

/*
 * aim_debugconn_sendconnect()
 *
 * For aimdebugd.  If you don't know what it is, you don't want to.
 */
faim_export int aim_debugconn_sendconnect(aim_session_t *sess, aim_conn_t *conn)
{
	return aim_genericreq_n(sess, conn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_DEBUGCONN_CONNECT);
}

/*
 * Generic routine for sending commands.
 *
 *
 * I know I can do this in a smarter way...but I'm not thinking straight
 * right now...
 *
 * I had one big function that handled all three cases, but then it broke
 * and I split it up into three.  But then I fixed it.  I just never went
 * back to the single.  I don't see any advantage to doing it either way.
 *
 */
faim_internal int aim_genericreq_n(aim_session_t *sess, aim_conn_t *conn, fu16_t family, fu16_t subtype)
{
	aim_frame_t *fr;
	aim_snacid_t snacid = 0x00000000;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10)))
		return -ENOMEM;

	aim_putsnac(&fr->data, family, subtype, 0x0000, snacid);

	aim_tx_enqueue(sess, fr);

	return 0;
}

faim_internal int aim_genericreq_n_snacid(aim_session_t *sess, aim_conn_t *conn, fu16_t family, fu16_t subtype)
{
	aim_frame_t *fr;
	aim_snacid_t snacid;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10)))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, family, subtype, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, family, subtype, 0x0000, snacid);

	aim_tx_enqueue(sess, fr);

	return 0;
}

/*
 *
 *
 */
faim_internal int aim_genericreq_l(aim_session_t *sess, aim_conn_t *conn, fu16_t family, fu16_t subtype, fu32_t *longdata)
{
	aim_frame_t *fr;
	aim_snacid_t snacid;

	if (!longdata)
		return aim_genericreq_n(sess, conn, family, subtype);

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10+4)))
		return -ENOMEM; 

	snacid = aim_cachesnac(sess, family, subtype, 0x0000, NULL, 0);

	aim_putsnac(&fr->data, family, subtype, 0x0000, snacid);
	aimbs_put32(&fr->data, *longdata);

	aim_tx_enqueue(sess, fr);

	return 0;
}

faim_internal int aim_genericreq_s(aim_session_t *sess, aim_conn_t *conn, fu16_t family, fu16_t subtype, fu16_t *shortdata)
{
	aim_frame_t *fr;
	aim_snacid_t snacid;

	if (!shortdata)
		return aim_genericreq_n(sess, conn, family, subtype);

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10+2)))
		return -ENOMEM; 

	snacid = aim_cachesnac(sess, family, subtype, 0x0000, NULL, 0);

	aim_putsnac(&fr->data, family, subtype, 0x0000, snacid);
	aimbs_put16(&fr->data, *shortdata);

	aim_tx_enqueue(sess, fr);

	return 0;
}

/*
 * aim_bos_reqlocaterights()
 *
 * Request Location services rights.
 *
 */
faim_export int aim_bos_reqlocaterights(aim_session_t *sess, aim_conn_t *conn)
{
	return aim_genericreq_n(sess, conn, 0x0002, 0x0002);
}

/* 
 * Set directory profile data (not the same as aim_bos_setprofile!)
 *
 * privacy: 1 to allow searching, 0 to disallow.
 */
faim_export int aim_setdirectoryinfo(aim_session_t *sess, aim_conn_t *conn, const char *first, const char *middle, const char *last, const char *maiden, const char *nickname, const char *street, const char *city, const char *state, const char *zip, int country, fu16_t privacy) 
{
	aim_frame_t *fr;
	aim_snacid_t snacid;
	aim_tlvlist_t *tl = NULL;


	aim_addtlvtochain16(&tl, 0x000a, privacy);

	if (first)
		aim_addtlvtochain_raw(&tl, 0x0001, strlen(first), first);
	if (last)
		aim_addtlvtochain_raw(&tl, 0x0002, strlen(last), last);
	if (middle)
		aim_addtlvtochain_raw(&tl, 0x0003, strlen(middle), middle);
	if (maiden)
		aim_addtlvtochain_raw(&tl, 0x0004, strlen(maiden), maiden);

	if (state)
		aim_addtlvtochain_raw(&tl, 0x0007, strlen(state), state);
	if (city)
		aim_addtlvtochain_raw(&tl, 0x0008, strlen(city), city);

	if (nickname)
		aim_addtlvtochain_raw(&tl, 0x000c, strlen(nickname), nickname);
	if (zip)
		aim_addtlvtochain_raw(&tl, 0x000d, strlen(zip), zip);

	if (street)
		aim_addtlvtochain_raw(&tl, 0x0021, strlen(street), street);

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10+aim_sizetlvchain(&tl))))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x0002, 0x0009, 0x0000, NULL, 0);
	
	aim_putsnac(&fr->data, 0x0002, 0x0009, 0x0000, snacid);
	aim_writetlvchain(&fr->data, &tl);
	aim_freetlvchain(&tl);

	aim_tx_enqueue(sess, fr);

	return 0;
}

/* XXX pass these in better */
faim_export int aim_setuserinterests(aim_session_t *sess, aim_conn_t *conn, const char *interest1, const char *interest2, const char *interest3, const char *interest4, const char *interest5, fu16_t privacy)
{
	aim_frame_t *fr;
	aim_snacid_t snacid;
	aim_tlvlist_t *tl = NULL;

	/* ?? privacy ?? */
	aim_addtlvtochain16(&tl, 0x000a, privacy);

	if (interest1)
		aim_addtlvtochain_raw(&tl, 0x0000b, strlen(interest1), interest1);
	if (interest2)
		aim_addtlvtochain_raw(&tl, 0x0000b, strlen(interest2), interest2);
	if (interest3)
		aim_addtlvtochain_raw(&tl, 0x0000b, strlen(interest3), interest3);
	if (interest4)
		aim_addtlvtochain_raw(&tl, 0x0000b, strlen(interest4), interest4);
	if (interest5)
		aim_addtlvtochain_raw(&tl, 0x0000b, strlen(interest5), interest5);

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10+aim_sizetlvchain(&tl))))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x0002, 0x000f, 0x0000, NULL, 0);

	aim_putsnac(&fr->data, 0x0002, 0x000f, 0x0000, 0);
	aim_writetlvchain(&fr->data, &tl);
	aim_freetlvchain(&tl);

	aim_tx_enqueue(sess, fr);

	return 0;
}

faim_export int aim_icq_setstatus(aim_session_t *sess, aim_conn_t *conn, fu16_t status)
{
	aim_frame_t *fr;
	aim_snacid_t snacid;
	aim_tlvlist_t *tl = NULL;
	fu32_t data;

	data = 0x00030000 | status; /* yay for error checking ;^) */

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10 + 8)))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x0001, 0x001e, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x0001, 0x001e, 0x0000, snacid);
	
	aim_addtlvtochain32(&tl, 0x0006, data);
	aim_writetlvchain(&fr->data, &tl);
	aim_freetlvchain(&tl);
	
	aim_tx_enqueue(sess, fr);

	return 0;
}

/*
 * Should be generic enough to handle the errors for all families...
 *
 */
static int generror(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
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

static int snachandler(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
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

faim_internal int misc_modfirst(aim_session_t *sess, aim_module_t *mod)
{

	mod->family = 0xffff;
	mod->version = 0x0000;
	mod->flags = AIM_MODFLAG_MULTIFAMILY;
	strncpy(mod->name, "misc", sizeof(mod->name));
	mod->snachandler = snachandler;

	return 0;
}
