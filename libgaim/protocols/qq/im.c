/**
 * The QQ2003C protocol plugin
 *
 * for gaim
 *
 * Copyright (C) 2004 Puzzlebird
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

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
#include "crypt.h"
#include "group_im.h"
#include "header_info.h"
#include "im.h"
#include "packet_parse.h"
#include "send_core.h"
#include "send_file.h"
#include "utils.h"

#define QQ_SEND_IM_REPLY_OK       0x00
#define DEFAULT_FONT_NAME_LEN 	  4

/* a debug function */
void _qq_show_packet(const gchar *desc, const guint8 *buf, gint len);

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

enum {
	QQ_RECV_SYS_IM_KICK_OUT = 0x01
};

typedef struct _qq_recv_im_header qq_recv_im_header;
typedef struct _qq_recv_normal_im_text qq_recv_normal_im_text;
typedef struct _qq_recv_normal_im_common qq_recv_normal_im_common;
typedef struct _qq_recv_normal_im_unprocessed qq_recv_normal_im_unprocessed;

struct _qq_recv_normal_im_common {
	/* this is the common part of normal_text */
	guint16 sender_ver;
	guint32 sender_uid;
	guint32 receiver_uid;
	guint8 *session_md5;
	guint16 normal_im_type;
};

struct _qq_recv_normal_im_text {
	qq_recv_normal_im_common *common;
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
};

struct _qq_recv_normal_im_unprocessed {
	qq_recv_normal_im_common *common;
	/* now comes the part of unprocessed */
	guint8 *unknown;	/* no fixed length */
	gint length;
};

struct _qq_recv_im_header {
	guint32 sender_uid;
	guint32 receiver_uid;
	guint32 server_im_seq;
	guint8 sender_ip[4];
	guint16 sender_port;
	guint16 im_type;
};

#define QQ_SEND_IM_AFTER_MSG_HEADER_LEN 8
#define DEFAULT_FONT_NAME "\0xcb\0xce\0xcc\0xe5"

guint8 *qq_get_send_im_tail(const gchar *font_color,
			    const gchar *font_size,
			    const gchar *font_name,
			    gboolean is_bold, gboolean is_italic, gboolean is_underline, gint tail_len)
{
	gchar *s1, *s2;
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
		/* Henry: maybe this is a bug of gaim, the string should have
		 * the length of odd number @_@
		 */
		s2 = g_strdup_printf("%sH", s1);
		rgb = gaim_base16_decode(s2, NULL);
		g_free(s1);
		g_free(s2);
		memcpy(send_im_tail + 2, rgb, 3);
		g_free(rgb);
	} else {
		send_im_tail[2] = send_im_tail[3] = send_im_tail[4] = 0;
	}

	send_im_tail[5] = 0x00;
	send_im_tail[6] = 0x86;
	send_im_tail[7] = 0x22;	/* encoding, 0x8622=GB, 0x0000=EN, define BIG5 support here */
	_qq_show_packet("QQ_MESG", send_im_tail, tail_len);
	return (guint8 *) send_im_tail;
}

static const gchar *qq_get_recv_im_type_str(gint type)
{
	switch (type) {
	case QQ_RECV_IM_TO_BUDDY:
		return "QQ_RECV_IM_TO_BUDDY";
	case QQ_RECV_IM_TO_UNKNOWN:
		return "QQ_RECV_IM_TO_UNKNOWN";
	case QQ_RECV_IM_UNKNOWN_QUN_IM:
		return "QQ_RECV_IM_UNKNOWN_QUN_IM";
	case QQ_RECV_IM_ADD_TO_QUN:
		return "QQ_RECV_IM_ADD_TO_QUN";
	case QQ_RECV_IM_DEL_FROM_QUN:
		return "QQ_RECV_IM_DEL_FROM_QUN";
	case QQ_RECV_IM_APPLY_ADD_TO_QUN:
		return "QQ_RECV_IM_APPLY_ADD_TO_QUN";
	case QQ_RECV_IM_CREATE_QUN:
		return "QQ_RECV_IM_CREATE_QUN";
	case QQ_RECV_IM_SYS_NOTIFICATION:
		return "QQ_RECV_IM_SYS_NOTIFICATION";
	case QQ_RECV_IM_APPROVE_APPLY_ADD_TO_QUN:
		return "QQ_RECV_IM_APPROVE_APPLY_ADD_TO_QUN";
	case QQ_RECV_IM_REJCT_APPLY_ADD_TO_QUN:
		return "QQ_RECV_IM_REJCT_APPLY_ADD_TO_QUN";
	case QQ_RECV_IM_TEMP_QUN_IM:
		return "QQ_RECV_IM_TEMP_QUN_IM";
	case QQ_RECV_IM_QUN_IM:
		return "QQ_RECV_IM_QUN_IM";
	default:
		return "QQ_RECV_IM_UNKNOWN";
	}
}

/* when we receive a message,
 * we send an ACK which is the first 16 bytes of incoming packet */
static void _qq_send_packet_recv_im_ack(GaimConnection *gc, guint16 seq, guint8 *data)
{
	qq_send_cmd(gc, QQ_CMD_RECV_IM, FALSE, seq, FALSE, data, 16);
}

/* read the common parts of the normal_im,
 * returns the bytes read if succeed, or -1 if there is any error */
static gint _qq_normal_im_common_read(guint8 *data, guint8 **cursor, gint len, qq_recv_normal_im_common *common)
{
	gint bytes;
	g_return_val_if_fail(data != NULL && len != 0 && common != NULL, -1);

	bytes = 0;
	/* now push data into common header */
	bytes += read_packet_w(data, cursor, len, &(common->sender_ver));
	bytes += read_packet_dw(data, cursor, len, &(common->sender_uid));
	bytes += read_packet_dw(data, cursor, len, &(common->receiver_uid));

	common->session_md5 = g_memdup(*cursor, QQ_KEY_LENGTH);
	bytes += QQ_KEY_LENGTH;
	*cursor += QQ_KEY_LENGTH;

	bytes += read_packet_w(data, cursor, len, &(common->normal_im_type));

	if (bytes != 28) {	/* read common place fail */
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Expect 28 bytes, read %d bytes\n", bytes);
		return -1;
	}

	return bytes;
}

/* process received normal text IM */
static void _qq_process_recv_normal_im_text
    (guint8 *data, guint8 **cursor, gint len, qq_recv_normal_im_common *common, GaimConnection *gc)
{
	guint16 gaim_msg_type;
	gchar *name;
	gchar *msg_with_gaim_smiley;
	gchar *msg_utf8_encoded;
	qq_data *qd;
	qq_recv_normal_im_text *im_text;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL && common != NULL);
	qd = (qq_data *) gc->proto_data;

	/* now it is QQ_NORMAL_IM_TEXT */
	if (*cursor >= (data + len - 1)) {
		gaim_debug(GAIM_DEBUG_WARNING, "QQ", "Received normal IM text is empty\n");
		return;
	} else
		im_text = g_newa(qq_recv_normal_im_text, 1);

	im_text->common = common;

	/* push data into im_text */
	read_packet_w(data, cursor, len, &(im_text->msg_seq));
	read_packet_dw(data, cursor, len, &(im_text->send_time));
	read_packet_w(data, cursor, len, &(im_text->sender_icon));
	read_packet_data(data, cursor, len, (guint8 *) & (im_text->unknown2), 3);
	read_packet_b(data, cursor, len, &(im_text->is_there_font_attr));
	/**
	 * from lumaqq	for unknown3
	 *	totalFragments = buf.get() & 255;
         *	fragmentSequence = buf.get() & 255;
         *	messageId = buf.getChar();
	 */
	read_packet_data(data, cursor, len, (guint8 *) & (im_text->unknown3), 4);
	read_packet_b(data, cursor, len, &(im_text->msg_type));

	/* we need to check if this is auto-reply
	 * QQ2003iii build 0304, returns the msg without font_attr
	 * even the is_there_font_attr shows 0x01, and msg does not ends with 0x00 */
	if (im_text->msg_type == QQ_IM_AUTO_REPLY) {
		im_text->is_there_font_attr = 0x00;	/* indeed there is no this flag */
		im_text->msg = g_strndup(*(gchar **) cursor, data + len - *cursor);
	} else {		/* it is normal mesasge */
		if (im_text->is_there_font_attr) {
			im_text->msg = g_strdup(*(gchar **) cursor);
			*cursor += strlen(im_text->msg) + 1;
			im_text->font_attr_len = data + len - *cursor;
			im_text->font_attr = g_memdup(*cursor, im_text->font_attr_len);
		} else		/* not im_text->is_there_font_attr */
			im_text->msg = g_strndup(*(gchar **) cursor, data + len - *cursor);
	}			/* if im_text->msg_type */
	_qq_show_packet("QQ_MESG recv", data, *cursor - data);

	name = uid_to_gaim_name(common->sender_uid);
	if (gaim_find_buddy(gc->account, name) == NULL)
		qq_add_buddy_by_recv_packet(gc, common->sender_uid, FALSE, TRUE);

	gaim_msg_type = (im_text->msg_type == QQ_IM_AUTO_REPLY) ? GAIM_MESSAGE_AUTO_RESP : 0;

	msg_with_gaim_smiley = qq_smiley_to_gaim(im_text->msg);
	msg_utf8_encoded = im_text->is_there_font_attr ?
	    qq_encode_to_gaim(im_text->font_attr,
			      im_text->font_attr_len,
			      msg_with_gaim_smiley) : qq_to_utf8(msg_with_gaim_smiley, QQ_CHARSET_DEFAULT);

	/* send encoded to gaim, note that we use im_text->send_time,
	 * not the time we receive the message
	 * as it may have been delayed when I am not online. */
	serv_got_im(gc, name, msg_utf8_encoded, gaim_msg_type, (time_t) im_text->send_time);

	g_free(msg_utf8_encoded);
	g_free(msg_with_gaim_smiley);
	g_free(name);
	g_free(im_text->msg);
	if (im_text->is_there_font_attr)
		g_free(im_text->font_attr);
}

/* it is a normal IM, maybe text or video request */
static void _qq_process_recv_normal_im(guint8 *data, guint8 **cursor, gint len, GaimConnection *gc)
{
	gint bytes;
	qq_recv_normal_im_common *common;
	qq_recv_normal_im_unprocessed *im_unprocessed;

	g_return_if_fail (data != NULL && len != 0);

	if (*cursor >= (data + len - 1)) {
		gaim_debug (GAIM_DEBUG_WARNING, "QQ",
			    "Received normal IM is empty\n");
		return;
	}
	else
		common = g_newa (qq_recv_normal_im_common, 1);

	bytes = _qq_normal_im_common_read (data, cursor, len, common);
	if (bytes < 0) {
		gaim_debug (GAIM_DEBUG_ERROR, "QQ",
			    "Fail read the common part of normal IM\n");
		return;
	}

	switch (common->normal_im_type) {
	case QQ_NORMAL_IM_TEXT:
		gaim_debug (GAIM_DEBUG_INFO,
			    "QQ",
			    "Normal IM, text type:\n [%d] => [%d], src: %s\n",
			    common->sender_uid, common->receiver_uid,
			    qq_get_source_str (common->sender_ver));
		_qq_process_recv_normal_im_text (data, cursor, len, common,
						 gc);
		break;
	case QQ_NORMAL_IM_FILE_REJECT_UDP:
		qq_process_recv_file_reject (data, cursor, len,
					     common->sender_uid, gc);
		break;
	case QQ_NORMAL_IM_FILE_APPROVE_UDP:
		qq_process_recv_file_accept (data, cursor, len,
					     common->sender_uid, gc);
		break;
	case QQ_NORMAL_IM_FILE_REQUEST_UDP:
		qq_process_recv_file_request (data, cursor, len,
					      common->sender_uid, gc);
		break;
	case QQ_NORMAL_IM_FILE_CANCEL:
		qq_process_recv_file_cancel (data, cursor, len,
					     common->sender_uid, gc);
		break;
	case QQ_NORMAL_IM_FILE_NOTIFY:
		qq_process_recv_file_notify (data, cursor, len,
				common->sender_uid, gc);
		break;
	default:
		im_unprocessed = g_newa (qq_recv_normal_im_unprocessed, 1);
		im_unprocessed->common = common;
		im_unprocessed->unknown = *cursor;
		im_unprocessed->length = data + len - *cursor;
		/* a simple process here, maybe more later */
		gaim_debug (GAIM_DEBUG_WARNING, "QQ",
			    "Normal IM, unprocessed type [0x%04x]\n",
			    common->normal_im_type);
		gaim_debug (GAIM_DEBUG_WARNING, "QQ",
			    "Dump unknown part.\n%s",
			    hex_dump_to_str (im_unprocessed->unknown,
					     im_unprocessed->length));
		g_free (common->session_md5);
		return;
	}

	g_free (common->session_md5);
}

/* process im from system administrator */
static void _qq_process_recv_sys_im(guint8 *data, guint8 **cursor, gint data_len, GaimConnection *gc)
{
	gint len;
	guint8 reply;
	gchar **segments, *msg_utf8;

	g_return_if_fail(gc != NULL && data != NULL && data_len != 0);

	if (*cursor >= (data + data_len - 1)) {
		gaim_debug(GAIM_DEBUG_WARNING, "QQ", "Received sys IM is empty\n");
		return;
	}

	len = data + data_len - *cursor;

	if (NULL == (segments = split_data(*cursor, len, "\x2f", 2)))
		return;

	reply = strtol(segments[0], NULL, 10);
	if (reply == QQ_RECV_SYS_IM_KICK_OUT)
		gaim_debug(GAIM_DEBUG_WARNING, "QQ", "We are kicked out by QQ server\n");
	msg_utf8 = qq_to_utf8(segments[1], QQ_CHARSET_DEFAULT);
	gaim_notify_warning(gc, NULL, _("System Message"), msg_utf8);
}

/* send an IM to to_uid */
void qq_send_packet_im(GaimConnection *gc, guint32 to_uid, gchar *msg, gint type)
{
	qq_data *qd;
	guint8 *cursor, *raw_data, *send_im_tail, *md5;
	guint16 client_tag, normal_im_type;
	gint msg_len, raw_len, font_name_len, tail_len, bytes;
	time_t now;
	gchar *msg_filtered;
	GData *attribs;
	gchar *font_size = NULL, *font_color = NULL, *font_name = NULL, *tmp;
	gboolean is_bold = FALSE, is_italic = FALSE, is_underline = FALSE;
	const gchar *start, *end, *last;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);

	qd = (qq_data *) gc->proto_data;
	client_tag = QQ_CLIENT;
	normal_im_type = QQ_NORMAL_IM_TEXT;

	last = msg;
	while (gaim_markup_find_tag("font", last, &start, &end, &attribs)) {
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

	if (gaim_markup_find_tag("b", msg, &start, &end, &attribs)) {
		is_bold = TRUE;
		g_datalist_clear(&attribs);
	}

	if (gaim_markup_find_tag("i", msg, &start, &end, &attribs)) {
		is_italic = TRUE;
		g_datalist_clear(&attribs);
	}

	if (gaim_markup_find_tag("u", msg, &start, &end, &attribs)) {
		is_underline = TRUE;
		g_datalist_clear(&attribs);
	}

	gaim_debug(GAIM_DEBUG_INFO, "QQ_MESG", "send mesg: %s\n", msg);
	msg_filtered = gaim_markup_strip_html(msg);
	msg_len = strlen(msg_filtered);
	now = time(NULL);
	md5 = _gen_session_md5(qd->uid, qd->session_key);

	font_name_len = (font_name) ? strlen(font_name) : DEFAULT_FONT_NAME_LEN;
	tail_len = font_name_len + QQ_SEND_IM_AFTER_MSG_HEADER_LEN + 1;

	raw_len = QQ_SEND_IM_BEFORE_MSG_LEN + msg_len + tail_len;
	raw_data = g_newa(guint8, raw_len);
	cursor = raw_data;
	bytes = 0;

	/* 000-003: receiver uid */
	bytes += create_packet_dw(raw_data, &cursor, qd->uid);
	/* 004-007: sender uid */
	bytes += create_packet_dw(raw_data, &cursor, to_uid);
	/* 008-009: sender client version */
	bytes += create_packet_w(raw_data, &cursor, client_tag);
	/* 010-013: receiver uid */
	bytes += create_packet_dw(raw_data, &cursor, qd->uid);
	/* 014-017: sender uid */
	bytes += create_packet_dw(raw_data, &cursor, to_uid);
	/* 018-033: md5 of (uid+session_key) */
	bytes += create_packet_data(raw_data, &cursor, md5, 16);
	/* 034-035: message type */
	bytes += create_packet_w(raw_data, &cursor, normal_im_type);
	/* 036-037: sequence number */
	bytes += create_packet_w(raw_data, &cursor, qd->send_seq);
	/* 038-041: send time */
	bytes += create_packet_dw(raw_data, &cursor, (guint32) now);
	/* 042-043: sender icon */
	bytes += create_packet_w(raw_data, &cursor, qd->my_icon);
	/* 044-046: always 0x00 */
	bytes += create_packet_w(raw_data, &cursor, 0x0000);
	bytes += create_packet_b(raw_data, &cursor, 0x00);
	/* 047-047: we use font attr */
	bytes += create_packet_b(raw_data, &cursor, 0x01);
	/* 048-051: always 0x00 */
	bytes += create_packet_dw(raw_data, &cursor, 0x00000000);
	/* 052-052: text message type (normal/auto-reply) */
	bytes += create_packet_b(raw_data, &cursor, type);
	/* 053-   : msg ends with 0x00 */
	bytes += create_packet_data(raw_data, &cursor, (guint8 *) msg_filtered, msg_len);
	send_im_tail = qq_get_send_im_tail(font_color, font_size, font_name, is_bold,
						   is_italic, is_underline, tail_len);
	_qq_show_packet("QQ_MESG debug", send_im_tail, tail_len);
	bytes += create_packet_data(raw_data, &cursor, send_im_tail, tail_len);

	_qq_show_packet("QQ_MESG raw", raw_data, cursor - raw_data);

	if (bytes == raw_len)	/* create packet OK */
		qq_send_cmd(gc, QQ_CMD_SEND_IM, TRUE, 0, TRUE, raw_data, cursor - raw_data);
	else
		gaim_debug(GAIM_DEBUG_ERROR, "QQ",
			   "Fail creating send_im packet, expect %d bytes, build %d bytes\n", raw_len, bytes);

	if (font_color)
		g_free(font_color);
	if (font_size)
		g_free(font_size);
	g_free(send_im_tail);
	g_free(msg_filtered);
}

/* parse the reply to send_im */
void qq_process_send_im_reply(guint8 *buf, gint buf_len, GaimConnection *gc)
{
	qq_data *qd;
	gint len;
	guint8 *data, *cursor, reply;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	g_return_if_fail(buf != NULL && buf_len != 0);

	qd = gc->proto_data;
	len = buf_len;
	data = g_newa(guint8, len);

	if (qq_crypt(DECRYPT, buf, buf_len, qd->session_key, data, &len)) {
		cursor = data;
		read_packet_b(data, &cursor, len, &reply);
		if (reply != QQ_SEND_IM_REPLY_OK) {
			gaim_debug(GAIM_DEBUG_WARNING, "QQ", "Send IM fail\n");
			gaim_notify_error(gc, _("Server ACK"), _("Send IM fail\n"), NULL);
		}
		else
			gaim_debug(GAIM_DEBUG_INFO, "QQ", "IM ACK OK\n");
	} else {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Error decrypt send im reply\n");
	}
}

/* I receive a message, mainly it is text msg,
 * but we need to proess other types (group etc) */
void qq_process_recv_im(guint8 *buf, gint buf_len, guint16 seq, GaimConnection *gc)
{
	qq_data *qd;
	gint len, bytes;
	guint8 *data, *cursor;
	qq_recv_im_header *im_header;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	g_return_if_fail(buf != NULL && buf_len != 0);

	qd = (qq_data *) gc->proto_data;
	len = buf_len;
	data = g_newa(guint8, len);

	if (qq_crypt(DECRYPT, buf, buf_len, qd->session_key, data, &len)) {
		if (len < 16) {	/* we need to ack with the first 16 bytes */
			gaim_debug(GAIM_DEBUG_ERROR, "QQ", "IM is too short\n");
			return;
		} else
			_qq_send_packet_recv_im_ack(gc, seq, data);

		cursor = data;
		bytes = 0;
		im_header = g_newa(qq_recv_im_header, 1);
		bytes += read_packet_dw(data, &cursor, len, &(im_header->sender_uid));
		bytes += read_packet_dw(data, &cursor, len, &(im_header->receiver_uid));
		bytes += read_packet_dw(data, &cursor, len, &(im_header->server_im_seq));
		/* if the message is delivered via server, it is server IP/port */
		bytes += read_packet_data(data, &cursor, len, (guint8 *) & (im_header->sender_ip), 4);
		bytes += read_packet_w(data, &cursor, len, &(im_header->sender_port));
		bytes += read_packet_w(data, &cursor, len, &(im_header->im_type));

		if (bytes != 20) {	/* length of im_header */
			gaim_debug(GAIM_DEBUG_ERROR, "QQ",
				   "Fail read recv IM header, expect 20 bytes, read %d bytes\n", bytes);
			return;
		}

		if (im_header->receiver_uid != qd->uid) {	/* should not happen */
			gaim_debug(GAIM_DEBUG_ERROR, "QQ", "IM to [%d], NOT me\n", im_header->receiver_uid);
			return;
		}

		switch (im_header->im_type) {
		case QQ_RECV_IM_TO_BUDDY:
			gaim_debug(GAIM_DEBUG_INFO, "QQ",
				   "IM from buddy [%d], I am in his/her buddy list\n", im_header->sender_uid);
			_qq_process_recv_normal_im(data, &cursor, len, gc);
			break;
		case QQ_RECV_IM_TO_UNKNOWN:
			gaim_debug(GAIM_DEBUG_INFO, "QQ",
				   "IM from buddy [%d], I am a stranger to him/her\n", im_header->sender_uid);
			_qq_process_recv_normal_im(data, &cursor, len, gc);
			break;
		case QQ_RECV_IM_UNKNOWN_QUN_IM:
		case QQ_RECV_IM_TEMP_QUN_IM:
		case QQ_RECV_IM_QUN_IM:
			gaim_debug(GAIM_DEBUG_INFO, "QQ", "IM from group, internal_id [%d]\n", im_header->sender_uid);
			/* sender_uid is in fact internal_group_id */
			qq_process_recv_group_im(data, &cursor, len, im_header->sender_uid, gc, im_header->im_type);
			break;
		case QQ_RECV_IM_ADD_TO_QUN:
			gaim_debug(GAIM_DEBUG_INFO, "QQ",
				   "IM from group, added by group internal_id [%d]\n", im_header->sender_uid);
			/* sender_uid is in fact internal_group_id
			 * we need this to create a dummy group and add to blist */
			qq_process_recv_group_im_been_added(data, &cursor, len, im_header->sender_uid, gc);
			break;
		case QQ_RECV_IM_DEL_FROM_QUN:
			gaim_debug(GAIM_DEBUG_INFO, "QQ",
				   "IM from group, removed by group internal_ID [%d]\n", im_header->sender_uid);
			/* sender_uid is in fact internal_group_id */
			qq_process_recv_group_im_been_removed(data, &cursor, len, im_header->sender_uid, gc);
			break;
		case QQ_RECV_IM_APPLY_ADD_TO_QUN:
			gaim_debug(GAIM_DEBUG_INFO, "QQ",
				   "IM from group, apply to join group internal_ID [%d]\n", im_header->sender_uid);
			/* sender_uid is in fact internal_group_id */
			qq_process_recv_group_im_apply_join(data, &cursor, len, im_header->sender_uid, gc);
			break;
		case QQ_RECV_IM_APPROVE_APPLY_ADD_TO_QUN:
			gaim_debug(GAIM_DEBUG_INFO, "QQ",
				   "IM for group system info, approved by group internal_id [%d]\n",
				   im_header->sender_uid);
			/* sender_uid is in fact internal_group_id */
			qq_process_recv_group_im_been_approved(data, &cursor, len, im_header->sender_uid, gc);
			break;
		case QQ_RECV_IM_REJCT_APPLY_ADD_TO_QUN:
			gaim_debug(GAIM_DEBUG_INFO, "QQ",
				   "IM for group system info, rejected by group internal_id [%d]\n",
				   im_header->sender_uid);
			/* sender_uid is in fact internal_group_id */
			qq_process_recv_group_im_been_rejected(data, &cursor, len, im_header->sender_uid, gc);
			break;
		case QQ_RECV_IM_SYS_NOTIFICATION:
			gaim_debug(GAIM_DEBUG_INFO, "QQ",
				   "IM from [%d], should be a system administrator\n", im_header->sender_uid);
			_qq_process_recv_sys_im(data, &cursor, len, gc);
			break;
		default:
			gaim_debug(GAIM_DEBUG_WARNING, "QQ",
				   "IM from [%d], [0x%02x] %s is not processed\n",
				   im_header->sender_uid,
				   im_header->im_type, qq_get_recv_im_type_str(im_header->im_type));
		}
	} else {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Error decrypt rev im\n");
	}
}
