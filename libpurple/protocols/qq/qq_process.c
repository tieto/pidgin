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

#ifdef _WIN32
#define random rand
#define srandom srand
#endif

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

void qq_proc_cmd_server(PurpleConnection *gc,
	guint16 cmd, guint16 seq, guint8 *rcved, gint rcved_len)
{
	qq_data *qd;

	guint8 *data;
	gint data_len;

	g_return_if_fail (gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	data = g_newa(guint8, rcved_len);
	data_len = qq_decrypt(data, rcved, rcved_len, qd->session_key);
	if (data_len < 0) {
		purple_debug(PURPLE_DEBUG_WARNING, "QQ",
			"Can not decrypt server cmd by session key, [%05d], 0x%04X %s, len %d\n", 
			seq, cmd, qq_get_cmd_desc(cmd), rcved_len);
		qq_show_packet("Can not decrypted", rcved, rcved_len);
		return;
	}

	if (data_len <= 0) {
		purple_debug(PURPLE_DEBUG_WARNING, "QQ",
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
		case QQ_CMD_RECV_MSG_BUDDY_CHANGE_STATUS:
			qq_process_buddy_change_status(data, data_len, gc);
			break;
		default:
			process_cmd_unknow(gc, "Unknow SERVER CMD", data, data_len, cmd, seq);
			break;
	}
}

static void process_cmd_login(PurpleConnection *gc, guint8 *data, gint data_len)
{
	qq_data *qd;
	guint ret_8;

	g_return_if_fail (gc != NULL && gc->proto_data != NULL);
	
	qd = (qq_data *) gc->proto_data;

	ret_8 = qq_process_login_reply(data, data_len, gc);
	if (ret_8 == QQ_LOGIN_REPLY_OK) {
		purple_debug(PURPLE_DEBUG_INFO, "QQ", "Login repliess OK; everything is fine\n");

		purple_connection_set_state(gc, PURPLE_CONNECTED);
		qd->logged_in = TRUE;	/* must be defined after sev_finish_login */

		/* now initiate QQ Qun, do it first as it may take longer to finish */
		qq_group_init(gc);

		/* Now goes on updating my icon/nickname, not showing info_window */
		qd->modifying_face = FALSE;

		qq_send_packet_get_info(gc, qd->uid, FALSE);
		/* grab my level */
		qq_send_packet_get_level(gc, qd->uid);

		qq_send_packet_change_status(gc);

		/* refresh buddies */
		qq_send_packet_get_buddies_list(gc, 0);

		/* refresh groups */
		qq_send_packet_get_buddies_and_rooms(gc, 0);

		return;
	}

	if (ret_8 == QQ_LOGIN_REPLY_REDIRECT) {
		qd->is_redirect = TRUE;
		/*
		purple_debug(PURPLE_DEBUG_WARNING, "QQ",
			"Redirected to new server: %s:%d\n", qd->real_hostname, qd->real_port);
		*/
		return;
	}

	if (ret_8 == QQ_LOGIN_REPLY_ERR_PWD) {
		if (!purple_account_get_remember_password(gc->account)) {
			purple_account_set_password(gc->account, NULL);
		}
		purple_connection_error_reason(gc,
			PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED, _("Incorrect password."));
		return;
	}

	if (ret_8 == QQ_LOGIN_REPLY_ERR_MISC) {
		if (purple_debug_is_enabled())
			purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR, _("Unable to login. Check debug log."));
		else
			purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR, _("Unable to login"));
		return;
	}
}

static void process_room_cmd_notify(PurpleConnection *gc, 
	guint8 room_cmd, guint8 room_id, guint8 reply_cmd, guint8 reply, guint8 *data, gint data_len)
{
	gchar *msg, *msg_utf8;
	g_return_if_fail(data != NULL && data_len > 0);

	msg = g_strndup((gchar *) data, data_len);	/* it will append 0x00 */
	msg_utf8 = qq_to_utf8(msg, QQ_CHARSET_DEFAULT);
	g_free(msg);
	
	msg = g_strdup_printf(_(
		"Reply %s(0x%02X )\n"
		"Sent %s(0x%02X )\n"
		"Room id %d, reply [0x%02X]: \n"
		"%s"), 
		qq_get_room_cmd_desc(reply_cmd), reply_cmd, 
		qq_get_room_cmd_desc(room_cmd), room_cmd, 
		room_id, reply, msg_utf8);
		
	purple_notify_error(gc, NULL, _("Failed room reply"), msg);
	g_free(msg);
	g_free(msg_utf8);
}

void qq_proc_room_cmd_reply(PurpleConnection *gc,
	guint16 seq, guint8 room_cmd, guint32 room_id, guint8 *rcved, gint rcved_len)
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
		purple_debug(PURPLE_DEBUG_WARNING, "QQ",
			"Can not decrypt room cmd by session key, [%05d], 0x%02X %s for %d, len %d\n", 
			seq, room_cmd, qq_get_room_cmd_desc(room_cmd), room_id, rcved_len);
		qq_show_packet("Can not decrypted", rcved, rcved_len);
		return;
	}

	if (room_id <= 0) {
		purple_debug(PURPLE_DEBUG_WARNING, "QQ",
			"Invaild room id, [%05d], 0x%02X %s for %d, len %d\n", 
			seq, room_cmd, qq_get_room_cmd_desc(room_cmd), room_id, rcved_len);
		return;
	}

	if (data_len <= 2) {
		purple_debug(PURPLE_DEBUG_WARNING, "QQ",
			"Invaild len of room cmd decrypted, [%05d], 0x%02X %s for %d, len %d\n", 
			seq, room_cmd, qq_get_room_cmd_desc(room_cmd), room_id, rcved_len);
		return;
	}
	
	group = qq_room_search_id(gc, room_id);
	if (group == NULL) {
		purple_debug(PURPLE_DEBUG_WARNING, "QQ",
			"Missing room id in [%05d], 0x%02X %s for %d, len %d\n", 
			seq, room_cmd, qq_get_room_cmd_desc(room_cmd), room_id, rcved_len);
	}
	
	bytes = 0;
	bytes += qq_get8(&reply_cmd, data + bytes);
	bytes += qq_get8(&reply, data + bytes);

	if (reply_cmd != room_cmd) {
		purple_debug(PURPLE_DEBUG_WARNING, "QQ",
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
				purple_debug(PURPLE_DEBUG_WARNING,
					   "QQ",
					   _("You are not a member of group \"%s\"\n"), group->group_name_utf8);
				group->my_status = QQ_GROUP_MEMBER_STATUS_NOT_MEMBER;
				qq_group_refresh(gc, group);
			}
			break;
		case QQ_ROOM_CMD_REPLY_SEARCH_ERROR:
			if (qd->roomlist != NULL) {
				if (purple_roomlist_get_in_progress(qd->roomlist))
					purple_roomlist_set_in_progress(qd->roomlist, FALSE);
			}
		default:
			process_room_cmd_notify(gc, room_cmd, room_id, reply_cmd, reply, data + bytes, data_len - bytes);
		}
		return;
	}

	/* seems ok so far, so we process the reply according to sub_cmd */
	switch (reply_cmd) {
	case QQ_ROOM_CMD_GET_INFO:
		qq_process_room_cmd_get_info(data + bytes, data_len - bytes, gc);
		if (group != NULL) {
			qq_send_cmd_group_get_members_info(gc, group);
			qq_send_cmd_group_get_online_members(gc, group);
		}
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
	case QQ_ROOM_CMD_GET_MEMBER_INFO:
		qq_process_room_cmd_get_members(data + bytes, data_len - bytes, gc);
		if (group != NULL)
			qq_group_conv_refresh_online_member(gc, group);
		break;
	default:
		purple_debug(PURPLE_DEBUG_WARNING, "QQ",
			   "Unknow room cmd 0x%02X %s\n", 
			   reply_cmd, qq_get_room_cmd_desc(reply_cmd));
	}
}

void qq_proc_cmd_reply(PurpleConnection *gc,
	guint16 cmd, guint16 seq, guint8 *rcved, gint rcved_len)
{
	qq_data *qd;

	guint8 *data;
	gint data_len;

	guint8 ret_8 = 0;
	guint16 ret_16 = 0;
	guint32 ret_32 = 0;
	gchar *error_msg = NULL;

	g_return_if_fail(rcved_len > 0);

	g_return_if_fail (gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	data = g_newa(guint8, rcved_len);
	if (cmd == QQ_CMD_TOKEN) {
		g_memmove(data, rcved, rcved_len);
		data_len = rcved_len;
	} else if (cmd == QQ_CMD_LOGIN) {
		/* May use password_twice_md5 in the past version like QQ2005*/
		data_len = qq_decrypt(data, rcved, rcved_len, qd->inikey);
		if (data_len >= 0) {
			purple_debug(PURPLE_DEBUG_WARNING, "QQ", 
					"Decrypt login reply packet with inikey, %d bytes\n", data_len);
		} else {
			data_len = qq_decrypt(data, rcved, rcved_len, qd->password_twice_md5);
			if (data_len >= 0) {
				purple_debug(PURPLE_DEBUG_WARNING, "QQ", 
					"Decrypt login reply packet with password_twice_md5, %d bytes\n", data_len);
			} else {
				purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, 
					_("Can not decrypt login reply"));
				return;
			}
		}
	} else {
		data_len = qq_decrypt(data, rcved, rcved_len, qd->session_key);
		if (data_len < 0) {
			purple_debug(PURPLE_DEBUG_WARNING, "QQ",
				"Can not reply by session key, [%05d], 0x%04X %s, len %d\n", 
				seq, cmd, qq_get_cmd_desc(cmd), rcved_len);
			qq_show_packet("Can not decrypted", rcved, rcved_len);
			return;
		}
	}
	
	if (data_len <= 0) {
		purple_debug(PURPLE_DEBUG_WARNING, "QQ",
			"Reply decrypted is empty, [%05d], 0x%04X %s, len %d\n", 
			seq, cmd, qq_get_cmd_desc(cmd), rcved_len);
		return;
	}

	switch (cmd) {
		case QQ_CMD_TOKEN:
			ret_8 = qq_process_token_reply(gc, error_msg, data, data_len);
			if (ret_8 != QQ_TOKEN_REPLY_OK) {
				if (error_msg == NULL) {
					error_msg = g_strdup_printf( _("Invalid token reply code, 0x%02X"), ret_8);
				}
				purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, error_msg);
				g_free(error_msg);
				return;
			}
			
			qq_send_packet_login(gc);
			break;
		case QQ_CMD_LOGIN:
			process_cmd_login(gc, data, data_len);
			break;
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
		case QQ_CMD_CHANGE_ONLINE_STATUS:
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
				purple_debug(PURPLE_DEBUG_INFO, "QQ", "Requesting for more online buddies\n"); 
				qq_send_packet_get_buddies_online(gc, ret_8);
			} else {
				purple_debug(PURPLE_DEBUG_INFO, "QQ", "All online buddies received\n"); 
				/* Fixme: this should not be called once*/
				qq_send_packet_get_buddies_levels(gc);

				qq_refresh_all_buddy_status(gc);
			}
			break;
		case QQ_CMD_GET_LEVEL:
			qq_process_get_level_reply(data, data_len, gc);
			break;
		case QQ_CMD_GET_BUDDIES_LIST:
			ret_16 = qq_process_get_buddies_list_reply(data, data_len, gc);
			if (ret_16 > 0	&& ret_16 < 0xffff) { 
				purple_debug(PURPLE_DEBUG_INFO, "QQ", "Requesting for more buddies\n"); 
				qq_send_packet_get_buddies_list(gc, ret_16);
			} else {
				purple_debug(PURPLE_DEBUG_INFO, "QQ", "All buddies received. Requesting buddies' levels\n");
				qq_send_packet_get_buddies_online(gc, 0);
			}
			break;
		case QQ_CMD_GET_BUDDIES_AND_ROOMS:
			ret_32 = qq_process_get_buddies_and_rooms(data, data_len, gc);
			if (ret_32 > 0 && ret_32 < 0xffffffff) {
				purple_debug(PURPLE_DEBUG_INFO, "QQ", "Requesting for more buddies and groups\n");
				qq_send_packet_get_buddies_and_rooms(gc, ret_32);
			} else {
				purple_debug(PURPLE_DEBUG_INFO, "QQ", "All buddies and groups received\n"); 
			}
			break;
		default:
			process_cmd_unknow(gc, "Unknow reply CMD", data, data_len, cmd, seq);
			break;
	}
}

