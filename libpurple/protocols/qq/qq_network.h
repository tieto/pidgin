/**
 * @file qq_network.h
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

#ifndef _QQ_NETWORK_H
#define _QQ_NETWORK_H

#include <glib.h>
#include "connection.h"

#include "qq.h"

#define QQ_CONNECT_STEPS    4	/* steps in connection */

gboolean qq_connect_later(gpointer data);
void qq_disconnect(PurpleConnection *gc);

gint qq_send_cmd_encrypted(PurpleConnection *gc, guint16 cmd, guint16 seq,
		guint8 *encrypted_data, gint encrypted_len, gboolean is_save2trans);
gint qq_send_cmd(PurpleConnection *gc, guint16 cmd, guint8 *data, gint datalen);
gint qq_send_cmd_mess(PurpleConnection *gc, guint16 cmd, guint8 *data, gint data_len,
		gint update_class, guint32 ship32);

gint qq_send_server_reply(PurpleConnection *gc, guint16 cmd, guint16 seq,
		guint8 *data, gint data_len);

gint qq_send_room_cmd(PurpleConnection *gc, guint8 room_cmd, guint32 room_id,
		guint8 *data, gint data_len);
gint qq_send_room_cmd_mess(PurpleConnection *gc, guint8 room_cmd, guint32 room_id,
		guint8 *data, gint data_len, gint update_class, guint32 ship32);
gint qq_send_room_cmd_only(PurpleConnection *gc, guint8 room_cmd, guint32 room_id);
gint qq_send_room_cmd_noid(PurpleConnection *gc, guint8 room_cmd,
		guint8 *data, gint data_len);
#endif
