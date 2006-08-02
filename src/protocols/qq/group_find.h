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

#ifndef _QQ_GROUP_FIND_H_
#define _QQ_GROUP_FIND_H_

#include <glib.h>
#include "connection.h"
#include "group.h"

gchar *qq_group_find_member_by_channel_and_nickname(GaimConnection *gc, gint channel, const gchar *who);
qq_buddy *qq_group_find_member_by_uid(qq_group *group, guint32 uid);
void qq_group_remove_member_by_uid(qq_group *group, guint32 uid);
qq_buddy *qq_group_find_or_add_member(GaimConnection *gc, qq_group *group, guint32 member_uid);
gboolean qq_group_find_internal_group_id_by_seq(GaimConnection *gc, guint16 seq, guint32 *internal_group_id);
qq_group *qq_group_find_by_channel(GaimConnection *gc, gint channel);
qq_group *qq_group_find_by_internal_group_id(GaimConnection *gc, guint32 internal_group_id);

#endif
