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

#include "debug.h"

#include "buddy_status.h"
#include "group_free.h"
#include "group_network.h"

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
	g_free(group->group_name_utf8);
	g_free(group->group_desc_utf8);
	g_free(group);
}

/* clean up group_packets and free all contents */
void qq_group_packets_free(qq_data *qd)
{
	group_packet *p;
	gint i;

	i = 0;
	while (qd->group_packets != NULL) {
		p = (group_packet *) (qd->group_packets->data);
		qd->group_packets = g_list_remove(qd->group_packets, p);
		g_free(p);
		i++;
	}
	gaim_debug(GAIM_DEBUG_INFO, "QQ", "%d group packets are freed!\n", i);
}

void qq_group_free_all(qq_data *qd)
{
	qq_group *group;
	gint i;
	g_return_if_fail(qd != NULL);

	i = 0;
	while (qd->groups != NULL) {
		i++;
		group = (qq_group *) qd->groups->data;
		qd->groups = g_list_remove(qd->groups, group);
		qq_group_free(group);
	}

	gaim_debug(GAIM_DEBUG_INFO, "QQ", "%d groups are freed\n", i);
}
