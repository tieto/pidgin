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
#include "group_network.h"
#include "group_search.h"

enum {
	QQ_GROUP_JOIN_OK = 0x01,
	QQ_GROUP_JOIN_NEED_AUTH = 0x02,
};

static void _qq_group_exit_with_gc_and_id(gc_and_uid *g)
{
	GaimConnection *gc;
	guint32 internal_group_id;
	qq_group *group;

	gc = g->gc;
	internal_group_id = g->uid;

	group = qq_group_find_by_id(gc, internal_group_id, QQ_INTERNAL_ID);
	g_return_if_fail(group != NULL);

	qq_send_cmd_group_exit_group(gc, group);
}

/* send packet to join a group without auth */
void qq_send_cmd_group_join_group(GaimConnection *gc, qq_group *group)
{
	guint8 *raw_data, *cursor;
	gint bytes, data_len;

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
		gaim_notify_warning(gc, NULL, _("This group does not allow others to join"), NULL);
		return;
	default:
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Unknown group auth type: %d\n", group->auth_type);
		break;
	}

	data_len = 5;
	raw_data = g_newa(guint8, data_len);
	cursor = raw_data;

	bytes = 0;
	bytes += create_packet_b(raw_data, &cursor, QQ_GROUP_CMD_JOIN_GROUP);
	bytes += create_packet_dw(raw_data, &cursor, group->internal_group_id);

	if (bytes != data_len)
		gaim_debug(GAIM_DEBUG_ERROR, "QQ",
			   "Fail create packet for %s\n", qq_group_cmd_get_desc(QQ_GROUP_CMD_JOIN_GROUP));
	else
		qq_send_group_cmd(gc, group, raw_data, data_len);
}

static void _qq_group_join_auth_with_gc_and_id(gc_and_uid *g, const gchar *reason_utf8)
{
	GaimConnection *gc;
	qq_group *group;
	guint32 internal_group_id;

	gc = g->gc;
	internal_group_id = g->uid;

	group = qq_group_find_by_id(gc, internal_group_id, QQ_INTERNAL_ID);
	if (group == NULL) {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Can not find qq_group by internal_id: %d\n", internal_group_id);
		return;
	} else {		/* everything is OK */
		qq_send_cmd_group_auth(gc, group, QQ_GROUP_AUTH_REQUEST_APPLY, 0, reason_utf8);
	}
}

static void _qq_group_join_auth(GaimConnection *gc, qq_group *group)
{
	gchar *msg;
	gc_and_uid *g;
	g_return_if_fail(group != NULL);

	gaim_debug(GAIM_DEBUG_INFO, "QQ", 
			"Group (internal id: %d) needs authentication\n", group->internal_group_id);

	msg = g_strdup_printf("Group \"%s\" needs authentication\n", group->group_name_utf8);
	g = g_new0(gc_and_uid, 1);
	g->gc = gc;
	g->uid = group->internal_group_id;
	gaim_request_input(gc, NULL, msg,
			   _("Input request here"),
			   _("Would you be my friend?"), TRUE, FALSE, NULL,
			   _("Send"),
			   G_CALLBACK(_qq_group_join_auth_with_gc_and_id),
			   _("Cancel"), G_CALLBACK(qq_do_nothing_with_gc_and_uid), g);
	g_free(msg);
}

void qq_send_cmd_group_auth(GaimConnection *gc, qq_group *group, guint8 opt, guint32 uid, const gchar *reason_utf8)
{
	guint8 *raw_data, *cursor;
	gchar *reason_qq;
	gint bytes, data_len;

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

	data_len = 10 + strlen(reason_qq) + 1;
	raw_data = g_newa(guint8, data_len);
	cursor = raw_data;

	bytes = 0;
	bytes += create_packet_b(raw_data, &cursor, QQ_GROUP_CMD_JOIN_GROUP_AUTH);
	bytes += create_packet_dw(raw_data, &cursor, group->internal_group_id);
	bytes += create_packet_b(raw_data, &cursor, opt);
	bytes += create_packet_dw(raw_data, &cursor, uid);
	bytes += create_packet_b(raw_data, &cursor, strlen(reason_qq));
	bytes += create_packet_data(raw_data, &cursor, (guint8 *) reason_qq, strlen(reason_qq));

	if (bytes != data_len)
		gaim_debug(GAIM_DEBUG_ERROR, "QQ",
			   "Fail create packet for %s\n", qq_group_cmd_get_desc(QQ_GROUP_CMD_JOIN_GROUP_AUTH));
	else
		qq_send_group_cmd(gc, group, raw_data, data_len);
}

/* send a packet to exit a group */
void qq_send_cmd_group_exit_group(GaimConnection *gc, qq_group *group)
{
	guint8 *raw_data, *cursor;
	gint bytes, data_len;

	g_return_if_fail(group != NULL);

	data_len = 5;
	raw_data = g_newa(guint8, data_len);
	cursor = raw_data;

	bytes = 0;
	bytes += create_packet_b(raw_data, &cursor, QQ_GROUP_CMD_EXIT_GROUP);
	bytes += create_packet_dw(raw_data, &cursor, group->internal_group_id);

	if (bytes != data_len)
		gaim_debug(GAIM_DEBUG_ERROR, "QQ",
			   "Fail create packet for %s\n", qq_group_cmd_get_desc(QQ_GROUP_CMD_EXIT_GROUP));
	else
		qq_send_group_cmd(gc, group, raw_data, data_len);
}

/* If comes here, cmd is OK already */
void qq_process_group_cmd_exit_group(guint8 *data, guint8 **cursor, gint len, GaimConnection *gc)
{
	gint bytes, expected_bytes;
	guint32 internal_group_id;
	GaimChat *chat;
	qq_group *group;
	qq_data *qd;

	g_return_if_fail(data != NULL && len > 0);
	qd = (qq_data *) gc->proto_data;

	bytes = 0;
	expected_bytes = 4;
	bytes += read_packet_dw(data, cursor, len, &internal_group_id);

	if (bytes == expected_bytes) {
		group = qq_group_find_by_id(gc, internal_group_id, QQ_INTERNAL_ID);
		if (group != NULL) {
			chat =
			    gaim_blist_find_chat
			    (gaim_connection_get_account(gc), g_strdup_printf("%d", group->external_group_id));
			if (chat != NULL)
				gaim_blist_remove_chat(chat);
			qq_group_delete_internal_record(qd, internal_group_id);
		}
		gaim_notify_info(gc, _("QQ Qun Operation"), _("You have successfully exited the group"), NULL);
	} else {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ",
			   "Invalid exit group reply, expect %d bytes, read %d bytes\n", expected_bytes, bytes);
	}
}

/* Process the reply to group_auth subcmd */
void qq_process_group_cmd_join_group_auth(guint8 *data, guint8 **cursor, gint len, GaimConnection *gc)
{
	gint bytes, expected_bytes;
	guint32 internal_group_id;
	qq_data *qd;

	g_return_if_fail(data != NULL && len > 0);
	qd = (qq_data *) gc->proto_data;

	bytes = 0;
	expected_bytes = 4;
	bytes += read_packet_dw(data, cursor, len, &internal_group_id);
	g_return_if_fail(internal_group_id > 0);

	if (bytes == expected_bytes)
		gaim_notify_info
		    (gc, _("QQ Group Auth"), 
		     _("Your authorization operation has been accepted by the QQ server"), NULL);
	else
		gaim_debug(GAIM_DEBUG_ERROR, "QQ",
			   "Invalid join group reply, expect %d bytes, read %d bytes\n", expected_bytes, bytes);
}

/* process group cmd reply "join group" */
void qq_process_group_cmd_join_group(guint8 *data, guint8 **cursor, gint len, GaimConnection *gc)
{
	gint bytes, expected_bytes;
	guint32 internal_group_id;
	guint8 reply;
	qq_group *group;

	g_return_if_fail(data != NULL && len > 0);

	bytes = 0;
	expected_bytes = 5;
	bytes += read_packet_dw(data, cursor, len, &internal_group_id);
	bytes += read_packet_b(data, cursor, len, &reply);

	if (bytes != expected_bytes) {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ",
			   "Invalid join group reply, expect %d bytes, read %d bytes\n", expected_bytes, bytes);
		return;
	} else {		/* join group OK */
		group = qq_group_find_by_id(gc, internal_group_id, QQ_INTERNAL_ID);
		/* need to check if group is NULL or not. */
		g_return_if_fail(group != NULL);
		switch (reply) {
		case QQ_GROUP_JOIN_OK:
			gaim_debug(GAIM_DEBUG_INFO, "QQ", "Succeed joining group \"%s\"\n", group->group_name_utf8);
			group->my_status = QQ_GROUP_MEMBER_STATUS_IS_MEMBER;
			qq_group_refresh(gc, group);
			/* this must be shown before getting online members */
			qq_group_conv_show_window(gc, group);
			qq_send_cmd_group_get_group_info(gc, group);
			break;
		case QQ_GROUP_JOIN_NEED_AUTH:
			gaim_debug(GAIM_DEBUG_INFO, "QQ",
				   "Fail joining group [%d] %s, needs authentication\n",
				   group->external_group_id, group->group_name_utf8);
			group->my_status = QQ_GROUP_MEMBER_STATUS_NOT_MEMBER;
			qq_group_refresh(gc, group);
			_qq_group_join_auth(gc, group);
			break;
		default:
			gaim_debug(GAIM_DEBUG_INFO, "QQ",
				   "Error joining group [%d] %s, unknown reply: 0x%02x\n",
				   group->external_group_id, group->group_name_utf8, reply);
		}
	}
}

/* Attempt to join a group without auth */
void qq_group_join(GaimConnection *gc, GHashTable *data)
{
	qq_data *qd;
	gchar *external_group_id_ptr;
	guint32 external_group_id;
	qq_group *group;

	g_return_if_fail(data != NULL);
	qd = (qq_data *) gc->proto_data;

	external_group_id_ptr = g_hash_table_lookup(data, QQ_GROUP_KEY_EXTERNAL_ID);
	g_return_if_fail(external_group_id_ptr != NULL);
	errno = 0;
	external_group_id = strtol(external_group_id_ptr, NULL, 10);
	if (errno != 0) {
		gaim_notify_error(gc, _("Error"), 
				_("You inputted a group id outside the acceptable range"), NULL);
		return;
	}

	group = qq_group_find_by_id(gc, external_group_id, QQ_EXTERNAL_ID);
	if (group) {
		qq_send_cmd_group_join_group(gc, group);
	} else {
		qq_set_pending_id(&qd->joining_groups, external_group_id, TRUE);
		qq_send_cmd_group_search_group(gc, external_group_id);
	}
}

void qq_group_exit(GaimConnection *gc, GHashTable *data)
{
	gchar *internal_group_id_ptr;
	guint32 internal_group_id;
	gc_and_uid *g;

	g_return_if_fail(data != NULL);

	internal_group_id_ptr = g_hash_table_lookup(data, "internal_group_id");
	internal_group_id = strtol(internal_group_id_ptr, NULL, 10);

	g_return_if_fail(internal_group_id > 0);

	g = g_new0(gc_and_uid, 1);
	g->gc = gc;
	g->uid = internal_group_id;

	gaim_request_action(gc, _("QQ Qun Operation"),
			    _("Are you sure to exit this Qun?"),
			    _
			    ("Note, if you are the creator, \nthis operation will eventually remove this Qun."),
			    1, g, 2, _("Cancel"),
			    G_CALLBACK(qq_do_nothing_with_gc_and_uid),
			    _("Go ahead"), G_CALLBACK(_qq_group_exit_with_gc_and_id));
}
