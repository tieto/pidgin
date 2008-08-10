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
#include "buddy_opt.h"
#include "char_conv.h"
#include "header_info.h"
#include "qq_base.h"
#include "group.h"
#include "group_find.h"
#include "group_internal.h"
#include "group_info.h"

#include "qq_network.h"

#define QQ_GET_ONLINE_BUDDY_02          0x02
#define QQ_GET_ONLINE_BUDDY_03          0x03	/* unknown function */

typedef struct _qq_buddy_online {
	qq_buddy_status bs;
	guint16 unknown1;
	guint8 ext_flag;
	guint8 comm_flag;
	guint16 unknown2;
	guint8 ending;		/* 0x00 */
} qq_buddy_online;

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

	qq_send_cmd(qd, QQ_CMD_GET_BUDDIES_ONLINE, raw_data, 5);
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

	qq_send_cmd(qd, QQ_CMD_GET_BUDDIES_LIST, raw_data, bytes);
}

/* get all list, buddies & Quns with groupsid support */
void qq_send_packet_get_buddies_and_rooms(PurpleConnection *gc, guint32 position)
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

	qq_send_cmd(qd, QQ_CMD_GET_BUDDIES_AND_ROOMS, raw_data, bytes);
}

/* parse the data into qq_buddy_status */
static gint get_buddy_status(qq_buddy_status *bs, guint8 *data)
{
	gint bytes = 0;

	g_return_val_if_fail(data != NULL && bs != NULL, -1);

	/* 000-003: uid */
	bytes += qq_get32(&bs->uid, data + bytes);
	/* 004-004: 0x01 */
	bytes += qq_get8(&bs->unknown1, data + bytes);
	/* this is no longer the IP, it seems QQ (as of 2006) no longer sends
	 * the buddy's IP in this packet. all 0s */
	/* 005-008: ip */
	bytes += qq_getIP(&bs->ip, data + bytes);
	/* port info is no longer here either */
	/* 009-010: port */
	bytes += qq_get16(&bs->port, data + bytes);
	/* 011-011: 0x00 */
	bytes += qq_get8(&bs->unknown2, data + bytes);
	/* 012-012: status */
	bytes += qq_get8(&bs->status, data + bytes);
	/* 013-014: client_version */
	bytes += qq_get16(&bs->unknown3, data + bytes);
	/* 015-030: unknown key */
	bytes += qq_getdata(&(bs->unknown_key[0]), QQ_KEY_LENGTH, data + bytes);

	purple_debug(PURPLE_DEBUG_INFO, "QQ_STATUS", 
			"uid: %d, U1: %d, ip: %s:%d, U2:%d, status:%d, U3:%04X\n", 
			bs->uid, bs->unknown1, inet_ntoa(bs->ip), bs->port,
			bs->unknown2, bs->status, bs->unknown3);

	return bytes;
}

#define QQ_ONLINE_BUDDY_ENTRY_LEN       38

/* process the reply packet for get_buddies_online packet */
guint8 qq_process_get_buddies_online_reply(guint8 *data, gint data_len, PurpleConnection *gc)
{
	qq_data *qd;
	gint bytes, bytes_buddy;
	gint count;
	guint8  position;
	PurpleBuddy *b;
	qq_buddy *q_bud;
	qq_buddy_online bo;
	gchar *purple_name;

	g_return_val_if_fail(data != NULL && data_len != 0, -1);

	qd = (qq_data *) gc->proto_data;

	/* qq_show_packet("Get buddies online reply packet", data, len); */

	bytes = 0;
	bytes += qq_get8(&position, data + bytes);

	count = 0;
	while (bytes < data_len) {
		if (data_len - bytes < QQ_ONLINE_BUDDY_ENTRY_LEN) {
			purple_debug(PURPLE_DEBUG_ERROR, "QQ", 
					"[buddies online] only %d, need %d", 
					(data_len - bytes), QQ_ONLINE_BUDDY_ENTRY_LEN);
			break;
		}
		memset(&bo, 0 ,sizeof(bo));
		
		/* set flag */
		bytes_buddy = bytes;
		/* based on one online buddy entry */
		/* 000-030 qq_buddy_status */
		bytes += get_buddy_status(&(bo.bs), data + bytes);
		/* 031-032: */
		bytes += qq_get16(&bo.unknown1, data + bytes);
		/* 033-033: ext_flag */
		bytes += qq_get8(&bo.ext_flag, data + bytes);
		/* 034-034: comm_flag */
		bytes += qq_get8(&bo.comm_flag, data + bytes);
		/* 035-036: */
		bytes += qq_get16(&bo.unknown2, data + bytes);
		/* 037-037: */
		bytes += qq_get8(&bo.ending, data + bytes);	/* 0x00 */

		if (bo.bs.uid == 0 || (bytes - bytes_buddy) != QQ_ONLINE_BUDDY_ENTRY_LEN) {
			purple_debug(PURPLE_DEBUG_ERROR, "QQ", 
					"uid=0 or entry complete len(%d) != %d", 
					(bytes - bytes_buddy), QQ_ONLINE_BUDDY_ENTRY_LEN);
			continue;
		}	/* check if it is a valid entry */

		if (bo.bs.uid == qd->uid) {
			purple_debug(PURPLE_DEBUG_WARNING, "QQ", 
					"I am in online list %d\n", bo.bs.uid);
			continue;
		}

		/* update buddy information */
		purple_name = uid_to_purple_name(bo.bs.uid);
		if (purple_name == NULL) {
			purple_debug(PURPLE_DEBUG_ERROR, "QQ", 
					"Got an online buddy %d, but not find purple name\n", bo.bs.uid);
			continue;
		}
		b = purple_find_buddy(purple_connection_get_account(gc), purple_name);
		g_free(purple_name);
		
		q_bud = (b == NULL) ? NULL : (qq_buddy *) b->proto_data;
		if (q_bud == NULL) {
			purple_debug(PURPLE_DEBUG_ERROR, "QQ", 
					"Got an online buddy %d, but not in my buddy list\n", bo.bs.uid);
			continue;
		}
		/* we find one and update qq_buddy */
		/*
		if(0 != fe->s->client_version)
			q_bud->client_version = fe->s->client_version;
		*/
		q_bud->ip.s_addr = bo.bs.ip.s_addr;
		q_bud->port = bo.bs.port;
		q_bud->status = bo.bs.status;
		q_bud->ext_flag = bo.ext_flag;
		q_bud->comm_flag = bo.comm_flag;
		qq_update_buddy_contact(gc, q_bud);
		count++;
	}

	if(bytes > data_len) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", 
				"qq_process_get_buddies_online_reply: Dangerous error! maybe protocol changed, notify developers!\n");
	}

	purple_debug(PURPLE_DEBUG_INFO, "QQ", "Received %d online buddies, nextposition=%u\n",
							count, (guint) position);
	return position;
}


/* process reply for get_buddies_list */
guint16 qq_process_get_buddies_list_reply(guint8 *data, gint data_len, PurpleConnection *gc)
{
	qq_data *qd;
	qq_buddy *q_bud;
	gint bytes_expected, count;
	gint bytes, buddy_bytes;
	guint16 position, unknown;
	guint8 pascal_len;
	gchar *name;
	PurpleBuddy *b;

	g_return_val_if_fail(data != NULL && data_len != 0, -1);

	qd = (qq_data *) gc->proto_data;

	if (data_len <= 2) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", "empty buddies list");
		return -1;
	}
	/* qq_show_packet("QQ get buddies list", data, data_len); */
	bytes = 0;
	bytes += qq_get16(&position, data + bytes);
	/* the following data is buddy list in this packet */
	count = 0;
	while (bytes < data_len) {
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
		bytes += qq_get8(&q_bud->ext_flag, data + bytes);
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

#if 1
		purple_debug(PURPLE_DEBUG_INFO, "QQ",
				"buddy [%09d]: ext_flag=0x%02x, comm_flag=0x%02x, nick=%s\n",
				q_bud->uid, q_bud->ext_flag, q_bud->comm_flag, q_bud->nickname);
#endif

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

	if(bytes > data_len) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", 
				"qq_process_get_buddies_list_reply: Dangerous error! maybe protocol changed, notify developers!");
	}

	purple_debug(PURPLE_DEBUG_INFO, "QQ", "Received %d buddies, nextposition=%u\n",
		count, (guint) position);
	return position;
}

guint32 qq_process_get_buddies_and_rooms(guint8 *data, gint data_len, PurpleConnection *gc)
{
	qq_data *qd;
	gint i, j;
	gint bytes;
	guint8 sub_cmd, reply_code;
	guint32 unknown, position;
	guint32 uid;
	guint8 type, groupid;
	qq_group *group;

	g_return_val_if_fail(data != NULL && data_len != 0, -1);

	qd = (qq_data *) gc->proto_data;

	bytes = 0;
	bytes += qq_get8(&sub_cmd, data + bytes);
	g_return_val_if_fail(sub_cmd == 0x01, -1);

	bytes += qq_get8(&reply_code, data + bytes);
	if(0 != reply_code) {
		purple_debug(PURPLE_DEBUG_WARNING, "QQ", 
				"qq_process_get_buddies_and_rooms, %d", reply_code);
	}

	bytes += qq_get32(&unknown, data + bytes);
	bytes += qq_get32(&position, data + bytes);
	/* the following data is all list in this packet */
	i = 0;
	j = 0;
	while (bytes < data_len) {
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
			group = qq_room_search_id(gc, uid);
			if(group == NULL) {
				purple_debug(PURPLE_DEBUG_INFO, "QQ",
					"Not find room id %d in qq_process_get_buddies_and_rooms\n", uid);
				qq_set_pending_id(&qd->adding_groups_from_server, uid, TRUE);
				qq_send_room_cmd_only(gc, QQ_ROOM_CMD_GET_INFO, uid);
			} else {
				group->my_status = QQ_GROUP_MEMBER_STATUS_IS_MEMBER;
				qq_group_refresh(gc, group);
				qq_send_room_cmd_only(gc, QQ_ROOM_CMD_GET_INFO, group->id);
			}
			++j;
		}
	}

	if(bytes > data_len) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", 
				"qq_process_get_buddies_and_rooms: Dangerous error! maybe protocol changed, notify developers!");
	}

	purple_debug(PURPLE_DEBUG_INFO, "QQ", "Received %d buddies and %d groups, nextposition=%u\n", i, j, (guint) position);
	return position;
}

#define QQ_MISC_STATUS_HAVING_VIIDEO      0x00000001
#define QQ_CHANGE_ONLINE_STATUS_REPLY_OK 	0x30	/* ASCII value of "0" */

/* TODO: figure out what's going on with the IP region. Sometimes I get valid IP addresses, 
 * but the port number's weird, other times I get 0s. I get these simultaneously on the same buddy, 
 * using different accounts to get info. */

/* check if status means online or offline */
gboolean is_online(guint8 status)
{
	switch(status) {
		case QQ_BUDDY_ONLINE_NORMAL:
		case QQ_BUDDY_ONLINE_AWAY:
		case QQ_BUDDY_ONLINE_INVISIBLE:
			return TRUE;
		case QQ_BUDDY_ONLINE_OFFLINE:
			return FALSE;
	}
	return FALSE;
}

/* Help calculate the correct icon index to tell the server. */
gint get_icon_offset(PurpleConnection *gc)
{ 
	PurpleAccount *account;
	PurplePresence *presence; 

	account = purple_connection_get_account(gc);
	presence = purple_account_get_presence(account);

	if (purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_INVISIBLE)) {
		return 2;
	} else if (purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_AWAY)
			|| purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_EXTENDED_AWAY)
			|| purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_UNAVAILABLE)) {
		return 1;
	} else {
		return 0;
	}
}

/* send a packet to change my online status */
void qq_send_packet_change_status(PurpleConnection *gc)
{
	qq_data *qd;
	guint8 raw_data[16] = {0};
	gint bytes = 0;
	guint8 away_cmd;
	guint32 misc_status;
	gboolean fake_video;
	PurpleAccount *account;
	PurplePresence *presence; 

	account = purple_connection_get_account(gc);
	presence = purple_account_get_presence(account);

	qd = (qq_data *) gc->proto_data;
	if (!qd->logged_in)
		return;

	if (purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_INVISIBLE)) {
		away_cmd = QQ_BUDDY_ONLINE_INVISIBLE;
	} else if (purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_AWAY)
			|| purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_EXTENDED_AWAY)
			|| purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_UNAVAILABLE)) {
		away_cmd = QQ_BUDDY_ONLINE_AWAY;
	} else {
		away_cmd = QQ_BUDDY_ONLINE_NORMAL;
	}

	misc_status = 0x00000000;
	fake_video = purple_prefs_get_bool("/plugins/prpl/qq/show_fake_video");
	if (fake_video)
		misc_status |= QQ_MISC_STATUS_HAVING_VIIDEO;

	bytes = 0;
	bytes += qq_put8(raw_data + bytes, away_cmd);
	bytes += qq_put32(raw_data + bytes, misc_status);

	qq_send_cmd(qd, QQ_CMD_CHANGE_ONLINE_STATUS, raw_data, bytes);
}

/* parse the reply packet for change_status */
void qq_process_change_status_reply(guint8 *data, gint data_len, PurpleConnection *gc)
{
	qq_data *qd;
	gint bytes;
	guint8 reply;
	PurpleBuddy *b;
	qq_buddy *q_bud;
	gchar *name;

	g_return_if_fail(data != NULL && data_len != 0);

	qd = (qq_data *) gc->proto_data;
	
	bytes = 0;
	bytes = qq_get8(&reply, data + bytes);
	if (reply != QQ_CHANGE_ONLINE_STATUS_REPLY_OK) {
		purple_debug(PURPLE_DEBUG_WARNING, "QQ", "Change status fail 0x%02X\n", reply);
		return;
	}

	/* purple_debug(PURPLE_DEBUG_INFO, "QQ", "Change status OK\n"); */
	name = uid_to_purple_name(qd->uid);
	b = purple_find_buddy(gc->account, name);
	g_free(name);
	q_bud = (b == NULL) ? NULL : (qq_buddy *) b->proto_data;
	if (q_bud != NULL) {
		qq_update_buddy_contact(gc, q_bud);
	}
}

/* it is a server message indicating that one of my buddies has changed its status */
void qq_process_buddy_change_status(guint8 *data, gint data_len, PurpleConnection *gc) 
{
	qq_data *qd;
	gint bytes;
	guint32 my_uid;
	PurpleBuddy *b;
	qq_buddy *q_bud;
	qq_buddy_status bs;
	gchar *name;

	g_return_if_fail(data != NULL && data_len != 0);

	qd = (qq_data *) gc->proto_data;

	if (data_len < 35) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", "[buddy status change] only %d, need 35 bytes\n", data_len);
		return;
	}
	
	memset(&bs, 0, sizeof(bs));
	bytes = 0;
	/* 000-030: qq_buddy_status */
	bytes += get_buddy_status(&bs, data + bytes);
	/* 031-034:  Unknow, maybe my uid */ 
	/* This has a value of 0 when we've changed our status to 
	 * QQ_BUDDY_ONLINE_INVISIBLE */
	bytes += qq_get32(&my_uid, data + bytes);

	name = uid_to_purple_name(bs.uid);
	b = purple_find_buddy(gc->account, name);
	g_free(name);
	q_bud = (b == NULL) ? NULL : (qq_buddy *) b->proto_data;
	if (q_bud == NULL) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", 
				"got information of unknown buddy %d\n", bs.uid);
		return;
	}

	purple_debug(PURPLE_DEBUG_INFO, "QQ", "status:.uid = %d, q_bud->uid = %d\n", bs.uid , q_bud->uid);
	if(bs.ip.s_addr != 0) { 
		q_bud->ip.s_addr = bs.ip.s_addr;
		q_bud->port = bs.port;
	}
	q_bud->status =bs.status;

	if (q_bud->status == QQ_BUDDY_ONLINE_NORMAL) {
		qq_send_packet_get_level(gc, q_bud->uid);
	}
	qq_update_buddy_contact(gc, q_bud);
}

/*TODO: maybe this should be qq_update_buddy_status() ?*/
void qq_update_buddy_contact(PurpleConnection *gc, qq_buddy *q_bud)
{
	gchar *purple_name;
	PurpleBuddy *bud;
	gchar *status_id;
	
	g_return_if_fail(q_bud != NULL);

	purple_name = uid_to_purple_name(q_bud->uid);
	if (purple_name == NULL) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", "Not find purple name: %d\n", q_bud->uid);
		return;
	}

	bud = purple_find_buddy(gc->account, purple_name);
	if (bud == NULL) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", "Not find buddy: %d\n", q_bud->uid);
		g_free(purple_name);
		return;
	}
	
	purple_blist_server_alias_buddy(bud, q_bud->nickname); /* server */
	q_bud->last_refresh = time(NULL);

	/* purple supports signon and idle time
	 * but it is not much use for QQ, I do not use them */
	/* serv_got_update(gc, name, online, 0, q_bud->signon, q_bud->idle, bud->uc); */
	status_id = "available";
	switch(q_bud->status) {
	case QQ_BUDDY_OFFLINE:
		status_id = "offline";
		break;
	case QQ_BUDDY_ONLINE_NORMAL:
		status_id = "available";
		break;
	case QQ_BUDDY_ONLINE_OFFLINE:
		status_id = "offline";
		break;
	case QQ_BUDDY_ONLINE_AWAY:
		status_id = "away";
		break;
	case QQ_BUDDY_ONLINE_INVISIBLE:
		status_id = "invisible";
		break;
	default:
		status_id = "invisible";
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", "unknown status: %x\n", q_bud->status);
		break;
	}
	purple_debug(PURPLE_DEBUG_INFO, "QQ", "buddy %d %s\n", q_bud->uid, status_id);
	purple_prpl_got_user_status(gc->account, purple_name, status_id, NULL);

	if (q_bud->comm_flag & QQ_COMM_FLAG_MOBILE && q_bud->status != QQ_BUDDY_OFFLINE)
		purple_prpl_got_user_status(gc->account, purple_name, "mobile", NULL);
	else
		purple_prpl_got_user_status_deactive(gc->account, purple_name, "mobile");

	if (q_bud->comm_flag & QQ_COMM_FLAG_VIDEO && q_bud->status != QQ_BUDDY_OFFLINE)
		purple_prpl_got_user_status(gc->account, purple_name, "video", NULL);
	else
		purple_prpl_got_user_status_deactive(gc->account, purple_name, "video");

	g_free(purple_name);
}

/* refresh all buddies online/offline,
 * after receiving reply for get_buddies_online packet */
void qq_refresh_all_buddy_status(PurpleConnection *gc)
{
	time_t now;
	GList *list;
	qq_data *qd;
	qq_buddy *q_bud;

	qd = (qq_data *) (gc->proto_data);
	now = time(NULL);
	list = qd->buddies;

	while (list != NULL) {
		q_bud = (qq_buddy *) list->data;
		if (q_bud != NULL && now > q_bud->last_refresh + QQ_UPDATE_ONLINE_INTERVAL
				&& q_bud->status != QQ_BUDDY_ONLINE_INVISIBLE) {
			q_bud->status = QQ_BUDDY_ONLINE_OFFLINE;
			qq_update_buddy_contact(gc, q_bud);
		}
		list = list->next;
	}
}
