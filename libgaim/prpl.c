/*
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
gaim_prpl_got_account_idle(GaimAccount *account, gboolean idle,
						   time_t idle_time)
{
	g_return_if_fail(account != NULL);
	g_return_if_fail(gaim_account_is_connected(account));

	gaim_presence_set_idle(gaim_account_get_presence(account),
						   idle, idle_time);
}

void
gaim_prpl_got_account_login_time(GaimAccount *account, time_t login_time)
{
	GaimPresence *presence;

	g_return_if_fail(account != NULL);
	g_return_if_fail(gaim_account_is_connected(account));

	if (login_time == 0)
		login_time = time(NULL);

	presence = gaim_account_get_presence(account);

	gaim_presence_set_login_time(presence, login_time);
}

void
gaim_prpl_got_account_status(GaimAccount *account, const char *status_id, ...)
{
	GaimPresence *presence;
	GaimStatus *status;
	va_list args;

	g_return_if_fail(account   != NULL);
	g_return_if_fail(status_id != NULL);
	g_return_if_fail(gaim_account_is_connected(account));

	presence = gaim_account_get_presence(account);
	status   = gaim_presence_get_status(presence, status_id);

	g_return_if_fail(status != NULL);

	va_start(args, status_id);
	gaim_status_set_active_with_attrs(status, TRUE, args);
	va_end(args);
}

void
gaim_prpl_got_user_idle(GaimAccount *account, const char *name,
		gboolean idle, time_t idle_time)
{
	GaimBuddy *buddy;
	GaimPresence *presence;

	g_return_if_fail(account != NULL);
	g_return_if_fail(name    != NULL);
	g_return_if_fail(gaim_account_is_connected(account));

	if ((buddy = gaim_find_buddy(account, name)) == NULL)
		return;

	presence = gaim_buddy_get_presence(buddy);

	gaim_presence_set_idle(presence, idle, idle_time);
}

void
gaim_prpl_got_user_login_time(GaimAccount *account, const char *name,
		time_t login_time)
{
	GaimBuddy *buddy;
	GaimPresence *presence;

	g_return_if_fail(account != NULL);
	g_return_if_fail(name    != NULL);

	if ((buddy = gaim_find_buddy(account, name)) == NULL)
		return;

	if (login_time == 0)
		login_time = time(NULL);

	presence = gaim_buddy_get_presence(buddy);

	if (gaim_presence_get_login_time(presence) != login_time)
	{
		gaim_presence_set_login_time(presence, login_time);

		gaim_signal_emit(gaim_blist_get_handle(), "buddy-got-login-time", buddy);
	}
}

void
gaim_prpl_got_user_status(GaimAccount *account, const char *name,
		const char *status_id, ...)
{
	GSList *list;
	GaimBuddy *buddy;
	GaimPresence *presence;
	GaimStatus *status;
	GaimStatus *old_status;
	va_list args;

	g_return_if_fail(account   != NULL);
	g_return_if_fail(name      != NULL);
	g_return_if_fail(status_id != NULL);
	g_return_if_fail(gaim_account_is_connected(account) || gaim_account_is_connecting(account));

	if ((buddy = gaim_find_buddy(account, name)) == NULL)
		return;

	presence = gaim_buddy_get_presence(buddy);
	status   = gaim_presence_get_status(presence, status_id);

	g_return_if_fail(status != NULL);

	old_status = gaim_presence_get_active_status(presence);

	va_start(args, status_id);
	gaim_status_set_active_with_attrs(status, TRUE, args);
	va_end(args);

	list = gaim_find_buddies(account, name);
	g_slist_foreach(list, (GFunc)gaim_blist_update_buddy_status, old_status);
	g_slist_free(list);

	if (!gaim_status_is_online(status))
		serv_got_typing_stopped(gaim_account_get_connection(account), name);
}

static void
do_prpl_change_account_status(GaimAccount *account,
								GaimStatus *old_status, GaimStatus *new_status)
{
	GaimPlugin *prpl;
	GaimPluginProtocolInfo *prpl_info;

	if (gaim_status_is_online(new_status) &&
		gaim_account_is_disconnected(account))
	{
		gaim_account_connect(account);
		return;
	}

	if (!gaim_status_is_online(new_status))
	{
		if (!gaim_account_is_disconnected(account))
			gaim_account_disconnect(account);
		return;
	}

	if (gaim_account_is_connecting(account))
		/*
		 * We don't need to call the set_status PRPL function because
		 * the PRPL will take care of setting its status during the
		 * connection process.
		 */
		return;

	prpl = gaim_find_prpl(gaim_account_get_protocol_id(account));

	if (prpl == NULL)
		return;

	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);

	if (!gaim_account_is_disconnected(account) && prpl_info->set_status != NULL)
	{
		prpl_info->set_status(account, new_status);
	}
}

void
gaim_prpl_change_account_status(GaimAccount *account,
								GaimStatus *old_status, GaimStatus *new_status)
{
	g_return_if_fail(account    != NULL);
	g_return_if_fail(old_status != NULL);
	g_return_if_fail(new_status != NULL);

	do_prpl_change_account_status(account, old_status, new_status);

	gaim_signal_emit(gaim_accounts_get_handle(), "account-status-changed",
					account, old_status, new_status);
}

GList *
gaim_prpl_get_statuses(GaimAccount *account, GaimPresence *presence)
{
	GaimPlugin *prpl;
	GaimPluginProtocolInfo *prpl_info;
	GList *statuses = NULL;
	GList *l, *list;
	GaimStatus *status;

	g_return_val_if_fail(account  != NULL, NULL);
	g_return_val_if_fail(presence != NULL, NULL);

	prpl = gaim_find_prpl(gaim_account_get_protocol_id(account));

	if (prpl == NULL)
		return NULL;

	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);
	if (prpl_info == NULL || prpl_info->status_types == NULL)
		return NULL;

	for (l = list = prpl_info->status_types(account); l != NULL; l = l->next)
	{
		status = gaim_status_new((GaimStatusType *)l->data, presence);
		statuses = g_list_append(statuses, status);
	}

	g_list_free(list);

	return statuses;
}


/**************************************************************************
 * Protocol Plugin Subsystem API
 **************************************************************************/

GaimPlugin *
gaim_find_prpl(const char *id)
{
	GList *l;
	GaimPlugin *plugin;

	g_return_val_if_fail(id != NULL, NULL);

	for (l = gaim_plugins_get_protocols(); l != NULL; l = l->next) {
		plugin = (GaimPlugin *)l->data;

		if (!strcmp(plugin->info->id, id))
			return plugin;
	}

	return NULL;
}
