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

#include "header_info.h"
#include "qq_network.h"
#include "qq_process.h"
#include "qq_trans.h"

#define QQ_RESEND_MAX               3	/* max resend per packet */

qq_transaction *qq_trans_find_rcved(qq_data *qd, guint16 cmd, guint16 seq)
{
	GList *curr;
	GList *next;
	qq_transaction *trans;

	if (qd->transactions == NULL) {
		return NULL;
	}

	next = qd->transactions;
	while( (curr = next) ) {
		next = curr->next;
		
		trans = (qq_transaction *) (curr->data);
		if(trans->cmd == cmd && trans->seq == seq) {
			if (trans->rcved_times == 0) {
				trans->scan_times = 0;
			}
			trans->rcved_times++;
			if (qq_trans_is_server(trans) && qq_trans_is_dup(trans)) {
				/* server may not get our confirm reply before, send reply again*/
				if (trans->data != NULL && trans->data_len > 0) {
					qq_send_data(qd, trans->cmd, trans->seq, FALSE, trans->data, trans->data_len);
				}
			}
			return trans;
		}
	}

	return NULL;
}

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

/* Remove a packet with seq from send trans */
static void trans_remove(qq_data *qd, qq_transaction *trans) 
{
	g_return_if_fail(qd != NULL && trans != NULL);
	
	purple_debug(PURPLE_DEBUG_INFO, "QQ_TRANS",
				"Remove [%s%05d] retry %d rcved %d scan %d %s\n",
				(trans->flag & QQ_TRANS_IS_SERVER) ? "SRV-" : "",
				trans->seq,
				trans->send_retries, trans->rcved_times, trans->scan_times,
				qq_get_cmd_desc(trans->cmd));

	if (trans->data)	g_free(trans->data);
	qd->transactions = g_list_remove(qd->transactions, trans);
	g_free(trans);
}

void qq_trans_add_client_cmd(qq_data *qd, guint16 cmd, guint16 seq, guint8 *data, gint data_len)
{
	qq_transaction *trans = g_new0(qq_transaction, 1);

	g_return_if_fail(trans != NULL);

	trans->flag = 0;
	if (cmd == QQ_CMD_TOKEN || cmd == QQ_CMD_LOGIN || cmd == QQ_CMD_KEEP_ALIVE) {
		trans->flag |= QQ_TRANS_CLI_IMPORT;
	}
	trans->fd = qd->fd;
	trans->cmd = cmd;
	trans->seq = seq;
	trans->room_cmd = 0;
	trans->room_id = 0;
	trans->send_retries = QQ_RESEND_MAX;
	trans->rcved_times = 0;
	trans->scan_times = 0;

	trans->data = NULL;
	trans->data_len = 0;
	if (data != NULL && data_len > 0) {
		trans->data = g_memdup(data, data_len);	/* don't use g_strdup, may have 0x00 */
		trans->data_len = data_len;
	}
	purple_debug(PURPLE_DEBUG_INFO, "QQ_TRANS",
			"Add client cmd, seq = %d, data = %p, len = %d\n",
			trans->seq, trans->data, trans->data_len);
	qd->transactions = g_list_append(qd->transactions, trans);
}

void qq_trans_add_room_cmd(qq_data *qd, guint16 seq, guint8 room_cmd, guint32 room_id,
		guint8 *data, gint data_len)
{
	qq_transaction *trans = g_new0(qq_transaction, 1);

	g_return_if_fail(trans != NULL);

	trans->flag = 0;
	trans->fd = qd->fd;
	trans->seq = seq;
	trans->cmd = QQ_CMD_ROOM;
	trans->room_cmd = room_cmd;
	trans->room_id = room_id;
	trans->send_retries = QQ_RESEND_MAX;
	trans->rcved_times = 0;
	trans->scan_times = 0;

	trans->data = NULL;
	trans->data_len = 0;
	if (data != NULL && data_len > 0) {
		trans->data = g_memdup(data, data_len);	/* don't use g_strdup, may have 0x00 */
		trans->data_len = data_len;
	}
	purple_debug(PURPLE_DEBUG_INFO, "QQ_TRANS",
			"Add room cmd, seq = %d, data = %p, len = %d\n",
			trans->seq, trans->data, trans->data_len);
	qd->transactions = g_list_append(qd->transactions, trans);
}

void qq_trans_add_server_cmd(qq_data *qd, guint16 cmd, guint16 seq, guint8 *data, gint data_len)
{
	qq_transaction *trans = g_new0(qq_transaction, 1);

	g_return_if_fail(trans != NULL);

	trans->flag = QQ_TRANS_IS_SERVER;
	if ( !qd->logged_in ) {
		trans->flag |= QQ_TRANS_BEFORE_LOGIN;
	}
	trans->fd = qd->fd;
	trans->cmd = cmd;
	trans->seq = seq;
	trans->room_cmd = 0;
	trans->room_id = 0;
	trans->send_retries = 0;
	trans->rcved_times = 1;
	trans->scan_times = 0;
	trans->data = NULL;
	trans->data_len = 0;
	if (data != NULL && data_len > 0) {
		trans->data = g_memdup(data, data_len);	/* don't use g_strdup, may have 0x00 */
		trans->data_len = data_len;
	}
	purple_debug(PURPLE_DEBUG_INFO, "QQ_TRANS",
			"Add server cmd, seq = %d, data = %p, len = %d\n",
			trans->seq, trans->data, trans->data_len);
	qd->transactions = g_list_append(qd->transactions, trans);
}

void qq_trans_process_before_login(qq_data *qd)
{
	GList *curr;
	GList *next;
	qq_transaction *trans;

	g_return_if_fail(qd != NULL);

	next = qd->transactions;
	while( (curr = next) ) {
		next = curr->next;
		trans = (qq_transaction *) (curr->data);
		/* purple_debug(PURPLE_DEBUG_ERROR, "QQ_TRANS", "Scan [%d]\n", trans->seq); */
		
		if ( !(trans->flag & QQ_TRANS_IS_SERVER) ) {
			continue;
		}
		if ( !(trans->flag & QQ_TRANS_BEFORE_LOGIN) ) {
			continue;
		}
		// set QQ_TRANS_BEFORE_LOGIN off
		trans->flag &= ~QQ_TRANS_BEFORE_LOGIN;

		purple_debug(PURPLE_DEBUG_ERROR, "QQ_TRANS",
				"Process server cmd before login, seq %d, data %p, len %d, send_retries %d\n",
				trans->seq, trans->data, trans->data_len, trans->send_retries);

		qq_proc_cmd_reply(qd->gc, trans->seq, trans->cmd, trans->data, trans->data_len);
	}

	/* purple_debug(PURPLE_DEBUG_INFO, "QQ_TRANS", "Scan finished\n"); */
	return;
}

gboolean qq_trans_scan(qq_data *qd)
{
	GList *curr;
	GList *next;
	qq_transaction *trans;

	g_return_val_if_fail(qd != NULL, FALSE);
	
	next = qd->transactions;
	while( (curr = next) ) {
		next = curr->next;
		trans = (qq_transaction *) (curr->data);
		/* purple_debug(PURPLE_DEBUG_INFO, "QQ_TRANS", "Scan [%d]\n", trans->seq); */
		
		if (trans->flag & QQ_TRANS_BEFORE_LOGIN) {
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
			trans_remove(qd, trans);
			continue;
		}

		if (trans->flag & QQ_TRANS_IS_SERVER) {
			continue;
		}
		
		/* Never get reply */
		trans->send_retries--;
		if (trans->send_retries <= 0) {
			purple_debug(PURPLE_DEBUG_WARNING, "QQ_TRANS",
				"[%d] %s is lost.\n",
				trans->seq, qq_get_cmd_desc(trans->cmd));
			if (trans->flag & QQ_TRANS_CLI_IMPORT) {
				return TRUE;
			}

			purple_debug(PURPLE_DEBUG_ERROR, "QQ_TRANS",
				"Lost [%d] %s, data %p, len %d, retries %d\n",
				trans->seq, qq_get_cmd_desc(trans->cmd),
				trans->data, trans->data_len, trans->send_retries);
			trans_remove(qd, trans);
			continue;
		}

		purple_debug(PURPLE_DEBUG_ERROR, "QQ_TRANS",
				"Resend [%d] %s data %p, len %d, send_retries %d\n",
				trans->seq, qq_get_cmd_desc(trans->cmd),
				trans->data, trans->data_len, trans->send_retries);
		qq_send_data(qd, trans->cmd, trans->seq, FALSE, trans->data, trans->data_len);
	}

	/* purple_debug(PURPLE_DEBUG_INFO, "QQ_TRANS", "Scan finished\n"); */
	return FALSE;
}

/* clean up send trans and free all contents */
void qq_trans_remove_all(qq_data *qd)
{
	GList *curr;
	GList *next;
	qq_transaction *trans;
	gint count = 0;

	curr = qd->transactions;
	while(curr) {
		next = curr->next;
		
		trans = (qq_transaction *) (curr->data);
		/*
		purple_debug(PURPLE_DEBUG_ERROR, "QQ_TRANS",
			"Remove to transaction, seq = %d, buf = %p, len = %d\n",
			trans->seq, trans->buf, trans->len);
		*/
		trans_remove(qd, trans);

		count++;
		curr = next;
	}
	g_list_free(qd->transactions);

	purple_debug(PURPLE_DEBUG_INFO, "QQ_TRANS", "Free all %d packets\n", count);
}
