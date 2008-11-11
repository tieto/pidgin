/**
 * @file group_internal.c
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include "internal.h"
#include "blist.h"
#include "debug.h"

#include "buddy_opt.h"
#include "group_internal.h"
#include "utils.h"

static qq_room_data *room_data_new(guint32 id, guint32 ext_id, gchar *title)
{
	qq_room_data *rmd;

	purple_debug_info("QQ", "Created room data: %s, ext id %u, id %u\n",
			title == NULL ? "(NULL)" : title,
			ext_id, id);
	rmd = g_new0(qq_room_data, 1);
	rmd->my_role = QQ_ROOM_ROLE_NO;
	rmd->id = id;
	rmd->ext_id = ext_id;
	rmd->type8 = 0x01;       /* assume permanent Qun */
	rmd->creator_uid = 10000;     /* assume by QQ admin */
	rmd->category = 0x01;
	rmd->auth_type = 0x02;        /* assume need auth */
	rmd->title_utf8 = g_strdup(title == NULL ? "" : title);
	rmd->desc_utf8 = g_strdup("");
	rmd->notice_utf8 = g_strdup("");
	rmd->members = NULL;
	rmd->is_got_buddies = FALSE;
	return rmd;
}

/* create a qq_room_data from hashtable */
static qq_room_data *room_data_new_by_hashtable(PurpleConnection *gc, GHashTable *data)
{
	qq_room_data *rmd;
	guint32 id, ext_id;
	gchar *value;

	value = g_hash_table_lookup(data, QQ_ROOM_KEY_INTERNAL_ID);
	id = value ? strtoul(value, NULL, 10) : 0;
	value= g_hash_table_lookup(data, QQ_ROOM_KEY_EXTERNAL_ID);
	ext_id = value ? strtoul(value, NULL, 10) : 0;
	value = g_strdup(g_hash_table_lookup(data, QQ_ROOM_KEY_TITLE_UTF8));

	rmd = room_data_new(id, ext_id, value);
	rmd->my_role = QQ_ROOM_ROLE_YES;
	return rmd;
}

/* gracefully free all members in a room */
static void room_buddies_free(qq_room_data *rmd)
{
	gint i;
	GList *list;
	qq_buddy_data *bd;

	g_return_if_fail(rmd != NULL);
	i = 0;
	while (NULL != (list = rmd->members)) {
		bd = (qq_buddy_data *) list->data;
		i++;
		rmd->members = g_list_remove(rmd->members, bd);
		qq_buddy_data_free(bd);
	}

	rmd->members = NULL;
}

/* gracefully free the memory for one qq_room_data */
static void room_data_free(qq_room_data *rmd)
{
	g_return_if_fail(rmd != NULL);
	room_buddies_free(rmd);
	g_free(rmd->title_utf8);
	g_free(rmd->desc_utf8);
	g_free(rmd->notice_utf8);
	g_free(rmd);
}

void qq_room_update_chat_info(PurpleChat *chat, qq_room_data *rmd)
{
	if (rmd->title_utf8 != NULL && strlen(rmd->title_utf8) > 0) {
		purple_blist_alias_chat(chat, rmd->title_utf8);
	}
	g_hash_table_replace(chat->components,
		     g_strdup(QQ_ROOM_KEY_INTERNAL_ID),
		     g_strdup_printf("%u", rmd->id));
	g_hash_table_replace(chat->components,
		     g_strdup(QQ_ROOM_KEY_EXTERNAL_ID),
		     g_strdup_printf("%u", rmd->ext_id));
	g_hash_table_replace(chat->components,
		     g_strdup(QQ_ROOM_KEY_TITLE_UTF8), g_strdup(rmd->title_utf8));
}

static PurpleChat *chat_new(PurpleConnection *gc, qq_room_data *rmd)
{
	GHashTable *components;
	PurpleGroup *g;
	PurpleChat *chat;

	purple_debug_info("QQ", "Add new chat: id %u, ext id %u, title %s\n",
		rmd->id, rmd->ext_id,
		rmd->title_utf8 == NULL ? "(NULL)" : rmd->title_utf8);

	components = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	g_hash_table_insert(components,
			    g_strdup(QQ_ROOM_KEY_INTERNAL_ID), g_strdup_printf("%u", rmd->id));
	g_hash_table_insert(components, g_strdup(QQ_ROOM_KEY_EXTERNAL_ID),
			    g_strdup_printf("%u", rmd->ext_id));
	g_hash_table_insert(components, g_strdup(QQ_ROOM_KEY_TITLE_UTF8), g_strdup(rmd->title_utf8));

	chat = purple_chat_new(purple_connection_get_account(gc), rmd->title_utf8, components);
	g = qq_group_find_or_new(PURPLE_GROUP_QQ_QUN);
	purple_blist_add_chat(chat, g, NULL);

	return chat;
}

PurpleChat *qq_room_find_or_new(PurpleConnection *gc, guint32 id, guint32 ext_id)
{
	qq_data *qd;
	qq_room_data *rmd;
	PurpleChat *chat;
	gchar *num_str;

	g_return_val_if_fail (gc != NULL && gc->proto_data != NULL, NULL);
	qd = (qq_data *) gc->proto_data;

	g_return_val_if_fail(id != 0 && ext_id != 0, NULL);

	purple_debug_info("QQ", "Find or add new room: id %u, ext id %u\n", id, ext_id);

	rmd = qq_room_data_find(gc, id);
	if (rmd == NULL) {
		rmd = room_data_new(id, ext_id, NULL);
		g_return_val_if_fail(rmd != NULL, NULL);
		rmd->my_role = QQ_ROOM_ROLE_YES;
		qd->groups = g_list_append(qd->groups, rmd);
	}

	num_str = g_strdup_printf("%u", ext_id);
	chat = purple_blist_find_chat(purple_connection_get_account(gc), num_str);
	g_free(num_str);
	if (chat) {
		return chat;
	}

	return chat_new(gc, rmd);
}

void qq_room_remove(PurpleConnection *gc, guint32 id)
{
	qq_data *qd;
	PurpleChat *chat;
	qq_room_data *rmd;
	gchar *num_str;
	guint32 ext_id;

	g_return_if_fail (gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	purple_debug_info("QQ", "Find and remove room data, id %u", id);
	rmd = qq_room_data_find(gc, id);
	g_return_if_fail (rmd != NULL);

	ext_id = rmd->ext_id;
	qd->groups = g_list_remove(qd->groups, rmd);
	room_data_free(rmd);

	purple_debug_info("QQ", "Find and remove chat, ext_id %u", ext_id);
	num_str = g_strdup_printf("%u", ext_id);
	chat = purple_blist_find_chat(purple_connection_get_account(gc), num_str);
	g_free(num_str);

	g_return_if_fail (chat != NULL);

	purple_blist_remove_chat(chat);
}

/* find a qq_buddy_data by uid, called by im.c */
qq_buddy_data *qq_room_buddy_find(qq_room_data *rmd, guint32 uid)
{
	GList *list;
	qq_buddy_data *bd;
	g_return_val_if_fail(rmd != NULL && uid > 0, NULL);

	list = rmd->members;
	while (list != NULL) {
		bd = (qq_buddy_data *) list->data;
		if (bd->uid == uid)
			return bd;
		else
			list = list->next;
	}

	return NULL;
}

/* remove a qq_buddy_data by uid, called by qq_group_opt.c */
void qq_room_buddy_remove(qq_room_data *rmd, guint32 uid)
{
	GList *list;
	qq_buddy_data *bd;
	g_return_if_fail(rmd != NULL && uid > 0);

	list = rmd->members;
	while (list != NULL) {
		bd = (qq_buddy_data *) list->data;
		if (bd->uid == uid) {
			rmd->members = g_list_remove(rmd->members, bd);
			return;
		} else {
			list = list->next;
		}
	}
}

qq_buddy_data *qq_room_buddy_find_or_new(PurpleConnection *gc, qq_room_data *rmd, guint32 member_uid)
{
	qq_buddy_data *member, *bd;
	PurpleBuddy *buddy;
	g_return_val_if_fail(rmd != NULL && member_uid > 0, NULL);

	member = qq_room_buddy_find(rmd, member_uid);
	if (member == NULL) {	/* first appear during my session */
		member = g_new0(qq_buddy_data, 1);
		member->uid = member_uid;
		buddy = purple_find_buddy(purple_connection_get_account(gc), uid_to_purple_name(member_uid));
		if (buddy != NULL) {
			bd = (qq_buddy_data *) buddy->proto_data;
			if (bd != NULL && bd->nickname != NULL)
				member->nickname = g_strdup(bd->nickname);
			else if (buddy->alias != NULL)
				member->nickname = g_strdup(buddy->alias);
		}
		rmd->members = g_list_append(rmd->members, member);
	}

	return member;
}

qq_room_data *qq_room_data_find(PurpleConnection *gc, guint32 room_id)
{
	GList *list;
	qq_room_data *rmd;
	qq_data *qd;

	qd = (qq_data *) gc->proto_data;

	if (qd->groups == NULL || room_id <= 0)
		return 0;

	list = qd->groups;
	while (list != NULL) {
		rmd = (qq_room_data *) list->data;
		if (rmd->id == room_id) {
			return rmd;
		}
		list = list->next;
	}

	return NULL;
}

guint32 qq_room_get_next(PurpleConnection *gc, guint32 room_id)
{
	GList *list;
	qq_room_data *rmd;
	qq_data *qd;
	gboolean is_find = FALSE;

	qd = (qq_data *) gc->proto_data;

	if (qd->groups == NULL) {
		return 0;
	}

	 if (room_id <= 0) {
	 	rmd = (qq_room_data *) qd->groups->data;
		return rmd->id;
	}

	list = qd->groups;
	while (list != NULL) {
		rmd = (qq_room_data *) list->data;
		list = list->next;
		if (rmd->id == room_id) {
			is_find = TRUE;
			break;
		}
	}

	g_return_val_if_fail(is_find, 0);
	if (list == NULL) return 0;	/* be the end */
 	rmd = (qq_room_data *) list->data;
	g_return_val_if_fail(rmd != NULL, 0);
	return rmd->id;
}

guint32 qq_room_get_next_conv(PurpleConnection *gc, guint32 room_id)
{
	GList *list;
	qq_room_data *rmd;
	qq_data *qd;
	gboolean is_find;

	qd = (qq_data *) gc->proto_data;

 	list = qd->groups;
	if (room_id > 0) {
		/* search next room */
		is_find = FALSE;
		while (list != NULL) {
			rmd = (qq_room_data *) list->data;
			list = list->next;
			if (rmd->id == room_id) {
				is_find = TRUE;
				break;
			}
		}
		g_return_val_if_fail(is_find, 0);
	}

	while (list != NULL) {
		rmd = (qq_room_data *) list->data;
		g_return_val_if_fail(rmd != NULL, 0);

		if (rmd->my_role == QQ_ROOM_ROLE_YES || rmd->my_role == QQ_ROOM_ROLE_ADMIN) {
			if (NULL != purple_find_conversation_with_account(
						PURPLE_CONV_TYPE_CHAT,rmd->title_utf8, purple_connection_get_account(gc))) {
				/* In convseration*/
				return rmd->id;
			}
		}
		list = list->next;
	}

	return 0;
}

/* this should be called upon signin, even when we did not open group chat window */
void qq_room_data_initial(PurpleConnection *gc)
{
	PurpleAccount *account;
	PurpleChat *chat;
	PurpleGroup *purple_group;
	PurpleBlistNode *node;
	qq_data *qd;
	qq_room_data *rmd;
	gint count;

	account = purple_connection_get_account(gc);
	qd = (qq_data *) gc->proto_data;

	purple_debug_info("QQ", "Initial QQ Qun configurations\n");
	purple_group = purple_find_group(PURPLE_GROUP_QQ_QUN);
	if (purple_group == NULL) {
		purple_debug_info("QQ", "We have no QQ Qun\n");
		return;
	}

	count = 0;
	for (node = ((PurpleBlistNode *) purple_group)->child; node != NULL; node = node->next) {
		if ( !PURPLE_BLIST_NODE_IS_CHAT(node)) {
			continue;
		}
		/* got one */
		chat = (PurpleChat *) node;
		if (account != chat->account)	/* not qq account*/
			continue;

		rmd = room_data_new_by_hashtable(gc, chat->components);
		qd->groups = g_list_append(qd->groups, rmd);
		count++;
	}

	purple_debug_info("QQ", "Load %d QQ Qun configurations\n", count);
}

void qq_room_data_free_all(PurpleConnection *gc)
{
	qq_data *qd;
	qq_room_data *rmd;
	gint count;

	g_return_if_fail (gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	count = 0;
	while (qd->groups != NULL) {
		rmd = (qq_room_data *) qd->groups->data;
		qd->groups = g_list_remove(qd->groups, rmd);
		room_data_free(rmd);
		count++;
	}

	if (count > 0) {
		purple_debug_info("QQ", "%d rooms are freed\n", count);
	}
}
