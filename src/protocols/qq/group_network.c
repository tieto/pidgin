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

#include "debug.h"
#include "notify.h"

#include "char_conv.h"
#include "crypt.h"
#include "group_conv.h"
#include "group_find.h"
#include "group_hash.h"
#include "group_im.h"
#include "group_info.h"
#include "group_join.h"
#include "group_network.h"
#include "group_opt.h"
#include "group_search.h"
#include "header_info.h"
#include "send_core.h"
#include "utils.h"

enum {
	QQ_GROUP_CMD_REPLY_OK = 0x00,
	QQ_GROUP_CMD_REPLY_NOT_MEMBER = 0x0a
};

const gchar *qq_group_cmd_get_desc(qq_group_cmd cmd)
{
	switch (cmd) {
	case QQ_GROUP_CMD_CREATE_GROUP:
		return "QQ_GROUP_CMD_CREATE_GROUP";
	case QQ_GROUP_CMD_MEMBER_OPT:
		return "QQ_GROUP_CMD_MEMBER_OPT";
	case QQ_GROUP_CMD_MODIFY_GROUP_INFO:
		return "QQ_GROUP_CMD_MODIFY_GROUP_INFO";
	case QQ_GROUP_CMD_GET_GROUP_INFO:
		return "QQ_GROUP_CMD_GET_GROUP_INFO";
	case QQ_GROUP_CMD_ACTIVATE_GROUP:
		return "QQ_GROUP_CMD_ACTIVATE_GROUP";
	case QQ_GROUP_CMD_SEARCH_GROUP:
		return "QQ_GROUP_CMD_SEARCH_GROUP";
	case QQ_GROUP_CMD_JOIN_GROUP:
		return "QQ_GROUP_CMD_JOIN_GROUP";
	case QQ_GROUP_CMD_JOIN_GROUP_AUTH:
		return "QQ_GROUP_CMD_JOIN_GROUP_AUTH";
	case QQ_GROUP_CMD_EXIT_GROUP:
		return "QQ_GROUP_CMD_EXIT_GROUP";
	case QQ_GROUP_CMD_SEND_MSG:
		return "QQ_GROUP_CMD_SEND_MSG";
	case QQ_GROUP_CMD_GET_ONLINE_MEMBER:
		return "QQ_GROUP_CMD_GET_ONLINE_MEMBER";
	case QQ_GROUP_CMD_GET_MEMBER_INFO:
		return "QQ_GROUP_CMD_GET_MEMBER_INFO";
	default:
		return "Unknown QQ Group Command";
	}
}

/* default process of reply error */
static void _qq_process_group_cmd_reply_error_default(guint8 reply, guint8 *cursor, gint len, GaimConnection *gc)
{
	gchar *msg, *msg_utf8;
	g_return_if_fail(cursor != NULL && len > 0 && gc != NULL);

	msg = g_strndup(cursor, len);	/* it will append 0x00 */
	msg_utf8 = qq_to_utf8(msg, QQ_CHARSET_DEFAULT);
	g_free(msg);
	msg = g_strdup_printf(_("Code [0x%02X]: %s"), reply, msg_utf8);
	gaim_notify_error(gc, NULL, _("Group Operation Error"), msg);
	g_free(msg);
	g_free(msg_utf8);
}

/* default process, dump only */
static void _qq_process_group_cmd_reply_default(guint8 *data, guint8 **cursor, gint len, GaimConnection *gc)
{
	g_return_if_fail(gc != NULL && data != NULL && len > 0);
	gaim_debug(GAIM_DEBUG_INFO, "QQ", "Dump unprocessed group cmd reply:\n%s", hex_dump_to_str(data, len));
}

/* The lower layer command of send group cmd */
void qq_send_group_cmd(GaimConnection *gc, qq_group *group, guint8 *raw_data, gint data_len)
{
	qq_data *qd;
	group_packet *p;

	g_return_if_fail(gc != NULL);
	g_return_if_fail(raw_data != NULL && data_len > 0);

	qd = (qq_data *) gc->proto_data;
	g_return_if_fail(qd != NULL);

	qq_send_cmd(gc, QQ_CMD_GROUP_CMD, TRUE, 0, TRUE, raw_data, data_len);

	p = g_new0(group_packet, 1);

	p->send_seq = qd->send_seq;
	if (group == NULL)
		p->internal_group_id = 0;
	else
		p->internal_group_id = group->internal_group_id;

	qd->group_packets = g_list_append(qd->group_packets, p);
}

/* the main entry of group cmd processing, called by qq_recv_core.c */
void qq_process_group_cmd_reply(guint8 *buf, gint buf_len, guint16 seq, GaimConnection *gc)
{
	qq_group *group;
	qq_data *qd;
	gint len, bytes;
	guint32 internal_group_id;
	guint8 *data, *cursor, sub_cmd, reply;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	g_return_if_fail(buf != NULL && buf_len != 0);

	qd = (qq_data *) gc->proto_data;
	len = buf_len;
	data = g_newa(guint8, len);

	if (!qq_group_find_internal_group_id_by_seq(gc, seq, &internal_group_id)) {
		gaim_debug(GAIM_DEBUG_WARNING, "QQ", "We have no record of group cmd, seq [%d]\n", seq);
		return;
	}

	if (qq_crypt(DECRYPT, buf, buf_len, qd->session_key, data, &len)) {
		if (len <= 2) {
			gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Group cmd reply is too short, only %d bytes\n", len);
			return;
		}

		bytes = 0;
		cursor = data;
		bytes += read_packet_b(data, &cursor, len, &sub_cmd);
		bytes += read_packet_b(data, &cursor, len, &reply);

		group = qq_group_find_by_internal_group_id(gc, internal_group_id);

		if (reply != QQ_GROUP_CMD_REPLY_OK) {
			gaim_debug(GAIM_DEBUG_WARNING, "QQ",
				   "Group cmd reply says cmd %s fails\n", qq_group_cmd_get_desc(sub_cmd));
			switch (reply) {	/* this should be all errors */
			case QQ_GROUP_CMD_REPLY_NOT_MEMBER:
				if (group != NULL) {
					gaim_debug(GAIM_DEBUG_WARNING,
						   "QQ",
						   "You are not a member of group \"%s\"\n", group->group_name_utf8);
					group->my_status = QQ_GROUP_MEMBER_STATUS_NOT_MEMBER;
					qq_group_refresh(gc, group);
				}
				break;
			default:
				_qq_process_group_cmd_reply_error_default(reply, cursor, len - bytes, gc);
			}
			return;
		}

		/* seems to ok so far, so we process the reply according to sub_cmd */
		switch (sub_cmd) {
		case QQ_GROUP_CMD_GET_GROUP_INFO:
			qq_process_group_cmd_get_group_info(data, &cursor, len, gc);
			if (group != NULL) {
				qq_send_cmd_group_get_member_info(gc, group);
				qq_send_cmd_group_get_online_member(gc, group);
			}
			break;
		case QQ_GROUP_CMD_CREATE_GROUP:
			qq_group_process_create_group_reply(data, &cursor, len, gc);
			break;
		case QQ_GROUP_CMD_MODIFY_GROUP_INFO:
			qq_group_process_modify_info_reply(data, &cursor, len, gc);
			break;
		case QQ_GROUP_CMD_MEMBER_OPT:
			qq_group_process_modify_members_reply(data, &cursor, len, gc);
			break;
		case QQ_GROUP_CMD_ACTIVATE_GROUP:
			qq_group_process_activate_group_reply(data, &cursor, len, gc);
			break;
		case QQ_GROUP_CMD_SEARCH_GROUP:
			qq_process_group_cmd_search_group(data, &cursor, len, gc);
			break;
		case QQ_GROUP_CMD_JOIN_GROUP:
			qq_process_group_cmd_join_group(data, &cursor, len, gc);
			break;
		case QQ_GROUP_CMD_JOIN_GROUP_AUTH:
			qq_process_group_cmd_join_group_auth(data, &cursor, len, gc);
			break;
		case QQ_GROUP_CMD_EXIT_GROUP:
			qq_process_group_cmd_exit_group(data, &cursor, len, gc);
			break;
		case QQ_GROUP_CMD_SEND_MSG:
			qq_process_group_cmd_im(data, &cursor, len, gc);
			break;
		case QQ_GROUP_CMD_GET_ONLINE_MEMBER:
			qq_process_group_cmd_get_online_member(data, &cursor, len, gc);
			if (group != NULL)
				qq_group_conv_refresh_online_member(gc, group);
			break;
		case QQ_GROUP_CMD_GET_MEMBER_INFO:
			qq_process_group_cmd_get_member_info(data, &cursor, len, gc);
			if (group != NULL)
				qq_group_conv_refresh_online_member(gc, group);
			break;
		default:
			gaim_debug(GAIM_DEBUG_WARNING, "QQ",
				   "Group cmd %s is processed by default\n", qq_group_cmd_get_desc(sub_cmd));
			_qq_process_group_cmd_reply_default(data, &cursor, len, gc);
		}

	} else {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Error decrypt group cmd reply\n");
	}
}
