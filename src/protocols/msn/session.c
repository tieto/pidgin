/**
 * @file session.c MSN session functions
 *
 * gaim
 *
 * Copyright (C) 2003-2004 Christian Hammond <chipx86@gnupdate.org>
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

MsnSession *
msn_session_new(GaimAccount *account, const char *host, int port)
{
	MsnSession *session;

	g_return_val_if_fail(account != NULL, NULL);

	session = g_new0(MsnSession, 1);

	session->account       = account;
	session->dispatch_host = g_strdup(host);
	session->dispatch_port = port;

	session->away_state = NULL;

	session->users  = msn_users_new();
	session->groups = msn_groups_new();

#ifdef HAVE_SSL
	session->protocol_ver = 9;
#else
	session->protocol_ver = 7;
#endif

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

	while (session->switches != NULL)
		msn_switchboard_destroy(session->switches->data);

	while (session->lists.forward)
	{
		MsnUser *user = (MsnUser *)session->lists.forward->data;

		msn_user_destroy(user);

		session->lists.forward = g_slist_remove(session->lists.forward, user);
	}

	if (session->lists.allow != NULL)
		g_slist_free(session->lists.allow);

	if (session->lists.block != NULL)
		g_slist_free(session->lists.block);

	msn_groups_destroy(session->groups);
	msn_users_destroy(session->users);

	if (session->passport_info.kv != NULL)
		g_free(session->passport_info.kv);

	if (session->passport_info.sid != NULL)
		g_free(session->passport_info.sid);

	if (session->passport_info.mspauth != NULL)
		g_free(session->passport_info.mspauth);

	if (session->passport_info.file != NULL)
		g_free(session->passport_info.file);

	if (session->away_state != NULL)
		g_free(session->away_state);

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

	session->notification_conn = msn_notification_new(session);

	if (msn_notification_connect(session->notification_conn,
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
	{
		MsnSwitchBoard *board = (MsnSwitchBoard *)session->switches->data;

		msn_switchboard_destroy(board);
	}

	if (session->notification_conn != NULL)
	{
		msn_servconn_destroy(session->notification_conn);
		session->notification_conn = NULL;
	}
}

MsnSwitchBoard *
msn_session_open_switchboard(MsnSession *session)
{
	MsnSwitchBoard *swboard;
	MsnCmdProc *cmdproc;

	g_return_val_if_fail(session != NULL, NULL);

	cmdproc = session->notification_conn->cmdproc;

	msn_cmdproc_send(cmdproc, "XFR", "%s", "SB");

	if (cmdproc->error)
		return NULL;

	swboard = msn_switchboard_new(session);

	return swboard;
}

gboolean
msn_session_change_status(MsnSession *session, const char *state)
{
	MsnCmdProc *cmdproc;
	MsnUser *user;
	MsnObject *msnobj;

	g_return_val_if_fail(session != NULL, FALSE);
	g_return_val_if_fail(state   != NULL, FALSE);

	user = session->user;
	msnobj = msn_user_get_object(user);

	if (state != session->away_state)
	{
		if (session->away_state != NULL)
			g_free(session->away_state);

		session->away_state = g_strdup(state);
	}

	cmdproc = session->notification_conn->cmdproc;

	if (msnobj == NULL)
	{
		msn_cmdproc_send(cmdproc, "CHG", "%s %d", state, MSN_CLIENT_ID);
	}
	else
	{
		char *msnobj_str = msn_object_to_string(msnobj);

		msn_cmdproc_send(cmdproc, "CHG", "%s %d %s", state, MSN_CLIENT_ID,
						 gaim_url_encode(msnobj_str));

		g_free(msnobj_str);
	}

	return TRUE;
}

MsnSwitchBoard *
msn_session_find_switch_with_passport(const MsnSession *session,
									  const char *passport)
{
	GList *l;
	MsnSwitchBoard *swboard;

	g_return_val_if_fail(session  != NULL, NULL);
	g_return_val_if_fail(passport != NULL, NULL);

	for (l = session->switches; l != NULL; l = l->next)
	{
		swboard = (MsnSwitchBoard *)l->data;

		if (!swboard->hidden && !swboard->chat_id &&
			!g_ascii_strcasecmp(passport,
								msn_user_get_passport(swboard->user)))
		{
			return swboard;
		}
	}

	return NULL;
}

MsnSwitchBoard *
msn_session_find_switch_with_id(const MsnSession *session, int chat_id)
{
	GList *l;
	MsnSwitchBoard *swboard;

	g_return_val_if_fail(session != NULL, NULL);
	g_return_val_if_fail(chat_id > 0, NULL);

	for (l = session->switches; l != NULL; l = l->next)
	{
		swboard = (MsnSwitchBoard *)l->data;

		if (swboard->chat_id == chat_id)
			return swboard;
	}

	return NULL;
}

MsnSwitchBoard *
msn_session_find_unused_switch(const MsnSession *session)
{
	GList *l;
	MsnSwitchBoard *swboard;

	g_return_val_if_fail(session != NULL, NULL);

	for (l = session->switches; l != NULL; l = l->next)
	{
		swboard = (MsnSwitchBoard *)l->data;

		if (!swboard->in_use)
			return swboard;
	}

	return NULL;
}
