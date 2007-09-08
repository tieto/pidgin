/**
 * @file qq.h
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

#ifndef _QQ_QQ_H_
#define _QQ_QQ_H_

#include <glib.h>
#include "internal.h"
#include "ft.h"
#include "proxy.h"
#include "roomlist.h"

#define QQ_FACES	    100
#define QQ_KEY_LENGTH       16
#define QQ_DEBUG            1	/* whether we are doing DEBUG */

#ifdef _WIN32
const char *qq_win32_buddy_icon_dir(void);
#define QQ_BUDDY_ICON_DIR qq_win32_buddy_icon_dir()
#endif

typedef struct _qq_data qq_data;
typedef struct _qq_buddy qq_buddy;

struct _qq_buddy {
	guint32 uid;
	guint16 face;		/* index: 0 - 299 */
	guint8 age;
	guint8 gender;
	gchar *nickname;
	guint8 ip[4];
	guint16 port;
	guint8 status;
	guint8 flag1;
	guint8 comm_flag;	/* details in qq_buddy_list.c */
	guint16 client_version;
	guint8 onlineTime;
	guint16 level;
	guint16 timeRemainder;
	time_t signon;
	time_t idle;
	time_t last_refresh;

	gint8  role;		/* role in group, used only in group->members list */
};

struct _qq_data {
	gint fd;			/* socket file handler */
	guint32 uid;			/* QQ number */
	guint8 *inikey;			/* initial key to encrypt login packet */
	guint8 *pwkey;			/* password in md5 (or md5' md5) */
	guint8 *session_key;		/* later use this as key in this session */
	guint8 *session_md5;		/* concatenate my uid with session_key and md5 it */

	guint16 send_seq;		/* send sequence number */
	guint8 login_mode;		/* online of invisible */
	gboolean logged_in;		/* used by qq-add_buddy */
	gboolean use_tcp;		/* network in tcp or udp */

	PurpleProxyType proxy_type;
	PurpleConnection *gc;

	PurpleXfer *xfer;			/* file transfer handler */
	struct sockaddr_in dest_sin;

	/* from real connction */
	gchar *server_ip;
	guint16 server_port;
	/* get from login reply packet */
	time_t login_time;
	time_t last_login_time;
	gchar *last_login_ip;
	/* get from keep_alive packet */
	gchar *my_ip;			/* my ip address detected by server */
	guint16 my_port;		/* my port detected by server */
	guint16 my_icon;		/* my icon index */
	guint16 my_level;		/* my level */
	guint32 all_online;		/* the number of online QQ users */
	time_t last_get_online;		/* last time send get_friends_online packet */

	guint8 window[1 << 13];		/* check up for duplicated packet */
	gint sendqueue_timeout;

	PurpleRoomlist *roomlist;
	gint channel;			/* the id for opened chat conversation */

	GList *groups;
	GList *group_packets;
	GSList *joining_groups;
	GSList *adding_groups_from_server; /* internal ids of groups the server wants in my blist */
	GList *buddies;
	GList *contact_info_window;
	GList *group_info_window;
	GList *sendqueue;
	GList *info_query;
	GList *add_buddy_request;
	GQueue *before_login_packets;

	/* TODO pass qq_send_packet_get_info() a callback and use signals to get rid of these */
	gboolean modifying_info;
	gboolean modifying_face;
};

void qq_function_not_implemented(PurpleConnection *gc);

#endif
