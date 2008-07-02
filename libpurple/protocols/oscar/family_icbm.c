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
 * Family 0x0004 - Routines for sending/receiving Instant Messages.
 *
 * Note the term ICBM (Inter-Client Basic Message) which blankets
 * all types of generically routed through-server messages.  Within
 * the ICBM types (family 4), a channel is defined.  Each channel
 * represents a different type of message.  Channel 1 is used for
 * what would commonly be called an "instant message".  Channel 2
 * is used for negotiating "rendezvous".  These transactions end in
 * something more complex happening, such as a chat invitation, or
 * a file transfer.  Channel 3 is used for chat messages (not in
 * the same family as these channels).  Channel 4 is used for
 * various ICQ messages.  Examples are normal messages, URLs, and
 * old-style authorization.
 *
 * In addition to the channel, every ICBM contains a cookie.  For
 * standard IMs, these are only used for error messages.  However,
 * the more complex rendezvous messages make suitably more complex
 * use of this field.
 *
 * TODO: Split this up into an im.c file an an icbm.c file.  It
 *       will be beautiful, you'll see.
 *
 *       Make sure flap_connection_findbygroup is used by all functions.
 */

#include "oscar.h"
#include "peer.h"

#ifdef _WIN32
#include "win32dep.h"
#endif

#include "util.h"


/**
 * Add a standard ICBM header to the given bstream with the given
 * information.
 *
 * @param bs The bstream to write the ICBM header to.
 * @param c c is for cookie, and cookie is for me.
 * @param channel The ICBM channel (1 through 4).
 * @param sn Null-terminated scrizeen nizame.
 * @return The number of bytes written.  It's really not useful.
 */
static int aim_im_puticbm(ByteStream *bs, const guchar *c, guint16 channel, const char *sn)
{
	byte_stream_putraw(bs, c, 8);
	byte_stream_put16(bs, channel);
	byte_stream_put8(bs, strlen(sn));
	byte_stream_putstr(bs, sn);
	return 8+2+1+strlen(sn);
}

/**
 * Generates a random ICBM cookie in a character array of length 8
 * and copies it into the variable passed as cookie
 * TODO: Maybe we should stop limiting our characters to the visible range?
 */
void aim_icbm_makecookie(guchar *cookie)
{
	int i;

	/* Should be like "21CBF95" and null terminated */
	for (i = 0; i < 7; i++)
		cookie[i] = 0x30 + ((guchar)rand() % 10);
	cookie[7] = '\0';
}

/*
 * Takes a msghdr (and a length) and returns a client type
 * code.  Note that this is *only a guess* and has a low likelihood
 * of actually being accurate.
 *
 * Its based on experimental data, with the help of Eric Warmenhoven
 * who seems to have collected a wide variety of different AIM clients.
 *
 *
 * Heres the current collection:
 *  0501 0003 0101 0101 01		AOL Mobile Communicator, WinAIM 1.0.414
 *  0501 0003 0101 0201 01		WinAIM 2.0.847, 2.1.1187, 3.0.1464,
 *					4.3.2229, 4.4.2286
 *  0501 0004 0101 0102 0101		WinAIM 4.1.2010, libfaim (right here)
 *  0501 0003 0101 02			WinAIM 5
 *  0501 0001 01			iChat x.x, mobile buddies
 *  0501 0001 0101 01			AOL v6.0, CompuServe 2000 v6.0, any TOC client
 *  0501 0002 0106			WinICQ 5.45.1.3777.85
 *
 * Note that in this function, only the feature bytes are tested, since
 * the rest will always be the same.
 *
 */
guint16 aim_im_fingerprint(const guint8 *msghdr, int len)
{
	static const struct {
		guint16 clientid;
		int len;
		guint8 data[10];
	} fingerprints[] = {
		/* AOL Mobile Communicator, WinAIM 1.0.414 */
		{ AIM_CLIENTTYPE_MC,
		  3, {0x01, 0x01, 0x01}},

		/* WinAIM 2.0.847, 2.1.1187, 3.0.1464, 4.3.2229, 4.4.2286 */
		{ AIM_CLIENTTYPE_WINAIM,
		  3, {0x01, 0x01, 0x02}},

		/* WinAIM 4.1.2010, libfaim */
		{ AIM_CLIENTTYPE_WINAIM41,
		  4, {0x01, 0x01, 0x01, 0x02}},

		/* AOL v6.0, CompuServe 2000 v6.0, any TOC client */
		{ AIM_CLIENTTYPE_AOL_TOC,
		  1, {0x01}},

		{ 0, 0, {0x00}}
	};
	int i;

	if (!msghdr || (len <= 0))
		return AIM_CLIENTTYPE_UNKNOWN;

	for (i = 0; fingerprints[i].len; i++) {
		if (fingerprints[i].len != len)
			continue;
		if (memcmp(fingerprints[i].data, msghdr, fingerprints[i].len) == 0)
			return fingerprints[i].clientid;
	}

	return AIM_CLIENTTYPE_UNKNOWN;
}

/**
 * Subtype 0x0002 - Set ICBM parameters.
 *
 * I definitely recommend sending this.  If you don't, you'll be stuck
 * with the rather unreasonable defaults.
 *
 */
int aim_im_setparams(OscarData *od, struct aim_icbmparameters *params)
{
	FlapConnection *conn;
	ByteStream bs;
	aim_snacid_t snacid;

	if (!od || !(conn = flap_connection_findbygroup(od, SNAC_FAMILY_ICBM)))
		return -EINVAL;

	if (!params)
		return -EINVAL;

	byte_stream_new(&bs, 16);

	/* This is read-only (see Parameter Reply). Must be set to zero here. */
	byte_stream_put16(&bs, 0x0000);

	/* These are all read-write */
	byte_stream_put32(&bs, params->flags);
	byte_stream_put16(&bs, params->maxmsglen);
	byte_stream_put16(&bs, params->maxsenderwarn);
	byte_stream_put16(&bs, params->maxrecverwarn);
	byte_stream_put32(&bs, params->minmsginterval);

	snacid = aim_cachesnac(od, SNAC_FAMILY_ICBM, 0x0002, 0x0000, NULL, 0);
	flap_connection_send_snac(od, conn, SNAC_FAMILY_ICBM, 0x0002, 0x0000, snacid, &bs);

	byte_stream_destroy(&bs);

	return 0;
}

/**
 * Subtype 0x0004 - Request ICBM parameter information.
 *
 */
int aim_im_reqparams(OscarData *od)
{
	FlapConnection *conn;

	if (!od || !(conn = flap_connection_findbygroup(od, SNAC_FAMILY_ICBM)))
		return -EINVAL;

	aim_genericreq_n_snacid(od, conn, SNAC_FAMILY_ICBM, 0x0004);

	return 0;
}

/**
 * Subtype 0x0005 - Receive parameter information.
 *
 */
static int aim_im_paraminfo(OscarData *od, FlapConnection *conn, aim_module_t *mod, FlapFrame *frame, aim_modsnac_t *snac, ByteStream *bs)
{
	struct aim_icbmparameters params;

	params.maxchan = byte_stream_get16(bs);
	params.flags = byte_stream_get32(bs);
	params.maxmsglen = byte_stream_get16(bs);
	params.maxsenderwarn = byte_stream_get16(bs);
	params.maxrecverwarn = byte_stream_get16(bs);
	params.minmsginterval = byte_stream_get32(bs);

	params.flags = 0x0000000b | AIM_IMPARAM_FLAG_SUPPORT_OFFLINEMSGS;
	params.maxmsglen = 8000;
	params.minmsginterval = 0;

	aim_im_setparams(od, &params);

	return 0;
}

/**
 * Subtype 0x0006 - Send an ICBM (instant message).
 *
 *
 * Possible flags:
 *   AIM_IMFLAGS_AWAY  -- Marks the message as an autoresponse
 *   AIM_IMFLAGS_ACK   -- Requests that the server send an ack
 *                        when the message is received (of type SNAC_FAMILY_ICBM/0x000c)
 *   AIM_IMFLAGS_OFFLINE--If destination is offline, store it until they are
 *                        online (probably ICQ only).
 *
 * Generally, you should use the lowest encoding possible to send
 * your message.  If you only use basic punctuation and the generic
 * Latin alphabet, use ASCII7 (no flags).  If you happen to use non-ASCII7
 * characters, but they are all clearly defined in ISO-8859-1, then
 * use that.  Keep in mind that not all characters in the PC ASCII8
 * character set are defined in the ISO standard. For those cases (most
 * notably when the (r) symbol is used), you must use the full UNICODE
 * encoding for your message.  In UNICODE mode, _all_ characters must
 * occupy 16bits, including ones that are not special.  (Remember that
 * the first 128 UNICODE symbols are equivalent to ASCII7, however they
 * must be prefixed with a zero high order byte.)
 *
 * I strongly discourage the use of UNICODE mode, mainly because none
 * of the clients I use can parse those messages (and besides that,
 * wchars are difficult and non-portable to handle in most UNIX environments).
 * If you really need to include special characters, use the HTML UNICODE
 * entities.  These are of the form &#2026; where 2026 is the hex
 * representation of the UNICODE index (in this case, UNICODE
 * "Horizontal Ellipsis", or 133 in in ASCII8).
 *
 * Implementation note:  Since this is one of the most-used functions
 * in all of libfaim, it is written with performance in mind.  As such,
 * it is not as clear as it could be in respect to how this message is
 * supposed to be layed out. Most obviously, tlvlists should be used
 * instead of writing out the bytes manually.
 *
 * XXX - more precise verification that we never send SNACs larger than 8192
 * XXX - check SNAC size for multipart
 *
 */
int aim_im_sendch1_ext(OscarData *od, struct aim_sendimext_args *args)
{
	FlapConnection *conn;
	aim_snacid_t snacid;
	ByteStream data;
	guchar cookie[8];
	int msgtlvlen;
	static const guint8 deffeatures[] = { 0x01, 0x01, 0x01, 0x02 };

	if (!od || !(conn = flap_connection_findbygroup(od, SNAC_FAMILY_ICBM)))
		return -EINVAL;

	if (!args)
		return -EINVAL;

	if (args->flags & AIM_IMFLAGS_MULTIPART) {
		if (args->mpmsg->numparts == 0)
			return -EINVAL;
	} else {
		if (!args->msg || (args->msglen <= 0))
			return -EINVAL;

		if (args->msglen > MAXMSGLEN)
			return -E2BIG;
	}

	/* Painfully calculate the size of the message TLV */
	msgtlvlen = 1 + 1; /* 0501 */

	if (args->flags & AIM_IMFLAGS_CUSTOMFEATURES)
		msgtlvlen += 2 + args->featureslen;
	else
		msgtlvlen += 2 + sizeof(deffeatures);

	if (args->flags & AIM_IMFLAGS_MULTIPART) {
		aim_mpmsg_section_t *sec;

		for (sec = args->mpmsg->parts; sec; sec = sec->next) {
			msgtlvlen += 2 /* 0101 */ + 2 /* block len */;
			msgtlvlen += 4 /* charset */ + sec->datalen;
		}

	} else {
		msgtlvlen += 2 /* 0101 */ + 2 /* block len */;
		msgtlvlen += 4 /* charset */ + args->msglen;
	}

	byte_stream_new(&data, msgtlvlen + 128);

	/* Generate an ICBM cookie */
	aim_icbm_makecookie(cookie);

	/* ICBM header */
	aim_im_puticbm(&data, cookie, 0x0001, args->destsn);

	/* Message TLV (type 0x0002) */
	byte_stream_put16(&data, 0x0002);
	byte_stream_put16(&data, msgtlvlen);

	/* Features TLV (type 0x0501) */
	byte_stream_put16(&data, 0x0501);
	if (args->flags & AIM_IMFLAGS_CUSTOMFEATURES) {
		byte_stream_put16(&data, args->featureslen);
		byte_stream_putraw(&data, args->features, args->featureslen);
	} else {
		byte_stream_put16(&data, sizeof(deffeatures));
		byte_stream_putraw(&data, deffeatures, sizeof(deffeatures));
	}

	if (args->flags & AIM_IMFLAGS_MULTIPART) {
		aim_mpmsg_section_t *sec;

		/* Insert each message part in a TLV (type 0x0101) */
		for (sec = args->mpmsg->parts; sec; sec = sec->next) {
			byte_stream_put16(&data, 0x0101);
			byte_stream_put16(&data, sec->datalen + 4);
			byte_stream_put16(&data, sec->charset);
			byte_stream_put16(&data, sec->charsubset);
			byte_stream_putraw(&data, (guchar *)sec->data, sec->datalen);
		}

	} else {

		/* Insert message text in a TLV (type 0x0101) */
		byte_stream_put16(&data, 0x0101);

		/* Message block length */
		byte_stream_put16(&data, args->msglen + 0x04);

		/* Character set */
		byte_stream_put16(&data, args->charset);
		byte_stream_put16(&data, args->charsubset);

		/* Message.  Not terminated */
		byte_stream_putraw(&data, (guchar *)args->msg, args->msglen);
	}

	/* Set the Autoresponse flag */
	if (args->flags & AIM_IMFLAGS_AWAY) {
		byte_stream_put16(&data, 0x0004);
		byte_stream_put16(&data, 0x0000);
	} else {
		if (args->flags & AIM_IMFLAGS_ACK) {
			/* Set the Request Acknowledge flag */
			byte_stream_put16(&data, 0x0003);
			byte_stream_put16(&data, 0x0000);
		}

		if (args->flags & AIM_IMFLAGS_OFFLINE) {
			/* Allow this message to be queued as an offline message */
			byte_stream_put16(&data, 0x0006);
			byte_stream_put16(&data, 0x0000);
		}
	}

	/*
	 * Set the I HAVE A REALLY PURTY ICON flag.
	 * XXX - This should really only be sent on initial
	 * IMs and when you change your icon.
	 */
	if (args->flags & AIM_IMFLAGS_HASICON) {
		byte_stream_put16(&data, 0x0008);
		byte_stream_put16(&data, 0x000c);
		byte_stream_put32(&data, args->iconlen);
		byte_stream_put16(&data, 0x0001);
		byte_stream_put16(&data, args->iconsum);
		byte_stream_put32(&data, args->iconstamp);
	}

	/*
	 * Set the Buddy Icon Requested flag.
	 * XXX - Every time?  Surely not...
	 */
	if (args->flags & AIM_IMFLAGS_BUDDYREQ) {
		byte_stream_put16(&data, 0x0009);
		byte_stream_put16(&data, 0x0000);
	}

	/* XXX - should be optional */
	snacid = aim_cachesnac(od, SNAC_FAMILY_ICBM, 0x0006, 0x0000, args->destsn, strlen(args->destsn)+1);

	flap_connection_send_snac(od, conn, SNAC_FAMILY_ICBM, 0x0006, 0x0000, snacid, &data);
	byte_stream_destroy(&data);

	/* clean out SNACs over 60sec old */
	aim_cleansnacs(od, 60);

	return 0;
}

/*
 * Simple wrapper for aim_im_sendch1_ext()
 *
 * You cannot use aim_send_im if you need the HASICON flag.  You must
 * use aim_im_sendch1_ext directly for that.
 *
 * aim_send_im also cannot be used if you require UNICODE messages, because
 * that requires an explicit message length.  Use aim_im_sendch1_ext().
 *
 */
int aim_im_sendch1(OscarData *od, const char *sn, guint16 flags, const char *msg)
{
	struct aim_sendimext_args args;

	args.destsn = sn;
	args.flags = flags;
	args.msg = msg;
	args.msglen = strlen(msg);
	args.charset = 0x0000;
	args.charsubset = 0x0000;

	/* Make these don't get set by accident -- they need aim_im_sendch1_ext */
	args.flags &= ~(AIM_IMFLAGS_CUSTOMFEATURES | AIM_IMFLAGS_HASICON | AIM_IMFLAGS_MULTIPART);

	return aim_im_sendch1_ext(od, &args);
}

/*
 * Subtype 0x0006 - Send a chat invitation.
 */
int aim_im_sendch2_chatinvite(OscarData *od, const char *sn, const char *msg, guint16 exchange, const char *roomname, guint16 instance)
{
	FlapConnection *conn;
	ByteStream bs;
	aim_snacid_t snacid;
	IcbmCookie *msgcookie;
	struct aim_invite_priv *priv;
	guchar cookie[8];
	GSList *outer_tlvlist = NULL, *inner_tlvlist = NULL;
	ByteStream hdrbs;

	if (!od || !(conn = flap_connection_findbygroup(od, SNAC_FAMILY_ICBM)))
		return -EINVAL;

	if (!sn || !msg || !roomname)
		return -EINVAL;

	aim_icbm_makecookie(cookie);

	byte_stream_new(&bs, 1142+strlen(sn)+strlen(roomname)+strlen(msg));

	snacid = aim_cachesnac(od, SNAC_FAMILY_ICBM, 0x0006, 0x0000, sn, strlen(sn)+1);

	/* XXX should be uncached by an unwritten 'invite accept' handler */
	priv = g_malloc(sizeof(struct aim_invite_priv));
	priv->sn = g_strdup(sn);
	priv->roomname = g_strdup(roomname);
	priv->exchange = exchange;
	priv->instance = instance;

	if ((msgcookie = aim_mkcookie(cookie, AIM_COOKIETYPE_INVITE, priv)))
		aim_cachecookie(od, msgcookie);
	else
		g_free(priv);

	/* ICBM Header */
	aim_im_puticbm(&bs, cookie, 0x0002, sn);

	/*
	 * TLV t(0005)
	 *
	 * Everything else is inside this TLV.
	 *
	 * Sigh.  AOL was rather inconsistent right here.  So we have
	 * to play some minor tricks.  Right inside the type 5 is some
	 * raw data, followed by a series of TLVs.
	 *
	 */
	byte_stream_new(&hdrbs, 2+8+16+6+4+4+strlen(msg)+4+2+1+strlen(roomname)+2);

	byte_stream_put16(&hdrbs, 0x0000); /* Unknown! */
	byte_stream_putraw(&hdrbs, cookie, sizeof(cookie)); /* I think... */
	byte_stream_putcaps(&hdrbs, OSCAR_CAPABILITY_CHAT);

	aim_tlvlist_add_16(&inner_tlvlist, 0x000a, 0x0001);
	aim_tlvlist_add_noval(&inner_tlvlist, 0x000f);
	aim_tlvlist_add_str(&inner_tlvlist, 0x000c, msg);
	aim_tlvlist_add_chatroom(&inner_tlvlist, 0x2711, exchange, roomname, instance);
	aim_tlvlist_write(&hdrbs, &inner_tlvlist);

	aim_tlvlist_add_raw(&outer_tlvlist, 0x0005, byte_stream_curpos(&hdrbs), hdrbs.data);
	byte_stream_destroy(&hdrbs);

	aim_tlvlist_write(&bs, &outer_tlvlist);

	aim_tlvlist_free(inner_tlvlist);
	aim_tlvlist_free(outer_tlvlist);

	flap_connection_send_snac(od, conn, SNAC_FAMILY_ICBM, 0x0006, 0x0000, snacid, &bs);

	byte_stream_destroy(&bs);

	return 0;
}

/**
 * Subtype 0x0006 - Send your icon to a given user.
 *
 * This is also performance sensitive. (If you can believe it...)
 *
 */
int aim_im_sendch2_icon(OscarData *od, const char *sn, const guint8 *icon, int iconlen, time_t stamp, guint16 iconsum)
{
	FlapConnection *conn;
	ByteStream bs;
	aim_snacid_t snacid;
	guchar cookie[8];

	if (!od || !(conn = flap_connection_findbygroup(od, SNAC_FAMILY_ICBM)))
		return -EINVAL;

	if (!sn || !icon || (iconlen <= 0) || (iconlen >= MAXICONLEN))
		return -EINVAL;

	aim_icbm_makecookie(cookie);

	byte_stream_new(&bs, 8+2+1+strlen(sn)+2+2+2+8+16+2+2+2+2+2+2+2+4+4+4+iconlen+strlen(AIM_ICONIDENT)+2+2);

	snacid = aim_cachesnac(od, SNAC_FAMILY_ICBM, 0x0006, 0x0000, NULL, 0);

	/* ICBM header */
	aim_im_puticbm(&bs, cookie, 0x0002, sn);

	/*
	 * TLV t(0005)
	 *
	 * Encompasses everything below.
	 */
	byte_stream_put16(&bs, 0x0005);
	byte_stream_put16(&bs, 2+8+16+6+4+4+iconlen+4+4+4+strlen(AIM_ICONIDENT));

	byte_stream_put16(&bs, 0x0000);
	byte_stream_putraw(&bs, cookie, 8);
	byte_stream_putcaps(&bs, OSCAR_CAPABILITY_BUDDYICON);

	/* TLV t(000a) */
	byte_stream_put16(&bs, 0x000a);
	byte_stream_put16(&bs, 0x0002);
	byte_stream_put16(&bs, 0x0001);

	/* TLV t(000f) */
	byte_stream_put16(&bs, 0x000f);
	byte_stream_put16(&bs, 0x0000);

	/* TLV t(2711) */
	byte_stream_put16(&bs, 0x2711);
	byte_stream_put16(&bs, 4+4+4+iconlen+strlen(AIM_ICONIDENT));
	byte_stream_put16(&bs, 0x0000);
	byte_stream_put16(&bs, iconsum);
	byte_stream_put32(&bs, iconlen);
	byte_stream_put32(&bs, stamp);
	byte_stream_putraw(&bs, icon, iconlen);
	byte_stream_putstr(&bs, AIM_ICONIDENT);

	/* TLV t(0003) */
	byte_stream_put16(&bs, 0x0003);
	byte_stream_put16(&bs, 0x0000);

	flap_connection_send_snac(od, conn, SNAC_FAMILY_ICBM, 0x0006, 0x0000, snacid, &bs);

	byte_stream_destroy(&bs);

	return 0;
}

/*
 * Subtype 0x0006 - Send a rich text message.
 *
 * This only works for ICQ 2001b (thats 2001 not 2000).  Better, only
 * send it to clients advertising the RTF capability.  In fact, if you send
 * it to a client that doesn't support that capability, the server will gladly
 * bounce it back to you.
 *
 * You'd think this would be in icq.c, but, well, I'm trying to stick with
 * the one-group-per-file scheme as much as possible.  This could easily
 * be an exception, since Rendezvous IMs are external of the Oscar core,
 * and therefore are undefined.  Really I just need to think of a good way to
 * make an interface similar to what AOL actually uses.  But I'm not using COM.
 *
 */
int aim_im_sendch2_rtfmsg(OscarData *od, struct aim_sendrtfmsg_args *args)
{
	FlapConnection *conn;
	ByteStream bs;
	aim_snacid_t snacid;
	guchar cookie[8];
	const char rtfcap[] = {"{97B12751-243C-4334-AD22-D6ABF73F1492}"}; /* OSCAR_CAPABILITY_ICQRTF capability in string form */
	int servdatalen;

	if (!od || !(conn = flap_connection_findbygroup(od, SNAC_FAMILY_ICBM)))
		return -EINVAL;

	if (!args || !args->destsn || !args->rtfmsg)
		return -EINVAL;

	servdatalen = 2+2+16+2+4+1+2  +  2+2+4+4+4  +  2+4+2+strlen(args->rtfmsg)+1  +  4+4+4+strlen(rtfcap)+1;

	aim_icbm_makecookie(cookie);

	byte_stream_new(&bs, 128+servdatalen);

	snacid = aim_cachesnac(od, SNAC_FAMILY_ICBM, 0x0006, 0x0000, NULL, 0);

	/* ICBM header */
	aim_im_puticbm(&bs, cookie, 0x0002, args->destsn);

	/* TLV t(0005) - Encompasses everything below. */
	byte_stream_put16(&bs, 0x0005);
	byte_stream_put16(&bs, 2+8+16  +  2+2+2  +  2+2  +  2+2+servdatalen);

	byte_stream_put16(&bs, 0x0000);
	byte_stream_putraw(&bs, cookie, 8);
	byte_stream_putcaps(&bs, OSCAR_CAPABILITY_ICQSERVERRELAY);

	/* t(000a) l(0002) v(0001) */
	byte_stream_put16(&bs, 0x000a);
	byte_stream_put16(&bs, 0x0002);
	byte_stream_put16(&bs, 0x0001);

	/* t(000f) l(0000) v() */
	byte_stream_put16(&bs, 0x000f);
	byte_stream_put16(&bs, 0x0000);

	/* Service Data TLV */
	byte_stream_put16(&bs, 0x2711);
	byte_stream_put16(&bs, servdatalen);

	byte_stream_putle16(&bs, 11 + 16 /* 11 + (sizeof CLSID) */);
	byte_stream_putle16(&bs, 9);
	byte_stream_putcaps(&bs, OSCAR_CAPABILITY_EMPTY);
	byte_stream_putle16(&bs, 0);
	byte_stream_putle32(&bs, 0);
	byte_stream_putle8(&bs, 0);
	byte_stream_putle16(&bs, 0x03ea); /* trid1 */

	byte_stream_putle16(&bs, 14);
	byte_stream_putle16(&bs, 0x03eb); /* trid2 */
	byte_stream_putle32(&bs, 0);
	byte_stream_putle32(&bs, 0);
	byte_stream_putle32(&bs, 0);

	byte_stream_putle16(&bs, 0x0001);
	byte_stream_putle32(&bs, 0);
	byte_stream_putle16(&bs, strlen(args->rtfmsg)+1);
	byte_stream_putraw(&bs, (const guint8 *)args->rtfmsg, strlen(args->rtfmsg)+1);

	byte_stream_putle32(&bs, args->fgcolor);
	byte_stream_putle32(&bs, args->bgcolor);
	byte_stream_putle32(&bs, strlen(rtfcap)+1);
	byte_stream_putraw(&bs, (const guint8 *)rtfcap, strlen(rtfcap)+1);

	flap_connection_send_snac(od, conn, SNAC_FAMILY_ICBM, 0x0006, 0x0000, snacid, &bs);

	byte_stream_destroy(&bs);

	return 0;
}

/**
 * Cancel a rendezvous invitation.  It could be an invitation to
 * establish a direct connection, or a file-send, or a chat invite.
 */
void
aim_im_sendch2_cancel(PeerConnection *peer_conn)
{
	OscarData *od;
	FlapConnection *conn;
	ByteStream bs;
	aim_snacid_t snacid;
	GSList *outer_tlvlist = NULL, *inner_tlvlist = NULL;
	ByteStream hdrbs;

	od = peer_conn->od;
	conn = flap_connection_findbygroup(od, SNAC_FAMILY_ICBM);
	if (conn == NULL)
		return;

	byte_stream_new(&bs, 118+strlen(peer_conn->sn));

	snacid = aim_cachesnac(od, SNAC_FAMILY_ICBM, 0x0006, 0x0000, NULL, 0);

	/* ICBM header */
	aim_im_puticbm(&bs, peer_conn->cookie, 0x0002, peer_conn->sn);

	aim_tlvlist_add_noval(&outer_tlvlist, 0x0003);

	byte_stream_new(&hdrbs, 64);

	byte_stream_put16(&hdrbs, AIM_RENDEZVOUS_CANCEL);
	byte_stream_putraw(&hdrbs, peer_conn->cookie, 8);
	byte_stream_putcaps(&hdrbs, peer_conn->type);

	/* This TLV means "cancel!" */
	aim_tlvlist_add_16(&inner_tlvlist, 0x000b, 0x0001);
	aim_tlvlist_write(&hdrbs, &inner_tlvlist);

	aim_tlvlist_add_raw(&outer_tlvlist, 0x0005, byte_stream_curpos(&hdrbs), hdrbs.data);
	byte_stream_destroy(&hdrbs);

	aim_tlvlist_write(&bs, &outer_tlvlist);

	aim_tlvlist_free(inner_tlvlist);
	aim_tlvlist_free(outer_tlvlist);

	flap_connection_send_snac(od, conn, SNAC_FAMILY_ICBM, 0x0006, 0x0000, snacid, &bs);

	byte_stream_destroy(&bs);
}

/**
 * Subtype 0x0006 - Send an "I accept and I've connected to
 * you" message.
 */
void
aim_im_sendch2_connected(PeerConnection *peer_conn)
{
	OscarData *od;
	FlapConnection *conn;
	ByteStream bs;
	aim_snacid_t snacid;

	od = peer_conn->od;
	conn = flap_connection_findbygroup(od, SNAC_FAMILY_ICBM);
	if (conn == NULL)
		return;

	byte_stream_new(&bs, 11+strlen(peer_conn->sn) + 4+2+8+16);

	snacid = aim_cachesnac(od, SNAC_FAMILY_ICBM, 0x0006, 0x0000, NULL, 0);

	/* ICBM header */
	aim_im_puticbm(&bs, peer_conn->cookie, 0x0002, peer_conn->sn);

	byte_stream_put16(&bs, 0x0005);
	byte_stream_put16(&bs, 0x001a);
	byte_stream_put16(&bs, AIM_RENDEZVOUS_CONNECTED);
	byte_stream_putraw(&bs, peer_conn->cookie, 8);
	byte_stream_putcaps(&bs, peer_conn->type);

	flap_connection_send_snac(od, conn, SNAC_FAMILY_ICBM, 0x0006, 0x0000, snacid, &bs);

	byte_stream_destroy(&bs);
}

/**
 * Subtype 0x0006 - Send a direct connect rendezvous ICBM.  This
 * could have a number of meanings, depending on the content:
 * "I want you to connect to me"
 * "I want to connect to you"
 * "I want to connect through a proxy server"
 */
void
aim_im_sendch2_odc_requestdirect(OscarData *od, guchar *cookie, const char *sn, const guint8 *ip, guint16 port, guint16 requestnumber)
{
	FlapConnection *conn;
	ByteStream bs;
	aim_snacid_t snacid;
	GSList *outer_tlvlist = NULL, *inner_tlvlist = NULL;
	ByteStream hdrbs;

	conn = flap_connection_findbygroup(od, SNAC_FAMILY_ICBM);
	if (conn == NULL)
		return;

	byte_stream_new(&bs, 246+strlen(sn));

	snacid = aim_cachesnac(od, SNAC_FAMILY_ICBM, 0x0006, 0x0000, NULL, 0);

	/* ICBM header */
	aim_im_puticbm(&bs, cookie, 0x0002, sn);

	aim_tlvlist_add_noval(&outer_tlvlist, 0x0003);

	byte_stream_new(&hdrbs, 128);

	byte_stream_put16(&hdrbs, AIM_RENDEZVOUS_PROPOSE);
	byte_stream_putraw(&hdrbs, cookie, 8);
	byte_stream_putcaps(&hdrbs, OSCAR_CAPABILITY_DIRECTIM);

	aim_tlvlist_add_raw(&inner_tlvlist, 0x0002, 4, ip);
	aim_tlvlist_add_raw(&inner_tlvlist, 0x0003, 4, ip);
	aim_tlvlist_add_16(&inner_tlvlist, 0x0005, port);
	aim_tlvlist_add_16(&inner_tlvlist, 0x000a, requestnumber);
	aim_tlvlist_add_noval(&inner_tlvlist, 0x000f);
	aim_tlvlist_write(&hdrbs, &inner_tlvlist);

	aim_tlvlist_add_raw(&outer_tlvlist, 0x0005, byte_stream_curpos(&hdrbs), hdrbs.data);
	byte_stream_destroy(&hdrbs);

	aim_tlvlist_write(&bs, &outer_tlvlist);

	aim_tlvlist_free(inner_tlvlist);
	aim_tlvlist_free(outer_tlvlist);

	flap_connection_send_snac(od, conn, SNAC_FAMILY_ICBM, 0x0006, 0x0000, snacid, &bs);

	byte_stream_destroy(&bs);
}

/**
 * Subtype 0x0006 - Send a direct connect rendezvous ICBM asking the
 * remote user to connect to us via a proxy server.
 */
void
aim_im_sendch2_odc_requestproxy(OscarData *od, guchar *cookie, const char *sn, const guint8 *ip, guint16 pin, guint16 requestnumber)
{
	FlapConnection *conn;
	ByteStream bs;
	aim_snacid_t snacid;
	GSList *outer_tlvlist = NULL, *inner_tlvlist = NULL;
	ByteStream hdrbs;
	guint8 ip_comp[4];

	conn = flap_connection_findbygroup(od, SNAC_FAMILY_ICBM);
	if (conn == NULL)
		return;

	byte_stream_new(&bs, 246+strlen(sn));

	snacid = aim_cachesnac(od, SNAC_FAMILY_ICBM, 0x0006, 0x0000, NULL, 0);

	/* ICBM header */
	aim_im_puticbm(&bs, cookie, 0x0002, sn);

	aim_tlvlist_add_noval(&outer_tlvlist, 0x0003);

	byte_stream_new(&hdrbs, 128);

	byte_stream_put16(&hdrbs, AIM_RENDEZVOUS_PROPOSE);
	byte_stream_putraw(&hdrbs, cookie, 8);
	byte_stream_putcaps(&hdrbs, OSCAR_CAPABILITY_DIRECTIM);

	aim_tlvlist_add_raw(&inner_tlvlist, 0x0002, 4, ip);
	aim_tlvlist_add_raw(&inner_tlvlist, 0x0003, 4, ip);
	aim_tlvlist_add_16(&inner_tlvlist, 0x0005, pin);
	aim_tlvlist_add_16(&inner_tlvlist, 0x000a, requestnumber);
	aim_tlvlist_add_noval(&inner_tlvlist, 0x000f);
	aim_tlvlist_add_noval(&inner_tlvlist, 0x0010);

	/* Send the bitwise complement of the port and ip.  As a check? */
	ip_comp[0] = ~ip[0];
	ip_comp[1] = ~ip[1];
	ip_comp[2] = ~ip[2];
	ip_comp[3] = ~ip[3];
	aim_tlvlist_add_raw(&inner_tlvlist, 0x0016, 4, ip_comp);
	aim_tlvlist_add_16(&inner_tlvlist, 0x0017, ~pin);

	aim_tlvlist_write(&hdrbs, &inner_tlvlist);

	aim_tlvlist_add_raw(&outer_tlvlist, 0x0005, byte_stream_curpos(&hdrbs), hdrbs.data);
	byte_stream_destroy(&hdrbs);

	aim_tlvlist_write(&bs, &outer_tlvlist);

	aim_tlvlist_free(inner_tlvlist);
	aim_tlvlist_free(outer_tlvlist);

	flap_connection_send_snac(od, conn, SNAC_FAMILY_ICBM, 0x0006, 0x0000, snacid, &bs);

	byte_stream_destroy(&bs);
}

/**
 * Subtype 0x0006 - Send an "I want to send you this file" message
 *
 */
void
aim_im_sendch2_sendfile_requestdirect(OscarData *od, guchar *cookie, const char *sn, const guint8 *ip, guint16 port, guint16 requestnumber, const gchar *filename, guint32 size, guint16 numfiles)
{
	FlapConnection *conn;
	ByteStream bs;
	aim_snacid_t snacid;
	GSList *outer_tlvlist = NULL, *inner_tlvlist = NULL;
	ByteStream hdrbs;

	conn = flap_connection_findbygroup(od, SNAC_FAMILY_ICBM);
	if (conn == NULL)
		return;

	byte_stream_new(&bs, 1014);

	snacid = aim_cachesnac(od, SNAC_FAMILY_ICBM, 0x0006, 0x0000, NULL, 0);

	/* ICBM header */
	aim_im_puticbm(&bs, cookie, 0x0002, sn);

	aim_tlvlist_add_noval(&outer_tlvlist, 0x0003);

	byte_stream_new(&hdrbs, 512);

	byte_stream_put16(&hdrbs, AIM_RENDEZVOUS_PROPOSE);
	byte_stream_putraw(&hdrbs, cookie, 8);
	byte_stream_putcaps(&hdrbs, OSCAR_CAPABILITY_SENDFILE);

	aim_tlvlist_add_raw(&inner_tlvlist, 0x0002, 4, ip);
	aim_tlvlist_add_raw(&inner_tlvlist, 0x0003, 4, ip);
	aim_tlvlist_add_16(&inner_tlvlist, 0x0005, port);
	aim_tlvlist_add_16(&inner_tlvlist, 0x000a, requestnumber);
	aim_tlvlist_add_noval(&inner_tlvlist, 0x000f);
	/* TODO: Send 0x0016 and 0x0017 */

#if 0
	/* TODO: If the following is ever enabled, ensure that it is
	 *       not sent with a receive redirect or stage 3 proxy
	 *       redirect for a file receive (same conditions for
	 *       sending 0x000f above)
	 */
	aim_tlvlist_add_raw(&inner_tlvlist, 0x000e, 2, "en");
	aim_tlvlist_add_raw(&inner_tlvlist, 0x000d, 8, "us-ascii");
	aim_tlvlist_add_raw(&inner_tlvlist, 0x000c, 24, "Please accept this file.");
#endif

	if (filename != NULL)
	{
		ByteStream inner_bs;

		/* Begin TLV t(2711) */
		byte_stream_new(&inner_bs, 2+2+4+strlen(filename)+1);
		byte_stream_put16(&inner_bs, (numfiles > 1) ? 0x0002 : 0x0001);
		byte_stream_put16(&inner_bs, numfiles);
		byte_stream_put32(&inner_bs, size);

		/* Filename - NULL terminated, for some odd reason */
		byte_stream_putstr(&inner_bs, filename);
		byte_stream_put8(&inner_bs, 0x00);

		aim_tlvlist_add_raw(&inner_tlvlist, 0x2711, inner_bs.len, inner_bs.data);
		byte_stream_destroy(&inner_bs);
		/* End TLV t(2711) */
	}

	aim_tlvlist_write(&hdrbs, &inner_tlvlist);
	aim_tlvlist_add_raw(&outer_tlvlist, 0x0005, byte_stream_curpos(&hdrbs), hdrbs.data);
	byte_stream_destroy(&hdrbs);

	aim_tlvlist_write(&bs, &outer_tlvlist);

	aim_tlvlist_free(inner_tlvlist);
	aim_tlvlist_free(outer_tlvlist);

	flap_connection_send_snac(od, conn, SNAC_FAMILY_ICBM, 0x0006, 0x0000, snacid, &bs);

	byte_stream_destroy(&bs);
}

/**
 * Subtype 0x0006 - Send a sendfile connect rendezvous ICBM asking the
 * remote user to connect to us via a proxy server.
 */
void
aim_im_sendch2_sendfile_requestproxy(OscarData *od, guchar *cookie, const char *sn, const guint8 *ip, guint16 pin, guint16 requestnumber, const gchar *filename, guint32 size, guint16 numfiles)
{
	FlapConnection *conn;
	ByteStream bs;
	aim_snacid_t snacid;
	GSList *outer_tlvlist = NULL, *inner_tlvlist = NULL;
	ByteStream hdrbs;
	guint8 ip_comp[4];

	conn = flap_connection_findbygroup(od, SNAC_FAMILY_ICBM);
	if (conn == NULL)
		return;

	byte_stream_new(&bs, 1014);

	snacid = aim_cachesnac(od, SNAC_FAMILY_ICBM, 0x0006, 0x0000, NULL, 0);

	/* ICBM header */
	aim_im_puticbm(&bs, cookie, 0x0002, sn);

	aim_tlvlist_add_noval(&outer_tlvlist, 0x0003);

	byte_stream_new(&hdrbs, 512);

	byte_stream_put16(&hdrbs, AIM_RENDEZVOUS_PROPOSE);
	byte_stream_putraw(&hdrbs, cookie, 8);
	byte_stream_putcaps(&hdrbs, OSCAR_CAPABILITY_SENDFILE);

	aim_tlvlist_add_raw(&inner_tlvlist, 0x0002, 4, ip);
	aim_tlvlist_add_raw(&inner_tlvlist, 0x0003, 4, ip);
	aim_tlvlist_add_16(&inner_tlvlist, 0x0005, pin);
	aim_tlvlist_add_16(&inner_tlvlist, 0x000a, requestnumber);
	aim_tlvlist_add_noval(&inner_tlvlist, 0x000f);
	aim_tlvlist_add_noval(&inner_tlvlist, 0x0010);

	/* Send the bitwise complement of the port and ip.  As a check? */
	ip_comp[0] = ~ip[0];
	ip_comp[1] = ~ip[1];
	ip_comp[2] = ~ip[2];
	ip_comp[3] = ~ip[3];
	aim_tlvlist_add_raw(&inner_tlvlist, 0x0016, 4, ip_comp);
	aim_tlvlist_add_16(&inner_tlvlist, 0x0017, ~pin);

#if 0
	/* TODO: If the following is ever enabled, ensure that it is
	 *       not sent with a receive redirect or stage 3 proxy
	 *       redirect for a file receive (same conditions for
	 *       sending 0x000f above)
	 */
	aim_tlvlist_add_raw(&inner_tlvlist, 0x000e, 2, "en");
	aim_tlvlist_add_raw(&inner_tlvlist, 0x000d, 8, "us-ascii");
	aim_tlvlist_add_raw(&inner_tlvlist, 0x000c, 24, "Please accept this file.");
#endif

	if (filename != NULL)
	{
		ByteStream filename_bs;

		/* Begin TLV t(2711) */
		byte_stream_new(&filename_bs, 2+2+4+strlen(filename)+1);
		byte_stream_put16(&filename_bs, (numfiles > 1) ? 0x0002 : 0x0001);
		byte_stream_put16(&filename_bs, numfiles);
		byte_stream_put32(&filename_bs, size);

		/* Filename - NULL terminated, for some odd reason */
		byte_stream_putstr(&filename_bs, filename);
		byte_stream_put8(&filename_bs, 0x00);

		aim_tlvlist_add_raw(&inner_tlvlist, 0x2711, filename_bs.len, filename_bs.data);
		byte_stream_destroy(&filename_bs);
		/* End TLV t(2711) */
	}

	aim_tlvlist_write(&hdrbs, &inner_tlvlist);

	aim_tlvlist_add_raw(&outer_tlvlist, 0x0005, byte_stream_curpos(&hdrbs), hdrbs.data);
	byte_stream_destroy(&hdrbs);

	aim_tlvlist_write(&bs, &outer_tlvlist);

	aim_tlvlist_free(inner_tlvlist);
	aim_tlvlist_free(outer_tlvlist);

	flap_connection_send_snac(od, conn, SNAC_FAMILY_ICBM, 0x0006, 0x0000, snacid, &bs);

	byte_stream_destroy(&bs);
}

/**
 * Subtype 0x0006 - Request the status message of the given ICQ user.
 *
 * @param od The oscar session.
 * @param sn The UIN of the user of whom you wish to request info.
 * @param type The type of info you wish to request.  This should be the current
 *        state of the user, as one of the AIM_ICQ_STATE_* defines.
 * @return Return 0 if no errors, otherwise return the error number.
 */
int aim_im_sendch2_geticqaway(OscarData *od, const char *sn, int type)
{
	FlapConnection *conn;
	ByteStream bs;
	aim_snacid_t snacid;
	guchar cookie[8];

	if (!od || !(conn = flap_connection_findbygroup(od, SNAC_FAMILY_ICBM)) || !sn)
		return -EINVAL;

	aim_icbm_makecookie(cookie);

	byte_stream_new(&bs, 8+2+1+strlen(sn) + 4+0x5e + 4);

	snacid = aim_cachesnac(od, SNAC_FAMILY_ICBM, 0x0006, 0x0000, NULL, 0);

	/* ICBM header */
	aim_im_puticbm(&bs, cookie, 0x0002, sn);

	/* TLV t(0005) - Encompasses almost everything below. */
	byte_stream_put16(&bs, 0x0005); /* T */
	byte_stream_put16(&bs, 0x005e); /* L */
	{ /* V */
		byte_stream_put16(&bs, 0x0000);

		/* Cookie */
		byte_stream_putraw(&bs, cookie, 8);

		/* Put the 16 byte server relay capability */
		byte_stream_putcaps(&bs, OSCAR_CAPABILITY_ICQSERVERRELAY);

		/* TLV t(000a) */
		byte_stream_put16(&bs, 0x000a);
		byte_stream_put16(&bs, 0x0002);
		byte_stream_put16(&bs, 0x0001);

		/* TLV t(000f) */
		byte_stream_put16(&bs, 0x000f);
		byte_stream_put16(&bs, 0x0000);

		/* TLV t(2711) */
		byte_stream_put16(&bs, 0x2711);
		byte_stream_put16(&bs, 0x0036);
		{ /* V */
			byte_stream_putle16(&bs, 0x001b); /* L */
			byte_stream_putle16(&bs, 0x0009); /* Protocol version */
			byte_stream_putcaps(&bs, OSCAR_CAPABILITY_EMPTY);
			byte_stream_putle16(&bs, 0x0000); /* Unknown */
			byte_stream_putle16(&bs, 0x0001); /* Client features? */
			byte_stream_putle16(&bs, 0x0000); /* Unknown */
			byte_stream_putle8(&bs, 0x00); /* Unkizown */
			byte_stream_putle16(&bs, 0xffff); /* Sequence number?  XXX - This should decrement by 1 with each request */

			byte_stream_putle16(&bs, 0x000e); /* L */
			byte_stream_putle16(&bs, 0xffff); /* Sequence number?  XXX - This should decrement by 1 with each request */
			byte_stream_putle32(&bs, 0x00000000); /* Unknown */
			byte_stream_putle32(&bs, 0x00000000); /* Unknown */
			byte_stream_putle32(&bs, 0x00000000); /* Unknown */

			/* The type of status message being requested */
			if (type & AIM_ICQ_STATE_CHAT)
				byte_stream_putle16(&bs, 0x03ec);
			else if(type & AIM_ICQ_STATE_DND)
				byte_stream_putle16(&bs, 0x03eb);
			else if(type & AIM_ICQ_STATE_OUT)
				byte_stream_putle16(&bs, 0x03ea);
			else if(type & AIM_ICQ_STATE_BUSY)
				byte_stream_putle16(&bs, 0x03e9);
			else if(type & AIM_ICQ_STATE_AWAY)
				byte_stream_putle16(&bs, 0x03e8);

			byte_stream_putle16(&bs, 0x0001); /* Status? */
			byte_stream_putle16(&bs, 0x0001); /* Priority of this message? */
			byte_stream_putle16(&bs, 0x0001); /* L */
			byte_stream_putle8(&bs, 0x00); /* String of length L */
		} /* End TLV t(2711) */
	} /* End TLV t(0005) */

	/* TLV t(0003) */
	byte_stream_put16(&bs, 0x0003);
	byte_stream_put16(&bs, 0x0000);

	flap_connection_send_snac(od, conn, SNAC_FAMILY_ICBM, 0x0006, 0x0000, snacid, &bs);

	byte_stream_destroy(&bs);

	return 0;
}

/**
 * Subtype 0x0006 - Send an ICQ-esque ICBM.
 *
 * This can be used to send an ICQ authorization reply (deny or grant).  It is the "old way."
 * The new way is to use SSI.  I like the new way a lot better.  This seems like such a hack,
 * mostly because it's in network byte order.  Figuring this stuff out sometimes takes a while,
 * but thats ok, because it gives me time to try to figure out what kind of drugs the AOL people
 * were taking when they merged the two protocols.
 *
 * @param sn The destination screen name.
 * @param type The type of message.  0x0007 for authorization denied.  0x0008 for authorization granted.
 * @param message The message you want to send, it should be null terminated.
 * @return Return 0 if no errors, otherwise return the error number.
 */
int aim_im_sendch4(OscarData *od, const char *sn, guint16 type, const char *message)
{
	FlapConnection *conn;
	ByteStream bs;
	aim_snacid_t snacid;
	guchar cookie[8];

	if (!od || !(conn = flap_connection_findbygroup(od, 0x0002)))
		return -EINVAL;

	if (!sn || !type || !message)
		return -EINVAL;

	byte_stream_new(&bs, 8+3+strlen(sn)+12+strlen(message)+1+4);

	snacid = aim_cachesnac(od, SNAC_FAMILY_ICBM, 0x0006, 0x0000, NULL, 0);

	aim_icbm_makecookie(cookie);

	/* ICBM header */
	aim_im_puticbm(&bs, cookie, 0x0004, sn);

	/*
	 * TLV t(0005)
	 *
	 * ICQ data (the UIN and the message).
	 */
	byte_stream_put16(&bs, 0x0005);
	byte_stream_put16(&bs, 4 + 2+2+strlen(message)+1);

	/*
	 * Your UIN
	 */
	byte_stream_putle32(&bs, atoi(od->sn));

	/*
	 * TLV t(type) l(strlen(message)+1) v(message+NULL)
	 */
	byte_stream_putle16(&bs, type);
	byte_stream_putle16(&bs, strlen(message)+1);
	byte_stream_putraw(&bs, (const guint8 *)message, strlen(message)+1);

	/*
	 * TLV t(0006) l(0000) v()
	 */
	byte_stream_put16(&bs, 0x0006);
	byte_stream_put16(&bs, 0x0000);

	flap_connection_send_snac(od, conn, SNAC_FAMILY_ICBM, 0x0006, 0x0000, snacid, &bs);

	byte_stream_destroy(&bs);

	return 0;
}

/*
 * XXX - I don't see when this would ever get called...
 */
static int outgoingim(OscarData *od, FlapConnection *conn, aim_module_t *mod, FlapFrame *frame, aim_modsnac_t *snac, ByteStream *bs)
{
	int ret = 0;
	aim_rxcallback_t userfunc;
	guchar cookie[8];
	guint16 channel;
	GSList *tlvlist;
	char *sn;
	int snlen;
	guint16 icbmflags = 0;
	guint8 flag1 = 0, flag2 = 0;
	gchar *msg = NULL;
	aim_tlv_t *msgblock;

	/* ICBM Cookie. */
	aim_icbm_makecookie(cookie);

	/* Channel ID */
	channel = byte_stream_get16(bs);

	if (channel != 0x01) {
		purple_debug_misc("oscar", "icbm: ICBM recieved on unsupported channel.  Ignoring. (chan = %04x)\n", channel);
		return 0;
	}

	snlen = byte_stream_get8(bs);
	sn = byte_stream_getstr(bs, snlen);

	tlvlist = aim_tlvlist_read(bs);

	if (aim_tlv_gettlv(tlvlist, 0x0003, 1))
		icbmflags |= AIM_IMFLAGS_ACK;
	if (aim_tlv_gettlv(tlvlist, 0x0004, 1))
		icbmflags |= AIM_IMFLAGS_AWAY;

	if ((msgblock = aim_tlv_gettlv(tlvlist, 0x0002, 1))) {
		ByteStream mbs;
		int featurelen, msglen;

		byte_stream_init(&mbs, msgblock->value, msgblock->length);

		byte_stream_get8(&mbs);
		byte_stream_get8(&mbs);
		for (featurelen = byte_stream_get16(&mbs); featurelen; featurelen--)
			byte_stream_get8(&mbs);
		byte_stream_get8(&mbs);
		byte_stream_get8(&mbs);

		msglen = byte_stream_get16(&mbs) - 4; /* final block length */

		flag1 = byte_stream_get16(&mbs);
		flag2 = byte_stream_get16(&mbs);

		msg = byte_stream_getstr(&mbs, msglen);
	}

	if ((userfunc = aim_callhandler(od, snac->family, snac->subtype)))
		ret = userfunc(od, conn, frame, channel, sn, msg, icbmflags, flag1, flag2);

	g_free(sn);
	g_free(msg);
	aim_tlvlist_free(tlvlist);

	return ret;
}

/*
 * Ahh, the joys of nearly ridiculous over-engineering.
 *
 * Not only do AIM ICBM's support multiple channels.  Not only do they
 * support multiple character sets.  But they support multiple character
 * sets / encodings within the same ICBM.
 *
 * These multipart messages allow for complex space savings techniques, which
 * seem utterly unnecessary by today's standards.  In fact, there is only
 * one client still in popular use that still uses this method: AOL for the
 * Macintosh, Version 5.0.  Obscure, yes, I know.
 *
 * In modern (non-"legacy") clients, if the user tries to send a character
 * that is not ISO-8859-1 or ASCII, the client will send the entire message
 * as UNICODE, meaning that every character in the message will occupy the
 * full 16 bit UNICODE field, even if the high order byte would be zero.
 * Multipart messages prevent this wasted space by allowing the client to
 * only send the characters in UNICODE that need to be sent that way, and
 * the rest of the message can be sent in whatever the native character
 * set is (probably ASCII).
 *
 * An important note is that sections will be displayed in the order that
 * they appear in the ICBM.  There is no facility for merging or rearranging
 * sections at run time.  So if you have, say, ASCII then UNICODE then ASCII,
 * you must supply two ASCII sections with a UNICODE in the middle, and incur
 * the associated overhead.
 *
 * Normally I would have laughed and given a firm 'no' to supporting this
 * seldom-used feature, but something is attracting me to it.  In the future,
 * it may be possible to abuse this to send mixed-media messages to other
 * open source clients (like encryption or something) -- see faimtest for
 * examples of how to do this.
 *
 * I would definitely recommend avoiding this feature unless you really
 * know what you are doing, and/or you have something neat to do with it.
 *
 */
int aim_mpmsg_init(OscarData *od, aim_mpmsg_t *mpm)
{

	memset(mpm, 0, sizeof(aim_mpmsg_t));

	return 0;
}

static int mpmsg_addsection(OscarData *od, aim_mpmsg_t *mpm, guint16 charset, guint16 charsubset, gchar *data, guint16 datalen)
{
	aim_mpmsg_section_t *sec;

	sec = g_malloc(sizeof(aim_mpmsg_section_t));

	sec->charset = charset;
	sec->charsubset = charsubset;
	sec->data = data;
	sec->datalen = datalen;
	sec->next = NULL;

	if (!mpm->parts)
		mpm->parts = sec;
	else {
		aim_mpmsg_section_t *cur;

		for (cur = mpm->parts; cur->next; cur = cur->next)
			;
		cur->next = sec;
	}

	mpm->numparts++;

	return 0;
}

int aim_mpmsg_addraw(OscarData *od, aim_mpmsg_t *mpm, guint16 charset, guint16 charsubset, const gchar *data, guint16 datalen)
{
	gchar *dup;

	dup = g_malloc(datalen);
	memcpy(dup, data, datalen);

	if (mpmsg_addsection(od, mpm, charset, charsubset, dup, datalen) == -1) {
		g_free(dup);
		return -1;
	}

	return 0;
}

/* XXX - should provide a way of saying ISO-8859-1 specifically */
int aim_mpmsg_addascii(OscarData *od, aim_mpmsg_t *mpm, const char *ascii)
{
	gchar *dup;

	if (!(dup = g_strdup(ascii)))
		return -1;

	if (mpmsg_addsection(od, mpm, 0x0000, 0x0000, dup, strlen(ascii)) == -1) {
		g_free(dup);
		return -1;
	}

	return 0;
}

int aim_mpmsg_addunicode(OscarData *od, aim_mpmsg_t *mpm, const guint16 *unicode, guint16 unicodelen)
{
	gchar *buf;
	ByteStream bs;
	int i;

	buf = g_malloc(unicodelen * 2);

	byte_stream_init(&bs, (guchar *)buf, unicodelen * 2);

	/* We assume unicode is in /host/ byte order -- convert to network */
	for (i = 0; i < unicodelen; i++)
		byte_stream_put16(&bs, unicode[i]);

	if (mpmsg_addsection(od, mpm, 0x0002, 0x0000, buf, byte_stream_curpos(&bs)) == -1) {
		g_free(buf);
		return -1;
	}

	return 0;
}

void aim_mpmsg_free(OscarData *od, aim_mpmsg_t *mpm)
{
	aim_mpmsg_section_t *cur;

	for (cur = mpm->parts; cur; ) {
		aim_mpmsg_section_t *tmp;

		tmp = cur->next;
		g_free(cur->data);
		g_free(cur);
		cur = tmp;
	}

	mpm->numparts = 0;
	mpm->parts = NULL;

	return;
}

/*
 * Start by building the multipart structures, then pick the first
 * human-readable section and stuff it into args->msg so no one gets
 * suspicious.
 */
static int incomingim_ch1_parsemsgs(OscarData *od, aim_userinfo_t *userinfo, guint8 *data, int len, struct aim_incomingim_ch1_args *args)
{
	/* Should this be ASCII -> UNICODE -> Custom */
	static const guint16 charsetpri[] = {
		AIM_CHARSET_ASCII, /* ASCII first */
		AIM_CHARSET_CUSTOM, /* then ISO-8859-1 */
		AIM_CHARSET_UNICODE, /* UNICODE as last resort */
	};
	static const int charsetpricount = 3;
	int i;
	ByteStream mbs;
	aim_mpmsg_section_t *sec;

	byte_stream_init(&mbs, data, len);

	while (byte_stream_empty(&mbs)) {
		guint16 msglen, flag1, flag2;
		gchar *msgbuf;

		byte_stream_get8(&mbs); /* 01 */
		byte_stream_get8(&mbs); /* 01 */

		/* Message string length, including character set info. */
		msglen = byte_stream_get16(&mbs);
		if (msglen > byte_stream_empty(&mbs))
		{
			purple_debug_misc("oscar", "Received an IM containing an invalid message part from %s.  They are probably trying to do something malicious.\n", userinfo->sn);
			break;
		}

		/* Character set info */
		flag1 = byte_stream_get16(&mbs);
		flag2 = byte_stream_get16(&mbs);

		/* Message. */
		msglen -= 4;

		/*
		 * For now, we don't care what the encoding is.  Just copy
		 * it into a multipart struct and deal with it later. However,
		 * always pad the ending with a NULL.  This makes it easier
		 * to treat ASCII sections as strings.  It won't matter for
		 * UNICODE or binary data, as you should never read past
		 * the specified data length, which will not include the pad.
		 *
		 * XXX - There's an API bug here.  For sending, the UNICODE is
		 * given in host byte order (aim_mpmsg_addunicode), but here
		 * the received messages are given in network byte order.
		 *
		 */
		msgbuf = (gchar *)byte_stream_getraw(&mbs, msglen);
		mpmsg_addsection(od, &args->mpmsg, flag1, flag2, msgbuf, msglen);

	} /* while */

	args->icbmflags |= AIM_IMFLAGS_MULTIPART; /* always set */

	/*
	 * Clients that support multiparts should never use args->msg, as it
	 * will point to an arbitrary section.
	 *
	 * Here, we attempt to provide clients that do not support multipart
	 * messages with something to look at -- hopefully a human-readable
	 * string.  But, failing that, a UNICODE message, or nothing at all.
	 *
	 * Which means that even if args->msg is NULL, it does not mean the
	 * message was blank.
	 *
	 */
	for (i = 0; i < charsetpricount; i++) {
		for (sec = args->mpmsg.parts; sec; sec = sec->next) {

			if (sec->charset != charsetpri[i])
				continue;

			/* Great. We found one.  Fill it in. */
			args->charset = sec->charset;
			args->charsubset = sec->charsubset;

			/* Set up the simple flags */
			switch (args->charsubset)
			{
				case 0x0000:
					/* standard subencoding? */
					break;
				case 0x000b:
					args->icbmflags |= AIM_IMFLAGS_SUBENC_MACINTOSH;
					break;
				case 0xffff:
					/* no subencoding */
					break;
				default:
					break;
			}

			args->msg = sec->data;
			args->msglen = sec->datalen;

			return 0;
		}
	}

	/* No human-readable sections found.  Oh well. */
	args->charset = args->charsubset = 0xffff;
	args->msg = NULL;
	args->msglen = 0;

	return 0;
}

static int incomingim_ch1(OscarData *od, FlapConnection *conn, aim_module_t *mod, FlapFrame *frame, aim_modsnac_t *snac, guint16 channel, aim_userinfo_t *userinfo, ByteStream *bs, guint8 *cookie)
{
	guint16 type, length;
	aim_rxcallback_t userfunc;
	int ret = 0;
	struct aim_incomingim_ch1_args args;
	unsigned int endpos;

	memset(&args, 0, sizeof(args));

	aim_mpmsg_init(od, &args.mpmsg);

	/*
	 * This used to be done using tlvchains.  For performance reasons,
	 * I've changed it to process the TLVs in-place.  This avoids lots
	 * of per-IM memory allocations.
	 */
	while (byte_stream_empty(bs) >= 4)
	{
		type = byte_stream_get16(bs);
		length = byte_stream_get16(bs);

		if (length > byte_stream_empty(bs))
		{
			purple_debug_misc("oscar", "Received an IM containing an invalid message part from %s.  They are probably trying to do something malicious.\n", userinfo->sn);
			break;
		}

		endpos = byte_stream_curpos(bs) + length;

		if (type == 0x0002) { /* Message Block */

			/*
			 * This TLV consists of the following:
			 *   - 0501 -- Unknown
			 *   - Features: Don't know how to interpret these
			 *   - 0101 -- Unknown
			 *   - Message
			 *
			 */

			byte_stream_get8(bs); /* 05 */
			byte_stream_get8(bs); /* 01 */

			args.featureslen = byte_stream_get16(bs);
			if (args.featureslen > byte_stream_empty(bs))
			{
				purple_debug_misc("oscar", "Received an IM containing an invalid message part from %s.  They are probably trying to do something malicious.\n", userinfo->sn);
				break;
			}
			if (args.featureslen == 0)
			{
				args.features = NULL;
			}
			else
			{
				args.features = byte_stream_getraw(bs, args.featureslen);
				args.icbmflags |= AIM_IMFLAGS_CUSTOMFEATURES;
			}

			/*
			 * The rest of the TLV contains one or more message
			 * blocks...
			 */
			incomingim_ch1_parsemsgs(od, userinfo, bs->data + bs->offset /* XXX evil!!! */, length - 2 - 2 - args.featureslen, &args);

		} else if (type == 0x0003) { /* Server Ack Requested */

			args.icbmflags |= AIM_IMFLAGS_ACK;

		} else if (type == 0x0004) { /* Message is Auto Response */

			args.icbmflags |= AIM_IMFLAGS_AWAY;

		} else if (type == 0x0006) { /* Message was received offline. */

			/*
			 * This flag is set on incoming offline messages for both
			 * AIM and ICQ accounts.
			 */
			args.icbmflags |= AIM_IMFLAGS_OFFLINE;

		} else if (type == 0x0008) { /* I-HAVE-A-REALLY-PURTY-ICON Flag */

			args.iconlen = byte_stream_get32(bs);
			byte_stream_get16(bs); /* 0x0001 */
			args.iconsum = byte_stream_get16(bs);
			args.iconstamp = byte_stream_get32(bs);

			/*
			 * This looks to be a client bug.  MacAIM 4.3 will
			 * send this tag, but with all zero values, in the
			 * first message of a conversation. This makes no
			 * sense whatsoever, so I'm going to say its a bug.
			 *
			 * You really shouldn't advertise a zero-length icon
			 * anyway.
			 *
			 */
			if (args.iconlen)
				args.icbmflags |= AIM_IMFLAGS_HASICON;

		} else if (type == 0x0009) {

			args.icbmflags |= AIM_IMFLAGS_BUDDYREQ;

		} else if (type == 0x000b) { /* Non-direct connect typing notification */

			args.icbmflags |= AIM_IMFLAGS_TYPINGNOT;

		} else if (type == 0x0016) {

			/*
			 * UTC timestamp for when the message was sent.  Only
			 * provided for offline messages.
			 */
			args.timestamp = byte_stream_get32(bs);

		} else if (type == 0x0017) {

			if (length > byte_stream_empty(bs))
			{
				purple_debug_misc("oscar", "Received an IM containing an invalid message part from %s.  They are probably trying to do something malicious.\n", userinfo->sn);
				break;
			}
			g_free(args.extdata);
			args.extdatalen = length;
			if (args.extdatalen == 0)
				args.extdata = NULL;
			else
				args.extdata = byte_stream_getraw(bs, args.extdatalen);

		} else {
			purple_debug_misc("oscar", "incomingim_ch1: unknown TLV 0x%04x (len %d)\n", type, length);
		}

		/*
		 * This is here to protect ourselves from ourselves.  That
		 * is, if something above doesn't completely parse its value
		 * section, or, worse, overparses it, this will set the
		 * stream where it needs to be in order to land on the next
		 * TLV when the loop continues.
		 *
		 */
		byte_stream_setpos(bs, endpos);
	}


	if ((userfunc = aim_callhandler(od, snac->family, snac->subtype)))
		ret = userfunc(od, conn, frame, channel, userinfo, &args);

	aim_mpmsg_free(od, &args.mpmsg);
	g_free(args.features);
	g_free(args.extdata);

	return ret;
}

static void
incomingim_ch2_buddylist(OscarData *od, FlapConnection *conn, aim_module_t *mod, FlapFrame *frame, aim_modsnac_t *snac, aim_userinfo_t *userinfo, IcbmArgsCh2 *args, ByteStream *servdata)
{
	/*
	 * This goes like this...
	 *
	 *   group name length
	 *   group name
	 *     num of buddies in group
	 *     buddy name length
	 *     buddy name
	 *     buddy name length
	 *     buddy name
	 *     ...
	 *   group name length
	 *   group name
	 *     num of buddies in group
	 *     buddy name length
	 *     buddy name
	 *     ...
	 *   ...
	 */
	while (byte_stream_empty(servdata))
	{
		guint16 gnlen, numb;
		int i;
		char *gn;

		gnlen = byte_stream_get16(servdata);
		gn = byte_stream_getstr(servdata, gnlen);
		numb = byte_stream_get16(servdata);

		for (i = 0; i < numb; i++) {
			guint16 bnlen;
			char *bn;

			bnlen = byte_stream_get16(servdata);
			bn = byte_stream_getstr(servdata, bnlen);

			purple_debug_misc("oscar", "got a buddy list from %s: group %s, buddy %s\n", userinfo->sn, gn, bn);

			g_free(bn);
		}

		g_free(gn);
	}

	return;
}

static void
incomingim_ch2_buddyicon_free(OscarData *od, IcbmArgsCh2 *args)
{
	g_free(args->info.icon.icon);

	return;
}

static void
incomingim_ch2_buddyicon(OscarData *od, FlapConnection *conn, aim_module_t *mod, FlapFrame *frame, aim_modsnac_t *snac, aim_userinfo_t *userinfo, IcbmArgsCh2 *args, ByteStream *servdata)
{
	args->info.icon.checksum = byte_stream_get32(servdata);
	args->info.icon.length = byte_stream_get32(servdata);
	args->info.icon.timestamp = byte_stream_get32(servdata);
	args->info.icon.icon = byte_stream_getraw(servdata, args->info.icon.length);

	args->destructor = (void *)incomingim_ch2_buddyicon_free;

	return;
}

static void
incomingim_ch2_chat_free(OscarData *od, IcbmArgsCh2 *args)
{
	/* XXX - aim_chat_roominfo_free() */
	g_free(args->info.chat.roominfo.name);

	return;
}

static void
incomingim_ch2_chat(OscarData *od, FlapConnection *conn, aim_module_t *mod, FlapFrame *frame, aim_modsnac_t *snac, aim_userinfo_t *userinfo, IcbmArgsCh2 *args, ByteStream *servdata)
{
	/*
	 * Chat room info.
	 */
	aim_chat_readroominfo(servdata, &args->info.chat.roominfo);

	args->destructor = (void *)incomingim_ch2_chat_free;
}

static void
incomingim_ch2_icqserverrelay_free(OscarData *od, IcbmArgsCh2 *args)
{
	g_free((char *)args->info.rtfmsg.rtfmsg);
}

/*
 * The relationship between OSCAR_CAPABILITY_ICQSERVERRELAY and OSCAR_CAPABILITY_ICQRTF is
 * kind of odd. This sends the client ICQRTF since that is all that I've seen
 * SERVERRELAY used for.
 *
 * Note that this is all little-endian.  Cringe.
 *
 */
static void
incomingim_ch2_icqserverrelay(OscarData *od, FlapConnection *conn, aim_module_t *mod, FlapFrame *frame, aim_modsnac_t *snac, aim_userinfo_t *userinfo, IcbmArgsCh2 *args, ByteStream *servdata)
{
	guint16 hdrlen, anslen, msglen;

	if (byte_stream_empty(servdata) < 24)
		/* Someone sent us a short server relay ICBM.  Weird.  (Maybe?) */
		return;

	hdrlen = byte_stream_getle16(servdata);
	byte_stream_advance(servdata, hdrlen);

	hdrlen = byte_stream_getle16(servdata);
	byte_stream_advance(servdata, hdrlen);

	args->info.rtfmsg.msgtype = byte_stream_getle16(servdata);

	anslen = byte_stream_getle32(servdata);
	byte_stream_advance(servdata, anslen);

	msglen = byte_stream_getle16(servdata);
	args->info.rtfmsg.rtfmsg = byte_stream_getstr(servdata, msglen);

	args->info.rtfmsg.fgcolor = byte_stream_getle32(servdata);
	args->info.rtfmsg.bgcolor = byte_stream_getle32(servdata);

	hdrlen = byte_stream_getle32(servdata);
	byte_stream_advance(servdata, hdrlen);

	args->destructor = (void *)incomingim_ch2_icqserverrelay_free;
}

static void
incomingim_ch2_sendfile_free(OscarData *od, IcbmArgsCh2 *args)
{
	g_free(args->info.sendfile.filename);
}

/* Someone is sending us a file */
static void
incomingim_ch2_sendfile(OscarData *od, FlapConnection *conn, aim_module_t *mod, FlapFrame *frame, aim_modsnac_t *snac, aim_userinfo_t *userinfo, IcbmArgsCh2 *args, ByteStream *servdata)
{
	int flen;

	args->destructor = (void *)incomingim_ch2_sendfile_free;

	/* Maybe there is a better way to tell what kind of sendfile
	 * this is?  Maybe TLV t(000a)? */

	/* subtype is one of AIM_OFT_SUBTYPE_* */
	args->info.sendfile.subtype = byte_stream_get16(servdata);
	args->info.sendfile.totfiles = byte_stream_get16(servdata);
	args->info.sendfile.totsize = byte_stream_get32(servdata);

	/*
	 * I hope to God I'm right when I guess that there is a
	 * 32 char max filename length for single files.  I think
	 * OFT tends to do that.  Gotta love inconsistency.  I saw
	 * a 26 byte filename?
	 */
	/* AAA - create an byte_stream_getnullstr function (don't anymore)(maybe) */
	/* Use an inelegant way of getting the null-terminated filename,
	 * since there's no easy bstream routine. */
	for (flen = 0; byte_stream_get8(servdata); flen++);
	byte_stream_advance(servdata, -flen -1);
	args->info.sendfile.filename = byte_stream_getstr(servdata, flen);

	/* There is sometimes more after the null-terminated filename,
	 * but I'm unsure of its format. */
	/* I don't believe him. */
	/* There is sometimes a null byte inside a unicode filename,
	 * but as far as I can tell the filename is the last
	 * piece of data that will be in this message. --Jonathan */
}

typedef void (*ch2_args_destructor_t)(OscarData *od, IcbmArgsCh2 *args);

static int incomingim_ch2(OscarData *od, FlapConnection *conn, aim_module_t *mod, FlapFrame *frame, aim_modsnac_t *snac, guint16 channel, aim_userinfo_t *userinfo, GSList *tlvlist, guint8 *cookie)
{
	aim_rxcallback_t userfunc;
	aim_tlv_t *block1, *servdatatlv;
	GSList *list2;
	aim_tlv_t *tlv;
	IcbmArgsCh2 args;
	ByteStream bbs, sdbs, *sdbsptr = NULL;
	guint8 *cookie2;
	int ret = 0;

	char proxyip[30] = {""};
	char clientip[30] = {""};
	char verifiedip[30] = {""};

	memset(&args, 0, sizeof(args));

	/*
	 * There's another block of TLVs embedded in the type 5 here.
	 */
	block1 = aim_tlv_gettlv(tlvlist, 0x0005, 1);
	if (block1 == NULL)
	{
		/* The server sent us ch2 ICBM without ch2 info?  Weird. */
		return 1;
	}
	byte_stream_init(&bbs, block1->value, block1->length);

	/*
	 * First two bytes represent the status of the connection.
	 * One of the AIM_RENDEZVOUS_ defines.
	 *
	 * 0 is a request, 1 is a cancel, 2 is an accept
	 */
	args.status = byte_stream_get16(&bbs);

	/*
	 * Next comes the cookie.  Should match the ICBM cookie.
	 */
	cookie2 = byte_stream_getraw(&bbs, 8);
	if (memcmp(cookie, cookie2, 8) != 0)
	{
		purple_debug_warning("oscar",
				"Cookies don't match in rendezvous ICBM, bailing out.\n");
		g_free(cookie2);
		return 1;
	}
	memcpy(args.cookie, cookie2, 8);
	g_free(cookie2);

	/*
	 * The next 16bytes are a capability block so we can
	 * identify what type of rendezvous this is.
	 */
	args.type = aim_locate_getcaps(od, &bbs, 0x10);

	/*
	 * What follows may be TLVs or nothing, depending on the
	 * purpose of the message.
	 *
	 * Ack packets for instance have nothing more to them.
	 */
	list2 = aim_tlvlist_read(&bbs);

	/*
	 * IP address to proxy the file transfer through.
	 *
	 * TODO: I don't like this.  Maybe just read in an int?  Or inet_ntoa...
	 */
	tlv = aim_tlv_gettlv(list2, 0x0002, 1);
	if ((tlv != NULL) && (tlv->length == 4))
		snprintf(proxyip, sizeof(proxyip), "%hhu.%hhu.%hhu.%hhu",
				tlv->value[0], tlv->value[1],
				tlv->value[2], tlv->value[3]);

	/*
	 * IP address from the perspective of the client.
	 */
	tlv = aim_tlv_gettlv(list2, 0x0003, 1);
	if ((tlv != NULL) && (tlv->length == 4))
		snprintf(clientip, sizeof(clientip), "%hhu.%hhu.%hhu.%hhu",
				tlv->value[0], tlv->value[1],
				tlv->value[2], tlv->value[3]);

	/*
	 * Verified IP address (from the perspective of Oscar).
	 *
	 * This is added by the server.
	 */
	tlv = aim_tlv_gettlv(list2, 0x0004, 1);
	if ((tlv != NULL) && (tlv->length == 4))
		snprintf(verifiedip, sizeof(verifiedip), "%hhu.%hhu.%hhu.%hhu",
				tlv->value[0], tlv->value[1],
				tlv->value[2], tlv->value[3]);

	/*
	 * Port number for something.
	 */
	if (aim_tlv_gettlv(list2, 0x0005, 1))
		args.port = aim_tlv_get16(list2, 0x0005, 1);

	/*
	 * File transfer "request number":
	 * 0x0001 - Initial file transfer request for no proxy or stage 1 proxy
	 * 0x0002 - "Reply request" for a stage 2 proxy (receiver wants to use proxy)
	 * 0x0003 - A third request has been sent; applies only to stage 3 proxied transfers
	 */
	if (aim_tlv_gettlv(list2, 0x000a, 1))
		args.requestnumber = aim_tlv_get16(list2, 0x000a, 1);

	/*
	 * Terminate connection/error code.  0x0001 means the other user
	 * canceled the connection.
	 */
	if (aim_tlv_gettlv(list2, 0x000b, 1))
		args.errorcode = aim_tlv_get16(list2, 0x000b, 1);

	/*
	 * Invitation message / chat description.
	 */
	if (aim_tlv_gettlv(list2, 0x000c, 1)) {
		args.msg = aim_tlv_getstr(list2, 0x000c, 1);
		args.msglen = aim_tlv_getlength(list2, 0x000c, 1);
	}

	/*
	 * Character set.
	 */
	if (aim_tlv_gettlv(list2, 0x000d, 1))
		args.encoding = aim_tlv_getstr(list2, 0x000d, 1);

	/*
	 * Language.
	 */
	if (aim_tlv_gettlv(list2, 0x000e, 1))
		args.language = aim_tlv_getstr(list2, 0x000e, 1);

#if 0
	/*
	 * Unknown -- no value
	 *
	 * Maybe means we should connect directly to transfer the file?
	 * Also used in ICQ Lite Beta 4.0 URLs.  Also empty.
	 */
	 /* I don't think this indicates a direct transfer; this flag is
	  * also present in a stage 1 proxied file send request -- Jonathan */
	if (aim_tlv_gettlv(list2, 0x000f, 1)) {
		/* Unhandled */
	}
#endif

	/*
	 * Flag meaning we should proxy the file transfer through an AIM server
	 */
	if (aim_tlv_gettlv(list2, 0x0010, 1))
		args.use_proxy = TRUE;

	if (strlen(proxyip))
		args.proxyip = (char *)proxyip;
	if (strlen(clientip))
		args.clientip = (char *)clientip;
	if (strlen(verifiedip))
		args.verifiedip = (char *)verifiedip;

	/*
	 * This must be present in PROPOSALs, but will probably not
	 * exist in CANCELs and ACCEPTs.  Also exists in ICQ Lite
	 * Beta 4.0 URLs (OSCAR_CAPABILITY_ICQSERVERRELAY).
	 *
	 * Service Data blocks are module-specific in format.
	 */
	if ((servdatatlv = aim_tlv_gettlv(list2, 0x2711 /* 10001 */, 1))) {

		byte_stream_init(&sdbs, servdatatlv->value, servdatatlv->length);
		sdbsptr = &sdbs;

		/*
		 * The rest of the handling depends on what type it is.
		 *
		 * Not all of them have special handling (yet).
		 */
		if (args.type & OSCAR_CAPABILITY_BUDDYICON)
			incomingim_ch2_buddyicon(od, conn, mod, frame, snac, userinfo, &args, sdbsptr);
		else if (args.type & OSCAR_CAPABILITY_SENDBUDDYLIST)
			incomingim_ch2_buddylist(od, conn, mod, frame, snac, userinfo, &args, sdbsptr);
		else if (args.type & OSCAR_CAPABILITY_CHAT)
			incomingim_ch2_chat(od, conn, mod, frame, snac, userinfo, &args, sdbsptr);
		else if (args.type & OSCAR_CAPABILITY_ICQSERVERRELAY)
			incomingim_ch2_icqserverrelay(od, conn, mod, frame, snac, userinfo, &args, sdbsptr);
		else if (args.type & OSCAR_CAPABILITY_SENDFILE)
			incomingim_ch2_sendfile(od, conn, mod, frame, snac, userinfo, &args, sdbsptr);
	}

	if ((userfunc = aim_callhandler(od, snac->family, snac->subtype)))
		ret = userfunc(od, conn, frame, channel, userinfo, &args);


	if (args.destructor)
		((ch2_args_destructor_t)args.destructor)(od, &args);

	g_free((char *)args.msg);
	g_free((char *)args.encoding);
	g_free((char *)args.language);

	aim_tlvlist_free(list2);

	return ret;
}

static int incomingim_ch4(OscarData *od, FlapConnection *conn, aim_module_t *mod, FlapFrame *frame, aim_modsnac_t *snac, guint16 channel, aim_userinfo_t *userinfo, GSList *tlvlist, guint8 *cookie)
{
	ByteStream meat;
	aim_rxcallback_t userfunc;
	aim_tlv_t *block;
	struct aim_incomingim_ch4_args args;
	int ret = 0;

	/*
	 * Make a bstream for the meaty part.  Yum.  Meat.
	 */
	if (!(block = aim_tlv_gettlv(tlvlist, 0x0005, 1)))
		return -1;
	byte_stream_init(&meat, block->value, block->length);

	args.uin = byte_stream_getle32(&meat);
	args.type = byte_stream_getle8(&meat);
	args.flags = byte_stream_getle8(&meat);
	if (args.type == 0x1a)
		/* There seems to be a problem with the length in SMS msgs from server, this fixed it */
		args.msglen = block->length - 6;
	else
		args.msglen = byte_stream_getle16(&meat);
	args.msg = (gchar *)byte_stream_getraw(&meat, args.msglen);

	if ((userfunc = aim_callhandler(od, snac->family, snac->subtype)))
		ret = userfunc(od, conn, frame, channel, userinfo, &args);

	g_free(args.msg);

	return ret;
}

/*
 * Subtype 0x0007
 *
 * It can easily be said that parsing ICBMs is THE single
 * most difficult thing to do in the in AIM protocol.  In
 * fact, I think I just did say that.
 *
 * Below is the best damned solution I've come up with
 * over the past sixteen months of battling with it. This
 * can parse both away and normal messages from every client
 * I have access to.  Its not fast, its not clean.  But it works.
 *
 */
static int incomingim(OscarData *od, FlapConnection *conn, aim_module_t *mod, FlapFrame *frame, aim_modsnac_t *snac, ByteStream *bs)
{
	int ret = 0;
	guchar *cookie;
	guint16 channel;
	aim_userinfo_t userinfo;

	memset(&userinfo, 0x00, sizeof(aim_userinfo_t));

	/*
	 * Read ICBM Cookie.
	 */
	cookie = byte_stream_getraw(bs, 8);

	/*
	 * Channel ID.
	 *
	 * Channel 0x0001 is the message channel.  It is
	 * used to send basic ICBMs.
	 *
	 * Channel 0x0002 is the Rendezvous channel, which
	 * is where Chat Invitiations and various client-client
	 * connection negotiations come from.
	 *
	 * Channel 0x0003 is used for chat messages.
	 *
	 * Channel 0x0004 is used for ICQ authorization, or
	 * possibly any system notice.
	 *
	 */
	channel = byte_stream_get16(bs);

	/*
	 * Extract the standard user info block.
	 *
	 * Note that although this contains TLVs that appear contiguous
	 * with the TLVs read below, they are two different pieces.  The
	 * userinfo block contains the number of TLVs that contain user
	 * information, the rest are not even though there is no separation.
	 * You can start reading the message TLVs after aim_info_extract()
	 * parses out the standard userinfo block.
	 *
	 * That also means that TLV types can be duplicated between the
	 * userinfo block and the rest of the message, however there should
	 * never be two TLVs of the same type in one block.
	 *
	 */
	aim_info_extract(od, bs, &userinfo);

	/*
	 * From here on, its depends on what channel we're on.
	 *
	 * Technically all channels have a TLV list have this, however,
	 * for the common channel 1 case, in-place parsing is used for
	 * performance reasons (less memory allocation).
	 */
	if (channel == 1) {

		ret = incomingim_ch1(od, conn, mod, frame, snac, channel, &userinfo, bs, cookie);

	} else if (channel == 2) {
		GSList *tlvlist;

		/*
		 * Read block of TLVs (not including the userinfo data).  All
		 * further data is derived from what is parsed here.
		 */
		tlvlist = aim_tlvlist_read(bs);

		ret = incomingim_ch2(od, conn, mod, frame, snac, channel, &userinfo, tlvlist, cookie);

		aim_tlvlist_free(tlvlist);

	} else if (channel == 4) {
		GSList *tlvlist;

		tlvlist = aim_tlvlist_read(bs);
		ret = incomingim_ch4(od, conn, mod, frame, snac, channel, &userinfo, tlvlist, cookie);
		aim_tlvlist_free(tlvlist);

	} else {
		purple_debug_misc("oscar", "icbm: ICBM received on an unsupported channel.  Ignoring.  (chan = %04x)\n", channel);
	}

	aim_info_free(&userinfo);
	g_free(cookie);

	return ret;
}

/*
 * Subtype 0x0008 - Send a warning to sn.
 *
 * Flags:
 *  AIM_WARN_ANON  Send as an anonymous (doesn't count as much)
 *
 * returns -1 on error (couldn't alloc packet), 0 on success.
 *
 */
int aim_im_warn(OscarData *od, FlapConnection *conn, const char *sn, guint32 flags)
{
	ByteStream bs;
	aim_snacid_t snacid;

	if (!od || !conn || !sn)
		return -EINVAL;

	byte_stream_new(&bs, strlen(sn)+3);

	snacid = aim_cachesnac(od, SNAC_FAMILY_ICBM, 0x0008, 0x0000, sn, strlen(sn)+1);

	byte_stream_put16(&bs, (flags & AIM_WARN_ANON) ? 0x0001 : 0x0000);
	byte_stream_put8(&bs, strlen(sn));
	byte_stream_putstr(&bs, sn);

	flap_connection_send_snac(od, conn, SNAC_FAMILY_ICBM, 0x0008, 0x0000, snacid, &bs);

	byte_stream_destroy(&bs);

	return 0;
}

/* Subtype 0x000a */
static int missedcall(OscarData *od, FlapConnection *conn, aim_module_t *mod, FlapFrame *frame, aim_modsnac_t *snac, ByteStream *bs)
{
	int ret = 0;
	aim_rxcallback_t userfunc;
	guint16 channel, nummissed, reason;
	aim_userinfo_t userinfo;

	while (byte_stream_empty(bs)) {

		channel = byte_stream_get16(bs);
		aim_info_extract(od, bs, &userinfo);
		nummissed = byte_stream_get16(bs);
		reason = byte_stream_get16(bs);

		if ((userfunc = aim_callhandler(od, snac->family, snac->subtype)))
			 ret = userfunc(od, conn, frame, channel, &userinfo, nummissed, reason);

		aim_info_free(&userinfo);
	}

	return ret;
}

/*
 * Subtype 0x000b
 *
 * Possible codes:
 *    AIM_TRANSFER_DENY_NOTSUPPORTED -- "client does not support"
 *    AIM_TRANSFER_DENY_DECLINE -- "client has declined transfer"
 *    AIM_TRANSFER_DENY_NOTACCEPTING -- "client is not accepting transfers"
 *
 */
int aim_im_denytransfer(OscarData *od, const char *sn, const guchar *cookie, guint16 code)
{
	FlapConnection *conn;
	ByteStream bs;
	aim_snacid_t snacid;
	GSList *tlvlist = NULL;

	if (!od || !(conn = flap_connection_findbygroup(od, SNAC_FAMILY_ICBM)))
		return -EINVAL;

	byte_stream_new(&bs, 8+2+1+strlen(sn)+6);

	snacid = aim_cachesnac(od, SNAC_FAMILY_ICBM, 0x000b, 0x0000, NULL, 0);

	byte_stream_putraw(&bs, cookie, 8);

	byte_stream_put16(&bs, 0x0002); /* channel */
	byte_stream_put8(&bs, strlen(sn));
	byte_stream_putstr(&bs, sn);

	aim_tlvlist_add_16(&tlvlist, 0x0003, code);
	aim_tlvlist_write(&bs, &tlvlist);
	aim_tlvlist_free(tlvlist);

	flap_connection_send_snac(od, conn, SNAC_FAMILY_ICBM, 0x000b, 0x0000, snacid, &bs);

	byte_stream_destroy(&bs);

	return 0;
}

static void parse_status_note_text(OscarData *od, guchar *cookie, char *sn, ByteStream *bs)
{
	struct aim_icq_info *info;
	struct aim_icq_info *prev_info;
	char *response;
	char *encoding;
	char *stripped_encoding;
	char *status_note_title;
	char *status_note_text;
	char *stripped_status_note_text;
	char *status_note;
	guint32 length;
	guint16 version;
	guint32 capability;
	guint8 message_type;
	guint16 status_code;
	guint16 text_length;
	guint32 request_length;
	guint32 response_length;
	guint32 encoding_length;
	PurpleAccount *account;
	PurpleBuddy *buddy;
	PurplePresence *presence;
	PurpleStatus *status;

	for (prev_info = NULL, info = od->icq_info; info != NULL; prev_info = info, info = info->next)
	{
		if (memcmp(&info->icbm_cookie, cookie, 8) == 0)
		{
			if (prev_info == NULL)
				od->icq_info = info->next;
			else
				prev_info->next = info->next;

			break;
		}
	}

	if (info == NULL)
		return;

	status_note_title = info->status_note_title;
	g_free(info);

	length = byte_stream_getle16(bs);
	if (length != 27) {
		purple_debug_misc("oscar", "clientautoresp: incorrect header "
				"size; expected 27, received %u.\n", length);
		g_free(status_note_title);
		return;
	}

	version = byte_stream_getle16(bs);
	if (version != 9) {
		purple_debug_misc("oscar", "clientautoresp: incorrect version; "
				"expected 9, received %u.\n", version);
		g_free(status_note_title);
		return;
	}

	capability = aim_locate_getcaps(od, bs, 0x10);
	if (capability != OSCAR_CAPABILITY_EMPTY) {
		purple_debug_misc("oscar", "clientautoresp: plugin ID is not null.\n");
		g_free(status_note_title);
		return;
	}

	byte_stream_advance(bs, 2); /* unknown */
	byte_stream_advance(bs, 4); /* client capabilities flags */
	byte_stream_advance(bs, 1); /* unknown */
	byte_stream_advance(bs, 2); /* downcouner? */

	length = byte_stream_getle16(bs);
	if (length != 14) {
		purple_debug_misc("oscar", "clientautoresp: incorrect header "
				"size; expected 14, received %u.\n", length);
		g_free(status_note_title);
		return;
	}

	byte_stream_advance(bs, 2); /* downcounter? */
	byte_stream_advance(bs, 12); /* unknown */

	message_type = byte_stream_get8(bs);
	if (message_type != 0x1a) {
		purple_debug_misc("oscar", "clientautoresp: incorrect message "
				"type; expected 0x1a, received 0x%x.\n", message_type);
		g_free(status_note_title);
		return;
	}

	byte_stream_advance(bs, 1); /* message flags */

	status_code = byte_stream_getle16(bs);
	if (status_code != 0) {
		purple_debug_misc("oscar", "clientautoresp: incorrect status "
				"code; expected 0, received %u.\n", status_code);
		g_free(status_note_title);
		return;
	}

	byte_stream_advance(bs, 2); /* priority code */

	text_length = byte_stream_getle16(bs);
	byte_stream_advance(bs, text_length); /* text */

	length = byte_stream_getle16(bs);
	byte_stream_advance(bs, 18); /* unknown */

	request_length = byte_stream_getle32(bs);
	if (length != 18 + 4 + request_length + 17) {
		purple_debug_misc("oscar", "clientautoresp: incorrect block; "
				"expected length is %u, got %u.\n",
				18 + 4 + request_length + 17, length);
		g_free(status_note_title);
		return;
	}

	byte_stream_advance(bs, request_length); /* x request */
	byte_stream_advance(bs, 17); /* unknown */

	length = byte_stream_getle32(bs);
	response_length = byte_stream_getle32(bs);
	response = byte_stream_getstr(bs, response_length);
	encoding_length = byte_stream_getle32(bs);
	if (length != 4 + response_length + 4 + encoding_length) {
		purple_debug_misc("oscar", "clientautoresp: incorrect block; "
				"expected length is %u, got %u.\n",
				4 + response_length + 4 + encoding_length, length);
		g_free(status_note_title);
		g_free(response);
		return;
	}

	encoding = byte_stream_getstr(bs, encoding_length);

	account = purple_connection_get_account(od->gc);

	stripped_encoding = oscar_encoding_extract(encoding);
	status_note_text = oscar_encoding_to_utf8(account, stripped_encoding, response, response_length);
	stripped_status_note_text = purple_markup_strip_html(status_note_text);

	if (stripped_status_note_text != NULL && stripped_status_note_text[0] != 0)
		status_note = g_strdup_printf("%s: %s", status_note_title, stripped_status_note_text);
	else
		status_note = g_strdup(status_note_title);

	g_free(status_note_title);
	g_free(response);
	g_free(encoding);
	g_free(stripped_encoding);
	g_free(status_note_text);
	g_free(stripped_status_note_text);

	buddy = purple_find_buddy(account, sn);
	if (buddy == NULL)
	{
		purple_debug_misc("oscar", "clientautoresp: buddy %s was not found.\n", sn);
		g_free(status_note);
		return;
	}

	purple_debug_misc("oscar", "clientautoresp: setting status "
			"message to \"%s\".\n", status_note);

	presence = purple_buddy_get_presence(buddy);
	status = purple_presence_get_active_status(presence);

	purple_prpl_got_user_status(account, sn,
			purple_status_get_id(status),
			"message", status_note, NULL);

	g_free(status_note);
}

/*
 * Subtype 0x000b - Receive the response from an ICQ status message
 * request (in which case this contains the ICQ status message) or
 * a file transfer or direct IM request was declined.
 */
static int clientautoresp(OscarData *od, FlapConnection *conn, aim_module_t *mod, FlapFrame *frame, aim_modsnac_t *snac, ByteStream *bs)
{
	int ret = 0;
	aim_rxcallback_t userfunc;
	guint16 channel, reason;
	char *sn;
	guchar *cookie;
	guint8 snlen;

	cookie = byte_stream_getraw(bs, 8);
	channel = byte_stream_get16(bs);
	snlen = byte_stream_get8(bs);
	sn = byte_stream_getstr(bs, snlen);
	reason = byte_stream_get16(bs);

	if (channel == 0x0002)
	{
		if (reason == 0x0003) /* channel-specific */
			/* parse status note text */
			parse_status_note_text(od, cookie, sn, bs);

		byte_stream_get16(bs); /* Unknown */
		byte_stream_get16(bs); /* Unknown */
		if ((userfunc = aim_callhandler(od, snac->family, snac->subtype)))
			ret = userfunc(od, conn, frame, channel, sn, reason, cookie);

	} else if (channel == 0x0004) { /* ICQ message */
		switch (reason) {
			case 0x0003: { /* ICQ status message.  Maybe other stuff too, you never know with these people. */
				guint8 statusmsgtype, *msg;
				guint16 len;
				guint32 state;

				len = byte_stream_getle16(bs); /* Should be 0x001b */
				byte_stream_advance(bs, len); /* Unknown */

				len = byte_stream_getle16(bs); /* Should be 0x000e */
				byte_stream_advance(bs, len); /* Unknown */

				statusmsgtype = byte_stream_getle8(bs);
				switch (statusmsgtype) {
					case 0xe8:
						state = AIM_ICQ_STATE_AWAY;
						break;
					case 0xe9:
						state = AIM_ICQ_STATE_AWAY | AIM_ICQ_STATE_BUSY;
						break;
					case 0xea:
						state = AIM_ICQ_STATE_AWAY | AIM_ICQ_STATE_OUT;
						break;
					case 0xeb:
						state = AIM_ICQ_STATE_AWAY | AIM_ICQ_STATE_DND | AIM_ICQ_STATE_BUSY;
						break;
					case 0xec:
						state = AIM_ICQ_STATE_CHAT;
						break;
					default:
						state = 0;
						break;
				}

				byte_stream_getle8(bs); /* Unknown - 0x03 Maybe this means this is an auto-reply */
				byte_stream_getle16(bs); /* Unknown - 0x0000 */
				byte_stream_getle16(bs); /* Unknown - 0x0000 */

				len = byte_stream_getle16(bs);
				msg = byte_stream_getraw(bs, len);

				if ((userfunc = aim_callhandler(od, snac->family, snac->subtype)))
					ret = userfunc(od, conn, frame, channel, sn, reason, state, msg);

				g_free(msg);
			} break;

			default: {
				if ((userfunc = aim_callhandler(od, snac->family, snac->subtype)))
					ret = userfunc(od, conn, frame, channel, sn, reason);
			} break;
		} /* end switch */
	}

	g_free(cookie);
	g_free(sn);

	return ret;
}

/*
 * Subtype 0x000c - Receive an ack after sending an ICBM.
 *
 * You have to have send the message with the AIM_IMFLAGS_ACK flag set
 * (TLV t(0003)).  The ack contains the ICBM header of the message you
 * sent.
 *
 */
static int msgack(OscarData *od, FlapConnection *conn, aim_module_t *mod, FlapFrame *frame, aim_modsnac_t *snac, ByteStream *bs)
{
	aim_rxcallback_t userfunc;
	guint16 ch;
	guchar *cookie;
	char *sn;
	int ret = 0;

	cookie = byte_stream_getraw(bs, 8);
	ch = byte_stream_get16(bs);
	sn = byte_stream_getstr(bs, byte_stream_get8(bs));

	if ((userfunc = aim_callhandler(od, snac->family, snac->subtype)))
		ret = userfunc(od, conn, frame, ch, sn);

	g_free(sn);
	g_free(cookie);

	return ret;
}

/*
 * Subtype 0x0010 - Request any offline messages that are waiting for
 * us.  This is the "new" way of handling offline messages which is
 * used for both AIM and ICQ.  The old way is to use the ugly
 * aim_icq_reqofflinemsgs() function, but that is no longer necessary.
 *
 * We set the 0x00000100 flag on the ICBM message parameters, which
 * tells the oscar servers that we support offline messages.  When we
 * set that flag the servers do not automatically send us offline
 * messages.  Instead we must request them using this function.  This
 * should happen after sending the 0x0001/0x0002 "client online" SNAC.
 */
int aim_im_reqofflinemsgs(OscarData *od)
{
	FlapConnection *conn;

	if (!od || !(conn = flap_connection_findbygroup(od, 0x0002)))
		return -EINVAL;

	aim_genericreq_n(od, conn, SNAC_FAMILY_ICBM, 0x0010);

	return 0;
}

/*
 * Subtype 0x0014 - Send a mini typing notification (mtn) packet.
 *
 * This is supported by winaim5 and newer, MacAIM bleh and newer, iChat bleh and newer,
 * and Purple 0.60 and newer.
 *
 */
int aim_im_sendmtn(OscarData *od, guint16 type1, const char *sn, guint16 type2)
{
	FlapConnection *conn;
	ByteStream bs;
	aim_snacid_t snacid;

	if (!od || !(conn = flap_connection_findbygroup(od, 0x0002)))
		return -EINVAL;

	if (!sn)
		return -EINVAL;

	byte_stream_new(&bs, 11+strlen(sn)+2);

	snacid = aim_cachesnac(od, SNAC_FAMILY_ICBM, 0x0014, 0x0000, NULL, 0);

	/*
	 * 8 days of light
	 * Er, that is to say, 8 bytes of 0's
	 */
	byte_stream_put16(&bs, 0x0000);
	byte_stream_put16(&bs, 0x0000);
	byte_stream_put16(&bs, 0x0000);
	byte_stream_put16(&bs, 0x0000);

	/*
	 * Type 1 (should be 0x0001 for mtn)
	 */
	byte_stream_put16(&bs, type1);

	/*
	 * Dest sn
	 */
	byte_stream_put8(&bs, strlen(sn));
	byte_stream_putstr(&bs, sn);

	/*
	 * Type 2 (should be 0x0000, 0x0001, or 0x0002 for mtn)
	 */
	byte_stream_put16(&bs, type2);

	flap_connection_send_snac(od, conn, SNAC_FAMILY_ICBM, 0x0014, 0x0000, snacid, &bs);

	byte_stream_destroy(&bs);

	return 0;
}

/*
 * Subtype 0x0014 - Receive a mini typing notification (mtn) packet.
 *
 * This is supported by winaim5 and newer, MacAIM bleh and newer, iChat bleh and newer,
 * and Purple 0.60 and newer.
 *
 */
static int mtn_receive(OscarData *od, FlapConnection *conn, aim_module_t *mod, FlapFrame *frame, aim_modsnac_t *snac, ByteStream *bs)
{
	int ret = 0;
	aim_rxcallback_t userfunc;
	char *sn;
	guint8 snlen;
	guint16 type1, type2;

	byte_stream_advance(bs, 8); /* Unknown - All 0's */
	type1 = byte_stream_get16(bs);
	snlen = byte_stream_get8(bs);
	sn = byte_stream_getstr(bs, snlen);
	type2 = byte_stream_get16(bs);

	if ((userfunc = aim_callhandler(od, snac->family, snac->subtype)))
		ret = userfunc(od, conn, frame, type1, sn, type2);

	g_free(sn);

	return ret;
}

static int
snachandler(OscarData *od, FlapConnection *conn, aim_module_t *mod, FlapFrame *frame, aim_modsnac_t *snac, ByteStream *bs)
{
	if (snac->subtype == 0x0005)
		return aim_im_paraminfo(od, conn, mod, frame, snac, bs);
	else if (snac->subtype == 0x0006)
		return outgoingim(od, conn, mod, frame, snac, bs);
	else if (snac->subtype == 0x0007)
		return incomingim(od, conn, mod, frame, snac, bs);
	else if (snac->subtype == 0x000a)
		return missedcall(od, conn, mod, frame, snac, bs);
	else if (snac->subtype == 0x000b)
		return clientautoresp(od, conn, mod, frame, snac, bs);
	else if (snac->subtype == 0x000c)
		return msgack(od, conn, mod, frame, snac, bs);
	else if (snac->subtype == 0x0014)
		return mtn_receive(od, conn, mod, frame, snac, bs);

	return 0;
}

int
msg_modfirst(OscarData *od, aim_module_t *mod)
{
	mod->family = SNAC_FAMILY_ICBM;
	mod->version = 0x0001;
	mod->toolid = 0x0110;
	mod->toolversion = 0x0629;
	mod->flags = 0;
	strncpy(mod->name, "messaging", sizeof(mod->name));
	mod->snachandler = snachandler;

	return 0;
}
