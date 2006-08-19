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

#ifndef _QQ_SEND_QUEUE_H_
#define _QQ_SEND_QUEUE_H_

#include <glib.h>
#include "qq.h"

#define QQ_SENDQUEUE_TIMEOUT 			5000	/* in 1/1000 sec */

typedef struct _qq_sendpacket qq_sendpacket;

struct _qq_sendpacket {
	gint fd;
	gint len;
	guint8 *buf;
	guint16 cmd;
	guint16 send_seq;
	gint resend_times;
	time_t sendtime;
};

void qq_sendqueue_free(qq_data *qd);

void qq_sendqueue_remove(qq_data *qd, guint16 send_seq);
gboolean qq_sendqueue_timeout_callback(gpointer data);

#endif
