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
#include "group_free.h"
#include "char_conv.h"
#include "qq_crypt.h"

#include "group_conv.h"
#include "group_find.h"
#include "group_internal.h"
#include "group_im.h"
#include "group_info.h"
#include "group_join.h"
#include "group_opt.h"
#include "group_search.h"

#include "header_info.h"
#include "qq_base.h"
#include "im.h"
#include "qq_process.h"
#include "packet_parse.h"
#include "qq_network.h"
#include "qq_trans.h"
#include "sys_msg.h"
#include "utils.h"

enum {
	QQ_ROOM_CMD_REPLY_OK = 0x00,
	QQ_ROOM_CMD_REPLY_SEARCH_ERROR = 0x02,
	QQ_ROOM_CMD_REPLY_NOT_MEMBER = 0x0a
};

/* default process, decrypt and dump */
static void process_cmd_unknow(PurpleConnection *gc,gchar *title, guint8 *data, gint data_len, guint16 cmd, guint16 seq)
{
	qq_data *qd;
	gchar *msg_utf8 = NULL;

	g_return_if_fail(data != NULL && data_len != 0);

	qq_show_packet(title, data, data_len);

	qd = (qq_data *) gc->proto_data;

	qq_hex_dump(PURPLE_DEBUG_WARNING, "QQ",
			data, data_len,
			">>> [%d] %s -> [default] decrypt and dump",
			seq, qq_get_cmd_desc(cmd));

	msg_utf8 = try_dump_as_gbk(data, data_len);
	if (msg_utf8) {
		purple_notify_info(gc, NULL, msg_utf8, NULL);
		g_free(msg_utf8);
	}
}

void qq_proc_cmd_server(PurpleConnection *gc, guint16 cmd, guint16 seq, guint8 *rcved, gint rcved_len)
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
			qq_process_recv_im(data, data_len, seq, gc);
			break;
		case QQ_CMD_RECV_MSG_SYS:
			qq_process_msg_sys(data, data_len, seq, gc);
			break;
		case QQ_CMD_BUDDY_CHANGE_STATUS:
			qq_process_buddy_change_status(data, data_len, gc);
			break;
		default:
			process_cmd_unknow(gc, "Unknow SERVER CMD", data, data_len, cmd, seq);
			break;
	}
}

static void process_room_cmd_notify(PurpleConnection *gc,
	guint8 room_cmd, guint8 room_id, guint8 reply, guint8 *data, gint data_len)
{
	gchar *msg, *msg_utf8;
	g_return_if_fail(data != NULL && data_len > 0);

	msg = g_strndup((gchar *) data, data_len);	/* it will append 0x00 */
	msg_utf8 = qq_to_utf8(msg, QQ_CHARSET_DEFAULT);
	g_free(msg);

	msg = g_strdup_printf(_("Command %s(0x%02X) id %d, reply [0x%02X]:\n%s"),
		qq_get_room_cmd_desc(room_cmd), room_cmd, room_id, reply, msg_utf8);

	purple_notify_error(gc, NULL, _("Invalid QQ Qun reply"), msg);
	g_free(msg);
	g_free(msg_utf8);
}

void qq_room_update(PurpleConnection *gc, guint8 room_cmd, guint32 room_id)
{
	qq_data *qd;
	qq_group *group;
	gint ret;

	g_return_if_fail (gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	group = qq_room_search_id(gc, room_id);
	if (group == NULL && room_id <= 0) {
		purple_debug_info("QQ", "No room, nothing update\n");
		return;
	}
	if (group == NULL ) {
		purple_debug_warning("QQ", "Failed search room id [%d]\n", room_id);
		return;
	}

	switch (room_cmd) {
		case 0:
			qq_send_room_cmd_mess(gc, QQ_ROOM_CMD_GET_INFO, group->id, NULL, 0,
					QQ_CMD_CLASS_UPDATE_ROOM, 0);
			break;
		case QQ_ROOM_CMD_GET_INFO:
			ret = qq_request_room_get_buddies(gc, group, QQ_CMD_CLASS_UPDATE_ROOM);
			if (ret <= 0) {
				qq_send_room_cmd_mess(gc, QQ_ROOM_CMD_GET_ONLINES, group->id, NULL, 0,
						QQ_CMD_CLASS_UPDATE_ROOM, 0);
			}
			break;
		case QQ_ROOM_CMD_GET_BUDDIES:
			qq_send_room_cmd_mess(gc, QQ_ROOM_CMD_GET_ONLINES, group->id, NULL, 0,
					QQ_CMD_CLASS_UPDATE_ROOM, 0);
			break;
		case QQ_ROOM_CMD_GET_ONLINES:
			/* last command */
		default:
			break;
	}
}

static void update_all_rooms(PurpleConnection *gc, guint8 room_cmd, guint32 room_id)
{
	qq_data *qd;
	gboolean is_new_turn = FALSE;
	qq_group *next_group;

	g_return_if_fail (gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	next_group = qq_room_get_next(gc, room_id);
	if (next_group == NULL && room_id <= 0) {
		purple_debug_info("QQ", "No room, nothing update\n");
		qd->is_finish_update = TRUE;
		return;
	}
	if (next_group == NULL ) {
		is_new_turn = TRUE;
		next_group = qq_room_get_next(gc, 0);
		g_return_if_fail(next_group != NULL);
	}

	switch (room_cmd) {
		case 0:
			qq_send_room_cmd_mess(gc, QQ_ROOM_CMD_GET_INFO, next_group->id, NULL, 0,
					QQ_CMD_CLASS_UPDATE_ALL, 0);
			break;
		case QQ_ROOM_CMD_GET_INFO:
			if (!is_new_turn) {
				qq_send_room_cmd_mess(gc, QQ_ROOM_CMD_GET_INFO, next_group->id, NULL, 0,
						QQ_CMD_CLASS_UPDATE_ALL, 0);
			} else {
				qq_request_room_get_buddies(gc, next_group, QQ_CMD_CLASS_UPDATE_ALL);
			}
			break;
		case QQ_ROOM_CMD_GET_BUDDIES:
			/* last command */
			if (!is_new_turn) {
				qq_request_room_get_buddies(gc, next_group, QQ_CMD_CLASS_UPDATE_ALL);
			} else {
				qd->is_finish_update = TRUE;
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
			qq_request_buddy_info(gc, qd->uid, QQ_CMD_CLASS_UPDATE_ALL, QQ_BUDDY_INFO_UPDATE_ONLY);
			break;
		case QQ_CMD_GET_USER_INFO:
			qq_request_change_status(gc, QQ_CMD_CLASS_UPDATE_ALL);
			break;
		case QQ_CMD_CHANGE_STATUS:
			qq_request_get_buddies_list(gc, 0, QQ_CMD_CLASS_UPDATE_ALL);
			break;
		case QQ_CMD_GET_BUDDIES_LIST:
			qq_request_get_buddies_and_rooms(gc, 0, QQ_CMD_CLASS_UPDATE_ALL);
			break;
		case QQ_CMD_GET_BUDDIES_AND_ROOMS:
			qq_request_get_buddies_levels(gc, QQ_CMD_CLASS_UPDATE_ALL);
			break;
		case QQ_CMD_GET_LEVEL:
			qq_request_get_buddies_online(gc, 0, QQ_CMD_CLASS_UPDATE_ALL);
			break;
		case QQ_CMD_GET_BUDDIES_ONLINE:
			/* last command */
			update_all_rooms(gc, 0, 0);
			break;
		default:
			break;
	}
}

static void update_all_rooms_online(PurpleConnection *gc, guint8 room_cmd, guint32 room_id)
{
	qq_data *qd;
	qq_group *next_group;

	g_return_if_fail (gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	next_group = qq_room_get_next_conv(gc, room_id);
	if (next_group == NULL && room_id <= 0) {
		purple_debug_info("QQ", "No room, no update online buddies\n");
		return;
	}
	if (next_group == NULL ) {
		purple_debug_info("QQ", "finished update online buddies\n");
		return;
	}

	switch (room_cmd) {
		case 0:
			qq_send_room_cmd_mess(gc, QQ_ROOM_CMD_GET_ONLINES, next_group->id, NULL, 0,
					QQ_CMD_CLASS_UPDATE_ALL, 0);
			break;
		case QQ_ROOM_CMD_GET_ONLINES:
			qq_send_room_cmd_mess(gc, QQ_ROOM_CMD_GET_ONLINES, next_group->id, NULL, 0,
					QQ_CMD_CLASS_UPDATE_ALL, 0);
			break;
		default:
			break;
	}
}

void qq_update_online(PurpleConnection *gc, guint16 cmd)
{
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
}

void qq_proc_room_cmd_reply(PurpleConnection *gc, guint16 seq,
		guint8 room_cmd, guint32 room_id, guint8 *rcved, gint rcved_len,
		gint update_class, guint32 ship32)
{
	qq_data *qd;
	guint8 *data;
	gint data_len;
	qq_group *group;
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
		return;
	}

	if (data_len <= 2) {
		purple_debug_warning("QQ",
			"Invaild len of room cmd decrypted, [%05d], 0x%02X %s for %d, len %d\n",
			seq, room_cmd, qq_get_room_cmd_desc(room_cmd), room_id, rcved_len);
		return;
	}

	group = qq_room_search_id(gc, room_id);
	if (group == NULL) {
		purple_debug_warning("QQ",
			"Missing room id in [%05d], 0x%02X %s for %d, len %d\n",
			seq, room_cmd, qq_get_room_cmd_desc(room_cmd), room_id, rcved_len);
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
		if (group != NULL) {
			qq_set_pending_id(&qd->joining_groups, group->ext_id, FALSE);
		}

		switch (reply) {	/* this should be all errors */
		case QQ_ROOM_CMD_REPLY_NOT_MEMBER:
			if (group != NULL) {
				purple_debug_warning("QQ",
					   _("You are not a member of QQ Qun \"%s\"\n"), group->title_utf8);
				group->my_role = QQ_ROOM_ROLE_NO;
				qq_group_refresh(gc, group);
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
		qq_process_room_cmd_get_info(data + bytes, data_len - bytes, gc);
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
		qq_process_group_cmd_search_group(data + bytes, data_len - bytes, gc);
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
	case QQ_ROOM_CMD_SEND_MSG:
		qq_process_group_cmd_im(data + bytes, data_len - bytes, gc);
		break;
	case QQ_ROOM_CMD_GET_ONLINES:
		qq_process_room_cmd_get_onlines(data + bytes, data_len - bytes, gc);
		if (group != NULL)
			qq_group_conv_refresh_online_member(gc, group);
		break;
	case QQ_ROOM_CMD_GET_BUDDIES:
		qq_process_room_cmd_get_buddies(data + bytes, data_len - bytes, gc);
		if (group != NULL)
			qq_group_conv_refresh_online_member(gc, group);
		break;
	default:
		purple_debug_warning("QQ", "Unknow room cmd 0x%02X %s\n",
			   reply_cmd, qq_get_room_cmd_desc(reply_cmd));
	}

	purple_debug_info("QQ", "Update class %d\n", update_class);
	if (update_class == QQ_CMD_CLASS_UPDATE_ALL) {
		update_all_rooms(gc, room_cmd, room_id);
		return;
	}
	if (update_class == QQ_CMD_CLASS_UPDATE_ONLINE) {
		update_all_rooms_online(gc, room_cmd, room_id);
		return;
	}
	if (update_class == QQ_CMD_CLASS_UPDATE_ROOM) {
		qq_room_update(gc, room_cmd, room_id);
	}
}

void qq_proc_cmd_login(PurpleConnection *gc, guint8 *rcved, gint rcved_len)
{
	qq_data *qd;
	guint8 *data;
	gint data_len;
	guint ret_8;

	g_return_if_fail (gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	data = g_newa(guint8, rcved_len);
	/* May use password_twice_md5 in the past version like QQ2005*/
	data_len = qq_decrypt(data, rcved, rcved_len, qd->inikey);
	if (data_len >= 0) {
		purple_debug_warning("QQ",
				"Decrypt login reply packet with inikey, %d bytes\n", data_len);
	} else {
		data_len = qq_decrypt(data, rcved, rcved_len, qd->password_twice_md5);
		if (data_len >= 0) {
			purple_debug_warning("QQ",
				"Decrypt login reply packet with password_twice_md5, %d bytes\n", data_len);
		} else {
			purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Can not decrypt login reply"));
			return;
		}
	}

	ret_8 = qq_process_login_reply(gc, data, data_len);
	if (ret_8 != QQ_LOGIN_REPLY_OK) {
		return;
	}

	purple_debug_info("QQ", "Login repliess OK; everything is fine\n");

	purple_connection_set_state(gc, PURPLE_CONNECTED);
	qd->is_login = TRUE;	/* must be defined after sev_finish_login */

	/* now initiate QQ Qun, do it first as it may take longer to finish */
	qq_group_init(gc);

	/* Now goes on updating my icon/nickname, not showing info_window */
	qd->modifying_face = FALSE;

	qq_update_all(gc, 0);
	return;
}

void qq_proc_cmd_reply(PurpleConnection *gc, guint16 cmd, guint16 seq,
		guint8 *rcved, gint rcved_len, gint update_class, guint32 ship32)
{
	qq_data *qd;

	guint8 *data;
	gint data_len;

	gboolean ret_bool = FALSE;
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
			qq_process_modify_info_reply(data, data_len, gc);
			break;
		case QQ_CMD_ADD_BUDDY_WO_AUTH:
			qq_process_add_buddy_reply(data, data_len, seq, gc);
			break;
		case QQ_CMD_DEL_BUDDY:
			qq_process_remove_buddy_reply(data, data_len, gc);
			break;
		case QQ_CMD_REMOVE_SELF:
			qq_process_remove_self_reply(data, data_len, gc);
			break;
		case QQ_CMD_BUDDY_AUTH:
			qq_process_add_buddy_auth_reply(data, data_len, gc);
			break;
		case QQ_CMD_GET_USER_INFO:
			qq_process_get_info_reply(data, data_len, gc);
			break;
		case QQ_CMD_CHANGE_STATUS:
			qq_process_change_status_reply(data, data_len, gc);
			break;
		case QQ_CMD_SEND_IM:
			qq_process_send_im_reply(data, data_len, gc);
			break;
		case QQ_CMD_KEEP_ALIVE:
			qq_process_keep_alive(data, data_len, gc);
			break;
		case QQ_CMD_GET_BUDDIES_ONLINE:
			ret_8 = qq_process_get_buddies_online_reply(data, data_len, gc);
			if (ret_8  > 0 && ret_8 < 0xff) {
				purple_debug_info("QQ", "Requesting for more online buddies\n");
				qq_request_get_buddies_online(gc, ret_8, update_class);
				return;
			}
			purple_debug_info("QQ", "All online buddies received\n");
			qq_refresh_all_buddy_status(gc);
			break;
		case QQ_CMD_GET_LEVEL:
			qq_process_get_level_reply(data, data_len, gc);
			break;
		case QQ_CMD_GET_BUDDIES_LIST:
			ret_16 = qq_process_get_buddies_list_reply(data, data_len, gc);
			if (ret_16 > 0	&& ret_16 < 0xffff) {
				purple_debug_info("QQ", "Requesting for more buddies\n");
				qq_request_get_buddies_list(gc, ret_16, update_class);
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
		default:
			process_cmd_unknow(gc, "Unknow reply CMD", data, data_len, cmd, seq);
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

