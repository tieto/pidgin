/**
 * @file confer.c
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


#include "lib/libgadu.h"
#include "gg.h"
#include "utils.h"
#include "confer.h"

/* GaimConversation *ggp_confer_find_by_name(GaimConnection *gc, const gchar *name) {{{ */
GaimConversation *ggp_confer_find_by_name(GaimConnection *gc, const gchar *name)
{
	g_return_val_if_fail(gc   != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	return gaim_find_conversation_with_account(GAIM_CONV_TYPE_CHAT, name,
			gaim_connection_get_account(gc));
}
/* }}} */

/* void ggp_confer_participants_add_uin(GaimConnection *gc, const gchar *chat_name, const uin_t uin) {{{ */
void ggp_confer_participants_add_uin(GaimConnection *gc, const gchar *chat_name,
							 const uin_t uin)
{
	GaimConversation *conv;
	GGPInfo *info = gc->proto_data;
	GGPChat *chat;
	GList *l;
	gchar *str_uin;

	str_uin = g_strdup_printf("%lu", (unsigned long int)uin);

	for (l = info->chats; l != NULL; l = l->next) {
		chat = l->data;

		if (g_utf8_collate(chat->name, chat_name) != 0)
			continue;

		if (g_list_find(chat->participants, str_uin) == NULL) {
			chat->participants = g_list_append(
						chat->participants, str_uin);

			conv = ggp_confer_find_by_name(gc, chat_name);

			gaim_conv_chat_add_user(GAIM_CONV_CHAT(conv),
				ggp_buddy_get_name(gc, uin), NULL,
				GAIM_CBFLAGS_NONE, TRUE);
		}
		break;
	}
}
/* }}} */

/* void ggp_confer_participants_add(GaimConnection *gc, const gchar *chat_name, const uin_t *recipients, int count) {{{ */
void ggp_confer_participants_add(GaimConnection *gc, const gchar *chat_name,
				 const uin_t *recipients, int count)
{
	GaimConversation *conv;
	GGPInfo *info = gc->proto_data;
	GGPChat *chat;
	GList *l;
	int i;
	gchar *uin;

	for (l = info->chats; l != NULL; l = l->next) {
		chat = l->data;

		if (g_utf8_collate(chat->name, chat_name) != 0)
			continue;

		for (i = 0; i < count; i++) {
			uin = g_strdup_printf("%lu", (unsigned long int)recipients[i]);

			if (g_list_find(chat->participants, uin) != NULL) {
				g_free(uin);
				continue;
			}

			chat->participants = g_list_append(chat->participants, uin);
			conv = ggp_confer_find_by_name(gc, chat_name);

			gaim_conv_chat_add_user(GAIM_CONV_CHAT(conv),
				ggp_buddy_get_name(gc, recipients[i]),
				NULL, GAIM_CBFLAGS_NONE, TRUE);

			g_free(uin);
		}
		break;
	}
}
/* }}} */

/* const char *ggp_confer_find_by_participants(GaimConnection *gc, const uin_t *recipients, int count) {{{ */
const char *ggp_confer_find_by_participants(GaimConnection *gc,
					    const uin_t *recipients, int count)
{
	GGPInfo *info = gc->proto_data;
	GGPChat *chat = NULL;
	GList *l, *m;
	int i;
	int maches;

	g_return_val_if_fail(info->chats != NULL, NULL);

	for (l = info->chats; l != NULL; l = l->next) {
		chat = l->data;
		maches = 0;

		for (m = chat->participants; m != NULL; m = m->next) {
			uin_t p = ggp_str_to_uin(m->data);

			for (i = 0; i < count; i++)
				if (p == recipients[i])
					maches++;
		}

		if (maches == count)
			break;

		chat = NULL;
	}

	if (chat == NULL)
		return NULL;
	else
		return chat->name;
}
/* }}} */

/* const char *ggp_confer_add_new(GaimConnection *gc, const char *name) {{{ */
const char *ggp_confer_add_new(GaimConnection *gc, const char *name)
{
	GGPInfo *info = gc->proto_data;
	GGPChat *chat;

	chat = g_new0(GGPChat, 1);

	if (name == NULL)
		chat->name = g_strdup_printf("conf#%d", info->chats_count++);
	else
		chat->name = g_strdup(name);

	chat->participants = NULL;

	info->chats = g_list_append(info->chats, chat);

	return chat->name;
}
/* }}} */

/* vim: set ts=8 sts=0 sw=8 noet: */
