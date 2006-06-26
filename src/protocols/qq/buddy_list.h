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
 */

// START OF FILE
/*****************************************************************************/
#ifndef _QQ_BUDDY_LIST_H_
#define _QQ_BUDDY_LIST_H_

#include <glib.h>
#include "connection.h"		// GaimConnection

#define QQ_FRIENDS_LIST_POSITION_START 		0x0000
#define QQ_FRIENDS_LIST_POSITION_END 		0xffff
#define QQ_FRIENDS_ONLINE_POSITION_START 	0x00
#define QQ_FRIENDS_ONLINE_POSITION_END 		0xff

void qq_send_packet_get_buddies_online(GaimConnection * gc, guint8 position);
void qq_process_get_buddies_online_reply(guint8 * buf, gint buf_len, GaimConnection * gc);
void qq_send_packet_get_buddies_list(GaimConnection * gc, guint16 position);
void qq_process_get_buddies_list_reply(guint8 * buf, gint buf_len, GaimConnection * gc);

//added by gfhuang
void qq_send_packet_get_all_list_with_group(GaimConnection * gc, guint32 position);
void qq_process_get_all_list_with_group_reply(guint8 * buf, gint buf_len, GaimConnection * gc);

#endif
/*****************************************************************************/
// END OF FILE
