/**
 * @file send_file.h
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _QQ_QQ_SEND_FILE_H_
#define _QQ_QQ_SEND_FILE_H_

#include "ft.h"

typedef struct _ft_info {
	guint32 to_uid;
	guint16 send_seq;
	guint8 file_session_key[16];
	guint8 conn_method;
	guint32 remote_internet_ip;
	guint16 remote_internet_port;
	guint16 remote_major_port;
	guint32 remote_real_ip;
	guint16 remote_minor_port;
	guint32 local_internet_ip;
	guint16 local_internet_port;
	guint16 local_major_port;
	guint32 local_real_ip;
	guint16 local_minor_port;
	/* we use these to control the packets sent or received */
	guint32 fragment_num;
	guint32 fragment_len;
	/* The max index of sending/receiving fragment
	 * for sender, it is the lower bolder of a slide window for sending
	 * for receiver, it seems that packets having a fragment index lower
	 * than max_fragment_index have been received already
	 */
	guint32 max_fragment_index;
	guint32 window;

	/* It seems that using xfer's function is not enough for our
	 * transfer module. So I will use our own structure instead
	 * of xfer provided
	 */
	int major_fd;
	int minor_fd;
	int sender_fd;
	int recv_fd;
	FILE *dest_fp;
	/* guint8 *buffer; */
	gboolean use_major;
} ft_info;

void qq_process_recv_file_accept(guint8 *data, guint8 **cursor, gint data_len, 
		guint32 sender_uid, PurpleConnection *gc);
void qq_process_recv_file_reject(guint8 *data, guint8 **cursor, gint data_len, 
		guint32 sender_uid, PurpleConnection *gc);
void qq_process_recv_file_cancel(guint8 *data, guint8 **cursor, gint data_len, 
		guint32 sender_uid, PurpleConnection *gc);
void qq_process_recv_file_request(guint8 *data, guint8 **cursor, gint data_len, 
		guint32 sender_uid, PurpleConnection *gc);
void qq_process_recv_file_notify(guint8 *data, guint8 **cursor, gint data_len, 
		guint32 sender_uid, PurpleConnection *gc);
gboolean qq_can_receive_file(PurpleConnection *gc, const char *who);
void qq_send_file(PurpleConnection *gc, const char *who, const char *file);
void qq_get_conn_info(guint8 *data, guint8 **cursor, gint data_len, ft_info *info);
gint qq_fill_conn_info(guint8 *data, guint8 **cursor, ft_info *info);
gssize _qq_xfer_write(const guint8 *buf, size_t len, PurpleXfer *xfer);

#endif
