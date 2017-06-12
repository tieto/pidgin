/*
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
 *
 */
#include <string.h>
#include <glib.h>

#include "dbus-useful.h"
#include "accounts.h"
#include "conversation.h"
#include "util.h"


PurpleAccount *
purple_accounts_find_ext(const char *name, const char *protocol_id,
		       gboolean (*account_test)(const PurpleAccount *account))
{
	PurpleAccount *result = NULL;
	GList *l;
	char *who;

	if (name)
		who = g_strdup(purple_normalize(NULL, name));
	else
		who = NULL;

	for (l = purple_accounts_get_all(); l != NULL; l = l->next) {
		PurpleAccount *account = (PurpleAccount *)l->data;

		if (who && !purple_strequal(purple_normalize(NULL, purple_account_get_username(account)), who))
			continue;

		if (protocol_id && !purple_strequal(purple_account_get_protocol_id(account), protocol_id))
			continue;

		if (account_test && !account_test(account))
			continue;

		result = account;
		break;
	}

	g_free(who);

	return result;
}

PurpleAccount *purple_accounts_find_any(const char *name, const char *protocol)
{
	return purple_accounts_find_ext(name, protocol, NULL);
}

PurpleAccount *purple_accounts_find_connected(const char *name, const char *protocol)
{
	return purple_accounts_find_ext(name, protocol, purple_account_is_connected);
}


