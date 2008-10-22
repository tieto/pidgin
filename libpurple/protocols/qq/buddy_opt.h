/**
 * @file buddy_opt.h
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

#ifndef _QQ_BUDDY_OPT_H_
#define _QQ_BUDDY_OPT_H_

#include <glib.h>
#include "connection.h"

#include "qq.h"

void qq_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group);
qq_buddy *qq_get_buddy(PurpleConnection *gc, guint32 uid);
void qq_change_buddys_group(PurpleConnection *gc, const char *who,
		const char *old_group, const char *new_group);
void qq_remove_buddy_and_me(PurpleBlistNode * node);
void qq_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group);
PurpleBuddy *qq_create_buddy(PurpleConnection *gc, guint32 uid, gboolean is_known, gboolean create);
void qq_buddies_list_free(PurpleAccount *account, qq_data *qd);

void qq_process_buddy_remove(guint8 *buf, gint buf_len, PurpleConnection *gc);
void qq_process_buddy_remove_me(guint8 *data, gint data_len, PurpleConnection *gc);
void qq_process_buddy_add_no_auth(guint8 *data, gint data_len, guint32 uid, PurpleConnection *gc);
void qq_process_buddy_add_auth(guint8 *data, gint data_len, PurpleConnection *gc);
void qq_process_buddy_from_server(PurpleConnection *gc, int funct,
		gchar *from, gchar *to, gchar *msg_utf8);

PurpleGroup *qq_create_group(const gchar *group_name);

#endif
