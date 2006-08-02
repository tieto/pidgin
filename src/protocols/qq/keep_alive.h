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
 *
 */

#ifndef _QQ_KEEP_ALIVE_H_
#define _QQ_KEEP_ALIVE_H_

#include <glib.h>
#include "connection.h"
#include "qq.h"

void qq_send_packet_keep_alive(GaimConnection *gc);

void qq_process_keep_alive_reply(guint8 *buf, gint buf_len, GaimConnection *gc);
void qq_refresh_all_buddy_status(GaimConnection *gc);

void qq_update_buddy_contact(GaimConnection *gc, qq_buddy *q_bud);

#endif
