/**
 * @file session.c MSN session functions
 *
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
 */
#include "msn.h"
#include "session.h"
#include "notification.h"

#include "slplink.h"

#include "dialog.h"

MsnSession *
msn_session_new(GaimAccount *account, const char *host, int port,
				gboolean http_method)
{
	MsnSession *session;

	g_return_val_if_fail(account != NULL, NULL);

	session = g_new0(MsnSession, 1);

	session->account       = account;
	session->dispatch_host = g_strdup(host);
	session->dispatch_port = port;
	session->http_method   = http_method;

	session->notification = msn_notification_new(session);
	session->userlist = msn_userlist_new(session);
	session->sync_userlist = msn_userlist_new(session);

	session->protocol_ver = 9;

	return session;
}

void
msn_session_destroy(MsnSession *session)
{
	g_return_if_fail(session != NULL);

	if (session->connected)
		msn_session_disconnect(session);

	if (session->dispatch_host != NULL)
		g_free(session->dispatch_host);

	if (session->notification != NULL)
		msn_notification_destroy(session->notification);

	while (session->switches != NULL)
		msn_switchboard_destroy(session->switches->data);

	while (session->slplinks != NULL)
		msn_slplink_destroy(session->slplinks->data);

	msn_userlist_destroy(session->userlist);

	if (session->sync_userlist != NULL)
		msn_userlist_destroy(session->sync_userlist);

	if (session->passport_info.kv != NULL)
		g_free(session->passport_info.kv);

	if (session->passport_info.sid != NULL)
		g_free(session->passport_info.sid);

	if (session->passport_info.mspauth != NULL)
		g_free(session->passport_info.mspauth);

	if (session->passport_info.file != NULL)
		g_free(session->passport_info.file);

	if (session->sync != NULL)
		msn_sync_destroy(session->sync);

	if (session->nexus != NULL)
		msn_nexus_destroy(session->nexus);

	g_free(session);
}

gboolean
msn_session_connect(MsnSession *session)
{
	g_return_val_if_fail(session != NULL, FALSE);
	g_return_val_if_fail(!session->connected, TRUE);

	session->connected = TRUE;

	if (msn_notification_connect(session->notification,
								 session->dispatch_host,
								 session->dispatch_port))
	{
		return TRUE;
	}

	return FALSE;
}

void
msn_session_disconnect(MsnSession *session)
{
	g_return_if_fail(session != NULL);
	g_return_if_fail(session->connected);

	while (session->switches != NULL)
		msn_switchboard_destroy(session->switches->data);

	if (session->notification != NULL)
		msn_notification_disconnect(session->notification);
}

/* TODO: This must go away when conversation is redesigned */
MsnSwitchBoard *
msn_session_find_swboard(MsnSession *session, const char *username)
{
	GList *l;

	g_return_val_if_fail(session  != NULL, NULL);
	g_return_val_if_fail(username != NULL, NULL);

	for (l = session->switches; l != NULL; l = l->next)
	{
		MsnSwitchBoard *swboard;

		swboard = l->data;

		if (swboard->im_user != NULL)
			if (!strcmp(username, swboard->im_user))
				return swboard;
	}

	return NULL;
}

MsnSwitchBoard *
msn_session_find_switch_with_id(const MsnSession *session, int chat_id)
{
	GList *l;

	g_return_val_if_fail(session != NULL, NULL);
	g_return_val_if_fail(chat_id >= 0,    NULL);

	for (l = session->switches; l != NULL; l = l->next)
	{
		MsnSwitchBoard *swboard;

		swboard = l->data;

		if (swboard->chat_id == chat_id)
			return swboard;
	}

	return NULL;
}

MsnSwitchBoard *
msn_session_get_swboard(MsnSession *session, const char *username)
{
	MsnSwitchBoard *swboard;

	swboard = msn_session_find_swboard(session, username);

	if (swboard == NULL)
	{
		swboard = msn_switchboard_new(session);
		swboard->im_user = g_strdup(username);
		msn_switchboard_request(swboard);
		msn_switchboard_request_add_user(swboard, username);
	}

	return swboard;
}

static void
msn_session_sync_users(MsnSession *session)
{
	GList *l;

	l = session->sync_userlist->users;

	while (l != NULL)
	{
		MsnUser *local_user;

		local_user = (MsnUser *)l->data;

		if (local_user->passport != NULL)
		{
			MsnUser *remote_user;

			remote_user = msn_userlist_find_user(session->userlist,
												 local_user->passport);

			if (remote_user == NULL ||
				((local_user->list_op & ( 1 << MSN_LIST_FL)) &&
				 !(remote_user->list_op & ( 1 << MSN_LIST_FL))))
			{
				/* The user was not on the server list */
				msn_show_sync_issue(session, local_user->passport, NULL);
			}
			else
			{
				GList *l;

				for (l = local_user->group_ids; l != NULL; l = l->next)
				{
					const char *group_name;
					int gid;
					gboolean found = FALSE;
					GList *l2;

					group_name =
						msn_userlist_find_group_name(local_user->userlist,
													 GPOINTER_TO_INT(l->data));

					gid = msn_userlist_find_group_id(remote_user->userlist,
													 group_name);

					for (l2 = remote_user->group_ids; l2 != NULL; l2 = l2->next)
					{
						if (GPOINTER_TO_INT(l2->data) == gid)
						{
							found = TRUE;
							break;
						}
					}

					if (!found)
					{
						/* The user was not on that group on the server list */
						msn_show_sync_issue(session, local_user->passport,
											group_name);
					}
				}
			}
		}

		l = l->next;
	}

	msn_userlist_destroy(session->sync_userlist);
	session->sync_userlist = NULL;
}

void
msn_session_finish_login(MsnSession *session)
{
	GaimAccount *account;
	GaimConnection *gc;

	account = session->account;
	gc = gaim_account_get_connection(account);

	msn_user_set_buddy_icon(session->user,
							gaim_account_get_buddy_icon(session->account));

	msn_change_status(session, MSN_ONLINE);

	gaim_connection_set_state(gc, GAIM_CONNECTED);
	session->logged_in = TRUE;

	/* Sync users */
	msn_session_sync_users(session);

	serv_finish_login(gc);
}
