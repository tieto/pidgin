/**
 * @file buddy_info.h
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

typedef struct _contact_info {
        gchar *uid;
        gchar *nick;
        gchar *country;
        gchar *province;
        gchar *zipcode;
        gchar *address;
        gchar *tel;
        gchar *age;
        gchar *gender;
        gchar *name;
        gchar *email;
        gchar *pager_sn;
        gchar *pager_num;
        gchar *pager_sp;
        gchar *pager_base_num;
        gchar *pager_type;
        gchar *occupation;
        gchar *homepage;
        gchar *auth_type;
        gchar *unknown1;
        gchar *unknown2;
        gchar *face;
        gchar *hp_num;
        gchar *hp_type;
        gchar *intro;
        gchar *city;
        gchar *unknown3;
        gchar *unknown4;
        gchar *unknown5;
        gchar *is_open_hp;
        gchar *is_open_contact;
        gchar *college;
        gchar *horoscope;
        gchar *zodiac;
        gchar *blood;
        gchar *qq_show;
        gchar *unknown6;        /* always 0x2D */
} contact_info;

void qq_refresh_buddy_and_myself(contact_info *info, GaimConnection *gc);
void qq_send_packet_get_info(GaimConnection *gc, guint32 uid, gboolean show_window);
void qq_set_my_buddy_icon(GaimConnection *gc, const gchar *iconfile);
void qq_set_buddy_icon_for_user(GaimAccount *account, const gchar *who, const gchar *iconfile);
void qq_prepare_modify_info(GaimConnection *gc);
void qq_process_modify_info_reply(guint8 *buf, gint buf_len, GaimConnection *gc);
void qq_process_get_info_reply(guint8 *buf, gint buf_len, GaimConnection *gc);
void qq_info_query_free(qq_data *qd);
void qq_send_packet_get_level(GaimConnection *gc, guint32 uid);
void qq_send_packet_get_buddies_levels(GaimConnection *gc);
void qq_process_get_level_reply(guint8 *buf, gint buf_len, GaimConnection *gc);

#endif
