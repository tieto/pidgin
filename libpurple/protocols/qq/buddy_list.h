/**
 * @file buddy_list.h
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

#ifndef _QQ_BUDDY_LIST_H_
#define _QQ_BUDDY_LIST_H_

#include <glib.h>
#include "connection.h"

#include "qq.h"
typedef struct _qq_buddy_status {
	guint32 uid;
	guint8 unknown1;
	struct in_addr ip;
	guint16 port;
	guint8 unknown2;
	guint8 status;
	guint16 unknown3;
	guint8 unknown_key[QQ_KEY_LENGTH];
} qq_buddy_status;

enum {
	QQ_BUDDY_OFFLINE = 0x00,
	QQ_BUDDY_ONLINE_NORMAL = 0x0a,
	QQ_BUDDY_ONLINE_OFFLINE = 0x14,
	QQ_BUDDY_ONLINE_AWAY = 0x1e,
	QQ_BUDDY_ONLINE_INVISIBLE = 0x28
};

void qq_send_packet_get_buddies_online(PurpleConnection *gc, guint8 position);
guint8 qq_process_get_buddies_online_reply(guint8 *data, gint data_len, PurpleConnection *gc);

void qq_send_packet_get_buddies_list(PurpleConnection *gc, guint16 position);
guint16 qq_process_get_buddies_list_reply(guint8 *data, gint data_len, PurpleConnection *gc);

void qq_send_packet_get_buddies_and_rooms(PurpleConnection *gc, guint32 position);
guint32 qq_process_get_buddies_and_rooms(guint8 *data, gint data_len, PurpleConnection *gc);

void qq_refresh_all_buddy_status(PurpleConnection *gc);

gboolean is_online(guint8 status);

gint get_icon_offset(PurpleConnection *gc);

void qq_send_packet_change_status(PurpleConnection *gc);
void qq_process_change_status_reply(guint8 *data, gint data_len, PurpleConnection *gc);
void qq_process_buddy_change_status(guint8 *data, gint data_len, PurpleConnection *gc);

void qq_refresh_all_buddy_status(PurpleConnection *gc);
void qq_update_buddy_contact(PurpleConnection *gc, qq_buddy *q_bud);
#endif
