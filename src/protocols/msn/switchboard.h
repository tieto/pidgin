/**
 * @file switchboard.h MSN switchboard functions
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
#ifndef _MSN_SWITCHBOARD_H_
#define _MSN_SWITCHBOARD_H_

typedef struct _MsnSwitchBoard MsnSwitchBoard;

#include "servconn.h"
#include "msg.h"
#include "user.h"
#include "buddyicon.h"

struct _MsnSwitchBoard
{
	MsnServConn *servconn;
	MsnUser *user;

	char *auth_key;
	char *session_id;

	gboolean invited;

	struct gaim_conversation *chat;

	gboolean in_use;

	int total_users;

	gboolean msg;
	int msglen;

	int chat_id;
	int trId;

	gboolean hidden;

	MsnBuddyIconXfer *buddy_icon_xfer;
};

/**
 * Creates a new switchboard.
 *
 * @param session The MSN session.
 *
 * @return The new switchboard.
 */
MsnSwitchBoard *msn_switchboard_new(MsnSession *session);

/**
 * Destroys a switchboard.
 *
 * @param swboard The switchboard to destroy.
 */
void msn_switchboard_destroy(MsnSwitchBoard *swboard);

/**
 * Sets the user the switchboard is supposed to connect to.
 *
 * @param swboard The switchboard.
 * @param user    The user.
 */
void msn_switchboard_set_user(MsnSwitchBoard *swboard, MsnUser *user);

/**
 * Returns the user the switchboard is supposed to connect to.
 *
 * @param swboard The switchboard.
 *
 * @return The user.
 */
MsnUser *msn_switchboard_get_user(const MsnSwitchBoard *swboard);

/**
 * Sets the auth key the switchboard must use when connecting.
 *
 * @param swboard The switchboard.
 * @param key     The auth key.
 */
void msn_switchboard_set_auth_key(MsnSwitchBoard *swboard, const char *key);

/**
 * Returns the auth key the switchboard must use when connecting.
 *
 * @param swboard The switchboard.
 *
 * @return The auth key.
 */
const char *msn_switchboard_get_auth_key(const MsnSwitchBoard *swboard);

/**
 * Sets the session ID the switchboard must use when connecting.
 *
 * @param swboard The switchboard.
 * @param id      The session ID.
 */
void msn_switchboard_set_session_id(MsnSwitchBoard *swboard, const char *id);

/**
 * Returns the session ID the switchboard must use when connecting.
 *
 * @param swboard The switchboard.
 *
 * @return The session ID.
 */
const char *msn_switchboard_get_session_id(const MsnSwitchBoard *swboard);

/**
 * Sets whether or not the user was invited to this switchboard.
 *
 * @param swboard The switchboard.
 * @param invite  @c TRUE if invited, @c FALSE otherwise.
 */
void msn_switchboard_set_invited(MsnSwitchBoard *swboard, gboolean invited);

/**
 * Returns whether or not the user was invited to this switchboard.
 *
 * @param swboard The switchboard.
 *
 * @return @c TRUE if invited, @c FALSE otherwise.
 */
gboolean msn_switchboard_is_invited(const MsnSwitchBoard *swboard);

/**
 * Connects to a switchboard.
 *
 * @param swboard The switchboard.
 * @param server  The server.
 * @param port    The port.
 *
 * @return @c TRUE if able to connect, or @c FALSE otherwise.
 */
gboolean msn_switchboard_connect(MsnSwitchBoard *swboard,
								 const char *server, int port);

/**
 * Disconnects from a switchboard.
 *
 * @param swboard The switchboard.
 */
void msn_switchboard_disconnect(MsnSwitchBoard *swboard);

/**
 * Sends a message to a switchboard.
 *
 * @param swboard The switchboard.
 * @param msg     The message to send.
 *
 * @return @c TRUE if successful, or @c FALSE otherwise.
 */
gboolean msn_switchboard_send_msg(MsnSwitchBoard *swboard,
								  MsnMessage *msg);

/**
 * Sends a command to the switchboard.
 *
 * @param swboard The switchboard.
 * @param command The command.
 * @param params  The parameters.
 *
 * @return @c TRUE if successful, or @c FALSE otherwise.
 */
gboolean msn_switchboard_send_command(MsnSwitchBoard *swboard,
									  const char *command,
									  const char *params);

#endif /* _MSN_SWITCHBOARD_H_ */
