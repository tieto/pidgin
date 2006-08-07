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

#ifndef _QQ_SEND_CORE_H_
#define _QQ_SEND_CORE_H_

#include <glib.h>
#include "connection.h"


#define	QQ_CLIENT	0x0E1B
/* #define	QQ_CLIENT	0x0F3F */

gint qq_send_cmd(GaimConnection *gc, guint16 cmd, gboolean is_auto_seq, guint16 seq, 
		gboolean need_ack, guint8 *data, gint len);
gint _qq_send_packet(GaimConnection * gc, guint8 *buf, gint len, guint16 cmd);

#endif
