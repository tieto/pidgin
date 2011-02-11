/**
 * @file group_info.h
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

#ifndef _QQ_GROUP_INFO_H_
#define _QQ_GROUP_INFO_H_

#include <glib.h>
#include "connection.h"
#include "group.h"

enum {
	QQ_ROOM_INFO_UPDATE_ONLY = 0,
	QQ_ROOM_INFO_DISPLAY
};

gint qq_request_room_get_buddies(PurpleConnection *gc, guint32 room_id, UPDCLS update_class);

void qq_process_room_cmd_get_info(guint8 *data, gint len, guint32 action, PurpleConnection *gc);
void qq_process_room_cmd_get_onlines(guint8 *data, gint len, PurpleConnection *gc);
void qq_process_room_cmd_get_buddies(guint8 *data, gint len, PurpleConnection *gc);
#endif
