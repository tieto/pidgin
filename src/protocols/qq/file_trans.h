/**
 * The QQ2003C protocol plugin
 *
 * for gaim
 *
 *	Author: Henry Ou <henry@linux.net>
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

#ifndef _QQ_QQ_FILE_TRANS_H_
#define _QQ_QQ_FILE_TRANS_H_

#include "server.h"

enum {
	QQ_FILE_CMD_SENDER_SAY_HELLO = 0x31,
	QQ_FILE_CMD_SENDER_SAY_HELLO_ACK = 0x32,
	QQ_FILE_CMD_RECEIVER_SAY_HELLO = 0x33,
	QQ_FILE_CMD_RECEIVER_SAY_HELLO_ACK = 0x34,
	QQ_FILE_CMD_NOTIFY_IP_ACK = 0x3c,
	QQ_FILE_CMD_PING = 0x3d,
	QQ_FILE_CMD_PONG = 0x3e,
	QQ_FILE_CMD_INITATIVE_CONNECT = 0x40
};

enum {
	QQ_FILE_TRANSFER_FILE = 0x65,
	QQ_FILE_TRANSFER_FACE = 0x6b
};

enum {
	QQ_FILE_BASIC_INFO = 0x01,
	QQ_FILE_DATA_INFO = 0x02,
	QQ_FILE_EOF = 0x03,
	QQ_FILE_CMD_FILE_OP = 0x07,
	QQ_FILE_CMD_FILE_OP_ACK = 0x08
};

#define QQ_FILE_FRAGMENT_MAXLEN 1000

#define QQ_FILE_CONTROL_PACKET_TAG 0x00
/* #define QQ_PACKET_TAG           0x02 */ /* all QQ text packets starts with it */
#define QQ_FILE_DATA_PACKET_TAG 0x03
#define QQ_FILE_AGENT_PACKET_TAG 0x04
/* #define QQ_PACKET_TAIL          0x03 */   /* all QQ text packets end with it */

void qq_send_file_ctl_packet(GaimConnection *gc, guint16 packet_type, guint32 to_uid, guint8 hellobyte);
void qq_process_recv_file(GaimConnection *gc, guint8 *data, gint len);
/* void qq_send_file_data_packet(GaimConnection *gc, guint16 packet_type, guint8 sub_type, guint32 fragment_index, guint16 seq, guint8 *data, gint len); */
void qq_xfer_close_file(GaimXfer *xfer);
#endif
