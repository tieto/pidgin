/**
 * @file buddyicon.h Buddy icon support
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
#ifndef _MSN_BUDDY_ICON_H_
#define _MSN_BUDDY_ICON_H_

typedef struct _MsnBuddyIconXfer MsnBuddyIconXfer;

#include "servconn.h"

/**
 * State of a buddy icon transfer.
 */
struct _MsnBuddyIconXfer
{
	MsnUser *user;         /**< The user on the other end of the transfer. */

	gboolean sending;      /**< True if sending this icon.                 */

	size_t bytes_xfer;     /**< The current bytes sent or retrieved.       */
	size_t total_size;     /**< The total size of the base64 icon data.    */
	size_t file_size;      /**< The file size of the actual icon.          */

	char *md5sum;          /**< The MD5SUM of the icon.                    */
	char *data;            /**< The buddy icon data.                       */
};

/**
 * Creates an MsnBuddyIconXfer structure.
 *
 * @return The MsnBuddyIconXfer structure.
 */
MsnBuddyIconXfer *msn_buddy_icon_xfer_new(void);

/**
 * Destroys an MsnBuddyIconXfer structure.
 *
 * @param The buddy icon structure.
 */
void msn_buddy_icon_xfer_destroy(MsnBuddyIconXfer *xfer);

/**
 * Processed application/x-buddyicon messages.
 *
 * @param servconn The server connection.
 * @param msg      The message.
 *
 * @return TRUE
 */
gboolean msn_buddy_icon_msg(MsnServConn *servconn, MsnMessage *msg);

/**
 * Sends a buddy icon invitation message.
 *
 * @param swboard The switchboard to send to.
 */
void msn_buddy_icon_invite(MsnSwitchBoard *swboard);

#endif /* _MSN_BUDDY_ICON_H_ */
