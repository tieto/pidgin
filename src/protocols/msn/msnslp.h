/**
 * @file msnslp.h MSNSLP support
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
#ifndef _MSN_SLP_H_
#define _MSN_SLP_H_

typedef struct _MsnSlpSession MsnSlpSession;

#include "msnobject.h"
#include "user.h"

#include "switchboard.h"

struct _MsnSlpSession
{
	gboolean local_initiated;

	MsnSwitchBoard *swboard;

	char *branch;
	char *call_id;

	long session_id;
	long base_id;
	long prev_msg_id;

	size_t offset;

	void *orig_body;
	size_t orig_len;

	guint send_timer;
	FILE *send_fp;

	size_t remaining_size;

	MsnUser *receiver;
	MsnUser *sender;

	MsnMessage *outgoing_msg;
};

/**
 * Creates a MSNSLP session.
 *
 * @param swboard         The switchboard.
 * @param local_initiated TRUE if the session was initiated locally.
 *
 * @return The new MSNSLP session handle.
 */
MsnSlpSession *msn_slp_session_new(MsnSwitchBoard *swboard,
								   gboolean local_initiated);

/**
 * Destroys a MSNSLP session handle.
 *
 * This does not close the connection.
 *
 * @param slpsession The MSNSLP session to destroy.
 */
void msn_slp_session_destroy(MsnSlpSession *slpsession);

/**
 * Notifies the MSNSLP session handle that a message was received.
 *
 * @param slpsession The MSNSLP session.
 * @param msg        The message.
 *
 * @return TRUE if the session was closed, or FALSE otherwise.
 */
gboolean msn_slp_session_msg_received(MsnSlpSession *slpsession,
									  MsnMessage *msg);

/**
 * Sends a message over a MSNSLP session.
 *
 * @param slpsession The MSNSLP session to send the message over.
 * @param msg        The message to send.
 */
void msn_slp_session_send_msg(MsnSlpSession *session, MsnMessage *msg);

/**
 * Sends an acknowledgement message over a MSNSLP session for the
 * specified message.
 *
 * @param slpsession The MSNSLP session to send the acknowledgement over.
 * @param acked_msg  The message to acknowledge.
 */
void msn_slp_session_send_ack(MsnSlpSession *session, MsnMessage *acked_msg);

/**
 * Requests a User Display image over a MSNSLP session.
 *
 * @param slpsession The MSNSLP session to request the image over.
 * @param localUser  The local user initiating the invite.
 * @param remoteUser The remote user the invite is sent to.
 * @param obj        The MSNObject representing the user display info.
 */
void msn_slp_session_request_user_display(MsnSlpSession *session,
										  MsnUser *localUser,
										  MsnUser *remoteUser,
										  const MsnObject *obj);

/**
 * Processes application/x-msnmsgrp2p messages.
 *
 * @param servconn The server connection.
 * @param msg      The message.
 *
 * @return TRUE
 */
gboolean msn_p2p_msg(MsnServConn *servconn, MsnMessage *msg);

#endif /* _MSN_SLP_H_ */
