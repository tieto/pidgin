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

#include "char_conv.h"
#include "group_conv.h"
#include "group_find.h"
#include "group_internal.h"
#include "group_info.h"
#include "group_join.h"
#include "group_opt.h"
#include "group_conv.h"
#include "group_search.h"
#include "header_info.h"
#include "packet_parse.h"
#include "qq_network.h"
#include "qq_process.h"

enum {
	QQ_ROOM_JOIN_OK = 0x01,
	QQ_ROOM_JOIN_NEED_AUTH = 0x02,
	QQ_ROOM_JOIN_DENIED = 0x03,
};

static void group_quit_cb(qq_add_request *add_req)
{
	PurpleConnection *gc;
	guint32 id;
	qq_group *group;

	if (add_req->gc == NULL || add_req->uid == 0) {
		g_free(add_req);
		return;
	}

	gc = add_req->gc;
	id = add_req->uid;

	group = qq_room_search_id(gc, id);
	if (group == NULL) {
		g_free(add_req);
		return;
	}

	qq_send_room_cmd_only(gc, QQ_ROOM_CMD_QUIT, group->id);
	g_free(add_req);
}

/* send packet to join a group without auth */
void qq_request_room_join(PurpleConnection *gc, qq_group *group)
{
	g_return_if_fail(group != NULL);

	if (group->my_role == QQ_ROOM_ROLE_NO) {
		group->my_role = QQ_ROOM_ROLE_REQUESTING;
		qq_group_refresh(gc, group);
	}

	switch (group->auth_type) {
	case QQ_ROOM_AUTH_TYPE_NO_AUTH:
	case QQ_ROOM_AUTH_TYPE_NEED_AUTH:
		break;
	case QQ_ROOM_AUTH_TYPE_NO_ADD:
		if (group->my_role == QQ_ROOM_ROLE_NO
				&& group->my_role == QQ_ROOM_ROLE_REQUESTING) {
			purple_notify_warning(gc, NULL, _("The Qun does not allow others to join"), NULL);
			return;
		}
		break;
	default:
		purple_debug_error("QQ", "Unknown room auth type: %d\n", group->auth_type);
		break;
	}

	qq_send_room_cmd_only(gc, QQ_ROOM_CMD_JOIN, group->id);
}

static void group_join_cb(qq_add_request *add_req, const gchar *reason_utf8)
{
	qq_group *group;

	g_return_if_fail(add_req != NULL);
	if (add_req->gc == NULL || add_req->uid == 0) {
		g_free(add_req);
		return;
	}

	group = qq_room_search_id(add_req->gc, add_req->uid);
	if (group == NULL) {
		purple_debug_error("QQ", "Can not find qq_group by internal_id: %d\n", add_req->uid);
		g_free(add_req);
		return;
	}

	qq_send_cmd_group_auth(add_req->gc, group, QQ_ROOM_AUTH_REQUEST_APPLY, 0, reason_utf8);
	g_free(add_req);
}

void qq_group_cancel_cb(qq_add_request *add_req, const gchar *msg)
{
	g_return_if_fail(add_req != NULL);
	g_free(add_req);
}

static void _qq_group_join_auth(PurpleConnection *gc, qq_group *group)
{
	gchar *msg;
	qq_add_request *add_req;
	g_return_if_fail(group != NULL);

	purple_debug_info("QQ", "Group (internal id: %d) needs authentication\n", group->id);

	msg = g_strdup_printf("Group \"%s\" needs authentication\n", group->title_utf8);
	add_req = g_new0(qq_add_request, 1);
	add_req->gc = gc;
	add_req->uid = group->id;
	purple_request_input(gc, NULL, msg,
			   _("Input request here"),
			   _("Would you be my friend?"), TRUE, FALSE, NULL,
			   _("Send"),
			   G_CALLBACK(group_join_cb),
			   _("Cancel"), G_CALLBACK(qq_group_cancel_cb),
			   purple_connection_get_account(gc), group->title_utf8, NULL,
			   add_req);
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

	if (opt == QQ_ROOM_AUTH_REQUEST_APPLY) {
		group->my_role = QQ_ROOM_ROLE_REQUESTING;
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
		purple_debug_error("QQ", "Invalid exit group reply, expect %d bytes, read %d bytes\n", 4, len);
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
	purple_notify_info(gc, _("QQ Qun Operation"), _("Successed:"), _("Remove from Qun"));
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
		purple_debug_error("QQ",
			"Invalid join room reply, expect %d bytes, read %d bytes\n", 4, len);
		return;
	}
	bytes = 0;
	bytes += qq_get32(&id, data + bytes);
	g_return_if_fail(id > 0);

	purple_notify_info(gc, _("QQ Qun Operation"), _("Successed:"), _("Join to Qun"));
}

/* process group cmd reply "join group" */
void qq_process_group_cmd_join_group(guint8 *data, gint len, PurpleConnection *gc)
{
	gint bytes;
	guint32 id;
	guint8 reply;
	qq_group *group;
	gchar *msg;

	g_return_if_fail(data != NULL && len > 0);

	if (len < 5) {
		purple_debug_error("QQ",
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
	case QQ_ROOM_JOIN_OK:
		purple_debug_info("QQ", "Successed in joining group \"%s\"\n", group->title_utf8);
		group->my_role = QQ_ROOM_ROLE_YES;
		qq_group_refresh(gc, group);
		/* this must be shown before getting online members */
		qq_room_conv_create(gc, group);
		break;
	case QQ_ROOM_JOIN_NEED_AUTH:
		purple_debug_info("QQ",
			   "Fail joining group [%d] %s, needs authentication\n",
			   group->ext_id, group->title_utf8);
		group->my_role = QQ_ROOM_ROLE_NO;
		qq_group_refresh(gc, group);
		_qq_group_join_auth(gc, group);
		break;
	case QQ_ROOM_JOIN_DENIED:
		msg = g_strdup_printf(_("Qun %d denied to join"), group->ext_id);
		purple_notify_info(gc, _("QQ Qun Operation"), _("Failed:"), msg);
		g_free(msg);
		break;
	default:
		purple_debug_info("QQ",
			   "Failed joining group [%d] %s, unknown reply: 0x%02x\n",
			   group->ext_id, group->title_utf8, reply);

		purple_notify_info(gc, _("QQ Qun Operation"), _("Failed:"), _("Join Qun, Unknow Reply"));
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

	ext_id_ptr = g_hash_table_lookup(data, QQ_ROOM_KEY_EXTERNAL_ID);
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
		qq_request_room_join(gc, group);
	} else {
		qq_set_pending_id(&qd->joining_groups, ext_id, TRUE);
		qq_send_cmd_group_search_group(gc, ext_id);
	}
}

void qq_group_exit(PurpleConnection *gc, GHashTable *data)
{
	gchar *id_ptr;
	guint32 id;
	qq_add_request *add_req;

	g_return_if_fail(data != NULL);

	id_ptr = g_hash_table_lookup(data, QQ_ROOM_KEY_INTERNAL_ID);
	id = strtol(id_ptr, NULL, 10);

	g_return_if_fail(id > 0);

	add_req = g_new0(qq_add_request, 1);
	add_req->gc = gc;
	add_req->uid = id;

	purple_request_action(gc, _("QQ Qun Operation"),
			    _("Are you sure you want to leave this Qun?"),
			    _("Note, if you are the creator, \nthis operation will eventually remove this Qun."),
			    1,
				purple_connection_get_account(gc), NULL, NULL,
			    add_req, 2, _("Cancel"),
			    G_CALLBACK(qq_group_cancel_cb),
			    _("Continue"), G_CALLBACK(group_quit_cb));
}
