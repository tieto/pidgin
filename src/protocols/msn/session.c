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

#include "slplink.h"

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
		msn_switchboard_request(swboard);
		msn_switchboard_request_add_user(swboard, username);
		swboard->im_user = g_strdup(username);
	}

	return swboard;
}
