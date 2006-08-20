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

#include "buddy_info.h"
#include "char_conv.h"
/*#include "group_admindlg.h"	*/
#include "group_find.h"
#include "group_hash.h"
#include "group_info.h"
#include "group_join.h"
#include "group_network.h"
#include "group_opt.h"
#include "packet_parse.h"
#include "qq.h"
#include "utils.h"

/* TODO: can't we use qsort here? */
/* This implement quick sort algorithm (low->high) */
static void _quick_sort(gint *numbers, gint left, gint right)
{
	gint pivot, l_hold, r_hold;

	l_hold = left;
	r_hold = right;
	pivot = numbers[left];
	while (left < right) {
		while ((numbers[right] >= pivot) && (left < right))
			right--;
		if (left != right) {
			numbers[left] = numbers[right];
			left++;
		}
		while ((numbers[left] <= pivot) && (left < right))
			left++;
		if (left != right) {
			numbers[right] = numbers[left];
			right--;
		}
	}
	numbers[left] = pivot;
	pivot = left;
	left = l_hold;
	right = r_hold;
	if (left < pivot)
		_quick_sort(numbers, left, pivot - 1);
	if (right > pivot)
		_quick_sort(numbers, pivot + 1, right);
}

static void _sort(guint32 *list)
{
	gint i;
	for (i = 0; list[i] < 0xffffffff; i++) {;
	}
	_quick_sort((gint *) list, 0, i - 1);
}

static void _qq_group_member_opt(GaimConnection *gc, qq_group *group, gint operation, guint32 *members)
{
	guint8 *data, *cursor;
	gint i, count, data_len;
	g_return_if_fail(gc != NULL && group != NULL && members != NULL);

	for (i = 0; members[i] != 0xffffffff; i++) {;
	}
	count = i;
	data_len = 6 + count * 4;
	data = g_newa(guint8, data_len);
	cursor = data;
	create_packet_b(data, &cursor, QQ_GROUP_CMD_MEMBER_OPT);
	create_packet_dw(data, &cursor, group->internal_group_id);
	create_packet_b(data, &cursor, operation);
	for (i = 0; i < count; i++)
		create_packet_dw(data, &cursor, members[i]);
	qq_send_group_cmd(gc, group, data, data_len);
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
	group = qq_group_find_by_internal_group_id(g->gc, g->internal_group_id);
	g_return_if_fail(group != NULL);
	qq_send_cmd_group_auth(g->gc, group, QQ_GROUP_AUTH_REQUEST_REJECT, g->member, msg_utf8);
	g_free(g);
}

void qq_group_search_application_with_struct(group_member_opt *g)
{
	g_return_if_fail(g != NULL && g->gc != NULL && g->member > 0);

	qq_send_packet_get_info(g->gc, g->member, TRUE);	/* we wanna see window */
	gaim_request_action
	    (g->gc, NULL, _("Do you wanna approve the request?"), "", 2, g,
	     2, _("Reject"),
	     G_CALLBACK(qq_group_reject_application_with_struct),
	     _("Approve"), G_CALLBACK(qq_group_approve_application_with_struct));
}

void qq_group_reject_application_with_struct(group_member_opt *g)
{
	gchar *msg1, *msg2;
	g_return_if_fail(g != NULL && g->gc != NULL && g->member > 0);

	msg1 = g_strdup_printf(_("You rejected %d's request"), g->member);
	msg2 = g_strdup(_("Input your reason:"));

	gaim_request_input(g->gc, NULL, msg1, msg2,
			   _("Sorry, you are not my type..."), TRUE, FALSE,
			   NULL, _("Send"),
			   G_CALLBACK(_qq_group_reject_application_real),
			   _("Cancel"), G_CALLBACK(_qq_group_do_nothing_with_struct), g);

	g_free(msg1);
	g_free(msg2);
}

void qq_group_approve_application_with_struct(group_member_opt *g)
{
	qq_group *group;
	g_return_if_fail(g != NULL && g->gc != NULL && g->internal_group_id > 0 && g->member > 0);
	group = qq_group_find_by_internal_group_id(g->gc, g->internal_group_id);
	g_return_if_fail(group != NULL);
	qq_send_cmd_group_auth(g->gc, group, QQ_GROUP_AUTH_REQUEST_APPROVE, g->member, "");
	qq_group_find_or_add_member(g->gc, group, g->member);
	g_free(g);
}

void qq_group_modify_members(GaimConnection *gc, qq_group *group, guint32 *new_members)
{
	guint32 *old_members, *del_members, *add_members;
	qq_buddy *q_bud;
	qq_data *qd;
	gint i = 0, old = 0, new = 0, del = 0, add = 0;
	GList *list;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL && group != NULL);
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

void qq_group_process_modify_members_reply(guint8 *data, guint8 **cursor, gint len, GaimConnection *gc)
{
	guint32 internal_group_id;
	qq_group *group;
	g_return_if_fail(data != NULL && gc != NULL);

	read_packet_dw(data, cursor, len, &internal_group_id);
	g_return_if_fail(internal_group_id > 0);

	/* we should have its info locally */
	group = qq_group_find_by_internal_group_id(gc, internal_group_id);
	g_return_if_fail(group != NULL);

	gaim_debug(GAIM_DEBUG_INFO, "QQ", "Succeed in modify members for Qun %d\n", group->external_group_id);

	gaim_notify_info(gc, _("QQ Qun Operation"), _("You have successfully modify Qun member"), NULL);
}

void qq_group_modify_info(GaimConnection *gc, qq_group *group)
{
	gint data_len, data_written;
	guint8 *data, *cursor;
	gchar *group_name, *group_desc, *notice;

	g_return_if_fail(gc != NULL && group != NULL);

	group_name = group->group_name_utf8 == NULL ? "" : utf8_to_qq(group->group_name_utf8, QQ_CHARSET_DEFAULT);
	group_desc = group->group_desc_utf8 == NULL ? "" : utf8_to_qq(group->group_desc_utf8, QQ_CHARSET_DEFAULT);
	notice = group->notice_utf8 == NULL ? "" : utf8_to_qq(group->notice_utf8, QQ_CHARSET_DEFAULT);

	data_len = 13 + 1 + strlen(group_name)
	    + 1 + strlen(group_desc)
	    + 1 + strlen(notice);

	data = g_newa(guint8, data_len);
	cursor = data;
	data_written = 0;
	/* 000-000 */
	data_written += create_packet_b(data, &cursor, QQ_GROUP_CMD_MODIFY_GROUP_INFO);
	/* 001-004 */
	data_written += create_packet_dw(data, &cursor, group->internal_group_id);
	/* 005-005 */
	data_written += create_packet_b(data, &cursor, 0x01);
	/* 006-006 */
	data_written += create_packet_b(data, &cursor, group->auth_type);
	/* 007-008 */
	data_written += create_packet_w(data, &cursor, 0x0000);
	/* 009-010 */
	data_written += create_packet_w(data, &cursor, group->group_category);

	data_written += create_packet_b(data, &cursor, strlen(group_name));
	data_written += create_packet_data(data, &cursor, (guint8 *) group_name, strlen(group_name));

	data_written += create_packet_w(data, &cursor, 0x0000);

	data_written += create_packet_b(data, &cursor, strlen(notice));
	data_written += create_packet_data(data, &cursor, (guint8 *) notice, strlen(notice));

	data_written += create_packet_b(data, &cursor, strlen(group_desc));
	data_written += create_packet_data(data, &cursor, (guint8 *) group_desc, strlen(group_desc));

	if (data_written != data_len)
		gaim_debug(GAIM_DEBUG_ERROR, "QQ",
			   "Fail to create group_modify_info packet, expect %d bytes, wrote %d bytes\n",
			   data_len, data_written);
	else
		qq_send_group_cmd(gc, group, data, data_len);
}

void qq_group_process_modify_info_reply(guint8 *data, guint8 **cursor, gint len, GaimConnection *gc)
{
	guint32 internal_group_id;
	qq_group *group;
	g_return_if_fail(data != NULL && gc != NULL);

	read_packet_dw(data, cursor, len, &internal_group_id);
	g_return_if_fail(internal_group_id > 0);

	/* we should have its info locally */
	group = qq_group_find_by_internal_group_id(gc, internal_group_id);
	g_return_if_fail(group != NULL);

	gaim_debug(GAIM_DEBUG_INFO, "QQ", "Succeed in modify info for Qun %d\n", group->external_group_id);
	qq_group_refresh(gc, group);

	gaim_notify_info(gc, _("QQ Qun Operation"), _("You have successfully modify Qun information"), NULL);
}

/* we create a very simple group first, and then let the user to modify */
void qq_group_create_with_name(GaimConnection *gc, const gchar *name)
{
	gint data_len, data_written;
	guint8 *data, *cursor;
	qq_data *qd;
	g_return_if_fail(gc != NULL && name != NULL);

	qd = (qq_data *) gc->proto_data;
	data_len = 7 + 1 + strlen(name) + 2 + 1 + 1 + 4;
	data = g_newa(guint8, data_len);
	cursor = data;

	data_written = 0;
	/* we create the simpleset group, only group name is given */
	/* 000 */
	data_written += create_packet_b(data, &cursor, QQ_GROUP_CMD_CREATE_GROUP);
	/* 001 */
	data_written += create_packet_b(data, &cursor, QQ_GROUP_TYPE_PERMANENT);
	/* 002 */
	data_written += create_packet_b(data, &cursor, QQ_GROUP_AUTH_TYPE_NEED_AUTH);
	/* 003-004 */
	data_written += create_packet_w(data, &cursor, 0x0000);
	/* 005-006 */
	data_written += create_packet_w(data, &cursor, 0x0003);
	/* 007 */
	data_written += create_packet_b(data, &cursor, strlen(name));
	data_written += create_packet_data(data, &cursor, (guint8 *) name, strlen(name));
	data_written += create_packet_w(data, &cursor, 0x0000);
	data_written += create_packet_b(data, &cursor, 0x00);	/* no group notice */
	data_written += create_packet_b(data, &cursor, 0x00);	/* no group desc */
	data_written += create_packet_dw(data, &cursor, qd->uid);	/* I am member of coz */

	if (data_written != data_len)
		gaim_debug(GAIM_DEBUG_ERROR, "QQ",
			   "Fail create create_group packet, expect %d bytes, written %d bytes\n",
			   data_len, data_written);
	else
		qq_send_group_cmd(gc, NULL, data, data_len);
}

static void qq_group_setup_with_gc_and_uid(gc_and_uid *g)
{
	qq_group *group;
	g_return_if_fail(g != NULL && g->gc != NULL && g->uid > 0);

	group = qq_group_find_by_internal_group_id(g->gc, g->uid);
	g_return_if_fail(group != NULL);

	/* XXX insert UI code here */
	/* qq_group_detail_window_show(g->gc, group); */
	g_free(g);
}

void qq_group_process_create_group_reply(guint8 *data, guint8 **cursor, gint len, GaimConnection *gc)
{
	guint32 internal_group_id, external_group_id;
	qq_group *group;
	gc_and_uid *g;
	qq_data *qd;

	g_return_if_fail(data != NULL && gc != NULL);
	g_return_if_fail(gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	read_packet_dw(data, cursor, len, &internal_group_id);
	read_packet_dw(data, cursor, len, &external_group_id);
	g_return_if_fail(internal_group_id > 0 && external_group_id);

	group = qq_group_create_by_id(gc, internal_group_id, external_group_id);
	group->my_status = QQ_GROUP_MEMBER_STATUS_IS_ADMIN;
	group->creator_uid = qd->uid;
	qq_group_refresh(gc, group);

	qq_group_activate_group(gc, internal_group_id);
	qq_send_cmd_group_get_group_info(gc, group);

	gaim_debug(GAIM_DEBUG_INFO, "QQ", "Succeed in create Qun, external ID %d\n", group->external_group_id);

	g = g_new0(gc_and_uid, 1);
	g->gc = gc;
	g->uid = internal_group_id;

	gaim_request_action(gc, _("QQ Qun Operation"),
			    _("You have successfully created a Qun"),
			    _
			    ("Would you like to set up the Qun details now?"),
			    1, g, 2, _("Setup"),
			    G_CALLBACK(qq_group_setup_with_gc_and_uid),
			    _("Cancel"), G_CALLBACK(qq_do_nothing_with_gc_and_uid));
}

/* we have to activate group after creation, otherwise the group can not be searched */
void qq_group_activate_group(GaimConnection *gc, guint32 internal_group_id)
{
	gint data_len, data_written;
	guint8 *data, *cursor;
	g_return_if_fail(gc != NULL && internal_group_id > 0);

	data_len = 5;
	data = g_newa(guint8, data_len);
	cursor = data;

	data_written = 0;
	/* we create the simplest group, only group name is given */
	/* 000 */
	data_written += create_packet_b(data, &cursor, QQ_GROUP_CMD_ACTIVATE_GROUP);
	/* 001-005 */
	data_written += create_packet_dw(data, &cursor, internal_group_id);

	if (data_written != data_len)
		gaim_debug(GAIM_DEBUG_ERROR, "QQ",
			   "Fail create activate_group packet, expect %d bytes, written %d bytes\n",
			   data_len, data_written);
	else
		qq_send_group_cmd(gc, NULL, data, data_len);
}

void qq_group_process_activate_group_reply(guint8 *data, guint8 **cursor, gint len, GaimConnection *gc)
{
	guint32 internal_group_id;
	qq_group *group;
	g_return_if_fail(data != NULL && gc != NULL);

	read_packet_dw(data, cursor, len, &internal_group_id);
	g_return_if_fail(internal_group_id > 0);

	/* we should have its info locally */
	group = qq_group_find_by_internal_group_id(gc, internal_group_id);
	g_return_if_fail(group != NULL);

	gaim_debug(GAIM_DEBUG_INFO, "QQ", "Succeed in activate Qun %d\n", group->external_group_id);
}

void qq_group_manage_group(GaimConnection *gc, GHashTable *data)
{
	gchar *internal_group_id_ptr;
	guint32 internal_group_id;
	qq_group *group;

	g_return_if_fail(gc != NULL && data != NULL);

	internal_group_id_ptr = g_hash_table_lookup(data, "internal_group_id");
	internal_group_id = strtol(internal_group_id_ptr, NULL, 10);
	g_return_if_fail(internal_group_id > 0);

	group = qq_group_find_by_internal_group_id(gc, internal_group_id);
	g_return_if_fail(group != NULL);

	/* XXX insert UI code here */
	/* qq_group_detail_window_show(gc, group); */
}
