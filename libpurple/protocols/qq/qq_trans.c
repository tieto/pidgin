/**
 * @file qq_trans.c
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

#include "connection.h"
#include "debug.h"
#include "notify.h"
#include "prefs.h"
#include "request.h"

#include "qq_define.h"
#include "qq_network.h"
#include "qq_process.h"
#include "qq_trans.h"

enum {
	QQ_TRANS_IS_SERVER = 0x01,			/* Is server command or client command */
	QQ_TRANS_IS_IMPORT = 0x02,			/* Only notice if not get reply; or resend, disconn if reties get 0*/
	QQ_TRANS_REMAINED = 0x04,				/* server command before login*/
	QQ_TRANS_IS_REPLY = 0x08				/* server command before login*/
};

struct _qq_transaction {
	guint8 flag;
	guint16 seq;
	guint16 cmd;

	guint8 room_cmd;
	guint32 room_id;

	guint8 *data;
	gint data_len;

	gint fd;
	gint send_retries;
	gint rcved_times;
	gint scan_times;

	guint32 update_class;
	guint32 ship32;
};

gboolean qq_trans_is_server(qq_transaction *trans)
{
	g_return_val_if_fail(trans != NULL, FALSE);

	if (trans->flag & QQ_TRANS_IS_SERVER)
		return TRUE;
	else
		return FALSE;
}

gboolean qq_trans_is_dup(qq_transaction *trans)
{
	g_return_val_if_fail(trans != NULL, TRUE);

	if (trans->rcved_times > 1)
		return TRUE;
	else
		return FALSE;
}

guint8 qq_trans_get_room_cmd(qq_transaction *trans)
{
	g_return_val_if_fail(trans != NULL, 0);
	return trans->room_cmd;
}

guint32 qq_trans_get_room_id(qq_transaction *trans)
{
	g_return_val_if_fail(trans != NULL, 0);
	return trans->room_id;
}

guint32 qq_trans_get_class(qq_transaction *trans)
{
	g_return_val_if_fail(trans != NULL, QQ_CMD_CLASS_NONE);
	return trans->update_class;
}

guint32 qq_trans_get_ship(qq_transaction *trans)
{
	g_return_val_if_fail(trans != NULL, 0);
	return trans->ship32;
}

static qq_transaction *trans_create(PurpleConnection *gc, gint fd,
	guint16 cmd, guint16 seq, guint8 *data, gint data_len, guint32 update_class, guint32 ship32)
{
	qq_transaction *trans;

	g_return_val_if_fail(gc != NULL, NULL);

	trans = g_new0(qq_transaction, 1);

	memset(trans, 0, sizeof(qq_transaction));
	trans->fd = fd;
	trans->cmd = cmd;
	trans->seq = seq;

	trans->data = NULL;
	trans->data_len = 0;
	if (data != NULL && data_len > 0) {
		/* don't use g_strdup, may have 0x00 */
		trans->data = g_memdup(data, data_len);
		trans->data_len = data_len;
	}

	trans->update_class = update_class;
	trans->ship32 = ship32;
	return trans;
}

/* Remove a packet with seq from send trans */
static void trans_remove(PurpleConnection *gc, qq_transaction *trans)
{
	qq_data *qd;

	g_return_if_fail(gc != NULL);
	qd = (qq_data *) gc->proto_data;
	g_return_if_fail(qd != NULL);

	g_return_if_fail(trans != NULL);
#if 0
	purple_debug_info("QQ_TRANS",
				"Remove [%s%05d] retry %d rcved %d scan %d %s\n",
				(trans->flag & QQ_TRANS_IS_SERVER) ? "SRV-" : "",
				trans->seq,
				trans->send_retries, trans->rcved_times, trans->scan_times,
				qq_get_cmd_desc(trans->cmd));
#endif
	if (trans->data)	g_free(trans->data);
	qd->transactions = g_list_remove(qd->transactions, trans);
	g_free(trans);
}

static qq_transaction *trans_find(PurpleConnection *gc, guint16 cmd, guint16 seq)
{
	qq_data *qd;
	GList *list;
	qq_transaction *trans;

	g_return_val_if_fail(gc != NULL && gc->proto_data != NULL, NULL);
	qd = (qq_data *) gc->proto_data;

	list = qd->transactions;
	while (list != NULL) {
		trans = (qq_transaction *) list->data;
		if(trans->cmd == cmd && trans->seq == seq) {
			return trans;
		}
		list = list->next;
	}

	return NULL;
}

void qq_trans_add_client_cmd(PurpleConnection *gc,
	guint16 cmd, guint16 seq, guint8 *data, gint data_len, guint32 update_class, guint32 ship32)
{
	qq_data *qd = (qq_data *)gc->proto_data;
	qq_transaction *trans = trans_create(gc, qd->fd, cmd, seq, data, data_len, update_class, ship32);

	if (cmd == QQ_CMD_TOKEN || cmd == QQ_CMD_LOGIN || cmd == QQ_CMD_KEEP_ALIVE) {
		trans->flag |= QQ_TRANS_IS_IMPORT;
	}
	trans->send_retries = qd->resend_times;
#if 0
	purple_debug_info("QQ_TRANS", "Add client cmd, seq %d, data %p, len %d\n",
			trans->seq, trans->data, trans->data_len);
#endif
	qd->transactions = g_list_append(qd->transactions, trans);
}

qq_transaction *qq_trans_find_rcved(PurpleConnection *gc, guint16 cmd, guint16 seq)
{
	qq_transaction *trans;

	trans = trans_find(gc, cmd, seq);
	if (trans == NULL) {
		return NULL;
	}

	if (trans->rcved_times == 0) {
		trans->scan_times = 0;
	}
	trans->rcved_times++;
	/* server may not get our confirm reply before, send reply again*/
	if (qq_trans_is_server(trans) && (trans->flag & QQ_TRANS_IS_REPLY)) {
		if (trans->data != NULL && trans->data_len > 0) {
			qq_send_cmd_encrypted(gc, trans->cmd, trans->seq, trans->data, trans->data_len, FALSE);
		}
	}
	return trans;
}

void qq_trans_add_room_cmd(PurpleConnection *gc,
		guint16 seq, guint8 room_cmd, guint32 room_id, guint8 *data, gint data_len,
		guint32 update_class, guint32 ship32)
{
	qq_data *qd = (qq_data *)gc->proto_data;
	qq_transaction *trans = trans_create(gc, qd->fd, QQ_CMD_ROOM, seq, data, data_len,
			update_class, ship32);

	trans->room_cmd = room_cmd;
	trans->room_id = room_id;
	trans->send_retries = qd->resend_times;
#if 0
	purple_debug_info("QQ_TRANS", "Add room cmd, seq %d, data %p, len %d\n",
			trans->seq, trans->data, trans->data_len);
#endif
	qd->transactions = g_list_append(qd->transactions, trans);
}

void qq_trans_add_server_cmd(PurpleConnection *gc, guint16 cmd, guint16 seq,
		guint8 *rcved, gint rcved_len)
{
	qq_data *qd = (qq_data *)gc->proto_data;
	qq_transaction *trans = trans_create(gc, qd->fd, cmd, seq, rcved, rcved_len, QQ_CMD_CLASS_NONE, 0);

	trans->flag = QQ_TRANS_IS_SERVER;
	trans->send_retries = 0;
	trans->rcved_times = 1;
#if 0
	purple_debug_info("QQ_TRANS", "Add server cmd, seq %d, data %p, len %d\n",
			trans->seq, trans->data, trans->data_len);
#endif
	qd->transactions = g_list_append(qd->transactions, trans);
}

void qq_trans_add_server_reply(PurpleConnection *gc, guint16 cmd, guint16 seq,
		guint8 *reply, gint reply_len)
{
	qq_transaction *trans;

	g_return_if_fail(reply != NULL && reply_len > 0);

	trans = trans_find(gc, cmd, seq);
	if (trans == NULL) {
		return;
	}

	g_return_if_fail(trans->flag & QQ_TRANS_IS_SERVER);
	trans->flag |= QQ_TRANS_IS_REPLY;

	if (trans->data)	g_free(trans->data);

	trans->data = g_memdup(reply, reply_len);
	trans->data_len = reply_len;
}

void qq_trans_add_remain(PurpleConnection *gc, guint16 cmd, guint16 seq,
		guint8 *data, gint data_len)
{
	qq_data *qd = (qq_data *)gc->proto_data;
	qq_transaction *trans = trans_create(gc, qd->fd, cmd, seq, data, data_len, QQ_CMD_CLASS_NONE, 0);

	trans->flag = QQ_TRANS_IS_SERVER;
	trans->flag |= QQ_TRANS_REMAINED;
	trans->send_retries = 0;
	trans->rcved_times = 1;
#if 1
	purple_debug_info("QQ_TRANS", "Add server cmd and remained, seq %d, data %p, len %d\n",
			trans->seq, trans->data, trans->data_len);
#endif
	qd->transactions = g_list_append(qd->transactions, trans);
}

void qq_trans_process_remained(PurpleConnection *gc)
{
	qq_data *qd = (qq_data *)gc->proto_data;
	GList *curr;
	GList *next;
	qq_transaction *trans;

	g_return_if_fail(qd != NULL);

	next = qd->transactions;
	while( (curr = next) ) {
		next = curr->next;
		trans = (qq_transaction *) (curr->data);
#if 0
		purple_debug_info("QQ_TRANS", "Scan [%d]\n", trans->seq);
#endif
		if ( !(trans->flag & QQ_TRANS_IS_SERVER) ) {
			continue;
		}
		if ( !(trans->flag & QQ_TRANS_REMAINED) ) {
			continue;
		}
		/* set QQ_TRANS_REMAINED off */
		trans->flag &= ~QQ_TRANS_REMAINED;

#if 1
		purple_debug_info("QQ_TRANS",
				"Process server cmd remained, seq %d, data %p, len %d, send_retries %d\n",
				trans->seq, trans->data, trans->data_len, trans->send_retries);
#endif
		qq_proc_server_cmd(gc, trans->cmd, trans->seq, trans->data, trans->data_len);
	}

	/* purple_debug_info("QQ_TRANS", "Scan finished\n"); */
	return;
}

gboolean qq_trans_scan(PurpleConnection *gc)
{
	qq_data *qd = (qq_data *)gc->proto_data;
	GList *curr;
	GList *next;
	qq_transaction *trans;

	g_return_val_if_fail(qd != NULL, FALSE);

	next = qd->transactions;
	while( (curr = next) ) {
		next = curr->next;
		trans = (qq_transaction *) (curr->data);
		/* purple_debug_info("QQ_TRANS", "Scan [%d]\n", trans->seq); */

		if (trans->flag & QQ_TRANS_REMAINED) {
			/* keep server cmd before login*/
			continue;
		}

		trans->scan_times++;
		if (trans->scan_times <= 1) {
			/* skip in 10 seconds */
			continue;
		}

		if (trans->rcved_times > 0) {
			/* Has been received */
			trans_remove(gc, trans);
			continue;
		}

		if (trans->flag & QQ_TRANS_IS_SERVER) {
			continue;
		}

		/* Never get reply */
		trans->send_retries--;
		if (trans->send_retries <= 0) {
			purple_debug_warning("QQ_TRANS",
				"[%d] %s is lost.\n",
				trans->seq, qq_get_cmd_desc(trans->cmd));
			if (trans->flag & QQ_TRANS_IS_IMPORT) {
				return TRUE;
			}

			qd->net_stat.lost++;
			purple_debug_error("QQ_TRANS",
				"Lost [%d] %s, data %p, len %d, retries %d\n",
				trans->seq, qq_get_cmd_desc(trans->cmd),
				trans->data, trans->data_len, trans->send_retries);
			trans_remove(gc, trans);
			continue;
		}

		qd->net_stat.resend++;
		purple_debug_warning("QQ_TRANS",
				"Resend [%d] %s data %p, len %d, send_retries %d\n",
				trans->seq, qq_get_cmd_desc(trans->cmd),
				trans->data, trans->data_len, trans->send_retries);
		qq_send_cmd_encrypted(gc, trans->cmd, trans->seq, trans->data, trans->data_len, FALSE);
	}

	/* purple_debug_info("QQ_TRANS", "Scan finished\n"); */
	return FALSE;
}

/* clean up send trans and free all contents */
void qq_trans_remove_all(PurpleConnection *gc)
{
	qq_data *qd = (qq_data *)gc->proto_data;
	qq_transaction *trans;
	gint count = 0;

	while(qd->transactions != NULL) {
		trans = (qq_transaction *) (qd->transactions->data);
		qd->transactions = g_list_remove(qd->transactions, trans);

		if (trans->data)	g_free(trans->data);
		g_free(trans);

		count++;
	}
	if (count > 0) {
		purple_debug_info("QQ_TRANS", "Free all %d packets\n", count);
	}
}
