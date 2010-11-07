/**
 * @file group_network.h
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

#ifndef _QQ_GROUP_NETWORK_H_
#define _QQ_GROUP_NETWORK_H_

#include <glib.h>
#include "connection.h"
#include "group.h"
#include "packet_parse.h"

typedef enum {
	QQ_GROUP_CMD_CREATE_GROUP = 0x01,
	QQ_GROUP_CMD_MEMBER_OPT = 0x02,
	QQ_GROUP_CMD_MODIFY_GROUP_INFO = 0x03,
	QQ_GROUP_CMD_GET_GROUP_INFO = 0x04,
	QQ_GROUP_CMD_ACTIVATE_GROUP = 0x05,
	QQ_GROUP_CMD_SEARCH_GROUP = 0x06,
	QQ_GROUP_CMD_JOIN_GROUP = 0x07,
	QQ_GROUP_CMD_JOIN_GROUP_AUTH = 0x08,
	QQ_GROUP_CMD_EXIT_GROUP = 0x09,
	QQ_GROUP_CMD_SEND_MSG = 0x0a,
	QQ_GROUP_CMD_GET_ONLINE_MEMBER = 0x0b,
	QQ_GROUP_CMD_GET_MEMBER_INFO = 0x0c
} qq_group_cmd;

typedef struct _group_packet {
	guint16 send_seq;
	guint32 internal_group_id;
} group_packet;

const gchar *qq_group_cmd_get_desc(qq_group_cmd cmd);

void qq_send_group_cmd(PurpleConnection *gc, qq_group *group, guint8 *raw_data, gint data_len);
void qq_process_group_cmd_reply(guint8 *buf, gint buf_len, guint16 seq, PurpleConnection *gc);

#endif
