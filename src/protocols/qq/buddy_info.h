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

// START OF FILE
/*****************************************************************************/
#ifndef _QQ_BUDDY_INFO_H_
#define _QQ_BUDDY_INFO_H_

#include <glib.h>
#include "connection.h"		// GaimConnection
#include "buddy_opt.h"		// gc_and_uid
#include "qq.h"			// qq_data

#define QQ_COMM_FLAG_QQ_MEMBER      0x02
#define QQ_COMM_FLAG_TCP_MODE       0x10
#define QQ_COMM_FLAG_MOBILE         0x20
#define QQ_COMM_FLAG_BIND_MOBILE    0x40
#define QQ_COMM_FLAG_VIDEO          0x80

#define QQ_BUDDY_GENDER_GG          0x00
#define QQ_BUDDY_GENDER_MM          0x01
#define QQ_BUDDY_GENDER_UNKNOWN     0xff

typedef struct _contact_info {
	gchar *uid;		//0
	gchar *nick;		//1
	gchar *country;		//2 
	gchar *province;	//3
	gchar *zipcode;		//4
	gchar *address;		//5
	gchar *tel;		//6
	gchar *age;		//7
	gchar *gender;		//8
	gchar *name;		//9
	gchar *email;		//10
	gchar *pager_sn;	//11
	gchar *pager_num;	//12
	gchar *pager_sp;	//13
	gchar *pager_base_num;	//14
	gchar *pager_type;	//15
	gchar *occupation;	//16
	gchar *homepage;	//17
	gchar *auth_type;	//18
	gchar *unknown1;	//19
	gchar *unknown2;	//20
	gchar *face;		//21
	gchar *hp_num;		//22
	gchar *hp_type;		//23
	gchar *intro;		//24
	gchar *city;		//25
	gchar *unknown3;	//26
	gchar *unknown4;	//27
	gchar *unknown5;	//28
	gchar *is_open_hp;	//29
	gchar *is_open_contact;	//30
	gchar *college;		//31
	gchar *horoscope;	//32
	gchar *zodiac;		//33 sheng xiao
	gchar *blood;		//34
	gchar *qq_show;		//35
	gchar *unknown6;	//36, always 0x2D
} contact_info;

// There is no user id stored in the reply packet for information query
// we have to manually store the query, so that we know the query source
typedef struct _qq_info_query {
	guint32 uid;
	gboolean show_window;
	gboolean modify_info;
} qq_info_query;

// We get an info packet on ourselves before we modify our information.
// Even though not all of the information is currently modifiable, it still
// all needs to be there when we send out the modify info packet
typedef struct _modify_info_data {
	GaimConnection *gc;
	GList *misc, *node;
} modify_info_data;

#define QQ_CONTACT_FIELDS 				37

#define QQ_MAIN_INFO		"Primary Information"
#define QQ_EXTRA_INFO		"Detailed Information"
#define QQ_PERSONAL_INTRO	"Personal Introduction"
#define QQ_MISC			"Miscellaneous"

#define QQ_NO_CHOICE		0
#define QQ_HOROSCOPE		 1
#define QQ_ZODIAC		 2
#define QQ_BLOOD		 3
#define QQ_GENDER		 4
#define QQ_COUNTRY		 5
#define QQ_PROVINCE		 6
#define QQ_OCCUPATION		 7

void qq_refresh_buddy_and_myself(contact_info *info, GaimConnection *gc);
void qq_send_packet_get_info(GaimConnection *gc, guint32 uid, gboolean show_window);
void qq_send_packet_modify_info(GaimConnection *gc, contact_info *info);
void qq_prepare_modify_info(GaimConnection *gc);
void qq_process_modify_info_reply(guint8 *buf, gint buf_len, GaimConnection *gc);
void qq_process_get_info_reply(guint8 *buf, gint buf_len, GaimConnection *gc);
void qq_info_query_free(qq_data *qd);

#endif
