/**
 * @file group_opt.h
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

#ifndef _QQ_GROUP_OPT_H_
#define _QQ_GROUP_OPT_H_

#include <glib.h>
#include "connection.h"
#include "group.h"

#define QQ_QUN_MEMBER_MAX       80	/* max number of the group */

typedef struct _group_member_opt {
	PurpleConnection *gc;
	guint32 internal_group_id;
	guint32 member;
} group_member_opt;

enum {
	QQ_GROUP_TYPE_PERMANENT = 0x01,
	QQ_GROUP_TYPE_TEMPORARY
};

enum {
	QQ_GROUP_MEMBER_ADD = 0x01,
	QQ_GROUP_MEMBER_DEL
};

void qq_group_modify_members(PurpleConnection *gc, qq_group *group, guint32 *new_members);
void qq_group_modify_info(PurpleConnection *gc, qq_group *group);

void qq_group_approve_application_with_struct(group_member_opt *g);
void qq_group_reject_application_with_struct(group_member_opt *g);
void qq_group_search_application_with_struct(group_member_opt *g);

void qq_group_process_modify_info_reply(guint8 *data, guint8 **cursor, gint len, PurpleConnection *gc);
void qq_group_process_modify_members_reply(guint8 *data, guint8 **cursor, gint len, PurpleConnection *gc);
void qq_group_manage_group(PurpleConnection *gc, GHashTable *data);
void qq_group_create_with_name(PurpleConnection *gc, const gchar *name);
void qq_group_activate_group(PurpleConnection *gc, guint32 internal_group_id);
void qq_group_process_activate_group_reply(guint8 *data, guint8 **cursor, gint len, PurpleConnection *gc);
void qq_group_process_create_group_reply(guint8 *data, guint8 **cursor, gint len, PurpleConnection *gc);

#endif
