/**
 * @file yahoochat.h The Yahoo! protocol plugin, chat and conference stuff
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

#ifndef _YAHOOCHAT_H_
#define _YAHOOCHAT_H_

#include "roomlist.h"
#include "yahoo_packet.h"

void yahoo_process_conference_invite(GaimConnection *gc, struct yahoo_packet *pkt);
void yahoo_process_conference_decline(GaimConnection *gc, struct yahoo_packet *pkt);
void yahoo_process_conference_logon(GaimConnection *gc, struct yahoo_packet *pkt);
void yahoo_process_conference_logoff(GaimConnection *gc, struct yahoo_packet *pkt);
void yahoo_process_conference_message(GaimConnection *gc, struct yahoo_packet *pkt);

void yahoo_process_chat_online(GaimConnection *gc, struct yahoo_packet *pkt);
void yahoo_process_chat_logout(GaimConnection *gc, struct yahoo_packet *pkt);
void yahoo_process_chat_join(GaimConnection *gc, struct yahoo_packet *pkt);
void yahoo_process_chat_exit(GaimConnection *gc, struct yahoo_packet *pkt);
void yahoo_process_chat_message(GaimConnection *gc, struct yahoo_packet *pkt);
void yahoo_process_chat_addinvite(GaimConnection *gc, struct yahoo_packet *pkt);
void yahoo_process_chat_goto(GaimConnection *gc, struct yahoo_packet *pkt);

void yahoo_c_leave(GaimConnection *gc, int id);
int yahoo_c_send(GaimConnection *gc, int id, const char *what, GaimMessageFlags flags);
GList *yahoo_c_info(GaimConnection *gc);
GHashTable *yahoo_c_info_defaults(GaimConnection *gc, const char *chat_name);
void yahoo_c_join(GaimConnection *gc, GHashTable *data);
char *yahoo_get_chat_name(GHashTable *data);
void yahoo_c_invite(GaimConnection *gc, int id, const char *msg, const char *name);

void yahoo_conf_leave(struct yahoo_data *yd, const char *room, const char *dn, GList *who);

void yahoo_chat_goto(GaimConnection *gc, const char *name);

/* room listing functions */
GaimRoomlist *yahoo_roomlist_get_list(GaimConnection *gc);
void yahoo_roomlist_cancel(GaimRoomlist *list);
void yahoo_roomlist_expand_category(GaimRoomlist *list, GaimRoomlistRoom *category);

/* util */
void yahoo_chat_add_users(GaimConvChat *chat, GList *newusers);
void yahoo_chat_add_user(GaimConvChat *chat, const char *user, const char *reason);

#endif /* _YAHOO_CHAT_H_ */
