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
#include "group_im.h"
#include "group_find.h"
#include "group_internal.h"
#include "group_info.h"
#include "buddy_list.h"
#include "qq_define.h"
#include "packet_parse.h"
#include "qq_network.h"
#include "utils.h"

/* we check who needs to update member info every minutes
 * this interval determines if their member info is outdated */
#define QQ_GROUP_CHAT_REFRESH_NICKNAME_INTERNAL  180

static gboolean check_update_interval(qq_buddy_data *member)
{
	g_return_val_if_fail(member != NULL, FALSE);
	return (member->nickname == NULL) ||
		(time(NULL) - member->last_update) > QQ_GROUP_CHAT_REFRESH_NICKNAME_INTERNAL;
}

/* this is done when we receive the reply to get_online_members sub_cmd
 * all member are set offline, and then only those in reply packets are online */
static void set_all_offline(qq_group *group)
{
	GList *list;
	qq_buddy_data *bd;
	g_return_if_fail(group != NULL);

	list = group->members;
	while (list != NULL) {
		bd = (qq_buddy_data *) list->data;
		bd->status = QQ_BUDDY_CHANGE_TO_OFFLINE;
		list = list->next;
	}
}

/* send packet to get info for each group member */
gint qq_request_room_get_buddies(PurpleConnection *gc, qq_group *group, gint update_class)
{
	guint8 *raw_data;
	gint bytes, num;
	GList *list;
	qq_buddy_data *bd;

	g_return_val_if_fail(group != NULL, 0);
	for (num = 0, list = group->members; list != NULL; list = list->next) {
		bd = (qq_buddy_data *) list->data;
		if (check_update_interval(bd))
			num++;
	}

	if (num <= 0) {
		purple_debug_info("QQ", "No group member info needs to be updated now.\n");
		return 0;
	}

	raw_data = g_newa(guint8, 4 * num);

	bytes = 0;

	list = group->members;
	while (list != NULL) {
		bd = (qq_buddy_data *) list->data;
		if (check_update_interval(bd))
			bytes += qq_put32(raw_data + bytes, bd->uid);
		list = list->next;
	}

	qq_send_room_cmd_mess(gc, QQ_ROOM_CMD_GET_BUDDIES, group->id, raw_data, bytes,
			update_class, 0);
	return num;
}

static void room_info_display(PurpleConnection *gc, qq_group *group)
{
	PurpleNotifyUserInfo *room_info;
	gchar *utf8_value;

	g_return_if_fail(group != NULL && group->id > 0);

	room_info = purple_notify_user_info_new();

	purple_notify_user_info_add_pair(room_info, _("Title"), group->title_utf8);
	purple_notify_user_info_add_pair(room_info, _("Notice"), group->notice_utf8);
	purple_notify_user_info_add_pair(room_info, _("Detail"), group->desc_utf8);

	purple_notify_user_info_add_section_break(room_info);

	utf8_value = g_strdup_printf(("%d"), group->creator_uid);
	purple_notify_user_info_add_pair(room_info, _("Creator"), utf8_value);
	g_free(utf8_value);

	purple_notify_user_info_add_pair(room_info, _("About me"), group->my_role_desc);

	utf8_value = g_strdup_printf(("%d"), group->category);
	purple_notify_user_info_add_pair(room_info, _("Category"), utf8_value);
	g_free(utf8_value);

	utf8_value = g_strdup_printf(("%d"), group->auth_type);
	purple_notify_user_info_add_pair(room_info, _("Authorize"), utf8_value);
	g_free(utf8_value);

	utf8_value = g_strdup_printf(("%d"), group->ext_id);
	purple_notify_userinfo(gc, utf8_value, room_info, NULL, NULL);
	g_free(utf8_value);

	purple_notify_user_info_destroy(room_info);
}

void qq_process_room_cmd_get_info(guint8 *data, gint data_len, guint32 action, PurpleConnection *gc)
{
	qq_group *group;
	qq_buddy_data *bd;
	qq_data *qd;
	PurpleConversation *conv;
	guint8 organization, role;
	guint16 unknown, max_members;
	guint32 member_uid, id, ext_id;
	guint32 unknown4;
	guint8 unknown1;
	gint bytes, num;
	gchar *notice;
	gchar *topic_utf8;

	g_return_if_fail(data != NULL && data_len > 0);
	qd = (qq_data *) gc->proto_data;

	/* qq_show_packet("Room Info", data, data_len); */

	bytes = 0;
	bytes += qq_get32(&id, data + bytes);
	g_return_if_fail(id > 0);

	bytes += qq_get32(&ext_id, data + bytes);
	g_return_if_fail(ext_id > 0);

	if (action == QQ_ROOM_INFO_CREATE ) {
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
	bytes += qq_get32(&(group->category), data + bytes);
	bytes += qq_get16(&max_members, data + bytes);
	bytes += qq_get8(&unknown1, data + bytes);
	/* the following, while Eva:
	 * 4(unk), 4(verID), 1(nameLen), nameLen(qunNameContent), 1(0x00),
	 * 2(qunNoticeLen), qunNoticeLen(qunNoticeContent, 1(qunDescLen),
	 * qunDestLen(qunDestcontent)) */
	bytes += qq_get8(&unknown1, data + bytes);
	purple_debug_info("QQ", "type=%u creatorid=%u category=%u maxmembers=%u\n",
			group->type8, group->creator_uid, group->category, max_members);

	if (qd->client_version >= 2007) {
		/* skip 7 bytes unknow in qq2007 0x(00 00 01 00 00 00 fc)*/
		bytes += 7;
	}
	/* qq_show_packet("Room Info", data + bytes, data_len - bytes); */
	/* strlen + <str content> */
	bytes += qq_get_vstr(&(group->title_utf8), QQ_CHARSET_DEFAULT, data + bytes);
	bytes += qq_get16(&unknown, data + bytes);	/* 0x0000 */
	bytes += qq_get_vstr(&notice, QQ_CHARSET_DEFAULT, data + bytes);
	bytes += qq_get_vstr(&(group->desc_utf8), QQ_CHARSET_DEFAULT, data + bytes);

	purple_debug_info("QQ", "room [%s] notice [%s] desc [%s] unknow 0x%04X\n",
			group->title_utf8, notice, group->desc_utf8, unknown);

	num = 0;
	/* now comes the member list separated by 0x00 */
	while (bytes < data_len) {
		bytes += qq_get32(&member_uid, data + bytes);
		num++;
		bytes += qq_get8(&organization, data + bytes);
		bytes += qq_get8(&role, data + bytes);

#if 0
		if(organization != 0 || role != 0) {
			purple_debug_info("QQ_GRP", "%d, organization=%d, role=%d\n", member_uid, organization, role);
		}
#endif

		bd = qq_group_find_or_add_member(gc, group, member_uid);
		if (bd != NULL)
			bd->role = role;
	}
	if(bytes > data_len) {
		purple_debug_error("QQ",
			"group_cmd_get_group_info: Dangerous error! maybe protocol changed, notify me!");
	}

	purple_debug_info("QQ", "group \"%s\" has %d members\n", group->title_utf8, num);

	if (group->creator_uid == qd->uid)
		group->my_role = QQ_ROOM_ROLE_ADMIN;

	/* filter \r\n in notice */
	qq_filter_str(notice);
	group->notice_utf8 = strdup(notice);
	g_free(notice);

	qq_group_refresh(gc, group);

	if (action == QQ_ROOM_INFO_DISPLAY) {
		room_info_display(gc, group);
	}

	conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,
			group->title_utf8, purple_connection_get_account(gc));
	if(NULL == conv) {
		purple_debug_warning("QQ", "Conversation \"%s\" is not opened\n", group->title_utf8);
		return;
	}

	topic_utf8 = g_strdup_printf("%d %s", group->ext_id, group->notice_utf8);
	purple_debug_info("QQ", "Set chat topic to %s\n", topic_utf8);
	purple_conv_chat_set_topic(PURPLE_CONV_CHAT(conv), NULL, topic_utf8);
	g_free(topic_utf8);
}

void qq_process_room_cmd_get_onlines(guint8 *data, gint len, PurpleConnection *gc)
{
	guint32 id, member_uid;
	guint8 unknown;
	gint bytes, num;
	qq_group *group;
	qq_buddy_data *bd;

	g_return_if_fail(data != NULL && len > 0);

	if (len <= 3) {
		purple_debug_error("QQ", "Invalid group online member reply, discard it!\n");
		return;
	}

	bytes = 0;
	bytes += qq_get32(&id, data + bytes);
	bytes += qq_get8(&unknown, data + bytes);	/* 0x3c ?? */
	g_return_if_fail(id > 0);

	group = qq_room_search_id(gc, id);
	if (group == NULL) {
		purple_debug_error("QQ", "We have no group info for internal id [%d]\n", id);
		return;
	}

	/* set all offline first, then update those online */
	set_all_offline(group);
	num = 0;
	while (bytes < len) {
		bytes += qq_get32(&member_uid, data + bytes);
		num++;
		bd = qq_group_find_or_add_member(gc, group, member_uid);
		if (bd != NULL)
			bd->status = QQ_BUDDY_ONLINE_NORMAL;
	}
	if(bytes > len) {
		purple_debug_error("QQ",
			"group_cmd_get_online_members: Dangerous error! maybe protocol changed, notify developers!");
	}

	purple_debug_info("QQ", "Group \"%s\" has %d online members\n", group->title_utf8, num);
	qq_room_conv_set_onlines(gc, group);
}

/* process the reply to get_members_info packet */
void qq_process_room_cmd_get_buddies(guint8 *data, gint len, PurpleConnection *gc)
{
	gint bytes;
	gint num;
	guint32 id, member_uid;
	guint16 unknown;
	qq_group *group;
	qq_buddy_data *bd;
	gchar *nick;

	g_return_if_fail(data != NULL && len > 0);

	/* qq_show_packet("qq_process_room_cmd_get_buddies", data, len); */

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
		bd = qq_group_find_member_by_uid(group, member_uid);
		g_return_if_fail(bd != NULL);

		num++;
		bytes += qq_get16(&(bd->face), data + bytes);
		bytes += qq_get8(&(bd->age), data + bytes);
		bytes += qq_get8(&(bd->gender), data + bytes);
		bytes += qq_get_vstr(&nick, QQ_CHARSET_DEFAULT, data + bytes);
		bytes += qq_get16(&unknown, data + bytes);
		bytes += qq_get8(&(bd->ext_flag), data + bytes);
		bytes += qq_get8(&(bd->comm_flag), data + bytes);

		/* filter \r\n in nick */
		qq_filter_str(nick);
		bd->nickname = g_strdup(nick);
		g_free(nick);

#if 0
		purple_debug_info("QQ",
				"member [%09d]: ext_flag=0x%02x, comm_flag=0x%02x, nick=%s\n",
				member_uid, bd->ext_flag, bd->comm_flag, bd->nickname);
#endif

		bd->last_update = time(NULL);
	}
	if (bytes > len) {
		purple_debug_error("QQ",
				"group_cmd_get_members_info: Dangerous error! maybe protocol changed, notify developers!");
	}
	purple_debug_info("QQ", "Group \"%s\" obtained %d member info\n", group->title_utf8, num);

	group->is_got_buddies = TRUE;
	qq_room_conv_set_onlines(gc, group);
}

