/**
 * @file group.c Group functions
 *
 * gaim
 *
 * Copyright (C) 2003 Christian Hammond <chipx86@gnupdate.org>
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
#include "msn.h"
#include "group.h"

MsnGroup *
msn_group_new(MsnSession *session, int id, const char *name)
{
	MsnGroup *group;

	g_return_val_if_fail(session != NULL, NULL);
	g_return_val_if_fail(id >= 0,         NULL);
	g_return_val_if_fail(name != NULL,    NULL);

	group = msn_groups_find_with_id(session->groups, id);

	if (group == NULL) {
		group = g_new0(MsnGroup, 1);

		group->session = session;
		group->id      = id;
		group->name    = g_strdup(name);
		group->users   = msn_users_new();
	}

	msn_group_ref(group);

	return group;
}

void
msn_group_destroy(MsnGroup *group)
{
	g_return_if_fail(group != NULL);

	if (group->ref_count > 0) {
		msn_group_unref(group);

		return;
	}

	if (group->session != NULL && group->session->groups != NULL)
		msn_groups_remove(group->session->groups, group);

	msn_users_destroy(group->users);

	g_free(group->name);
	g_free(group);
}

MsnGroup *
msn_group_ref(MsnGroup *group)
{
	g_return_val_if_fail(group != NULL, NULL);

	group->ref_count++;

	return group;
}

MsnGroup *
msn_group_unref(MsnGroup *group)
{
	g_return_val_if_fail(group != NULL, NULL);

	if (group->ref_count <= 0)
		return NULL;

	group->ref_count--;

	if (group->ref_count == 0) {
		msn_group_destroy(group);

		return NULL;
	}

	return group;
}

void
msn_group_set_id(MsnGroup *group, int id)
{
	g_return_if_fail(group != NULL);
	g_return_if_fail(id >= 0);

	group->id = id;
}

void
msn_group_set_name(MsnGroup *group, const char *name)
{
	g_return_if_fail(group != NULL);
	g_return_if_fail(name  != NULL);

	if (group->name != NULL)
		g_free(group->name);

	group->name = g_strdup(name);
}

int
msn_group_get_id(const MsnGroup *group)
{
	g_return_val_if_fail(group != NULL, -1);

	return group->id;
}

const char *
msn_group_get_name(const MsnGroup *group)
{
	g_return_val_if_fail(group != NULL, NULL);

	return group->name;
}

void
msn_group_add_user(MsnGroup *group, MsnUser *user)
{
	g_return_if_fail(group != NULL);
	g_return_if_fail(user  != NULL);

	msn_users_add(group->users, user);

	msn_user_ref(user);

	gaim_debug(GAIM_DEBUG_INFO, "msn", "Adding user %s to group %s (%d)\n",
			   msn_user_get_passport(user), msn_group_get_name(group),
			   msn_group_get_id(group));
}

void
msn_group_remove_user(MsnGroup *group, MsnUser *user)
{
	g_return_if_fail(group != NULL);
	g_return_if_fail(user  != NULL);

	msn_users_remove(group->users, user);

	msn_user_unref(user);
}

MsnUsers *
msn_group_get_users(const MsnGroup *group)
{
	g_return_val_if_fail(group != NULL, NULL);

	return group->users;
}


MsnGroups *
msn_groups_new(void)
{
	return g_new0(MsnGroups, 1);
}

void
msn_groups_destroy(MsnGroups *groups)
{
	g_return_if_fail(groups != NULL);

	while (groups->groups != NULL)
		msn_group_destroy(groups->groups->data);

	/* See if we've leaked anybody. */
	while (groups->groups != NULL) {
		gaim_debug(GAIM_DEBUG_WARNING, "msn",
				   "Leaking group %s (id %d)\n",
				   msn_group_get_name(groups->groups->data),
				   msn_group_get_id(groups->groups->data));
	}

	g_free(groups);
}

void
msn_groups_add(MsnGroups *groups, MsnGroup *group)
{
	g_return_if_fail(groups != NULL);
	g_return_if_fail(group != NULL);

	groups->groups = g_list_append(groups->groups, group);

	groups->count++;

	gaim_debug(GAIM_DEBUG_INFO, "msn", "Adding group %s (%d)\n",
			   msn_group_get_name(group), msn_group_get_id(group));
}

void
msn_groups_remove(MsnGroups *groups, MsnGroup *group)
{
	g_return_if_fail(groups != NULL);
	g_return_if_fail(group != NULL);

	gaim_debug(GAIM_DEBUG_INFO, "msn", "Removing group %s (%d)\n",
			   msn_group_get_name(group), msn_group_get_id(group));

	groups->groups = g_list_remove(groups->groups, group);

	groups->count--;
}

size_t
msn_groups_get_count(const MsnGroups *groups)
{
	g_return_val_if_fail(groups != NULL, 0);

	return groups->count;
}

GList *
msn_groups_get_list(const MsnGroups *groups)
{
	g_return_val_if_fail(groups != NULL, NULL);

	return groups->groups;
}

MsnGroup *
msn_groups_find_with_id(MsnGroups *groups, int id)
{
	GList *l;

	g_return_val_if_fail(groups != NULL, NULL);
	g_return_val_if_fail(id >= 0,        NULL);

	for (l = groups->groups; l != NULL; l = l->next) {
		MsnGroup *group = l->data;

		if (group->id == id)
			return group;
	}

	return NULL;
}

MsnGroup *
msn_groups_find_with_name(MsnGroups *groups, const char *name)
{
	GList *l;

	g_return_val_if_fail(groups != NULL, NULL);
	g_return_val_if_fail(name   != NULL, NULL);

	for (l = groups->groups; l != NULL; l = l->next) {
		MsnGroup *group = l->data;

		if (group->name != NULL && !g_ascii_strcasecmp(name, group->name))
			return group;
	}

	return NULL;
}
