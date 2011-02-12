/**
 * @file qq_trans.h
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

#ifndef _QQ_SEND_QUEUE_H_
#define _QQ_SEND_QUEUE_H_

#include <glib.h>
#include "qq.h"

typedef struct _qq_transaction {
	guint8 flag;
	guint16 seq;
	guint16 cmd;

	guint8 room_cmd;
	guint32 room_id;

	guint8 *data;
	gint data_len;

	gint fd;
	gint send_retries;
	gint rcved_times;
	gint scan_times;

	UPDCLS update_class;
	guint32 ship32;
} qq_transaction;

qq_transaction *qq_trans_find_rcved(PurpleConnection *gc, guint16 cmd, guint16 seq);
gboolean qq_trans_is_server(qq_transaction *trans) ;
gboolean qq_trans_is_dup(qq_transaction *trans);
guint8 qq_trans_get_room_cmd(qq_transaction *trans);
guint32 qq_trans_get_room_id(qq_transaction *trans);
guint32 qq_trans_get_class(qq_transaction *trans);
guint32 qq_trans_get_ship(qq_transaction *trans);

void qq_trans_add_client_cmd(PurpleConnection *gc, guint16 cmd, guint16 seq,
		guint8 *data, gint data_len, UPDCLS update_class, guint32 ship32);
void qq_trans_add_room_cmd(PurpleConnection *gc,
		guint16 seq, guint8 room_cmd, guint32 room_id,
		guint8 *data, gint data_len, UPDCLS update_class, guint32 ship32);
void qq_trans_add_server_cmd(PurpleConnection *gc, guint16 cmd, guint16 seq,
	guint8 *rcved, gint rcved_len);
void qq_trans_add_server_reply(PurpleConnection *gc, guint16 cmd, guint16 seq,
		guint8 *reply, gint reply_len);
void qq_trans_add_remain(PurpleConnection *gc, guint16 cmd, guint16 seq,
	guint8 *data, gint data_len);

void qq_trans_process_remained(PurpleConnection *gc);
gboolean qq_trans_scan(PurpleConnection *gc);
void qq_trans_remove_all(PurpleConnection *gc);

#endif
