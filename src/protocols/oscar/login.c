/*
 *  aim_login.c
 *
 *  This contains all the functions needed to actually login.
 *
 */

#define FAIM_INTERNAL
#include <aim.h>

#include "md5.h"

static int aim_encode_password(const char *password, unsigned char *encoded);

faim_export int aim_sendflapver(aim_session_t *sess, aim_conn_t *conn)
{
	aim_frame_t *fr;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x01, 4)))
		return -ENOMEM;

	aimbs_put32(&fr->data, 0x00000001);

	aim_tx_enqueue(sess, fr);

	return 0;
}

/*
 * This is a bit confusing.
 *
 * Normal SNAC login goes like this:
 *   - connect
 *   - server sends flap version
 *   - client sends flap version
 *   - client sends screen name (17/6)
 *   - server sends hash key (17/7)
 *   - client sends auth request (17/2 -- aim_send_login)
 *   - server yells
 *
 * XOR login (for ICQ) goes like this:
 *   - connect
 *   - server sends flap version
 *   - client sends auth request which contains flap version (aim_send_login)
 *   - server yells
 *
 * For the client API, we make them implement the most complicated version,
 * and for the simpler version, we fake it and make it look like the more
 * complicated process.
 *
 * This is done by giving the client a faked key, just so we can convince
 * them to call aim_send_login right away, which will detect the session
 * flag that says this is XOR login and ignore the key, sending an ICQ
 * login request instead of the normal SNAC one.
 *
 * As soon as AOL makes ICQ log in the same way as AIM, this is /gone/.
 *
 * XXX This may cause problems if the client relies on callbacks only
 * being called from the context of aim_rxdispatch()...
 *
 */
static int goddamnicq(aim_session_t *sess, aim_conn_t *conn, const char *sn)
{
	aim_frame_t fr;
	aim_rxcallback_t userfunc;
	
	sess->flags &= ~AIM_SESS_FLAGS_SNACLOGIN;
	sess->flags |= AIM_SESS_FLAGS_XORLOGIN;

	fr.conn = conn;
	
	if ((userfunc = aim_callhandler(sess, conn, 0x0017, 0x0007)))
		userfunc(sess, &fr, "");

	return 0;
}

/*
 * In AIM 3.5 protocol, the first stage of login is to request login from the 
 * Authorizer, passing it the screen name for verification.  If the name is 
 * invalid, a 0017/0003 is spit back, with the standard error contents.  If 
 * valid, a 0017/0007 comes back, which is the signal to send it the main 
 * login command (0017/0002). 
 *
 */
faim_export int aim_request_login(aim_session_t *sess, aim_conn_t *conn, const char *sn)
{
	aim_frame_t *fr;
	aim_snacid_t snacid;
	aim_tlvlist_t *tl = NULL;
	
	if (!sess || !conn || !sn)
		return -EINVAL;

	if ((sn[0] >= '0') && (sn[0] <= '9'))
		return goddamnicq(sess, conn, sn);

	sess->flags |= AIM_SESS_FLAGS_SNACLOGIN;

	aim_sendflapver(sess, conn);

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10+2+2+strlen(sn))))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x0017, 0x0006, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x0017, 0x0006, 0x0000, snacid);

	aim_addtlvtochain_raw(&tl, 0x0001, strlen(sn), sn);
	aim_writetlvchain(&fr->data, &tl);
	aim_freetlvchain(&tl);

	aim_tx_enqueue(sess, fr);

	return 0;
}

/*
 * Part two of the ICQ hack.  Note the ignoring of the key and clientinfo.
 */
static int goddamnicq2(aim_session_t *sess, aim_conn_t *conn, const char *sn, const char *password)
{
	static const char clientstr[] = {"ICQ Inc. - Product of ICQ (TM) 2000b.4.65.1.3281.85"};
	static const char lang[] = {"en"};
	static const char country[] = {"us"};
	aim_frame_t *fr;
	aim_tlvlist_t *tl = NULL;
	char *password_encoded;

	if (!(password_encoded = (char *) malloc(strlen(password))))
		return -ENOMEM;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x01, 1152))) {
		free(password_encoded);
		return -ENOMEM;
	}

	aim_encode_password(password, password_encoded);

	aimbs_put32(&fr->data, 0x00000001);
	aim_addtlvtochain_raw(&tl, 0x0001, strlen(sn), sn);
	aim_addtlvtochain_raw(&tl, 0x0002, strlen(password), password_encoded);
	aim_addtlvtochain_raw(&tl, 0x0003, strlen(clientstr), clientstr);
	aim_addtlvtochain16(&tl, 0x0016, 0x010a);
	aim_addtlvtochain16(&tl, 0x0017, 0x0004);
	aim_addtlvtochain16(&tl, 0x0018, 0x0041);
	aim_addtlvtochain16(&tl, 0x0019, 0x0001);
	aim_addtlvtochain16(&tl, 0x001a, 0x0cd1);
	aim_addtlvtochain32(&tl, 0x0014, 0x00000055);
	aim_addtlvtochain_raw(&tl, 0x000f, strlen(lang), lang);
	aim_addtlvtochain_raw(&tl, 0x000e, strlen(country), country);

	aim_writetlvchain(&fr->data, &tl);

	free(password_encoded);
	aim_freetlvchain(&tl);

	aim_tx_enqueue(sess, fr);

	return 0;
}

/*
 * send_login(int socket, char *sn, char *password)
 *  
 * This is the initial login request packet.
 *
 * NOTE!! If you want/need to make use of the aim_sendmemblock() function,
 * then the client information you send here must exactly match the
 * executable that you're pulling the data from.
 *
 * Latest WinAIM:
 *   clientstring = "AOL Instant Messenger (SM), version 4.3.2188/WIN32"
 *   major2 = 0x0109
 *   major = 0x0400
 *   minor = 0x0003
 *   minor2 = 0x0000
 *   build = 0x088c
 *   unknown = 0x00000086
 *   lang = "en"
 *   country = "us"
 *   unknown4a = 0x01
 *
 * Latest WinAIM that libfaim can emulate without server-side buddylists:
 *   clientstring = "AOL Instant Messenger (SM), version 4.1.2010/WIN32"
 *   major2 = 0x0004
 *   major  = 0x0004
 *   minor  = 0x0001
 *   minor2 = 0x0000
 *   build  = 0x07da
 *   unknown= 0x0000004b
 *
 * WinAIM 3.5.1670:
 *   clientstring = "AOL Instant Messenger (SM), version 3.5.1670/WIN32"
 *   major2 = 0x0004
 *   major =  0x0003
 *   minor =  0x0005
 *   minor2 = 0x0000
 *   build =  0x0686
 *   unknown =0x0000002a
 *
 * Java AIM 1.1.19:
 *   clientstring = "AOL Instant Messenger (TM) version 1.1.19 for Java built 03/24/98, freeMem 215871 totalMem 1048567, i686, Linus, #2 SMP Sun Feb 11 03:41:17 UTC 2001 2.4.1-ac9, IBM Corporation, 1.1.8, 45.3, Tue Mar 27 12:09:17 PST 2001"
 *   major2 = 0x0001
 *   major  = 0x0001
 *   minor  = 0x0001
 *   minor2 = (not sent)
 *   build  = 0x0013
 *   unknown= (not sent)
 *   
 * AIM for Linux 1.1.112:
 *   clientstring = "AOL Instant Messenger (SM)"
 *   major2 = 0x1d09
 *   major  = 0x0001
 *   minor  = 0x0001
 *   minor2 = 0x0001
 *   build  = 0x0070
 *   unknown= 0x0000008b
 *   serverstore = 0x01
 *
 */
faim_export int aim_send_login(aim_session_t *sess, aim_conn_t *conn, const char *sn, const char *password, struct client_info_s *clientinfo, const char *key)
{
	aim_frame_t *fr;
	aim_tlvlist_t *tl = NULL;
	fu8_t digest[16];
	aim_snacid_t snacid;

	if (!clientinfo || !sn || !password)
		return -EINVAL;

	if (sess->flags & AIM_SESS_FLAGS_XORLOGIN)
		return goddamnicq2(sess, conn, sn, password);

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 1152)))
		return -ENOMEM;

	if (sess->flags & AIM_SESS_FLAGS_XORLOGIN) {
		fr->hdr.flap.type = 0x01;

		/* Use very specific version numbers to further indicate hack */
		clientinfo->major2 = 0x010a;
		clientinfo->major = 0x0004;
		clientinfo->minor = 0x003c;
		clientinfo->minor2 = 0x0001;
		clientinfo->build = 0x0cce;
		clientinfo->unknown = 0x00000055;
	}

	snacid = aim_cachesnac(sess, 0x0017, 0x0002, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x0017, 0x0002, 0x0000, snacid);

	aim_addtlvtochain_raw(&tl, 0x0001, strlen(sn), sn);

	aim_encode_password_md5(password, key, digest);
	aim_addtlvtochain_raw(&tl, 0x0025, 16, digest);

	aim_addtlvtochain_raw(&tl, 0x0003, strlen(clientinfo->clientstring), clientinfo->clientstring);
	aim_addtlvtochain16(&tl, 0x0016, (fu16_t)clientinfo->major2);
	aim_addtlvtochain16(&tl, 0x0017, (fu16_t)clientinfo->major);
	aim_addtlvtochain16(&tl, 0x0018, (fu16_t)clientinfo->minor);
	aim_addtlvtochain16(&tl, 0x0019, (fu16_t)clientinfo->minor2);
	aim_addtlvtochain16(&tl, 0x001a, (fu16_t)clientinfo->build);
	aim_addtlvtochain_raw(&tl, 0x000e, strlen(clientinfo->country), clientinfo->country);
	aim_addtlvtochain_raw(&tl, 0x000f, strlen(clientinfo->lang), clientinfo->lang);
	aim_addtlvtochain16(&tl, 0x0009, 0x0015);

	aim_writetlvchain(&fr->data, &tl);

	aim_freetlvchain(&tl);
	
	aim_tx_enqueue(sess, fr);

	return 0;
}

faim_export int aim_encode_password_md5(const char *password, const char *key, fu8_t *digest)
{
	md5_state_t state;

	md5_init(&state);	
	md5_append(&state, (const md5_byte_t *)key, strlen(key));
	md5_append(&state, (const md5_byte_t *)password, strlen(password));
	md5_append(&state, (const md5_byte_t *)AIM_MD5_STRING, strlen(AIM_MD5_STRING));
	md5_finish(&state, (md5_byte_t *)digest);

	return 0;
}

/**
 * aim_encode_password - Encode a password using old XOR method
 * @password: incoming password
 * @encoded: buffer to put encoded password
 *
 * This takes a const pointer to a (null terminated) string
 * containing the unencoded password.  It also gets passed
 * an already allocated buffer to store the encoded password.
 * This buffer should be the exact length of the password without
 * the null.  The encoded password buffer /is not %NULL terminated/.
 *
 * The encoding_table seems to be a fixed set of values.  We'll
 * hope it doesn't change over time!  
 *
 * This is only used for the XOR method, not the better MD5 method.
 *
 */
static int aim_encode_password(const char *password, fu8_t *encoded)
{
	fu8_t encoding_table[] = {
#if 0 /* old v1 table */
		0xf3, 0xb3, 0x6c, 0x99,
		0x95, 0x3f, 0xac, 0xb6,
		0xc5, 0xfa, 0x6b, 0x63,
		0x69, 0x6c, 0xc3, 0x9f
#else /* v2.1 table, also works for ICQ */
		0xf3, 0x26, 0x81, 0xc4,
		0x39, 0x86, 0xdb, 0x92,
		0x71, 0xa3, 0xb9, 0xe6,
		0x53, 0x7a, 0x95, 0x7c
#endif
	};
	int i;

	for (i = 0; i < strlen(password); i++)
		encoded[i] = (password[i] ^ encoding_table[i]);

	return 0;
}

/*
 * Generate an authorization response.  
 *
 * You probably don't want this unless you're writing an AIM server. Which
 * I hope you're not doing.  Because it's far more difficult than it looks.
 *
 */
faim_export int aim_sendauthresp(aim_session_t *sess, aim_conn_t *conn, const char *sn, int errorcode, const char *errorurl, const char *bosip, const char *cookie, const char *email, int regstatus)
{	
	aim_tlvlist_t *tlvlist = NULL;
	aim_frame_t *fr;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x04, 1152)))
		return -ENOMEM;

	if (sn)
		aim_addtlvtochain_raw(&tlvlist, 0x0001, strlen(sn), sn);
	else
		aim_addtlvtochain_raw(&tlvlist, 0x0001, strlen(sess->sn), sess->sn);

	if (errorcode) {
		aim_addtlvtochain16(&tlvlist, 0x0008, errorcode);
		aim_addtlvtochain_raw(&tlvlist, 0x0004, strlen(errorurl), errorurl);
	} else {
		aim_addtlvtochain_raw(&tlvlist, 0x0005, strlen(bosip), bosip);
		aim_addtlvtochain_raw(&tlvlist, 0x0006, AIM_COOKIELEN, cookie);
		aim_addtlvtochain_raw(&tlvlist, 0x0011, strlen(email), email);
		aim_addtlvtochain16(&tlvlist, 0x0013, (fu16_t)regstatus);
	}

	aim_writetlvchain(&fr->data, &tlvlist);
	aim_freetlvchain(&tlvlist);

	aim_tx_enqueue(sess, fr);

	return 0;
}

/*
 * Generate a random cookie.  (Non-client use only)
 */
faim_export int aim_gencookie(fu8_t *buf)
{
	int i;

	srand(time(NULL));

	for (i = 0; i < AIM_COOKIELEN; i++)
		buf[i] = 1+(int) (256.0*rand()/(RAND_MAX+0.0));

	return i;
}

/*
 * Send Server Ready.  (Non-client)
 * 
 * XXX If anyone cares, this should be made to use the conn-stored group
 * system.
 *
 */
faim_export int aim_sendserverready(aim_session_t *sess, aim_conn_t *conn)
{
	aim_frame_t *fr;
	aim_snacid_t snacid;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10+0x22)))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x0001, 0x0003, 0x0000, NULL, 0);

	aim_putsnac(&fr->data, 0x0001, 0x0003, 0x0000, snacid);
	aimbs_put16(&fr->data, 0x0001);
	aimbs_put16(&fr->data, 0x0002);
	aimbs_put16(&fr->data, 0x0003);
	aimbs_put16(&fr->data, 0x0004);
	aimbs_put16(&fr->data, 0x0006);
	aimbs_put16(&fr->data, 0x0008);
	aimbs_put16(&fr->data, 0x0009);
	aimbs_put16(&fr->data, 0x000a);
	aimbs_put16(&fr->data, 0x000b);
	aimbs_put16(&fr->data, 0x000c);
	aimbs_put16(&fr->data, 0x0013);
	aimbs_put16(&fr->data, 0x0015);

	aim_tx_enqueue(sess, fr);

	return 0;
}

/* 
 * Send service redirect.  (Non-Client)
 */
faim_export int aim_sendredirect(aim_session_t *sess, aim_conn_t *conn, fu16_t servid, const char *ip, const char *cookie)
{	
	aim_tlvlist_t *tlvlist = NULL;
	aim_frame_t *fr;
	aim_snacid_t snacid;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 1152)))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x0001, 0x0005, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x0001, 0x0005, 0x0000, snacid);

	aim_addtlvtochain16(&tlvlist, 0x000d, servid);
	aim_addtlvtochain_raw(&tlvlist, 0x0005, strlen(ip), ip);
	aim_addtlvtochain_raw(&tlvlist, 0x0006, AIM_COOKIELEN, cookie);

	aim_writetlvchain(&fr->data, &tlvlist);
	aim_freetlvchain(&tlvlist);

	aim_tx_enqueue(sess, fr);

	return 0;
}

/*
 * See comments in conn.c about how the group associations are supposed
 * to work, and how they really work.
 *
 * This info probably doesn't even need to make it to the client.
 *
 * We don't actually call the client here.  This starts off the connection
 * initialization routine required by all AIM connections.  The next time
 * the client is called is the CONNINITDONE callback, which should be 
 * shortly after the rate information is acknowledged.
 * 
 */
static int hostonline(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
{
	fu16_t *families;
	int famcount;


	if (!(families = malloc(aim_bstream_empty(bs))))
		return 0;

	for (famcount = 0; aim_bstream_empty(bs); famcount++) {
		families[famcount] = aimbs_get16(bs);
		aim_conn_addgroup(rx->conn, families[famcount]);
	}

	free(families);


	/*
	 * Next step is in the Host Versions handler.
	 *
	 * Note that we must send this before we request rates, since
	 * the format of the rate information depends on the versions we
	 * give it.
	 *
	 */
	aim_setversions(sess, rx->conn);

	return 1; 
}

static int redirect(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
{
	int serviceid;
	fu8_t *cookie;
	char *ip;
	aim_rxcallback_t userfunc;
	aim_tlvlist_t *tlvlist;
	char *chathack = NULL;
	int chathackex = 0;
	int ret = 0;

	tlvlist = aim_readtlvchain(bs);

	if (!aim_gettlv(tlvlist, 0x000d, 1) ||
			!aim_gettlv(tlvlist, 0x0005, 1) ||
			!aim_gettlv(tlvlist, 0x0006, 1)) {
		aim_freetlvchain(&tlvlist);
		return 0;
	}

	serviceid = aim_gettlv16(tlvlist, 0x000d, 1);
	ip = aim_gettlv_str(tlvlist, 0x0005, 1);
	cookie = aim_gettlv_str(tlvlist, 0x0006, 1);

	/*
	 * Chat hack.
	 */
	if ((serviceid == AIM_CONN_TYPE_CHAT) && sess->pendingjoin) {
		chathack = sess->pendingjoin;
		chathackex = sess->pendingjoinexchange;
		sess->pendingjoin = NULL;
		sess->pendingjoinexchange = 0;
	}

	if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
		ret = userfunc(sess, rx, serviceid, ip, cookie, chathack, chathackex);

	free(ip);
	free(cookie);
	free(chathack);

	aim_freetlvchain(&tlvlist);

	return ret;
}

/* 
 *  Request Rate Information.
 * 
 */
faim_internal int aim_reqrates(aim_session_t *sess, aim_conn_t *conn)
{
	return aim_genericreq_n(sess, conn, 0x0001, 0x0006);
}

/* 
 *  Rate Information Response Acknowledge.
 *
 */
faim_internal int aim_ratesack(aim_session_t *sess, aim_conn_t *conn)
{
	aim_conn_inside_t *ins = (aim_conn_inside_t *)conn->inside;
	aim_frame_t *fr;	
	aim_snacid_t snacid;
	struct rateclass *rc;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 512)))
		return -ENOMEM; 

	snacid = aim_cachesnac(sess, 0x0001, 0x0008, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x0001, 0x0008, 0x0000, snacid);

	for (rc = ins->rates; rc; rc = rc->next)
		aimbs_put16(&fr->data, rc->classid);

	aim_tx_enqueue(sess, fr);

	return 0;
}

/*
 * The Rate Limiting System, An Abridged Guide to Nonsense.
 *
 * OSCAR defines several 'rate classes'.  Each class has seperate
 * rate limiting properties (limit level, alert level, disconnect
 * level, etc), and a set of SNAC family/type pairs associated with
 * it.  The rate classes, their limiting properties, and the definitions
 * of which SNACs are belong to which class, are defined in the
 * Rate Response packet at login to each host.  
 *
 * Logically, all rate offenses within one class count against further
 * offenses for other SNACs in the same class (ie, sending messages
 * too fast will limit the number of user info requests you can send,
 * since those two SNACs are in the same rate class).
 *
 * Since the rate classes are defined dynamically at login, the values
 * below may change. But they seem to be fairly constant.
 *
 * Currently, BOS defines five rate classes, with the commonly used
 * members as follows...
 *
 *  Rate class 0x0001:
 *  	- Everything thats not in any of the other classes
 *
 *  Rate class 0x0002:
 * 	- Buddy list add/remove
 *	- Permit list add/remove
 *	- Deny list add/remove
 *
 *  Rate class 0x0003:
 *	- User information requests
 *	- Outgoing ICBMs
 *
 *  Rate class 0x0004:
 *	- A few unknowns: 2/9, 2/b, and f/2
 *
 *  Rate class 0x0005:
 *	- Chat room create
 *	- Outgoing chat ICBMs
 *
 * The only other thing of note is that class 5 (chat) has slightly looser
 * limiting properties than class 3 (normal messages).  But thats just a 
 * small bit of trivia for you.
 *
 * The last thing that needs to be learned about the rate limiting
 * system is how the actual numbers relate to the passing of time.  This
 * seems to be a big mystery.
 * 
 */

static void rc_addclass(struct rateclass **head, struct rateclass *inrc)
{
	struct rateclass *rc, *rc2;

	if (!(rc = malloc(sizeof(struct rateclass))))
		return;

	memcpy(rc, inrc, sizeof(struct rateclass));
	rc->next = NULL;

	for (rc2 = *head; rc2 && rc2->next; rc2 = rc2->next)
		;

	if (!rc2)
		*head = rc;
	else
		rc2->next = rc;

	return;
}

static struct rateclass *rc_findclass(struct rateclass **head, fu16_t id)
{
	struct rateclass *rc;

	for (rc = *head; rc; rc = rc->next) {
		if (rc->classid == id)
			return rc;
	}

	return NULL;
}

static void rc_addpair(struct rateclass *rc, fu16_t group, fu16_t type)
{
	struct snacpair *sp, *sp2;

	if (!(sp = malloc(sizeof(struct snacpair))))
		return;
	memset(sp, 0, sizeof(struct snacpair));

	sp->group = group;
	sp->subtype = type;
	sp->next = NULL;

	for (sp2 = rc->members; sp2 && sp2->next; sp2 = sp2->next)
		;

	if (!sp2)
		rc->members = sp;
	else
		sp2->next = sp;

	return;
}

static int rateresp(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
{
	aim_conn_inside_t *ins = (aim_conn_inside_t *)rx->conn->inside;
	fu16_t numclasses, i;
	aim_rxcallback_t userfunc;


	/*
	 * First are the parameters for each rate class.
	 */
	numclasses = aimbs_get16(bs);
	for (i = 0; i < numclasses; i++) {
		struct rateclass rc;

		memset(&rc, 0, sizeof(struct rateclass));

		rc.classid = aimbs_get16(bs);
		rc.windowsize = aimbs_get32(bs);
		rc.clear = aimbs_get32(bs);
		rc.alert = aimbs_get32(bs);
		rc.limit = aimbs_get32(bs);
		rc.disconnect = aimbs_get32(bs);
		rc.current = aimbs_get32(bs);
		rc.max = aimbs_get32(bs);

		/*
		 * The server will send an extra five bytes of parameters
		 * depending on the version we advertised in 1/17.  If we
		 * didn't send 1/17 (evil!), then this will crash and you
		 * die, as it will default to the old version but we have 
		 * the new version hardcoded here. 
		 */
		if (mod->version >= 3)
			aimbs_getrawbuf(bs, rc.unknown, sizeof(rc.unknown));

		rc_addclass(&ins->rates, &rc);
	}

	/*
	 * Then the members of each class.
	 */
	for (i = 0; i < numclasses; i++) {
		fu16_t classid, count;
		struct rateclass *rc;
		int j;

		classid = aimbs_get16(bs);
		count = aimbs_get16(bs);

		rc = rc_findclass(&ins->rates, classid);

		for (j = 0; j < count; j++) {
			fu16_t group, subtype;

			group = aimbs_get16(bs);
			subtype = aimbs_get16(bs);

			if (rc)
				rc_addpair(rc, group, subtype);
		}
	}

	/*
	 * We don't pass the rate information up to the client, as it really
	 * doesn't care.  The information is stored in the connection, however
	 * so that we can do more fun stuff later (not really).
	 */

	/*
	 * Last step in the conn init procedure is to acknowledge that we
	 * agree to these draconian limitations.
	 */
	aim_ratesack(sess, rx->conn);

	/*
	 * Finally, tell the client it's ready to go...
	 */
	if ((userfunc = aim_callhandler(sess, rx->conn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNINITDONE)))
		userfunc(sess, rx);


	return 1;
}

static int ratechange(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
{
	aim_rxcallback_t userfunc;
	fu16_t code, rateclass;
	fu32_t currentavg, maxavg, windowsize, clear, alert, limit, disconnect;

	code = aimbs_get16(bs);
	rateclass = aimbs_get16(bs);
	
	windowsize = aimbs_get32(bs);
	clear = aimbs_get32(bs);
	alert = aimbs_get32(bs);
	limit = aimbs_get32(bs);
	disconnect = aimbs_get32(bs);
	currentavg = aimbs_get32(bs);
	maxavg = aimbs_get32(bs);

	if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
		return userfunc(sess, rx, code, rateclass, windowsize, clear, alert, limit, disconnect, currentavg, maxavg);

	return 0;
}

static int selfinfo(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
{
	aim_rxcallback_t userfunc;
	aim_userinfo_t userinfo;

	aim_extractuserinfo(sess, bs, &userinfo);

	if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
		return userfunc(sess, rx, &userinfo);

	return 0;
}

static int evilnotify(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
{
	aim_rxcallback_t userfunc;
	fu16_t newevil;
	aim_userinfo_t userinfo;

	memset(&userinfo, 0, sizeof(aim_userinfo_t));
	
	newevil = aimbs_get16(bs);

	if (aim_bstream_empty(bs))
		aim_extractuserinfo(sess, bs, &userinfo);

	if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
		return userfunc(sess, rx, newevil, &userinfo);

	return 0;
}

/*
 * How Migrations work.  
 *
 * The server sends a Server Pause message, which the client should respond to 
 * with a Server Pause Ack, which contains the families it needs on this 
 * connection. The server will send a Migration Notice with an IP address, and 
 * then disconnect. Next the client should open the connection and send the 
 * cookie.  Repeat the normal login process and pretend this never happened.
 *
 * The Server Pause contains no data.
 *
 */
static int serverpause(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
{
	aim_rxcallback_t userfunc;

	if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
		return userfunc(sess, rx);

	return 0;
}

/*
 * It is rather important that aim_sendpauseack() gets called for the exact
 * same connection that the Server Pause callback was called for, since
 * libfaim extracts the data for the SNAC from the connection structure.
 *
 * Of course, if you don't do that, more bad things happen than just what
 * libfaim can cause.
 *
 */
faim_export int aim_sendpauseack(aim_session_t *sess, aim_conn_t *conn)
{
	aim_frame_t *fr;
	aim_snacid_t snacid;
	aim_conn_inside_t *ins = (aim_conn_inside_t *)conn->inside;
	struct snacgroup *sg;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 1024)))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x0001, 0x000c, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x0001, 0x000c, 0x0000, snacid);

	/*
	 * This list should have all the groups that the original 
	 * Host Online / Server Ready said this host supports.  And 
	 * we want them all back after the migration.
	 */
	for (sg = ins->groups; sg; sg = sg->next)
		aimbs_put16(&fr->data, sg->group);

	aim_tx_enqueue(sess, fr);

	return 0;
}

/*
 * This is the final SNAC sent on the original connection during a migration.
 * It contains the IP and cookie used to connect to the new server, and 
 * optionally a list of the SNAC groups being migrated.
 *
 */
static int migrate(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
{
	aim_rxcallback_t userfunc;
	int ret = 0;
	fu16_t groupcount, i;
	aim_tlvlist_t *tl;
	char *ip = NULL;
	aim_tlv_t *cktlv;

	/*
	 * Apparently there's some fun stuff that can happen right here. The
	 * migration can actually be quite selective about what groups it
	 * moves to the new server.  When not all the groups for a connection
	 * are migrated, or they are all migrated but some groups are moved
	 * to a different server than others, it is called a bifurcated 
	 * migration.
	 *
	 * Let's play dumb and not support that.
	 *
	 */
	groupcount = aimbs_get16(bs);
	for (i = 0; i < groupcount; i++) {
		fu16_t group;

		group = aimbs_get16(bs);

		faimdprintf(sess, 0, "bifurcated migration unsupported -- group 0x%04x\n", group);
	}

	tl = aim_readtlvchain(bs);

	if (aim_gettlv(tl, 0x0005, 1))
		ip = aim_gettlv_str(tl, 0x0005, 1);

	cktlv = aim_gettlv(tl, 0x0006, 1);

	if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
		ret = userfunc(sess, rx, ip, cktlv ? cktlv->value : NULL);

	aim_freetlvchain(&tl);
	free(ip);

	return ret;
}

static int motd(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
{
	aim_rxcallback_t userfunc;
	char *msg = NULL;
	int ret = 0;
	aim_tlvlist_t *tlvlist;
	fu16_t id;

	/*
	 * Code.
	 *
	 * Valid values:
	 *   1 Mandatory upgrade
	 *   2 Advisory upgrade
	 *   3 System bulletin
	 *   4 Nothing's wrong ("top o the world" -- normal)
	 *   5 Lets-break-something. 
	 *
	 */
	id = aimbs_get16(bs);

	/* 
	 * TLVs follow 
	 */
	tlvlist = aim_readtlvchain(bs);

	msg = aim_gettlv_str(tlvlist, 0x000b, 1);

	if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
		ret = userfunc(sess, rx, id, msg);

	free(msg);

	aim_freetlvchain(&tlvlist);

	return ret;
}

faim_internal int aim_setversions(aim_session_t *sess, aim_conn_t *conn)
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

static int hostversions(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
{
	int vercount;
	fu8_t *versions;

	/* This is frivolous. (Thank you SmarterChild.) */
	vercount = aim_bstream_empty(bs)/4;
	versions = aimbs_getraw(bs, aim_bstream_empty(bs));
	free(versions);

	/*
	 * Now request rates.
	 */
	aim_reqrates(sess, rx->conn);

	return 1;
}

/*
 * Starting this past week (26 Mar 2001, say), AOL has started sending
 * this nice little extra SNAC.  AFAIK, it has never been used until now.
 *
 * The request contains eight bytes.  The first four are an offset, the
 * second four are a length.
 *
 * The offset is an offset into aim.exe when it is mapped during execution
 * on Win32.  So far, AOL has only been requesting bytes in static regions
 * of memory.  (I won't put it past them to start requesting data in
 * less static regions -- regions that are initialized at run time, but still
 * before the client recieves this request.)
 *
 * When the client recieves the request, it adds it to the current ds
 * (0x00400000) and dereferences it, copying the data into a buffer which
 * it then runs directly through the MD5 hasher.  The 16 byte output of
 * the hash is then sent back to the server.
 *
 * If the client does not send any data back, or the data does not match
 * the data that the specific client should have, the client will get the
 * following message from "AOL Instant Messenger":
 *    "You have been disconnected from the AOL Instant Message Service (SM) 
 *     for accessing the AOL network using unauthorized software.  You can
 *     download a FREE, fully featured, and authorized client, here 
 *     http://www.aol.com/aim/download2.html"
 * The connection is then closed, recieving disconnect code 1, URL
 * http://www.aim.aol.com/errors/USER_LOGGED_OFF_NEW_LOGIN.html.  
 *
 * Note, however, that numerous inconsistencies can cause the above error, 
 * not just sending back a bad hash.  Do not immediatly suspect this code
 * if you get disconnected.  AOL and the open/free software community have
 * played this game for a couple years now, generating the above message
 * on numerous ocassions.
 *
 * Anyway, neener.  We win again.
 *
 */
static int memrequest(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
{
	aim_rxcallback_t userfunc;
	fu32_t offset, len;
	aim_tlvlist_t *list;
	char *modname;

	offset = aimbs_get32(bs);
	len = aimbs_get32(bs);
	list = aim_readtlvchain(bs);

	modname = aim_gettlv_str(list, 0x0001, 1);

	faimdprintf(sess, 1, "data at 0x%08lx (%d bytes) of requested\n", offset, len, modname ? modname : "aim.exe");

	if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
		return userfunc(sess, rx, offset, len, modname);

	free(modname);
	aim_freetlvchain(&list);

	return 0;
}

#if 0
static void dumpbox(aim_session_t *sess, unsigned char *buf, int len)
{
	int i;

	if (!sess || !buf || !len)
		return;

	faimdprintf(sess, 1, "\nDump of %d bytes at %p:", len, buf);

	for (i = 0; i < len; i++) {
		if ((i % 8) == 0)
			faimdprintf(sess, 1, "\n\t");

		faimdprintf(sess, 1, "0x%2x ", buf[i]);
	}

	faimdprintf(sess, 1, "\n\n");

	return;
}
#endif

faim_export int aim_sendmemblock(aim_session_t *sess, aim_conn_t *conn, fu32_t offset, fu32_t len, const fu8_t *buf, fu8_t flag)
{
	aim_frame_t *fr;
	aim_snacid_t snacid;

	if (!sess || !conn)
		return -EINVAL;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10+2+16)))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x0001, 0x0020, 0x0000, NULL, 0);

	aim_putsnac(&fr->data, 0x0001, 0x0020, 0x0000, snacid);
	aimbs_put16(&fr->data, 0x0010); /* md5 is always 16 bytes */

	if ((flag == AIM_SENDMEMBLOCK_FLAG_ISHASH) && buf && (len == 0x10)) { /* we're getting a hash */

		aimbs_putraw(&fr->data, buf, 0x10); 

	} else if (buf && (len > 0)) { /* use input buffer */
		md5_state_t state;
		md5_byte_t digest[0x10];

		md5_init(&state);	
		md5_append(&state, (const md5_byte_t *)buf, len);
		md5_finish(&state, digest);

		aimbs_putraw(&fr->data, (fu8_t *)digest, 0x10);

	} else if (len == 0) { /* no length, just hash NULL (buf is optional) */
		md5_state_t state;
		fu8_t nil = '\0';
		md5_byte_t digest[0x10];

		/*
		 * These MD5 routines are stupid in that you have to have
		 * at least one append.  So thats why this doesn't look 
		 * real logical.
		 */
		md5_init(&state);
		md5_append(&state, (const md5_byte_t *)&nil, 0);
		md5_finish(&state, digest);

		aimbs_putraw(&fr->data, (fu8_t *)digest, 0x10);

	} else {

		/* 
		 * This data is correct for AIM 3.5.1670.
		 *
		 * Using these blocks is as close to "legal" as you can get
		 * without using an AIM binary.
		 *
		 */
		if ((offset == 0x03ffffff) && (len == 0x03ffffff)) {

#if 1 /* with "AnrbnrAqhfzcd" */
			aimbs_put32(&fr->data, 0x44a95d26);
			aimbs_put32(&fr->data, 0xd2490423);
			aimbs_put32(&fr->data, 0x93b8821f);
			aimbs_put32(&fr->data, 0x51c54b01);
#else /* no filename */
			aimbs_put32(&fr->data, 0x1df8cbae);
			aimbs_put32(&fr->data, 0x5523b839);
			aimbs_put32(&fr->data, 0xa0e10db3);
			aimbs_put32(&fr->data, 0xa46d3b39);
#endif

		} else if ((offset == 0x00001000) && (len == 0x00000000)) {

			aimbs_put32(&fr->data, 0xd41d8cd9);
			aimbs_put32(&fr->data, 0x8f00b204);
			aimbs_put32(&fr->data, 0xe9800998);
			aimbs_put32(&fr->data, 0xecf8427e);

		} else
			faimdprintf(sess, 0, "sendmemblock: WARNING: unknown hash request\n");

	}

	aim_tx_enqueue(sess, fr);

	return 0;
}

static int snachandler(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
{

	if (snac->subtype == 0x0003)
		return hostonline(sess, mod, rx, snac, bs);
	else if (snac->subtype == 0x0005)
		return redirect(sess, mod, rx, snac, bs);
	else if (snac->subtype == 0x0007)
		return rateresp(sess, mod, rx, snac, bs);
	else if (snac->subtype == 0x000a)
		return ratechange(sess, mod, rx, snac, bs);
	else if (snac->subtype == 0x000b)
		return serverpause(sess, mod, rx, snac, bs);
	else if (snac->subtype == 0x000f)
		return selfinfo(sess, mod, rx, snac, bs);
	else if (snac->subtype == 0x0010)
		return evilnotify(sess, mod, rx, snac, bs);
	else if (snac->subtype == 0x0012)
		return migrate(sess, mod, rx, snac, bs);
	else if (snac->subtype == 0x0013)
		return motd(sess, mod, rx, snac, bs);
	else if (snac->subtype == 0x0018)
		return hostversions(sess, mod, rx, snac, bs);
	else if (snac->subtype == 0x001f)
		return memrequest(sess, mod, rx, snac, bs);

	return 0;
}

faim_internal int general_modfirst(aim_session_t *sess, aim_module_t *mod)
{

	mod->family = 0x0001;
	mod->version = 0x0003;
	mod->toolid = 0x0110;
	mod->toolversion = 0x047b;
	mod->flags = 0;
	strncpy(mod->name, "general", sizeof(mod->name));
	mod->snachandler = snachandler;

	return 0;
}

