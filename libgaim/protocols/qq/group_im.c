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

#include "conversation.h"
#include "debug.h"
#include "notify.h"
#include "prefs.h"
#include "request.h"
#include "util.h"

#include "char_conv.h"
#include "group_find.h"
#include "group_internal.h"
#include "group_info.h"
#include "group_im.h"
#include "group_network.h"
#include "group_opt.h"
#include "im.h"
#include "packet_parse.h"
#include "utils.h"

typedef struct _qq_recv_group_im {
	guint32 external_group_id;
	guint8 group_type;
	guint32 member_uid;
	guint16 msg_seq;
	time_t send_time;
	guint16 msg_len;
	gchar *msg;
	guint8 *font_attr;
	gint font_attr_len;
} qq_recv_group_im;

/* send IM to a group */
void qq_send_packet_group_im(GaimConnection *gc, qq_group *group, const gchar *msg)
{
	gint data_len, bytes;
	guint8 *raw_data, *cursor, *send_im_tail;
	guint16 msg_len;
	gchar *msg_filtered;

	g_return_if_fail(gc != NULL && group != NULL && msg != NULL);

	msg_filtered = gaim_markup_strip_html(msg);
	msg_len = strlen(msg_filtered);
	data_len = 7 + msg_len + QQ_SEND_IM_AFTER_MSG_LEN;
	raw_data = g_newa(guint8, data_len);
	cursor = raw_data;

	bytes = 0;
	bytes += create_packet_b(raw_data, &cursor, QQ_GROUP_CMD_SEND_MSG);
	bytes += create_packet_dw(raw_data, &cursor, group->internal_group_id);
	bytes += create_packet_w(raw_data, &cursor, msg_len + QQ_SEND_IM_AFTER_MSG_LEN);
	bytes += create_packet_data(raw_data, &cursor, (guint8 *) msg_filtered, msg_len);
	send_im_tail = qq_get_send_im_tail(NULL, NULL, NULL,
						   FALSE, FALSE, FALSE,
						   QQ_SEND_IM_AFTER_MSG_LEN);
	bytes += create_packet_data(raw_data, &cursor, send_im_tail, QQ_SEND_IM_AFTER_MSG_LEN);
	g_free(send_im_tail);
	g_free(msg_filtered);

	if (bytes == data_len)	/* create OK */
		qq_send_group_cmd(gc, group, raw_data, data_len);
	else
		gaim_debug(GAIM_DEBUG_ERROR, "QQ",
			   "Fail creating group_im packet, expect %d bytes, build %d bytes\n", data_len, bytes);
}

/* this is the ACK */
void qq_process_group_cmd_im(guint8 *data, guint8 **cursor, gint len, GaimConnection *gc) 
{
	/* return should be the internal group id
	 * but we have nothing to do with it */
	return;
}

/* receive an application to join the group */
void qq_process_recv_group_im_apply_join
    (guint8 *data, guint8 **cursor, gint len, guint32 internal_group_id, GaimConnection *gc)
{
	guint32 external_group_id, user_uid;
	guint8 group_type;
	gchar *reason_utf8, *msg, *reason;
	group_member_opt *g;

	g_return_if_fail(gc != NULL && internal_group_id > 0 && data != NULL && len > 0);

	if (*cursor >= (data + len - 1)) {
		gaim_debug(GAIM_DEBUG_WARNING, "QQ", "Received group msg apply_join is empty\n");
		return;
	}

	read_packet_dw(data, cursor, len, &external_group_id);
	read_packet_b(data, cursor, len, &group_type);
	read_packet_dw(data, cursor, len, &user_uid);

	g_return_if_fail(external_group_id > 0 && user_uid > 0);

	convert_as_pascal_string(*cursor, &reason_utf8, QQ_CHARSET_DEFAULT);

	msg = g_strdup_printf(_("User %d applied to join group %d"), user_uid, external_group_id);
	reason = g_strdup_printf(_("Reason: %s"), reason_utf8);

	g = g_new0(group_member_opt, 1);
	g->gc = gc;
	g->internal_group_id = internal_group_id;
	g->member = user_uid;

	gaim_request_action(gc, _("QQ Qun Operation"),
			    msg, reason,
			    2, g, 3,
			    _("Approve"),
			    G_CALLBACK
			    (qq_group_approve_application_with_struct),
			    _("Reject"),
			    G_CALLBACK
			    (qq_group_reject_application_with_struct),
			    _("Search"), G_CALLBACK(qq_group_search_application_with_struct));

	g_free(reason);
	g_free(msg);
	g_free(reason_utf8);
}

/* the request to join a group is rejected */
void qq_process_recv_group_im_been_rejected
    (guint8 *data, guint8 **cursor, gint len, guint32 internal_group_id, GaimConnection *gc)
{
	guint32 external_group_id, admin_uid;
	guint8 group_type;
	gchar *reason_utf8, *msg, *reason;
	qq_group *group;

	g_return_if_fail(gc != NULL && data != NULL && len > 0);

	if (*cursor >= (data + len - 1)) {
		gaim_debug(GAIM_DEBUG_WARNING, "QQ", "Received group msg been_rejected is empty\n");
		return;
	}

	read_packet_dw(data, cursor, len, &external_group_id);
	read_packet_b(data, cursor, len, &group_type);
	read_packet_dw(data, cursor, len, &admin_uid);

	g_return_if_fail(external_group_id > 0 && admin_uid > 0);

	convert_as_pascal_string(*cursor, &reason_utf8, QQ_CHARSET_DEFAULT);

	msg = g_strdup_printf
	    (_("You request to join group %d has been rejected by admin %d"), external_group_id, admin_uid);
	reason = g_strdup_printf(_("Reason: %s"), reason_utf8);

	gaim_notify_warning(gc, _("QQ Qun Operation"), msg, reason);

	group = qq_group_find_by_id(gc, internal_group_id, QQ_INTERNAL_ID);
	if (group != NULL) {
		group->my_status = QQ_GROUP_MEMBER_STATUS_NOT_MEMBER;
		qq_group_refresh(gc, group);
	}

	g_free(reason);
	g_free(msg);
	g_free(reason_utf8);
}

/* the request to join a group is approved */
void qq_process_recv_group_im_been_approved
    (guint8 *data, guint8 **cursor, gint len, guint32 internal_group_id, GaimConnection *gc)
{
	guint32 external_group_id, admin_uid;
	guint8 group_type;
	gchar *reason_utf8, *msg;
	qq_group *group;

	g_return_if_fail(gc != NULL && data != NULL && len > 0);

	if (*cursor >= (data + len - 1)) {
		gaim_debug(GAIM_DEBUG_WARNING, "QQ", "Received group msg been_approved is empty\n");
		return;
	}

	read_packet_dw(data, cursor, len, &external_group_id);
	read_packet_b(data, cursor, len, &group_type);
	read_packet_dw(data, cursor, len, &admin_uid);

	g_return_if_fail(external_group_id > 0 && admin_uid > 0);
	/* it is also a "无" here, so do not display */
	convert_as_pascal_string(*cursor, &reason_utf8, QQ_CHARSET_DEFAULT);

	msg = g_strdup_printf
	    (_("You request to join group %d has been approved by admin %d"), external_group_id, admin_uid);

	gaim_notify_warning(gc, _("QQ Qun Operation"), msg, NULL);

	group = qq_group_find_by_id(gc, internal_group_id, QQ_INTERNAL_ID);
	if (group != NULL) {
		group->my_status = QQ_GROUP_MEMBER_STATUS_IS_MEMBER;
		qq_group_refresh(gc, group);
	}

	g_free(msg);
	g_free(reason_utf8);
}

/* process the packet when removed from a group */
void qq_process_recv_group_im_been_removed
    (guint8 *data, guint8 **cursor, gint len, guint32 internal_group_id, GaimConnection *gc)
{
	guint32 external_group_id, uid;
	guint8 group_type;
	gchar *msg;
	qq_group *group;

	g_return_if_fail(gc != NULL && data != NULL && len > 0);

	if (*cursor >= (data + len - 1)) {
		gaim_debug(GAIM_DEBUG_WARNING, "QQ", "Received group msg been_removed is empty\n");
		return;
	}

	read_packet_dw(data, cursor, len, &external_group_id);
	read_packet_b(data, cursor, len, &group_type);
	read_packet_dw(data, cursor, len, &uid);

	g_return_if_fail(external_group_id > 0 && uid > 0);

	msg = g_strdup_printf(_("You [%d] has exit group \"%d\""), uid, external_group_id);
	gaim_notify_info(gc, _("QQ Qun Operation"), msg, NULL);

	group = qq_group_find_by_id(gc, internal_group_id, QQ_INTERNAL_ID);
	if (group != NULL) {
		group->my_status = QQ_GROUP_MEMBER_STATUS_NOT_MEMBER;
		qq_group_refresh(gc, group);
	}

	g_free(msg);
}

/* process the packet when added to a group */
void qq_process_recv_group_im_been_added
    (guint8 *data, guint8 **cursor, gint len, guint32 internal_group_id, GaimConnection *gc)
{
	guint32 external_group_id, uid;
	guint8 group_type;
	qq_group *group;
	gchar *msg;

	g_return_if_fail(gc != NULL && data != NULL && len > 0);

	if (*cursor >= (data + len - 1)) {
		gaim_debug(GAIM_DEBUG_WARNING, "QQ", "Received group msg been_added is empty\n");
		return;
	}

	read_packet_dw(data, cursor, len, &external_group_id);
	read_packet_b(data, cursor, len, &group_type);
	read_packet_dw(data, cursor, len, &uid);

	g_return_if_fail(external_group_id > 0 && uid > 0);

	msg = g_strdup_printf(_("You [%d] has been added by group \"%d\""), uid, external_group_id);
	gaim_notify_info(gc, _("QQ Qun Operation"), msg, _("This group has been added to your buddy list"));

	group = qq_group_find_by_id(gc, internal_group_id, QQ_INTERNAL_ID);
	if (group != NULL) {
		group->my_status = QQ_GROUP_MEMBER_STATUS_IS_MEMBER;
		qq_group_refresh(gc, group);
	} else {		/* no such group, try to create a dummy first, and then update */
		group = qq_group_create_internal_record(gc, internal_group_id, external_group_id, NULL);
		group->my_status = QQ_GROUP_MEMBER_STATUS_IS_MEMBER;
		qq_group_refresh(gc, group);
		qq_send_cmd_group_get_group_info(gc, group);
		/* the return of this cmd will automatically update the group in blist */
	}

	g_free(msg);
}

/* recv an IM from a group chat */
void qq_process_recv_group_im(guint8 *data, guint8 **cursor, gint data_len, 
		guint32 internal_group_id, GaimConnection *gc, guint16 im_type)
{
	gchar *msg_with_gaim_smiley, *msg_utf8_encoded, *im_src_name;
	guint16 unknown;
	guint32 unknown4;
	GaimConversation *conv;
	qq_data *qd;
	qq_buddy *member;
	qq_group *group;
	qq_recv_group_im *im_group;
	gint skip_len;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL && data != NULL && data_len > 0);
	qd = (qq_data *) gc->proto_data;

	gaim_debug(GAIM_DEBUG_INFO, "QQ",
			   "group im hex dump\n%s\n", hex_dump_to_str(*cursor, data_len - (*cursor - data)));

	if (*cursor >= (data + data_len - 1)) {
		gaim_debug(GAIM_DEBUG_WARNING, "QQ", "Received group im_group is empty\n");
		return;
	}

	im_group = g_newa(qq_recv_group_im, 1);

	read_packet_dw(data, cursor, data_len, &(im_group->external_group_id));
	read_packet_b(data, cursor, data_len, &(im_group->group_type));

	if(QQ_RECV_IM_TEMP_QUN_IM == im_type) {
		read_packet_dw(data, cursor, data_len, &(internal_group_id));
	}

	read_packet_dw(data, cursor, data_len, &(im_group->member_uid));
	read_packet_w(data, cursor, data_len, &unknown);	/* 0x0001? */
	read_packet_w(data, cursor, data_len, &(im_group->msg_seq));
	read_packet_dw(data, cursor, data_len, (guint32 *) & (im_group->send_time));
	read_packet_dw(data, cursor, data_len, &unknown4);	/* versionID */
	/*
	 * length includes font_attr
	 * this msg_len includes msg and font_attr
	 **** the format is ****
	 * length of all
	 * 1. unknown 10 bytes
	 * 2. 0-ended string
	 * 3. font_attr
	 */

	read_packet_w(data, cursor, data_len, &(im_group->msg_len));
	g_return_if_fail(im_group->msg_len > 0);

	/*
	 * 10 bytes from lumaqq
	 *    contentType = buf.getChar();
	 *    totalFragments = buf.get() & 255;
	 *    fragmentSequence = buf.get() & 255;
	 *    messageId = buf.getChar();
	 *    buf.getInt();
	 */

	if(im_type != QQ_RECV_IM_UNKNOWN_QUN_IM)
		skip_len = 10;
	else
		skip_len = 0;
	*cursor += skip_len;

	im_group->msg = g_strdup((gchar *) *cursor);
	*cursor += strlen(im_group->msg) + 1;
	/* there might not be any font_attr, check it */
	im_group->font_attr_len = im_group->msg_len - strlen(im_group->msg) - 1 - skip_len;
	if (im_group->font_attr_len > 0)
		im_group->font_attr = g_memdup(*cursor, im_group->font_attr_len);
	else
		im_group->font_attr = NULL;

	/* group im_group has no flag to indicate whether it has font_attr or not */
	msg_with_gaim_smiley = qq_smiley_to_gaim(im_group->msg);
	if (im_group->font_attr_len > 0)
		msg_utf8_encoded = qq_encode_to_gaim(im_group->font_attr,
						     im_group->font_attr_len, msg_with_gaim_smiley);
	else
		msg_utf8_encoded = qq_to_utf8(msg_with_gaim_smiley, QQ_CHARSET_DEFAULT);

	group = qq_group_find_by_id(gc, internal_group_id, QQ_INTERNAL_ID);
	g_return_if_fail(group != NULL);

	conv = gaim_find_conversation_with_account(GAIM_CONV_TYPE_CHAT, group->group_name_utf8, gaim_connection_get_account(gc));
	if (conv == NULL && gaim_prefs_get_bool("/plugins/prpl/qq/prompt_group_msg_on_recv")) {
		serv_got_joined_chat(gc, qd->channel++, group->group_name_utf8);
		conv = gaim_find_conversation_with_account(GAIM_CONV_TYPE_CHAT, group->group_name_utf8, gaim_connection_get_account(gc));
	}

	if (conv != NULL) {
		member = qq_group_find_member_by_uid(group, im_group->member_uid);
		if (member == NULL || member->nickname == NULL)
			im_src_name = uid_to_gaim_name(im_group->member_uid);
		else
			im_src_name = g_strdup(member->nickname);
		serv_got_chat_in(gc,
				 gaim_conv_chat_get_id(GAIM_CONV_CHAT
						       (conv)), im_src_name, 0, msg_utf8_encoded, im_group->send_time);
		g_free(im_src_name);
	}
	g_free(msg_with_gaim_smiley);
	g_free(msg_utf8_encoded);
	g_free(im_group->msg);
	g_free(im_group->font_attr);
}
