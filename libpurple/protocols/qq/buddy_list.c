/**
 * @file buddy_list.c
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

#include <string.h>

#include "qq.h"

#include "debug.h"
#include "notify.h"
#include "utils.h"
#include "packet_parse.h"
#include "buddy_info.h"
#include "buddy_list.h"
#include "buddy_status.h"
#include "buddy_opt.h"
#include "char_conv.h"
#include "crypt.h"
#include "header_info.h"
#include "keep_alive.h"
#include "group.h"
#include "group_find.h"
#include "group_internal.h"
#include "group_info.h"

#include "qq_network.h"

#define QQ_GET_ONLINE_BUDDY_02          0x02
#define QQ_GET_ONLINE_BUDDY_03          0x03	/* unknown function */

#define QQ_ONLINE_BUDDY_ENTRY_LEN       38

typedef struct _qq_friends_online_entry {
	qq_buddy_status *s;
	guint16 unknown1;
	guint8 flag1;
	guint8 comm_flag;
	guint16 unknown2;
	guint8 ending;		/* 0x00 */
} qq_friends_online_entry;

/* get a list of online_buddies */
void qq_send_packet_get_buddies_online(PurpleConnection *gc, guint8 position)
{
	qq_data *qd;
	guint8 *raw_data;
	gint bytes = 0;

	qd = (qq_data *) gc->proto_data;
	raw_data = g_newa(guint8, 5);

	/* 000-000 get online friends cmd
	 * only 0x02 and 0x03 returns info from server, other valuse all return 0xff
	 * I can also only send the first byte (0x02, or 0x03)
	 * and the result is the same */
	bytes += qq_put8(raw_data + bytes, QQ_GET_ONLINE_BUDDY_02);
	/* 001-001 seems it supports 255 online buddies at most */
	bytes += qq_put8(raw_data + bytes, position);
	/* 002-002 */
	bytes += qq_put8(raw_data + bytes, 0x00);
	/* 003-004 */
	bytes += qq_put16(raw_data + bytes, 0x0000);

	qq_send_cmd(qd, QQ_CMD_GET_FRIENDS_ONLINE, raw_data, 5);
	qd->last_get_online = time(NULL);
}

/* position starts with 0x0000, 
 * server may return a position tag if list is too long for one packet */
void qq_send_packet_get_buddies_list(PurpleConnection *gc, guint16 position)
{
	qq_data *qd = (qq_data *) gc->proto_data;
	guint8 raw_data[16] = {0};
	gint bytes = 0;

	/* 000-001 starting position, can manually specify */
	bytes += qq_put16(raw_data + bytes, position);
	/* before Mar 18, 2004, any value can work, and we sent 00
	 * I do not know what data QQ server is expecting, as QQ2003iii 0304 itself
	 * even can sending packets 00 and get no response.
	 * Now I tested that 00,00,00,00,00,01 work perfectly
	 * March 22, found the 00,00,00 starts to work as well */
	bytes += qq_put8(raw_data + bytes, 0x00);

	qq_send_cmd(qd, QQ_CMD_GET_FRIENDS_LIST, raw_data, bytes);
}

/* get all list, buddies & Quns with groupsid support */
void qq_send_packet_get_all_list_with_group(PurpleConnection *gc, guint32 position)
{
	qq_data *qd = (qq_data *) gc->proto_data;
	guint8 raw_data[16] = {0};
	gint bytes = 0;

	/* 0x01 download, 0x02, upload */
	bytes += qq_put8(raw_data + bytes, 0x01);
	/* unknown 0x02 */
	bytes += qq_put8(raw_data + bytes, 0x02);
	/* unknown 00 00 00 00 */
	bytes += qq_put32(raw_data + bytes, 0x00000000);
	bytes += qq_put32(raw_data + bytes, position);

	qq_send_cmd(qd, QQ_CMD_GET_ALL_LIST_WITH_GROUP, raw_data, bytes);
}

static void _qq_buddies_online_reply_dump_unclear(qq_friends_online_entry *fe)
{
	GString *dump;

	g_return_if_fail(fe != NULL);

	qq_buddy_status_dump_unclear(fe->s);

	dump = g_string_new("");
	g_string_append_printf(dump, "unclear fields for [%d]:\n", fe->s->uid);
	g_string_append_printf(dump, "031-032: %04x (unknown)\n", fe->unknown1);
	g_string_append_printf(dump, "033:     %02x   (flag1)\n", fe->flag1);
	g_string_append_printf(dump, "034:     %02x   (comm_flag)\n", fe->comm_flag);
	g_string_append_printf(dump, "035-036: %04x (unknown)\n", fe->unknown2);

	purple_debug(PURPLE_DEBUG_INFO, "QQ", "Online buddy entry, %s", dump->str);
	g_string_free(dump, TRUE);
}

/* process the reply packet for get_buddies_online packet */
void qq_process_get_buddies_online_reply(guint8 *buf, gint buf_len, PurpleConnection *gc)
{
	qq_data *qd;
	gint len, bytes, bytes_buddy;
	gint count;
	guint8 *data, position;
	PurpleBuddy *b;
	qq_buddy *q_bud;
	qq_friends_online_entry *fe;

	g_return_if_fail(buf != NULL && buf_len != 0);

	qd = (qq_data *) gc->proto_data;
	len = buf_len;
	data = g_newa(guint8, len);

	purple_debug(PURPLE_DEBUG_INFO, "QQ", "processing get_buddies_online_reply\n");

	if (!qq_decrypt(buf, buf_len, qd->session_key, data, &len)) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", "Error decrypt buddies online");
		return;
	}

	qq_show_packet("Get buddies online reply packet", data, len);

	bytes = 0;
	bytes += qq_get8(&position, data + bytes);

	fe = g_newa(qq_friends_online_entry, 1);
	fe->s = g_newa(qq_buddy_status, 1);

	count = 0;
	while (bytes < len) {
		/* set flag */
		bytes_buddy = bytes;
		/* based on one online buddy entry */
		/* ATTTENTION! NEWED in the sub function, but FREED here */
		/* 000-030 qq_buddy_status */
		bytes += qq_buddy_status_read(fe->s, data + bytes);
		/* 031-032: unknown4 */
		bytes += qq_get16(&fe->unknown1, data + bytes);
		/* 033-033: flag1 */
		bytes += qq_get8(&fe->flag1, data + bytes);
		/* 034-034: comm_flag */
		bytes += qq_get8(&fe->comm_flag, data + bytes);
		/* 035-036: */
		bytes += qq_get16(&fe->unknown2, data + bytes);
		/* 037-037: */
		bytes += qq_get8(&fe->ending, data + bytes);	/* 0x00 */

		if (fe->s->uid == 0 || (bytes - bytes_buddy) != QQ_ONLINE_BUDDY_ENTRY_LEN) {
			purple_debug(PURPLE_DEBUG_ERROR, "QQ", 
					"uid=0 or entry complete len(%d) != %d", 
					(bytes - bytes_buddy), QQ_ONLINE_BUDDY_ENTRY_LEN);
			g_free(fe->s->ip);
			g_free(fe->s->unknown_key);
			continue;
		}	/* check if it is a valid entry */

		if (QQ_DEBUG) {
			_qq_buddies_online_reply_dump_unclear(fe);
		}

		/* update buddy information */
		b = purple_find_buddy(purple_connection_get_account(gc), uid_to_purple_name(fe->s->uid));
		q_bud = (b == NULL) ? NULL : (qq_buddy *) b->proto_data;

		if (q_bud != NULL) {	/* we find one and update qq_buddy */
			if(0 != fe->s->client_version)
				q_bud->client_version = fe->s->client_version;
			g_memmove(q_bud->ip, fe->s->ip, 4);
			q_bud->port = fe->s->port;
			q_bud->status = fe->s->status;
			q_bud->flag1 = fe->flag1;
			q_bud->comm_flag = fe->comm_flag;
			qq_update_buddy_contact(gc, q_bud);
			count++;
		} else {
			purple_debug(PURPLE_DEBUG_ERROR, "QQ", 
					"Got an online buddy %d, but not in my buddy list\n", fe->s->uid);
		}

		g_free(fe->s->ip);
		g_free(fe->s->unknown_key);
	}

	if(bytes > len) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", 
				"qq_process_get_buddies_online_reply: Dangerous error! maybe protocol changed, notify developers!\n");
	}

	if (position != QQ_FRIENDS_ONLINE_POSITION_END) {
		purple_debug(PURPLE_DEBUG_INFO, "QQ", "Received %d online buddies, nextposition=%u\n",
								count, (guint) position);
		if (position != QQ_FRIENDS_ONLINE_POSITION_START) {
			purple_debug(PURPLE_DEBUG_INFO, "QQ", "Requesting for more online buddies\n"); 
		}
		qq_send_packet_get_buddies_online(gc, position);
	} else {
		purple_debug(PURPLE_DEBUG_INFO, "QQ", "All online buddies received\n"); 
		qq_send_packet_get_buddies_levels(gc);
		qq_refresh_all_buddy_status(gc);
	}
}


/* process reply for get_buddies_list */
void qq_process_get_buddies_list_reply(guint8 *buf, gint buf_len, PurpleConnection *gc)
{
	qq_data *qd;
	qq_buddy *q_bud;
	gint len, bytes_expected, count;
	gint bytes, buddy_bytes;
	guint16 position, unknown;
	guint8 *data, pascal_len;
	gchar *name;
	PurpleBuddy *b;

	g_return_if_fail(buf != NULL && buf_len != 0);

	qd = (qq_data *) gc->proto_data;
	len = buf_len;
	data = g_newa(guint8, len);

	if (!qq_decrypt(buf, buf_len, qd->session_key, data, &len)) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", "Error decrypt buddies list");
		return;
	}
	bytes = 0;
	bytes += qq_get16(&position, data + bytes);
	/* the following data is buddy list in this packet */
	count = 0;
	while (bytes < len) {
		q_bud = g_new0(qq_buddy, 1);
		/* set flag */
		buddy_bytes = bytes;
		/* 000-003: uid */
		bytes += qq_get32(&q_bud->uid, data + bytes);
		/* 004-005: icon index (1-255) */
		bytes += qq_get16(&q_bud->face, data + bytes);
		/* 006-006: age */
		bytes += qq_get8(&q_bud->age, data + bytes);
		/* 007-007: gender */
		bytes += qq_get8(&q_bud->gender, data + bytes);

		pascal_len = convert_as_pascal_string(data + bytes, &q_bud->nickname, QQ_CHARSET_DEFAULT);
		bytes += pascal_len;

		bytes += qq_get16(&unknown, data + bytes);
		/* flag1: (0-7)
		 *        bit1 => qq show
		 * comm_flag: (0-7)
		 *        bit1 => member
		 *        bit4 => TCP mode
		 *        bit5 => open mobile QQ
		 *        bit6 => bind to mobile
		 *        bit7 => whether having a video
		 */
		bytes += qq_get8(&q_bud->flag1, data + bytes);
		bytes += qq_get8(&q_bud->comm_flag, data + bytes);

		bytes_expected = 12 + pascal_len;

		if (q_bud->uid == 0 || (bytes - buddy_bytes) != bytes_expected) {
			purple_debug(PURPLE_DEBUG_INFO, "QQ",
					"Buddy entry, expect %d bytes, read %d bytes\n", bytes_expected, bytes - buddy_bytes);
			g_free(q_bud->nickname);
			g_free(q_bud);
			continue;
		} else {
			count++;
		}

		if (QQ_DEBUG) {
			purple_debug(PURPLE_DEBUG_INFO, "QQ",
					"buddy [%09d]: flag1=0x%02x, comm_flag=0x%02x\n",
					q_bud->uid, q_bud->flag1, q_bud->comm_flag);
		}

		name = uid_to_purple_name(q_bud->uid);
		b = purple_find_buddy(gc->account, name);
		g_free(name);

		if (b == NULL) {
			b = qq_add_buddy_by_recv_packet(gc, q_bud->uid, TRUE, FALSE);
		}

		b->proto_data = q_bud;
		qd->buddies = g_list_append(qd->buddies, q_bud);
		qq_update_buddy_contact(gc, q_bud);
	}

	if(bytes > len) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", 
				"qq_process_get_buddies_list_reply: Dangerous error! maybe protocol changed, notify developers!");
	}

	purple_debug(PURPLE_DEBUG_INFO, "QQ", "Received %d buddies, nextposition=%u\n",
		count, (guint) position);
	if (position != QQ_FRIENDS_LIST_POSITION_START
		&& position != QQ_FRIENDS_LIST_POSITION_END) { 
		purple_debug(PURPLE_DEBUG_INFO, "QQ", "Requesting for more buddies\n"); 
		qq_send_packet_get_buddies_list(gc, position);
	} else {
		purple_debug(PURPLE_DEBUG_INFO, "QQ", "All buddies received. Requesting for online buddies list\n");
		qq_send_packet_get_buddies_online(gc, QQ_FRIENDS_LIST_POSITION_START); 
	}
}

void qq_process_get_all_list_with_group_reply(guint8 *buf, gint buf_len, PurpleConnection *gc)
{
	qq_data *qd;
	gint len, i, j;
	gint bytes = 0;
	guint8 *data;
	guint8 sub_cmd, reply_code;
	guint32 unknown, position;
	guint32 uid;
	guint8 type, groupid;
	qq_group *group;

	g_return_if_fail(buf != NULL && buf_len != 0);

	qd = (qq_data *) gc->proto_data;
	len = buf_len;
	data = g_newa(guint8, len);

	if (!qq_decrypt(buf, buf_len, qd->session_key, data, &len)) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", "Error decrypt all list with group");
		return;
	}

	bytes += qq_get8(&sub_cmd, data + bytes);
	g_return_if_fail(sub_cmd == 0x01);

	bytes += qq_get8(&reply_code, data + bytes);
	if(0 != reply_code) {
		purple_debug(PURPLE_DEBUG_WARNING, "QQ", 
				"Get all list with group reply, reply_code(%d) is not zero", reply_code);
	}

	bytes += qq_get32(&unknown, data + bytes);
	bytes += qq_get32(&position, data + bytes);
	/* the following data is all list in this packet */
	i = 0;
	j = 0;
	while (bytes < len) {
		/* 00-03: uid */
		bytes += qq_get32(&uid, data + bytes);
		/* 04: type 0x1:buddy 0x4:Qun */
		bytes += qq_get8(&type, data + bytes);
		/* 05: groupid*4 */ /* seems to always be 0 */
		bytes += qq_get8(&groupid, data + bytes);
		/*
		   purple_debug(PURPLE_DEBUG_INFO, "QQ", "groupid: %i\n", groupid);
		   groupid >>= 2;
		   */
		if (uid == 0 || (type != 0x1 && type != 0x4)) {
			purple_debug(PURPLE_DEBUG_INFO, "QQ",
					"Buddy entry, uid=%d, type=%d", uid, type);
			continue;
		} 
		if(0x1 == type) { /* a buddy */
			/* don't do anything but count - buddies are handled by 
			 * qq_send_packet_get_buddies_list */
			++i;
		} else { /* a group */
			group = qq_group_find_by_id(gc, uid, QQ_INTERNAL_ID);
			if(group == NULL) {
				qq_set_pending_id(&qd->adding_groups_from_server, uid, TRUE);
				group = g_newa(qq_group, 1);
				group->internal_group_id = uid;
				qq_send_cmd_group_get_group_info(gc, group);
			} else {
				group->my_status = QQ_GROUP_MEMBER_STATUS_IS_MEMBER;
				qq_group_refresh(gc, group);
				qq_send_cmd_group_get_group_info(gc, group);
			}
			++j;
		}
	}

	if(bytes > len) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", 
				"qq_process_get_all_list_with_group_reply: Dangerous error! maybe protocol changed, notify developers!");
	}

	purple_debug(PURPLE_DEBUG_INFO, "QQ", "Get all list done, %d buddies and %d Quns\n", i, j);
	purple_debug(PURPLE_DEBUG_INFO, "QQ", "Received %d buddies and %d groups, nextposition=%u\n", i, j, (guint) position);
	if (position != QQ_FRIENDS_ALL_LIST_POSITION_START && position != QQ_FRIENDS_ALL_LIST_POSITION_START) {
		purple_debug(PURPLE_DEBUG_INFO, "QQ", "Requesting for more buddies and groups\n");
		qq_send_packet_get_all_list_with_group(gc, position);
	} else {
		purple_debug(PURPLE_DEBUG_INFO, "QQ", "All buddies and groups received\n"); 
	}
}
