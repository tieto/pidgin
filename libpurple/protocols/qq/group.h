/**
 * @file group.h
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

#ifndef _QQ_GROUP_H_
#define _QQ_GROUP_H_

#include <glib.h>
#include "account.h"
#include "connection.h"
#include "roomlist.h"
#include "qq.h"

#define PURPLE_GROUP_QQ_QUN         "QQ 群"

typedef enum {
	QQ_ROOM_ROLE_NO = 0x00,	/* default 0x00 means not member */
	QQ_ROOM_ROLE_YES,
	QQ_ROOM_ROLE_REQUESTING,
	QQ_ROOM_ROLE_ADMIN
} qq_room_role;

typedef struct _qq_room_data {
	/* all these will be saved when we exit Purple */
	qq_room_role my_role;	/* my role for this room */
	guint32 id;
	guint32 ext_id;
	guint8 type8;			/* permanent or temporory */
	UID creator_uid;
	guint32 category;
	guint8 auth_type;
	gchar *title_utf8;
	gchar *desc_utf8;
	/* all these will be loaded from the network */
	gchar *notice_utf8;	/* group notice by admin */

	gboolean is_got_buddies;
	GList *members;
} qq_room_data;

GList *qq_chat_info(PurpleConnection *gc);
GHashTable *qq_chat_info_defaults(PurpleConnection *gc, const gchar *chat_name);

PurpleRoomlist *qq_roomlist_get_list(PurpleConnection *gc);

void qq_roomlist_cancel(PurpleRoomlist *list);

#endif
