/**
 * @file group_join.h
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

#ifndef _QQ_GROUP_JOIN_H_
#define _QQ_GROUP_JOIN_H_

#include <glib.h>
#include "connection.h"
#include "group.h"

enum {
	QQ_ROOM_AUTH_TYPE_NO_AUTH = 0x01,
	QQ_ROOM_AUTH_TYPE_NEED_AUTH = 0x02,
	QQ_ROOM_AUTH_TYPE_NO_ADD = 0x03
};

enum {
	QQ_ROOM_AUTH_REQUEST_APPLY = 0x01,
	QQ_ROOM_AUTH_REQUEST_APPROVE = 0x02,
	QQ_ROOM_AUTH_REQUEST_REJECT = 0x03
};

enum {
	QQ_ROOM_SEARCH_ONLY = 0,
	QQ_ROOM_SEARCH_FOR_JOIN
};

void qq_request_room_search(PurpleConnection *gc, guint32 ext_id, int action);
void qq_process_room_search(PurpleConnection *gc, guint8 *data, gint len, guint32 ship32);

void qq_send_cmd_group_auth(PurpleConnection *gc, qq_room_data *rmd, guint8 opt, UID uid, const gchar *reason_utf8);
void qq_group_join(PurpleConnection *gc, GHashTable *data);
void qq_request_room_join(PurpleConnection *gc, qq_room_data *rmd);
void qq_room_quit(PurpleConnection *gc, guint32 room_id);
void qq_process_group_cmd_exit_group(guint8 *data, gint len, PurpleConnection *gc);
void qq_process_group_cmd_join_group_auth(guint8 *data, gint len, PurpleConnection *gc);
void qq_process_group_cmd_join_group(guint8 *data, gint len, PurpleConnection *gc);
#endif

