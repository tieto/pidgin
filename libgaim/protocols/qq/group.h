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

#ifndef _QQ_GROUP_H_
#define _QQ_GROUP_H_

#include <glib.h>
#include "account.h"
#include "connection.h"
#include "roomlist.h"
#include "qq.h"

#define GAIM_GROUP_QQ_QUN         "QQ 群"

typedef enum {
	QQ_GROUP_MEMBER_STATUS_NOT_MEMBER = 0x00,	/* default 0x00 means not member */
	QQ_GROUP_MEMBER_STATUS_IS_MEMBER,
	QQ_GROUP_MEMBER_STATUS_APPLYING,
	QQ_GROUP_MEMBER_STATUS_IS_ADMIN,
} qq_group_member_status;

typedef struct _qq_group {
	/* all these will be saved when we exit Gaim */
	qq_group_member_status my_status;	/* my status for this group */
	gchar *my_status_desc;			/* my status description */
	guint32 internal_group_id;
	guint32 external_group_id;
	guint8 group_type;			/* permanent or temporory */
	guint32 creator_uid;
	guint32 group_category;
	guint8 auth_type;
	gchar *group_name_utf8;
	gchar *group_desc_utf8;
	/* all these will be loaded from the network */
	gchar *notice_utf8;	/* group notice by admin */
	GList *members;	
} qq_group;

GList *qq_chat_info(GaimConnection *gc);
GHashTable *qq_chat_info_defaults(GaimConnection *gc, const gchar *chat_name);

void qq_group_init(GaimConnection *gc);

GaimRoomlist *qq_roomlist_get_list(GaimConnection *gc);

void qq_roomlist_cancel(GaimRoomlist *list);

#endif
