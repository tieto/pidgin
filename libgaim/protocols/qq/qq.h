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

#ifndef _QQ_QQ_H_
#define _QQ_QQ_H_

#include <glib.h>
#include "ft.h"
#include "internal.h"
#include "proxy.h"
#include "roomlist.h"

#define QQ_FACES	    100
#define QQ_KEY_LENGTH       16
#define QQ_DEBUG            1	/* whether we are doing DEBUG */

typedef struct _qq_data qq_data;
typedef struct _qq_buddy qq_buddy;

struct _qq_buddy {
	guint32 uid;
	guint16 icon;		/* index: 01 - 85 */
	guint8 age;
	guint8 gender;
	gchar *nickname;
	guint8 ip[4];
	guint16 port;
	guint8 status;
	guint8 flag1;
	guint8 comm_flag;	/* details in qq_buddy_list.c */
	guint16 client_version;
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

	guint16 send_seq;		/* send sequence number */
	guint8 login_mode;		/* online of invisible */
	gboolean logged_in;		/* used by qq-add_buddy */
	gboolean use_tcp;		/* network in tcp or udp */

	GaimProxyType proxy_type;
	GaimConnection *gc;

	GaimXfer *xfer;			/* file transfer handler */
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
	guint16 my_icon;			/* my icon index */
	guint32 all_online;		/* the number of online QQ users */
	time_t last_get_online;		/* last time send get_friends_online packet */

	guint8 window[1 << 13];		/* check up for duplicated packet */
	gint sendqueue_timeout;

	GaimRoomlist *roomlist;
	gint channel;			/* the id for opened chat conversation */

	GList *groups;
	GList *group_packets;
	GList *buddies;
	GList *contact_info_window;
	GList *qun_info_window;
	GList *sendqueue;
	GList *info_query;
	GList *add_buddy_request;
	GQueue *before_login_packets;

	/* TODO is there a better way of handling these? */
	gboolean modifying_info;
	gboolean modifying_face;
};

void qq_function_not_implemented(GaimConnection *gc);

#endif
