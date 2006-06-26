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
#ifndef _QQ_BUDDY_OPT_H_
#define _QQ_BUDDY_OPT_H_

#include <glib.h>
#include "connection.h"		// GaimConnection

#include "qq.h"			// qq_buddy

typedef struct _gc_and_uid gc_and_uid;

struct _gc_and_uid {
	guint32 uid;
	GaimConnection *gc;
};

void qq_approve_add_request_with_gc_and_uid(gc_and_uid * g);

void qq_reject_add_request_with_gc_and_uid(gc_and_uid * g);

void qq_add_buddy_with_gc_and_uid(gc_and_uid * g);

void qq_block_buddy_with_gc_and_uid(gc_and_uid * g);

void qq_do_nothing_with_gc_and_uid(gc_and_uid * g, const gchar * msg);

void qq_process_remove_buddy_reply(guint8 * buf, gint buf_len, GaimConnection * gc);
void qq_process_remove_self_reply(guint8 * buf, gint buf_len, GaimConnection * gc);
void qq_process_add_buddy_reply(guint8 * buf, gint buf_len, guint16 seq, GaimConnection * gc);
void qq_process_add_buddy_auth_reply(guint8 * buf, gint buf_len, GaimConnection * gc);
GaimBuddy *qq_add_buddy_by_recv_packet(GaimConnection * gc, guint32 uid, gboolean is_known, gboolean create);
void qq_add_buddy(GaimConnection * gc, GaimBuddy * buddy, GaimGroup * group);
GaimGroup *qq_get_gaim_group(const gchar * group_name);

void qq_remove_buddy(GaimConnection * gc, GaimBuddy * buddy, GaimGroup * group);
void qq_add_buddy_request_free(qq_data * qd);

void qq_buddies_list_free(GaimAccount *account, qq_data * qd);

#endif
/*****************************************************************************/
// END OF FILE
