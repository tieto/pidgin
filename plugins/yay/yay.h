/*
 * libyay
 *
 * Copyright (C) 2001 Eric Warmenhoven <warmenhoven@yahoo.com>
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
 *
 */

#ifndef _LIBYAY_H
#define _LIBYAY_H

#include <glib.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/types.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

struct yahoo_session;

#define YAHOO_AUTH_HOST "msg.edit.yahoo.com"
#define YAHOO_AUTH_PORT 80
#define YAHOO_PAGER_HOST "cs.yahoo.com"
#define YAHOO_PAGER_PORT 5050
#define YAHOO_DATA_HOST YAHOO_AUTH_HOST
#define YAHOO_DATA_PORT YAHOO_AUTH_PORT

enum yahoo_status {
	YAHOO_STATUS_AVAILABLE,
	YAHOO_STATUS_BRB,
	YAHOO_STATUS_BUSY,
	YAHOO_STATUS_NOTATHOME,
	YAHOO_STATUS_NOTATDESK,
	YAHOO_STATUS_NOTINOFFICE,
	YAHOO_STATUS_ONPHONE,
	YAHOO_STATUS_ONVACATION,
	YAHOO_STATUS_OUTTOLUNCH,
	YAHOO_STATUS_STEPPEDOUT,
	YAHOO_STATUS_INVISIBLE = 12,
	YAHOO_STATUS_CUSTOM = 99,
	YAHOO_STATUS_IDLE = 999
};

struct yahoo_session *yahoo_new();
int yahoo_connect(struct yahoo_session *session, const char *host, int port);
int yahoo_send_login(struct yahoo_session *session, const char *name, const char *password);
int yahoo_major_connect(struct yahoo_session *session, const char *host, int port);
int yahoo_finish_logon(struct yahoo_session *session, enum yahoo_status status);
int yahoo_logoff(struct yahoo_session *session);
int yahoo_disconnect(struct yahoo_session *session);
int yahoo_delete(struct yahoo_session *session);

int yahoo_activate_id(struct yahoo_session *session, char *id);
int yahoo_deactivate_id(struct yahoo_session *session, char *id);
int yahoo_send_message(struct yahoo_session *session, const char *id, const char *user, const char *msg);
int yahoo_away(struct yahoo_session *session, enum yahoo_status stats, char *msg);
int yahoo_back(struct yahoo_session *session, enum yahoo_status stats, char *msg);
int yahoo_ping(struct yahoo_session *session);

int yahoo_add_buddy(struct yahoo_session *session, const char *active_id,
		const char *group, const char *buddy, const char *message);
int yahoo_remove_buddy(struct yahoo_session *session, const char *active_id,
		const char *group, const char *buddy, const char *message);

extern void (*yahoo_socket_notify)(struct yahoo_session *session, int socket, int type, gboolean status);
#define YAHOO_SOCKET_READ  1
#define YAHOO_SOCKET_WRITE 2
void yahoo_socket_handler(struct yahoo_session *session, int socket, int type);

extern void (*yahoo_print)(struct yahoo_session *session, int level, const char *log);
#define YAHOO_LOG_DEBUG    4
#define YAHOO_LOG_NOTICE   3
#define YAHOO_LOG_WARNING  2
#define YAHOO_LOG_ERROR    1
#define YAHOO_LOG_CRITICAL 0

typedef int (*yahoo_callback)(struct yahoo_session *session, ...);
void yahoo_add_handler(struct yahoo_session *session, int type, yahoo_callback function);
#define YAHOO_HANDLE_DISCONNECT   0
#define YAHOO_HANDLE_AUTHCONNECT  1
#define YAHOO_HANDLE_BADPASSWORD  2
#define YAHOO_HANDLE_LOGINCOOKIE  3
#define YAHOO_HANDLE_MAINCONNECT  4
#define YAHOO_HANDLE_ONLINE       5
#define YAHOO_HANDLE_NEWMAIL      6
#define YAHOO_HANDLE_MESSAGE      7
#define YAHOO_HANDLE_BOUNCE       8
#define YAHOO_HANDLE_STATUS       9
#define YAHOO_HANDLE_ACTIVATE    10
#define YAHOO_HANDLE_MAX         11

struct callback {
	yahoo_callback function;
	void *data;
};

struct yahoo_group {
	char *name;
	char **buddies;
};

struct yahoo_session {
	void *user_data;
	struct callback callbacks[YAHOO_HANDLE_MAX];

	char *name;

	char *cookie;
	char *login_cookie;
	GList *connlist;

	GList *groups;
	GList *ignored;

	char **identities;
	int mail;
	char *login;
};

#endif
