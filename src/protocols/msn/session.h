/**
 * @file session.h MSN session functions
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
#ifndef _MSN_SESSION_H_
#define _MSN_SESSION_H_

typedef struct _MsnSession MsnSession;

#include "sslconn.h"

#include "notification.h"
#include "switchboard.h"
#include "user.h"
#include "group.h"

#include "cmdproc.h"
#include "nexus.h"
#include "httpconn.h"

#include "userlist.h"
#include "sync.h"

struct _MsnSession
{
	GaimAccount *account;
	MsnUser *user;
	int state;

	guint protocol_ver;

	char *dispatch_host;
	int dispatch_port;

	gboolean connected;
	gboolean logged_in; /**< A temporal flag to ignore local buddy list adds. */
	gboolean destroying; /**< A flag that states if the session is being destroyed. */

	MsnNotification *notification;
	MsnNexus *nexus;

	gboolean http_method;
	gint http_poll_timer;

	MsnUserList *userlist;
	MsnUserList *sync_userlist;

	int servconns_count; /**< The count of server connections. */
	GList *switches; /**< The list of all the switchboards. */
	GList *directconns; /**< The list of all the directconnections. */

	int conv_seq;

	struct
	{
		char *kv;
		char *sid;
		char *mspauth;
		unsigned long sl;
		char *file;
		char *client_ip;
		int client_port;

	} passport_info;

	/* You have no idea how much I hate all that is below. */
	/* shx: What? ;) */

	MsnSync *sync;

	GList *slplinks;
};

/**
 * Creates an MSN session.
 *
 * @param account The account.
 * @param host    The dispatch server host.
 * @param port    The dispatch server port.
 *
 * @return The new MSN session.
 */
MsnSession *msn_session_new(GaimAccount *account,
							const char *host, int port,
							gboolean http_method);

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
 * Finds a switchboard with the given chat ID.
 *
 * @param session The MSN session.
 * @param chat_id The chat ID to search for.
 *
 * @return The switchboard, if found.
 */
MsnSwitchBoard *msn_session_find_switch_with_id(const MsnSession *session,
												int chat_id);

MsnSwitchBoard *msn_session_find_swboard(MsnSession *session,
										 const char *username);
MsnSwitchBoard *msn_session_get_swboard(MsnSession *session,
										const char *username);

void msn_session_finish_login(MsnSession *session);

#endif /* _MSN_SESSION_H_ */
