/**
 * @file group_info.c
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

#include "conversation.h"
#include "debug.h"

#include "char_conv.h"
#include "group_find.h"
#include "group_internal.h"
#include "group_info.h"
#include "buddy_list.h"
#include "header_info.h"
#include "packet_parse.h"
#include "qq_network.h"
#include "utils.h"

/* we check who needs to update member info every minutes
 * this interval determines if their member info is outdated */
#define QQ_GROUP_CHAT_REFRESH_NICKNAME_INTERNAL  180

static gboolean _is_group_member_need_update_info(qq_buddy *member)
{
	g_return_val_if_fail(member != NULL, FALSE);
	return (member->nickname == NULL) ||
		(time(NULL) - member->last_refresh) > QQ_GROUP_CHAT_REFRESH_NICKNAME_INTERNAL;
}

/* this is done when we receive the reply to get_online_members sub_cmd
 * all member are set offline, and then only those in reply packets are online */
static void _qq_group_set_members_all_offline(qq_group *group)
{
	GList *list;
	qq_buddy *member;
	g_return_if_fail(group != NULL);

	list = group->members;
	while (list != NULL) {
		member = (qq_buddy *) list->data;
		member->status = QQ_BUDDY_ONLINE_OFFLINE;
		list = list->next;
	}
}

/* send packet to get online group member, called by keep_alive */
void qq_send_cmd_group_all_get_online_members(PurpleConnection *gc)
{
	qq_data *qd;
	qq_group *group;
	GList *list;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	list = qd->groups;
	while (list != NULL) {
		group = (qq_group *) list->data;
		if (group->my_status == QQ_GROUP_MEMBER_STATUS_IS_MEMBER ||
		    group->my_status == QQ_GROUP_MEMBER_STATUS_IS_ADMIN)
			/* no need to get info time and time again, online members enough */
			qq_send_cmd_group_get_online_members(gc, group);

		list = list->next;
	}
}

void qq_send_cmd_group_get_online_members(PurpleConnection *gc, qq_group *group)
{
	g_return_if_fail(group != NULL);

	/* only get online members when conversation window is on */
	if (NULL == purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,group->group_name_utf8, purple_connection_get_account(gc))) {
		purple_debug(PURPLE_DEBUG_WARNING, "QQ",
				"Conversation \"%s\" is not open, ignore to get online members\n", group->group_name_utf8);
		return;
	}

	qq_send_room_cmd_only(gc, QQ_ROOM_CMD_GET_ONLINES, group->id);
}

/* send packet to get info for each group member */
void qq_send_cmd_group_get_members_info(PurpleConnection *gc, qq_group *group)
{
	guint8 *raw_data;
	gint bytes, num;
	GList *list;
	qq_buddy *member;

	g_return_if_fail(group != NULL);
	for (num = 0, list = group->members; list != NULL; list = list->next) {
		member = (qq_buddy *) list->data;
		if (_is_group_member_need_update_info(member))
			num++;
	}

	if (num <= 0) {
		purple_debug(PURPLE_DEBUG_INFO, "QQ", "No group member info needs to be updated now.\n");
		return;
	}

	raw_data = g_newa(guint8, 4 * num);

	bytes = 0;

	list = group->members;
	while (list != NULL) {
		member = (qq_buddy *) list->data;
		if (_is_group_member_need_update_info(member))
			bytes += qq_put32(raw_data + bytes, member->uid);
		list = list->next;
	}

	qq_send_room_cmd(gc, QQ_ROOM_CMD_GET_MEMBER_INFO, group->id, raw_data, bytes);
}

void qq_process_room_cmd_get_info(guint8 *data, gint data_len, PurpleConnection *gc)
{
	qq_group *group;
	qq_buddy *member;
	qq_data *qd;
	PurpleConversation *purple_conv;
	guint8 organization, role;
	guint16 unknown, max_members;
	guint32 member_uid, id, ext_id;
	GSList *pending_id;
	guint32 unknown4;
	guint8 unknown1;
	gint bytes, num;
	gchar *notice;

	g_return_if_fail(data != NULL && data_len > 0);
	qd = (qq_data *) gc->proto_data;

	bytes = 0;
	bytes += qq_get32(&id, data + bytes);
	g_return_if_fail(id > 0);

	bytes += qq_get32(&ext_id, data + bytes);
	g_return_if_fail(ext_id > 0);

	pending_id = qq_get_pending_id(qd->adding_groups_from_server, id);
	if (pending_id != NULL) {
		qq_set_pending_id(&qd->adding_groups_from_server, id, FALSE);
		qq_group_create_internal_record(gc, id, ext_id, NULL);
	}

	group = qq_room_search_id(gc, id);
	g_return_if_fail(group != NULL);

	bytes += qq_get8(&(group->type8), data + bytes);
	bytes += qq_get32(&unknown4, data + bytes);	/* unknown 4 bytes */
	bytes += qq_get32(&(group->creator_uid), data + bytes);
	bytes += qq_get8(&(group->auth_type), data + bytes);
	bytes += qq_get32(&unknown4, data + bytes);	/* oldCategory */
	bytes += qq_get16(&unknown, data + bytes);	
	bytes += qq_get32(&(group->group_category), data + bytes);
	bytes += qq_get16(&max_members, data + bytes);
	bytes += qq_get8(&unknown1, data + bytes);
	/* the following, while Eva:
	 * 4(unk), 4(verID), 1(nameLen), nameLen(qunNameContent), 1(0x00),
	 * 2(qunNoticeLen), qunNoticeLen(qunNoticeContent, 1(qunDescLen),
	 * qunDestLen(qunDestcontent)) */
	bytes += qq_get8(&unknown1, data + bytes);
	purple_debug(PURPLE_DEBUG_INFO, "QQ", "type=%u creatorid=%u category=%u maxmembers=%u\n",
			group->type8, group->creator_uid, group->group_category, max_members);
	
	/* strlen + <str content> */
	bytes += convert_as_pascal_string(data + bytes, &(group->group_name_utf8), QQ_CHARSET_DEFAULT);
	purple_debug(PURPLE_DEBUG_INFO, "QQ", "group \"%s\"\n", group->group_name_utf8); 
	bytes += qq_get16(&unknown, data + bytes);	/* 0x0000 */
	bytes += convert_as_pascal_string(data + bytes, &notice, QQ_CHARSET_DEFAULT);
	purple_debug(PURPLE_DEBUG_INFO, "QQ", "notice \"%s\"\n", notice); 
	bytes += convert_as_pascal_string(data + bytes, &(group->group_desc_utf8), QQ_CHARSET_DEFAULT);
	purple_debug(PURPLE_DEBUG_INFO, "QQ", "group_desc \"%s\"\n", group->group_desc_utf8); 

	num = 0;
	/* now comes the member list separated by 0x00 */
	while (bytes < data_len) {
		bytes += qq_get32(&member_uid, data + bytes);
		num++;
		bytes += qq_get8(&organization, data + bytes);
		bytes += qq_get8(&role, data + bytes);

#if 0
		if(organization != 0 || role != 0) {
			purple_debug(PURPLE_DEBUG_INFO, "QQ_GRP", "%d, organization=%d, role=%d\n", member_uid, organization, role);
		}
#endif

		member = qq_group_find_or_add_member(gc, group, member_uid);
		if (member != NULL)
			member->role = role;
	}
	if(bytes > data_len) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ",
			"group_cmd_get_group_info: Dangerous error! maybe protocol changed, notify me!");
	}

	purple_debug(PURPLE_DEBUG_INFO, "QQ", "group \"%s\" has %d members\n", group->group_name_utf8, num);

	if (group->creator_uid == qd->uid)
		group->my_status = QQ_GROUP_MEMBER_STATUS_IS_ADMIN;

	qq_group_refresh(gc, group);

	purple_conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT, 
			group->group_name_utf8, purple_connection_get_account(gc));
	if(NULL == purple_conv) {
		purple_debug(PURPLE_DEBUG_WARNING, "QQ",
				"Conversation \"%s\" is not open, do not set topic\n", group->group_name_utf8);
		return;
	}

	/* filter \r\n in notice */
	qq_filter_str(notice);
	group->notice_utf8 = strdup(notice);
	g_free(notice);
	
	purple_conv_chat_set_topic(PURPLE_CONV_CHAT(purple_conv), NULL, group->notice_utf8);
}

void qq_process_room_cmd_get_onlines(guint8 *data, gint len, PurpleConnection *gc)
{
	guint32 id, member_uid;
	guint8 unknown;
	gint bytes, num;
	qq_group *group;
	qq_buddy *member;

	g_return_if_fail(data != NULL && len > 0);

	if (len <= 3) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", "Invalid group online member reply, discard it!\n");
		return;
	}

	bytes = 0;
	bytes += qq_get32(&id, data + bytes);
	bytes += qq_get8(&unknown, data + bytes);	/* 0x3c ?? */
	g_return_if_fail(id > 0);

	group = qq_room_search_id(gc, id);
	if (group == NULL) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", 
				"We have no group info for internal id [%d]\n", id);
		return;
	}

	/* set all offline first, then update those online */
	_qq_group_set_members_all_offline(group);
	num = 0;
	while (bytes < len) {
		bytes += qq_get32(&member_uid, data + bytes);
		num++;
		member = qq_group_find_or_add_member(gc, group, member_uid);
		if (member != NULL)
			member->status = QQ_BUDDY_ONLINE_NORMAL;
	}
	if(bytes > len) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", 
				"group_cmd_get_online_members: Dangerous error! maybe protocol changed, notify developers!");
	}

	purple_debug(PURPLE_DEBUG_INFO, "QQ", "Group \"%s\" has %d online members\n", group->group_name_utf8, num);
}

/* process the reply to get_members_info packet */
void qq_process_room_cmd_get_members(guint8 *data, gint len, PurpleConnection *gc)
{
	gint bytes;
	gint num;
	guint32 id, member_uid;
	guint16 unknown;
	qq_group *group;
	qq_buddy *member;
	gchar *nick;

	g_return_if_fail(data != NULL && len > 0);

#if 0
	qq_show_packet("qq_process_room_cmd_get_members", data, len);
#endif

	bytes = 0;
	bytes += qq_get32(&id, data + bytes);
	g_return_if_fail(id > 0);

	group = qq_room_search_id(gc, id);
	g_return_if_fail(group != NULL);

	num = 0;
	/* now starts the member info, as get buddy list reply */
	while (bytes < len) {
		bytes += qq_get32(&member_uid, data + bytes);
		g_return_if_fail(member_uid > 0);
		member = qq_group_find_member_by_uid(group, member_uid);
		g_return_if_fail(member != NULL);

		num++;
		bytes += qq_get16(&(member->face), data + bytes);
		bytes += qq_get8(&(member->age), data + bytes);
		bytes += qq_get8(&(member->gender), data + bytes);
		bytes += convert_as_pascal_string(data + bytes, &nick, QQ_CHARSET_DEFAULT);
		bytes += qq_get16(&unknown, data + bytes);
		bytes += qq_get8(&(member->ext_flag), data + bytes);
		bytes += qq_get8(&(member->comm_flag), data + bytes);

		/* filter \r\n in nick */
		qq_filter_str(nick);
		member->nickname = g_strdup(nick);
		g_free(nick);
		
#if 0
		purple_debug(PURPLE_DEBUG_INFO, "QQ",
				"member [%09d]: ext_flag=0x%02x, comm_flag=0x%02x, nick=%s\n",
				member_uid, member->ext_flag, member->comm_flag, member->nickname);
#endif

		member->last_refresh = time(NULL);
	}
	if (bytes > len) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", 
				"group_cmd_get_members_info: Dangerous error! maybe protocol changed, notify developers!");
	}
	purple_debug(PURPLE_DEBUG_INFO, "QQ", "Group \"%s\" obtained %d member info\n", group->group_name_utf8, num);
}

