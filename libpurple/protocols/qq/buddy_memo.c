/**
 * @file buddy_memo.c
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

#include "buddy_memo.h"
#include "utils.h"
#include "packet_parse.h"
#include "buddy_list.h"
#include "buddy_info.h"
#include "char_conv.h"
#include "im.h"
#include "qq_define.h"
#include "qq_base.h"
#include "qq_network.h"
#include "qq.h"


#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <stdlib.h>
#include <stdio.h>

/* memo index */
enum {
	QQ_MEMO_ALIAS = 0,
	QQ_MEMO_MOBILD,
	QQ_MEMO_TELEPHONE,
	QQ_MEMO_ADDRESS,
	QQ_MEMO_EMAIL,
	QQ_MEMO_ZIPCODE,
	QQ_MEMO_NOTE,
	QQ_MEMO_SIZE
};

/* memo id */
static const gchar *memo_id[] = {
	"mm_alias",
	"mm_mobile",
	"mm_telephone",
	"mm_address",
	"mm_email",
	"mm_zipcode",
	"mm_note"
};

/* memo text */
static const gchar *memo_txt[] = {
	N_("Alias"),
	N_("Mobile"),
	N_("Telephone"),
	N_("Address"),
	N_("Email"),
	N_("Postal Code"),
	N_("Note")
};

typedef struct _modify_memo_request {
	PurpleConnection *gc;
	UID bd_uid;
	gchar **segments;
} modify_memo_request;


static void memo_debug(gchar **segments)
{
	gint index;
	g_return_if_fail(NULL != segments);
	for (index = 0;  index < QQ_MEMO_SIZE; index++) {
		purple_debug_info("QQ","memo[%i]=%s\n", index, segments[index]);
	}
}

static void memo_free(gchar **segments)
{
	gint index;
	g_return_if_fail(NULL != segments);
	for (index = 0; index < QQ_MEMO_SIZE; index++) {
		g_free(segments[index]);
	}
	purple_debug_info("QQ", "memo freed\n");
}

static void update_buddy_memo(PurpleConnection *gc, UID bd_uid, gchar *alias)
{
	PurpleAccount *account;
	PurpleBuddy *buddy;
	gchar *who;
	g_return_if_fail(NULL != gc && NULL != alias);

	account = (PurpleAccount *)gc->account;
	g_return_if_fail(NULL != account);

	who = uid_to_purple_name(bd_uid);
	buddy = purple_find_buddy(account, who);
	if (buddy == NULL || purple_buddy_get_protocol_data(buddy) == NULL) {
		g_free(who);
		purple_debug_info("QQ", "Error...Can NOT find %d!\n", bd_uid);
		return;
	}
	purple_blist_alias_buddy(buddy, (const char*)alias);
}

static void request_change_memo(PurpleConnection *gc, UID bd_uid, gchar **segments)
{
	gint bytes;
	/* Attention, length of each segment must be guint8(0~255),
	 * so length of memo string is limited.
	 * convert it to guint8 first before putting data */
	guint seg_len;
	gint index;
	guint8 raw_data[MAX_PACKET_SIZE - 16] = {0};

	purple_debug_info( "QQ", "request_change_memo\n" );
	g_return_if_fail(NULL != gc && NULL != segments);

	bytes = 0;
	bytes += qq_put8(raw_data+bytes, QQ_BUDDY_MEMO_MODIFY);
	bytes += qq_put8(raw_data+bytes, 0x00);
	bytes += qq_put32(raw_data+bytes, (guint32)bd_uid);
	bytes += qq_put8(raw_data+bytes, 0x00);
	for (index = 0; index < QQ_MEMO_SIZE; index++) {
		seg_len = strlen(segments[index]);
		seg_len = seg_len & 0xff;
		bytes += qq_put8(raw_data+bytes, (guint8)seg_len);
		bytes += qq_putdata(raw_data+bytes, (const guint8 *)segments[index], (guint8)seg_len);
	}

	/* debug */
	/*
	   qq_show_packet("MEMO MODIFY", raw_data, bytes);
	   */

	qq_send_cmd(gc, QQ_CMD_BUDDY_MEMO, raw_data, bytes);
}

static void memo_modify_cancle_cb(modify_memo_request *memo_request, PurpleRequestFields *fields)
{
	memo_free(memo_request->segments);
	g_free(memo_request);
}

/* prepare segments to be sent, string all convert to qq charset */
static void memo_modify_ok_cb(modify_memo_request *memo_request, PurpleRequestFields *fields)
{
	PurpleConnection *gc;
	UID bd_uid;
	gchar **segments;
	const gchar *utf8_str;
	gchar *value = NULL;
	gint index;

	g_return_if_fail(NULL != memo_request);
	gc = (PurpleConnection *)memo_request->gc;
	segments = (gchar **)memo_request->segments;
	g_return_if_fail(NULL != gc && NULL != segments);
	bd_uid = (guint32)memo_request->bd_uid;


	for (index = 0; index < QQ_MEMO_SIZE; index++) {
		utf8_str = purple_request_fields_get_string(fields, memo_id[index]);
		/* update alias */
		if (QQ_MEMO_ALIAS == index) {
			update_buddy_memo(gc, (UID)bd_uid, segments[QQ_MEMO_ALIAS]);
		}
		if (NULL == utf8_str) {
			value = g_strdup("");
		}
		else {
			value = utf8_to_qq(utf8_str, QQ_CHARSET_DEFAULT);
			/* Warnning: value will be string "(NULL)" instead of NULL */
			if (!qq_strcmp("(NULL)", value)) {
				value = g_strdup("");
			}
		}
		g_free(segments[index]);
		segments[index] = value;
	}

	memo_debug(segments);
	/* send segments */
	request_change_memo(gc, bd_uid, segments);

	/* free segments */
	memo_free(segments);
	g_free(memo_request);
}

/* memo modify dialogue */
static void memo_modify_dialogue(PurpleConnection *gc, UID bd_uid, gchar **segments, guint32 action)
{
	modify_memo_request *memo_request;
	PurpleRequestField *field;
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	int index;
	gchar *utf8_title;
	gchar *utf8_primary;

	g_return_if_fail(NULL != gc && NULL != segments);

	switch (action) {
		case QQ_BUDDY_MEMO_GET:
			memo_free(segments);
			break;
		case QQ_BUDDY_MEMO_MODIFY:
			/* keep one dialog once a time */
			purple_request_close_with_handle(gc);
			/* show dialog */
			fields = purple_request_fields_new();
			group = purple_request_field_group_new(NULL);
			purple_request_fields_add_group(fields, group);

			for(index = 0; index < QQ_MEMO_SIZE; index++) {
				/*
				   purple_debug_info("QQ", "id:%s txt:%s segment:%s\n",
				   memo_id[index], memo_txt[index], segments[index]);
				   */
				field = purple_request_field_string_new(memo_id[index], memo_txt[index],
						segments[index], FALSE);
				purple_request_field_group_add_field(group, field);
			}

			/* for upload cb */
			memo_request = g_new0(modify_memo_request, 1);
			memo_request->gc = gc;
			memo_request->bd_uid = bd_uid;
			memo_request->segments = segments;
			/* callback */
			utf8_title = g_strdup(_("Buddy Memo"));
			utf8_primary = g_strdup(_("Change his/her memo as you like"));

			purple_request_fields(gc, utf8_title, utf8_primary, NULL,
					fields,
					_("_Modify"), G_CALLBACK(memo_modify_ok_cb),
					_("_Cancel"), G_CALLBACK(memo_modify_cancle_cb),
					purple_connection_get_account(gc), NULL, NULL,
					memo_request);

			g_free(utf8_title);
			g_free(utf8_primary);
			break;
		default:
			purple_debug_info("QQ", "Error...unknown memo action, please tell us\n");
			break;
	}
}

static void qq_create_buddy_memo(PurpleConnection *gc, UID bd_uid, guint32 action)
{
	gchar **segments;
	gint index;
	g_return_if_fail(NULL != gc);

	segments = g_new0(gchar*, QQ_MEMO_SIZE);
	for (index = 0; index < QQ_MEMO_SIZE; index++) {
		segments[index] = g_strdup("");;
	}
	memo_modify_dialogue(gc, bd_uid, segments, action);
}

/* process reply to get_memo packet
 * here, update_class will be regarded as buddy's uid. because some 
 * memo packages returned without uid, which will make us confused */
void qq_process_get_buddy_memo(PurpleConnection *gc, guint8* data, gint data_len,
		UPDCLS update_class, guint32 action)
{
	gchar **segments;
	gint bytes;
	gint index;
	guint8 rcv_cmd;
	guint32 rcv_uid;
	guint8 unk1_8;
	guint8 is_success;

	g_return_if_fail(NULL != gc && NULL != data && 0 != data_len);
	/*
	   qq_show_packet("MEMO REACH", data, data_len);
	   */
	purple_debug_info("QQ", "action=0x%02X\n", action);

	bytes = 0;

	/* TX looks a bit clever than before... :) */
	bytes += qq_get8(&rcv_cmd, data+bytes);
	purple_debug_info("QQ", "rcv_cmd=0x%02X\n", rcv_cmd);

	/* it's possible that packet contains no buddy uid and no memo!!!
	 * go to next step according to previous action sent */
	if (1 == data_len) { /* only one byte */
		purple_debug_info("QQ", "memo packet contains no buddy uid and memo...\n");
		if (QQ_BUDDY_MEMO_MODIFY == action) {
			UID mod_uid;
			mod_uid = (UID)update_class;
			qq_create_buddy_memo(gc, mod_uid, QQ_BUDDY_MEMO_MODIFY);
			return;
		}
		return;
	}

	switch (rcv_cmd) {
		case QQ_BUDDY_MEMO_MODIFY:
		case QQ_BUDDY_MEMO_REMOVE:
			bytes += qq_get8(&is_success, data+bytes);
			if (QQ_BUDDY_MEMO_REQUEST_SUCCESS == is_success) {
				purple_notify_message(gc, PURPLE_NOTIFY_MSG_INFO,
						_("Memo Modify"), _("Server says:"),
						_("Your request was accepted."),
						NULL, NULL);
				purple_debug_info("QQ", "memo change succeessfully!\n");
			}
			else {
				purple_notify_message(gc, PURPLE_NOTIFY_MSG_INFO,
						_("Memo Modify"), _("Server says:"),
						_("Your request was rejected."),
						NULL, NULL);
				purple_debug_info("QQ", "memo change failed\n");
			}
			break;
		case QQ_BUDDY_MEMO_GET:
			bytes += qq_get32(&rcv_uid, data+bytes);
			purple_debug_info("QQ", "rcv_uid=%u\n", rcv_uid);
			bytes += qq_get8(&unk1_8, data+bytes);
			purple_debug_info("QQ", "unk1_8=0x%02X\n", unk1_8);
			segments = g_new0(gchar*, QQ_MEMO_SIZE);
			for (index = 0; index < QQ_MEMO_SIZE; index++) {
				/* get utf8 string */
				bytes += qq_get_vstr(&segments[index], QQ_CHARSET_DEFAULT, data+bytes);
				/*
				   purple_debug_info("QQ", "bytes:%d, seg:%s\n", bytes, segments[index]);
				   */
			}

			/* common action, update buddy memo */
			update_buddy_memo(gc, (UID)rcv_uid, segments[QQ_MEMO_ALIAS]);

			/* memo is a thing that we regard our buddy as, so we need one more buddy_uid */
			memo_modify_dialogue(gc, rcv_uid, segments, action);
			break;
		default:
			purple_debug_info("QQ", "received an UNKNOWN memo cmd!!!\n");
			break;
	}
}

/* request buddy memo
 *
 * param: gc, uid, update_class, action
 * here, update_class will be set to buddy's uid. because some memo 
 * packages returned without uid, which will make us confused */
void qq_request_buddy_memo(PurpleConnection *gc, UID bd_uid, UPDCLS update_class, guint32 action)
{
	guint8 raw_data[16] = {0};
	gint bytes;

	purple_debug_info("QQ", "qq_request_buddy_memo, buddy uid=%u, update_class=%u\n",
			bd_uid, update_class);
	g_return_if_fail(NULL != gc);
	/* '0' is ok
	   g_return_if_fail(uid != 0);
	   */
	bytes = 0;
	bytes += qq_put8(raw_data+bytes, QQ_BUDDY_MEMO_GET);
	bytes += qq_put32(raw_data+bytes, bd_uid);
	/*
	   qq_show_packet("MEMO REQUEST", raw_data, bytes);
	   */

	qq_send_cmd_mess(gc, QQ_CMD_BUDDY_MEMO, (guint8*)raw_data, bytes, update_class, action);
}


