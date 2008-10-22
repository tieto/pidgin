/**
 * @file group_search.c
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

#include "internal.h"

#include "debug.h"

#include "char_conv.h"
#include "group_find.h"
#include "group_free.h"
#include "group_internal.h"
#include "group_join.h"
#include "group_search.h"
#include "utils.h"
#include "qq_define.h"
#include "packet_parse.h"
#include "qq_network.h"

enum {
	QQ_ROOM_SEARCH_TYPE_BY_ID = 0x01,
	QQ_ROOM_SEARCH_TYPE_DEMO = 0x02
};

/* send packet to search for qq_group */
void qq_request_room_search(PurpleConnection *gc, guint32 ext_id, int action)
{
	guint8 raw_data[16] = {0};
	gint bytes = 0;
	guint8 type;

	purple_debug_info("QQ", "Search QQ Qun %d\n", ext_id);
	type = (ext_id == 0x00000000) ? QQ_ROOM_SEARCH_TYPE_DEMO : QQ_ROOM_SEARCH_TYPE_BY_ID;

	bytes = 0;
	bytes += qq_put8(raw_data + bytes, type);
	bytes += qq_put32(raw_data + bytes, ext_id);

	qq_send_room_cmd_mess(gc, QQ_ROOM_CMD_SEARCH, 0, raw_data, bytes, 0, action);
}

static void add_to_roomlist(qq_data *qd, qq_group *group)
{
	PurpleRoomlistRoom *room;
	gchar field[11];

	room = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_ROOM, group->title_utf8, NULL);
	g_snprintf(field, sizeof(field), "%d", group->ext_id);
	purple_roomlist_room_add_field(qd->roomlist, room, field);
	g_snprintf(field, sizeof(field), "%d", group->creator_uid);
	purple_roomlist_room_add_field(qd->roomlist, room, field);
	purple_roomlist_room_add_field(qd->roomlist, room, group->desc_utf8);
	g_snprintf(field, sizeof(field), "%d", group->id);
	purple_roomlist_room_add_field(qd->roomlist, room, field);
	g_snprintf(field, sizeof(field), "%d", group->type8);
	purple_roomlist_room_add_field(qd->roomlist, room, field);
	g_snprintf(field, sizeof(field), "%d", group->auth_type);
	purple_roomlist_room_add_field(qd->roomlist, room, field);
	g_snprintf(field, sizeof(field), "%d", group->category);
	purple_roomlist_room_add_field(qd->roomlist, room, field);
	purple_roomlist_room_add_field(qd->roomlist, room, group->title_utf8);
	purple_roomlist_room_add(qd->roomlist, room);

	purple_roomlist_set_in_progress(qd->roomlist, FALSE);
}

/* process group cmd reply "search group" */
void qq_process_room_search(PurpleConnection *gc, guint8 *data, gint len, guint32 ship32)
{
	gint bytes;
	guint8 search_type;
	guint16 unknown;
	qq_group group;
	qq_data *qd;

	g_return_if_fail(data != NULL && len > 0);
	qd = (qq_data *) gc->proto_data;

	bytes = 0;
	bytes += qq_get8(&search_type, data + bytes);

	/* now it starts with group_info_entry */
	bytes += qq_get32(&(group.id), data + bytes);
	bytes += qq_get32(&(group.ext_id), data + bytes);
	bytes += qq_get8(&(group.type8), data + bytes);
	bytes += qq_get16(&(unknown), data + bytes);
	bytes += qq_get16(&(unknown), data + bytes);
	bytes += qq_get32(&(group.creator_uid), data + bytes);
	bytes += qq_get16(&(unknown), data + bytes);
	bytes += qq_get16(&(unknown), data + bytes);
	bytes += qq_get16(&(unknown), data + bytes);
	bytes += qq_get32(&(group.category), data + bytes);
	bytes += qq_get_vstr(&(group.title_utf8), QQ_CHARSET_DEFAULT, data + bytes);
	bytes += qq_get16(&(unknown), data + bytes);
	bytes += qq_get8(&(group.auth_type), data + bytes);
	bytes += qq_get_vstr(&(group.desc_utf8), QQ_CHARSET_DEFAULT, data + bytes);
	/* end of one qq_group */
	if(bytes != len) {
		purple_debug_error("QQ",
			"group_cmd_search_group: Dangerous error! maybe protocol changed, notify developers!");
	}

	if (ship32 == QQ_ROOM_SEARCH_FOR_JOIN) {
		if (qq_room_search_id(gc, group.id) == NULL)
			qq_group_create_internal_record(gc,
					group.id, group.ext_id, group.title_utf8);
		qq_request_room_join(gc, &group);
	} else {
		add_to_roomlist(qd, &group);
	}
}
