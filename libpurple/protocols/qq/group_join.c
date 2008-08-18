/**
 * @file group_join.c
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
#include "notify.h"
#include "request.h"
#include "server.h"

#include "buddy_opt.h"
#include "char_conv.h"
#include "group_conv.h"
#include "group_find.h"
#include "group_free.h"
#include "group_internal.h"
#include "group_info.h"
#include "group_join.h"
#include "group_opt.h"
#include "group_search.h"
#include "header_info.h"
#include "packet_parse.h"
#include "qq_network.h"

enum {
	QQ_GROUP_JOIN_OK = 0x01,
	QQ_GROUP_JOIN_NEED_AUTH = 0x02,
};

static void _qq_group_exit_with_gc_and_id(gc_and_uid *g)
{
	PurpleConnection *gc;
	guint32 id;
	qq_group *group;

	gc = g->gc;
	id = g->uid;

	group = qq_room_search_id(gc, id);
	g_return_if_fail(group != NULL);

	qq_send_room_cmd_only(gc, QQ_ROOM_CMD_QUIT, group->id);
}

/* send packet to join a group without auth */
void qq_send_cmd_group_join_group(PurpleConnection *gc, qq_group *group)
{
	g_return_if_fail(group != NULL);

	if (group->my_status == QQ_GROUP_MEMBER_STATUS_NOT_MEMBER) {
		group->my_status = QQ_GROUP_MEMBER_STATUS_APPLYING;
		qq_group_refresh(gc, group);
	}

	switch (group->auth_type) {
	case QQ_GROUP_AUTH_TYPE_NO_AUTH:
	case QQ_GROUP_AUTH_TYPE_NEED_AUTH:
		break;
	case QQ_GROUP_AUTH_TYPE_NO_ADD:
		purple_notify_warning(gc, NULL, _("This group does not allow others to join"), NULL);
		return;
	default:
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", "Unknown group auth type: %d\n", group->auth_type);
		break;
	}

	qq_send_room_cmd_only(gc, QQ_ROOM_CMD_JOIN, group->id);
}

static void _qq_group_join_auth_with_gc_and_id(gc_and_uid *g, const gchar *reason_utf8)
{
	PurpleConnection *gc;
	qq_group *group;
	guint32 id;

	gc = g->gc;
	id = g->uid;

	group = qq_room_search_id(gc, id);
	if (group == NULL) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", "Can not find qq_group by internal_id: %d\n", id);
		return;
	} else {		/* everything is OK */
		qq_send_cmd_group_auth(gc, group, QQ_GROUP_AUTH_REQUEST_APPLY, 0, reason_utf8);
	}
}

static void _qq_group_join_auth(PurpleConnection *gc, qq_group *group)
{
	gchar *msg;
	gc_and_uid *g;
	g_return_if_fail(group != NULL);

	purple_debug(PURPLE_DEBUG_INFO, "QQ", 
			"Group (internal id: %d) needs authentication\n", group->id);

	msg = g_strdup_printf("Group \"%s\" needs authentication\n", group->group_name_utf8);
	g = g_new0(gc_and_uid, 1);
	g->gc = gc;
	g->uid = group->id;
	purple_request_input(gc, NULL, msg,
			   _("Input request here"),
			   _("Would you be my friend?"), TRUE, FALSE, NULL,
			   _("Send"),
			   G_CALLBACK(_qq_group_join_auth_with_gc_and_id),
			   _("Cancel"), G_CALLBACK(qq_do_nothing_with_gc_and_uid),
			   purple_connection_get_account(gc), group->group_name_utf8, NULL,
			   g);
	g_free(msg);
}

void qq_send_cmd_group_auth(PurpleConnection *gc, qq_group *group, guint8 opt, guint32 uid, const gchar *reason_utf8)
{
	guint8 *raw_data;
	gchar *reason_qq;
	gint bytes;

	g_return_if_fail(group != NULL);

	if (reason_utf8 == NULL || strlen(reason_utf8) == 0)
		reason_qq = g_strdup("");
	else
		reason_qq = utf8_to_qq(reason_utf8, QQ_CHARSET_DEFAULT);

	if (opt == QQ_GROUP_AUTH_REQUEST_APPLY) {
		group->my_status = QQ_GROUP_MEMBER_STATUS_APPLYING;
		qq_group_refresh(gc, group);
		uid = 0;
	}

	raw_data = g_newa(guint8, 6 + strlen(reason_qq));

	bytes = 0;
	bytes += qq_put8(raw_data + bytes, opt);
	bytes += qq_put32(raw_data + bytes, uid);
	bytes += qq_put8(raw_data + bytes, strlen(reason_qq));
	bytes += qq_putdata(raw_data + bytes, (guint8 *) reason_qq, strlen(reason_qq));

	qq_send_room_cmd(gc, QQ_ROOM_CMD_AUTH, group->id, raw_data, bytes);
}

/* If comes here, cmd is OK already */
void qq_process_group_cmd_exit_group(guint8 *data, gint len, PurpleConnection *gc)
{
	gint bytes;
	guint32 id;
	PurpleChat *chat;
	qq_group *group;
	qq_data *qd;

	g_return_if_fail(data != NULL && len > 0);
	qd = (qq_data *) gc->proto_data;

	if (len < 4) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ",
			   "Invalid exit group reply, expect %d bytes, read %d bytes\n", 4, len);
		return;
	}

	bytes = 0;
	bytes += qq_get32(&id, data + bytes);

	group = qq_room_search_id(gc, id);
	if (group != NULL) {
		chat = purple_blist_find_chat
			    (purple_connection_get_account(gc), g_strdup_printf("%d", group->ext_id));
		if (chat != NULL)
			purple_blist_remove_chat(chat);
		qq_group_delete_internal_record(qd, id);
	}
	purple_notify_info(gc, _("QQ Qun Operation"), _("You have successfully left the group"), NULL);
}

/* Process the reply to group_auth subcmd */
void qq_process_group_cmd_join_group_auth(guint8 *data, gint len, PurpleConnection *gc)
{
	gint bytes;
	guint32 id;
	qq_data *qd;

	g_return_if_fail(data != NULL && len > 0);
	qd = (qq_data *) gc->proto_data;

	if (len < 4) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ",
			   "Invalid join group reply, expect %d bytes, read %d bytes\n", 4, len);
		return;
	}
	bytes = 0;
	bytes += qq_get32(&id, data + bytes);
	g_return_if_fail(id > 0);

	purple_notify_info(gc, _("QQ Group Auth"),
		     _("Your authorization request has been accepted by the QQ server"), NULL);
}

/* process group cmd reply "join group" */
void qq_process_group_cmd_join_group(guint8 *data, gint len, PurpleConnection *gc)
{
	gint bytes;
	guint32 id;
	guint8 reply;
	qq_group *group;

	g_return_if_fail(data != NULL && len > 0);

	if (len < 5) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ",
			   "Invalid join group reply, expect %d bytes, read %d bytes\n", 5, len);
		return;
	}
	
	bytes = 0;
	bytes += qq_get32(&id, data + bytes);
	bytes += qq_get8(&reply, data + bytes);

	/* join group OK */
	group = qq_room_search_id(gc, id);
	/* need to check if group is NULL or not. */
	g_return_if_fail(group != NULL);
	switch (reply) {
	case QQ_GROUP_JOIN_OK:
		purple_debug(PURPLE_DEBUG_INFO, "QQ", "Succeed joining group \"%s\"\n", group->group_name_utf8);
		group->my_status = QQ_GROUP_MEMBER_STATUS_IS_MEMBER;
		qq_group_refresh(gc, group);
		/* this must be shown before getting online members */
		qq_group_conv_show_window(gc, group);
		qq_send_room_cmd_only(gc, QQ_ROOM_CMD_GET_INFO, group->id);
		break;
	case QQ_GROUP_JOIN_NEED_AUTH:
		purple_debug(PURPLE_DEBUG_INFO, "QQ",
			   "Fail joining group [%d] %s, needs authentication\n",
			   group->ext_id, group->group_name_utf8);
		group->my_status = QQ_GROUP_MEMBER_STATUS_NOT_MEMBER;
		qq_group_refresh(gc, group);
		_qq_group_join_auth(gc, group);
		break;
	default:
		purple_debug(PURPLE_DEBUG_INFO, "QQ",
			   "Error joining group [%d] %s, unknown reply: 0x%02x\n",
			   group->ext_id, group->group_name_utf8, reply);
	}
}

/* Attempt to join a group without auth */
void qq_group_join(PurpleConnection *gc, GHashTable *data)
{
	qq_data *qd;
	gchar *ext_id_ptr;
	guint32 ext_id;
	qq_group *group;

	g_return_if_fail(data != NULL);
	qd = (qq_data *) gc->proto_data;

	ext_id_ptr = g_hash_table_lookup(data, QQ_GROUP_KEY_EXTERNAL_ID);
	g_return_if_fail(ext_id_ptr != NULL);
	errno = 0;
	ext_id = strtol(ext_id_ptr, NULL, 10);
	if (errno != 0) {
		purple_notify_error(gc, _("Error"),
				_("You entered a group ID outside the acceptable range"), NULL);
		return;
	}

	group = qq_room_search_ext_id(gc, ext_id);
	if (group) {
		qq_send_cmd_group_join_group(gc, group);
	} else {
		qq_set_pending_id(&qd->joining_groups, ext_id, TRUE);
		qq_send_cmd_group_search_group(gc, ext_id);
	}
}

void qq_group_exit(PurpleConnection *gc, GHashTable *data)
{
	gchar *id_ptr;
	guint32 id;
	gc_and_uid *g;

	g_return_if_fail(data != NULL);

	id_ptr = g_hash_table_lookup(data, QQ_GROUP_KEY_INTERNAL_ID);
	id = strtol(id_ptr, NULL, 10);

	g_return_if_fail(id > 0);

	g = g_new0(gc_and_uid, 1);
	g->gc = gc;
	g->uid = id;

	purple_request_action(gc, _("QQ Qun Operation"),
			    _("Are you sure you want to leave this Qun?"),
			    _
			    ("Note, if you are the creator, \nthis operation will eventually remove this Qun."),
			    1,
				purple_connection_get_account(gc), NULL, NULL,
			    g, 2, _("Cancel"),
			    G_CALLBACK(qq_do_nothing_with_gc_and_uid),
			    _("Continue"), G_CALLBACK(_qq_group_exit_with_gc_and_id));
}
