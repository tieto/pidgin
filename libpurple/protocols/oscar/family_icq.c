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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
 * Family 0x0015 - Encapsulated ICQ.
 *
 */

#include "oscar.h"

int aim_icq_reqofflinemsgs(OscarData *od)
{
	FlapConnection *conn;
	FlapFrame *frame;
	aim_snacid_t snacid;
	int bslen;

	if (!od || !(conn = flap_connection_findbygroup(od, 0x0015)))
		return -EINVAL;

	bslen = 2 + 4 + 2 + 2;

	frame = flap_frame_new(od, 0x02, 10 + 4 + bslen);

	snacid = aim_cachesnac(od, 0x0015, 0x0002, 0x0000, NULL, 0);
	aim_putsnac(&frame->data, 0x0015, 0x0002, 0x0000, snacid);

	/* For simplicity, don't bother using a tlvlist */
	byte_stream_put16(&frame->data, 0x0001);
	byte_stream_put16(&frame->data, bslen);

	byte_stream_putle16(&frame->data, bslen - 2);
	byte_stream_putle32(&frame->data, atoi(od->sn));
	byte_stream_putle16(&frame->data, 0x003c); /* I command thee. */
	byte_stream_putle16(&frame->data, snacid); /* eh. */

	flap_connection_send(conn, frame);

	return 0;
}

int aim_icq_ackofflinemsgs(OscarData *od)
{
	FlapConnection *conn;
	FlapFrame *frame;
	aim_snacid_t snacid;
	int bslen;

	if (!od || !(conn = flap_connection_findbygroup(od, 0x0015)))
		return -EINVAL;

	bslen = 2 + 4 + 2 + 2;

	frame = flap_frame_new(od, 0x02, 10 + 4 + bslen);

	snacid = aim_cachesnac(od, 0x0015, 0x0002, 0x0000, NULL, 0);
	aim_putsnac(&frame->data, 0x0015, 0x0002, 0x0000, snacid);

	/* For simplicity, don't bother using a tlvlist */
	byte_stream_put16(&frame->data, 0x0001);
	byte_stream_put16(&frame->data, bslen);

	byte_stream_putle16(&frame->data, bslen - 2);
	byte_stream_putle32(&frame->data, atoi(od->sn));
	byte_stream_putle16(&frame->data, 0x003e); /* I command thee. */
	byte_stream_putle16(&frame->data, snacid); /* eh. */

	flap_connection_send(conn, frame);

	return 0;
}

int
aim_icq_setsecurity(OscarData *od, gboolean auth_required, gboolean webaware)
{
	FlapConnection *conn;
	FlapFrame *frame;
	aim_snacid_t snacid;
	int bslen;

	if (!od || !(conn = flap_connection_findbygroup(od, 0x0015)))
		return -EINVAL;

	bslen = 2+4+2+2+2+2+2+1+1+1+1+1+1;

	frame = flap_frame_new(od, 0x02, 10 + 4 + bslen);

	snacid = aim_cachesnac(od, 0x0015, 0x0002, 0x0000, NULL, 0);
	aim_putsnac(&frame->data, 0x0015, 0x0002, 0x0000, snacid);

	/* For simplicity, don't bother using a tlvlist */
	byte_stream_put16(&frame->data, 0x0001);
	byte_stream_put16(&frame->data, bslen);

	byte_stream_putle16(&frame->data, bslen - 2);
	byte_stream_putle32(&frame->data, atoi(od->sn));
	byte_stream_putle16(&frame->data, 0x07d0); /* I command thee. */
	byte_stream_putle16(&frame->data, snacid); /* eh. */
	byte_stream_putle16(&frame->data, 0x0c3a); /* shrug. */
	byte_stream_putle16(&frame->data, 0x030c);
	byte_stream_putle16(&frame->data, 0x0001);
	byte_stream_putle8(&frame->data, webaware);
	byte_stream_putle8(&frame->data, 0xf8);
	byte_stream_putle8(&frame->data, 0x02);
	byte_stream_putle8(&frame->data, 0x01);
	byte_stream_putle8(&frame->data, 0x00);
	byte_stream_putle8(&frame->data, !auth_required);

	flap_connection_send(conn, frame);

	return 0;
}

/**
 * Change your ICQ password.
 *
 * @param od The oscar session
 * @param passwd The new password.  If this is longer than 8 characters it
 *        will be truncated.
 * @return Return 0 if no errors, otherwise return the error number.
 */
int aim_icq_changepasswd(OscarData *od, const char *passwd)
{
	FlapConnection *conn;
	FlapFrame *frame;
	aim_snacid_t snacid;
	int bslen, passwdlen;

	if (!passwd)
		return -EINVAL;

	if (!od || !(conn = flap_connection_findbygroup(od, 0x0015)))
		return -EINVAL;

	passwdlen = strlen(passwd);
	if (passwdlen > MAXICQPASSLEN)
		passwdlen = MAXICQPASSLEN;
	bslen = 2+4+2+2+2+2+passwdlen+1;

	frame = flap_frame_new(od, 0x02, 10 + 4 + bslen);

	snacid = aim_cachesnac(od, 0x0015, 0x0002, 0x0000, NULL, 0);
	aim_putsnac(&frame->data, 0x0015, 0x0002, 0x0000, snacid);

	/* For simplicity, don't bother using a tlvlist */
	byte_stream_put16(&frame->data, 0x0001);
	byte_stream_put16(&frame->data, bslen);

	byte_stream_putle16(&frame->data, bslen - 2);
	byte_stream_putle32(&frame->data, atoi(od->sn));
	byte_stream_putle16(&frame->data, 0x07d0); /* I command thee. */
	byte_stream_putle16(&frame->data, snacid); /* eh. */
	byte_stream_putle16(&frame->data, 0x042e); /* shrug. */
	byte_stream_putle16(&frame->data, passwdlen+1);
	byte_stream_putstr(&frame->data, passwd);
	byte_stream_putle8(&frame->data, '\0');

	flap_connection_send(conn, frame);

	return 0;
}

int aim_icq_getallinfo(OscarData *od, const char *uin)
{
	FlapConnection *conn;
	FlapFrame *frame;
	aim_snacid_t snacid;
	int bslen;
	struct aim_icq_info *info;

	if (!uin || uin[0] < '0' || uin[0] > '9')
		return -EINVAL;

	if (!od || !(conn = flap_connection_findbygroup(od, 0x0015)))
		return -EINVAL;

	bslen = 2 + 4 + 2 + 2 + 2 + 4;

	frame = flap_frame_new(od, 0x02, 10 + 4 + bslen);

	snacid = aim_cachesnac(od, 0x0015, 0x0002, 0x0000, NULL, 0);
	aim_putsnac(&frame->data, 0x0015, 0x0002, 0x0000, snacid);

	/* For simplicity, don't bother using a tlvlist */
	byte_stream_put16(&frame->data, 0x0001);
	byte_stream_put16(&frame->data, bslen);

	byte_stream_putle16(&frame->data, bslen - 2);
	byte_stream_putle32(&frame->data, atoi(od->sn));
	byte_stream_putle16(&frame->data, 0x07d0); /* I command thee. */
	byte_stream_putle16(&frame->data, snacid); /* eh. */
	byte_stream_putle16(&frame->data, 0x04b2); /* shrug. */
	byte_stream_putle32(&frame->data, atoi(uin));

	flap_connection_send(conn, frame);

	/* Keep track of this request and the ICQ number and request ID */
	info = (struct aim_icq_info *)calloc(1, sizeof(struct aim_icq_info));
	info->reqid = snacid;
	info->uin = atoi(uin);
	info->next = od->icq_info;
	od->icq_info = info;

	return 0;
}

int aim_icq_getalias(OscarData *od, const char *uin)
{
	FlapConnection *conn;
	FlapFrame *frame;
	aim_snacid_t snacid;
	int bslen;
	struct aim_icq_info *info;

	if (!uin || uin[0] < '0' || uin[0] > '9')
		return -EINVAL;

	if (!od || !(conn = flap_connection_findbygroup(od, 0x0015)))
		return -EINVAL;

	bslen = 2 + 4 + 2 + 2 + 2 + 4;

	frame = flap_frame_new(od, 0x02, 10 + 4 + bslen);

	snacid = aim_cachesnac(od, 0x0015, 0x0002, 0x0000, NULL, 0);
	aim_putsnac(&frame->data, 0x0015, 0x0002, 0x0000, snacid);

	/* For simplicity, don't bother using a tlvlist */
	byte_stream_put16(&frame->data, 0x0001);
	byte_stream_put16(&frame->data, bslen);

	byte_stream_putle16(&frame->data, bslen - 2);
	byte_stream_putle32(&frame->data, atoi(od->sn));
	byte_stream_putle16(&frame->data, 0x07d0); /* I command thee. */
	byte_stream_putle16(&frame->data, snacid); /* eh. */
	byte_stream_putle16(&frame->data, 0x04ba); /* shrug. */
	byte_stream_putle32(&frame->data, atoi(uin));

	flap_connection_send(conn, frame);

	/* Keep track of this request and the ICQ number and request ID */
	info = (struct aim_icq_info *)calloc(1, sizeof(struct aim_icq_info));
	info->reqid = snacid;
	info->uin = atoi(uin);
	info->next = od->icq_info;
	od->icq_info = info;

	return 0;
}

int aim_icq_getsimpleinfo(OscarData *od, const char *uin)
{
	FlapConnection *conn;
	FlapFrame *frame;
	aim_snacid_t snacid;
	int bslen;

	if (!uin || uin[0] < '0' || uin[0] > '9')
		return -EINVAL;

	if (!od || !(conn = flap_connection_findbygroup(od, 0x0015)))
		return -EINVAL;

	bslen = 2 + 4 + 2 + 2 + 2 + 4;

	frame = flap_frame_new(od, 0x02, 10 + 4 + bslen);

	snacid = aim_cachesnac(od, 0x0015, 0x0002, 0x0000, NULL, 0);
	aim_putsnac(&frame->data, 0x0015, 0x0002, 0x0000, snacid);

	/* For simplicity, don't bother using a tlvlist */
	byte_stream_put16(&frame->data, 0x0001);
	byte_stream_put16(&frame->data, bslen);

	byte_stream_putle16(&frame->data, bslen - 2);
	byte_stream_putle32(&frame->data, atoi(od->sn));
	byte_stream_putle16(&frame->data, 0x07d0); /* I command thee. */
	byte_stream_putle16(&frame->data, snacid); /* eh. */
	byte_stream_putle16(&frame->data, 0x051f); /* shrug. */
	byte_stream_putle32(&frame->data, atoi(uin));

	flap_connection_send(conn, frame);

	return 0;
}

#if 0
int aim_icq_sendxmlreq(OscarData *od, const char *xml)
{
	FlapConnection *conn;
	FlapFrame *frame;
	aim_snacid_t snacid;
	int bslen;

	if (!xml || !strlen(xml))
		return -EINVAL;

	if (!od || !(conn = flap_connection_findbygroup(od, 0x0015)))
		return -EINVAL;

	bslen = 2 + 10 + 2 + strlen(xml) + 1;

	frame = flap_frame_new(od, 0x02, 10 + 4 + bslen);

	snacid = aim_cachesnac(od, 0x0015, 0x0002, 0x0000, NULL, 0);
	aim_putsnac(&frame->data, 0x0015, 0x0002, 0x0000, snacid);

	/* For simplicity, don't bother using a tlvlist */
	byte_stream_put16(&frame->data, 0x0001);
	byte_stream_put16(&frame->data, bslen);

	byte_stream_putle16(&frame->data, bslen - 2);
	byte_stream_putle32(&frame->data, atoi(od->sn));
	byte_stream_putle16(&frame->data, 0x07d0); /* I command thee. */
	byte_stream_putle16(&frame->data, snacid); /* eh. */
	byte_stream_putle16(&frame->data, 0x0998); /* shrug. */
	byte_stream_putle16(&frame->data, strlen(xml) + 1);
	byte_stream_putraw(&frame->data, (guint8 *)xml, strlen(xml) + 1);

	flap_connection_send(conn, frame);

	return 0;
}
#endif

#if 0
/*
 * Send an SMS message.  This is the non-US way.  The US-way is to IM
 * their cell phone number (+19195551234).
 *
 * We basically construct and send an XML message.  The format is:
 * <icq_sms_message>
 *   <destination>full_phone_without_leading_+</destination>
 *   <text>message</text>
 *   <codepage>1252</codepage>
 *   <senders_UIN>self_uin</senders_UIN>
 *   <senders_name>self_name</senders_name>
 *   <delivery_receipt>Yes|No</delivery_receipt>
 *   <time>Wkd, DD Mmm YYYY HH:MM:SS TMZ</time>
 * </icq_sms_message>
 *
 * Yeah hi Peter, whaaaat's happening.  If there's any way to use
 * a codepage other than 1252 that would be great.  Thaaaanks.
 */
int aim_icq_sendsms(OscarData *od, const char *name, const char *msg, const char *alias)
{
	FlapConnection *conn;
	FlapFrame *frame;
	aim_snacid_t snacid;
	int bslen, xmllen;
	char *xml;
	const char *timestr;
	time_t t;
	struct tm *tm;

	if (!od || !(conn = flap_connection_findbygroup(od, 0x0015)))
		return -EINVAL;

	if (!name || !msg || !alias)
		return -EINVAL;

	time(&t);
	tm = gmtime(&t);
	timestr = purple_utf8_strftime("%a, %d %b %Y %T %Z", tm);

	/* The length of xml included the null terminating character */
	xmllen = 225 + strlen(name) + strlen(msg) + strlen(od->sn) + strlen(alias) + strlen(timestr) + 1;

	xml = g_new(char, xmllen);
	snprintf(xml, xmllen, "<icq_sms_message>\n"
		"\t<destination>%s</destination>\n"
		"\t<text>%s</text>\n"
		"\t<codepage>1252</codepage>\n"
		"\t<senders_UIN>%s</senders_UIN>\n"
		"\t<senders_name>%s</senders_name>\n"
		"\t<delivery_receipt>Yes</delivery_receipt>\n"
		"\t<time>%s</time>\n"
		"</icq_sms_message>\n",
		name, msg, od->sn, alias, timestr);

	bslen = 37 + xmllen;

	frame = flap_frame_new(od, 0x02, 10 + 4 + bslen);

	snacid = aim_cachesnac(od, 0x0015, 0x0002, 0x0000, NULL, 0);
	aim_putsnac(&frame->data, 0x0015, 0x0002, 0x0000, snacid);

	/* For simplicity, don't bother using a tlvlist */
	byte_stream_put16(&frame->data, 0x0001);
	byte_stream_put16(&frame->data, bslen);

	byte_stream_putle16(&frame->data, bslen - 2);
	byte_stream_putle32(&frame->data, atoi(od->sn));
	byte_stream_putle16(&frame->data, 0x07d0); /* I command thee. */
	byte_stream_putle16(&frame->data, snacid); /* eh. */

	/* From libicq200-0.3.2/src/SNAC-SRV.cpp */
	byte_stream_putle16(&frame->data, 0x8214);
	byte_stream_put16(&frame->data, 0x0001);
	byte_stream_put16(&frame->data, 0x0016);
	byte_stream_put32(&frame->data, 0x00000000);
	byte_stream_put32(&frame->data, 0x00000000);
	byte_stream_put32(&frame->data, 0x00000000);
	byte_stream_put32(&frame->data, 0x00000000);

	byte_stream_put16(&frame->data, 0x0000);
	byte_stream_put16(&frame->data, xmllen);
	byte_stream_putstr(&frame->data, xml);

	flap_connection_send(conn, frame);

	free(xml);

	return 0;
}
#endif

static void aim_icq_freeinfo(struct aim_icq_info *info) {
	int i;

	if (!info)
		return;
	free(info->nick);
	free(info->first);
	free(info->last);
	free(info->email);
	free(info->homecity);
	free(info->homestate);
	free(info->homephone);
	free(info->homefax);
	free(info->homeaddr);
	free(info->mobile);
	free(info->homezip);
	free(info->personalwebpage);
	if (info->email2)
		for (i = 0; i < info->numaddresses; i++)
			free(info->email2[i]);
	free(info->email2);
	free(info->workcity);
	free(info->workstate);
	free(info->workphone);
	free(info->workfax);
	free(info->workaddr);
	free(info->workzip);
	free(info->workcompany);
	free(info->workdivision);
	free(info->workposition);
	free(info->workwebpage);
	free(info->info);
	free(info);
}

/**
 * Subtype 0x0003 - Response to 0x0015/0x002, contains an ICQesque packet.
 */
static int
icqresponse(OscarData *od, FlapConnection *conn, aim_module_t *mod, FlapFrame *frame, aim_modsnac_t *snac, ByteStream *bs)
{
	int ret = 0;
	aim_tlvlist_t *tl;
	aim_tlv_t *datatlv;
	ByteStream qbs;
	guint32 ouruin;
	guint16 cmdlen, cmd, reqid;

	if (!(tl = aim_tlvlist_read(bs)) || !(datatlv = aim_tlv_gettlv(tl, 0x0001, 1))) {
		aim_tlvlist_free(&tl);
		purple_debug_misc("oscar", "corrupt ICQ response\n");
		return 0;
	}

	byte_stream_init(&qbs, datatlv->value, datatlv->length);

	cmdlen = byte_stream_getle16(&qbs);
	ouruin = byte_stream_getle32(&qbs);
	cmd = byte_stream_getle16(&qbs);
	reqid = byte_stream_getle16(&qbs);

	purple_debug_misc("oscar", "icq response: %d bytes, %ld, 0x%04x, 0x%04x\n", cmdlen, ouruin, cmd, reqid);

	if (cmd == 0x0041) { /* offline message */
		struct aim_icq_offlinemsg msg;
		aim_rxcallback_t userfunc;

		memset(&msg, 0, sizeof(msg));

		msg.sender = byte_stream_getle32(&qbs);
		msg.year = byte_stream_getle16(&qbs);
		msg.month = byte_stream_getle8(&qbs);
		msg.day = byte_stream_getle8(&qbs);
		msg.hour = byte_stream_getle8(&qbs);
		msg.minute = byte_stream_getle8(&qbs);
		msg.type = byte_stream_getle8(&qbs);
		msg.flags = byte_stream_getle8(&qbs);
		msg.msglen = byte_stream_getle16(&qbs);
		msg.msg = byte_stream_getstr(&qbs, msg.msglen);

		if ((userfunc = aim_callhandler(od, SNAC_FAMILY_ICQ, SNAC_SUBTYPE_ICQ_OFFLINEMSG)))
			ret = userfunc(od, conn, frame, &msg);

		free(msg.msg);

	} else if (cmd == 0x0042) {
		aim_rxcallback_t userfunc;

		if ((userfunc = aim_callhandler(od, SNAC_FAMILY_ICQ, SNAC_SUBTYPE_ICQ_OFFLINEMSGCOMPLETE)))
			ret = userfunc(od, conn, frame);

	} else if (cmd == 0x07da) { /* information */
		guint16 subtype;
		struct aim_icq_info *info;
		aim_rxcallback_t userfunc;

		subtype = byte_stream_getle16(&qbs);
		byte_stream_advance(&qbs, 1); /* 0x0a */

		/* find other data from the same request */
		for (info = od->icq_info; info && (info->reqid != reqid); info = info->next);
		if (!info) {
			info = (struct aim_icq_info *)calloc(1, sizeof(struct aim_icq_info));
			info->reqid = reqid;
			info->next = od->icq_info;
			od->icq_info = info;
		}

		switch (subtype) {
		case 0x00a0: { /* hide ip status */
			/* nothing */
		} break;

		case 0x00aa: { /* password change status */
			/* nothing */
		} break;

		case 0x00c8: { /* general and "home" information */
			info->nick = byte_stream_getstr(&qbs, byte_stream_getle16(&qbs));
			info->first = byte_stream_getstr(&qbs, byte_stream_getle16(&qbs));
			info->last = byte_stream_getstr(&qbs, byte_stream_getle16(&qbs));
			info->email = byte_stream_getstr(&qbs, byte_stream_getle16(&qbs));
			info->homecity = byte_stream_getstr(&qbs, byte_stream_getle16(&qbs));
			info->homestate = byte_stream_getstr(&qbs, byte_stream_getle16(&qbs));
			info->homephone = byte_stream_getstr(&qbs, byte_stream_getle16(&qbs));
			info->homefax = byte_stream_getstr(&qbs, byte_stream_getle16(&qbs));
			info->homeaddr = byte_stream_getstr(&qbs, byte_stream_getle16(&qbs));
			info->mobile = byte_stream_getstr(&qbs, byte_stream_getle16(&qbs));
			info->homezip = byte_stream_getstr(&qbs, byte_stream_getle16(&qbs));
			info->homecountry = byte_stream_getle16(&qbs);
			/* 0x0a 00 02 00 */
			/* 1 byte timezone? */
			/* 1 byte hide email flag? */
		} break;

		case 0x00dc: { /* personal information */
			info->age = byte_stream_getle8(&qbs);
			info->unknown = byte_stream_getle8(&qbs);
			info->gender = byte_stream_getle8(&qbs); /* Not specified=0x00, Female=0x01, Male=0x02 */
			info->personalwebpage = byte_stream_getstr(&qbs, byte_stream_getle16(&qbs));
			info->birthyear = byte_stream_getle16(&qbs);
			info->birthmonth = byte_stream_getle8(&qbs);
			info->birthday = byte_stream_getle8(&qbs);
			info->language1 = byte_stream_getle8(&qbs);
			info->language2 = byte_stream_getle8(&qbs);
			info->language3 = byte_stream_getle8(&qbs);
			/* 0x00 00 01 00 00 01 00 00 00 00 00 */
		} break;

		case 0x00d2: { /* work information */
			info->workcity = byte_stream_getstr(&qbs, byte_stream_getle16(&qbs));
			info->workstate = byte_stream_getstr(&qbs, byte_stream_getle16(&qbs));
			info->workphone = byte_stream_getstr(&qbs, byte_stream_getle16(&qbs));
			info->workfax = byte_stream_getstr(&qbs, byte_stream_getle16(&qbs));
			info->workaddr = byte_stream_getstr(&qbs, byte_stream_getle16(&qbs));
			info->workzip = byte_stream_getstr(&qbs, byte_stream_getle16(&qbs));
			info->workcountry = byte_stream_getle16(&qbs);
			info->workcompany = byte_stream_getstr(&qbs, byte_stream_getle16(&qbs));
			info->workdivision = byte_stream_getstr(&qbs, byte_stream_getle16(&qbs));
			info->workposition = byte_stream_getstr(&qbs, byte_stream_getle16(&qbs));
			byte_stream_advance(&qbs, 2); /* 0x01 00 */
			info->workwebpage = byte_stream_getstr(&qbs, byte_stream_getle16(&qbs));
		} break;

		case 0x00e6: { /* additional personal information */
			info->info = byte_stream_getstr(&qbs, byte_stream_getle16(&qbs)-1);
		} break;

		case 0x00eb: { /* email address(es) */
			int i;
			info->numaddresses = byte_stream_getle16(&qbs);
			info->email2 = (char **)calloc(info->numaddresses, sizeof(char *));
			for (i = 0; i < info->numaddresses; i++) {
				info->email2[i] = byte_stream_getstr(&qbs, byte_stream_getle16(&qbs));
				if (i+1 != info->numaddresses)
					byte_stream_advance(&qbs, 1); /* 0x00 */
			}
		} break;

		case 0x00f0: { /* personal interests */
		} break;

		case 0x00fa: { /* past background and current organizations */
		} break;

		case 0x0104: { /* alias info */
			info->nick = byte_stream_getstr(&qbs, byte_stream_getle16(&qbs));
			info->first = byte_stream_getstr(&qbs, byte_stream_getle16(&qbs));
			info->last = byte_stream_getstr(&qbs, byte_stream_getle16(&qbs));
			byte_stream_advance(&qbs, byte_stream_getle16(&qbs)); /* email address? */
			/* Then 0x00 02 00 */
		} break;

		case 0x010e: { /* unknown */
			/* 0x00 00 */
		} break;

		case 0x019a: { /* simple info */
			byte_stream_advance(&qbs, 2);
			info->uin = byte_stream_getle32(&qbs);
			info->nick = byte_stream_getstr(&qbs, byte_stream_getle16(&qbs));
			info->first = byte_stream_getstr(&qbs, byte_stream_getle16(&qbs));
			info->last = byte_stream_getstr(&qbs, byte_stream_getle16(&qbs));
			info->email = byte_stream_getstr(&qbs, byte_stream_getle16(&qbs));
			/* Then 0x00 02 00 00 00 00 00 */
		} break;
		} /* End switch statement */

		if (!(snac->flags & 0x0001)) {
			if (subtype != 0x0104)
				if ((userfunc = aim_callhandler(od, SNAC_FAMILY_ICQ, SNAC_SUBTYPE_ICQ_INFO)))
					ret = userfunc(od, conn, frame, info);

			if (info->uin && info->nick)
				if ((userfunc = aim_callhandler(od, SNAC_FAMILY_ICQ, SNAC_SUBTYPE_ICQ_ALIAS)))
					ret = userfunc(od, conn, frame, info);

			if (od->icq_info == info) {
				od->icq_info = info->next;
			} else {
				struct aim_icq_info *cur;
				for (cur=od->icq_info; (cur->next && (cur->next!=info)); cur=cur->next);
				if (cur->next)
					cur->next = cur->next->next;
			}
			aim_icq_freeinfo(info);
		}
	}

	aim_tlvlist_free(&tl);

	return ret;
}

static int
snachandler(OscarData *od, FlapConnection *conn, aim_module_t *mod, FlapFrame *frame, aim_modsnac_t *snac, ByteStream *bs)
{
	if (snac->subtype == 0x0003)
		return icqresponse(od, conn, mod, frame, snac, bs);

	return 0;
}

static void
icq_shutdown(OscarData *od, aim_module_t *mod)
{
	struct aim_icq_info *del;

	while (od->icq_info) {
		del = od->icq_info;
		od->icq_info = od->icq_info->next;
		aim_icq_freeinfo(del);
	}

	return;
}

int
icq_modfirst(OscarData *od, aim_module_t *mod)
{
	mod->family = 0x0015;
	mod->version = 0x0001;
	mod->toolid = 0x0110;
	mod->toolversion = 0x047c;
	mod->flags = 0;
	strncpy(mod->name, "icq", sizeof(mod->name));
	mod->snachandler = snachandler;
	mod->shutdown = icq_shutdown;

	return 0;
}
