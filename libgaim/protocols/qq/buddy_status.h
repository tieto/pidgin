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
 *
 */

#ifndef _QQ_BUDDY_STATUS_H_
#define _QQ_BUDDY_STATUS_H_

#include <glib.h>
#include "connection.h"
#include "qq.h"

typedef struct _qq_buddy_status {
	guint32 uid;
	guint8 unknown1;
	guint8 *ip;
	guint16 port;
	guint8 unknown2;
	guint8 status;
	guint16 client_version;
	guint8 *unknown_key;
} qq_buddy_status;

enum {
	QQ_BUDDY_OFFLINE = 0x00,
	QQ_BUDDY_ONLINE_NORMAL = 0x0a,
	QQ_BUDDY_ONLINE_OFFLINE = 0x14,
	QQ_BUDDY_ONLINE_AWAY = 0x1e,
	QQ_BUDDY_ONLINE_INVISIBLE = 0x28
};

enum {
	QQ_SELF_STATUS_AVAILABLE = 0x11,
	QQ_SELF_STATUS_AWAY = 0x12,
	QQ_SELF_STATUS_INVISIBLE = 0x13,
	QQ_SELF_STATUS_CUSTOM = 0x14,
	QQ_SELF_STATUS_IDLE = 0x15
};
 
void qq_buddy_status_dump_unclear(qq_buddy_status *s);
gboolean is_online(guint8 status);

gint qq_buddy_status_read(guint8 *data, guint8 **cursor, gint len, qq_buddy_status *s);
gchar get_suffix_from_status(guint8 status);

void qq_send_packet_change_status(GaimConnection *gc);

void qq_process_change_status_reply(guint8 *buf, gint buf_len, GaimConnection *gc);
void qq_process_friend_change_status(guint8 *buf, gint buf_len, GaimConnection *gc);
#endif
