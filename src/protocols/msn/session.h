/**
 * @file session.h MSN session functions
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
#ifndef _MSN_SESSION_H_
#define _MSN_SESSION_H_

typedef struct _MsnSession MsnSession;

#include "servconn.h"
#include "switchboard.h"
#include "user.h"

struct _MsnSession
{
	struct gaim_account *account;

	char *dispatch_server;
	int dispatch_port;

	gboolean connected;

	MsnServConn *dispatch_conn;
	MsnServConn *notification_conn;

	unsigned int trId;

	MsnUsers *users;

	GList *switches;
	GHashTable *group_names; /* ID -> name */
	GHashTable *group_ids;   /* Name -> ID */

	struct
	{
		GSList *forward;
		GSList *reverse;
		GSList *allow;
		GSList *block;

	} lists;

	struct
	{
		char *kv;
		char *sid;
		char *mspauth;
		unsigned long sl;
		char *file;

	} passport_info;

	/* You have no idea how much I hate all that is below. */
	GaimPlugin *prpl;

	/* For moving buddies from one group to another. Ugh. */
	gboolean moving_buddy;
	char *dest_group_name;
};

/**
 * Creates an MSN session.
 *
 * @param account The account.
 * @param server  The dispatch server.
 * @param port    The dispatch port.
 *
 * @return The new MSN session.
 */
MsnSession *msn_session_new(struct gaim_account *account,
							const char *server, int port);

/**
 * Destroys an MSN session.
 *
 * @param session The MSN session to destroy.
 */
void msn_session_destroy(MsnSession *session);

/**
 * Connects to and initiates an MSN session.
 *
 * @param session The MSN session.
 *
 * @return @c TRUE on success, @c FALSE on failure.
 */
gboolean msn_session_connect(MsnSession *session);

/**
 * Disconnects from an MSN session.
 *
 * @param session The MSN session.
 */
void msn_session_disconnect(MsnSession *session);

/**
 * Opens a new switchboard connection.
 *
 * @param session The MSN session.
 *
 * @return The new switchboard connection.
 */
MsnSwitchBoard *msn_session_open_switchboard(MsnSession *session);

/**
 * Finds a switch with the given passport.
 *
 * @param session  The MSN session.
 * @param passport The passport to search for.
 *
 * @return The switchboard, if found.
 */
MsnSwitchBoard *msn_session_find_switch_with_passport(
		const MsnSession *session, const char *passport);

/**
 * Finds a switchboard with the given chat ID.
 *
 * @param session The MSN session.
 * @param chat_id The chat ID to search for.
 *
 * @return The switchboard, if found.
 */
MsnSwitchBoard *msn_session_find_switch_with_id(const MsnSession *session,
												int chat_id);

/**
 * Finds the first unused switchboard.
 *
 * @param session  The MSN session.
 *
 * @return The first unused, writable switchboard, if found.
 */
MsnSwitchBoard *msn_session_find_unused_switch(const MsnSession *session);

#endif /* _MSN_SESSION_H_ */
