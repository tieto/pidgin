/**
 * @file group_free.c
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

#include "debug.h"

#include "buddy_list.h"
#include "group_free.h"

/* gracefully free all members in a group */
static void qq_group_free_member(qq_group *group)
{
	gint i;
	GList *list;
	qq_buddy *member;

	g_return_if_fail(group != NULL);
	i = 0;
	while (NULL != (list = group->members)) {
		member = (qq_buddy *) list->data;
		i++;
		group->members = g_list_remove(group->members, member);
		g_free(member->nickname);
		g_free(member);
	}

	group->members = NULL;
}

/* gracefully free the memory for one qq_group */
void qq_group_free(qq_group *group)
{
	g_return_if_fail(group != NULL);
	qq_group_free_member(group);
	g_free(group->my_role_desc);
	g_free(group->title_utf8);
	g_free(group->desc_utf8);
	g_free(group->notice_utf8);
	g_free(group);
}

void qq_group_free_all(qq_data *qd)
{
	qq_group *group;
	gint count;

	g_return_if_fail(qd != NULL);
	count = 0;
	while (qd->groups != NULL) {
		group = (qq_group *) qd->groups->data;
		qd->groups = g_list_remove(qd->groups, group);
		qq_group_free(group);
		count++;
	}

	if (count > 0) {
		purple_debug_info("QQ", "%d rooms are freed\n", count);
	}
}
