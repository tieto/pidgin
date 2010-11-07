/**
 * @file qq_network.h
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

#ifndef _QQ_PROXY_H
#define _QQ_PROXY_H

#include <glib.h>
#include "connection.h"

#include "qq.h"

#define QQ_CONNECT_STEPS    3	/* steps in connection */

void qq_connect(PurpleAccount *account);
void qq_disconnect(PurpleConnection *gc);
void qq_connect_later(PurpleConnection *gc);

gint qq_send_data(qq_data *qd, guint16 cmd, guint8 *data, gint datalen);
gint qq_send_cmd(qq_data *qd, guint16 cmd, guint8 *data, gint datalen);
gint qq_send_cmd_detail(qq_data *qd, guint16 cmd, guint16 seq, gboolean need_ack,
	guint8 *data, gint data_len);

#endif
