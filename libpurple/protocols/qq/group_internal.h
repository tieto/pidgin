/**
 * @file group_internal.h
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

#ifndef _QQ_GROUP_HASH_H_
#define _QQ_GROUP_HASH_H_

#include <glib.h>
#include "group.h"

#define QQ_ROOM_KEY_ROLE									"my_role"
#define QQ_ROOM_KEY_ROLE_DESC						"my_role_desc"
#define QQ_ROOM_KEY_INTERNAL_ID					"id"
#define QQ_ROOM_KEY_EXTERNAL_ID					"ext_id"
#define QQ_ROOM_KEY_TYPE									"type"
#define QQ_ROOM_KEY_CREATOR_UID					"creator_uid"
#define QQ_ROOM_KEY_CATEGORY							"category"
#define QQ_ROOM_KEY_AUTH_TYPE						"auth_type"
#define QQ_ROOM_KEY_TITLE_UTF8						"title_utf8"
#define QQ_ROOM_KEY_DESC_UTF8						"desc_utf8"

qq_group *qq_group_create_internal_record(PurpleConnection *gc,
		guint32 internal_id, guint32 ext_id, gchar *group_name_utf8);
void qq_group_delete_internal_record(qq_data *qd, guint32 id);

GHashTable *qq_group_to_hashtable(qq_group *group);
qq_group *qq_room_data_new_by_hashtable(PurpleConnection *gc, GHashTable *data);

void qq_group_refresh(PurpleConnection *gc, qq_group *group);

#endif
