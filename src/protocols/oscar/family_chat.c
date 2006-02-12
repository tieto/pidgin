/*
 * Gaim's oscar protocol plugin
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
 * Family 0x000e - Routines for the Chat service.
 *
 */

#include "oscar.h"

#include <string.h>

/* Stored in the ->internal of chat connections */
struct chatconnpriv {
	guint16 exchange;
	char *name;
	guint16 instance;
};

faim_internal void oscar_connection_destroy_chat(OscarSession *sess, OscarConnection *conn)
{
	struct chatconnpriv *ccp = (struct chatconnpriv *)conn->internal;

	if (ccp)
		free(ccp->name);
	free(ccp);

	return;
}

faim_export char *aim_chat_getname(OscarConnection *conn)
{
	struct chatconnpriv *ccp;

	if (!conn)
		return NULL;

	if (conn->type != AIM_CONN_TYPE_CHAT)
		return NULL;

	ccp = (struct chatconnpriv *)conn->internal;

	return ccp->name;
}

/* XXX get this into conn.c -- evil!! */
faim_export OscarConnection *aim_chat_getconn(OscarSession *sess, const char *name)
{
	GList *cur;

	for (cur = sess->oscar_connections; cur; cur = cur->next)
	{
		OscarConnection *conn;
		struct chatconnpriv *ccp;

		conn = cur->data;
		ccp = (struct chatconnpriv *)conn->internal;

		if (conn->type != AIM_CONN_TYPE_CHAT)
			continue;
		if (!conn->internal) {
			gaim_debug_misc("oscar", "faim: chat: chat connection with no name! (fd = %d)\n", conn->fd);
			continue;
		}

		if (strcmp(ccp->name, name) == 0)
			return conn;;
	}

	return NULL;
}

faim_export int aim_chat_attachname(OscarConnection *conn, guint16 exchange, const char *roomname, guint16 instance)
{
	struct chatconnpriv *ccp;

	if (!conn || !roomname)
		return -EINVAL;

	if (conn->internal)
		free(conn->internal);

	if (!(ccp = malloc(sizeof(struct chatconnpriv))))
		return -ENOMEM;

	ccp->exchange = exchange;
	ccp->name = strdup(roomname);
	ccp->instance = instance;

	conn->internal = (void *)ccp;

	return 0;
}

faim_internal int aim_chat_readroominfo(ByteStream *bs, struct aim_chat_roominfo *outinfo)
{
	int namelen;

	if (!bs || !outinfo)
		return 0;

	outinfo->exchange = aimbs_get16(bs);
	namelen = aimbs_get8(bs);
	outinfo->name = aimbs_getstr(bs, namelen);
	outinfo->instance = aimbs_get16(bs);

	return 0;
}

faim_export int aim_chat_leaveroom(OscarSession *sess, const char *name)
{
	OscarConnection *conn;

	if (!(conn = aim_chat_getconn(sess, name)))
		return -ENOENT;

	aim_conn_close(sess, conn);

	return 0;
}

/*
 * Subtype 0x0002 - General room information.  Lots of stuff.
 *
 * Values I know are in here but I haven't attached
 * them to any of the 'Unknown's:
 *	- Language (English)
 *
 */
static int infoupdate(OscarSession *sess, aim_module_t *mod, FlapFrame *rx, aim_modsnac_t *snac, ByteStream *bs)
{
	aim_userinfo_t *userinfo = NULL;
	aim_rxcallback_t userfunc;
	int ret = 0;
	int usercount = 0;
	guint8 detaillevel = 0;
	char *roomname = NULL;
	struct aim_chat_roominfo roominfo;
	guint16 tlvcount = 0;
	aim_tlvlist_t *tlvlist;
	char *roomdesc = NULL;
	guint16 flags = 0;
	guint32 creationtime = 0;
	guint16 maxmsglen = 0, maxvisiblemsglen = 0;
	guint16 unknown_d2 = 0, unknown_d5 = 0;

	aim_chat_readroominfo(bs, &roominfo);

	detaillevel = aimbs_get8(bs);

	if (detaillevel != 0x02) {
		gaim_debug_misc("oscar", "faim: chat_roomupdateinfo: detail level %d not supported\n", detaillevel);
		return 1;
	}

	tlvcount = aimbs_get16(bs);

	/*
	 * Everything else are TLVs.
	 */
	tlvlist = aim_tlvlist_read(bs);

	/*
	 * TLV type 0x006a is the room name in Human Readable Form.
	 */
	if (aim_tlv_gettlv(tlvlist, 0x006a, 1))
		roomname = aim_tlv_getstr(tlvlist, 0x006a, 1);

	/*
	 * Type 0x006f: Number of occupants.
	 */
	if (aim_tlv_gettlv(tlvlist, 0x006f, 1))
		usercount = aim_tlv_get16(tlvlist, 0x006f, 1);

	/*
	 * Type 0x0073:  Occupant list.
	 */
	if (aim_tlv_gettlv(tlvlist, 0x0073, 1)) {
		int curoccupant = 0;
		aim_tlv_t *tmptlv;
		ByteStream occbs;

		tmptlv = aim_tlv_gettlv(tlvlist, 0x0073, 1);

		/* Allocate enough userinfo structs for all occupants */
		userinfo = calloc(usercount, sizeof(aim_userinfo_t));

		aim_bstream_init(&occbs, tmptlv->value, tmptlv->length);

		while (curoccupant < usercount)
			aim_info_extract(sess, &occbs, &userinfo[curoccupant++]);
	}

	/*
	 * Type 0x00c9: Flags. (AIM_CHATROOM_FLAG)
	 */
	if (aim_tlv_gettlv(tlvlist, 0x00c9, 1))
		flags = aim_tlv_get16(tlvlist, 0x00c9, 1);

	/*
	 * Type 0x00ca: Creation time (4 bytes)
	 */
	if (aim_tlv_gettlv(tlvlist, 0x00ca, 1))
		creationtime = aim_tlv_get32(tlvlist, 0x00ca, 1);

	/*
	 * Type 0x00d1: Maximum Message Length
	 */
	if (aim_tlv_gettlv(tlvlist, 0x00d1, 1))
		maxmsglen = aim_tlv_get16(tlvlist, 0x00d1, 1);

	/*
	 * Type 0x00d2: Unknown. (2 bytes)
	 */
	if (aim_tlv_gettlv(tlvlist, 0x00d2, 1))
		unknown_d2 = aim_tlv_get16(tlvlist, 0x00d2, 1);

	/*
	 * Type 0x00d3: Room Description
	 */
	if (aim_tlv_gettlv(tlvlist, 0x00d3, 1))
		roomdesc = aim_tlv_getstr(tlvlist, 0x00d3, 1);

#if 0
	/*
	 * Type 0x000d4: Unknown (flag only)
	 */
	if (aim_tlv_gettlv(tlvlist, 0x000d4, 1)) {
		/* Unhandled */
	}
#endif

	/*
	 * Type 0x00d5: Unknown. (1 byte)
	 */
	if (aim_tlv_gettlv(tlvlist, 0x00d5, 1))
		unknown_d5 = aim_tlv_get8(tlvlist, 0x00d5, 1);

#if 0
	/*
	 * Type 0x00d6: Encoding 1 ("us-ascii")
	 */
	if (aim_tlv_gettlv(tlvlist, 0x000d6, 1)) {
		/* Unhandled */
	}

	/*
	 * Type 0x00d7: Language 1 ("en")
	 */
	if (aim_tlv_gettlv(tlvlist, 0x000d7, 1)) {
		/* Unhandled */
	}

	/*
	 * Type 0x00d8: Encoding 2 ("us-ascii")
	 */
	if (aim_tlv_gettlv(tlvlist, 0x000d8, 1)) {
		/* Unhandled */
	}

	/*
	 * Type 0x00d9: Language 2 ("en")
	 */
	if (aim_tlv_gettlv(tlvlist, 0x000d9, 1)) {
		/* Unhandled */
	}
#endif

	/*
	 * Type 0x00da: Maximum visible message length
	 */
	if (aim_tlv_gettlv(tlvlist, 0x000da, 1))
		maxvisiblemsglen = aim_tlv_get16(tlvlist, 0x00da, 1);

	if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype))) {
		ret = userfunc(sess,
				rx,
				&roominfo,
				roomname,
				usercount,
				userinfo,
				roomdesc,
				flags,
				creationtime,
				maxmsglen,
				unknown_d2,
				unknown_d5,
				maxvisiblemsglen);
	}

	free(roominfo.name);

	while (usercount > 0)
		aim_info_free(&userinfo[--usercount]);

	free(userinfo);
	free(roomname);
	free(roomdesc);
	aim_tlvlist_free(&tlvlist);

	return ret;
}

/* Subtypes 0x0003 and 0x0004 */
static int userlistchange(OscarSession *sess, aim_module_t *mod, FlapFrame *rx, aim_modsnac_t *snac, ByteStream *bs)
{
	aim_userinfo_t *userinfo = NULL;
	aim_rxcallback_t userfunc;
	int curcount = 0, ret = 0;

	while (aim_bstream_empty(bs)) {
		curcount++;
		userinfo = realloc(userinfo, curcount * sizeof(aim_userinfo_t));
		aim_info_extract(sess, bs, &userinfo[curcount-1]);
	}

	if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
		ret = userfunc(sess, rx, curcount, userinfo);

	aim_info_free(userinfo);
	free(userinfo);

	return ret;
}

/*
 * Subtype 0x0005 - Send a Chat Message.
 *
 * Possible flags:
 *   AIM_CHATFLAGS_NOREFLECT   --  Unset the flag that requests messages
 *                                 should be sent to their sender.
 *   AIM_CHATFLAGS_AWAY        --  Mark the message as an autoresponse
 *                                 (Note that WinAIM does not honor this,
 *                                 and displays the message as normal.)
 *
 * XXX convert this to use tlvchains
 */
faim_export int aim_chat_send_im(OscarSession *sess, OscarConnection *conn, guint16 flags, const gchar *msg, int msglen, const char *encoding, const char *language)
{
	int i;
	FlapFrame *fr;
	IcbmCookie *cookie;
	aim_snacid_t snacid;
	guint8 ckstr[8];
	aim_tlvlist_t *otl = NULL, *itl = NULL;

	if (!sess || !conn || !msg || (msglen <= 0))
		return 0;

	if (!(fr = flap_frame_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 1152)))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x000e, 0x0005, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x000e, 0x0005, 0x0000, snacid);

	/*
	 * Cookie
	 *
	 * XXX mkcookie should generate the cookie and cache it in one
	 * operation to preserve uniqueness.
	 */
	for (i = 0; i < 8; i++)
		ckstr[i] = (guint8)rand();

	cookie = aim_mkcookie(ckstr, AIM_COOKIETYPE_CHAT, NULL);
	cookie->data = NULL; /* XXX store something useful here */

	aim_cachecookie(sess, cookie);

	/* ICBM Header */
	aimbs_putraw(&fr->data, ckstr, 8); /* Cookie */
	aimbs_put16(&fr->data, 0x0003); /* Channel */

	/*
	 * Type 1: Flag meaning this message is destined to the room.
	 */
	aim_tlvlist_add_noval(&otl, 0x0001);

	/*
	 * Type 6: Reflect
	 */
	if (!(flags & AIM_CHATFLAGS_NOREFLECT))
		aim_tlvlist_add_noval(&otl, 0x0006);

	/*
	 * Type 7: Autoresponse
	 */
	if (flags & AIM_CHATFLAGS_AWAY)
		aim_tlvlist_add_noval(&otl, 0x0007);

	/*
	 * SubTLV: Type 1: Message
	 */
	aim_tlvlist_add_raw(&itl, 0x0001, msglen, (guchar *)msg);

	/*
	 * SubTLV: Type 2: Encoding
	 */
	if (encoding != NULL)
		aim_tlvlist_add_str(&itl, 0x0002, encoding);

	/*
	 * SubTLV: Type 3: Language
	 */
	if (language != NULL)
		aim_tlvlist_add_str(&itl, 0x0003, language);

	/*
	 * Type 5: Message block.  Contains more TLVs.
	 *
	 * This could include other information... We just
	 * put in a message TLV however.
	 *
	 */
	aim_tlvlist_add_frozentlvlist(&otl, 0x0005, &itl);

	aim_tlvlist_write(&fr->data, &otl);

	aim_tlvlist_free(&itl);
	aim_tlvlist_free(&otl);

	aim_tx_enqueue(sess, fr);

	return 0;
}

/*
 * Subtype 0x0006
 *
 * We could probably include this in the normal ICBM parsing
 * code as channel 0x0003, however, since only the start
 * would be the same, we might as well do it here.
 *
 * General outline of this SNAC:
 *   snac
 *   cookie
 *   channel id
 *   tlvlist
 *     unknown
 *     source user info
 *       name
 *       evility
 *       userinfo tlvs
 *         online time
 *         etc
 *     message metatlv
 *       message tlv
 *         message string
 *       possibly others
 *
 */
static int incomingim_ch3(OscarSession *sess, aim_module_t *mod, FlapFrame *rx, aim_modsnac_t *snac, ByteStream *bs)
{
	int ret = 0, i;
	aim_rxcallback_t userfunc;
	aim_userinfo_t userinfo;
	guint8 cookie[8];
	guint16 channel;
	aim_tlvlist_t *otl;
	char *msg = NULL;
	int len = 0;
	char *encoding = NULL, *language = NULL;
	IcbmCookie *ck;

	memset(&userinfo, 0, sizeof(aim_userinfo_t));

	/*
	 * Read ICBM Cookie.
	 */
	for (i = 0; i < 8; i++)
		cookie[i] = aimbs_get8(bs);

	if ((ck = aim_uncachecookie(sess, cookie, AIM_COOKIETYPE_CHAT))) {
		free(ck->data);
		free(ck);
	}

	/*
	 * Channel ID
	 *
	 * Channel 0x0003 is used for chat messages.
	 *
	 */
	channel = aimbs_get16(bs);

	if (channel != 0x0003) {
		gaim_debug_misc("oscar", "faim: chat_incoming: unknown channel! (0x%04x)\n", channel);
		return 0;
	}

	/*
	 * Start parsing TLVs right away.
	 */
	otl = aim_tlvlist_read(bs);

	/*
	 * Type 0x0003: Source User Information
	 */
	if (aim_tlv_gettlv(otl, 0x0003, 1)) {
		aim_tlv_t *userinfotlv;
		ByteStream tbs;

		userinfotlv = aim_tlv_gettlv(otl, 0x0003, 1);

		aim_bstream_init(&tbs, userinfotlv->value, userinfotlv->length);
		aim_info_extract(sess, &tbs, &userinfo);
	}

#if 0
	/*
	 * Type 0x0001: If present, it means it was a message to the
	 * room (as opposed to a whisper).
	 */
	if (aim_tlv_gettlv(otl, 0x0001, 1)) {
		/* Unhandled */
	}
#endif

	/*
	 * Type 0x0005: Message Block.  Conains more TLVs.
	 */
	if (aim_tlv_gettlv(otl, 0x0005, 1)) {
		aim_tlvlist_t *itl;
		aim_tlv_t *msgblock;
		ByteStream tbs;

		msgblock = aim_tlv_gettlv(otl, 0x0005, 1);
		aim_bstream_init(&tbs, msgblock->value, msgblock->length);
		itl = aim_tlvlist_read(&tbs);

		/*
		 * Type 0x0001: Message.
		 */
		if (aim_tlv_gettlv(itl, 0x0001, 1)) {
			msg = aim_tlv_getstr(itl, 0x0001, 1);
			len = aim_tlv_gettlv(itl, 0x0001, 1)->length;
		}

		/*
		 * Type 0x0002: Encoding.
		 */
		if (aim_tlv_gettlv(itl, 0x0002, 1))
			encoding = aim_tlv_getstr(itl, 0x0002, 1);

		/*
		 * Type 0x0003: Language.
		 */
		if (aim_tlv_gettlv(itl, 0x0003, 1))
			language = aim_tlv_getstr(itl, 0x0003, 1);

		aim_tlvlist_free(&itl);
	}

	if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
		ret = userfunc(sess, rx, &userinfo, len, msg, encoding, language);

	aim_info_free(&userinfo);
	free(msg);
	aim_tlvlist_free(&otl);

	return ret;
}

static int snachandler(OscarSession *sess, aim_module_t *mod, FlapFrame *rx, aim_modsnac_t *snac, ByteStream *bs)
{

	if (snac->subtype == 0x0002)
		return infoupdate(sess, mod, rx, snac, bs);
	else if ((snac->subtype == 0x0003) || (snac->subtype == 0x0004))
		return userlistchange(sess, mod, rx, snac, bs);
	else if (snac->subtype == 0x0006)
		return incomingim_ch3(sess, mod, rx, snac, bs);

	return 0;
}

faim_internal int chat_modfirst(OscarSession *sess, aim_module_t *mod)
{

	mod->family = 0x000e;
	mod->version = 0x0001;
	mod->toolid = 0x0010;
	mod->toolversion = 0x0629;
	mod->flags = 0;
	strncpy(mod->name, "chat", sizeof(mod->name));
	mod->snachandler = snachandler;

	return 0;
}
