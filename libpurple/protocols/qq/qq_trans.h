/**
 * @file qq_trans.h
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

#ifndef _QQ_SEND_QUEUE_H_
#define _QQ_SEND_QUEUE_H_

#include <glib.h>
#include "qq.h"

enum {
	QQ_TRANS_IS_SERVER = 0x01,			/* Is server command or client command */
	/* prefix QQ_TRANS_CLI is for client command*/
	QQ_TRANS_CLI_EMERGE = 0x02,		/* send at once; or may wait for next reply*/
	QQ_TRANS_CLI_IMPORT = 0x04,		/* Only notice if not get reply; or resend, disconn if reties get 0*/
	QQ_TRANS_BEFORE_LOGIN = 0x08,	/* server command before login*/
};

typedef struct _qq_transaction {
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
} qq_transaction;

qq_transaction *qq_trans_find_rcved(qq_data *qd, guint16 cmd, guint16 seq);
gboolean qq_trans_is_server(qq_transaction *trans) ;
gboolean qq_trans_is_dup(qq_transaction *trans);
guint8 qq_trans_get_room_cmd(qq_transaction *trans);
guint32 qq_trans_get_room_id(qq_transaction *trans);

void qq_trans_add_client_cmd(qq_data *qd, guint16 cmd, guint16 seq, guint8 *data, gint data_len);
void qq_trans_add_server_cmd(qq_data *qd, guint16 cmd, guint16 seq, guint8 *data, gint data_len);
void qq_trans_add_room_cmd(qq_data *qd, guint16 seq, guint8 room_cmd, guint32 room_id,
	guint8 *data, gint data_len);

void qq_trans_process_before_login(qq_data *qd);
gboolean qq_trans_scan(qq_data *qd);
void qq_trans_remove_all(qq_data *qd);

#endif
