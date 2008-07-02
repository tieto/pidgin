/**
 * @file buddy_info.h
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

#ifndef _QQ_BUDDY_INFO_H_
#define _QQ_BUDDY_INFO_H_

#include <glib.h>
#include "connection.h"

#include "buddy_opt.h"
#include "qq.h"

#define QQ_COMM_FLAG_QQ_MEMBER      0x02
#define QQ_COMM_FLAG_TCP_MODE       0x10
#define QQ_COMM_FLAG_MOBILE         0x20
#define QQ_COMM_FLAG_BIND_MOBILE    0x40
#define QQ_COMM_FLAG_VIDEO          0x80

#define QQ_BUDDY_GENDER_GG          0x00
#define QQ_BUDDY_GENDER_MM          0x01
#define QQ_BUDDY_GENDER_UNKNOWN     0xff

#define QQ_ICON_PREFIX "qq_"
#define QQ_ICON_SUFFIX ".png"

void qq_send_packet_get_info(PurpleConnection *gc, guint32 uid, gboolean show_window);
void qq_set_my_buddy_icon(PurpleConnection *gc, PurpleStoredImage *img);
void qq_set_buddy_icon_for_user(PurpleAccount *account, const gchar *who, const gchar *icon_num, const gchar *iconfile);
void qq_prepare_modify_info(PurpleConnection *gc);
void qq_process_modify_info_reply(guint8 *buf, gint buf_len, PurpleConnection *gc);
void qq_process_get_info_reply(guint8 *buf, gint buf_len, PurpleConnection *gc);
void qq_info_query_free(qq_data *qd);
void qq_send_packet_get_level(PurpleConnection *gc, guint32 uid);
void qq_send_packet_get_buddies_levels(PurpleConnection *gc);
void qq_process_get_level_reply(guint8 *buf, gint buf_len, PurpleConnection *gc);

#endif
