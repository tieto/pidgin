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

#include "conversation.h"
#include "debug.h"

#include "group_find.h"
#include "group_network.h"
#include "qq.h"
#include "utils.h"

/* find a chat member's valid gaim_name of its nickname and chat room channel */
gchar *qq_group_find_member_by_channel_and_nickname(GaimConnection *gc, gint channel, const gchar *who) 
{
	qq_group *group;
	qq_buddy *member;
	GList *list;

	g_return_val_if_fail(gc != NULL && who != NULL, NULL);

	/* if it starts with QQ_NAME_PREFIX, we think it is valid name already
	 * otherwise we think it is nickname and try to find the matching gaim_name */
	if (g_str_has_prefix(who, QQ_NAME_PREFIX) && gaim_name_to_uid(who) > 0)
		return (gchar *) who;

	group = qq_group_find_by_channel(gc, channel);
	g_return_val_if_fail(group != NULL, NULL);

	list = group->members;
	member = NULL;
	while (list != NULL) {
		member = (qq_buddy *) list->data;
		if (member->nickname != NULL && !g_ascii_strcasecmp(member->nickname, who))
			break;
		list = list->next;
	}

	return (member == NULL) ? NULL : uid_to_gaim_name(member->uid);
}

/* find the internal_group_id by the reply packet sequence
 * return TRUE if we have a record of it, return FALSE if not */
gboolean qq_group_find_internal_group_id_by_seq(GaimConnection *gc, guint16 seq, guint32 *internal_group_id)
{
	GList *list;
	qq_data *qd;
	group_packet *p;

	g_return_val_if_fail(gc != NULL && gc->proto_data != NULL && internal_group_id != NULL, FALSE);
	qd = (qq_data *) gc->proto_data;

	list = qd->group_packets;
	while (list != NULL) {
		p = (group_packet *) (list->data);
		if (p->send_seq == seq) {	/* found and remove */
			*internal_group_id = p->internal_group_id;
			qd->group_packets = g_list_remove(qd->group_packets, p);
			g_free(p);
			return TRUE;
		}
		list = list->next;
	}

	return FALSE;
}

/* find a qq_buddy by uid, called by qq_im.c */
qq_buddy *qq_group_find_member_by_uid(qq_group *group, guint32 uid)
{
	GList *list;
	qq_buddy *member;
	g_return_val_if_fail(group != NULL && uid > 0, NULL);

	list = group->members;
	while (list != NULL) {
		member = (qq_buddy *) list->data;
		if (member->uid == uid)
			return member;
		else
			list = list->next;
	}

	return NULL;
}

/* remove a qq_buddy by uid, called by qq_group_opt.c */
void qq_group_remove_member_by_uid(qq_group *group, guint32 uid)
{
	GList *list;
	qq_buddy *member;
	g_return_if_fail(group != NULL && uid > 0);

	list = group->members;
	while (list != NULL) {
		member = (qq_buddy *) list->data;
		if (member->uid == uid) {
			group->members = g_list_remove(group->members, member);
			return;
		} else {
			list = list->next;
		}
	}
}

qq_buddy *qq_group_find_or_add_member(GaimConnection *gc, qq_group *group, guint32 member_uid)
{
	qq_buddy *member, *q_bud;
	GaimBuddy *buddy;
	g_return_val_if_fail(gc != NULL && group != NULL && member_uid > 0, NULL);

	member = qq_group_find_member_by_uid(group, member_uid);
	if (member == NULL) {	/* first appear during my session */
		member = g_new0(qq_buddy, 1);
		member->uid = member_uid;
		buddy = gaim_find_buddy(gaim_connection_get_account(gc), uid_to_gaim_name(member_uid));
		if (buddy != NULL) {
			q_bud = (qq_buddy *) buddy->proto_data;
			if (q_bud != NULL)
				member->nickname = g_strdup(q_bud->nickname);
			else if (buddy->alias != NULL)
				member->nickname = g_strdup(buddy->alias);
		}
		group->members = g_list_append(group->members, member);
	}

	return member;
}

/* find a qq_group by chatroom channel */
qq_group *qq_group_find_by_channel(GaimConnection *gc, gint channel)
{
	GaimConversation *conv;
	qq_data *qd;
	qq_group *group;
	GList *list;

	g_return_val_if_fail(gc != NULL && gc->proto_data != NULL, NULL);
	qd = (qq_data *) gc->proto_data;

	conv = gaim_find_chat(gc, channel);
	g_return_val_if_fail(conv != NULL, NULL);

	list = qd->groups;
	group = NULL;
	while (list != NULL) {
		group = (qq_group *) list->data;
		if (!g_ascii_strcasecmp(gaim_conversation_get_name(conv), group->group_name_utf8))
			break;
		list = list->next;
	}

	return group;
}

/* find a qq_group by internal_group_id */
qq_group *qq_group_find_by_internal_group_id(GaimConnection *gc, guint32 internal_group_id)
{
	GList *list;
	qq_group *group;
	qq_data *qd;

	g_return_val_if_fail(gc != NULL && gc->proto_data != NULL && internal_group_id > 0, NULL);

	qd = (qq_data *) gc->proto_data;
	if (qd->groups == NULL)
		return NULL;

	list = qd->groups;
	while (list != NULL) {
		group = (qq_group *) list->data;
		if (group->internal_group_id == internal_group_id)
			return group;
		list = list->next;
	}

	return NULL;
}
