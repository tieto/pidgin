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
#include "group_find.h"
#include "group_internal.h"
#include "group_info.h"
#include "group_join.h"
#include "group_network.h"
#include "group_opt.h"
#include "packet_parse.h"
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

static void _qq_group_member_opt(PurpleConnection *gc, qq_group *group, gint operation, guint32 *members)
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
	bytes += qq_put8(data + bytes, QQ_GROUP_CMD_MEMBER_OPT);
	bytes += qq_put32(data + bytes, group->internal_group_id);
	bytes += qq_put8(data + bytes, operation);
	for (i = 0; i < count; i++)
		bytes += qq_put32(data + bytes, members[i]);

	qq_send_group_cmd(gc, group, data, bytes);
}

static void _qq_group_do_nothing_with_struct(group_member_opt *g)
{
	if (g != NULL)
		g_free(g);
}

static void _qq_group_reject_application_real(group_member_opt *g, gchar *msg_utf8)
{
	qq_group *group;
	g_return_if_fail(g != NULL && g->gc != NULL && g->internal_group_id > 0 && g->member > 0);
	group = qq_group_find_by_id(g->gc, g->internal_group_id, QQ_INTERNAL_ID);
	g_return_if_fail(group != NULL);
	qq_send_cmd_group_auth(g->gc, group, QQ_GROUP_AUTH_REQUEST_REJECT, g->member, msg_utf8);
	g_free(g);
}

void qq_group_search_application_with_struct(group_member_opt *g)
{
	g_return_if_fail(g != NULL && g->gc != NULL && g->member > 0);

	qq_send_packet_get_info(g->gc, g->member, TRUE);	/* we want to see window */
	purple_request_action(g->gc, NULL, _("Do you want to approve the request?"), "",
				PURPLE_DEFAULT_ACTION_NONE,
				purple_connection_get_account(g->gc), NULL, NULL,
				g, 2,
				_("Reject"), G_CALLBACK(qq_group_reject_application_with_struct),
				_("Approve"), G_CALLBACK(qq_group_approve_application_with_struct));
}

void qq_group_reject_application_with_struct(group_member_opt *g)
{
	gchar *msg1, *msg2, *nombre;
	g_return_if_fail(g != NULL && g->gc != NULL && g->member > 0);

	msg1 = g_strdup_printf(_("You rejected %d's request"), g->member);
	msg2 = g_strdup(_("Enter your reason:"));

	nombre = uid_to_purple_name(g->member);
	purple_request_input(g->gc, /* title */ NULL, msg1, msg2,
			   _("Sorry, you are not my type..."), /* multiline */ TRUE, /* masked */ FALSE,
			   /* hint */ NULL,
			   _("Send"), G_CALLBACK(_qq_group_reject_application_real),
			   _("Cancel"), G_CALLBACK(_qq_group_do_nothing_with_struct),
			   purple_connection_get_account(g->gc), nombre, NULL,
			   g);

	g_free(msg1);
	g_free(msg2);
	g_free(nombre);
}

void qq_group_approve_application_with_struct(group_member_opt *g)
{
	qq_group *group;
	g_return_if_fail(g != NULL && g->gc != NULL && g->internal_group_id > 0 && g->member > 0);
	group = qq_group_find_by_id(g->gc, g->internal_group_id, QQ_INTERNAL_ID);
	g_return_if_fail(group != NULL);
	qq_send_cmd_group_auth(g->gc, group, QQ_GROUP_AUTH_REQUEST_APPROVE, g->member, "");
	qq_group_find_or_add_member(g->gc, group, g->member);
	g_free(g);
}

void qq_group_modify_members(PurpleConnection *gc, qq_group *group, guint32 *new_members)
{
	guint32 *old_members, *del_members, *add_members;
	qq_buddy *q_bud;
	qq_data *qd;
	gint i = 0, old = 0, new = 0, del = 0, add = 0;
	GList *list;

	g_return_if_fail(group != NULL);
	qd = (qq_data *) gc->proto_data;
	if (new_members[0] == 0xffffffff)
		return;

	old_members = g_newa(guint32, QQ_QUN_MEMBER_MAX);
	del_members = g_newa(guint32, QQ_QUN_MEMBER_MAX);
	add_members = g_newa(guint32, QQ_QUN_MEMBER_MAX);

	/* construct the old member list */
	list = group->members;
	while (list != NULL) {
		q_bud = (qq_buddy *) list->data;
		if (q_bud != NULL)
			old_members[i++] = q_bud->uid;
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
		qq_group_remove_member_by_uid(group, del_members[i]);
	for (i = 0; i < add; i++)
		qq_group_find_or_add_member(gc, group, add_members[i]);

	if (del > 0)
		_qq_group_member_opt(gc, group, QQ_GROUP_MEMBER_DEL, del_members);
	if (add > 0)
		_qq_group_member_opt(gc, group, QQ_GROUP_MEMBER_ADD, add_members);
}

void qq_group_process_modify_members_reply(guint8 *data, gint len, PurpleConnection *gc)
{
	gint bytes;
	guint32 internal_group_id;
	qq_group *group;
	g_return_if_fail(data != NULL);

	bytes = 0;
	bytes += qq_get32(&internal_group_id, data + bytes);
	g_return_if_fail(internal_group_id > 0);

	/* we should have its info locally */
	group = qq_group_find_by_id(gc, internal_group_id, QQ_INTERNAL_ID);
	g_return_if_fail(group != NULL);

	purple_debug(PURPLE_DEBUG_INFO, "QQ", "Succeed in modify members for Qun %d\n", group->external_group_id);

	purple_notify_info(gc, _("QQ Qun Operation"), _("You have successfully modified Qun member"), NULL);
}

void qq_group_modify_info(PurpleConnection *gc, qq_group *group)
{
	guint8 *data;
	gint data_len;
	gint bytes;
	gchar *group_name, *group_desc, *notice;

	g_return_if_fail(group != NULL);

	group_name = group->group_name_utf8 == NULL ? "" : utf8_to_qq(group->group_name_utf8, QQ_CHARSET_DEFAULT);
	group_desc = group->group_desc_utf8 == NULL ? "" : utf8_to_qq(group->group_desc_utf8, QQ_CHARSET_DEFAULT);
	notice = group->notice_utf8 == NULL ? "" : utf8_to_qq(group->notice_utf8, QQ_CHARSET_DEFAULT);

	data_len = 13 + 1 + strlen(group_name)
	    + 1 + strlen(group_desc)
	    + 1 + strlen(notice);

	data = g_newa(guint8, data_len);
	bytes = 0;
	/* 000-000 */
	bytes += qq_put8(data + bytes, QQ_GROUP_CMD_MODIFY_GROUP_INFO);
	/* 001-004 */
	bytes += qq_put32(data + bytes, group->internal_group_id);
	/* 005-005 */
	bytes += qq_put8(data + bytes, 0x01);
	/* 006-006 */
	bytes += qq_put8(data + bytes, group->auth_type);
	/* 007-008 */
	bytes += qq_put16(data + bytes, 0x0000);
	/* 009-010 */
	bytes += qq_put16(data + bytes, group->group_category);

	bytes += qq_put8(data + bytes, strlen(group_name));
	bytes += qq_putdata(data + bytes, (guint8 *) group_name, strlen(group_name));

	bytes += qq_put16(data + bytes, 0x0000);

	bytes += qq_put8(data + bytes, strlen(notice));
	bytes += qq_putdata(data+ bytes, (guint8 *) notice, strlen(notice));

	bytes += qq_put8(data + bytes, strlen(group_desc));
	bytes += qq_putdata(data + bytes, (guint8 *) group_desc, strlen(group_desc));

	if (bytes != data_len)	{
		purple_debug(PURPLE_DEBUG_ERROR, "QQ",
			   "Fail to create group_modify_info packet, expect %d bytes, wrote %d bytes\n",
			   data_len, bytes);
		return;
	}

	qq_send_group_cmd(gc, group, data, bytes);
}

void qq_group_process_modify_info_reply(guint8 *data, gint len, PurpleConnection *gc)
{
	gint bytes;
	guint32 internal_group_id;
	qq_group *group;
	g_return_if_fail(data != NULL);

	bytes = 0;
	bytes += qq_get32(&internal_group_id, data + bytes);
	g_return_if_fail(internal_group_id > 0);

	/* we should have its info locally */
	group = qq_group_find_by_id(gc, internal_group_id, QQ_INTERNAL_ID);
	g_return_if_fail(group != NULL);

	purple_debug(PURPLE_DEBUG_INFO, "QQ", "Succeed in modify info for Qun %d\n", group->external_group_id);
	qq_group_refresh(gc, group);

	purple_notify_info(gc, _("QQ Qun Operation"), _("You have successfully modified Qun information"), NULL);
}

/* we create a very simple group first, and then let the user to modify */
void qq_group_create_with_name(PurpleConnection *gc, const gchar *name)
{
	gint data_len;
	guint8 *data;
	gint bytes;
	qq_data *qd;
	g_return_if_fail(name != NULL);

	qd = (qq_data *) gc->proto_data;
	data_len = 7 + 1 + strlen(name) + 2 + 1 + 1 + 4;
	data = g_newa(guint8, data_len);

	bytes = 0;
	/* we create the simpleset group, only group name is given */
	/* 000 */
	bytes += qq_put8(data + bytes, QQ_GROUP_CMD_CREATE_GROUP);
	/* 001 */
	bytes += qq_put8(data + bytes, QQ_GROUP_TYPE_PERMANENT);
	/* 002 */
	bytes += qq_put8(data + bytes, QQ_GROUP_AUTH_TYPE_NEED_AUTH);
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

	if (bytes != data_len) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ",
			   "Fail create create_group packet, expect %d bytes, written %d bytes\n",
			   data_len, bytes);
		return;
	}

	qq_send_group_cmd(gc, NULL, data, bytes);
}

static void qq_group_setup_with_gc_and_uid(gc_and_uid *g)
{
	qq_group *group;
	g_return_if_fail(g != NULL && g->gc != NULL && g->uid > 0);

	group = qq_group_find_by_id(g->gc, g->uid, QQ_INTERNAL_ID);
	g_return_if_fail(group != NULL);

	/* TODO insert UI code here */
	/* qq_group_detail_window_show(g->gc, group); */
	g_free(g);
}

void qq_group_process_create_group_reply(guint8 *data, gint len, PurpleConnection *gc)
{
	gint bytes;
	guint32 internal_group_id, external_group_id;
	qq_group *group;
	gc_and_uid *g;
	qq_data *qd;

	g_return_if_fail(data != NULL);
	g_return_if_fail(gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	bytes = 0;
	bytes += qq_get32(&internal_group_id, data + bytes);
	bytes += qq_get32(&external_group_id, data + bytes);
	g_return_if_fail(internal_group_id > 0 && external_group_id);

	group = qq_group_create_internal_record(gc, internal_group_id, external_group_id, NULL);
	group->my_status = QQ_GROUP_MEMBER_STATUS_IS_ADMIN;
	group->creator_uid = qd->uid;
	qq_group_refresh(gc, group);

	qq_group_activate_group(gc, internal_group_id);
	qq_send_cmd_group_get_group_info(gc, group);

	purple_debug(PURPLE_DEBUG_INFO, "QQ", "Succeed in create Qun, external ID %d\n", group->external_group_id);

	g = g_new0(gc_and_uid, 1);
	g->gc = gc;
	g->uid = internal_group_id;

	purple_request_action(gc, _("QQ Qun Operation"),
			    _("You have successfully created a Qun"),
			    _
			    ("Would you like to set up the Qun details now?"),
			    1,
				purple_connection_get_account(gc), NULL, NULL,
				g, 2,
				_("Setup"), G_CALLBACK(qq_group_setup_with_gc_and_uid),
			    _("Cancel"), G_CALLBACK(qq_do_nothing_with_gc_and_uid));
}

/* we have to activate group after creation, otherwise the group can not be searched */
void qq_group_activate_group(PurpleConnection *gc, guint32 internal_group_id)
{
	guint8 data[16] = {0};
	gint bytes = 0;
	g_return_if_fail(internal_group_id > 0);

	bytes = 0;
	/* we create the simplest group, only group name is given */
	/* 000 */
	bytes += qq_put8(data + bytes, QQ_GROUP_CMD_ACTIVATE_GROUP);
	/* 001-005 */
	bytes += qq_put32(data + bytes, internal_group_id);

	qq_send_group_cmd(gc, NULL, data, bytes);
}

void qq_group_process_activate_group_reply(guint8 *data, gint len, PurpleConnection *gc)
{
	gint bytes;
	guint32 internal_group_id;
	qq_group *group;
	g_return_if_fail(data != NULL);

	bytes = 0;
	bytes += qq_get32(&internal_group_id, data + bytes);
	g_return_if_fail(internal_group_id > 0);

	/* we should have its info locally */
	group = qq_group_find_by_id(gc, internal_group_id, QQ_INTERNAL_ID);
	g_return_if_fail(group != NULL);

	purple_debug(PURPLE_DEBUG_INFO, "QQ", "Succeed in activate Qun %d\n", group->external_group_id);
}

void qq_group_manage_group(PurpleConnection *gc, GHashTable *data)
{
	gchar *internal_group_id_ptr;
	guint32 internal_group_id;
	qq_group *group;

	g_return_if_fail(data != NULL);

	internal_group_id_ptr = g_hash_table_lookup(data, "internal_group_id");
	internal_group_id = strtol(internal_group_id_ptr, NULL, 10);
	g_return_if_fail(internal_group_id > 0);

	group = qq_group_find_by_id(gc, internal_group_id, QQ_INTERNAL_ID);
	g_return_if_fail(group != NULL);

	/* XXX insert UI code here */
	/* qq_group_detail_window_show(gc, group); */
}
