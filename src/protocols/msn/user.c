/**
 * @file user.c User functions
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
#include "user.h"

MsnUser *
msn_user_new(MsnSession *session, const char *passport, const char *name)
{
	MsnUser *user;

	user = msn_users_find_with_passport(session->users, passport);

	if (user != NULL) {
		if (name != NULL)
			msn_user_set_name(user, name);

		msn_user_ref(user);

		return user;
	}

	user = g_new0(MsnUser, 1);

	user->session = session;

	if (name != NULL)
		msn_user_set_name(user, name);

	msn_user_set_passport(user, passport);
	msn_user_set_group_id(user, -1);

	msn_users_add(session->users, user);

	msn_user_ref(user);

	return user;
}

void
msn_user_destroy(MsnUser *user)
{
	g_return_if_fail(user != NULL);

	if (user->ref_count > 0) {
		msn_user_unref(user);

		return;
	}

	if (user->session != NULL && user->session->users != NULL)
		msn_users_remove(user->session->users, user);

	if (user->clientinfo != NULL)
		g_hash_table_destroy(user->clientinfo);

	if (user->passport != NULL) g_free(user->passport);
	if (user->name     != NULL) g_free(user->name);

	if (user->phone.home   != NULL) g_free(user->phone.home);
	if (user->phone.work   != NULL) g_free(user->phone.work);
	if (user->phone.mobile != NULL) g_free(user->phone.mobile);

	g_free(user);
}

MsnUser *
msn_user_ref(MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	user->ref_count++;

	return user;
}

MsnUser *
msn_user_unref(MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	if (user->ref_count <= 0)
		return NULL;

	user->ref_count--;

	if (user->ref_count == 0) {
		msn_user_destroy(user);

		return NULL;
	}

	return user;
}

void
msn_user_set_passport(MsnUser *user, const char *passport)
{
	g_return_if_fail(user != NULL);

	if (user->passport != NULL)
		g_free(user->passport);

	user->passport = g_strdup(passport);
}

void
msn_user_set_name(MsnUser *user, const char *name)
{
	g_return_if_fail(user != NULL);

	if (user->name != NULL)
		g_free(user->name);

	user->name = g_strdup(name);
}

void
msn_user_set_group_id(MsnUser *user, int id)
{
	g_return_if_fail(user != NULL);

	user->group_id = id;
}

void
msn_user_set_home_phone(MsnUser *user, const char *number)
{
	g_return_if_fail(user != NULL);

	if (user->phone.home != NULL)
		g_free(user->phone.home);

	user->phone.home = (number == NULL ? NULL : g_strdup(number));
}

void
msn_user_set_work_phone(MsnUser *user, const char *number)
{
	g_return_if_fail(user != NULL);

	if (user->phone.work != NULL)
		g_free(user->phone.work);

	user->phone.work = (number == NULL ? NULL : g_strdup(number));
}

void
msn_user_set_mobile_phone(MsnUser *user, const char *number)
{
	g_return_if_fail(user != NULL);

	if (user->phone.mobile != NULL)
		g_free(user->phone.mobile);

	user->phone.mobile = (number == NULL ? NULL : g_strdup(number));
}


const char *
msn_user_get_passport(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	return user->passport;
}

const char *
msn_user_get_name(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	return user->name;
}

int
msn_user_get_group_id(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, -1);

	return user->group_id;
}

const char *
msn_user_get_home_phone(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	return user->phone.home;
}

const char *
msn_user_get_work_phone(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	return user->phone.work;
}

const char *
msn_user_get_mobile_phone(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	return user->phone.mobile;
}

void
msn_user_set_client_info(MsnUser *user, GHashTable *info)
{
	g_return_if_fail(user != NULL);
	g_return_if_fail(info != NULL);

	if (user->clientinfo != NULL)
		g_hash_table_destroy(user->clientinfo);

	user->clientinfo = info;
}

GHashTable *
msn_user_get_client_info(const MsnUser *user)
{
	g_return_val_if_fail(user != NULL, NULL);

	return user->clientinfo;
}

MsnUsers *
msn_users_new(void)
{
	MsnUsers *users = g_new0(MsnUsers, 1);

	return users;
}

void
msn_users_destroy(MsnUsers *users)
{
	g_return_if_fail(users != NULL);

	while (users->users != NULL)
		msn_user_destroy(users->users->data);

	/* See if we've leaked anybody. */
	while (users->users != NULL) {
		gaim_debug(GAIM_DEBUG_WARNING, "msn",
				   "Leaking user %s\n",
				   msn_user_get_passport(users->users->data));
	}

	g_free(users);
}

void
msn_users_add(MsnUsers *users, MsnUser *user)
{
	g_return_if_fail(users != NULL);
	g_return_if_fail(user != NULL);

	users->users = g_list_append(users->users, user);
}

void
msn_users_remove(MsnUsers *users, MsnUser *user)
{
	g_return_if_fail(users != NULL);
	g_return_if_fail(user != NULL);

	users->users = g_list_remove(users->users, user);
}

MsnUser *
msn_users_find_with_passport(MsnUsers *users, const char *passport)
{
	GList *l;

	g_return_val_if_fail(users != NULL, NULL);
	g_return_val_if_fail(passport != NULL, NULL);

	for (l = users->users; l != NULL; l = l->next) {
		MsnUser *user = (MsnUser *)l->data;

		if (user->passport != NULL &&
			!g_ascii_strcasecmp(passport, user->passport)) {

			return user;
		}
	}

	return NULL;
}
