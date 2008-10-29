/**
 * @file im.c
 *
 * purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include "internal.h"

#include "conversation.h"
#include "debug.h"
#include "internal.h"
#include "notify.h"
#include "server.h"
#include "util.h"

#include "buddy_info.h"
#include "buddy_list.h"
#include "buddy_opt.h"
#include "char_conv.h"
#include "qq_define.h"
#include "im.h"
#include "packet_parse.h"
#include "qq_network.h"
#include "send_file.h"
#include "utils.h"

#define DEFAULT_FONT_NAME_LEN 	  4

enum
{
	QQ_NORMAL_IM_TEXT = 0x000b,
	QQ_NORMAL_IM_FILE_REQUEST_TCP = 0x0001,
	QQ_NORMAL_IM_FILE_APPROVE_TCP = 0x0003,
	QQ_NORMAL_IM_FILE_REJECT_TCP = 0x0005,
	QQ_NORMAL_IM_FILE_REQUEST_UDP = 0x0035,
	QQ_NORMAL_IM_FILE_APPROVE_UDP = 0x0037,
	QQ_NORMAL_IM_FILE_REJECT_UDP = 0x0039,
	QQ_NORMAL_IM_FILE_NOTIFY = 0x003b,
	QQ_NORMAL_IM_FILE_PASV = 0x003f,			/* are you behind a firewall? */
	QQ_NORMAL_IM_FILE_CANCEL = 0x0049,
	QQ_NORMAL_IM_FILE_EX_REQUEST_UDP = 0x81,
	QQ_NORMAL_IM_FILE_EX_REQUEST_ACCEPT = 0x83,
	QQ_NORMAL_IM_FILE_EX_REQUEST_CANCEL = 0x85,
	QQ_NORMAL_IM_FILE_EX_NOTIFY_IP = 0x87
};

typedef struct _qq_im_header qq_im_header;
typedef struct _qq_recv_extended_im_text qq_recv_extended_im_text;

struct _qq_im_header {
	/* this is the common part of normal_text */
	guint16 version_from;
	guint32 uid_from;
	guint32 uid_to;
	guint8 session_md5[QQ_KEY_LENGTH];
	guint16 im_type;
};

#define QQ_SEND_IM_AFTER_MSG_HEADER_LEN 8
#define DEFAULT_FONT_NAME "\0xcb\0xce\0xcc\0xe5"

guint8 *qq_get_send_im_tail(const gchar *font_color,
		const gchar *font_size,
		const gchar *font_name,
		gboolean is_bold, gboolean is_italic, gboolean is_underline, gint tail_len)
{
	gchar *s1;
	unsigned char *rgb;
	gint font_name_len;
	guint8 *send_im_tail;
	const guint8 simsun[] = { 0xcb, 0xce, 0xcc, 0xe5 };

	if (font_name) {
		font_name_len = strlen(font_name);
	} else {
		font_name_len = DEFAULT_FONT_NAME_LEN;
		font_name = (const gchar *) simsun;
	}

	send_im_tail = g_new0(guint8, tail_len);

	g_strlcpy((gchar *) (send_im_tail + QQ_SEND_IM_AFTER_MSG_HEADER_LEN),
			font_name, tail_len - QQ_SEND_IM_AFTER_MSG_HEADER_LEN);
	send_im_tail[tail_len - 1] = (guint8) tail_len;

	send_im_tail[0] = 0x00;
	if (font_size) {
		send_im_tail[1] = (guint8) (atoi(font_size) * 3 + 1);
	} else {
		send_im_tail[1] = 10;
	}
	if (is_bold)
		send_im_tail[1] |= 0x20;
	if (is_italic)
		send_im_tail[1] |= 0x40;
	if (is_underline)
		send_im_tail[1] |= 0x80;

	if (font_color) {
		s1 = g_strndup(font_color + 1, 6);
		/* Henry: maybe this is a bug of purple, the string should have
		 * the length of odd number @_@
		 * George Ang: This BUG maybe fixed by Purple. adding new byte
		 * would cause a crash.
		 */
		/* s2 = g_strdup_printf("%sH", s1); */
		rgb = purple_base16_decode(s1, NULL);
		g_free(s1);
		/* g_free(s2); */
		if (rgb)
		{
			memcpy(send_im_tail + 2, rgb, 3);
			g_free(rgb);
		} else {
			send_im_tail[2] = send_im_tail[3] = send_im_tail[4] = 0;
		}
	} else {
		send_im_tail[2] = send_im_tail[3] = send_im_tail[4] = 0;
	}

	send_im_tail[5] = 0x00;
	send_im_tail[6] = 0x86;
	send_im_tail[7] = 0x22;	/* encoding, 0x8622=GB, 0x0000=EN, define BIG5 support here */
	/* qq_show_packet("QQ_MESG", send_im_tail, tail_len); */
	return (guint8 *) send_im_tail;
}

/* read the common parts of the normal_im,
 * returns the bytes read if succeed, or -1 if there is any error */
static gint get_im_header(qq_im_header *im_header, guint8 *data, gint len)
{
	gint bytes;
	g_return_val_if_fail(data != NULL && len > 0, -1);

	bytes = 0;
	bytes += qq_get16(&(im_header->version_from), data + bytes);
	bytes += qq_get32(&(im_header->uid_from), data + bytes);
	bytes += qq_get32(&(im_header->uid_to), data + bytes);
	bytes += qq_getdata(im_header->session_md5, QQ_KEY_LENGTH, data + bytes);
	bytes += qq_get16(&(im_header->im_type), data + bytes);
	return bytes;
}

void qq_got_attention(PurpleConnection *gc, const gchar *msg)
{
	qq_data *qd;
	gchar *from;
	time_t now = time(NULL);

	g_return_if_fail(gc != NULL  && gc->proto_data != NULL);
	qd = gc->proto_data;

	g_return_if_fail(qd->uid > 0);

	qq_buddy_find_or_new(gc, qd->uid);

	from = uid_to_purple_name(qd->uid);
	serv_got_im(gc, from, msg, PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_NOTIFY, now);
	g_free(from);
}

/* process received normal text IM */
static void process_im_text(PurpleConnection *gc, guint8 *data, gint len, qq_im_header *im_header)
{
	guint16 purple_msg_type;
	gchar *who;
	gchar *msg_with_purple_smiley;
	gchar *msg_utf8_encoded;
	qq_data *qd;
	gint bytes = 0;
	PurpleBuddy *b;
	qq_buddy_data *bd;

	struct {
		/* now comes the part for text only */
		guint16 msg_seq;
		guint32 send_time;
		guint16 sender_icon;
		guint8 unknown2[3];
		guint8 is_there_font_attr;
		guint8 unknown3[4];
		guint8 msg_type;
		gchar *msg;		/* no fixed length, ends with 0x00 */
		guint8 *font_attr;
		gint font_attr_len;
	} im_text;

	g_return_if_fail (data != NULL && len > 0);
	g_return_if_fail(im_header != NULL);

	qd = (qq_data *) gc->proto_data;
	memset(&im_text, 0, sizeof(im_text));

	/* push data into im_text */
	bytes += qq_get16(&(im_text.msg_seq), data + bytes);
	bytes += qq_get32(&(im_text.send_time), data + bytes);
	bytes += qq_get16(&(im_text.sender_icon), data + bytes);
	bytes += qq_getdata((guint8 *) & (im_text.unknown2), 3, data + bytes);
	bytes += qq_get8(&(im_text.is_there_font_attr), data + bytes);
	/**
	 * from lumaqq	for unknown3
	 *	totalFragments = buf.get() & 255;
	 *	fragmentSequence = buf.get() & 255;
	 *	messageId = buf.getChar();
	 */
	bytes += qq_getdata((guint8 *) & (im_text.unknown3), 4, data + bytes);
	bytes += qq_get8(&(im_text.msg_type), data + bytes);

	/* we need to check if this is auto-reply
	 * QQ2003iii build 0304, returns the msg without font_attr
	 * even the is_there_font_attr shows 0x01, and msg does not ends with 0x00 */
	if (im_text.msg_type == QQ_IM_AUTO_REPLY) {
		im_text.is_there_font_attr = 0x00;	/* indeed there is no this flag */
		im_text.msg = g_strndup((gchar *)(data + bytes), len - bytes);
	} else {		/* it is normal mesasge */
		if (im_text.is_there_font_attr) {
			im_text.msg = g_strdup((gchar *)(data + bytes));
			bytes += strlen(im_text.msg) + 1; /* length decided by strlen! will it cause a crash? */
			im_text.font_attr_len = len - bytes;
			im_text.font_attr = g_memdup(data + bytes, im_text.font_attr_len);
		} else		/* not im_text.is_there_font_attr */
			im_text.msg = g_strndup((gchar *)(data + bytes), len - bytes);
	}			/* if im_text.msg_type */

	who = uid_to_purple_name(im_header->uid_from);
	b = purple_find_buddy(gc->account, who);
	if (b == NULL) {
		/* create no-auth buddy */
		b = qq_buddy_new(gc, im_header->uid_from);
	}
	bd = (b == NULL) ? NULL : (qq_buddy_data *) b->proto_data;
	if (bd != NULL) {
		bd->client_tag = im_header->version_from;
	}

	purple_msg_type = (im_text.msg_type == QQ_IM_AUTO_REPLY) ? PURPLE_MESSAGE_AUTO_RESP : 0;

	msg_with_purple_smiley = qq_smiley_to_purple(im_text.msg);
	msg_utf8_encoded = im_text.is_there_font_attr ?
		qq_encode_to_purple(im_text.font_attr,
				im_text.font_attr_len,
				msg_with_purple_smiley, qd->client_version)
		: qq_to_utf8(msg_with_purple_smiley, QQ_CHARSET_DEFAULT);

	/* send encoded to purple, note that we use im_text.send_time,
	 * not the time we receive the message
	 * as it may have been delayed when I am not online. */
	serv_got_im(gc, who, msg_utf8_encoded, purple_msg_type, (time_t) im_text.send_time);

	g_free(msg_utf8_encoded);
	g_free(msg_with_purple_smiley);
	g_free(who);
	g_free(im_text.msg);
	if (im_text.font_attr)	g_free(im_text.font_attr);
}

/* process received extended (2007) text IM */
static void process_extend_im_text(
		PurpleConnection *gc, guint8 *data, gint len, qq_im_header *im_header)
{
	guint16 purple_msg_type;
	gchar *who;
	gchar *msg_with_purple_smiley;
	gchar *msg_utf8_encoded;
	qq_data *qd;
	PurpleBuddy *b;
	qq_buddy_data *bd;
	gint bytes, text_len;

	struct {
		/* now comes the part for text only */
		guint16 sessionId;
		guint32 send_time;
		guint16 senderHead;
		guint32 flag;
		guint8 unknown2[8];
		guint8 fragmentCount;
		guint8 fragmentIndex;
		guint16 messageId;
		guint8 replyType;
		gchar *msg;		/* no fixed length, ends with 0x00 */
		guint8 fromMobileQQ;

		guint8 is_there_font_attr;
		guint8 *font_attr;
		gint8 font_attr_len;
	} im_text;

	g_return_if_fail (data != NULL && len > 0);
	g_return_if_fail(im_header != NULL);

	qd = (qq_data *) gc->proto_data;
	memset(&im_text, 0, sizeof(im_text));

	/* push data into im_text */
	bytes = 0;
	bytes += qq_get16(&(im_text.sessionId), data + bytes);
	bytes += qq_get32(&(im_text.send_time), data + bytes);
	bytes += qq_get16(&(im_text.senderHead), data + bytes);
	bytes += qq_get32(&(im_text.flag), data + bytes);

	bytes += qq_getdata(im_text.unknown2, 8, data + bytes);
	bytes += qq_get8(&(im_text.fragmentCount), data + bytes);
	bytes += qq_get8(&(im_text.fragmentIndex), data + bytes);

	bytes += qq_get16(&(im_text.messageId), data + bytes);
	bytes += qq_get8(&(im_text.replyType), data + bytes);

	im_text.font_attr_len = data[len-1] & 0xff;

	text_len = len - bytes - im_text.font_attr_len;
	im_text.msg = g_strndup((gchar *)(data + bytes), text_len);
	bytes += text_len;
	if(im_text.font_attr_len >= 0)
		im_text.font_attr = g_memdup(data + bytes, im_text.font_attr_len);
	else
	{
		purple_debug_error("QQ", "Failed to get IM's font attribute len %d\n",
			im_text.font_attr_len);
		return;
	}

	if(im_text.fragmentCount == 0)
		im_text.fragmentCount = 1;

	// Filter tail space
	if(im_text.fragmentIndex == im_text.fragmentCount -1)
	{
		gint real_len = text_len;
		while(real_len > 0 && im_text.msg[real_len - 1] == 0x20)
			real_len --;

		text_len = real_len;
		// Null string instaed of space
		im_text.msg[text_len] = 0;
	}

	who = uid_to_purple_name(im_header->uid_from);
	b = purple_find_buddy(gc->account, who);
	if (b == NULL) {
		/* create no-auth buddy */
		b = qq_buddy_new(gc, im_header->uid_from);
	}
	bd = (b == NULL) ? NULL : (qq_buddy_data *) b->proto_data;
	if (bd != NULL) {
		bd->client_tag = im_header->version_from;
	}

	purple_msg_type = 0;

	msg_with_purple_smiley = qq_smiley_to_purple(im_text.msg);
	msg_utf8_encoded = im_text.font_attr ?
	    qq_encode_to_purple(im_text.font_attr,
			      im_text.font_attr_len,
			      msg_with_purple_smiley, qd->client_version)
		: qq_to_utf8(msg_with_purple_smiley, QQ_CHARSET_DEFAULT);

	/* send encoded to purple, note that we use im_text.send_time,
	 * not the time we receive the message
	 * as it may have been delayed when I am not online. */
	serv_got_im(gc, who, msg_utf8_encoded, purple_msg_type, (time_t) im_text.send_time);

	g_free(msg_utf8_encoded);
	g_free(msg_with_purple_smiley);
	g_free(who);
	g_free(im_text.msg);
	if (im_text.font_attr) g_free(im_text.font_attr);
}

/* it is a normal IM, maybe text or video request */
void qq_process_im(PurpleConnection *gc, guint8 *data, gint len)
{
	gint bytes = 0;
	qq_im_header im_header;

	g_return_if_fail (data != NULL && len > 0);

	bytes = get_im_header(&im_header, data, len);
	if (bytes < 0) {
		purple_debug_error("QQ", "Fail read im header, len %d\n", len);
		qq_show_packet ("IM Header", data, len);
		return;
	}
	purple_debug_info("QQ",
			"Got IM to %d, type: %02X from: %d ver: %s (%04X)\n",
			im_header.uid_to, im_header.im_type, im_header.uid_from,
			qq_get_ver_desc(im_header.version_from), im_header.version_from);

	switch (im_header.im_type) {
		case QQ_NORMAL_IM_TEXT:
			if (bytes >= len - 1) {
				purple_debug_warning("QQ", "Received normal IM text is empty\n");
				return;
			}
			process_im_text(gc, data + bytes, len - bytes, &im_header);
			break;
		case QQ_NORMAL_IM_FILE_REJECT_UDP:
			qq_process_recv_file_reject(data + bytes, len - bytes, im_header.uid_from, gc);
			break;
		case QQ_NORMAL_IM_FILE_APPROVE_UDP:
			qq_process_recv_file_accept(data + bytes, len - bytes, im_header.uid_from, gc);
			break;
		case QQ_NORMAL_IM_FILE_REQUEST_UDP:
			qq_process_recv_file_request(data + bytes, len - bytes, im_header.uid_from, gc);
			break;
		case QQ_NORMAL_IM_FILE_CANCEL:
			qq_process_recv_file_cancel(data + bytes, len - bytes, im_header.uid_from, gc);
			break;
		case QQ_NORMAL_IM_FILE_NOTIFY:
			qq_process_recv_file_notify(data + bytes, len - bytes, im_header.uid_from, gc);
			break;
		case QQ_NORMAL_IM_FILE_REQUEST_TCP:
			/* Check ReceivedFileIM::parseContents in eva*/
			/* some client use this function for detect invisable buddy*/
		case QQ_NORMAL_IM_FILE_APPROVE_TCP:
		case QQ_NORMAL_IM_FILE_REJECT_TCP:
		case QQ_NORMAL_IM_FILE_PASV:
		case QQ_NORMAL_IM_FILE_EX_REQUEST_UDP:
		case QQ_NORMAL_IM_FILE_EX_REQUEST_ACCEPT:
		case QQ_NORMAL_IM_FILE_EX_REQUEST_CANCEL:
		case QQ_NORMAL_IM_FILE_EX_NOTIFY_IP:
			qq_show_packet ("Not support", data, len);
			break;
		default:
			/* a simple process here, maybe more later */
			qq_show_packet ("Unknow", data + bytes, len - bytes);
			return;
	}
}

/* it is a extended IM, maybe text or video request */
void qq_process_extend_im(PurpleConnection *gc, guint8 *data, gint len)
{
	gint bytes;
	qq_im_header im_header;

	g_return_if_fail (data != NULL && len > 0);

	bytes = get_im_header(&im_header, data, len);
	if (bytes < 0) {
		purple_debug_error("QQ", "Fail read im header, len %d\n", len);
		qq_show_packet ("IM Header", data, len);
		return;
	}
	purple_debug_info("QQ",
			"Got Extend IM to %d, type: %02X from: %d ver: %s (%04X)\n",
			im_header.uid_to, im_header.im_type, im_header.uid_from,
			qq_get_ver_desc(im_header.version_from), im_header.version_from);

	switch (im_header.im_type) {
	case QQ_NORMAL_IM_TEXT:
		process_extend_im_text(gc, data + bytes, len - bytes, &im_header);
		break;
	case QQ_NORMAL_IM_FILE_REJECT_UDP:
		qq_process_recv_file_reject (data + bytes, len - bytes, im_header.uid_from, gc);
		break;
	case QQ_NORMAL_IM_FILE_APPROVE_UDP:
		qq_process_recv_file_accept (data + bytes, len - bytes, im_header.uid_from, gc);
		break;
	case QQ_NORMAL_IM_FILE_REQUEST_UDP:
		qq_process_recv_file_request (data + bytes, len - bytes, im_header.uid_from, gc);
		break;
	case QQ_NORMAL_IM_FILE_CANCEL:
		qq_process_recv_file_cancel (data + bytes, len - bytes, im_header.uid_from, gc);
		break;
	case QQ_NORMAL_IM_FILE_NOTIFY:
		qq_process_recv_file_notify (data + bytes, len - bytes, im_header.uid_from, gc);
		break;
	default:
		/* a simple process here, maybe more later */
		qq_show_packet ("Unknow", data + bytes, len - bytes);
		break;
	}
}

/* send an IM to uid_to */
void qq_request_send_im(PurpleConnection *gc, guint32 uid_to, gchar *msg, gint type)
{
	qq_data *qd;
	guint8 *raw_data, *send_im_tail;
	guint16 im_type;
	gint msg_len, raw_len, font_name_len, tail_len, bytes;
	time_t now;
	gchar *msg_filtered;
	GData *attribs;
	gchar *font_size = NULL, *font_color = NULL, *font_name = NULL, *tmp;
	gboolean is_bold = FALSE, is_italic = FALSE, is_underline = FALSE;
	const gchar *start, *end, *last;

	qd = (qq_data *) gc->proto_data;
	im_type = QQ_NORMAL_IM_TEXT;

	last = msg;
	while (purple_markup_find_tag("font", last, &start, &end, &attribs)) {
		tmp = g_datalist_get_data(&attribs, "size");
		if (tmp) {
			if (font_size)
				g_free(font_size);
			font_size = g_strdup(tmp);
		}
		tmp = g_datalist_get_data(&attribs, "color");
		if (tmp) {
			if (font_color)
				g_free(font_color);
			font_color = g_strdup(tmp);
		}
		tmp = g_datalist_get_data(&attribs, "face");
		if (tmp) {
			if (font_name)
				g_free(font_name);
			font_name = g_strdup(tmp);
		}

		g_datalist_clear(&attribs);
		last = end + 1;
	}

	if (purple_markup_find_tag("b", msg, &start, &end, &attribs)) {
		is_bold = TRUE;
		g_datalist_clear(&attribs);
	}

	if (purple_markup_find_tag("i", msg, &start, &end, &attribs)) {
		is_italic = TRUE;
		g_datalist_clear(&attribs);
	}

	if (purple_markup_find_tag("u", msg, &start, &end, &attribs)) {
		is_underline = TRUE;
		g_datalist_clear(&attribs);
	}

	purple_debug_info("QQ_MESG", "send mesg: %s\n", msg);
	msg_filtered = purple_markup_strip_html(msg);
	msg_len = strlen(msg_filtered);
	now = time(NULL);

	font_name_len = (font_name) ? strlen(font_name) : DEFAULT_FONT_NAME_LEN;
	tail_len = font_name_len + QQ_SEND_IM_AFTER_MSG_HEADER_LEN + 1;

	raw_len = QQ_SEND_IM_BEFORE_MSG_LEN + msg_len + tail_len;
	raw_data = g_newa(guint8, raw_len);
	bytes = 0;

	/* 000-003: receiver uid */
	bytes += qq_put32(raw_data + bytes, qd->uid);
	/* 004-007: sender uid */
	bytes += qq_put32(raw_data + bytes, uid_to);
	/* 008-009: sender client version */
	bytes += qq_put16(raw_data + bytes, qd->client_tag);
	/* 010-013: receiver uid */
	bytes += qq_put32(raw_data + bytes, qd->uid);
	/* 014-017: sender uid */
	bytes += qq_put32(raw_data + bytes, uid_to);
	/* 018-033: md5 of (uid+session_key) */
	bytes += qq_putdata(raw_data + bytes, qd->session_md5, 16);
	/* 034-035: message type */
	bytes += qq_put16(raw_data + bytes, im_type);
	/* 036-037: sequence number */
	bytes += qq_put16(raw_data + bytes, qd->send_seq);
	/* 038-041: send time */
	bytes += qq_put32(raw_data + bytes, (guint32) now);
	/* 042-043: sender icon */
	bytes += qq_put16(raw_data + bytes, qd->my_icon);
	/* 044-046: always 0x00 */
	bytes += qq_put16(raw_data + bytes, 0x0000);
	bytes += qq_put8(raw_data + bytes, 0x00);
	/* 047-047: we use font attr */
	bytes += qq_put8(raw_data + bytes, 0x01);
	/* 048-051: always 0x00 */
	bytes += qq_put32(raw_data + bytes, 0x00000000);
	/* 052-052: text message type (normal/auto-reply) */
	bytes += qq_put8(raw_data + bytes, type);
	/* 053-   : msg ends with 0x00 */
	bytes += qq_putdata(raw_data + bytes, (guint8 *) msg_filtered, msg_len);
	send_im_tail = qq_get_send_im_tail(font_color, font_size, font_name, is_bold,
			is_italic, is_underline, tail_len);
	/* qq_show_packet("qq_get_send_im_tail", send_im_tail, tail_len); */
	bytes += qq_putdata(raw_data + bytes, send_im_tail, tail_len);

	/* qq_show_packet("QQ_CMD_SEND_IM, raw_data, bytes); */

	if (bytes == raw_len)	/* create packet OK */
		qq_send_cmd(gc, QQ_CMD_SEND_IM, raw_data, bytes);
	else
		purple_debug_error("QQ",
				"Fail creating send_im packet, expect %d bytes, build %d bytes\n", raw_len, bytes);

	if (font_color)
		g_free(font_color);
	if (font_size)
		g_free(font_size);
	g_free(send_im_tail);
	g_free(msg_filtered);
}



