/**
 * @file confer.h
 *
 * gaim
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifndef _GAIM_GG_CONFER_H
#define _GAIM_GG_CONFER_H

#include "gg.h"

/**
 * Finds a CHAT conversation for the current account with the specified name.
 *
 * @param gc   GaimConnection instance.
 * @param name Name of the conversation.
 *
 * @return GaimConversation or NULL if not found.
 */
GaimConversation *
ggp_confer_find_by_name(GaimConnection *gc, const gchar *name);

/**
 * Adds the specified UIN to the specified conversation.
 * 
 * @param gc        GaimConnection.
 * @param chat_name Name of the conversation.
 */
void
ggp_confer_participants_add_uin(GaimConnection *gc, const gchar *chat_name,
						    const uin_t uin);

/**
 * Add the specified UINs to the specified conversation.
 *
 * @param gc         GaimConnection.
 * @param chat_name  Name of the conversation.
 * @param recipients List of the UINs.
 * @param count      Number of the UINs.
 */
void
ggp_confer_participants_add(GaimConnection *gc, const gchar *chat_name,
			    const uin_t *recipients, int count);

/**
 * Finds a conversation in which all the specified recipients participate.
 * 
 * TODO: This function should be rewritten to better handle situations when
 * somebody adds more people to the converation.
 *
 * @param gc         GaimConnection.
 * @param recipients List of the people in the conversation.
 * @param count      Number of people.
 *
 * @return Name of the conversation.
 */
const char*
ggp_confer_find_by_participants(GaimConnection *gc, const uin_t *recipients,
						    int count);

/**
 * Adds a new conversation to the internal list of conversations.
 * If name is NULL then it will be automagically generated.
 *
 * @param gc   GaimConnection.
 * @param name Name of the conversation.
 * 
 * @return Name of the conversation.
 */
const char*
ggp_confer_add_new(GaimConnection *gc, const char *name);


#endif /* _GAIM_GG_CONFER_H */

/* vim: set ts=8 sts=0 sw=8 noet: */
