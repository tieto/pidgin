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
#include "im.h"
#include "group_internal.h"
#include "group_info.h"
#include "group_join.h"
#include "group_opt.h"
#include "group_im.h"
#include "qq_define.h"
#include "packet_parse.h"
#include "qq_network.h"
#include "qq_process.h"

enum {
	QQ_ROOM_JOIN_OK = 0x01,
	QQ_ROOM_JOIN_NEED_AUTH = 0x02,
	QQ_ROOM_JOIN_DENIED = 0x03
};

enum {
	QQ_ROOM_SEARCH_TYPE_BY_ID = 0x01,
	QQ_ROOM_SEARCH_TYPE_DEMO = 0x02
};

static void group_quit_cb(qq_room_req *add_req)
{
	PurpleConnection *gc;
	guint32 id;
	qq_room_data *rmd;

	if (add_req->gc == NULL || add_req->id == 0) {
		g_free(add_req);
		return;
	}

	gc = add_req->gc;
	id = add_req->id;

	rmd = qq_room_data_find(gc, id);
	if (rmd == NULL) {
		g_free(add_req);
		return;
	}

	qq_send_room_cmd_only(gc, QQ_ROOM_CMD_QUIT, rmd->id);
	g_free(add_req);
}

/* send packet to join a group without auth */
void qq_request_room_join(PurpleConnection *gc, qq_room_data *rmd)
{
	g_return_if_fail(rmd != NULL);

	if (rmd->my_role == QQ_ROOM_ROLE_NO) {
		rmd->my_role = QQ_ROOM_ROLE_REQUESTING;
	}

	switch (rmd->auth_type) {
	case QQ_ROOM_AUTH_TYPE_NO_AUTH:
	case QQ_ROOM_AUTH_TYPE_NEED_AUTH:
		break;
	case QQ_ROOM_AUTH_TYPE_NO_ADD:
		if (rmd->my_role == QQ_ROOM_ROLE_NO
				&& rmd->my_role == QQ_ROOM_ROLE_REQUESTING) {
			purple_notify_warning(gc, NULL, _("The Qun does not allow others to join"), NULL);
			return;
		}
		break;
	default:
		purple_debug_error("QQ", "Unknown room auth type: %d\n", rmd->auth_type);
		break;
	}

	qq_send_room_cmd_only(gc, QQ_ROOM_CMD_JOIN, rmd->id);
}

static void group_join_cb(qq_room_req *add_req, const gchar *reason_utf8)
{
	qq_room_data *rmd;

	g_return_if_fail(add_req != NULL);
	if (add_req->gc == NULL || add_req->id == 0) {
		g_free(add_req);
		return;
	}

	rmd = qq_room_data_find(add_req->gc, add_req->id);
	if (rmd == NULL) {
		purple_debug_error("QQ", "Can not find room data of %u\n", add_req->id);
		g_free(add_req);
		return;
	}

	qq_send_cmd_group_auth(add_req->gc, rmd, QQ_ROOM_AUTH_REQUEST_APPLY, 0, reason_utf8);
	g_free(add_req);
}

static void room_join_cancel_cb(qq_room_req *add_req, const gchar *msg)
{
	g_return_if_fail(add_req != NULL);
	g_free(add_req);
}

static void do_room_join_request(PurpleConnection *gc, qq_room_data *rmd)
{
	gchar *msg;
	qq_room_req *add_req;
	g_return_if_fail(rmd != NULL);

	purple_debug_info("QQ", "Room id %u needs authentication\n", rmd->id);

	msg = g_strdup_printf("QQ Qun %u needs authentication\n", rmd->ext_id);
	add_req = g_new0(qq_room_req, 1);
	add_req->gc = gc;
	add_req->id = rmd->id;
	purple_request_input(gc, _("Join QQ Qun"), msg,
			   _("Input request here"),
			   _("Would you be my friend?"), TRUE, FALSE, NULL,
			   _("Send"),
			   G_CALLBACK(group_join_cb),
			   _("Cancel"), G_CALLBACK(room_join_cancel_cb),
			   purple_connection_get_account(gc), rmd->title_utf8, NULL,
			   add_req);
	g_free(msg);
}

void qq_send_cmd_group_auth(PurpleConnection *gc, qq_room_data *rmd,
		guint8 opt, guint32 uid, const gchar *reason_utf8)
{
	guint8 raw_data[MAX_PACKET_SIZE - 16];
	gint bytes;

	g_return_if_fail(rmd != NULL);

	if (opt == QQ_ROOM_AUTH_REQUEST_APPLY) {
		rmd->my_role = QQ_ROOM_ROLE_REQUESTING;
		uid = 0;
	}

	bytes = 0;
	bytes += qq_put8(raw_data + bytes, opt);
	bytes += qq_put32(raw_data + bytes, uid);
	bytes += qq_put_vstr(raw_data + bytes, reason_utf8, QQ_CHARSET_DEFAULT);

	qq_send_room_cmd(gc, QQ_ROOM_CMD_AUTH, rmd->id, raw_data, bytes);
}

/* If comes here, cmd is OK already */
void qq_process_group_cmd_exit_group(guint8 *data, gint len, PurpleConnection *gc)
{
	gint bytes;
	guint32 id;

	g_return_if_fail(data != NULL && len > 0);

	if (len < 4) {
		purple_debug_error("QQ", "Invalid exit group reply, expect %d bytes, read %d bytes\n", 4, len);
		return;
	}

	bytes = 0;
	bytes += qq_get32(&id, data + bytes);

	qq_room_remove(gc, id);
}

/* Process the reply to group_auth subcmd */
void qq_process_group_cmd_join_group_auth(guint8 *data, gint len, PurpleConnection *gc)
{
	gint bytes;
	guint32 id;
	qq_room_data *rmd;
	gchar *msg;

	g_return_if_fail(data != NULL && len > 0);

	if (len < 4) {
		purple_debug_error("QQ",
			"Invalid join room reply, expect %d bytes, read %d bytes\n", 4, len);
		return;
	}
	bytes = 0;
	bytes += qq_get32(&id, data + bytes);
	g_return_if_fail(id > 0);

	rmd = qq_room_data_find(gc, id);
	if (rmd != NULL) {
		msg = g_strdup_printf(_("Successfully joined Qun %s (%u)"), rmd->title_utf8, rmd->ext_id);
		qq_got_message(gc, msg);
		g_free(msg);
	} else {
		qq_got_message(gc, _("Successfully joined Qun"));
	}
}

/* process group cmd reply "join group" */
void qq_process_group_cmd_join_group(guint8 *data, gint len, PurpleConnection *gc)
{
	gint bytes;
	guint32 id;
	guint8 reply;
	qq_room_data *rmd;
	gchar *msg;

	g_return_if_fail(data != NULL && len > 0);

	if (len < 5) {
		purple_debug_error("QQ",
			   "Invalid join room reply, expect %d bytes, read %d bytes\n", 5, len);
		return;
	}

	bytes = 0;
	bytes += qq_get32(&id, data + bytes);
	bytes += qq_get8(&reply, data + bytes);

	/* join group OK */
	rmd = qq_room_data_find(gc, id);
	/* need to check if group is NULL or not. */
	g_return_if_fail(rmd != NULL);
	switch (reply) {
	case QQ_ROOM_JOIN_OK:
		purple_debug_info("QQ", "Succeeded in joining group \"%s\"\n", rmd->title_utf8);
		rmd->my_role = QQ_ROOM_ROLE_YES;
		/* this must be shown before getting online members */
		qq_room_conv_open(gc, rmd);
		break;
	case QQ_ROOM_JOIN_NEED_AUTH:
		purple_debug_info("QQ",
			   "Failed to join room ext id %u %s, needs authentication\n",
			   rmd->ext_id, rmd->title_utf8);
		rmd->my_role = QQ_ROOM_ROLE_NO;
		do_room_join_request(gc, rmd);
		break;
	case QQ_ROOM_JOIN_DENIED:
		msg = g_strdup_printf(_("Qun %u denied from joining"), rmd->ext_id);
		purple_notify_info(gc, _("QQ Qun Operation"), _("Failed:"), msg);
		g_free(msg);
		break;
	default:
		purple_debug_info("QQ",
			   "Failed to join room ext id %u %s, unknown reply: 0x%02x\n",
			   rmd->ext_id, rmd->title_utf8, reply);

		purple_notify_info(gc, _("QQ Qun Operation"), _("Failed:"), _("Join Qun, Unknown Reply"));
	}
}

/* Attempt to join a group without auth */
void qq_group_join(PurpleConnection *gc, GHashTable *data)
{
	gchar *ext_id_str;
	gchar *id_str;
	guint32 ext_id;
	guint32 id;
	qq_room_data *rmd;

	g_return_if_fail(data != NULL);

	ext_id_str = g_hash_table_lookup(data, QQ_ROOM_KEY_EXTERNAL_ID);
	id_str = g_hash_table_lookup(data, QQ_ROOM_KEY_INTERNAL_ID);
	purple_debug_info("QQ", "Join room %s, extend id %s\n", id_str, ext_id_str);

	if (id_str != NULL) {
		id = strtoul(id_str, NULL, 10);
		if (id != 0) {
			rmd = qq_room_data_find(gc, id);
			if (rmd) {
				qq_request_room_join(gc, rmd);
				return;
			}
		}
	}

	purple_debug_info("QQ", "Search and join extend id %s\n", ext_id_str);
	if (ext_id_str == NULL) {
		return;
	}
	ext_id = strtoul(ext_id_str, NULL, 10);
	if (ext_id == 0) {
		return;
	}

	qq_request_room_search(gc, ext_id, QQ_ROOM_SEARCH_FOR_JOIN);
}

void qq_room_quit(PurpleConnection *gc, guint32 room_id)
{
	qq_room_req *add_req;

	add_req = g_new0(qq_room_req, 1);
	add_req->gc = gc;
	add_req->id = room_id;

	purple_request_action(gc, _("QQ Qun Operation"),
			    _("Quit Qun"),
			    _("Note, if you are the creator, \nthis operation will eventually remove this Qun."),
			    1,
				purple_connection_get_account(gc), NULL, NULL,
			    add_req, 2, _("Cancel"),
			    G_CALLBACK(room_join_cancel_cb),
			    _("Continue"), G_CALLBACK(group_quit_cb));
}

/* send packet to search for qq_group */
void qq_request_room_search(PurpleConnection *gc, guint32 ext_id, int action)
{
	guint8 raw_data[16] = {0};
	gint bytes = 0;
	guint8 type;

	purple_debug_info("QQ", "Search QQ Qun %u\n", ext_id);
	type = (ext_id == 0x00000000) ? QQ_ROOM_SEARCH_TYPE_DEMO : QQ_ROOM_SEARCH_TYPE_BY_ID;

	bytes = 0;
	bytes += qq_put8(raw_data + bytes, type);
	bytes += qq_put32(raw_data + bytes, ext_id);

	qq_send_room_cmd_mess(gc, QQ_ROOM_CMD_SEARCH, 0, raw_data, bytes, 0, action);
}

static void add_to_roomlist(qq_data *qd, qq_room_data *rmd)
{
	PurpleRoomlistRoom *room;
	gchar field[11];

	room = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_ROOM, rmd->title_utf8, NULL);
	g_snprintf(field, sizeof(field), "%u", rmd->ext_id);
	purple_roomlist_room_add_field(qd->roomlist, room, field);
	g_snprintf(field, sizeof(field), "%u", rmd->creator_uid);
	purple_roomlist_room_add_field(qd->roomlist, room, field);
	purple_roomlist_room_add_field(qd->roomlist, room, rmd->desc_utf8);
	g_snprintf(field, sizeof(field), "%u", rmd->id);
	purple_roomlist_room_add_field(qd->roomlist, room, field);
	g_snprintf(field, sizeof(field), "%d", rmd->type8);
	purple_roomlist_room_add_field(qd->roomlist, room, field);
	g_snprintf(field, sizeof(field), "%d", rmd->auth_type);
	purple_roomlist_room_add_field(qd->roomlist, room, field);
	g_snprintf(field, sizeof(field), "%d", rmd->category);
	purple_roomlist_room_add_field(qd->roomlist, room, field);
	purple_roomlist_room_add_field(qd->roomlist, room, rmd->title_utf8);
	purple_roomlist_room_add(qd->roomlist, room);

	purple_roomlist_set_in_progress(qd->roomlist, FALSE);
}

/* process group cmd reply "search group" */
void qq_process_room_search(PurpleConnection *gc, guint8 *data, gint len, guint32 ship32)
{
	qq_data *qd;
	qq_room_data rmd;
	PurpleChat *chat;
	gint bytes;
	guint8 search_type;
	guint16 unknown;

	g_return_if_fail(data != NULL && len > 0);
	qd = (qq_data *) gc->proto_data;

	bytes = 0;
	bytes += qq_get8(&search_type, data + bytes);

	/* now it starts with group_info_entry */
	bytes += qq_get32(&(rmd.id), data + bytes);
	bytes += qq_get32(&(rmd.ext_id), data + bytes);
	bytes += qq_get8(&(rmd.type8), data + bytes);
	bytes += qq_get16(&(unknown), data + bytes);
	bytes += qq_get16(&(unknown), data + bytes);
	bytes += qq_get32(&(rmd.creator_uid), data + bytes);
	bytes += qq_get16(&(unknown), data + bytes);
	bytes += qq_get16(&(unknown), data + bytes);
	bytes += qq_get16(&(unknown), data + bytes);
	bytes += qq_get32(&(rmd.category), data + bytes);
	bytes += qq_get_vstr(&(rmd.title_utf8), QQ_CHARSET_DEFAULT, data + bytes);
	bytes += qq_get16(&(unknown), data + bytes);
	bytes += qq_get8(&(rmd.auth_type), data + bytes);
	bytes += qq_get_vstr(&(rmd.desc_utf8), QQ_CHARSET_DEFAULT, data + bytes);
	/* end of one qq_group */
	if(bytes != len) {
		purple_debug_error("QQ",
			"group_cmd_search_group: Dangerous error! maybe protocol changed, notify developers!");
	}

	if (ship32 == QQ_ROOM_SEARCH_FOR_JOIN) {
		chat = qq_room_find_or_new(gc, rmd.id, rmd.ext_id);
		g_return_if_fail(chat != NULL);

		qq_room_update_chat_info(chat, &rmd);
		qq_request_room_join(gc, &rmd);
	} else {
		add_to_roomlist(qd, &rmd);
	}
}
