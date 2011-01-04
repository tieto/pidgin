/**
 * @file buddy_memo.h
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

#ifndef _QQ_BUDDY_MEMO_H_
#define _QQ_BUDDY_MEMO_H_

#include <glib.h>
#include "connection.h"
#include "blist.h"

#define QQ_BUDDY_MEMO_REQUEST_SUCCESS 0x00

/* clan command for memo */
enum
{
	QQ_BUDDY_MEMO_MODIFY = 0x01,	/* upload memo */
	QQ_BUDDY_MEMO_REMOVE,		/* remove memo */
	QQ_BUDDY_MEMO_GET		/* get memo */
};


void qq_process_get_buddy_memo(PurpleConnection *gc, guint8* data, gint data_len, guint32 update_class, guint32 action);

void qq_request_buddy_memo(PurpleConnection *gc, guint32 bd_uid, guint32 update_class, guint32 action);

#endif

