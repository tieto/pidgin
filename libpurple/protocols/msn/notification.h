/**
 * @file notification.h Notification server functions
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
#ifndef _MSN_NOTIFICATION_H_
#define _MSN_NOTIFICATION_H_

typedef struct _MsnNotification MsnNotification;

#include "session.h"
#include "servconn.h"
#include "cmdproc.h"

struct _MsnNotification
{
	MsnSession *session;
	MsnCmdProc *cmdproc;
	MsnServConn *servconn;

	gboolean in_use;
};

#include "state.h"

void msn_notification_end(void);
void msn_notification_init(void);

void msn_notification_add_buddy(MsnNotification *notification,
								const char *list, const char *who,
								const char *store_name, int group_id);
void msn_notification_rem_buddy(MsnNotification *notification,
								const char *list, const char *who,
								int group_id);
MsnNotification *msn_notification_new(MsnSession *session);
void msn_notification_destroy(MsnNotification *notification);
gboolean msn_notification_connect(MsnNotification *notification,
							  const char *host, int port);
void msn_notification_disconnect(MsnNotification *notification);

/**
 * Closes a notification.
 *
 * It's first closed, and then disconnected.
 * 
 * @param notification The notification object to close.
 */
void msn_notification_close(MsnNotification *notification);

void msn_got_login_params(MsnSession *session, const char *login_params);

#endif /* _MSN_NOTIFICATION_H_ */
