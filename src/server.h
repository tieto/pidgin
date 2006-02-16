/**
 * @file server.h Server API
 * @ingroup core
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
#ifndef _GAIM_SERVER_H_
#define _GAIM_SERVER_H_

#include "account.h"
#include "conversation.h"
#include "prpl.h"

#ifdef __cplusplus
extern "C" {
#endif

int  serv_send_im(GaimConnection *, const char *, const char *, GaimMessageFlags flags);
void serv_get_info(GaimConnection *, const char *);
void serv_set_info(GaimConnection *, const char *);
int  serv_send_typing(GaimConnection *, const char *, int);
void serv_move_buddy(GaimBuddy *, GaimGroup *, GaimGroup *);
void serv_add_permit(GaimConnection *, const char *);
void serv_add_deny(GaimConnection *, const char *);
void serv_rem_permit(GaimConnection *, const char *);
void serv_rem_deny(GaimConnection *, const char *);
void serv_set_permit_deny(GaimConnection *);
void serv_join_chat(GaimConnection *, GHashTable *);
void serv_reject_chat(GaimConnection *, GHashTable *);
void serv_chat_invite(GaimConnection *, int, const char *, const char *);
void serv_chat_leave(GaimConnection *, int);
void serv_chat_whisper(GaimConnection *, int, const char *, const char *);
int  serv_chat_send(GaimConnection *, int, const char *, GaimMessageFlags flags);
void serv_alias_buddy(GaimBuddy *);
void serv_got_alias(GaimConnection *gc, const char *who, const char *alias);
void serv_got_typing(GaimConnection *gc, const char *name, int timeout,
					 GaimTypingState state);
void serv_set_buddyicon(GaimConnection *gc, const char *filename);
void serv_got_typing_stopped(GaimConnection *gc, const char *name);
void serv_got_im(GaimConnection *gc, const char *who, const char *msg,
				 GaimMessageFlags flags, time_t mtime);
void serv_got_chat_invite(GaimConnection *gc, const char *name,
						  const char *who, const char *message,
						  GHashTable *data);
GaimConversation *serv_got_joined_chat(GaimConnection *gc,
									   int id, const char *name);
void serv_got_chat_left(GaimConnection *g, int id);
void serv_got_chat_in(GaimConnection *g, int id, const char *who,
					  GaimMessageFlags flags, const char *message, time_t mtime);
void serv_send_file(GaimConnection *gc, const char *who, const char *file);

#ifdef __cplusplus
}
#endif

#endif /* _GAIM_SERVER_H_ */
