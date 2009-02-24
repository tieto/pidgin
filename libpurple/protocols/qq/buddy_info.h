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

/* use in qq2005
 * ext_flag: (0-7)
 *        bit1 => qq space
 * comm_flag: (0-7)
 *        bit1 => member
 *        bit4 => TCP mode
 *        bit5 => open mobile QQ
 *        bit6 => bind to mobile
 *        bit7 => whether having a video
#define QQ_COMM_FLAG_QQ_MEMBER		0x02
#define QQ_COMM_FLAG_TCP_MODE    	0x10
#define QQ_COMM_FLAG_MOBILE       	0x20
#define QQ_COMM_FLAG_BIND_MOBILE	0x40
#define QQ_COMM_FLAG_VIDEO          	0x80
 */
/* status in eva for qq2006
#define QQ_FRIEND_FLAG_QQ_MEMBER  0x01
#define QQ_FRIEND_FLAG_MOBILE           0x10
#define QQ_FRIEND_FLAG_BIND_MOBILE  0x20
*/
#define QQ_COMM_FLAG_QQ_VIP			0x02
#define QQ_COMM_FLAG_QQ_MEMBER		0x04
#define QQ_COMM_FLAG_TCP_MODE    	0x10
#define QQ_COMM_FLAG_MOBILE       	0x20
#define QQ_COMM_FLAG_BIND_MOBILE	0x40
#define QQ_COMM_FLAG_VIDEO          	0x80

#define QQ_EXT_FLAG_ZONE				0x02

#define QQ_BUDDY_GENDER_GG          0x00
#define QQ_BUDDY_GENDER_MM          0x01
#define QQ_BUDDY_GENDER_UNKNOWN     0xff

enum {
	QQ_BUDDY_INFO_UPDATE_ONLY = 0,
	QQ_BUDDY_INFO_DISPLAY,
	QQ_BUDDY_INFO_SET_ICON,
	QQ_BUDDY_INFO_MODIFY_BASE,
	QQ_BUDDY_INFO_MODIFY_EXT,
	QQ_BUDDY_INFO_MODIFY_ADDR,
	QQ_BUDDY_INFO_MODIFY_CONTACT
};

gchar *qq_get_icon_name(gint face);
gchar *qq_get_icon_path(gchar *icon_name);
void qq_change_icon_cb(PurpleConnection *gc, const char *filepath);

void qq_request_buddy_info(PurpleConnection *gc, guint32 uid,
		guint32 update_class, int action);
void qq_set_custom_icon(PurpleConnection *gc, PurpleStoredImage *img);
void qq_process_change_info(PurpleConnection *gc, guint8 *data, gint data_len);
void qq_process_get_buddy_info(guint8 *data, gint data_len, guint32 action, PurpleConnection *gc);

void qq_request_get_level(PurpleConnection *gc, guint32 uid);
void qq_request_get_level_2007(PurpleConnection *gc, guint32 uid);
void qq_request_get_buddies_level(PurpleConnection *gc, guint32 update_class);
void qq_process_get_level_reply(guint8 *buf, gint buf_len, PurpleConnection *gc);

void qq_update_buddy_icon(PurpleAccount *account, const gchar *who, gint face);
#endif
