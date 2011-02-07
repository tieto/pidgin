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

#define QQ_ROOM_KEY_INTERNAL_ID					"id"
#define QQ_ROOM_KEY_EXTERNAL_ID					"ext_id"
#define QQ_ROOM_KEY_TITLE_UTF8					"title_utf8"

PurpleChat *qq_room_find_or_new(PurpleConnection *gc, guint32 id, guint32 ext_id);
void qq_room_remove(PurpleConnection *gc, guint32 id);
void qq_room_update_chat_info(PurpleChat *chat, qq_room_data *rmd);

qq_buddy_data *qq_room_buddy_find(qq_room_data *rmd, UID uid);
void qq_room_buddy_remove(qq_room_data *rmd, UID uid);
qq_buddy_data *qq_room_buddy_find_or_new(PurpleConnection *gc, qq_room_data *rmd, UID member_uid);

void qq_room_data_initial(PurpleConnection *gc);
void qq_room_data_free_all(PurpleConnection *gc);
qq_room_data *qq_room_data_find(PurpleConnection *gc, guint32 room_id);

guint32 qq_room_get_next(PurpleConnection *gc, guint32 room_id);
guint32 qq_room_get_next_conv(PurpleConnection *gc, guint32 room_id);

#endif
