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

#include "blist.h"
#include "debug.h"

#include "buddy_opt.h"
#include "group_hash.h"
#include "group_misc.h"
#include "utils.h"

static gchar *_qq_group_set_my_status_desc(qq_group *group)
{
	const char *status_desc;
	g_return_val_if_fail(group != NULL, g_strdup(""));

	switch (group->my_status) {
	case QQ_GROUP_MEMBER_STATUS_NOT_MEMBER:
		status_desc = _("I am not member");
		break;
	case QQ_GROUP_MEMBER_STATUS_IS_MEMBER:
		status_desc = _("I am a member");
		break;
	case QQ_GROUP_MEMBER_STATUS_APPLYING:
		status_desc = _("I am applying to join");
		break;
	case QQ_GROUP_MEMBER_STATUS_IS_ADMIN:
		status_desc = _("I am the admin");
		break;
	default:
		status_desc = _("Unknown status");
	}

	return g_strdup(status_desc);
}

static void _qq_group_add_to_blist(GaimConnection *gc, qq_group *group)
{
	GHashTable *components;
	GaimGroup *g;
	GaimChat *chat;
	components = qq_group_to_hashtable(group);
	chat = gaim_chat_new(gaim_connection_get_account(gc), group->group_name_utf8, components);
	g = qq_get_gaim_group(GAIM_GROUP_QQ_QUN);
	gaim_blist_add_chat(chat, g, NULL);
	gaim_debug(GAIM_DEBUG_INFO, "QQ", "You have add group \"%s\" to blist locally\n", group->group_name_utf8);
}

/* create a dummy qq_group, which includes only internal_id and external_id
 * all other attributes should be set to empty.
 * and we need to send a get_group_info to QQ server to update it right away */
qq_group *qq_group_create_by_id(GaimConnection *gc, guint32 internal_id, guint32 external_id)
{
	qq_group *group;
	qq_data *qd;

	g_return_val_if_fail(gc != NULL && gc->proto_data != NULL, NULL);
	g_return_val_if_fail(internal_id > 0, NULL);
	qd = (qq_data *) gc->proto_data;

	group = g_new0(qq_group, 1);
	group->my_status = QQ_GROUP_MEMBER_STATUS_NOT_MEMBER;
	group->my_status_desc = _qq_group_set_my_status_desc(group);
	group->internal_group_id = internal_id;
	group->external_group_id = external_id;
	group->group_type = 0x01;	/* assume permanent Qun */
	group->creator_uid = 10000;	/* assume by QQ admin */
	group->group_category = 0x01;
	group->auth_type = 0x02;	/* assume need auth */
	group->group_name_utf8 = g_strdup("");
	group->group_desc_utf8 = g_strdup("");
	group->notice_utf8 = g_strdup("");
	group->members = NULL;

	qd->groups = g_list_append(qd->groups, group);
	_qq_group_add_to_blist(gc, group);

	return group;
}

/* convert a qq_group to hash-table, which could be component of GaimChat */
GHashTable *qq_group_to_hashtable(qq_group *group)
{
	GHashTable *components;
	components = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	g_hash_table_insert(components, g_strdup(QQ_GROUP_KEY_MEMBER_STATUS), g_strdup_printf("%d", group->my_status));
	group->my_status_desc = _qq_group_set_my_status_desc(group);

	g_hash_table_insert(components,
			    g_strdup(QQ_GROUP_KEY_INTERNAL_ID), g_strdup_printf("%d", group->internal_group_id));
	g_hash_table_insert(components, g_strdup(QQ_GROUP_KEY_EXTERNAL_ID),
			    g_strdup_printf("%d", group->external_group_id));
	g_hash_table_insert(components, g_strdup(QQ_GROUP_KEY_GROUP_TYPE), g_strdup_printf("%d", group->group_type));
	g_hash_table_insert(components, g_strdup(QQ_GROUP_KEY_CREATOR_UID), g_strdup_printf("%d", group->creator_uid));
	g_hash_table_insert(components,
			    g_strdup(QQ_GROUP_KEY_GROUP_CATEGORY), g_strdup_printf("%d", group->group_category));
	g_hash_table_insert(components, g_strdup(QQ_GROUP_KEY_AUTH_TYPE), g_strdup_printf("%d", group->auth_type));
	g_hash_table_insert(components, g_strdup(QQ_GROUP_KEY_MEMBER_STATUS_DESC), g_strdup(group->my_status_desc));
	g_hash_table_insert(components, g_strdup(QQ_GROUP_KEY_GROUP_NAME_UTF8), g_strdup(group->group_name_utf8));
	g_hash_table_insert(components, g_strdup(QQ_GROUP_KEY_GROUP_DESC_UTF8), g_strdup(group->group_desc_utf8));
	return components;
}

/* create a qq_group from hashtable */
qq_group *qq_group_from_hashtable(GaimConnection *gc, GHashTable *data)
{
	qq_data *qd;
	qq_group *group;

	g_return_val_if_fail(gc != NULL && gc->proto_data != NULL, NULL);
	g_return_val_if_fail(data != NULL, NULL);
	qd = (qq_data *) gc->proto_data;

	group = g_new0(qq_group, 1);
	group->my_status =
	    qq_string_to_dec_value
	    (NULL ==
	     g_hash_table_lookup(data,
				 QQ_GROUP_KEY_MEMBER_STATUS) ?
	     g_strdup_printf("%d",
			     QQ_GROUP_MEMBER_STATUS_NOT_MEMBER) :
	     g_hash_table_lookup(data, QQ_GROUP_KEY_MEMBER_STATUS));
	group->internal_group_id = qq_string_to_dec_value(g_hash_table_lookup(data, QQ_GROUP_KEY_INTERNAL_ID));
	group->external_group_id = qq_string_to_dec_value(g_hash_table_lookup(data, QQ_GROUP_KEY_EXTERNAL_ID));
	group->group_type = qq_string_to_dec_value(g_hash_table_lookup(data, QQ_GROUP_KEY_GROUP_TYPE));
	group->creator_uid = qq_string_to_dec_value(g_hash_table_lookup(data, QQ_GROUP_KEY_CREATOR_UID));
	group->group_category = qq_string_to_dec_value(g_hash_table_lookup(data, QQ_GROUP_KEY_GROUP_CATEGORY));
	group->auth_type = qq_string_to_dec_value(g_hash_table_lookup(data, QQ_GROUP_KEY_AUTH_TYPE));
	group->group_name_utf8 = g_strdup(g_hash_table_lookup(data, QQ_GROUP_KEY_GROUP_NAME_UTF8));
	group->group_desc_utf8 = g_strdup(g_hash_table_lookup(data, QQ_GROUP_KEY_GROUP_DESC_UTF8));
	group->my_status_desc = _qq_group_set_my_status_desc(group);

	qd->groups = g_list_append(qd->groups, group);

	return group;
}

/* refresh group local subscription */
void qq_group_refresh(GaimConnection *gc, qq_group *group)
{
	GaimChat *chat;
	g_return_if_fail(gc != NULL && group != NULL);

	chat = gaim_blist_find_chat(gaim_connection_get_account(gc), g_strdup_printf("%d", group->external_group_id));
	if (chat == NULL && group->my_status != QQ_GROUP_MEMBER_STATUS_NOT_MEMBER) {
		_qq_group_add_to_blist(gc, group);
	} else if (chat != NULL) {	/* we have a local record, update its info */
		/* if there is group_name_utf8, we update the group name */
		if (group->group_name_utf8 != NULL && strlen(group->group_name_utf8) > 0)
			gaim_blist_alias_chat(chat, group->group_name_utf8);
		g_hash_table_replace(chat->components,
				     g_strdup(QQ_GROUP_KEY_MEMBER_STATUS), g_strdup_printf("%d", group->my_status));
		group->my_status_desc = _qq_group_set_my_status_desc(group);
		g_hash_table_replace(chat->components,
				     g_strdup(QQ_GROUP_KEY_MEMBER_STATUS_DESC), g_strdup(group->my_status_desc));
		g_hash_table_replace(chat->components,
				     g_strdup(QQ_GROUP_KEY_INTERNAL_ID),
				     g_strdup_printf("%d", group->internal_group_id));
		g_hash_table_replace(chat->components,
				     g_strdup(QQ_GROUP_KEY_EXTERNAL_ID),
				     g_strdup_printf("%d", group->external_group_id));
		g_hash_table_replace(chat->components,
				     g_strdup(QQ_GROUP_KEY_GROUP_TYPE), g_strdup_printf("%d", group->group_type));
		g_hash_table_replace(chat->components,
				     g_strdup(QQ_GROUP_KEY_CREATOR_UID), g_strdup_printf("%d", group->creator_uid));
		g_hash_table_replace(chat->components,
				     g_strdup(QQ_GROUP_KEY_GROUP_CATEGORY),
				     g_strdup_printf("%d", group->group_category));
		g_hash_table_replace(chat->components,
				     g_strdup(QQ_GROUP_KEY_AUTH_TYPE), g_strdup_printf("%d", group->auth_type));
		g_hash_table_replace(chat->components,
				     g_strdup(QQ_GROUP_KEY_GROUP_NAME_UTF8), g_strdup(group->group_name_utf8));
		g_hash_table_replace(chat->components,
				     g_strdup(QQ_GROUP_KEY_GROUP_DESC_UTF8), g_strdup(group->group_desc_utf8));
	}
}
