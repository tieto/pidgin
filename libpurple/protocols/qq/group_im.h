/**
 * @file group_im.h
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

#ifndef _QQ_GROUP_IM_H_
#define _QQ_GROUP_IM_H_

#include <glib.h>
#include "connection.h"
#include "conversation.h"
#include "group.h"

PurpleConversation *qq_room_conv_open(PurpleConnection *gc, qq_room_data *rmd);
void qq_room_conv_set_onlines(PurpleConnection *gc, qq_room_data *rmd);

void qq_room_got_chat_in(PurpleConnection *gc,
		guint32 room_id, guint32 uid_from, const gchar *msg, time_t in_time);

int qq_chat_send(PurpleConnection *gc, int id, const char *message, PurpleMessageFlags flags);
void qq_process_room_send_im(PurpleConnection *gc, guint8 *data, gint len);
void qq_process_room_send_im_ex(PurpleConnection *gc, guint8 *data, gint len);

void qq_process_room_im(guint8 *data, gint data_len, guint32 id, PurpleConnection *gc, guint16 msg_type);

#endif
