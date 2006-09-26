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

#include <string.h>
#include "debug.h"
#include "prefs.h"

#include "buddy_status.h"
#include "crypt.h"
#include "header_info.h"
#include "keep_alive.h"
#include "packet_parse.h"
#include "send_core.h"
#include "utils.h"

#include "qq_proxy.h"

#define QQ_MISC_STATUS_HAVING_VIIDEO      0x00000001
#define QQ_CHANGE_ONLINE_STATUS_REPLY_OK 	0x30	/* ASCII value of "0" */

void qq_buddy_status_dump_unclear(qq_buddy_status *s)
{
	GString *dump;

	g_return_if_fail(s != NULL);

	dump = g_string_new("");
	g_string_append_printf(dump, "unclear fields for [%d]:\n", s->uid);
	g_string_append_printf(dump, "004:     %02x   (unknown)\n", s->unknown1);
	/* g_string_append_printf(dump, "005-008:     %09x   (ip)\n", *(s->ip)); */
	g_string_append_printf(dump, "009-010:     %04x   (port)\n", s->port);
	g_string_append_printf(dump, "011:     %02x   (unknown)\n", s->unknown2);
	g_string_append_printf(dump, "012:     %02x   (status)\n", s->status);
	g_string_append_printf(dump, "013-014:     %04x   (client_version)\n", s->client_version);
	/* g_string_append_printf(dump, "015-030:     %s   (unknown key)\n", s->unknown_key); */
	gaim_debug(GAIM_DEBUG_INFO, "QQ", "Buddy status entry, %s", dump->str);
	_qq_show_packet("Unknown key", s->unknown_key, QQ_KEY_LENGTH);
	g_string_free(dump, TRUE);
}

/* TODO: figure out what's going on with the IP region. Sometimes I get valid IP addresses, 
 * but the port number's weird, other times I get 0s. I get these simultaneously on the same buddy, 
 * using different accounts to get info. */

/* parse the data into qq_buddy_status */
gint qq_buddy_status_read(guint8 *data, guint8 **cursor, gint len, qq_buddy_status *s)
{
	gint bytes;

	g_return_val_if_fail(data != NULL && *cursor != NULL && s != NULL, -1);

	bytes = 0;

	/* 000-003: uid */
	bytes += read_packet_dw(data, cursor, len, &s->uid);
	/* 004-004: 0x01 */
	bytes += read_packet_b(data, cursor, len, &s->unknown1);
	/* this is no longer the IP, it seems QQ (as of 2006) no longer sends
	 * the buddy's IP in this packet. all 0s */
	/* 005-008: ip */
	s->ip = g_new0(guint8, 4);
	bytes += read_packet_data(data, cursor, len, s->ip, 4);
	/* port info is no longer here either */
	/* 009-010: port */
	bytes += read_packet_w(data, cursor, len, &s->port);
	/* 011-011: 0x00 */
	bytes += read_packet_b(data, cursor, len, &s->unknown2);
	/* 012-012: status */
	bytes += read_packet_b(data, cursor, len, &s->status);
	/* 013-014: client_version */
	bytes += read_packet_w(data, cursor, len, &s->client_version);
	/* 015-030: unknown key */
	s->unknown_key = g_new0(guint8, QQ_KEY_LENGTH);
	bytes += read_packet_data(data, cursor, len, s->unknown_key, QQ_KEY_LENGTH);

	if (s->uid == 0 || bytes != 31)
		return -1;

	return bytes;
}

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
gint get_icon_offset(GaimConnection *gc)
{ 
	GaimAccount *account;
	GaimPresence *presence; 

	account = gaim_connection_get_account(gc);
	presence = gaim_account_get_presence(account);

	if (gaim_presence_is_status_primitive_active(presence, GAIM_STATUS_INVISIBLE)) {
		return 2;
	} else if (gaim_presence_is_status_primitive_active(presence, GAIM_STATUS_AWAY)
			|| gaim_presence_is_status_primitive_active(presence, GAIM_STATUS_EXTENDED_AWAY)
			|| gaim_presence_is_status_primitive_active(presence, GAIM_STATUS_UNAVAILABLE)) {
		return 1;
        } else {
		return 0;
	}
}

/* send a packet to change my online status */
void qq_send_packet_change_status(GaimConnection *gc)
{
	qq_data *qd;
	guint8 *raw_data, *cursor, away_cmd;
	guint32 misc_status;
	gboolean fake_video;
	GaimAccount *account;
	GaimPresence *presence; 

	account = gaim_connection_get_account(gc);
	presence = gaim_account_get_presence(account);

	qd = (qq_data *) gc->proto_data;
	if (!qd->logged_in)
		return;

	if (gaim_presence_is_status_primitive_active(presence, GAIM_STATUS_INVISIBLE)) {
		away_cmd = QQ_BUDDY_ONLINE_INVISIBLE;
	} else if (gaim_presence_is_status_primitive_active(presence, GAIM_STATUS_AWAY)
			|| gaim_presence_is_status_primitive_active(presence, GAIM_STATUS_EXTENDED_AWAY)
			|| gaim_presence_is_status_primitive_active(presence, GAIM_STATUS_UNAVAILABLE)) {
		away_cmd = QQ_BUDDY_ONLINE_AWAY;
	} else {
		away_cmd = QQ_BUDDY_ONLINE_NORMAL;
	}

	raw_data = g_new0(guint8, 5);
	cursor = raw_data;
	misc_status = 0x00000000;

	fake_video = gaim_prefs_get_bool("/plugins/prpl/qq/show_fake_video");
	if (fake_video)
		misc_status |= QQ_MISC_STATUS_HAVING_VIIDEO;

	create_packet_b(raw_data, &cursor, away_cmd);
	create_packet_dw(raw_data, &cursor, misc_status);

	qq_send_cmd(gc, QQ_CMD_CHANGE_ONLINE_STATUS, TRUE, 0, TRUE, raw_data, 5);

	g_free(raw_data);
}

/* parse the reply packet for change_status */
void qq_process_change_status_reply(guint8 *buf, gint buf_len, GaimConnection *gc)
{
	qq_data *qd;
	gint len;
	guint8 *data, *cursor, reply;
	GaimBuddy *b;
	qq_buddy *q_bud;
	gchar *name;

	g_return_if_fail(buf != NULL && buf_len != 0);

	qd = (qq_data *) gc->proto_data;
	len = buf_len;
	data = g_newa(guint8, len);

	if (qq_crypt(DECRYPT, buf, buf_len, qd->session_key, data, &len)) {
		cursor = data;
		read_packet_b(data, &cursor, len, &reply);
		if (reply != QQ_CHANGE_ONLINE_STATUS_REPLY_OK) {
			gaim_debug(GAIM_DEBUG_WARNING, "QQ", "Change status fail\n");
		} else {
			gaim_debug(GAIM_DEBUG_INFO, "QQ", "Change status OK\n");
			name = uid_to_gaim_name(qd->uid);
			b = gaim_find_buddy(gc->account, name);
			g_free(name);
			q_bud = (b == NULL) ? NULL : (qq_buddy *) b->proto_data;
			qq_update_buddy_contact(gc, q_bud);
		}
	} else {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Error decrypt chg status reply\n");
	}
}

/* it is a server message indicating that one of my buddies has changed its status */
void qq_process_friend_change_status(guint8 *buf, gint buf_len, GaimConnection *gc) 
{
	qq_data *qd;
	gint len, bytes;
	guint32 my_uid;
	guint8 *data, *cursor;
	GaimBuddy *b;
	qq_buddy *q_bud;
	qq_buddy_status *s;
	gchar *name;

	g_return_if_fail(buf != NULL && buf_len != 0);

	qd = (qq_data *) gc->proto_data;
	len = buf_len;
	data = g_newa(guint8, len);
	cursor = data;

	if (qq_crypt(DECRYPT, buf, buf_len, qd->session_key, data, &len)) {
		s = g_new0(qq_buddy_status, 1);
		bytes = 0;
		/* 000-030: qq_buddy_status */
		bytes += qq_buddy_status_read(data, &cursor, len, s);
		/* 031-034: my uid */ 
		/* This has a value of 0 when we've changed our status to 
		 * QQ_BUDDY_ONLINE_INVISIBLE */
		bytes += read_packet_dw(data, &cursor, len, &my_uid);

		if (bytes != 35) {
			gaim_debug(GAIM_DEBUG_ERROR, "QQ", "bytes(%d) != 35\n", bytes);
			g_free(s->ip);
			g_free(s->unknown_key);
			g_free(s);
			return;
		}

		name = uid_to_gaim_name(s->uid);
		b = gaim_find_buddy(gc->account, name);
		g_free(name);
		q_bud = (b == NULL) ? NULL : (qq_buddy *) b->proto_data;
		if (q_bud) {
			gaim_debug(GAIM_DEBUG_INFO, "QQ", "s->uid = %d, q_bud->uid = %d\n", s->uid , q_bud->uid);
			if(0 != *((guint32 *)s->ip)) { 
				g_memmove(q_bud->ip, s->ip, 4);
				q_bud->port = s->port;
			}
			q_bud->status = s->status;
			if(0 != s->client_version) 
				q_bud->client_version = s->client_version; 
			qq_update_buddy_contact(gc, q_bud);
		} else {
			gaim_debug(GAIM_DEBUG_ERROR, "QQ", 
					"got information of unknown buddy %d\n", s->uid);
		}

		g_free(s->ip);
		g_free(s->unknown_key);
		g_free(s);
	} else {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Error decrypt buddy status change packet\n");
	}
}
