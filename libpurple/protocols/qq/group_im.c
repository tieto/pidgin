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
#include "group_opt.h"
#include "group_conv.h"
#include "im.h"
#include "qq_define.h"
#include "packet_parse.h"
#include "qq_network.h"
#include "qq_process.h"
#include "utils.h"

/* send IM to a group */
void qq_send_packet_group_im(PurpleConnection *gc, qq_group *group, const gchar *msg)
{
	gint data_len, bytes;
	guint8 *raw_data, *send_im_tail;
	guint16 msg_len;
	gchar *msg_filtered;

	g_return_if_fail(group != NULL && msg != NULL);

	msg_filtered = purple_markup_strip_html(msg);
	purple_debug_info("QQ_MESG", "Send qun mesg filterd: %s\n", msg_filtered);
	msg_len = strlen(msg_filtered);

	data_len = 2 + msg_len + QQ_SEND_IM_AFTER_MSG_LEN;
	raw_data = g_newa(guint8, data_len);

	bytes = 0;
	bytes += qq_put16(raw_data + bytes, msg_len + QQ_SEND_IM_AFTER_MSG_LEN);
	bytes += qq_putdata(raw_data + bytes, (guint8 *) msg_filtered, msg_len);
	send_im_tail = qq_get_send_im_tail(NULL, NULL, NULL,
			FALSE, FALSE, FALSE,
			QQ_SEND_IM_AFTER_MSG_LEN);
	bytes += qq_putdata(raw_data + bytes, send_im_tail, QQ_SEND_IM_AFTER_MSG_LEN);
	g_free(send_im_tail);
	g_free(msg_filtered);

	if (bytes == data_len)	/* create OK */
		qq_send_room_cmd(gc, QQ_ROOM_CMD_SEND_MSG, group->id, raw_data, data_len);
	else
		purple_debug_error("QQ",
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
void qq_process_room_msg_apply_join(guint8 *data, gint len, guint32 id, PurpleConnection *gc)
{
	guint32 ext_id, user_uid;
	guint8 type8;
	gchar *reason_utf8, *msg, *reason;
	group_member_opt *g;
	gchar *nombre;
	gint bytes = 0;

	g_return_if_fail(id > 0 && data != NULL && len > 0);

	/* FIXME: check length here */

	bytes += qq_get32(&ext_id, data + bytes);
	bytes += qq_get8(&type8, data + bytes);
	bytes += qq_get32(&user_uid, data + bytes);

	g_return_if_fail(ext_id > 0 && user_uid > 0);

	bytes += qq_get_vstr(&reason_utf8, QQ_CHARSET_DEFAULT, data + bytes);

	msg = g_strdup_printf(_("%d request to join Qun %d"), user_uid, ext_id);
	reason = g_strdup_printf(_("Message: %s"), reason_utf8);

	g = g_new0(group_member_opt, 1);
	g->gc = gc;
	g->id = id;
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

void qq_room_got_chat_in(PurpleConnection *gc,
		qq_group *group, guint32 uid_from, const gchar *msg, time_t in_time)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	PurpleConversation *conv;
	qq_buddy *buddy;
	gchar *from;

	g_return_if_fail(group != NULL);

	conv = purple_find_conversation_with_account(
			PURPLE_CONV_TYPE_CHAT, group->title_utf8, account);
	if (conv == NULL && purple_prefs_get_bool("/plugins/prpl/qq/show_room_when_newin")) {
		conv = qq_room_conv_create(gc, group);
	}

	if (conv == NULL) {
		return;
	}

	if (uid_from != 0) {
		buddy = qq_group_find_member_by_uid(group, uid_from);
		if (buddy == NULL || buddy->nickname == NULL)
			from = g_strdup_printf("%d", uid_from);
		else
			from = g_strdup(buddy->nickname);
	} else {
		from = g_strdup("");
	}
	serv_got_chat_in(gc,
			purple_conv_chat_get_id(PURPLE_CONV_CHAT(conv)),
				from, 0, msg, in_time);
	g_free(from);
}

/* the request to join a group is rejected */
void qq_process_room_msg_been_rejected(guint8 *data, gint len, guint32 id, PurpleConnection *gc)
{
	guint32 ext_id, admin_uid;
	guint8 type8;
	gchar *reason_utf8, *msg, *reason;
	qq_group *group;
	gint bytes;

	g_return_if_fail(data != NULL && len > 0);

	/* FIXME: check length here */
	bytes = 0;
	bytes += qq_get32(&ext_id, data + bytes);
	bytes += qq_get8(&type8, data + bytes);
	bytes += qq_get32(&admin_uid, data + bytes);

	g_return_if_fail(ext_id > 0 && admin_uid > 0);

	bytes += qq_get_vstr(&reason_utf8, QQ_CHARSET_DEFAULT, data + bytes);

	msg = g_strdup_printf
		(_("Failed to join Qun %d, operated by admin %d"), ext_id, admin_uid);
	reason = g_strdup_printf(_("Message: %s"), reason_utf8);

	purple_notify_warning(gc, _("QQ Qun Operation"), msg, reason);

	group = qq_room_search_id(gc, id);
	if (group != NULL) {
		group->my_role = QQ_ROOM_ROLE_NO;
		qq_group_refresh(gc, group);
	}

	g_free(msg);
	g_free(reason);
	g_free(reason_utf8);
}

/* the request to join a group is approved */
void qq_process_room_msg_been_approved(guint8 *data, gint len, guint32 id, PurpleConnection *gc)
{
	guint32 ext_id, admin_uid;
	guint8 type8;
	gchar *msg, *reason;
	qq_group *group;
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

	group = qq_room_search_id(gc, id);
	if (group != NULL) {
		group->my_role = QQ_ROOM_ROLE_YES;
		qq_group_refresh(gc, group);
	}

	msg = g_strdup_printf(_("<b>Joinning Qun %d is approved by Admin %d for %s</b>"),
			ext_id, admin_uid, reason);
	now = time(NULL);
	qq_room_got_chat_in(gc, group, 0, msg, now);

	g_free(msg);
	g_free(reason);
}

/* process the packet when removed from a group */
void qq_process_room_msg_been_removed(guint8 *data, gint len, guint32 id, PurpleConnection *gc)
{
	guint32 ext_id, uid;
	guint8 type8;
	gchar *msg;
	qq_group *group;
	gint bytes = 0;
	time_t now = time(NULL);

	g_return_if_fail(data != NULL && len > 0);

	/* FIXME: check length here */
	bytes = 0;
	bytes += qq_get32(&ext_id, data + bytes);
	bytes += qq_get8(&type8, data + bytes);
	bytes += qq_get32(&uid, data + bytes);

	g_return_if_fail(ext_id > 0 && uid > 0);

	group = qq_room_search_id(gc, id);
	if (group != NULL) {
		group->my_role = QQ_ROOM_ROLE_NO;
		qq_group_refresh(gc, group);
	}

	msg = g_strdup_printf(_("<b>Removed buddy %d.</b>"), uid);
	qq_room_got_chat_in(gc, group, 0, msg, now);
	g_free(msg);
}

/* process the packet when added to a group */
void qq_process_room_msg_been_added(guint8 *data, gint len, guint32 id, PurpleConnection *gc)
{
	guint32 ext_id, uid;
	guint8 type8;
	qq_group *group;
	gchar *msg;
	gint bytes;
	time_t now = time(NULL);

	g_return_if_fail(data != NULL && len > 0);

	/* FIXME: check length here */
	bytes = 0;
	bytes += qq_get32(&ext_id, data + bytes);
	bytes += qq_get8(&type8, data + bytes);
	bytes += qq_get32(&uid, data + bytes);

	g_return_if_fail(ext_id > 0 && uid > 0);

	group = qq_room_search_id(gc, id);
	if (group != NULL) {
		group->my_role = QQ_ROOM_ROLE_YES;
		qq_group_refresh(gc, group);
	} else {		/* no such group, try to create a dummy first, and then update */
		group = qq_group_create_internal_record(gc, id, ext_id, NULL);
		group->my_role = QQ_ROOM_ROLE_YES;
		qq_group_refresh(gc, group);
		qq_update_room(gc, 0, group->id);
		/* the return of this cmd will automatically update the group in blist */
	}

	msg = g_strdup_printf(_("<b>Added new buddy %d.</b>"), uid);
	qq_room_got_chat_in(gc, group, 0, msg, now);
	g_free(msg);
}

/* recv an IM from a group chat */
void qq_process_room_msg_normal(guint8 *data, gint data_len, guint32 id, PurpleConnection *gc, guint16 msg_type)
{
	gchar *msg_with_purple_smiley, *msg_utf8_encoded;
	qq_data *qd;
	qq_group *group;
	gint skip_len;
	gint bytes ;
	struct {
		guint32 ext_id;
		guint8 type8;
		guint32 member_uid;
		guint16 unknown;
		guint16 msg_seq;
		time_t send_time;
		guint32 unknown4;
		guint16 msg_len;
		gchar *msg;
		guint8 *font_attr;
		gint font_attr_len;
	} packet;


	g_return_if_fail(data != NULL && data_len > 0);

	/* FIXME: check length here */

	qd = (qq_data *) gc->proto_data;

#if 0
	qq_hex_dump(PURPLE_DEBUG_INFO, "QQ", data, data_len, "group im hex dump");
#endif
	memset(&packet, 0, sizeof(packet));
	bytes = 0;
	bytes += qq_get32(&(packet.ext_id), data + bytes);
	bytes += qq_get8(&(packet.type8), data + bytes);

	if(QQ_MSG_TEMP_QUN_IM == msg_type) {
		bytes += qq_get32(&(id), data + bytes);
	}

	bytes += qq_get32(&(packet.member_uid), bytes + data);
	bytes += qq_get16(&packet.unknown, data + bytes);	/* 0x0001? */
	bytes += qq_get16(&(packet.msg_seq), data + bytes);
	bytes += qq_getime(&packet.send_time, data + bytes);
	bytes += qq_get32(&packet.unknown4, data + bytes);	/* versionID */
	/*
	 * length includes font_attr
	 * this msg_len includes msg and font_attr
	 **** the format is ****
	 * length of all
	 * 1. unknown 10 bytes
	 * 2. 0-ended string
	 * 3. font_attr
	 */

	bytes += qq_get16(&(packet.msg_len), data + bytes);
	g_return_if_fail(packet.msg_len > 0);

	/*
	 * 10 bytes from lumaqq
	 *    contentType = buf.getChar();
	 *    totalFragments = buf.get() & 255;
	 *    fragmentSequence = buf.get() & 255;
	 *    messageId = buf.getChar();
	 *    buf.getInt();
	 */

	if(msg_type != QQ_MSG_UNKNOWN_QUN_IM)
		skip_len = 10;
	else
		skip_len = 0;
	bytes += skip_len;

	packet.msg = g_strdup((gchar *) data + bytes);
	bytes += strlen(packet.msg) + 1;
	/* there might not be any font_attr, check it */
	packet.font_attr_len = packet.msg_len - strlen(packet.msg) - 1 - skip_len;
	if (packet.font_attr_len > 0)
		packet.font_attr = g_memdup(data + bytes, packet.font_attr_len);
	else
		packet.font_attr = NULL;

	/* group im_group has no flag to indicate whether it has font_attr or not */
	msg_with_purple_smiley = qq_smiley_to_purple(packet.msg);
	if (packet.font_attr_len > 0) {
		msg_utf8_encoded = qq_encode_to_purple(packet.font_attr,
				packet.font_attr_len, msg_with_purple_smiley, qd->client_version);
	} else {
		msg_utf8_encoded = qq_to_utf8(msg_with_purple_smiley, QQ_CHARSET_DEFAULT);
	}
	group = qq_room_search_id(gc, id);
 	qq_room_got_chat_in(gc, group, packet.member_uid, msg_utf8_encoded, packet.send_time);

	g_free(msg_with_purple_smiley);
	g_free(msg_utf8_encoded);
	g_free(packet.msg);
	g_free(packet.font_attr);
}
