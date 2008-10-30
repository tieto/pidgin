/**
 * @file group_opt.h
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

#ifndef _QQ_GROUP_OPT_H_
#define _QQ_GROUP_OPT_H_

#include <glib.h>
#include "connection.h"
#include "group.h"

#define QQ_QUN_MEMBER_MAX       80	/* max number of the group */

typedef struct _qq_room_req {
	PurpleConnection *gc;
	guint32 id;
	guint32 member;
} qq_room_req;

enum {
	QQ_ROOM_TYPE_PERMANENT = 0x01,
	QQ_ROOM_TYPE_TEMPORARY
};

enum {
	QQ_ROOM_MEMBER_ADD = 0x01,
	QQ_ROOM_MEMBER_DEL
};

void qq_group_modify_members(PurpleConnection *gc, qq_room_data *rmd, guint32 *new_members);
void qq_room_change_info(PurpleConnection *gc, qq_room_data *rmd);

void qq_create_room(PurpleConnection *gc, const gchar *name);
void qq_group_process_modify_info_reply(guint8 *data, gint len, PurpleConnection *gc);
void qq_group_process_modify_members_reply(guint8 *data, gint len, PurpleConnection *gc);
void qq_group_manage_group(PurpleConnection *gc, GHashTable *data);
void qq_group_process_activate_group_reply(guint8 *data, gint len, PurpleConnection *gc);
void qq_group_process_create_group_reply(guint8 *data, gint len, PurpleConnection *gc);

void qq_process_room_buddy_request_join(guint8 *data, gint len, guint32 id, PurpleConnection *gc);
void qq_process_room_buddy_rejected(guint8 *data, gint len, guint32 id, PurpleConnection *gc);
void qq_process_room_buddy_approved(guint8 *data, gint len, guint32 id, PurpleConnection *gc);
void qq_process_room_buddy_removed(guint8 *data, gint len, guint32 id, PurpleConnection *gc);
void qq_process_room_buddy_joined(guint8 *data, gint len, guint32 id, PurpleConnection *gc);
#endif
