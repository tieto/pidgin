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
#include "qq_trans.h"

#define QQ_RESEND_MAX               8	/* max resend per packet */

typedef struct _transaction {
	guint16 seq;
	guint16 cmd;
	guint8 *buf;
	gint buf_len;

	gint fd;
	gint retries;
	time_t create_time;
} transaction;

void qq_send_trans_append(qq_data *qd, guint8 *buf, gint buf_len, guint16 cmd, guint16 seq)
{
	transaction *trans = g_new0(transaction, 1);

	g_return_if_fail(trans != NULL);

	trans->fd = qd->fd;
	trans->cmd = cmd;
	trans->seq = seq;
	trans->retries = QQ_RESEND_MAX;
	trans->create_time = time(NULL);
	trans->buf = g_memdup(buf, buf_len);	/* don't use g_strdup, may have 0x00 */
	trans->buf_len = buf_len;

	purple_debug(PURPLE_DEBUG_ERROR, "QQ",
			"Add to transaction, seq = %d, buf = %p, len = %d\n",
			trans->seq, trans->buf, trans->buf_len);
	qd->send_trans = g_list_append(qd->send_trans, trans);
}

/* Remove a packet with seq from send trans */
void qq_send_trans_remove(qq_data *qd, gpointer data) 
{
	transaction *trans = (transaction *)data;

	g_return_if_fail(qd != NULL && data != NULL);
	
	purple_debug(PURPLE_DEBUG_INFO, "QQ",
				"ack [%05d] %s, remove from send tranactions\n",
				trans->seq, qq_get_cmd_desc(trans->cmd));

	if (trans->buf)	g_free(trans->buf);
	qd->send_trans = g_list_remove(qd->send_trans, trans);
	g_free(trans);
}

gpointer qq_send_trans_find(qq_data *qd, guint16 seq)
{
	GList *curr;
	GList *next;
	transaction *trans;

	curr = qd->send_trans;
	while(curr) {
		next = curr->next;
		trans = (transaction *) (curr->data);
		if(trans->seq == seq) {
			return trans;
		}
		curr = next;
	}

	return NULL;
}

/* clean up send trans and free all contents */
void qq_send_trans_remove_all(qq_data *qd)
{
	GList *curr;
	GList *next;
	transaction *trans;
	gint count = 0;

	curr = qd->send_trans;
	while(curr) {
		next = curr->next;
		
		trans = (transaction *) (curr->data);
		/*
		purple_debug(PURPLE_DEBUG_ERROR, "QQ",
			"Remove to transaction, seq = %d, buf = %p, len = %d\n",
			trans->seq, trans->buf, trans->len);
		*/
		qq_send_trans_remove(qd, trans);

		count++;
		curr = next;
	}
	g_list_free(qd->send_trans);

	purple_debug(PURPLE_DEBUG_INFO, "QQ", "%d packets in send tranactions are freed!\n", count);
}

gint qq_send_trans_scan(qq_data *qd, gint *start,
	guint8 *buf, gint maxlen, guint16 *cmd, gint *retries)
{
	GList *curr;
	GList *next = NULL;
	transaction *trans;
	gint copylen;

	g_return_val_if_fail(qd != NULL && *start >= 0 && maxlen > 0, -1);
	
	/* purple_debug(PURPLE_DEBUG_ERROR, "QQ", "Scan from %d\n", *start); */
	curr = g_list_nth(qd->send_trans, *start);
	while(curr) {
		next = curr->next;
		*start = g_list_position(qd->send_trans, next);
		
		trans = (transaction *) (curr->data);
		if (trans->buf == NULL || trans->buf_len <= 0) {
			qq_send_trans_remove(qd, trans);
			curr = next;
			continue;
		}

		if (trans->retries < 0) {
			purple_debug(PURPLE_DEBUG_ERROR, "QQ",
				"Remove transaction, seq %d, buf %p, len %d, retries %d, next %d\n",
				trans->seq, trans->buf, trans->buf_len, trans->retries, *start);
			qq_send_trans_remove(qd, trans);
			curr = next;
			continue;
		}

		purple_debug(PURPLE_DEBUG_ERROR, "QQ",
				"Resend transaction, seq %d, buf %p, len %d, retries %d, next %d\n",
				trans->seq, trans->buf, trans->buf_len, trans->retries, *start);
		copylen = MIN(trans->buf_len, maxlen);
		g_memmove(buf, trans->buf, copylen);

		*cmd = trans->cmd;
		*retries = trans->retries;
		trans->retries--;
		return copylen;
	}

	/* purple_debug(PURPLE_DEBUG_INFO, "QQ", "Scan finished\n"); */
	return -1;
}

void qq_rcv_trans_push(qq_data *qd, guint16 cmd, guint16 seq, guint8 *data, gint data_len)
{
	transaction *trans = g_new0(transaction, 1);

	g_return_if_fail(data != NULL && data_len > 0);
	g_return_if_fail(trans != NULL);

	trans->cmd = cmd;
	trans->seq = seq;
	trans->buf = g_memdup(data, data_len);
	trans->buf_len = data_len;
	trans->create_time = time(NULL);

	if (qd->rcv_trans == NULL)
		qd->rcv_trans = g_queue_new();

	g_queue_push_head(qd->rcv_trans, trans);
}

gint qq_rcv_trans_pop(qq_data *qd, guint16 *cmd, guint16 *seq, guint8 *data, gint max_len)
{
	transaction *trans = NULL;
	gint copy_len;

	g_return_val_if_fail(data != NULL && max_len > 0, -1);

	if (g_queue_is_empty(qd->rcv_trans)) {
		return -1;
	}
	trans = (transaction *) g_queue_pop_head(qd->rcv_trans);
	if (trans == NULL) {
		return 0;
	}
	if (trans->buf == NULL || trans->buf_len <= 0) {
		return 0;
	}

	copy_len = MIN(max_len, trans->buf_len);
	g_memmove(data, trans->buf, copy_len);
	*cmd = trans->cmd;
	*seq = trans->seq;

	g_free(trans->buf);
	g_free(trans);
	return copy_len;
}

/* clean up the packets before login */
void qq_rcv_trans_remove_all(qq_data *qd)
{
	transaction *trans = NULL;
	gint count = 0;

	g_return_if_fail(qd != NULL);

	/* now clean up my own data structures */
	if (qd->rcv_trans != NULL) {
		while (NULL != (trans = g_queue_pop_tail(qd->rcv_trans))) {
			g_free(trans->buf);
			g_free(trans);
			count++;
		}
		g_queue_free(qd->rcv_trans);
	}
	purple_debug(PURPLE_DEBUG_INFO, "QQ", "%d packets in receive tranactions are freed!\n", count);
}
