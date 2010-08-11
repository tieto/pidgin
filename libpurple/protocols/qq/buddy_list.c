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
#include "buddy_memo.h"
#include "buddy_list.h"
#include "buddy_opt.h"
#include "char_conv.h"
#include "qq_define.h"
#include "qq_base.h"
#include "group.h"
#include "group_internal.h"
#include "group_info.h"

#include "qq_network.h"

#define QQ_GET_ONLINE_BUDDY_02          0x02
#define QQ_GET_ONLINE_BUDDY_03          0x03	/* unknown function */

typedef struct _qq_buddy_online {
	guint16 unknown1;
	guint8 ext_flag;
	guint8 comm_flag;
	guint16 unknown2;
	guint8 ending;		/* 0x00 */
} qq_buddy_online;

/* get a list of online_buddies */
void qq_request_get_buddies_online(PurpleConnection *gc, guint8 position, guint32 update_class)
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

	qq_send_cmd_mess(gc, QQ_CMD_GET_BUDDIES_ONLINE, raw_data, 5, update_class, 0);
}

/* position starts with 0x0000,
 * server may return a position tag if list is too long for one packet */
void qq_request_get_buddies(PurpleConnection *gc, guint16 position, guint32 update_class)
{
	qq_data *qd;
	guint8 raw_data[16] = {0};
	gint bytes = 0;

	qd = (qq_data *) gc->proto_data;

	/* 000-001 starting position, can manually specify */
	bytes += qq_put16(raw_data + bytes, position);
	/* before Mar 18, 2004, any value can work, and we sent 00
	 * I do not know what data QQ server is expecting, as QQ2003iii 0304 itself
	 * even can sending packets 00 and get no response.
	 * Now I tested that 00,00,00,00,00,01 work perfectly
	 * March 22, found the 00,00,00 starts to work as well */
	bytes += qq_put8(raw_data + bytes, 0x00);
	if (qd->client_version >= 2007) {
		bytes += qq_put16(raw_data + bytes, 0x0000);
	}

	qq_send_cmd_mess(gc, QQ_CMD_GET_BUDDIES_LIST, raw_data, bytes, update_class, 0);
}

/* get all list, buddies & Quns with groupsid support */
void qq_request_get_buddies_and_rooms(PurpleConnection *gc, guint32 position, guint32 update_class)
{
	guint8 raw_data[16] = {0};
	gint bytes = 0;

	/* 0x01 download, 0x02, upload */
	bytes += qq_put8(raw_data + bytes, 0x01);
	/* unknown 0x02 */
	bytes += qq_put8(raw_data + bytes, 0x02);
	/* unknown 00 00 00 00 */
	bytes += qq_put32(raw_data + bytes, 0x00000000);
	bytes += qq_put32(raw_data + bytes, position);

	qq_send_cmd_mess(gc, QQ_CMD_GET_BUDDIES_AND_ROOMS, raw_data, bytes, update_class, 0);
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
	/* 013-014: client tag */
	bytes += qq_get16(&bs->unknown3, data + bytes);
	/* 015-030: unknown key */
	bytes += qq_getdata(&(bs->unknown_key[0]), QQ_KEY_LENGTH, data + bytes);

	purple_debug_info("QQ", "Status:%d, uid: %u, ip: %s:%d, U: %d - %d - %04X\n",
			bs->status, bs->uid, inet_ntoa(bs->ip), bs->port,
			bs->unknown1, bs->unknown2, bs->unknown3);

	return bytes;
}

/* process the reply packet for get_buddies_online packet */
guint8 qq_process_get_buddies_online(guint8 *data, gint data_len, PurpleConnection *gc)
{
	qq_data *qd;
	gint bytes, bytes_start;
	gint count;
	guint8  position;
	gchar *who;
	PurpleBuddy *buddy;
	qq_buddy_data *bd;
	int entry_len = 38;

	qq_buddy_status bs;
	struct {
		guint16 unknown1;
		guint8 ext_flag;
		guint8 comm_flag;
		guint16 unknown2;
		guint8 ending;		/* 0x00 */
	} packet;

	g_return_val_if_fail(data != NULL && data_len != 0, -1);

	qd = (qq_data *) gc->proto_data;

	/* qq_show_packet("Get buddies online reply packet", data, len); */
	if (qd->client_version >= 2007)	entry_len += 4;

	bytes = 0;
	bytes += qq_get8(&position, data + bytes);

	count = 0;
	while (bytes < data_len) {
		if (data_len - bytes < entry_len) {
			purple_debug_error("QQ", "[buddies online] only %d, need %d\n",
					(data_len - bytes), entry_len);
			break;
		}
		memset(&bs, 0 ,sizeof(bs));
		memset(&packet, 0 ,sizeof(packet));

		/* set flag */
		bytes_start = bytes;
		/* based on one online buddy entry */
		/* 000-030 qq_buddy_status */
		bytes += get_buddy_status(&bs, data + bytes);
		/* 031-032: */
		bytes += qq_get16(&packet.unknown1, data + bytes);
		/* 033-033: ext_flag */
		bytes += qq_get8(&packet.ext_flag, data + bytes);
		/* 034-034: comm_flag */
		bytes += qq_get8(&packet.comm_flag, data + bytes);
		/* 035-036: */
		bytes += qq_get16(&packet.unknown2, data + bytes);
		/* 037-037: */
		bytes += qq_get8(&packet.ending, data + bytes);	/* 0x00 */
		/* skip 4 bytes in qq2007 */
		if (qd->client_version >= 2007)	bytes += 4;

		if (bs.uid == 0 || (bytes - bytes_start) != entry_len) {
			purple_debug_error("QQ", "uid=0 or entry complete len(%d) != %d",
					(bytes - bytes_start), entry_len);
			continue;
		}	/* check if it is a valid entry */

		if (bs.uid == qd->uid) {
			purple_debug_warning("QQ", "I am in online list %u\n", bs.uid);
		}

		/* update buddy information */
		who = uid_to_purple_name(bs.uid);
		buddy = purple_find_buddy(gc->account, who);
		g_free(who);
		if (buddy == NULL) {
			/* create no-auth buddy */
			buddy = qq_buddy_new(gc, bs.uid);
		}
		bd = (buddy == NULL) ? NULL : (qq_buddy_data *)purple_buddy_get_protocol_data(buddy);
		if (bd == NULL) {
			purple_debug_error("QQ",
					"Got an online buddy %u, but not in my buddy list\n", bs.uid);
			continue;
		}
		/*
		if(0 != fe->s->client_tag)
			q_bud->client_tag = fe->s->client_tag;
		*/
		if (bd->status != bs.status || bd->comm_flag != packet.comm_flag) {
			bd->status = bs.status;
			bd->comm_flag = packet.comm_flag;
			qq_update_buddy_status(gc, bd->uid, bd->status, bd->comm_flag);
		}
		bd->ip.s_addr = bs.ip.s_addr;
		bd->port = bs.port;
		bd->ext_flag = packet.ext_flag;
		bd->last_update = time(NULL);
		count++;
	}

	if(bytes > data_len) {
		purple_debug_error("QQ",
				"qq_process_get_buddies_online: Dangerous error! maybe protocol changed, notify developers!\n");
	}

	purple_debug_info("QQ", "Received %d online buddies, nextposition=%u\n",
			count, (guint) position);
	return position;
}


/* process reply for get_buddies_list */
guint16 qq_process_get_buddies(guint8 *data, gint data_len, PurpleConnection *gc)
{
	qq_data *qd;
	qq_buddy_data bd;
	gint bytes_expected, count;
	gint bytes, buddy_bytes;
	gint nickname_len;
	guint16 position, unknown;
	PurpleBuddy *buddy;

	g_return_val_if_fail(data != NULL && data_len != 0, -1);

	qd = (qq_data *) gc->proto_data;

	if (data_len <= 2) {
		purple_debug_error("QQ", "empty buddies list");
		return -1;
	}
	/* qq_show_packet("QQ get buddies list", data, data_len); */
	bytes = 0;
	bytes += qq_get16(&position, data + bytes);
	/* the following data is buddy list in this packet */
	count = 0;
	while (bytes < data_len) {
		memset(&bd, 0, sizeof(bd));
		/* set flag */
		buddy_bytes = bytes;
		/* 000-003: uid */
		bytes += qq_get32(&bd.uid, data + bytes);
		/* 004-005: icon index (1-255) */
		bytes += qq_get16(&bd.face, data + bytes);
		/* 006-006: age */
		bytes += qq_get8(&bd.age, data + bytes);
		/* 007-007: gender */
		bytes += qq_get8(&bd.gender, data + bytes);

		nickname_len = qq_get_vstr(&bd.nickname, QQ_CHARSET_DEFAULT, data + bytes);
		bytes += nickname_len;
		qq_filter_str(bd.nickname);

		/* Fixme: merge following as 32bit flag */
		bytes += qq_get16(&unknown, data + bytes);
		bytes += qq_get8(&bd.ext_flag, data + bytes);
		bytes += qq_get8(&bd.comm_flag, data + bytes);

		if (qd->client_version >= 2007) {
			bytes += 4;		/* skip 4 bytes */
			bytes_expected = 16 + nickname_len;
		} else {
			bytes_expected = 12 + nickname_len;
		}

		if (bd.uid == 0 || (bytes - buddy_bytes) != bytes_expected) {
			purple_debug_info("QQ",
					"Buddy entry, expect %d bytes, read %d bytes\n",
					bytes_expected, bytes - buddy_bytes);
			g_free(bd.nickname);
			continue;
		} else {
			count++;
		}

#if 1
		purple_debug_info("QQ", "buddy [%09d]: ext_flag=0x%02x, comm_flag=0x%02x, nick=%s\n",
				bd.uid, bd.ext_flag, bd.comm_flag, bd.nickname);
#endif

		buddy = qq_buddy_find_or_new(gc, bd.uid);
		if (buddy == NULL || purple_buddy_get_protocol_data(buddy) == NULL) {
			g_free(bd.nickname);
			continue;
		}
		purple_blist_server_alias_buddy(buddy, bd.nickname);
		bd.last_update = time(NULL);
		qq_update_buddy_status(gc, bd.uid, bd.status, bd.comm_flag);

		g_memmove(purple_buddy_get_protocol_data(buddy), &bd, sizeof(qq_buddy_data));
		/* nickname has been copy to buddy_data do not free
		   g_free(bd.nickname);
		*/
		/*qq_request_buddy_memo(gc, ((qq_buddy_data*)buddy->proto_data)->uid, 0, QQ_BUDDY_MEMO_GET);*/
		qq_request_buddy_memo(gc, bd.uid, bd.uid, QQ_BUDDY_MEMO_GET);
	}

	if(bytes > data_len) {
		purple_debug_error("QQ",
				"qq_process_get_buddies: Dangerous error! maybe protocol changed, notify developers!");
	}

	purple_debug_info("QQ", "Received %d buddies, nextposition=%u\n",
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
	guint8 type;
	qq_room_data *rmd;

	g_return_val_if_fail(data != NULL && data_len != 0, -1);

	qd = (qq_data *) gc->proto_data;

	bytes = 0;
	bytes += qq_get8(&sub_cmd, data + bytes);
	g_return_val_if_fail(sub_cmd == 0x01, -1);

	bytes += qq_get8(&reply_code, data + bytes);
	if(0 != reply_code) {
		purple_debug_warning("QQ", "qq_process_get_buddies_and_rooms, %d\n", reply_code);
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
		/* 05: skip unknow 0x00 */
		bytes += 1;
		if (uid == 0 || (type != 0x1 && type != 0x4)) {
			purple_debug_info("QQ", "Buddy entry, uid=%u, type=%d", uid, type);
			continue;
		}
		if(0x1 == type) { /* a buddy */
			/* don't do anything but count - buddies are handled by
			 * qq_request_get_buddies */
			++i;
		} else { /* a group */
			rmd = qq_room_data_find(gc, uid);
			if(rmd == NULL) {
				purple_debug_info("QQ", "Unknow room id %u", uid);
				qq_send_room_cmd_only(gc, QQ_ROOM_CMD_GET_INFO, uid);
			} else {
				rmd->my_role = QQ_ROOM_ROLE_YES;
			}
			++j;
		}
	}

	if(bytes > data_len) {
		purple_debug_error("QQ",
				"qq_process_get_buddies_and_rooms: Dangerous error! maybe protocol changed, notify developers!");
	}

	purple_debug_info("QQ", "Received %d buddies and %d groups, nextposition=%u\n", i, j, (guint) position);
	return position;
}

#define QQ_MISC_STATUS_HAVING_VIIDEO      0x00000001
#define QQ_CHANGE_ONLINE_STATUS_REPLY_OK 	0x30	/* ASCII value of "0" */

/* TODO: figure out what's going on with the IP region. Sometimes I get valid IP addresses,
 * but the port number's weird, other times I get 0s. I get these simultaneously on the same buddy,
 * using different accounts to get info. */
static guint8  get_status_from_purple(PurpleConnection *gc)
{
	qq_data *qd;
	PurpleAccount *account;
	PurplePresence *presence;
	guint8 ret;

	qd = (qq_data *) gc->proto_data;
	account = purple_connection_get_account(gc);
	presence = purple_account_get_presence(account);

	if (purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_INVISIBLE)) {
		ret = QQ_BUDDY_ONLINE_INVISIBLE;
	} else if (purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_UNAVAILABLE))
	{
		if (qd->client_version >= 2007) {
			ret = QQ_BUDDY_ONLINE_BUSY;
		} else {
			ret = QQ_BUDDY_ONLINE_INVISIBLE;
		}
	} else if (purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_AWAY)
			|| purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_EXTENDED_AWAY)
			|| purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_UNAVAILABLE)) {
		ret = QQ_BUDDY_ONLINE_AWAY;
	} else {
		ret = QQ_BUDDY_ONLINE_NORMAL;
	}
	return ret;
}

/* send a packet to change my online status */
void qq_request_change_status(PurpleConnection *gc, guint32 update_class)
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
	if (!qd->is_login)
		return;

	away_cmd = get_status_from_purple(gc);

	misc_status = 0x00000000;
	fake_video = purple_prefs_get_bool("/plugins/prpl/qq/show_fake_video");
	if (fake_video)
		misc_status |= QQ_MISC_STATUS_HAVING_VIIDEO;

	if (qd->client_version >= 2007) {
		bytes = 0;
		bytes += qq_put8(raw_data + bytes, away_cmd);
		/* status version */
		bytes += qq_put16(raw_data + bytes, 0);
		bytes += qq_put16(raw_data + bytes, 0);
		bytes += qq_put32(raw_data + bytes, misc_status);
		/* Fixme: custom status message, now is empty */
		bytes += qq_put16(raw_data + bytes, 0);
	} else {
		bytes = 0;
		bytes += qq_put8(raw_data + bytes, away_cmd);
		bytes += qq_put32(raw_data + bytes, misc_status);
	}
	qq_send_cmd_mess(gc, QQ_CMD_CHANGE_STATUS, raw_data, bytes, update_class, 0);
}

/* parse the reply packet for change_status */
void qq_process_change_status(guint8 *data, gint data_len, PurpleConnection *gc)
{
	qq_data *qd;
	gint bytes;
	guint8 reply;
	qq_buddy_data *bd;

	g_return_if_fail(data != NULL && data_len != 0);

	qd = (qq_data *) gc->proto_data;

	bytes = 0;
	bytes = qq_get8(&reply, data + bytes);
	if (reply != QQ_CHANGE_ONLINE_STATUS_REPLY_OK) {
		purple_debug_warning("QQ", "Change status fail 0x%02X\n", reply);
		return;
	}

	/* purple_debug_info("QQ", "Change status OK\n"); */
	bd = qq_buddy_data_find(gc, qd->uid);
	if (bd != NULL) {
		bd->status = get_status_from_purple(gc);
		bd->last_update = time(NULL);
		qq_update_buddy_status(gc, bd->uid, bd->status, bd->comm_flag);
	}
}

/* it is a server message indicating that one of my buddies has changed its status */
void qq_process_buddy_change_status(guint8 *data, gint data_len, PurpleConnection *gc)
{
	qq_data *qd;
	gint bytes;
	guint32 my_uid;
	gchar *who;
	PurpleBuddy *buddy;
	qq_buddy_data *bd;
	qq_buddy_status bs;

	g_return_if_fail(data != NULL && data_len != 0);

	qd = (qq_data *) gc->proto_data;

	if (data_len < 35) {
		purple_debug_error("QQ", "[buddy status change] only %d, need 35 bytes\n", data_len);
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

	/* update buddy information */
	who = uid_to_purple_name(bs.uid);
	buddy = purple_find_buddy(gc->account, who);
	g_free(who);
	if (buddy == NULL) {
		/* create no-auth buddy */
		buddy = qq_buddy_new(gc, bs.uid);
	}
	bd = (buddy == NULL) ? NULL : (qq_buddy_data *)purple_buddy_get_protocol_data(buddy);
	if (bd == NULL) {
		purple_debug_warning("QQ", "Got status of no-auth buddy %u\n", bs.uid);
		return;
	}

	if(bs.ip.s_addr != 0) {
		bd->ip.s_addr = bs.ip.s_addr;
		bd->port = bs.port;
	}
	if (bd->status != bs.status) {
		bd->status = bs.status;
		qq_update_buddy_status(gc, bd->uid, bd->status, bd->comm_flag);
	}
	bd->last_update = time(NULL);

	if (bd->status == QQ_BUDDY_ONLINE_NORMAL && bd->level <= 0) {
		if (qd->client_version >= 2007) {
			qq_request_get_level_2007(gc, bd->uid);
		} else {
			qq_request_get_level(gc, bd->uid);
		}
	}
}

/*TODO: maybe this should be qq_update_buddy_status() ?*/
void qq_update_buddy_status(PurpleConnection *gc, guint32 uid, guint8 status, guint8 flag)
{
	gchar *who;
	gchar *status_id;

	g_return_if_fail(uid != 0);

	/* purple supports signon and idle time
	 * but it is not much use for QQ, I do not use them */
	/* serv_got_update(gc, name, online, 0, q_bud->signon, q_bud->idle, bud->uc); */
	status_id = "available";
	switch(status) {
	case QQ_BUDDY_OFFLINE:
		status_id = "offline";
		break;
	case QQ_BUDDY_ONLINE_NORMAL:
		status_id = "available";
		break;
	case QQ_BUDDY_CHANGE_TO_OFFLINE:
		status_id = "offline";
		break;
	case QQ_BUDDY_ONLINE_AWAY:
		status_id = "away";
		break;
	case QQ_BUDDY_ONLINE_INVISIBLE:
		status_id = "invisible";
		break;
	case QQ_BUDDY_ONLINE_BUSY:
		status_id = "busy";
		break;
	default:
		status_id = "invisible";
		purple_debug_error("QQ", "unknown status: 0x%X\n", status);
		break;
	}

	purple_debug_info("QQ", "buddy %u status = %s\n", uid, status_id);
	who = uid_to_purple_name(uid);
	purple_prpl_got_user_status(gc->account, who, status_id, NULL);

	if (flag & QQ_COMM_FLAG_MOBILE && status != QQ_BUDDY_OFFLINE)
		purple_prpl_got_user_status(gc->account, who, "mobile", NULL);
	else
		purple_prpl_got_user_status_deactive(gc->account, who, "mobile");

	g_free(who);
}

/* refresh all buddies online/offline,
 * after receiving reply for get_buddies_online packet */
void qq_update_buddyies_status(PurpleConnection *gc)
{
	qq_data *qd;
	PurpleBuddy *buddy;
	qq_buddy_data *bd;
	GSList *buddies, *it;
	time_t tm_limit = time(NULL);

	qd = (qq_data *) (gc->proto_data);

	tm_limit -= QQ_UPDATE_ONLINE_INTERVAL;

	buddies = purple_find_buddies(purple_connection_get_account(gc), NULL);
	for (it = buddies; it; it = it->next) {
		buddy = it->data;
		if (buddy == NULL) continue;

		bd = purple_buddy_get_protocol_data(buddy);
		if (bd == NULL) continue;

		if (bd->uid == 0) continue;
		if (bd->uid == qd->uid) continue;	/* my status is always online in my buddy list */
		if (tm_limit < bd->last_update) continue;
		if (bd->status == QQ_BUDDY_ONLINE_INVISIBLE) continue;
		if (bd->status == QQ_BUDDY_CHANGE_TO_OFFLINE) continue;

		bd->status = QQ_BUDDY_CHANGE_TO_OFFLINE;
		bd->last_update = time(NULL);
		qq_update_buddy_status(gc, bd->uid, bd->status, bd->comm_flag);
	}
}

void qq_buddy_data_free_all(PurpleConnection *gc)
{
	qq_data *qd;
	PurpleBuddy *buddy;
	GSList *buddies, *it;
	gint count = 0;

	qd = (qq_data *)purple_connection_get_protocol_data(gc);

	buddies = purple_find_buddies(purple_connection_get_account(gc), NULL);
	for (it = buddies; it; it = it->next) {
		qq_buddy_data *qbd = NULL;

		buddy = it->data;
		if (buddy == NULL) continue;

		qbd = purple_buddy_get_protocol_data(buddy);
		if (qbd == NULL) continue;

		qq_buddy_data_free(qbd);
		purple_buddy_set_protocol_data(buddy, NULL);

		count++;
	}

	if (count > 0) {
		purple_debug_info("QQ", "%d buddies' data are freed\n", count);
	}
}

