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

typedef struct
{
	long session_id;
	long id;
	long offset;
	long total_size;
	long length;
	long flags;
	long prev_id;
	long prev_f9;
	long prev_total_size;

} MsnSlpHeader;

typedef struct
{
	long app_id;

} MsnSlpFooter;

#include "switchboard.h"

typedef struct
{
	gboolean local_initiated;

	MsnSwitchBoard *swboard;

	int session_id;
	int prev_msg_id;

} MsnSlpSession;

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
 * Sends a message over a MSNSLP session.
 *
 * @param slpsession The MSNSLP session to send the message over.
 * @param msg        The message to send.
 */
void msn_slp_session_send_msg(MsnSlpSession *session, MsnMessage *msg);

#endif /* _MSN_SLP_H_ */
