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

#include "buddy_status.h"
#include "char_conv.h"
#include "group_find.h"
#include "group_internal.h"
#include "group_info.h"
#include "buddy_status.h"
#include "group_network.h"

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

/* send packet to get detailed information of one group */
void qq_send_cmd_group_get_group_info(PurpleConnection *gc, qq_group *group)
{
	guint8 raw_data[16] = {0};
	gint bytes = 0;

	g_return_if_fail(group != NULL);

	bytes += qq_put8(raw_data + bytes, QQ_GROUP_CMD_GET_GROUP_INFO);
	bytes += qq_put32(raw_data + bytes, group->internal_group_id);

	qq_send_group_cmd(gc, group, raw_data, bytes);
}

/* send packet to get online group member, called by keep_alive */
void qq_send_cmd_group_get_online_members(PurpleConnection *gc, qq_group *group)
{
	guint8 raw_data[16] = {0};
	gint bytes = 0;

	g_return_if_fail(group != NULL);

	/* only get online members when conversation window is on */
	if (NULL == purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,group->group_name_utf8, purple_connection_get_account(gc))) {
		purple_debug(PURPLE_DEBUG_WARNING, "QQ",
				"Conv windows for \"%s\" is not on, do not get online members\n", group->group_name_utf8);
		return;
	}

	bytes += qq_put8(raw_data + bytes, QQ_GROUP_CMD_GET_ONLINE_MEMBER);
	bytes += qq_put32(raw_data + bytes, group->internal_group_id);

	qq_send_group_cmd(gc, group, raw_data, bytes);
}

/* send packet to get info for each group member */
void qq_send_cmd_group_get_members_info(PurpleConnection *gc, qq_group *group)
{
	guint8 *raw_data;
	gint bytes, num, data_len;
	GList *list;
	qq_buddy *member;

	g_return_if_fail(group != NULL);
	for (num = 0, list = group->members; list != NULL; list = list->next) {
		member = (qq_buddy *) list->data;
		if (_is_group_member_need_update_info(member))
			num++;
	}

	if (num <= 0) {
		purple_debug(PURPLE_DEBUG_INFO, "QQ", "No group member needs to to update info now.\n");
		return;
	}

	data_len = 5 + 4 * num;
	raw_data = g_newa(guint8, data_len);

	bytes = 0;
	bytes += qq_put8(raw_data + bytes, QQ_GROUP_CMD_GET_MEMBER_INFO);
	bytes += qq_put32(raw_data + bytes, group->internal_group_id);

	list = group->members;
	while (list != NULL) {
		member = (qq_buddy *) list->data;
		if (_is_group_member_need_update_info(member))
			bytes += qq_put32(raw_data + bytes, member->uid);
		list = list->next;
	}

	if (bytes != data_len) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ",
				"Fail create packet for %s\n", qq_group_cmd_get_desc(QQ_GROUP_CMD_GET_MEMBER_INFO));
		return;
	}

	qq_send_group_cmd(gc, group, raw_data, bytes);
}

/**
 * @brief 处理群信息.当前群信息的处理还不完善,由于版本的不同导致协议的解读有差异.
 */
void qq_process_group_cmd_get_group_info(guint8 *data, gint len, PurpleConnection *gc)
{
	qq_group *group;
	qq_buddy *member;
	qq_data *qd;
	PurpleConversation *purple_conv;
	guint8 organization, role;
	guint16 unknown, max_members;
	guint32 member_uid, internal_group_id, external_group_id;
	GSList *pending_id;
	guint32 unknown4;
	guint8 unknown1;
	gint bytes, num;

	g_return_if_fail(data != NULL && len > 0);
	qd = (qq_data *) gc->proto_data;

	bytes = 0;
	bytes += qq_get32(&(internal_group_id), data + bytes);
	g_return_if_fail(internal_group_id > 0);

	bytes += qq_get32(&(external_group_id), data + bytes);
	g_return_if_fail(external_group_id > 0);

	pending_id = qq_get_pending_id(qd->adding_groups_from_server, internal_group_id);
	if (pending_id != NULL) {
		qq_set_pending_id(&qd->adding_groups_from_server, internal_group_id, FALSE);
		qq_group_create_internal_record(gc, internal_group_id, external_group_id, NULL);
	}

	group = qq_group_find_by_id(gc, internal_group_id, QQ_INTERNAL_ID);
	g_return_if_fail(group != NULL);

	bytes += qq_get8(&(group->group_type), data + bytes);
	bytes += qq_get32(&unknown4, data + bytes);	/* unknown 4 bytes */
	bytes += qq_get32(&(group->creator_uid), data + bytes);
	bytes += qq_get8(&(group->auth_type), data + bytes);
	bytes += qq_get32(&unknown4, data + bytes);	/* oldCategory */
	bytes += qq_get16(&unknown, data + bytes);	
	bytes += qq_get32(&(group->group_category), data + bytes);
	bytes += qq_get16(&max_members, data + bytes);
	bytes += qq_get8(&unknown1, data + bytes);
	/* XXX
	 * the following, while Eva:
	 * 4(unk), 4(verID), 1(nameLen), nameLen(qunNameContent), 1(0x00),
	 * 2(qunNoticeLen), qunNoticeLen(qunNoticeContent, 1(qunDescLen),
	 * qunDestLen(qunDestcontent)) */
	bytes += qq_get8(&unknown1, data + bytes);
	purple_debug(PURPLE_DEBUG_INFO, "QQ", "type=%u creatorid=%u category=%u\n",
			group->group_type, group->creator_uid, group->group_category);
	purple_debug(PURPLE_DEBUG_INFO, "QQ", "maxmembers=%u", max_members); 
	
	/* strlen + <str content> */
	bytes += convert_as_pascal_string(data + bytes, &(group->group_name_utf8), QQ_CHARSET_DEFAULT);
	purple_debug(PURPLE_DEBUG_INFO, "QQ", "group \"%s\"\n", group->group_name_utf8); 
	bytes += qq_get16(&unknown, data + bytes);	/* 0x0000 */
	bytes += convert_as_pascal_string(data + bytes, &(group->notice_utf8), QQ_CHARSET_DEFAULT);
	purple_debug(PURPLE_DEBUG_INFO, "QQ", "notice \"%s\"\n", group->notice_utf8); 
	bytes += convert_as_pascal_string(data + bytes, &(group->group_desc_utf8), QQ_CHARSET_DEFAULT);
	purple_debug(PURPLE_DEBUG_INFO, "QQ", "group_desc \"%s\"\n", group->group_desc_utf8); 

	num = 0;
	/* now comes the member list separated by 0x00 */
	while (bytes < len) {
		bytes += qq_get32(&member_uid, data + bytes);
		num++;
		bytes += qq_get8(&organization, data + bytes);
		bytes += qq_get8(&role, data + bytes);

		if(organization != 0 || role != 0) {
			purple_debug(PURPLE_DEBUG_INFO, "QQ", "group member %d: organization=%d, role=%d\n", member_uid, organization, role);
		}
		member = qq_group_find_or_add_member(gc, group, member_uid);
		if (member != NULL)
			member->role = role;
	}
	if(bytes > len) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", "group_cmd_get_group_info: Dangerous error! maybe protocol changed, notify me!");
	}

	purple_debug(PURPLE_DEBUG_INFO, "QQ", "group \"%s\" has %d members\n", group->group_name_utf8, num);

	if (group->creator_uid == qd->uid)
		group->my_status = QQ_GROUP_MEMBER_STATUS_IS_ADMIN;

	qq_group_refresh(gc, group);

	purple_conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT, 
			group->group_name_utf8, purple_connection_get_account(gc));
	if(NULL == purple_conv) {
		purple_debug(PURPLE_DEBUG_WARNING, "QQ",
				"Conv windows for \"%s\" is not on, do not set topic\n", group->group_name_utf8);
	}
	else {
		purple_conv_chat_set_topic(PURPLE_CONV_CHAT(purple_conv), NULL, group->notice_utf8);
	}
}

void qq_process_group_cmd_get_online_members(guint8 *data, gint len, PurpleConnection *gc)
{
	guint32 internal_group_id, member_uid;
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
	bytes += qq_get32(&internal_group_id, data + bytes);
	bytes += qq_get8(&unknown, data + bytes);	/* 0x3c ?? */
	g_return_if_fail(internal_group_id > 0);

	group = qq_group_find_by_id(gc, internal_group_id, QQ_INTERNAL_ID);
	if (group == NULL) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", 
				"We have no group info for internal id [%d]\n", internal_group_id);
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
void qq_process_group_cmd_get_members_info(guint8 *data, gint len, PurpleConnection *gc)
{
	gint bytes;
	gint num;
	guint32 internal_group_id, member_uid;
	guint16 unknown;
	qq_group *group;
	qq_buddy *member;

	g_return_if_fail(data != NULL && len > 0);

	bytes = 0;
	bytes += qq_get32(&internal_group_id, data + bytes);
	g_return_if_fail(internal_group_id > 0);

	group = qq_group_find_by_id(gc, internal_group_id, QQ_INTERNAL_ID);
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
		bytes += convert_as_pascal_string(data + bytes, &(member->nickname), QQ_CHARSET_DEFAULT);
		bytes += qq_get16(&unknown, data + bytes);
		bytes += qq_get8(&(member->flag1), data + bytes);
		bytes += qq_get8(&(member->comm_flag), data + bytes);

		member->last_refresh = time(NULL);
	}
	if(bytes > len) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", 
				"group_cmd_get_members_info: Dangerous error! maybe protocol changed, notify developers!");
	}
	purple_debug(PURPLE_DEBUG_INFO, "QQ", "Group \"%s\" obtained %d member info\n", group->group_name_utf8, num);
}
