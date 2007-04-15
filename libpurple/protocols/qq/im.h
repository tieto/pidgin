/**
 * @file im.h
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

#ifndef _QQ_IM_H_
#define _QQ_IM_H_

#include <glib.h>
#include "connection.h"
#include "group.h"

#define QQ_MSG_IM_MAX               500	/* max length of IM */
#define QQ_SEND_IM_BEFORE_MSG_LEN   53
#define QQ_SEND_IM_AFTER_MSG_LEN    13	/* there is one 0x00 at the end */

enum {
	QQ_IM_TEXT = 0x01,
	QQ_IM_AUTO_REPLY = 0x02
};

enum {
	QQ_RECV_IM_TO_BUDDY = 0x0009,
	QQ_RECV_IM_TO_UNKNOWN = 0x000a,
	QQ_RECV_IM_UNKNOWN_QUN_IM = 0x0020,
	QQ_RECV_IM_ADD_TO_QUN = 0x0021,
	QQ_RECV_IM_DEL_FROM_QUN = 0x0022,
	QQ_RECV_IM_APPLY_ADD_TO_QUN = 0x0023,
	QQ_RECV_IM_APPROVE_APPLY_ADD_TO_QUN = 0x0024,
	QQ_RECV_IM_REJCT_APPLY_ADD_TO_QUN = 0x0025,
	QQ_RECV_IM_CREATE_QUN = 0x0026,
	QQ_RECV_IM_TEMP_QUN_IM = 0x002A,
	QQ_RECV_IM_QUN_IM = 0x002B,
	QQ_RECV_IM_SYS_NOTIFICATION = 0x0030
};

guint8 *qq_get_send_im_tail(const gchar *font_color,
			    const gchar *font_size,
			    const gchar *font_name,
			    gboolean is_bold, gboolean is_italic, gboolean is_underline, gint len);

void qq_send_packet_im(PurpleConnection *gc, guint32 to_uid, gchar *msg, gint type);
void qq_process_recv_im(guint8 *buf, gint buf_len, guint16 seq, PurpleConnection *gc);
void qq_process_send_im_reply(guint8 *buf, gint buf_len, PurpleConnection *gc);

#endif
