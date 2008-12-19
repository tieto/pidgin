/**
 * @file qq_network.c
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

#include "cipher.h"
#include "debug.h"
#include "internal.h"

#include "buddy_info.h"
#include "buddy_list.h"
#include "buddy_opt.h"
#include "group_info.h"
#include "char_conv.h"
#include "qq_crypt.h"

#include "group_internal.h"
#include "group_im.h"
#include "group_info.h"
#include "group_join.h"
#include "group_opt.h"

#include "qq_define.h"
#include "qq_base.h"
#include "im.h"
#include "qq_process.h"
#include "packet_parse.h"
#include "qq_network.h"
#include "qq_trans.h"
#include "utils.h"

enum {
	QQ_ROOM_CMD_REPLY_OK = 0x00,
	QQ_ROOM_CMD_REPLY_SEARCH_ERROR = 0x02,
	QQ_ROOM_CMD_REPLY_NOT_MEMBER = 0x0a
};

/* default process, decrypt and dump */
static void process_unknow_cmd(PurpleConnection *gc,const gchar *title, guint8 *data, gint data_len, guint16 cmd, guint16 seq)
{
	qq_data *qd;
	gchar *msg;

	g_return_if_fail(data != NULL && data_len != 0);

	qq_show_packet(title, data, data_len);

	qd = (qq_data *) gc->proto_data;

	qq_hex_dump(PURPLE_DEBUG_WARNING, "QQ",
			data, data_len,
			">>> [%d] %s -> [default] decrypt and dump",
			seq, qq_get_cmd_desc(cmd));

	msg = g_strdup_printf("Unknow command 0x%02X, %s", cmd, qq_get_cmd_desc(cmd));
	purple_notify_info(gc, _("QQ Error"), title, msg);
	g_free(msg);
}

/* parse the reply to send_im */
static void do_im_ack(guint8 *data, gint data_len, PurpleConnection *gc)
{
	qq_data *qd;

	g_return_if_fail(data != NULL && data_len != 0);

	qd = gc->proto_data;

	if (data[0] != 0) {
		purple_debug_warning("QQ", "Failed sent IM\n");
		purple_notify_error(gc, _("Error"), _("Unable to send message."), NULL);
		return;
	}

	purple_debug_info("QQ", "OK sent IM\n");
}

static void do_server_news(PurpleConnection *gc, guint8 *data, gint data_len)
{
	qq_data *qd = (qq_data *) gc->proto_data;
	gint bytes;
	gchar *title, *brief, *url;
	gchar *content;

	g_return_if_fail(data != NULL && data_len != 0);

	/* qq_show_packet("Rcv news", data, data_len); */

	bytes = 4;	/* skip unknown 4 bytes */

	bytes += qq_get_vstr(&title, QQ_CHARSET_DEFAULT, data + bytes);
	bytes += qq_get_vstr(&brief, QQ_CHARSET_DEFAULT, data + bytes);
	bytes += qq_get_vstr(&url, QQ_CHARSET_DEFAULT, data + bytes);

	content = g_strdup_printf(_("Server News:\n%s\n%s\n%s"), title, brief, url);

	if (qd->is_show_news) {
		qq_got_message(gc, content);
	} else {
		purple_debug_info("QQ", "QQ Server news:\n%s\n", content);
	}
	g_free(title);
	g_free(brief);
	g_free(url);
	g_free(content);
}

static void do_got_sms(PurpleConnection *gc, guint8 *data, gint data_len)
{
	gint bytes;
	gchar *mobile = NULL;
	gchar *msg = NULL;
	gchar *msg_utf8 = NULL;
	gchar *msg_formated;

	g_return_if_fail(data != NULL && data_len > 26);

	qq_show_packet("Rcv sms", data, data_len);

	bytes = 0;
	bytes += 1;	/* skip 0x00 */
	mobile = g_strndup((gchar *)data + bytes, 20);
	bytes += 20;
	bytes += 5; /* skip 0x(49 11 98 d5 03)*/
	if (bytes < data_len) {
		msg = g_strndup((gchar *)data + bytes, data_len - bytes);
		msg_utf8 = qq_to_utf8(msg, QQ_CHARSET_DEFAULT);
		g_free(msg);
	} else {
		msg_utf8 = g_strdup("");
	}

	msg_formated = g_strdup_printf(_("%s:%s"), mobile, msg_utf8);

	qq_got_message(gc, msg_formated);

	g_free(msg_formated);
	g_free(msg_utf8);
	g_free(mobile);
}

static void do_msg_sys_30(PurpleConnection *gc, guint8 *data, gint data_len)
{
	gint len;
	guint8 reply;
	gchar **segments, *msg_utf8;

	g_return_if_fail(data != NULL && data_len != 0);

	len = data_len;

	if (NULL == (segments = split_data(data, len, "\x2f", 2)))
		return;

	reply = strtol(segments[0], NULL, 10);
	if (reply == 1)
		purple_debug_warning("QQ", "We are kicked out by QQ server\n");

	msg_utf8 = qq_to_utf8(segments[1], QQ_CHARSET_DEFAULT);
	qq_got_message(gc, msg_utf8);
}

static void do_msg_sys_4c(PurpleConnection *gc, guint8 *data, gint data_len)
{
	gint bytes;
	gint msg_len;
	GString *content;
	gchar *msg = NULL;

	g_return_if_fail(data != NULL && data_len > 0);

	bytes = 6; /* skip 0x(06 00 01 1e 01 1c)*/

	content = g_string_new("");
	while (bytes < data_len) {
		msg_len = qq_get_vstr(&msg, QQ_CHARSET_DEFAULT, data + bytes);
		g_string_append(content, msg);
		g_string_append(content, "\n");
		g_free(msg);

		if (msg_len <= 1) {
			break;
		}
		bytes += msg_len;
	}
	if (bytes != data_len) {
		purple_debug_warning("QQ", "Failed to read QQ_MSG_SYS_4C\n");
		qq_show_packet("do_msg_sys_4c", data, data_len);
	}
	qq_got_message(gc, content->str);
	g_string_free(content, FALSE);
}

static const gchar *get_im_type_desc(gint type)
{
	switch (type) {
		case QQ_MSG_TO_BUDDY:
			return "QQ_MSG_TO_BUDDY";
		case QQ_MSG_TO_UNKNOWN:
			return "QQ_MSG_TO_UNKNOWN";
		case QQ_MSG_QUN_IM_UNKNOWN:
			return "QQ_MSG_QUN_IM_UNKNOWN";
		case QQ_MSG_ADD_TO_QUN:
			return "QQ_MSG_ADD_TO_QUN";
		case QQ_MSG_DEL_FROM_QUN:
			return "QQ_MSG_DEL_FROM_QUN";
		case QQ_MSG_APPLY_ADD_TO_QUN:
			return "QQ_MSG_APPLY_ADD_TO_QUN";
		case QQ_MSG_CREATE_QUN:
			return "QQ_MSG_CREATE_QUN";
		case QQ_MSG_SYS_30:
			return "QQ_MSG_SYS_30";
		case QQ_MSG_SYS_4C:
			return "QQ_MSG_SYS_4C";
		case QQ_MSG_APPROVE_APPLY_ADD_TO_QUN:
			return "QQ_MSG_APPROVE_APPLY_ADD_TO_QUN";
		case QQ_MSG_REJCT_APPLY_ADD_TO_QUN:
			return "QQ_MSG_REJCT_APPLY_ADD_TO_QUN";
		case QQ_MSG_TEMP_QUN_IM:
			return "QQ_MSG_TEMP_QUN_IM";
		case QQ_MSG_QUN_IM:
			return "QQ_MSG_QUN_IM";
		case QQ_MSG_NEWS:
			return "QQ_MSG_NEWS";
		case QQ_MSG_SMS:
			return "QQ_MSG_SMS";
		case QQ_MSG_EXTEND:
			return "QQ_MSG_EXTEND";
		case QQ_MSG_EXTEND_85:
			return "QQ_MSG_EXTEND_85";
		default:
			return "QQ_MSG_UNKNOWN";
	}
}

/* I receive a message, mainly it is text msg,
 * but we need to proess other types (group etc) */
static void process_private_msg(guint8 *data, gint data_len, guint16 seq, PurpleConnection *gc)
{
	qq_data *qd;
	gint bytes;

	struct {
		guint32 uid_from;
		guint32 uid_to;
		guint32 seq;
		struct in_addr ip_from;
		guint16 port_from;
		guint16 msg_type;
	} header;

	g_return_if_fail(data != NULL && data_len != 0);

	qd = (qq_data *) gc->proto_data;

	if (data_len < 16) {	/* we need to ack with the first 16 bytes */
		purple_debug_error("QQ", "MSG is too short\n");
		return;
	} else {
		/* when we receive a message,
		 * we send an ACK which is the first 16 bytes of incoming packet */
		qq_send_server_reply(gc, QQ_CMD_RECV_IM, seq, data, 16);
	}

	/* check len first */
	if (data_len < 20) {	/* length of im_header */
		purple_debug_error("QQ", "Invald MSG header, len %d < 20\n", data_len);
		return;
	}

	bytes = 0;
	bytes += qq_get32(&(header.uid_from), data + bytes);
	bytes += qq_get32(&(header.uid_to), data + bytes);
	bytes += qq_get32(&(header.seq), data + bytes);
	/* if the message is delivered via server, it is server IP/port */
	bytes += qq_getIP(&(header.ip_from), data + bytes);
	bytes += qq_get16(&(header.port_from), data + bytes);
	bytes += qq_get16(&(header.msg_type), data + bytes);
	/* im_header prepared */

	if (header.uid_to != qd->uid) {	/* should not happen */
		purple_debug_error("QQ", "MSG to %u, NOT me\n", header.uid_to);
		return;
	}

	/* check bytes */
	if (bytes >= data_len - 1) {
		purple_debug_warning("QQ", "Empty MSG\n");
		return;
	}

	switch (header.msg_type) {
		case QQ_MSG_NEWS:
			do_server_news(gc, data + bytes, data_len - bytes);
			break;
		case QQ_MSG_SMS:
			do_got_sms(gc, data + bytes, data_len - bytes);
			break;
		case QQ_MSG_EXTEND:
		case QQ_MSG_EXTEND_85:
			purple_debug_info("QQ", "MSG from buddy [%d]\n", header.uid_from);
			qq_process_extend_im(gc, data + bytes, data_len - bytes);
			break;
		case QQ_MSG_TO_UNKNOWN:
		case QQ_MSG_TO_BUDDY:
			purple_debug_info("QQ", "MSG from buddy [%d]\n", header.uid_from);
			qq_process_im(gc, data + bytes, data_len - bytes);
			break;
		case QQ_MSG_QUN_IM_UNKNOWN:
		case QQ_MSG_TEMP_QUN_IM:
		case QQ_MSG_QUN_IM:
			purple_debug_info("QQ", "MSG from room [%d]\n", header.uid_from);
			qq_process_room_im(data + bytes, data_len - bytes, header.uid_from, gc, header.msg_type);
			break;
		case QQ_MSG_ADD_TO_QUN:
			purple_debug_info("QQ", "Notice from [%d], Added\n", header.uid_from);
			/* uid_from is group id
			 * we need this to create a dummy group and add to blist */
			qq_process_room_buddy_joined(data + bytes, data_len - bytes, header.uid_from, gc);
			break;
		case QQ_MSG_DEL_FROM_QUN:
			purple_debug_info("QQ", "Notice from room [%d], Removed\n", header.uid_from);
			/* uid_from is group id */
			qq_process_room_buddy_removed(data + bytes, data_len - bytes, header.uid_from, gc);
			break;
		case QQ_MSG_APPLY_ADD_TO_QUN:
			purple_debug_info("QQ", "Notice from room [%d], Joined\n", header.uid_from);
			/* uid_from is group id */
			qq_process_room_buddy_request_join(data + bytes, data_len - bytes, header.uid_from, gc);
			break;
		case QQ_MSG_APPROVE_APPLY_ADD_TO_QUN:
			purple_debug_info("QQ", "Notice from room [%d], Confirm add in\n",
					header.uid_from);
			/* uid_from is group id */
			qq_process_room_buddy_approved(data + bytes, data_len - bytes, header.uid_from, gc);
			break;
		case QQ_MSG_REJCT_APPLY_ADD_TO_QUN:
			purple_debug_info("QQ", "Notice from room [%d], Refuse add in\n",
					header.uid_from);
			/* uid_from is group id */
			qq_process_room_buddy_rejected(data + bytes, data_len - bytes, header.uid_from, gc);
			break;
		case QQ_MSG_SYS_30:
			do_msg_sys_30(gc, data + bytes, data_len - bytes);
			break;
		case QQ_MSG_SYS_4C:
			do_msg_sys_4c(gc, data + bytes, data_len - bytes);
			break;
		default:
			purple_debug_warning("QQ", "MSG from %u, unknown type %s [0x%04X]\n",
					header.uid_from, get_im_type_desc(header.msg_type), header.msg_type);
			qq_show_packet("MSG header", data, bytes);
			if (data_len - bytes > 0) {
				qq_show_packet("MSG data", data + bytes, data_len - bytes);
			}
			break;
	}
}

/* Send ACK if the sys message needs an ACK */
static void request_server_ack(PurpleConnection *gc, gchar *funct_str, gchar *from, guint16 seq)
{
	qq_data *qd;
	guint8 *raw_data;
	gint bytes;
	guint8 bar;

	g_return_if_fail(funct_str != NULL && from != NULL);
	qd = (qq_data *) gc->proto_data;


	bar = 0x1e;
	raw_data = g_newa(guint8, strlen(funct_str) + strlen(from) + 16);

	bytes = 0;
	bytes += qq_putdata(raw_data + bytes, (guint8 *)funct_str, strlen(funct_str));
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_putdata(raw_data + bytes, (guint8 *)from, strlen(from));
	bytes += qq_put8(raw_data + bytes, bar);
	bytes += qq_put16(raw_data + bytes, seq);

	qq_send_server_reply(gc, QQ_CMD_ACK_SYS_MSG, 0, raw_data, bytes);
}

static void do_server_notice(PurpleConnection *gc, gchar *from, gchar *to,
		guint8 *data, gint data_len)
{
	qq_data *qd = (qq_data *) gc->proto_data;
	gchar *msg, *msg_utf8;
	gchar *title, *content;

	g_return_if_fail(from != NULL && to != NULL && data_len > 0);

	msg = g_strndup((gchar *)data, data_len);
	msg_utf8 = qq_to_utf8(msg, QQ_CHARSET_DEFAULT);
	g_free(msg);
	if (msg_utf8 == NULL) {
		purple_debug_error("QQ", "Recv NULL sys msg from %s to %s, discard\n",
				from, to);
		return;
	}

	title = g_strdup_printf(_("From %s:"), from);
	content = g_strdup_printf(_("Server notice From %s: \n%s"), from, msg_utf8);

	if (qd->is_show_notice) {
		qq_got_message(gc, content);
	} else {
		purple_debug_info("QQ", "QQ Server notice from %s:\n%s", from, msg_utf8);
	}
	g_free(msg_utf8);
	g_free(title);
	g_free(content);
}

static void process_server_msg(PurpleConnection *gc, guint8 *data, gint data_len, guint16 seq)
{
	qq_data *qd;
	guint8 *data_str;
	gchar **segments;
	gchar *funct_str, *from, *to;
	gint bytes, funct;

	g_return_if_fail(data != NULL && data_len != 0);

	qd = (qq_data *) gc->proto_data;

	data_str = g_newa(guint8, data_len + 1);
	g_memmove(data_str, data, data_len);
	data_str[data_len] = 0x00;

	segments = g_strsplit_set((gchar *) data_str, "\x1f", 0);
	g_return_if_fail(segments != NULL);
	if (g_strv_length(segments) < 3) {
		purple_debug_warning("QQ", "Server message segments is less than 3\n");
		g_strfreev(segments);
		return;
	}

	bytes = 0;
	funct_str = segments[0];
	bytes += strlen(funct_str) + 1;
	from = segments[1];
	bytes += strlen(from) + 1;
	to = segments[2];
	bytes += strlen(to) + 1;

	request_server_ack(gc, funct_str, from, seq);

	/* qq_show_packet("Server MSG", data, data_len); */
	if (strtoul(to, NULL, 10) != qd->uid) {	/* not to me */
		purple_debug_error("QQ", "Recv sys msg to [%s], not me!, discard\n", to);
		g_strfreev(segments);
		return;
	}

	funct = strtol(funct_str, NULL, 10);
	switch (funct) {
		case QQ_SERVER_BUDDY_ADDED:
		case QQ_SERVER_BUDDY_ADD_REQUEST:
		case QQ_SERVER_BUDDY_ADDED_ME:
		case QQ_SERVER_BUDDY_REJECTED_ME:
		case QQ_SERVER_BUDDY_ADD_REQUEST_EX:
		case QQ_SERVER_BUDDY_ADDING_EX:
		case QQ_SERVER_BUDDY_ADDED_ANSWER:
		case QQ_SERVER_BUDDY_ADDED_EX:
			qq_process_buddy_from_server(gc,  funct, from, to, data + bytes, data_len - bytes);
			break;
		case QQ_SERVER_NOTICE:
			do_server_notice(gc, from, to, data + bytes, data_len - bytes);
			break;
		case QQ_SERVER_NEW_CLIENT:
			purple_debug_warning("QQ", "QQ Server has newer client version\n");
			break;
		default:
			qq_show_packet("Unknown sys msg", data, data_len);
			purple_debug_warning("QQ", "Recv unknown sys msg code: %s\n", funct_str);
			break;
	}
	g_strfreev(segments);
}

void qq_proc_server_cmd(PurpleConnection *gc, guint16 cmd, guint16 seq, guint8 *rcved, gint rcved_len)
{
	qq_data *qd;

	guint8 *data;
	gint data_len;

	g_return_if_fail (gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	data = g_newa(guint8, rcved_len);
	data_len = qq_decrypt(data, rcved, rcved_len, qd->session_key);
	if (data_len < 0) {
		purple_debug_warning("QQ",
			"Can not decrypt server cmd by session key, [%05d], 0x%04X %s, len %d\n",
			seq, cmd, qq_get_cmd_desc(cmd), rcved_len);
		qq_show_packet("Can not decrypted", rcved, rcved_len);
		return;
	}

	if (data_len <= 0) {
		purple_debug_warning("QQ",
			"Server cmd decrypted is empty, [%05d], 0x%04X %s, len %d\n",
			seq, cmd, qq_get_cmd_desc(cmd), rcved_len);
		return;
	}

	/* now process the packet */
	switch (cmd) {
		case QQ_CMD_RECV_IM:
			process_private_msg(data, data_len, seq, gc);
			break;
		case QQ_CMD_RECV_MSG_SYS:
			process_server_msg(gc, data, data_len, seq);
			break;
		case QQ_CMD_BUDDY_CHANGE_STATUS:
			qq_process_buddy_change_status(data, data_len, gc);
			break;
		default:
			process_unknow_cmd(gc, _("Unknown SERVER CMD"), data, data_len, cmd, seq);
			break;
	}
}

static void process_room_cmd_notify(PurpleConnection *gc,
	guint8 room_cmd, guint8 room_id, guint8 reply, guint8 *data, gint data_len)
{
	gchar *prim;
	gchar *msg, *msg_utf8;
	g_return_if_fail(data != NULL && data_len > 0);

	msg = g_strndup((gchar *) data, data_len);	/* it will append 0x00 */
	msg_utf8 = qq_to_utf8(msg, QQ_CHARSET_DEFAULT);
	g_free(msg);

	prim = g_strdup_printf(_("Error reply of %s(0x%02X)\nRoom %u, reply 0x%02X"),
		qq_get_room_cmd_desc(room_cmd), room_cmd, room_id, reply);

	purple_notify_error(gc, _("QQ Qun Command"), prim, msg_utf8);

	g_free(prim);
	g_free(msg_utf8);
}

void qq_update_room(PurpleConnection *gc, guint8 room_cmd, guint32 room_id)
{
	qq_data *qd;
	gint ret;

	g_return_if_fail (gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	switch (room_cmd) {
		case 0:
			qq_send_room_cmd_mess(gc, QQ_ROOM_CMD_GET_INFO, room_id, NULL, 0,
					QQ_CMD_CLASS_UPDATE_ROOM, 0);
			break;
		case QQ_ROOM_CMD_GET_INFO:
			ret = qq_request_room_get_buddies(gc, room_id, QQ_CMD_CLASS_UPDATE_ROOM);
			if (ret <= 0) {
				qq_send_room_cmd_mess(gc, QQ_ROOM_CMD_GET_ONLINES, room_id, NULL, 0,
						QQ_CMD_CLASS_UPDATE_ROOM, 0);
			}
			break;
		case QQ_ROOM_CMD_GET_BUDDIES:
			qq_send_room_cmd_mess(gc, QQ_ROOM_CMD_GET_ONLINES, room_id, NULL, 0,
					QQ_CMD_CLASS_UPDATE_ROOM, 0);
			break;
		case QQ_ROOM_CMD_GET_ONLINES:
			/* last command */
		default:
			break;
	}
}

void qq_update_all_rooms(PurpleConnection *gc, guint8 room_cmd, guint32 room_id)
{
	qq_data *qd;
	gboolean is_new_turn = FALSE;
	guint32 next_id;

	g_return_if_fail (gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	next_id = qq_room_get_next(gc, room_id);
	purple_debug_info("QQ", "Update rooms, next id %u, prev id %u\n", next_id, room_id);

	if (next_id <= 0) {
		if (room_id > 0) {
			is_new_turn = TRUE;
			next_id = qq_room_get_next(gc, 0);
			purple_debug_info("QQ", "New turn, id %u\n", next_id);
		} else {
			purple_debug_info("QQ", "No room. Finished update\n");
			return;
		}
	}

	switch (room_cmd) {
		case 0:
			qq_send_room_cmd_mess(gc, QQ_ROOM_CMD_GET_INFO, next_id, NULL, 0,
					QQ_CMD_CLASS_UPDATE_ALL, 0);
			break;
		case QQ_ROOM_CMD_GET_INFO:
			if (!is_new_turn) {
				qq_send_room_cmd_mess(gc, QQ_ROOM_CMD_GET_INFO, next_id, NULL, 0,
						QQ_CMD_CLASS_UPDATE_ALL, 0);
			} else {
				qq_request_room_get_buddies(gc, next_id, QQ_CMD_CLASS_UPDATE_ALL);
			}
			break;
		case QQ_ROOM_CMD_GET_BUDDIES:
			/* last command */
			if (!is_new_turn) {
				qq_request_room_get_buddies(gc, next_id, QQ_CMD_CLASS_UPDATE_ALL);
			} else {
				purple_debug_info("QQ", "Finished update\n");
			}
			break;
		default:
			break;
	}
}

void qq_update_all(PurpleConnection *gc, guint16 cmd)
{
	qq_data *qd;

	g_return_if_fail (gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	switch (cmd) {
		case 0:
			qq_request_buddy_info(gc, qd->uid, QQ_CMD_CLASS_UPDATE_ALL, 0);
			break;
		case QQ_CMD_GET_BUDDY_INFO:
			qq_request_change_status(gc, QQ_CMD_CLASS_UPDATE_ALL);
			break;
		case QQ_CMD_CHANGE_STATUS:
			qq_request_get_buddies(gc, 0, QQ_CMD_CLASS_UPDATE_ALL);
			break;
		case QQ_CMD_GET_BUDDIES_LIST:
			qq_request_get_buddies_and_rooms(gc, 0, QQ_CMD_CLASS_UPDATE_ALL);
			break;
		case QQ_CMD_GET_BUDDIES_AND_ROOMS:
			if (qd->client_version >= 2007) {
				/* QQ2007/2008 can not get buddies level*/
				qq_request_get_buddies_online(gc, 0, QQ_CMD_CLASS_UPDATE_ALL);
			} else {
				qq_request_get_buddies_level(gc, QQ_CMD_CLASS_UPDATE_ALL);
			}
			break;
		case QQ_CMD_GET_LEVEL:
			qq_request_get_buddies_online(gc, 0, QQ_CMD_CLASS_UPDATE_ALL);
			break;
		case QQ_CMD_GET_BUDDIES_ONLINE:
			/* last command */
			qq_update_all_rooms(gc, 0, 0);
			break;
		default:
			break;
	}
	qd->online_last_update = time(NULL);
}

static void update_all_rooms_online(PurpleConnection *gc, guint8 room_cmd, guint32 room_id)
{
	qq_data *qd;
	guint32 next_id;

	g_return_if_fail (gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	next_id = qq_room_get_next_conv(gc, room_id);
	if (next_id <= 0 && room_id <= 0) {
		purple_debug_info("QQ", "No room in conversation, no update online buddies\n");
		return;
	}
	if (next_id <= 0 ) {
		purple_debug_info("QQ", "finished update rooms' online buddies\n");
		return;
	}

	switch (room_cmd) {
		case 0:
			qq_send_room_cmd_mess(gc, QQ_ROOM_CMD_GET_ONLINES, next_id, NULL, 0,
					QQ_CMD_CLASS_UPDATE_ALL, 0);
			break;
		case QQ_ROOM_CMD_GET_ONLINES:
			qq_send_room_cmd_mess(gc, QQ_ROOM_CMD_GET_ONLINES, next_id, NULL, 0,
					QQ_CMD_CLASS_UPDATE_ALL, 0);
			break;
		default:
			break;
	}
}

void qq_update_online(PurpleConnection *gc, guint16 cmd)
{
	qq_data *qd;
	g_return_if_fail (gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	switch (cmd) {
		case 0:
			qq_request_get_buddies_online(gc, 0, QQ_CMD_CLASS_UPDATE_ONLINE);
			break;
		case QQ_CMD_GET_BUDDIES_ONLINE:
			/* last command */
			update_all_rooms_online(gc, 0, 0);
			break;
		default:
			break;
	}
	qd->online_last_update = time(NULL);
}

void qq_proc_room_cmds(PurpleConnection *gc, guint16 seq,
		guint8 room_cmd, guint32 room_id, guint8 *rcved, gint rcved_len,
		gint update_class, guint32 ship32)
{
	qq_data *qd;
	guint8 *data;
	gint data_len;
	qq_room_data *rmd;
	gint bytes;
	guint8 reply_cmd, reply;

	g_return_if_fail (gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	data = g_newa(guint8, rcved_len);
	data_len = qq_decrypt(data, rcved, rcved_len, qd->session_key);
	if (data_len < 0) {
		purple_debug_warning("QQ",
			"Can not decrypt room cmd by session key, [%05d], 0x%02X %s for %d, len %d\n",
			seq, room_cmd, qq_get_room_cmd_desc(room_cmd), room_id, rcved_len);
		qq_show_packet("Can not decrypted", rcved, rcved_len);
		return;
	}

	if (room_id <= 0) {
		purple_debug_warning("QQ",
			"Invaild room id, [%05d], 0x%02X %s for %d, len %d\n",
			seq, room_cmd, qq_get_room_cmd_desc(room_cmd), room_id, rcved_len);
		/* Some room cmd has no room id, like QQ_ROOM_CMD_SEARCH */
	}

	if (data_len <= 2) {
		purple_debug_warning("QQ",
			"Invaild len of room cmd decrypted, [%05d], 0x%02X %s for %d, len %d\n",
			seq, room_cmd, qq_get_room_cmd_desc(room_cmd), room_id, rcved_len);
		return;
	}

	bytes = 0;
	bytes += qq_get8(&reply_cmd, data + bytes);
	bytes += qq_get8(&reply, data + bytes);

	if (reply_cmd != room_cmd) {
		purple_debug_warning("QQ",
			"Missing room cmd in reply 0x%02X %s, [%05d], 0x%02X %s for %d, len %d\n",
			reply_cmd, qq_get_room_cmd_desc(reply_cmd),
			seq, room_cmd, qq_get_room_cmd_desc(room_cmd), room_id, rcved_len);
	}

	/* now process the packet */
	if (reply != QQ_ROOM_CMD_REPLY_OK) {
		switch (reply) {	/* this should be all errors */
		case QQ_ROOM_CMD_REPLY_NOT_MEMBER:
			rmd = qq_room_data_find(gc, room_id);
			if (rmd == NULL) {
				purple_debug_warning("QQ",
						"Missing room id in [%05d], 0x%02X %s for %d, len %d\n",
						seq, room_cmd, qq_get_room_cmd_desc(room_cmd), room_id, rcved_len);
			} else {
				purple_debug_warning("QQ",
					   "Not a member of room \"%s\"\n", rmd->title_utf8);
				rmd->my_role = QQ_ROOM_ROLE_NO;
			}
			break;
		case QQ_ROOM_CMD_REPLY_SEARCH_ERROR:
			if (qd->roomlist != NULL) {
				if (purple_roomlist_get_in_progress(qd->roomlist))
					purple_roomlist_set_in_progress(qd->roomlist, FALSE);
			}
		default:
			process_room_cmd_notify(gc, reply_cmd, room_id, reply, data + bytes, data_len - bytes);
		}
		return;
	}

	/* seems ok so far, so we process the reply according to sub_cmd */
	switch (reply_cmd) {
	case QQ_ROOM_CMD_GET_INFO:
		qq_process_room_cmd_get_info(data + bytes, data_len - bytes, ship32, gc);
		break;
	case QQ_ROOM_CMD_CREATE:
		qq_group_process_create_group_reply(data + bytes, data_len - bytes, gc);
		break;
	case QQ_ROOM_CMD_CHANGE_INFO:
		qq_group_process_modify_info_reply(data + bytes, data_len - bytes, gc);
		break;
	case QQ_ROOM_CMD_MEMBER_OPT:
		qq_group_process_modify_members_reply(data + bytes, data_len - bytes, gc);
		break;
	case QQ_ROOM_CMD_ACTIVATE:
		qq_group_process_activate_group_reply(data + bytes, data_len - bytes, gc);
		break;
	case QQ_ROOM_CMD_SEARCH:
		qq_process_room_search(gc, data + bytes, data_len - bytes, ship32);
		break;
	case QQ_ROOM_CMD_JOIN:
		qq_process_group_cmd_join_group(data + bytes, data_len - bytes, gc);
		break;
	case QQ_ROOM_CMD_AUTH:
		qq_process_group_cmd_join_group_auth(data + bytes, data_len - bytes, gc);
		break;
	case QQ_ROOM_CMD_QUIT:
		qq_process_group_cmd_exit_group(data + bytes, data_len - bytes, gc);
		break;
	case QQ_ROOM_CMD_SEND_IM:
		qq_process_room_send_im(gc, data + bytes, data_len - bytes);
		break;
	case QQ_ROOM_CMD_SEND_IM_EX:
		qq_process_room_send_im_ex(gc, data + bytes, data_len - bytes);
		break;
	case QQ_ROOM_CMD_GET_ONLINES:
		qq_process_room_cmd_get_onlines(data + bytes, data_len - bytes, gc);
		break;
	case QQ_ROOM_CMD_GET_BUDDIES:
		qq_process_room_cmd_get_buddies(data + bytes, data_len - bytes, gc);
		break;
	default:
		purple_debug_warning("QQ", "Unknow room cmd 0x%02X %s\n",
			   reply_cmd, qq_get_room_cmd_desc(reply_cmd));
	}

	if (update_class == QQ_CMD_CLASS_NONE)
		return;

	if (update_class == QQ_CMD_CLASS_UPDATE_ALL) {
		qq_update_all_rooms(gc, room_cmd, room_id);
		return;
	}
	if (update_class == QQ_CMD_CLASS_UPDATE_ONLINE) {
		update_all_rooms_online(gc, room_cmd, room_id);
		return;
	}
	if (update_class == QQ_CMD_CLASS_UPDATE_ROOM) {
		qq_update_room(gc, room_cmd, room_id);
	}
}

guint8 qq_proc_login_cmds(PurpleConnection *gc,  guint16 cmd, guint16 seq,
		guint8 *rcved, gint rcved_len, gint update_class, guint32 ship32)
{
	qq_data *qd;
	guint8 *data = NULL;
	gint data_len = 0;
	guint ret_8 = QQ_LOGIN_REPLY_ERR;

	g_return_val_if_fail (gc != NULL && gc->proto_data != NULL, QQ_LOGIN_REPLY_ERR);
	qd = (qq_data *) gc->proto_data;

	g_return_val_if_fail(rcved_len > 0, QQ_LOGIN_REPLY_ERR);
	data = g_newa(guint8, rcved_len);

	switch (cmd) {
		case QQ_CMD_TOKEN:
			if (qq_process_token(gc, rcved, rcved_len) == QQ_LOGIN_REPLY_OK) {
				if (qd->client_version >= 2007) {
					qq_request_token_ex(gc);
				} else {
					qq_request_login(gc);
				}
				return QQ_LOGIN_REPLY_OK;
			}
			return QQ_LOGIN_REPLY_ERR;
		case QQ_CMD_GET_SERVER:
		case QQ_CMD_TOKEN_EX:
			data_len = qq_decrypt(data, rcved, rcved_len, qd->ld.random_key);
			break;
		case QQ_CMD_CHECK_PWD:
			/* May use password_twice_md5 in the past version like QQ2005 */
			data_len = qq_decrypt(data, rcved, rcved_len, qd->ld.random_key);
			if (data_len >= 0) {
				purple_debug_warning("QQ", "Decrypt login packet by random_key, %d bytes\n", data_len);
			} else {
				data_len = qq_decrypt(data, rcved, rcved_len, qd->ld.pwd_twice_md5);
				if (data_len >= 0) {
					purple_debug_warning("QQ", "Decrypt login packet by pwd_twice_md5, %d bytes\n", data_len);
				}
			}
			break;
		case QQ_CMD_LOGIN:
		default:
			if (qd->client_version >= 2007) {
				data_len = qq_decrypt(data, rcved, rcved_len, qd->ld.pwd_twice_md5);
				if (data_len >= 0) {
					purple_debug_warning("QQ", "Decrypt login packet by pwd_twice_md5\n");
				} else {
					data_len = qq_decrypt(data, rcved, rcved_len, qd->ld.login_key);
					if (data_len >= 0) {
						purple_debug_warning("QQ", "Decrypt login packet by login_key\n");
					}
				}
			} else {
				/* May use password_twice_md5 in the past version like QQ2005 */
				data_len = qq_decrypt(data, rcved, rcved_len, qd->ld.random_key);
				if (data_len >= 0) {
					purple_debug_warning("QQ", "Decrypt login packet by random_key\n");
				} else {
					data_len = qq_decrypt(data, rcved, rcved_len, qd->ld.pwd_twice_md5);
					if (data_len >= 0) {
						purple_debug_warning("QQ", "Decrypt login packet by pwd_twice_md5\n");
					}
				}
			}
			break;
	}

	if (data_len < 0) {
		purple_debug_warning("QQ",
				"Can not decrypt login cmd, [%05d], 0x%04X %s, len %d\n",
				seq, cmd, qq_get_cmd_desc(cmd), rcved_len);
		qq_show_packet("Can not decrypted", rcved, rcved_len);
		purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_ENCRYPTION_ERROR,
				_("Could not decrypt login reply"));
		return QQ_LOGIN_REPLY_ERR;
	}

	switch (cmd) {
		case QQ_CMD_GET_SERVER:
			ret_8 = qq_process_get_server(gc, data, data_len);
			if ( ret_8 == QQ_LOGIN_REPLY_OK) {
				qq_request_token(gc);
			} else if ( ret_8 == QQ_LOGIN_REPLY_REDIRECT) {
				return QQ_LOGIN_REPLY_REDIRECT;
			}
			break;
		case QQ_CMD_TOKEN_EX:
			ret_8 = qq_process_token_ex(gc, data, data_len);
			if (ret_8 == QQ_LOGIN_REPLY_OK) {
				qq_request_check_pwd(gc);
			} else if (ret_8 == QQ_LOGIN_REPLY_NEXT_TOKEN_EX) {
				qq_request_token_ex_next(gc);
			} else if (ret_8 == QQ_LOGIN_REPLY_CAPTCHA_DLG) {
				qq_captcha_input_dialog(gc, &(qd->captcha));
				g_free(qd->captcha.token);
				g_free(qd->captcha.data);
				memset(&qd->captcha, 0, sizeof(qd->captcha));
			}
			break;
		case QQ_CMD_CHECK_PWD:
			ret_8 = qq_process_check_pwd(gc, data, data_len);
			if (ret_8 != QQ_LOGIN_REPLY_OK) {
				return ret_8;
			}
			if (qd->client_version >= 2008) {
				qq_request_login_2008(gc);
			} else {
				qq_request_login_2007(gc);
			}
			break;
		case QQ_CMD_LOGIN:
			if (qd->client_version >= 2008) {
				ret_8 = qq_process_login_2008(gc, data, data_len);
				if ( ret_8 == QQ_LOGIN_REPLY_REDIRECT) {
                		qq_request_get_server(gc);
                		return QQ_LOGIN_REPLY_OK;
            	}
			} else if (qd->client_version >= 2007) {
				ret_8 = qq_process_login_2007(gc, data, data_len);
				if ( ret_8 == QQ_LOGIN_REPLY_REDIRECT) {
                		qq_request_get_server(gc);
                		return QQ_LOGIN_REPLY_OK;
            	}
			} else {
				ret_8 = qq_process_login(gc, data, data_len);
			}
			if (ret_8 != QQ_LOGIN_REPLY_OK) {
				return ret_8;
			}

			purple_connection_update_progress(gc, _("Logging in"), QQ_CONNECT_STEPS - 1, QQ_CONNECT_STEPS);
			purple_debug_info("QQ", "Login replies OK; everything is fine\n");
			purple_connection_set_state(gc, PURPLE_CONNECTED);
			qd->is_login = TRUE;	/* must be defined after sev_finish_login */

			/* now initiate QQ Qun, do it first as it may take longer to finish */
			qq_room_data_initial(gc);

			/* is_login, but we have packets before login */
			qq_trans_process_remained(gc);

			qq_update_all(gc, 0);
			break;
		default:
			process_unknow_cmd(gc, _("Unknown LOGIN CMD"), data, data_len, cmd, seq);
			return QQ_LOGIN_REPLY_ERR;
	}
	return QQ_LOGIN_REPLY_OK;
}

void qq_proc_client_cmds(PurpleConnection *gc, guint16 cmd, guint16 seq,
		guint8 *rcved, gint rcved_len, gint update_class, guint32 ship32)
{
	qq_data *qd;

	guint8 *data;
	gint data_len;

	guint8 ret_8 = 0;
	guint16 ret_16 = 0;
	guint32 ret_32 = 0;
	gboolean is_unknow = FALSE;

	g_return_if_fail(rcved_len > 0);

	g_return_if_fail (gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	data = g_newa(guint8, rcved_len);
	data_len = qq_decrypt(data, rcved, rcved_len, qd->session_key);
	if (data_len < 0) {
		purple_debug_warning("QQ",
			"Reply can not be decrypted by session key, [%05d], 0x%04X %s, len %d\n",
			seq, cmd, qq_get_cmd_desc(cmd), rcved_len);
		qq_show_packet("Can not decrypted", rcved, rcved_len);
		return;
	}

	if (data_len <= 0) {
		purple_debug_warning("QQ",
			"Reply decrypted is empty, [%05d], 0x%04X %s, len %d\n",
			seq, cmd, qq_get_cmd_desc(cmd), rcved_len);
		return;
	}

	switch (cmd) {
		case QQ_CMD_UPDATE_INFO:
			qq_process_change_info(gc, data, data_len);
			break;
		case QQ_CMD_ADD_BUDDY_NO_AUTH:
			qq_process_add_buddy_no_auth(gc, data, data_len, ship32);
			break;
		case QQ_CMD_REMOVE_BUDDY:
			qq_process_remove_buddy(gc, data, data_len, ship32);
			break;
		case QQ_CMD_REMOVE_ME:
			qq_process_buddy_remove_me(gc, data, data_len, ship32);
			break;
		case QQ_CMD_ADD_BUDDY_AUTH:
			qq_process_add_buddy_auth(data, data_len, gc);
			break;
		case QQ_CMD_GET_BUDDY_INFO:
			qq_process_get_buddy_info(data, data_len, ship32, gc);
			break;
		case QQ_CMD_CHANGE_STATUS:
			qq_process_change_status(data, data_len, gc);
			break;
		case QQ_CMD_SEND_IM:
			do_im_ack(data, data_len, gc);
			break;
		case QQ_CMD_KEEP_ALIVE:
			if (qd->client_version >= 2008) {
				qq_process_keep_alive_2008(data, data_len, gc);
			} else if (qd->client_version >= 2007) {
				qq_process_keep_alive_2007(data, data_len, gc);
			} else {
				qq_process_keep_alive(data, data_len, gc);
			}
			break;
		case QQ_CMD_GET_BUDDIES_ONLINE:
			ret_8 = qq_process_get_buddies_online(data, data_len, gc);
			if (ret_8  > 0 && ret_8 < 0xff) {
				purple_debug_info("QQ", "Requesting for more online buddies\n");
				qq_request_get_buddies_online(gc, ret_8, update_class);
				return;
			}
			purple_debug_info("QQ", "All online buddies received\n");
			qq_update_buddyies_status(gc);
			break;
		case QQ_CMD_GET_LEVEL:
			qq_process_get_level_reply(data, data_len, gc);
			break;
		case QQ_CMD_GET_BUDDIES_LIST:
			ret_16 = qq_process_get_buddies(data, data_len, gc);
			if (ret_16 > 0	&& ret_16 < 0xffff) {
				purple_debug_info("QQ", "Requesting for more buddies\n");
				qq_request_get_buddies(gc, ret_16, update_class);
				return;
			}
			purple_debug_info("QQ", "All buddies received. Requesting buddies' levels\n");
			break;
		case QQ_CMD_GET_BUDDIES_AND_ROOMS:
			ret_32 = qq_process_get_buddies_and_rooms(data, data_len, gc);
			if (ret_32 > 0 && ret_32 < 0xffffffff) {
				purple_debug_info("QQ", "Requesting for more buddies and groups\n");
				qq_request_get_buddies_and_rooms(gc, ret_32, update_class);
				return;
			}
			purple_debug_info("QQ", "All buddies and groups received\n");
			break;
		case QQ_CMD_AUTH_CODE:
			qq_process_auth_code(gc, data, data_len, ship32);
			break;
		case QQ_CMD_BUDDY_QUESTION:
			qq_process_question(gc, data, data_len, ship32);
			break;
		case QQ_CMD_ADD_BUDDY_NO_AUTH_EX:
			qq_process_add_buddy_no_auth_ex(gc, data, data_len, ship32);
			break;
		case QQ_CMD_ADD_BUDDY_AUTH_EX:
			qq_process_add_buddy_auth_ex(gc, data, data_len, ship32);
			break;
		case QQ_CMD_BUDDY_CHECK_CODE:
			qq_process_buddy_check_code(gc, data, data_len);
			break;
		default:
			process_unknow_cmd(gc, _("Unknown CLIENT CMD"), data, data_len, cmd, seq);
			is_unknow = TRUE;
			break;
	}
	if (is_unknow)
		return;

	if (update_class == QQ_CMD_CLASS_NONE)
		return;

	purple_debug_info("QQ", "Update class %d\n", update_class);
	if (update_class == QQ_CMD_CLASS_UPDATE_ALL) {
		qq_update_all(gc, cmd);
		return;
	}
	if (update_class == QQ_CMD_CLASS_UPDATE_ONLINE) {
		qq_update_online(gc, cmd);
		return;
	}
}

