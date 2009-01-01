/**
 * @file group_opt.c
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

#include "qq.h"

#include "debug.h"
#include "notify.h"
#include "request.h"

#include "buddy_info.h"
#include "char_conv.h"
#include "group_internal.h"
#include "group_info.h"
#include "group_join.h"
#include "group_im.h"
#include "group_opt.h"
#include "qq_define.h"
#include "packet_parse.h"
#include "qq_network.h"
#include "qq_process.h"
#include "utils.h"

static int _compare_guint32(const void *a,
                            const void *b)
{
	const guint32 *x = a;
	const guint32 *y = b;
	return (*x - *y);
}

static void _sort(guint32 *list)
{
	gint i;
	for (i = 0; list[i] < 0xffffffff; i++) {;
	}
	qsort (list, i, sizeof (guint32), _compare_guint32);
}

static void _qq_group_member_opt(PurpleConnection *gc, qq_room_data *rmd, gint operation, guint32 *members)
{
	guint8 *data;
	gint i, count, data_len;
	gint bytes;
	g_return_if_fail(members != NULL);

	for (count = 0; members[count] != 0xffffffff; count++) {;
	}
	data_len = 6 + count * 4;
	data = g_newa(guint8, data_len);

	bytes = 0;
	bytes += qq_put8(data + bytes, operation);
	for (i = 0; i < count; i++)
		bytes += qq_put32(data + bytes, members[i]);

	qq_send_room_cmd(gc, QQ_ROOM_CMD_MEMBER_OPT, rmd->id, data, bytes);
}

static void room_req_cancel_cb(qq_room_req *add_req)
{
	if (add_req != NULL)
		g_free(add_req);
}

static void member_join_authorize_cb(gpointer data)
{
	qq_room_req *add_req = (qq_room_req *)data;
	qq_room_data *rmd;
	g_return_if_fail(add_req != NULL && add_req->gc != NULL);
	g_return_if_fail(add_req->id > 0 && add_req->member > 0);
	rmd = qq_room_data_find(add_req->gc, add_req->id);
	g_return_if_fail(rmd != NULL);

	qq_send_cmd_group_auth(add_req->gc, rmd, QQ_ROOM_AUTH_REQUEST_APPROVE, add_req->member, "");
	qq_room_buddy_find_or_new(add_req->gc, rmd, add_req->member);
	g_free(add_req);
}

static void member_join_deny_reason_cb(qq_room_req *add_req, gchar *msg_utf8)
{
	qq_room_data *rmd;
	g_return_if_fail(add_req != NULL && add_req->gc != NULL);
	g_return_if_fail(add_req->id > 0 && add_req->member > 0);
	rmd = qq_room_data_find(add_req->gc, add_req->id);
	g_return_if_fail(rmd != NULL);
	qq_send_cmd_group_auth(add_req->gc, rmd, QQ_ROOM_AUTH_REQUEST_REJECT, add_req->member, msg_utf8);
	g_free(add_req);
}

static void member_join_deny_noreason_cb(qq_room_req *add_req, gchar *msg_utf8)
{
	member_join_deny_reason_cb(add_req, NULL);
}

static void member_join_deny_cb(gpointer data)
{
	qq_room_req *add_req = (qq_room_req *)data;
	gchar *who;
	g_return_if_fail(add_req != NULL && add_req->gc != NULL);
	g_return_if_fail(add_req->id > 0 && add_req->member > 0);

	who = uid_to_purple_name(add_req->member);
	purple_request_input(add_req->gc, NULL, _("Authorization denied message:"),
			NULL, _("Sorry, you are not our style"), TRUE, FALSE, NULL,
			_("OK"), G_CALLBACK(member_join_deny_reason_cb),
			_("Cancel"), G_CALLBACK(member_join_deny_noreason_cb),
			purple_connection_get_account(add_req->gc), who, NULL,
			add_req);
	g_free(who);
}

void qq_group_modify_members(PurpleConnection *gc, qq_room_data *rmd, guint32 *new_members)
{
	guint32 *old_members, *del_members, *add_members;
	qq_buddy_data *bd;
	qq_data *qd;
	gint i = 0, old = 0, new = 0, del = 0, add = 0;
	GList *list;

	g_return_if_fail(rmd != NULL);
	qd = (qq_data *) gc->proto_data;
	if (new_members[0] == 0xffffffff)
		return;

	old_members = g_newa(guint32, QQ_QUN_MEMBER_MAX);
	del_members = g_newa(guint32, QQ_QUN_MEMBER_MAX);
	add_members = g_newa(guint32, QQ_QUN_MEMBER_MAX);

	/* construct the old member list */
	list = rmd->members;
	while (list != NULL) {
		bd = (qq_buddy_data *) list->data;
		if (bd != NULL)
			old_members[i++] = bd->uid;
		list = list->next;
	}
	old_members[i] = 0xffffffff;	/* this is the end */

	/* sort to speed up making del_members and add_members list */
	_sort(old_members);
	_sort(new_members);

	for (old = 0, new = 0; old_members[old] < 0xffffffff || new_members[new] < 0xffffffff;) {
		if (old_members[old] > new_members[new]) {
			add_members[add++] = new_members[new++];
		} else if (old_members[old] < new_members[new]) {
			del_members[del++] = old_members[old++];
		} else {
			if (old_members[old] < 0xffffffff)
				old++;
			if (new_members[new] < 0xffffffff)
				new++;
		}
	}
	del_members[del] = add_members[add] = 0xffffffff;

	for (i = 0; i < del; i++)
		qq_room_buddy_remove(rmd, del_members[i]);
	for (i = 0; i < add; i++)
		qq_room_buddy_find_or_new(gc, rmd, add_members[i]);

	if (del > 0)
		_qq_group_member_opt(gc, rmd, QQ_ROOM_MEMBER_DEL, del_members);
	if (add > 0)
		_qq_group_member_opt(gc, rmd, QQ_ROOM_MEMBER_ADD, add_members);
}

void qq_group_process_modify_members_reply(guint8 *data, gint len, PurpleConnection *gc)
{
	gint bytes;
	guint32 id;
	time_t now = time(NULL);
	qq_room_data *rmd;
	g_return_if_fail(data != NULL);

	bytes = 0;
	bytes += qq_get32(&id, data + bytes);
	g_return_if_fail(id > 0);

	/* we should have its info locally */
	rmd = qq_room_data_find(gc, id);
	g_return_if_fail(rmd != NULL);

	purple_debug_info("QQ", "Succeed in modify members for room %u\n", rmd->ext_id);

	qq_room_got_chat_in(gc, id, 0, _("Successfully changed Qun members"), now);
}

void qq_room_change_info(PurpleConnection *gc, qq_room_data *rmd)
{
	guint8 data[MAX_PACKET_SIZE - 16];
	gint bytes;

	g_return_if_fail(rmd != NULL);

	bytes = 0;
	/* 005-005 */
	bytes += qq_put8(data + bytes, 0x01);
	/* 006-006 */
	bytes += qq_put8(data + bytes, rmd->auth_type);
	/* 007-008 */
	bytes += qq_put16(data + bytes, 0x0000);
	/* 009-010 */
	bytes += qq_put16(data + bytes, rmd->category);

	bytes += qq_put_vstr(data + bytes, rmd->title_utf8, QQ_CHARSET_DEFAULT);

	bytes += qq_put16(data + bytes, 0x0000);

	bytes += qq_put_vstr(data + bytes, rmd->notice_utf8, QQ_CHARSET_DEFAULT);
	bytes += qq_put_vstr(data + bytes, rmd->desc_utf8, QQ_CHARSET_DEFAULT);

	qq_send_room_cmd(gc, QQ_ROOM_CMD_CHANGE_INFO, rmd->id, data, bytes);
}

void qq_group_process_modify_info_reply(guint8 *data, gint len, PurpleConnection *gc)
{
	gint bytes;
	guint32 id;
	time_t now = time(NULL);

	g_return_if_fail(data != NULL);

	bytes = 0;
	bytes += qq_get32(&id, data + bytes);
	g_return_if_fail(id > 0);

	purple_debug_info("QQ", "Successfully modified room info of %u\n", id);

	qq_room_got_chat_in(gc, id, 0, _("Successfully changed Qun information"), now);
}

/* we create a very simple room first, and then let the user to modify */
void qq_create_room(PurpleConnection *gc, const gchar *name)
{
	guint8 *data;
	gint data_len;
	gint bytes;
	qq_data *qd;
	g_return_if_fail(name != NULL);

	qd = (qq_data *) gc->proto_data;

	data_len = 64 + strlen(name);
	data = g_newa(guint8, data_len);

	bytes = 0;
	/* we create the simpleset group, only group name is given */
	/* 001 */
	bytes += qq_put8(data + bytes, QQ_ROOM_TYPE_PERMANENT);
	/* 002 */
	bytes += qq_put8(data + bytes, QQ_ROOM_AUTH_TYPE_NEED_AUTH);
	/* 003-004 */
	bytes += qq_put16(data + bytes, 0x0000);
	/* 005-006 */
	bytes += qq_put16(data + bytes, 0x0003);
	/* 007 */
	bytes += qq_put8(data + bytes, strlen(name));
	bytes += qq_putdata(data + bytes, (guint8 *) name, strlen(name));
	bytes += qq_put16(data + bytes, 0x0000);
	bytes += qq_put8(data + bytes, 0x00);	/* no group notice */
	bytes += qq_put8(data + bytes, 0x00);	/* no group desc */
	bytes += qq_put32(data + bytes, qd->uid);	/* I am member of coz */

	if (bytes > data_len) {
		purple_debug_error("QQ",
			   "Overflow in qq_room_create, max %d bytes, now %d bytes\n",
			   data_len, bytes);
		return;
	}
	qq_send_room_cmd_noid(gc, QQ_ROOM_CMD_CREATE, data, bytes);
}

static void room_create_cb(qq_room_req *add_req)
{
	qq_room_data *rmd;
	g_return_if_fail(add_req != NULL);
	if (add_req->gc == NULL || add_req->id == 0) {
		g_free(add_req);
		return;
	}

	rmd = qq_room_data_find(add_req->gc, add_req->id);
	if (rmd == NULL) {
		g_free(add_req);
		return;
	}

	/* TODO insert UI code here */
	/* qq_group_detail_window_show(g->gc, rmd); */
	g_free(add_req);
}

void qq_group_process_create_group_reply(guint8 *data, gint len, PurpleConnection *gc)
{
	gint bytes;
	guint32 id, ext_id;
	qq_room_data *rmd;
	qq_room_req *add_req;
	qq_data *qd;

	g_return_if_fail(data != NULL);
	g_return_if_fail(gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	bytes = 0;
	bytes += qq_get32(&id, data + bytes);
	bytes += qq_get32(&ext_id, data + bytes);
	g_return_if_fail(id > 0 && ext_id);

	qq_room_find_or_new(gc, id, ext_id);
	rmd = qq_room_data_find(gc, id);
	g_return_if_fail(rmd != NULL);

	rmd->my_role = QQ_ROOM_ROLE_ADMIN;
	rmd->creator_uid = qd->uid;

	qq_send_room_cmd_only(gc, QQ_ROOM_CMD_ACTIVATE, id);
	qq_update_room(gc, 0, rmd->id);

	purple_debug_info("QQ", "Succeed in create Qun, ext id %u\n", rmd->ext_id);

	add_req = g_new0(qq_room_req, 1);
	add_req->gc = gc;
	add_req->id = id;

	purple_request_action(gc, _("QQ Qun Operation"),
			    _("You have successfully created a Qun"),
			    _("Would you like to set up detailed information now?"),
			    1,
				purple_connection_get_account(gc), NULL, NULL,
				add_req, 2,
				_("Setup"), G_CALLBACK(room_create_cb),
			    _("Cancel"), G_CALLBACK(room_req_cancel_cb));
}

void qq_group_process_activate_group_reply(guint8 *data, gint len, PurpleConnection *gc)
{
	gint bytes;
	guint32 id;
	qq_room_data *rmd;
	g_return_if_fail(data != NULL);

	bytes = 0;
	bytes += qq_get32(&id, data + bytes);
	g_return_if_fail(id > 0);

	/* we should have its info locally */
	rmd = qq_room_data_find(gc, id);
	g_return_if_fail(rmd != NULL);

	purple_debug_info("QQ", "Succeed in activate Qun %u\n", rmd->ext_id);
}

void qq_group_manage_group(PurpleConnection *gc, GHashTable *data)
{
	gchar *id_ptr;
	guint32 id;
	qq_room_data *rmd;

	g_return_if_fail(data != NULL);

	id_ptr = g_hash_table_lookup(data, QQ_ROOM_KEY_INTERNAL_ID);
	id = strtoul(id_ptr, NULL, 10);
	g_return_if_fail(id > 0);

	rmd = qq_room_data_find(gc, id);
	g_return_if_fail(rmd != NULL);

	/* XXX insert UI code here */
	/* qq_group_detail_window_show(gc, rmd); */
}

/* receive an application to join the group */
void qq_process_room_buddy_request_join(guint8 *data, gint len, guint32 id, PurpleConnection *gc)
{
	guint32 ext_id, member_id;
	guint8 type8;
	gchar *msg, *reason;
	qq_room_req *add_req;
	gchar *who;
	gint bytes = 0;
	qq_room_data *rmd;
	time_t now = time(NULL);

	g_return_if_fail(id > 0 && data != NULL && len > 0);

	/* FIXME: check length here */

	bytes += qq_get32(&ext_id, data + bytes);
	bytes += qq_get8(&type8, data + bytes);
	bytes += qq_get32(&member_id, data + bytes);

	g_return_if_fail(ext_id > 0 && member_id > 0);

	bytes += qq_get_vstr(&reason, QQ_CHARSET_DEFAULT, data + bytes);

	purple_debug_info("QQ", "%u requested to join room, ext id %u\n", member_id, ext_id);

	rmd = qq_room_data_find(gc, id);
	g_return_if_fail(rmd != NULL);
	if (qq_room_buddy_find(rmd, member_id)) {
		purple_debug_info("QQ", "Approve join, buddy joined before\n");
		msg = g_strdup_printf(_("%u requested to join Qun %u for %s"),
				member_id, ext_id, reason);
		qq_room_got_chat_in(gc, id, 0, msg, now);
		qq_send_cmd_group_auth(gc, rmd, QQ_ROOM_AUTH_REQUEST_APPROVE, member_id, "");
		g_free(msg);
		g_free(reason);
		return;
	}

	if (purple_prefs_get_bool("/plugins/prpl/qq/auto_get_authorize_info")) {
		qq_request_buddy_info(gc, member_id, 0, QQ_BUDDY_INFO_DISPLAY);
	}
	who = uid_to_purple_name(member_id);
	msg = g_strdup_printf(_("%u request to join Qun %u"), member_id, ext_id);

	add_req = g_new0(qq_room_req, 1);
	add_req->gc = gc;
	add_req->id = id;
	add_req->member = member_id;

	purple_request_action(gc, _("QQ Qun Operation"),
			msg, reason,
			PURPLE_DEFAULT_ACTION_NONE,
			purple_connection_get_account(gc), who, NULL,
			add_req, 2,
			_("Deny"), G_CALLBACK(member_join_deny_cb),
			_("Authorize"), G_CALLBACK(member_join_authorize_cb));

	g_free(who);
	g_free(msg);
	g_free(reason);
}

/* the request to join a group is rejected */
void qq_process_room_buddy_rejected(guint8 *data, gint len, guint32 id, PurpleConnection *gc)
{
	guint32 ext_id, admin_uid;
	guint8 type8;
	gchar *msg, *reason;
	qq_room_data *rmd;
	gint bytes;

	g_return_if_fail(data != NULL && len > 0);

	/* FIXME: check length here */
	bytes = 0;
	bytes += qq_get32(&ext_id, data + bytes);
	bytes += qq_get8(&type8, data + bytes);
	bytes += qq_get32(&admin_uid, data + bytes);

	g_return_if_fail(ext_id > 0 && admin_uid > 0);

	bytes += qq_get_vstr(&reason, QQ_CHARSET_DEFAULT, data + bytes);

	msg = g_strdup_printf
		(_("Failed to join Qun %u, operated by admin %u"), ext_id, admin_uid);

	purple_notify_warning(gc, _("QQ Qun Operation"), msg, reason);

	qq_room_find_or_new(gc, id, ext_id);
	rmd = qq_room_data_find(gc, id);
	if (rmd != NULL) {
		rmd->my_role = QQ_ROOM_ROLE_NO;
	}

	g_free(msg);
	g_free(reason);
}

/* the request to join a group is approved */
void qq_process_room_buddy_approved(guint8 *data, gint len, guint32 id, PurpleConnection *gc)
{
	guint32 ext_id, admin_uid;
	guint8 type8;
	gchar *msg, *reason;
	qq_room_data *rmd;
	gint bytes;
	time_t now;

	g_return_if_fail(data != NULL && len > 0);

	/* FIXME: check length here */
	bytes = 0;
	bytes += qq_get32(&ext_id, data + bytes);
	bytes += qq_get8(&type8, data + bytes);
	bytes += qq_get32(&admin_uid, data + bytes);

	g_return_if_fail(ext_id > 0 && admin_uid > 0);
	/* it is also a "无" here, so do not display */
	bytes += qq_get_vstr(&reason, QQ_CHARSET_DEFAULT, data + bytes);

	qq_room_find_or_new(gc, id, ext_id);
	rmd = qq_room_data_find(gc, id);
	if (rmd != NULL) {
		rmd->my_role = QQ_ROOM_ROLE_YES;
	}

	msg = g_strdup_printf(_("<b>Joining Qun %u is approved by admin %u for %s</b>"),
			ext_id, admin_uid, reason);
	now = time(NULL);
	qq_room_got_chat_in(gc, id, 0, msg, now);

	g_free(msg);
	g_free(reason);
}

/* process the packet when removed from a group */
void qq_process_room_buddy_removed(guint8 *data, gint len, guint32 id, PurpleConnection *gc)
{
	guint32 ext_id, uid;
	guint8 type8;
	gchar *msg;
	qq_room_data *rmd;
	gint bytes = 0;
	time_t now = time(NULL);

	g_return_if_fail(data != NULL && len > 0);

	/* FIXME: check length here */
	bytes = 0;
	bytes += qq_get32(&ext_id, data + bytes);
	bytes += qq_get8(&type8, data + bytes);
	bytes += qq_get32(&uid, data + bytes);

	g_return_if_fail(ext_id > 0 && uid > 0);

	qq_room_find_or_new(gc, id, ext_id);
	rmd = qq_room_data_find(gc, id);
	if (rmd != NULL) {
		rmd->my_role = QQ_ROOM_ROLE_NO;
	}

	msg = g_strdup_printf(_("<b>Removed buddy %u.</b>"), uid);
	qq_room_got_chat_in(gc, id, 0, msg, now);
	g_free(msg);
}

/* process the packet when added to a group */
void qq_process_room_buddy_joined(guint8 *data, gint len, guint32 id, PurpleConnection *gc)
{
	guint32 ext_id, uid;
	guint8 type8;
	qq_room_data *rmd;
	gint bytes;
	gchar *msg;
	time_t now = time(NULL);

	g_return_if_fail(data != NULL && len > 0);

	/* FIXME: check length here */
	bytes = 0;
	bytes += qq_get32(&ext_id, data + bytes);
	bytes += qq_get8(&type8, data + bytes);
	bytes += qq_get32(&uid, data + bytes);

	g_return_if_fail(ext_id > 0 && id > 0);

	qq_room_find_or_new(gc, id, ext_id);
	rmd = qq_room_data_find(gc, id);
	g_return_if_fail(rmd != NULL);

	rmd->my_role = QQ_ROOM_ROLE_YES;

	qq_update_room(gc, 0, rmd->id);

	msg = g_strdup_printf(_("<b>New buddy %u joined.</b>"), uid);
	qq_room_got_chat_in(gc, id, 0, msg, now);
	g_free(msg);
}

