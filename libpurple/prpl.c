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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include "internal.h"
#include "conversation.h"
#include "debug.h"
#include "notify.h"
#include "prpl.h"
#include "request.h"
#include "util.h"

/**************************************************************************/
/** @name Protocol Plugin API  */
/**************************************************************************/
void
purple_prpl_got_account_idle(PurpleAccount *account, gboolean idle,
						   time_t idle_time)
{
	g_return_if_fail(account != NULL);
	g_return_if_fail(purple_account_is_connected(account));

	purple_presence_set_idle(purple_account_get_presence(account),
						   idle, idle_time);
}

void
purple_prpl_got_account_login_time(PurpleAccount *account, time_t login_time)
{
	PurplePresence *presence;

	g_return_if_fail(account != NULL);
	g_return_if_fail(purple_account_is_connected(account));

	if (login_time == 0)
		login_time = time(NULL);

	presence = purple_account_get_presence(account);

	purple_presence_set_login_time(presence, login_time);
}

void
purple_prpl_got_account_status(PurpleAccount *account, const char *status_id, ...)
{
	PurplePresence *presence;
	PurpleStatus *status;
	va_list args;

	g_return_if_fail(account   != NULL);
	g_return_if_fail(status_id != NULL);
	g_return_if_fail(purple_account_is_connected(account));

	presence = purple_account_get_presence(account);
	status   = purple_presence_get_status(presence, status_id);

	g_return_if_fail(status != NULL);

	va_start(args, status_id);
	purple_status_set_active_with_attrs(status, TRUE, args);
	va_end(args);
}

void
purple_prpl_got_user_idle(PurpleAccount *account, const char *name,
		gboolean idle, time_t idle_time)
{
	PurpleBuddy *buddy;
	PurplePresence *presence;

	g_return_if_fail(account != NULL);
	g_return_if_fail(name    != NULL);
	g_return_if_fail(purple_account_is_connected(account));

	if ((buddy = purple_find_buddy(account, name)) == NULL)
		return;

	presence = purple_buddy_get_presence(buddy);

	purple_presence_set_idle(presence, idle, idle_time);
}

void
purple_prpl_got_user_login_time(PurpleAccount *account, const char *name,
		time_t login_time)
{
	PurpleBuddy *buddy;
	PurplePresence *presence;

	g_return_if_fail(account != NULL);
	g_return_if_fail(name    != NULL);

	if ((buddy = purple_find_buddy(account, name)) == NULL)
		return;

	if (login_time == 0)
		login_time = time(NULL);

	presence = purple_buddy_get_presence(buddy);

	if (purple_presence_get_login_time(presence) != login_time)
	{
		purple_presence_set_login_time(presence, login_time);

		purple_signal_emit(purple_blist_get_handle(), "buddy-got-login-time", buddy);
	}
}

void
purple_prpl_got_user_status(PurpleAccount *account, const char *name,
		const char *status_id, ...)
{
	GSList *list;
	PurpleBuddy *buddy;
	PurplePresence *presence;
	PurpleStatus *status;
	PurpleStatus *old_status;
	va_list args;

	g_return_if_fail(account   != NULL);
	g_return_if_fail(name      != NULL);
	g_return_if_fail(status_id != NULL);
	g_return_if_fail(purple_account_is_connected(account) || purple_account_is_connecting(account));

	if ((buddy = purple_find_buddy(account, name)) == NULL)
		return;

	presence = purple_buddy_get_presence(buddy);
	status   = purple_presence_get_status(presence, status_id);

	g_return_if_fail(status != NULL);

	old_status = purple_presence_get_active_status(presence);

	va_start(args, status_id);
	purple_status_set_active_with_attrs(status, TRUE, args);
	va_end(args);

	list = purple_find_buddies(account, name);
	g_slist_foreach(list, (GFunc)purple_blist_update_buddy_status, old_status);
	g_slist_free(list);

	if (!purple_status_is_online(status))
		serv_got_typing_stopped(purple_account_get_connection(account), name);
}

void purple_prpl_got_user_status_deactive(PurpleAccount *account, const char *name,
					const char *status_id)
{
	PurpleBuddy *buddy;
	PurplePresence *presence;
	PurpleStatus *status;

	g_return_if_fail(account   != NULL);
	g_return_if_fail(name      != NULL);
	g_return_if_fail(status_id != NULL);
	g_return_if_fail(purple_account_is_connected(account) || purple_account_is_connecting(account));

	if ((buddy = purple_find_buddy(account, name)) == NULL)
		return;

	presence = purple_buddy_get_presence(buddy);
	status   = purple_presence_get_status(presence, status_id);

	g_return_if_fail(status != NULL);
	purple_status_set_active(status, FALSE);
}

static void
do_prpl_change_account_status(PurpleAccount *account,
								PurpleStatus *old_status, PurpleStatus *new_status)
{
	PurplePlugin *prpl;
	PurplePluginProtocolInfo *prpl_info;

	if (purple_status_is_online(new_status) &&
		purple_account_is_disconnected(account))
	{
		purple_account_connect(account);
		return;
	}

	if (!purple_status_is_online(new_status))
	{
		if (!purple_account_is_disconnected(account))
			purple_account_disconnect(account);
		return;
	}

	if (purple_account_is_connecting(account))
		/*
		 * We don't need to call the set_status PRPL function because
		 * the PRPL will take care of setting its status during the
		 * connection process.
		 */
		return;

	prpl = purple_find_prpl(purple_account_get_protocol_id(account));

	if (prpl == NULL)
		return;

	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);

	if (!purple_account_is_disconnected(account) && prpl_info->set_status != NULL)
	{
		prpl_info->set_status(account, new_status);
	}
}

void
purple_prpl_change_account_status(PurpleAccount *account,
								PurpleStatus *old_status, PurpleStatus *new_status)
{
	g_return_if_fail(account    != NULL);
	g_return_if_fail(old_status != NULL);
	g_return_if_fail(new_status != NULL);

	do_prpl_change_account_status(account, old_status, new_status);

	purple_signal_emit(purple_accounts_get_handle(), "account-status-changed",
					account, old_status, new_status);
}

GList *
purple_prpl_get_statuses(PurpleAccount *account, PurplePresence *presence)
{
	GList *statuses = NULL;
	const GList *l;
	PurpleStatus *status;

	g_return_val_if_fail(account  != NULL, NULL);
	g_return_val_if_fail(presence != NULL, NULL);

	for (l = purple_account_get_status_types(account); l != NULL; l = l->next)
	{
		status = purple_status_new((PurpleStatusType *)l->data, presence);
		statuses = g_list_prepend(statuses, status);
	}

	statuses = g_list_reverse(statuses);

	return statuses;
}


/**************************************************************************
 * Protocol Plugin Subsystem API
 **************************************************************************/

PurplePlugin *
purple_find_prpl(const char *id)
{
	GList *l;
	PurplePlugin *plugin;

	g_return_val_if_fail(id != NULL, NULL);

	for (l = purple_plugins_get_protocols(); l != NULL; l = l->next) {
		plugin = (PurplePlugin *)l->data;

		if (!strcmp(plugin->info->id, id))
			return plugin;
	}

	return NULL;
}
