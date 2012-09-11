/**
 * @file gg.h
 *
 * purple
 *
 * Copyright (C) 2005  Bartosz Oler <bartosz@bzimage.us>
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


#ifndef _PURPLE_GG_H
#define _PURPLE_GG_H

#include <libgadu.h>
#include "internal.h"
#include "search.h"
#include "connection.h"

#include "image.h"
#include "avatar.h"
#include "account.h"
#include "roster.h"
#include "multilogon.h"
#include "status.h"
#include "chat.h"

#define PUBDIR_RESULTS_MAX 20

#define GGP_UIN_LEN_MAX 10

typedef struct {
	struct gg_session *session;
	guint inpa;

	ggp_image_connection_data image_data;
	ggp_avatar_session_data avatar_data;
	ggp_roster_session_data roster_data;
	ggp_multilogon_session_data *multilogon_data;
	ggp_status_session_data *status_data;
	ggp_chat_session_data *chat_data;
} GGPInfo;

typedef struct
{
	gboolean blocked;
} ggp_buddy_data;

void ggp_recv_message_handler(PurpleConnection *gc, const struct gg_event_msg *ev, gboolean multilogon);

ggp_buddy_data * ggp_buddy_get_data(PurpleBuddy *buddy);

#endif /* _PURPLE_GG_H */
