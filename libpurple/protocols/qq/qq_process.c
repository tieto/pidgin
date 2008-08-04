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
#include "crypt.h"
#include "group_network.h"
#include "header_info.h"
#include "qq_base.h"
#include "im.h"
#include "qq_process.h"
#include "packet_parse.h"
#include "qq_network.h"
#include "qq_trans.h"
#include "sys_msg.h"
#include "utils.h"

/* default process, decrypt and dump */
static void process_cmd_unknow(PurpleConnection *gc,gchar *title, guint8 *buf, gint buf_len, guint16 cmd, guint16 seq)
{
	qq_data *qd;
	guint8 *data;
	gint data_len;
	gchar *msg_utf8 = NULL;

	g_return_if_fail(buf != NULL && buf_len != 0);

	qq_show_packet(title, buf, buf_len);

	qd = (qq_data *) gc->proto_data;

	data_len = buf_len;
	data = g_newa(guint8, data_len);
	memset(data, 0, data_len);
	if ( !qq_decrypt(buf, buf_len, qd->session_key, data, &data_len )) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", "Fail decrypt packet with default process\n");
		return;
	}
	
	qq_hex_dump(PURPLE_DEBUG_WARNING, "QQ",
			data, data_len,
			">>> [%d] %s -> [default] decrypt and dump",
			seq, qq_get_cmd_desc(cmd));

	msg_utf8 = try_dump_as_gbk(data, data_len);
	if (msg_utf8) {
		g_free(msg_utf8);
	}
}

void qq_proc_cmd_server(PurpleConnection *gc,
	guint16 cmd, guint16 seq, guint8 *data, gint data_len)
{
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
		qq_send_packet_get_all_list_with_group(gc, 0);

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

void qq_proc_cmd_reply(PurpleConnection *gc,
	guint16 cmd, guint16 seq, guint8 *data, gint data_len)
{
	guint8 ret_8 = 0;
	guint16 ret_16 = 0;
	guint32 ret_32 = 0;
	gchar *error_msg = NULL;

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
		case QQ_CMD_GROUP_CMD:
			qq_process_group_cmd_reply(data, data_len, seq, gc);
			break;
		case QQ_CMD_GET_ALL_LIST_WITH_GROUP:
			ret_32 = qq_process_get_all_list_with_group_reply(data, data_len, gc);
			if (ret_32 > 0 && ret_32 < 0xffffffff) {
				purple_debug(PURPLE_DEBUG_INFO, "QQ", "Requesting for more buddies and groups\n");
				qq_send_packet_get_all_list_with_group(gc, ret_32);
			} else {
				purple_debug(PURPLE_DEBUG_INFO, "QQ", "All buddies and groups received\n"); 
			}
			break;
		default:
			process_cmd_unknow(gc, "Unknow reply CMD", data, data_len, cmd, seq);
			break;
	}
}

