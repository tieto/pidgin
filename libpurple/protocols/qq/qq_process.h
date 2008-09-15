/**
 * @file qq_process.h
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

#ifndef _QQ_PROCESS_H
#define _QQ_PROCESS_H

#include <glib.h>
#include "connection.h"

#include "qq.h"

void qq_proc_cmd_reply(PurpleConnection *gc,
		guint16 cmd, guint16 seq, guint8 *rcved, gint rcved_len);
void qq_proc_room_cmd_reply(PurpleConnection *gc,
	guint16 seq, guint8 room_cmd, guint32 room_id, guint8 *rcved, gint rcved_len);
	
void qq_proc_cmd_server(PurpleConnection *gc,
	guint16 cmd, guint16 seq, guint8 *rcved, gint rcved_len);
#endif

