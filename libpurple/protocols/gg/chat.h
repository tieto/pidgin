/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here. Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * Component written by Tomek Wasilczyk (http://www.wasilczyk.pl).
 *
 * This file is dual-licensed under the GPL2+ and the X11 (MIT) licences.
 * As a recipient of this file you may choose, which license to receive the
 * code under. As a contributor, you have to ensure the new code is
 * compatible with both.
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
#ifndef _GGP_CHAT_H
#define _GGP_CHAT_H

#include <internal.h>
#include <libgadu.h>

typedef struct _ggp_chat_session_data ggp_chat_session_data;

#include "gg.h"

void ggp_chat_setup(PurpleConnection *gc);
void ggp_chat_cleanup(PurpleConnection *gc);

#if GGP_ENABLE_GG11
void ggp_chat_got_event(PurpleConnection *gc, const struct gg_event *ev);

GList * ggp_chat_info(PurpleConnection *gc);
GHashTable * ggp_chat_info_defaults(PurpleConnection *gc,
	const char *chat_name);
char * ggp_chat_get_name(GHashTable *components);
void ggp_chat_join(PurpleConnection *gc, GHashTable *components);
void ggp_chat_leave(PurpleConnection *gc, int local_id);
void ggp_chat_invite(PurpleConnection *gc, int local_id, const char *message,
	const char *who);
int ggp_chat_send(PurpleConnection *gc, int local_id, PurpleMessage *msg);

void ggp_chat_got_message(PurpleConnection *gc, uint64_t chat_id,
	const char *message, time_t time, uin_t who);

PurpleRoomlist * ggp_chat_roomlist_get_list(PurpleConnection *gc);
#endif

#endif /* _GGP_CHAT_H */
