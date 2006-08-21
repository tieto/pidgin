/**
 * The QQ2003C protocol plugin
 *
 * for gaim
 *
 * Copyright (C) 2004 Puzzlebird
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "debug.h"

#include "char_conv.h"
#include "group_free.h"
#include "group_network.h"
#include "group_search.h"
#include "utils.h"

enum {
	QQ_GROUP_SEARCH_TYPE_BY_ID = 0x01,
	QQ_GROUP_SEARCH_TYPE_DEMO = 0x02
};

/* send packet to search for qq_group */
void qq_send_cmd_group_search_group(GaimConnection *gc, guint32 external_group_id)
{
	guint8 *raw_data, *cursor, type;
	gint bytes, data_len;

	g_return_if_fail(gc != NULL);

	data_len = 6;
	raw_data = g_newa(guint8, data_len);
	cursor = raw_data;
	type = (external_group_id == 0x00000000) ? QQ_GROUP_SEARCH_TYPE_DEMO : QQ_GROUP_SEARCH_TYPE_BY_ID;

	bytes = 0;
	bytes += create_packet_b(raw_data, &cursor, QQ_GROUP_CMD_SEARCH_GROUP);
	bytes += create_packet_b(raw_data, &cursor, type);
	bytes += create_packet_dw(raw_data, &cursor, external_group_id);

	if (bytes != data_len)
		gaim_debug(GAIM_DEBUG_ERROR, "QQ",
			   "Fail create packet for %s\n", qq_group_cmd_get_desc(QQ_GROUP_CMD_SEARCH_GROUP));
	else
		qq_send_group_cmd(gc, NULL, raw_data, data_len);
}

/* process group cmd reply "search group" */
void qq_process_group_cmd_search_group(guint8 *data, guint8 **cursor, gint len, GaimConnection *gc)
{
	guint8 search_type;
	guint16 unknown;
	gint bytes, pascal_len, i;
	qq_data *qd;
	GaimRoomlistRoom *room;
	qq_group *group;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	g_return_if_fail(data != NULL && len > 0);
	qd = (qq_data *) gc->proto_data;

	i = 0;
	read_packet_b(data, cursor, len, &search_type);
	group = g_newa(qq_group, 1);

	/* now it starts with group_info_entry */
	while (*cursor < (data + len)) {	/* still have data to read */
		/* begin of one qq_group */
		bytes = 0;
		i++;
		bytes += read_packet_dw(data, cursor, len, &(group->internal_group_id));
		bytes += read_packet_dw(data, cursor, len, &(group->external_group_id));
		bytes += read_packet_b(data, cursor, len, &(group->group_type));
		bytes += read_packet_w(data, cursor, len, &(unknown));
		bytes += read_packet_w(data, cursor, len, &(unknown));
		bytes += read_packet_dw(data, cursor, len, &(group->creator_uid));
		bytes += read_packet_w(data, cursor, len, &(unknown));
		bytes += read_packet_w(data, cursor, len, &(unknown));
		bytes += read_packet_w(data, cursor, len, &(unknown));
		bytes += read_packet_dw(data, cursor, len, &(group->group_category));
		pascal_len = convert_as_pascal_string(*cursor, &(group->group_name_utf8), QQ_CHARSET_DEFAULT);
		bytes += pascal_len;
		*cursor += pascal_len;
		bytes += read_packet_w(data, cursor, len, &(unknown));
		bytes += read_packet_b(data, cursor, len, &(group->auth_type));
		pascal_len = convert_as_pascal_string(*cursor, &(group->group_desc_utf8), QQ_CHARSET_DEFAULT);
		bytes += pascal_len;
		*cursor += pascal_len;
		/* end of one qq_group */
		room = gaim_roomlist_room_new(GAIM_ROOMLIST_ROOMTYPE_ROOM, group->group_name_utf8, NULL);
		gaim_roomlist_room_add_field(qd->roomlist, room, g_strdup_printf("%d", group->external_group_id));
		gaim_roomlist_room_add_field(qd->roomlist, room, g_strdup_printf("%d", group->creator_uid));
		gaim_roomlist_room_add_field(qd->roomlist, room, group->group_desc_utf8);
		gaim_roomlist_room_add_field(qd->roomlist, room, g_strdup_printf("%d", group->internal_group_id));
		gaim_roomlist_room_add_field(qd->roomlist, room, g_strdup_printf("%d", group->group_type));
		gaim_roomlist_room_add_field(qd->roomlist, room, g_strdup_printf("%d", group->auth_type));
		gaim_roomlist_room_add_field(qd->roomlist, room, g_strdup_printf("%d", group->group_category));
		gaim_roomlist_room_add_field(qd->roomlist, room, group->group_name_utf8);
		gaim_roomlist_room_add(qd->roomlist, room);
	}
        if(*cursor > (data + len)) {
                         gaim_debug(GAIM_DEBUG_ERROR, "QQ", 
					 "group_cmd_search_group: Dangerous error! maybe protocol changed, notify developers!");
        }
	gaim_roomlist_set_in_progress(qd->roomlist, FALSE);
	gaim_debug(GAIM_DEBUG_INFO, "QQ", "Search group reply: %d groups\n", i);
}
