/**
 * @file session.c MSN session functions
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
#include "session.h"
#include "dispatch.h"

MsnSession *
msn_session_new(struct gaim_account *account, const char *server, int port)
{
	MsnSession *session;

	g_return_val_if_fail(account != NULL, NULL);

	session = g_new0(MsnSession, 1);

	session->account         = account;
	session->dispatch_server = g_strdup(server);
	session->dispatch_port   = port;

	session->users  = msn_users_new();
	session->groups = msn_groups_new();

	return session;
}

void
msn_session_destroy(MsnSession *session)
{
	g_return_if_fail(session != NULL);

	if (session->connected)
		msn_session_disconnect(session);

	if (session->dispatch_server != NULL)
		g_free(session->dispatch_server);

	while (session->switches != NULL)
		msn_switchboard_destroy(session->switches->data);

	while (session->lists.forward)
		msn_user_destroy(session->lists.forward->data);

	g_slist_free(session->lists.allow);
	g_slist_free(session->lists.block);

	msn_groups_destroy(session->groups);
	msn_users_destroy(session->users);

	g_free(session->passport_info.kv);
	g_free(session->passport_info.sid);
	g_free(session->passport_info.mspauth);
	g_free(session->passport_info.file);

	g_free(session);
}

gboolean
msn_session_connect(MsnSession *session)
{
	g_return_val_if_fail(session != NULL, FALSE);
	g_return_val_if_fail(!session->connected, TRUE);

	session->connected = TRUE;

	session->dispatch_conn = msn_dispatch_new(session,
											  session->dispatch_server,
											  session->dispatch_port);

	if (msn_servconn_connect(session->dispatch_conn))
		return TRUE;

	return FALSE;
}

void
msn_session_disconnect(MsnSession *session)
{
	g_return_if_fail(session != NULL);
	g_return_if_fail(session->connected);

	if (session->dispatch_conn != NULL) {
		msn_servconn_destroy(session->dispatch_conn);
		session->dispatch_conn = NULL;
	}

	while (session->switches != NULL) {
		MsnSwitchBoard *board = (MsnSwitchBoard *)session->switches->data;

		msn_switchboard_destroy(board);
	}

	if (session->notification_conn != NULL) {
		msn_servconn_destroy(session->notification_conn);
		session->notification_conn = NULL;
	}
}

MsnSwitchBoard *
msn_session_open_switchboard(MsnSession *session)
{
	MsnSwitchBoard *swboard;

	g_return_val_if_fail(session != NULL, NULL);

	if (msn_servconn_send_command(session->notification_conn,
								  "XFR", "SB") < 0) {
		return NULL;
	}

	swboard = msn_switchboard_new(session);

	return swboard;
}

MsnSwitchBoard *
msn_session_find_switch_with_passport(const MsnSession *session,
									  const char *passport)
{
	GList *l;
	MsnSwitchBoard *swboard;

	g_return_val_if_fail(session != NULL, NULL);
	g_return_val_if_fail(passport != NULL, NULL);

	for (l = session->switches; l != NULL; l = l->next) {
		swboard = (MsnSwitchBoard *)l->data;

		if (!swboard->hidden &&
			!g_ascii_strcasecmp(passport,
								msn_user_get_passport(swboard->user))) {
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

	for (l = session->switches; l != NULL; l = l->next) {
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

	for (l = session->switches; l != NULL; l = l->next) {
		swboard = (MsnSwitchBoard *)l->data;

		if (!swboard->in_use)
			return swboard;
	}

	return NULL;
}

