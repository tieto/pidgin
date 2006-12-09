/**
 * @file keep_alive.c
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * OICQ encryption algorithm
 * Convert from ASM code provided by PerlOICQ
 * 
 * Puzzlebird, Nov-Dec 2002
 */

#include "debug.h"
#include "server.h"

#include "buddy_info.h"
#include "buddy_list.h"
#include "buddy_status.h"
#include "crypt.h"
#include "header_info.h"
#include "keep_alive.h"
#include "packet_parse.h"
#include "send_core.h"
#include "utils.h"

#define QQ_UPDATE_ONLINE_INTERVAL   300	/* in sec */
#define QQ_UPDATE_LEVELS_INTERVAL   600	/* in sec */

/* send keep-alive packet to QQ server (it is a heart-beat) */
void qq_send_packet_keep_alive(GaimConnection *gc)
{
	qq_data *qd;
	guint8 *raw_data, *cursor;

	qd = (qq_data *) gc->proto_data;
	raw_data = g_newa(guint8, 4);
	cursor = raw_data;

	/* In fact, we can send whatever we like to server
	 * with this command, server return the same result including
	 * the amount of online QQ users, my ip and port */
	create_packet_dw(raw_data, &cursor, qd->uid);

	qq_send_cmd(gc, QQ_CMD_KEEP_ALIVE, TRUE, 0, TRUE, raw_data, 4);
}

/* parse the return of keep-alive packet, it includes some system information */
void qq_process_keep_alive_reply(guint8 *buf, gint buf_len, GaimConnection *gc) 
{
	qq_data *qd;
	gint len;
	gchar **segments;
	guint8 *data;

	g_return_if_fail(buf != NULL && buf_len != 0);

	qd = (qq_data *) gc->proto_data;
	len = buf_len;
	data = g_newa(guint8, len);

	if (qq_crypt(DECRYPT, buf, buf_len, qd->session_key, data, &len)) {
		/* the last one is 60, don't know what it is */
		if (NULL == (segments = split_data(data, len, "\x1f", 6)))
			return;
		/* segments[0] and segment[1] are all 0x30 ("0") */
		qd->all_online = strtol(segments[2], NULL, 10);
		if(0 == qd->all_online)
			gaim_connection_error(gc, _("Keep alive error, seems connection lost!"));
		g_free(qd->my_ip);
		qd->my_ip = g_strdup(segments[3]);
		qd->my_port = strtol(segments[4], NULL, 10);
		g_strfreev(segments);
	} else
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Error decrypt keep alive reply\n");

	/* we refresh buddies's online status periodically */
	/* qd->last_get_online is updated when setting get_buddies_online packet */
	if ((time(NULL) - qd->last_get_online) >= QQ_UPDATE_ONLINE_INTERVAL)
		qq_send_packet_get_buddies_online(gc, QQ_FRIENDS_ONLINE_POSITION_START);
	if ((time(NULL) - qd->last_get_levels) >= QQ_UPDATE_LEVELS_INTERVAL)
		qq_send_packet_get_buddies_levels(gc);
}

/* refresh all buddies online/offline,
 * after receiving reply for get_buddies_online packet */
void qq_refresh_all_buddy_status(GaimConnection *gc)
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

/*TODO: maybe this should be qq_update_buddy_status() ?*/
void qq_update_buddy_contact(GaimConnection *gc, qq_buddy *q_bud)
{
	gchar *name;
	GaimBuddy *bud;
	gchar *status_id;
	
	g_return_if_fail(q_bud != NULL);

	name = uid_to_gaim_name(q_bud->uid);
	bud = gaim_find_buddy(gc->account, name);
	g_return_if_fail(bud != NULL);

	if (bud != NULL) {
		gaim_blist_server_alias_buddy(bud, q_bud->nickname); /* server */
		q_bud->last_refresh = time(NULL);

		/* gaim supports signon and idle time
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
			gaim_debug(GAIM_DEBUG_ERROR, "QQ", "unknown status: %x\n", q_bud->status);
			break;
		}
		gaim_debug(GAIM_DEBUG_INFO, "QQ", "set buddy %d to %s\n", q_bud->uid, status_id);
		gaim_prpl_got_user_status(gc->account, name, status_id, NULL);
	} else {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "unknown buddy: %d\n", q_bud->uid);
	}

	gaim_debug(GAIM_DEBUG_INFO, "QQ", "qq_update_buddy_contact, client=%04x\n", q_bud->client_version);
	g_free(name);
}
