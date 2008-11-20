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
#include "group_network.h"
#include "group_search.h"
#include "utils.h"

enum {
	QQ_GROUP_SEARCH_TYPE_BY_ID = 0x01,
	QQ_GROUP_SEARCH_TYPE_DEMO = 0x02
};

/* send packet to search for qq_group */
void qq_send_cmd_group_search_group(PurpleConnection *gc, guint32 external_group_id)
{
	guint8 raw_data[16] = {0};
	gint bytes = 0;
	guint8 type;

	type = (external_group_id == 0x00000000) ? QQ_GROUP_SEARCH_TYPE_DEMO : QQ_GROUP_SEARCH_TYPE_BY_ID;

	bytes = 0;
	bytes += qq_put8(raw_data + bytes, QQ_GROUP_CMD_SEARCH_GROUP);
	bytes += qq_put8(raw_data + bytes, type);
	bytes += qq_put32(raw_data + bytes, external_group_id);

	qq_send_group_cmd(gc, NULL, raw_data, bytes);
}

static void _qq_setup_roomlist(qq_data *qd, qq_group *group)
{
	PurpleRoomlistRoom *room;
	gchar field[11];

	room = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_ROOM, group->group_name_utf8, NULL);
	g_snprintf(field, sizeof(field), "%d", group->external_group_id);
	purple_roomlist_room_add_field(qd->roomlist, room, field);
	g_snprintf(field, sizeof(field), "%d", group->creator_uid);
	purple_roomlist_room_add_field(qd->roomlist, room, field);
	purple_roomlist_room_add_field(qd->roomlist, room, group->group_desc_utf8);
	g_snprintf(field, sizeof(field), "%d", group->internal_group_id);
	purple_roomlist_room_add_field(qd->roomlist, room, field);
	g_snprintf(field, sizeof(field), "%d", group->group_type);
	purple_roomlist_room_add_field(qd->roomlist, room, field);
	g_snprintf(field, sizeof(field), "%d", group->auth_type);
	purple_roomlist_room_add_field(qd->roomlist, room, field);
	g_snprintf(field, sizeof(field), "%d", group->group_category);
	purple_roomlist_room_add_field(qd->roomlist, room, field);
	purple_roomlist_room_add_field(qd->roomlist, room, group->group_name_utf8);
	purple_roomlist_room_add(qd->roomlist, room);

	purple_roomlist_set_in_progress(qd->roomlist, FALSE);
}

/* process group cmd reply "search group" */
void qq_process_group_cmd_search_group(guint8 *data, gint len, PurpleConnection *gc)
{
	gint bytes;
	guint8 search_type;
	guint16 unknown;
	qq_group group;
	qq_data *qd;
	GSList *pending_id;

	g_return_if_fail(data != NULL && len > 0);
	qd = (qq_data *) gc->proto_data;

	bytes = 0;
	bytes += qq_get8(&search_type, data + bytes);

	/* now it starts with group_info_entry */
	bytes += qq_get32(&(group.internal_group_id), data + bytes);
	bytes += qq_get32(&(group.external_group_id), data + bytes);
	bytes += qq_get8(&(group.group_type), data + bytes);
	bytes += qq_get16(&(unknown), data + bytes);
	bytes += qq_get16(&(unknown), data + bytes);
	bytes += qq_get32(&(group.creator_uid), data + bytes);
	bytes += qq_get16(&(unknown), data + bytes);
	bytes += qq_get16(&(unknown), data + bytes);
	bytes += qq_get16(&(unknown), data + bytes);
	bytes += qq_get32(&(group.group_category), data + bytes);
	bytes += convert_as_pascal_string(data + bytes, &(group.group_name_utf8), QQ_CHARSET_DEFAULT);
	bytes += qq_get16(&(unknown), data + bytes);
	bytes += qq_get8(&(group.auth_type), data + bytes);
	bytes += convert_as_pascal_string(data + bytes, &(group.group_desc_utf8), QQ_CHARSET_DEFAULT);
	/* end of one qq_group */
	if(bytes != len) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", 
			"group_cmd_search_group: Dangerous error! maybe protocol changed, notify developers!");
	}

	pending_id = qq_get_pending_id(qd->joining_groups, group.external_group_id);
	if (pending_id != NULL) {
		qq_set_pending_id(&qd->joining_groups, group.external_group_id, FALSE);
		if (qq_group_find_by_id(gc, group.internal_group_id, QQ_INTERNAL_ID) == NULL)
			qq_group_create_internal_record(gc, 
					group.internal_group_id, group.external_group_id, group.group_name_utf8);
		qq_send_cmd_group_join_group(gc, &group);
	} else {
		_qq_setup_roomlist(qd, &group);
	}
}
