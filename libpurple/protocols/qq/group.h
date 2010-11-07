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
	QQ_GROUP_MEMBER_STATUS_NOT_MEMBER = 0x00,	/* default 0x00 means not member */
	QQ_GROUP_MEMBER_STATUS_IS_MEMBER,
	QQ_GROUP_MEMBER_STATUS_APPLYING,
	QQ_GROUP_MEMBER_STATUS_IS_ADMIN,
} qq_group_member_status;

typedef struct _qq_group {
	/* all these will be saved when we exit Purple */
	qq_group_member_status my_status;	/* my status for this group */
	gchar *my_status_desc;			/* my status description */
	guint32 internal_group_id;
	guint32 external_group_id;
	guint8 group_type;			/* permanent or temporory */
	guint32 creator_uid;
	guint32 group_category;
	guint8 auth_type;
	gchar *group_name_utf8;
	gchar *group_desc_utf8;
	/* all these will be loaded from the network */
	gchar *notice_utf8;	/* group notice by admin */
	GList *members;	
} qq_group;

GList *qq_chat_info(PurpleConnection *gc);
GHashTable *qq_chat_info_defaults(PurpleConnection *gc, const gchar *chat_name);

void qq_group_init(PurpleConnection *gc);

PurpleRoomlist *qq_roomlist_get_list(PurpleConnection *gc);

void qq_roomlist_cancel(PurpleRoomlist *list);

#endif
