/**
 * @file user.h User functions
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
#ifndef _MSN_USER_H_
#define _MSN_USER_H_

typedef struct _MsnUser  MsnUser;
typedef struct _MsnUsers MsnUsers;

#include "session.h"

/**
 * A user.
 */
struct _MsnUser
{
	MsnSession *session;    /**< The MSN session.           */

	char *passport;         /**< The passport account.      */
	char *name;             /**< The friendly name.         */

	struct
	{
		char *home;         /**< Home phone number.         */
		char *work;         /**< Work phone number.         */
		char *mobile;       /**< Mobile phone number.       */

	} phone;

	gboolean mobile;        /**< Signed up with MSN Mobile. */

	int group_id;           /**< The group ID.              */

	size_t ref_count;       /**< The reference count.       */

	GHashTable *clientcaps; /**< The client's capabilities. */
};

/**
 * A collection of users.
 */
struct _MsnUsers
{
	size_t count; /**< The number of users. */

	GList *users; /**< The list of users.   */
};

/**************************************************************************/
/** @name User API                                                        */
/**************************************************************************/
/*@{*/

/**
 * Creates a new user structure.
 * 
 * @param session  The MSN session.
 * @param passport The initial passport.
 * @param name     The initial friendly name.
 *
 * @return A new user structure.
 */
MsnUser *msn_user_new(MsnSession *session, const char *passport,
					  const char *name);

/**
 * Destroys a user structure.
 *
 * @param user The user to destroy.
 */
void msn_user_destroy(MsnUser *user);

/**
 * Increments the reference count on a user.
 *
 * @param user The user.
 *
 * @return @a user
 */
MsnUser *msn_user_ref(MsnUser *user);

/**
 * Decrements the reference count on a user.
 *
 * This will destroy the structure if the count hits 0.
 *
 * @param user The user.
 *
 * @return @a user, or @c NULL if the new count is 0.
 */
MsnUser *msn_user_unref(MsnUser *user);

/**
 * Sets the passport account for a user.
 *
 * @param user     The user.
 * @param passport The passport account.
 */
void msn_user_set_passport(MsnUser *user, const char *passport);

/**
 * Sets the friendly name for a user.
 *
 * @param user The user.
 * @param name The friendly name.
 */
void msn_user_set_name(MsnUser *user, const char *name);

/**
 * Sets the group ID for a user.
 *
 * @param user The user.
 * @param id   The group ID.
 */
void msn_user_set_group_id(MsnUser *user, int id);

/**
 * Sets the home phone number for a user.
 *
 * @param user   The user.
 * @param number The home phone number.
 */
void msn_user_set_home_phone(MsnUser *user, const char *number);

/**
 * Sets the work phone number for a user.
 *
 * @param user   The user.
 * @param number The work phone number.
 */
void msn_user_set_work_phone(MsnUser *user, const char *number);

/**
 * Sets the mobile phone number for a user.
 *
 * @param user   The user.
 * @param number The mobile phone number.
 */
void msn_user_set_mobile_phone(MsnUser *user, const char *number);

/**
 * Returns the passport account for a user.
 *
 * @param user The user.
 *
 * @return The passport account.
 */
const char *msn_user_get_passport(const MsnUser *user);

/**
 * Returns the friendly name for a user.
 *
 * @param user The user.
 *
 * @return The friendly name.
 */
const char *msn_user_get_name(const MsnUser *user);

/**
 * Returns the group ID for a user.
 *
 * @param user The user.
 *
 * @return The group ID.
 */
int msn_user_get_group_id(const MsnUser *user);

/**
 * Returns the home phone number for a user.
 *
 * @param user The user.
 *
 * @return The user's home phone number.
 */
const char *msn_user_get_home_phone(const MsnUser *user);

/**
 * Returns the work phone number for a user.
 *
 * @param user The user.
 *
 * @return The user's work phone number.
 */
const char *msn_user_get_work_phone(const MsnUser *user);

/**
 * Returns the mobile phone number for a user.
 *
 * @param user The user.
 *
 * @return The user's mobile phone number.
 */
const char *msn_user_get_mobile_phone(const MsnUser *user);

/**
 * Sets the client information for a user.
 *
 * @param user The user.
 * @param info The client information.
 */
void msn_user_set_client_caps(MsnUser *user, GHashTable *info);

/**
 * Returns the client information for a user.
 *
 * @param user The user.
 *
 * @return The client information.
 */
GHashTable *msn_user_get_client_caps(const MsnUser *user);

/*@}*/

/**************************************************************************/
/** @name User List API                                                   */
/**************************************************************************/
/*@{*/

/**
 * Creates a new MsnUsers structure.
 *
 * @return A new MsnUsers structure.
 */
MsnUsers *msn_users_new(void);

/**
 * Destroys a users list.
 *
 * @param users The users list.
 */
void msn_users_destroy(MsnUsers *users);

/**
 * Adds a user to a users list.
 *
 * @param users The users list.
 * @param user  The user.
 */
void msn_users_add(MsnUsers *users, MsnUser *user);

/**
 * Removes a user from a users list.
 *
 * @param users The users list.
 * @param user  The user.
 */
void msn_users_remove(MsnUsers *users, MsnUser *user);

/**
 * Returns the number of users in a users list.
 *
 * @param users The users list.
 * 
 * @return The number of users.
 */
size_t msn_users_get_count(const MsnUsers *users);

/**
 * Finds a user with the specified passport.
 *
 * @param users    A list of users.
 * @param passport The passport.
 *
 * @return The user if found, or @c NULL otherwise.
 */
MsnUser *msn_users_find_with_passport(MsnUsers *users, const char *passport);

/*@}*/

#endif /* _MSN_USER_H_ */
