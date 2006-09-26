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

#include "internal.h"

#include "debug.h"
#include "prpl.h"
#include "request.h"

#include "group_internal.h"
#include "group_info.h"
#include "group_search.h"
#include "utils.h"

#include "group.h"

static void _qq_group_search_callback(GaimConnection *gc, const gchar *input)
{
	guint32 external_group_id;

	g_return_if_fail(input != NULL);
	external_group_id = qq_string_to_dec_value(input);
	/* 0x00000000 means search for demo group */
	qq_send_cmd_group_search_group(gc, external_group_id);
}

static void _qq_group_search_cancel_callback(GaimConnection *gc, const gchar *input)
{
	qq_data *qd;

	qd = (qq_data *) gc->proto_data;
	gaim_roomlist_set_in_progress(qd->roomlist, FALSE);
}

/* This is needed for GaimChat node to be valid */
GList *qq_chat_info(GaimConnection *gc)
{
	GList *m;
	struct proto_chat_entry *pce;

	m = NULL;

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("ID: ");
	pce->identifier = QQ_GROUP_KEY_EXTERNAL_ID;
	m = g_list_append(m, pce);
	
	return m;
}

GHashTable *qq_chat_info_defaults(GaimConnection *gc, const gchar *chat_name)
{
	GHashTable *defaults;

	defaults = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);

	if (chat_name != NULL)
		g_hash_table_insert(defaults, QQ_GROUP_KEY_EXTERNAL_ID, g_strdup(chat_name));

	return defaults;
}

/*  get a list of qq groups */
GaimRoomlist *qq_roomlist_get_list(GaimConnection *gc)
{
	GList *fields;
	qq_data *qd;
	GaimRoomlist *rl;
	GaimRoomlistField *f;

	qd = (qq_data *) gc->proto_data;

	fields = NULL;
	rl = gaim_roomlist_new(gaim_connection_get_account(gc));
	qd->roomlist = rl;

	f = gaim_roomlist_field_new(GAIM_ROOMLIST_FIELD_STRING, _("Group ID"), QQ_GROUP_KEY_EXTERNAL_ID, FALSE);
	fields = g_list_append(fields, f);
	f = gaim_roomlist_field_new(GAIM_ROOMLIST_FIELD_STRING, _("Creator"), QQ_GROUP_KEY_CREATOR_UID, FALSE);
	fields = g_list_append(fields, f);
	f = gaim_roomlist_field_new(GAIM_ROOMLIST_FIELD_STRING,
				    _("Group Description"), QQ_GROUP_KEY_GROUP_DESC_UTF8, FALSE);
	fields = g_list_append(fields, f);
	f = gaim_roomlist_field_new(GAIM_ROOMLIST_FIELD_STRING, "", QQ_GROUP_KEY_INTERNAL_ID, TRUE);
	fields = g_list_append(fields, f);
	f = gaim_roomlist_field_new(GAIM_ROOMLIST_FIELD_STRING, "", QQ_GROUP_KEY_GROUP_TYPE, TRUE);
	fields = g_list_append(fields, f);
	f = gaim_roomlist_field_new(GAIM_ROOMLIST_FIELD_STRING, _("Auth"), QQ_GROUP_KEY_AUTH_TYPE, TRUE);
	fields = g_list_append(fields, f);
	f = gaim_roomlist_field_new(GAIM_ROOMLIST_FIELD_STRING, "", QQ_GROUP_KEY_GROUP_CATEGORY, TRUE);
	fields = g_list_append(fields, f);
	f = gaim_roomlist_field_new(GAIM_ROOMLIST_FIELD_STRING, "", QQ_GROUP_KEY_GROUP_NAME_UTF8, TRUE);

	fields = g_list_append(fields, f);
	gaim_roomlist_set_fields(rl, fields);
	gaim_roomlist_set_in_progress(qd->roomlist, TRUE);

	gaim_request_input(gc, _("QQ Qun"),
			   _("Please input external group ID"),
			   _("You can only search for permanent QQ groups\n"),
			   NULL, FALSE, FALSE, NULL, 
			   _("Search"), G_CALLBACK(_qq_group_search_callback), 
			   _("Cancel"), G_CALLBACK(_qq_group_search_cancel_callback), 
			   gc);

	return qd->roomlist;
}

/* free roomlist space, I have no idea when this one is called ... */
void qq_roomlist_cancel(GaimRoomlist *list)
{
	qq_data *qd;
	GaimConnection *gc;

	g_return_if_fail(list != NULL);
	gc = gaim_account_get_connection(list->account);

	qd = (qq_data *) gc->proto_data;
	gaim_roomlist_set_in_progress(list, FALSE);
	gaim_roomlist_unref(list);
}

/* this should be called upon signin, even when we did not open group chat window */
void qq_group_init(GaimConnection *gc)
{
	gint i;
	GaimAccount *account;
	GaimChat *chat;
	GaimGroup *gaim_group;
	GaimBlistNode *node;
	qq_group *group;

	account = gaim_connection_get_account(gc);

	gaim_group = gaim_find_group(GAIM_GROUP_QQ_QUN);
	if (gaim_group == NULL) {
		gaim_debug(GAIM_DEBUG_INFO, "QQ", "We have no QQ Qun\n");
		return;
	}

	i = 0;
	for (node = ((GaimBlistNode *) gaim_group)->child; node != NULL; node = node->next)
		if (GAIM_BLIST_NODE_IS_CHAT(node)) {	/* got one */
			chat = (GaimChat *) node;
			if (account != chat->account)
				continue;	/* very important here ! */
			group = qq_group_from_hashtable(gc, chat->components);
			if (group != NULL) {
				i++;
				qq_send_cmd_group_get_group_info(gc, group);	/* get group info and members */
			}
		}

	gaim_debug(GAIM_DEBUG_INFO, "QQ", "Load %d QQ Qun configurations\n", i);
}
