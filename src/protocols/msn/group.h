/**
 * @file group.h Group functions
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
#ifndef _MSN_GROUP_H_
#define _MSN_GROUP_H_

typedef struct _MsnGroup  MsnGroup;
typedef struct _MsnGroups MsnGroups;

#include "session.h"
#include "user.h"

/**
 * A group.
 */
struct _MsnGroup
{
	size_t ref_count;       /**< The reference count.       */

	MsnSession *session;    /**< The MSN session.           */

	int id;                 /**< The group ID.              */
	char *name;             /**< The name of the group.     */

	MsnUsers *users;        /**< The users in the group.    */
};

/**
 * A list of groups.
 */
struct _MsnGroups
{
	size_t count;  /**< The number of groups. */

	GList *groups; /**< The list of groups.   */
};

/**************************************************************************/
/** @name Group API                                                       */
/**************************************************************************/
/*@{*/

/**
 * Creates a new group structure.
 *
 * @param session The MSN session.
 * @param id      The group ID.
 * @param name    The name of the group.
 *
 * @return A new group structure.
 */
MsnGroup *msn_group_new(MsnSession *session, int id, const char *name);

/**
 * Destroys a group structure.
 *
 * @param group The group to destroy.
 */
void msn_group_destroy(MsnGroup *group);

/**
 * Increments the reference count on a group.
 *
 * @param group The group.
 *
 * @return @a group
 */
MsnGroup *msn_group_ref(MsnGroup *group);

/**
 * Decrements the reference count on a group.
 *
 * This will destroy the structure if the count hits 0.
 *
 * @param group The group.
 *
 * @return @a group, or @c NULL if the new count is 0.
 */
MsnGroup *msn_group_unref(MsnGroup *group);

/**
 * Sets the ID for a group.
 *
 * @param group The group.
 * @param id    The ID.
 */
void msn_group_set_id(MsnGroup *group, int id);

/**
 * Sets the name for a group.
 *
 * @param group The group.
 * @param name  The name.
 */
void msn_group_set_name(MsnGroup *group, const char *name);

/**
 * Returns the ID for a group.
 *
 * @param group The group.
 *
 * @return The ID.
 */
int msn_group_get_id(const MsnGroup *group);

/**
 * Returns the name for a group.
 *
 * @param group The group.
 *
 * @return The name.
 */
const char *msn_group_get_name(const MsnGroup *group);

/**
 * Adds a user to the group.
 *
 * @param group The group.
 * @param user  The user.
 */
void msn_group_add_user(MsnGroup *group, MsnUser *user);

/**
 * Removes a user from the group.
 *
 * @param group The group.
 * @param user  The user.
 */
void msn_group_remove_user(MsnGroup *group, MsnUser *user);

/**
 * Returns the users in a group.
 *
 * @param group The group.
 *
 * @return The list of users.
 */
MsnUsers *msn_group_get_users(const MsnGroup *group);

/*@}*/

/**************************************************************************/
/** @name Group List API                                                  */
/**************************************************************************/
/*@{*/

/**
 * Creates a new MsnGroups structure.
 *
 * @return A new MsnGroups structure.
 */
MsnGroups *msn_groups_new(void);

/**
 * Destroys a groups list.
 *
 * @param groups The groups list.
 */
void msn_groups_destroy(MsnGroups *groups);

/**
 * Adds a group to a groups list.
 *
 * @param groups The groups list.
 * @param group  The group.
 */
void msn_groups_add(MsnGroups *groups, MsnGroup *group);

/**
 * Removes a group from a groups list.
 *
 * @param groups The groups list.
 * @param group  The group.
 */
void msn_groups_remove(MsnGroups *groups, MsnGroup *group);

/**
 * Returns the number of groups in a groups list.
 *
 * @param groups The groups list.
 * 
 * @return The number of groups.
 */
size_t msn_groups_get_count(const MsnGroups *groups);

/**
 * Finds a group with the specified ID.
 *
 * @param groups A list of groups.
 * @param id     The group ID.
 *
 * @return The group if found, or @c NULL otherwise.
 */
MsnGroup *msn_groups_find_with_id(MsnGroups *groups, int id);

/**
 * Returns a GList of all groups.
 *
 * @param groups The list of groups.
 *
 * @return A GList of all groups.
 */
GList *msn_groups_get_list(const MsnGroups *groups);

/**
 * Finds a group with the specified name.
 *
 * @param groups A list of groups.
 * @param name   The group name.
 *
 * @return The group if found, or @c NULL otherwise.
 */
MsnGroup *msn_groups_find_with_name(MsnGroups *groups, const char *name);

#endif /* _MSN_GROUP_H_ */
