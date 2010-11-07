/**
 * @file group_im.c
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
void qq_send_packet_group_im(PurpleConnection *gc, qq_group *group, const gchar *msg)
{
	gint data_len, bytes;
	guint8 *raw_data, *send_im_tail;
	guint16 msg_len;
	gchar *msg_filtered;

	g_return_if_fail(group != NULL && msg != NULL);

	msg_filtered = purple_markup_strip_html(msg);
	purple_debug_info("QQ_MESG", "filterd qq qun mesg: %s\n", msg_filtered);
	msg_len = strlen(msg_filtered);

	data_len = 7 + msg_len + QQ_SEND_IM_AFTER_MSG_LEN;
	raw_data = g_newa(guint8, data_len);

	bytes = 0;
	bytes += qq_put8(raw_data + bytes, QQ_GROUP_CMD_SEND_MSG);
	bytes += qq_put32(raw_data + bytes, group->internal_group_id);
	bytes += qq_put16(raw_data + bytes, msg_len + QQ_SEND_IM_AFTER_MSG_LEN);
	bytes += qq_putdata(raw_data + bytes, (guint8 *) msg_filtered, msg_len);
	send_im_tail = qq_get_send_im_tail(NULL, NULL, NULL,
			FALSE, FALSE, FALSE,
			QQ_SEND_IM_AFTER_MSG_LEN);
	bytes += qq_putdata(raw_data + bytes, send_im_tail, QQ_SEND_IM_AFTER_MSG_LEN);
	g_free(send_im_tail);
	g_free(msg_filtered);

	if (bytes == data_len)	/* create OK */
		qq_send_group_cmd(gc, group, raw_data, data_len);
	else
		purple_debug(PURPLE_DEBUG_ERROR, "QQ",
				"Fail creating group_im packet, expect %d bytes, build %d bytes\n", data_len, bytes);
}

/* this is the ACK */
void qq_process_group_cmd_im(guint8 *data, gint len, PurpleConnection *gc) 
{
	/* return should be the internal group id
	 * but we have nothing to do with it */
	return;
}

/* receive an application to join the group */
void qq_process_recv_group_im_apply_join(guint8 *data, gint len, guint32 internal_group_id, PurpleConnection *gc)
{
	guint32 external_group_id, user_uid;
	guint8 group_type;
	gchar *reason_utf8, *msg, *reason;
	group_member_opt *g;
	gchar *nombre;
	gint bytes = 0;

	g_return_if_fail(internal_group_id > 0 && data != NULL && len > 0);

	/* FIXME: check length here */

	bytes += qq_get32(&external_group_id, data + bytes);
	bytes += qq_get8(&group_type, data + bytes);
	bytes += qq_get32(&user_uid, data + bytes);

	g_return_if_fail(external_group_id > 0 && user_uid > 0);

	bytes += convert_as_pascal_string(data + bytes, &reason_utf8, QQ_CHARSET_DEFAULT);

	msg = g_strdup_printf(_("User %d requested to join group %d"), user_uid, external_group_id);
	reason = g_strdup_printf(_("Reason: %s"), reason_utf8);

	g = g_new0(group_member_opt, 1);
	g->gc = gc;
	g->internal_group_id = internal_group_id;
	g->member = user_uid;

	nombre = uid_to_purple_name(user_uid);

	purple_request_action(gc, _("QQ Qun Operation"),
			msg, reason,
			PURPLE_DEFAULT_ACTION_NONE,
			purple_connection_get_account(gc), nombre, NULL,
			g, 3,
			_("Approve"),
			G_CALLBACK
			(qq_group_approve_application_with_struct),
			_("Reject"),
			G_CALLBACK
			(qq_group_reject_application_with_struct),
			_("Search"), G_CALLBACK(qq_group_search_application_with_struct));

	g_free(nombre);
	g_free(reason);
	g_free(msg);
	g_free(reason_utf8);
}

/* the request to join a group is rejected */
void qq_process_recv_group_im_been_rejected(guint8 *data, gint len, guint32 internal_group_id, PurpleConnection *gc)
{
	guint32 external_group_id, admin_uid;
	guint8 group_type;
	gchar *reason_utf8, *msg, *reason;
	qq_group *group;
	gint bytes = 0;

	g_return_if_fail(data != NULL && len > 0);

	/* FIXME: check length here */

	bytes += qq_get32(&external_group_id, data + bytes);
	bytes += qq_get8(&group_type, data + bytes);
	bytes += qq_get32(&admin_uid, data + bytes);

	g_return_if_fail(external_group_id > 0 && admin_uid > 0);

	bytes += convert_as_pascal_string(data + bytes, &reason_utf8, QQ_CHARSET_DEFAULT);

	msg = g_strdup_printf
		(_("Your request to join group %d has been rejected by admin %d"), external_group_id, admin_uid);
	reason = g_strdup_printf(_("Reason: %s"), reason_utf8);

	purple_notify_warning(gc, _("QQ Qun Operation"), msg, reason);

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
void qq_process_recv_group_im_been_approved(guint8 *data, gint len, guint32 internal_group_id, PurpleConnection *gc)
{
	guint32 external_group_id, admin_uid;
	guint8 group_type;
	gchar *reason_utf8, *msg;
	qq_group *group;
	gint bytes = 0;

	g_return_if_fail(data != NULL && len > 0);

	/* FIXME: check length here */

	bytes += qq_get32(&external_group_id, data + bytes);
	bytes += qq_get8(&group_type, data + bytes);
	bytes += qq_get32(&admin_uid, data + bytes);

	g_return_if_fail(external_group_id > 0 && admin_uid > 0);
	/* it is also a "无" here, so do not display */
	bytes += convert_as_pascal_string(data + bytes, &reason_utf8, QQ_CHARSET_DEFAULT);

	msg = g_strdup_printf
		(_("Your request to join group %d has been approved by admin %d"), external_group_id, admin_uid);

	purple_notify_warning(gc, _("QQ Qun Operation"), msg, NULL);

	group = qq_group_find_by_id(gc, internal_group_id, QQ_INTERNAL_ID);
	if (group != NULL) {
		group->my_status = QQ_GROUP_MEMBER_STATUS_IS_MEMBER;
		qq_group_refresh(gc, group);
	}

	g_free(msg);
	g_free(reason_utf8);
}

/* process the packet when removed from a group */
void qq_process_recv_group_im_been_removed(guint8 *data, gint len, guint32 internal_group_id, PurpleConnection *gc)
{
	guint32 external_group_id, uid;
	guint8 group_type;
	gchar *msg;
	qq_group *group;
	gint bytes = 0;

	g_return_if_fail(data != NULL && len > 0);

	/* FIXME: check length here */

	bytes += qq_get32(&external_group_id, data + bytes);
	bytes += qq_get8(&group_type, data + bytes);
	bytes += qq_get32(&uid, data + bytes);

	g_return_if_fail(external_group_id > 0 && uid > 0);

	msg = g_strdup_printf(_("You [%d] have left group \"%d\""), uid, external_group_id);
	purple_notify_info(gc, _("QQ Qun Operation"), msg, NULL);

	group = qq_group_find_by_id(gc, internal_group_id, QQ_INTERNAL_ID);
	if (group != NULL) {
		group->my_status = QQ_GROUP_MEMBER_STATUS_NOT_MEMBER;
		qq_group_refresh(gc, group);
	}

	g_free(msg);
}

/* process the packet when added to a group */
void qq_process_recv_group_im_been_added(guint8 *data, gint len, guint32 internal_group_id, PurpleConnection *gc)
{
	guint32 external_group_id, uid;
	guint8 group_type;
	qq_group *group;
	gchar *msg;
	gint bytes = 0;

	g_return_if_fail(data != NULL && len > 0);

	/* FIXME: check length here */

	bytes += qq_get32(&external_group_id, data + bytes);
	bytes += qq_get8(&group_type, data + bytes);
	bytes += qq_get32(&uid, data + bytes);

	g_return_if_fail(external_group_id > 0 && uid > 0);

	msg = g_strdup_printf(_("You [%d] have been added to group \"%d\""), uid, external_group_id);
	purple_notify_info(gc, _("QQ Qun Operation"), msg, _("This group has been added to your buddy list"));

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
void qq_process_recv_group_im(guint8 *data, gint data_len, guint32 internal_group_id, PurpleConnection *gc, guint16 im_type)
{
	gchar *msg_with_purple_smiley, *msg_utf8_encoded, *im_src_name;
	guint16 unknown;
	guint32 unknown4;
	PurpleConversation *conv;
	qq_data *qd;
	qq_buddy *member;
	qq_group *group;
	qq_recv_group_im *im_group;
	gint skip_len;
	gint bytes = 0;

	g_return_if_fail(data != NULL && data_len > 0);

	/* FIXME: check length here */

	qd = (qq_data *) gc->proto_data;

	qq_hex_dump(PURPLE_DEBUG_INFO, "QQ",
		data, data_len,
		"group im hex dump");

	im_group = g_newa(qq_recv_group_im, 1);

	bytes += qq_get32(&(im_group->external_group_id), data + bytes);
	bytes += qq_get8(&(im_group->group_type), data + bytes);

	if(QQ_RECV_IM_TEMP_QUN_IM == im_type) {
		bytes += qq_get32(&(internal_group_id), data + bytes);
	}

	bytes += qq_get32(&(im_group->member_uid), bytes + data);
	bytes += qq_get16(&unknown, data + bytes);	/* 0x0001? */
	bytes += qq_get16(&(im_group->msg_seq), data + bytes);
	bytes += qq_getime(&im_group->send_time, data + bytes);
	bytes += qq_get32(&unknown4, data + bytes);	/* versionID */
	/*
	 * length includes font_attr
	 * this msg_len includes msg and font_attr
	 **** the format is ****
	 * length of all
	 * 1. unknown 10 bytes
	 * 2. 0-ended string
	 * 3. font_attr
	 */

	bytes += qq_get16(&(im_group->msg_len), data + bytes);
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
	bytes += skip_len;

	im_group->msg = g_strdup((gchar *) data + bytes);
	bytes += strlen(im_group->msg) + 1;
	/* there might not be any font_attr, check it */
	im_group->font_attr_len = im_group->msg_len - strlen(im_group->msg) - 1 - skip_len;
	if (im_group->font_attr_len > 0)
		im_group->font_attr = g_memdup(data + bytes, im_group->font_attr_len);
	else
		im_group->font_attr = NULL;

	/* group im_group has no flag to indicate whether it has font_attr or not */
	msg_with_purple_smiley = qq_smiley_to_purple(im_group->msg);
	if (im_group->font_attr_len > 0)
		msg_utf8_encoded = qq_encode_to_purple(im_group->font_attr,
				im_group->font_attr_len, msg_with_purple_smiley);
	else
		msg_utf8_encoded = qq_to_utf8(msg_with_purple_smiley, QQ_CHARSET_DEFAULT);

	group = qq_group_find_by_id(gc, internal_group_id, QQ_INTERNAL_ID);
	g_return_if_fail(group != NULL);

	conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT, group->group_name_utf8, purple_connection_get_account(gc));
	if (conv == NULL && purple_prefs_get_bool("/plugins/prpl/qq/prompt_group_msg_on_recv")) {
		serv_got_joined_chat(gc, qd->channel++, group->group_name_utf8);
		conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT, group->group_name_utf8, purple_connection_get_account(gc));
	}

	if (conv != NULL) {
		member = qq_group_find_member_by_uid(group, im_group->member_uid);
		if (member == NULL || member->nickname == NULL)
			im_src_name = uid_to_purple_name(im_group->member_uid);
		else
			im_src_name = g_strdup(member->nickname);
		serv_got_chat_in(gc,
				purple_conv_chat_get_id(PURPLE_CONV_CHAT
					(conv)), im_src_name, 0, msg_utf8_encoded, im_group->send_time);
		g_free(im_src_name);
	}
	g_free(msg_with_purple_smiley);
	g_free(msg_utf8_encoded);
	g_free(im_group->msg);
	g_free(im_group->font_attr);
}
