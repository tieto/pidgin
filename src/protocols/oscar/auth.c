/*
 * Deals with the authorizer (group 0x0017=23, and old-style non-SNAC login).
 *
 */

#define FAIM_INTERNAL
#include <aim.h> 

#include "md5.h"

static int aim_encode_password(const char *password, unsigned char *encoded);

/* 
 * This just pushes the passed cookie onto the passed connection, without
 * the SNAC header or any of that.
 *
 * Very commonly used, as every connection except auth will require this to
 * be the first thing you send.
 *
 */
faim_export int aim_sendcookie(aim_session_t *sess, aim_conn_t *conn, const fu8_t *chipsahoy)
{
	aim_frame_t *fr;
	aim_tlvlist_t *tl = NULL;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x0001, 4+2+2+AIM_COOKIELEN)))
		return -ENOMEM;

	aimbs_put32(&fr->data, 0x00000001);
	aim_addtlvtochain_raw(&tl, 0x0006, AIM_COOKIELEN, chipsahoy);	
	aim_writetlvchain(&fr->data, &tl);
	aim_freetlvchain(&tl);

	aim_tx_enqueue(sess, fr);

	return 0;
}

/*
 * Normally the FLAP version is sent as the first few bytes of the cookie,
 * meaning you generally never call this.
 *
 * But there are times when something might want it seperate. Specifically,
 * libfaim sends this internally when doing SNAC login.
 *
 */
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
 * This is sent back as a general response to the login command.
 * It can be either an error or a success, depending on the
 * precense of certain TLVs.  
 *
 * The client should check the value passed as errorcode. If
 * its nonzero, there was an error.
 *
 */
static int parse(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
{
	aim_tlvlist_t *tlvlist;
	int ret = 0;
	aim_rxcallback_t userfunc;
	char *sn = NULL, *bosip = NULL, *errurl = NULL, *email = NULL;
	unsigned char *cookie = NULL;
	int errorcode = 0, regstatus = 0;
	int latestbuild = 0, latestbetabuild = 0;
	char *latestrelease = NULL, *latestbeta = NULL;
	char *latestreleaseurl = NULL, *latestbetaurl = NULL;
	char *latestreleaseinfo = NULL, *latestbetainfo = NULL;

	/*
	 * Read block of TLVs.  All further data is derived
	 * from what is parsed here.
	 */
	tlvlist = aim_readtlvchain(bs);

	/*
	 * No matter what, we should have a screen name.
	 */
	memset(sess->sn, 0, sizeof(sess->sn));
	if (aim_gettlv(tlvlist, 0x0001, 1)) {
		sn = aim_gettlv_str(tlvlist, 0x0001, 1);
		strncpy(sess->sn, sn, sizeof(sess->sn));
	}

	/*
	 * Check for an error code.  If so, we should also
	 * have an error url.
	 */
	if (aim_gettlv(tlvlist, 0x0008, 1)) 
		errorcode = aim_gettlv16(tlvlist, 0x0008, 1);
	if (aim_gettlv(tlvlist, 0x0004, 1))
		errurl = aim_gettlv_str(tlvlist, 0x0004, 1);

	/*
	 * BOS server address.
	 */
	if (aim_gettlv(tlvlist, 0x0005, 1))
		bosip = aim_gettlv_str(tlvlist, 0x0005, 1);

	/*
	 * Authorization cookie.
	 */
	if (aim_gettlv(tlvlist, 0x0006, 1)) {
		aim_tlv_t *tmptlv;

		tmptlv = aim_gettlv(tlvlist, 0x0006, 1);

		if ((cookie = malloc(tmptlv->length)))
			memcpy(cookie, tmptlv->value, tmptlv->length);
	}

	/*
	 * The email address attached to this account
	 *   Not available for ICQ logins.
	 */
	if (aim_gettlv(tlvlist, 0x0011, 1))
		email = aim_gettlv_str(tlvlist, 0x0011, 1);

	/*
	 * The registration status.  (Not real sure what it means.)
	 *   Not available for ICQ logins.
	 *
	 *   1 = No disclosure
	 *   2 = Limited disclosure
	 *   3 = Full disclosure
	 *
	 * This has to do with whether your email address is available
	 * to other users or not.  AFAIK, this feature is no longer used.
	 *
	 */
	if (aim_gettlv(tlvlist, 0x0013, 1))
		regstatus = aim_gettlv16(tlvlist, 0x0013, 1);

	if (aim_gettlv(tlvlist, 0x0040, 1))
		latestbetabuild = aim_gettlv32(tlvlist, 0x0040, 1);
	if (aim_gettlv(tlvlist, 0x0041, 1))
		latestbetaurl = aim_gettlv_str(tlvlist, 0x0041, 1);
	if (aim_gettlv(tlvlist, 0x0042, 1))
		latestbetainfo = aim_gettlv_str(tlvlist, 0x0042, 1);
	if (aim_gettlv(tlvlist, 0x0043, 1))
		latestbeta = aim_gettlv_str(tlvlist, 0x0043, 1);
	if (aim_gettlv(tlvlist, 0x0048, 1))
		; /* no idea what this is */

	if (aim_gettlv(tlvlist, 0x0044, 1))
		latestbuild = aim_gettlv32(tlvlist, 0x0044, 1);
	if (aim_gettlv(tlvlist, 0x0045, 1))
		latestreleaseurl = aim_gettlv_str(tlvlist, 0x0045, 1);
	if (aim_gettlv(tlvlist, 0x0046, 1))
		latestreleaseinfo = aim_gettlv_str(tlvlist, 0x0046, 1);
	if (aim_gettlv(tlvlist, 0x0047, 1))
		latestrelease = aim_gettlv_str(tlvlist, 0x0047, 1);
	if (aim_gettlv(tlvlist, 0x0049, 1))
		; /* no idea what this is */


	if ((userfunc = aim_callhandler(sess, rx->conn, snac ? snac->family : 0x0017, snac ? snac->subtype : 0x0003))) {
		/* XXX return as a struct? */
		ret = userfunc(sess, rx, sn, errorcode, errurl, regstatus, email, bosip, cookie, latestrelease, latestbuild, latestreleaseurl, latestreleaseinfo, latestbeta, latestbetabuild, latestbetaurl, latestbetainfo);
	}

	free(sn);
	free(bosip);
	free(errurl);
	free(email);
	free(cookie);
	free(latestrelease);
	free(latestreleaseurl);
	free(latestbeta);
	free(latestbetaurl);
	free(latestreleaseinfo);
	free(latestbetainfo);

	aim_freetlvchain(&tlvlist);

	return ret;
}

/*
 * Middle handler for 0017/0007 SNACs.  Contains the auth key prefixed
 * by only its length in a two byte word.
 *
 * Calls the client, which should then use the value to call aim_send_login.
 *
 */
static int keyparse(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
{
	int keylen, ret = 1;
	aim_rxcallback_t userfunc;
	char *keystr;

	keylen = aimbs_get16(bs);
	keystr = aimbs_getstr(bs, keylen);

	if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
		ret = userfunc(sess, rx, keystr);

	free(keystr); 

	return ret;
}

static int snachandler(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
{

	if (snac->subtype == 0x0003)
		return parse(sess, mod, rx, snac, bs);
	else if (snac->subtype == 0x0007)
		return keyparse(sess, mod, rx, snac, bs);

	return 0;
}

faim_internal int auth_modfirst(aim_session_t *sess, aim_module_t *mod)
{

	mod->family = 0x0017;
	mod->version = 0x0000;
	mod->flags = 0;
	strncpy(mod->name, "auth", sizeof(mod->name));
	mod->snachandler = snachandler;

	return 0;
}

