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
#include "group_internal.h"
#include "group_info.h"
#include "group_join.h"
#include "group_im.h"
#include "group_opt.h"
#include "im.h"
#include "qq_define.h"
#include "packet_parse.h"
#include "qq_network.h"
#include "qq_process.h"
#include "utils.h"

/* show group conversation window */
PurpleConversation *qq_room_conv_open(PurpleConnection *gc, qq_room_data *rmd)
{
	PurpleConversation *conv;
	qq_data *qd;
	gchar *topic_utf8;

	g_return_val_if_fail(rmd != NULL, NULL);
	qd = (qq_data *) gc->proto_data;

	conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,
			rmd->title_utf8, purple_connection_get_account(gc));
	if (conv != NULL)	{
		/* show only one conversation per room */
		return conv;
	}

	serv_got_joined_chat(gc, rmd->id, rmd->title_utf8);
	conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT, rmd->title_utf8, purple_connection_get_account(gc));
	if (conv != NULL) {
		topic_utf8 = g_strdup_printf("%d %s", rmd->ext_id, rmd->notice_utf8);
		purple_debug_info("QQ", "Set chat topic to %s\n", topic_utf8);
		purple_conv_chat_set_topic(PURPLE_CONV_CHAT(conv), NULL, topic_utf8);
		g_free(topic_utf8);

		if (rmd->is_got_buddies)
			qq_send_room_cmd_only(gc, QQ_ROOM_CMD_GET_ONLINES, rmd->id);
		else
			qq_update_room(gc, 0, rmd->id);
		return conv;
	}
	return NULL;
}

/* refresh online member in group conversation window */
void qq_room_conv_set_onlines(PurpleConnection *gc, qq_room_data *rmd)
{
	GList *names, *list, *flags;
	qq_buddy_data *bd;
	gchar *member_name, *member_uid;
	PurpleConversation *conv;
	gint flag;
	gboolean is_find;

	g_return_if_fail(rmd != NULL);

	conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,
			rmd->title_utf8, purple_connection_get_account(gc));
	if (conv == NULL) {
		purple_debug_warning("QQ", "Conversation \"%s\" is not opened\n", rmd->title_utf8);
		return;
	}
	g_return_if_fail(rmd->members != NULL);

	names = NULL;
	flags = NULL;

	list = rmd->members;
	while (list != NULL) {
		bd = (qq_buddy_data *) list->data;

		/* we need unique identifiers for everyone in the chat or else we'll
		 * run into problems with functions like get_cb_real_name from qq.c */
		member_name =   (bd->nickname != NULL && *(bd->nickname) != '\0') ?
				g_strdup_printf("%s (%u)", bd->nickname, bd->uid) :
				g_strdup_printf("(%u)", bd->uid);
		member_uid = g_strdup_printf("(%u)", bd->uid);

		flag = 0;
		/* TYPING to put online above OP and FOUNDER */
		if (is_online(bd->status)) flag |= (PURPLE_CBFLAGS_TYPING | PURPLE_CBFLAGS_VOICE);
		if(1 == (bd->role & 1)) flag |= PURPLE_CBFLAGS_OP;
		if(bd->uid == rmd->creator_uid) flag |= PURPLE_CBFLAGS_FOUNDER;

		is_find = TRUE;
		if (purple_conv_chat_find_user(PURPLE_CONV_CHAT(conv), member_name))
		{
			purple_conv_chat_user_set_flags(PURPLE_CONV_CHAT(conv),
					member_name,
					flag);
		} else if (purple_conv_chat_find_user(PURPLE_CONV_CHAT(conv), member_uid))
		{
			purple_conv_chat_user_set_flags(PURPLE_CONV_CHAT(conv),
					member_uid,
					flag);
			purple_conv_chat_rename_user(PURPLE_CONV_CHAT(conv), member_uid, member_name);
		} else {
			is_find = FALSE;
		}
		if (!is_find) {
			/* always put it even offline */
			names = g_list_append(names, member_name);
			flags = g_list_append(flags, GINT_TO_POINTER(flag));
		} else {
			g_free(member_name);
		}
		g_free(member_uid);
		list = list->next;
	}

	if (names != NULL && flags != NULL) {
		purple_conv_chat_add_users(PURPLE_CONV_CHAT(conv), names, NULL, flags, FALSE);
	}

	/* clean up names */
	while (names != NULL) {
		member_name = (gchar *) names->data;
		names = g_list_remove(names, member_name);
		g_free(member_name);
	}
	g_list_free(flags);
}

/* send IM to a group */
void qq_request_room_send_im(PurpleConnection *gc, guint32 room_id, const gchar *msg)
{
	gint data_len, bytes;
	guint8 *raw_data, *send_im_tail;
	guint16 msg_len;
	gchar *msg_filtered;

	g_return_if_fail(room_id != 0 && msg != NULL);

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
		qq_send_room_cmd(gc, QQ_ROOM_CMD_SEND_MSG, room_id, raw_data, data_len);
	else
		purple_debug_error("QQ",
				"Fail creating group_im packet, expect %d bytes, build %d bytes\n", data_len, bytes);
}

/* this is the ACK */
void qq_process_room_send_im(PurpleConnection *gc, guint8 *data, gint len)
{
	/* return should be the internal group id
	 * but we have nothing to do with it */
	return;
}

void qq_room_got_chat_in(PurpleConnection *gc,
		guint32 room_id, guint32 uid_from, const gchar *msg, time_t in_time)
{
	PurpleConversation *conv;
	qq_buddy_data *bd;
	qq_room_data *rmd;
	gchar *from;

	g_return_if_fail(gc != NULL && room_id != 0);

	conv = purple_find_chat(gc, room_id);
	rmd = qq_room_data_find(gc, room_id);
	g_return_if_fail(rmd != NULL);

	if (conv == NULL && purple_prefs_get_bool("/plugins/prpl/qq/show_room_when_newin")) {
		conv = qq_room_conv_open(gc, rmd);
	}

	if (conv == NULL) {
		return;
	}

	if (uid_from != 0) {

		bd = qq_room_buddy_find(rmd, uid_from);
		if (bd == NULL || bd->nickname == NULL)
			from = g_strdup_printf("%d", uid_from);
		else
			from = g_strdup(bd->nickname);
	} else {
		from = g_strdup("");
	}
	serv_got_chat_in(gc, room_id, from, 0, msg, in_time);
	g_free(from);
}

/* recv an IM from a group chat */
void qq_process_room_im(guint8 *data, gint data_len, guint32 id, PurpleConnection *gc, guint16 msg_type)
{
	gchar *msg_with_purple_smiley, *msg_utf8_encoded;
	qq_data *qd;
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

	/* qq_show_packet("ROOM_IM", data, data_len); */

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

	/* qq_show_packet("Message", data + bytes, data_len - bytes); */

	packet.msg = g_strdup((gchar *) data + bytes);
	bytes += strlen(packet.msg) + 1;
	/* there might not be any font_attr, check it */
	packet.font_attr_len = data_len - bytes;
	if (packet.font_attr_len > 0) {
		packet.font_attr = g_memdup(data + bytes, packet.font_attr_len);
		qq_show_packet("font_attr", packet.font_attr, packet.font_attr_len);
	} else {
		packet.font_attr = NULL;
	}

	/* group im_group has no flag to indicate whether it has font_attr or not */
	msg_with_purple_smiley = qq_smiley_to_purple(packet.msg);
	if (packet.font_attr_len > 0) {
		msg_utf8_encoded = qq_encode_to_purple(packet.font_attr,
				packet.font_attr_len, msg_with_purple_smiley, qd->client_version);
	} else {
		msg_utf8_encoded = qq_to_utf8(msg_with_purple_smiley, QQ_CHARSET_DEFAULT);
	}
 	qq_room_got_chat_in(gc, id, packet.member_uid, msg_utf8_encoded, packet.send_time);

	g_free(msg_with_purple_smiley);
	g_free(msg_utf8_encoded);
	g_free(packet.msg);
	g_free(packet.font_attr);
}
