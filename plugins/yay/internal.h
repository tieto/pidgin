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

#ifndef _LIBYAY_INTERNAL_H
#define _LIBYAY_INTERNAL_H

#include "yay.h"

#define YAHOO_CONN_STATUS_RESOLVERR  0x0040
#define YAHOO_CONN_STATUS_INPROGRESS 0x0100

#define YAHOO_USER_AGENT "Mozilla/4.6 (libyay/1.0)"

#define YAHOO_PRINT(x, y, z) if (yahoo_print) (*yahoo_print)(x, y, z)
#define CALLBACK(x, y, ...) if (x->callbacks[y].function) (*x->callbacks[y].function)(x, ##__VA_ARGS__)

struct yahoo_conn {
	int type;
	int socket;
	int magic_id;
	char *txqueue;
};

#define YAHOO_CONN_TYPE_AUTH 1
#define YAHOO_CONN_TYPE_MAIN 2
#define YAHOO_CONN_TYPE_DUMB 3

char *yahoo_urlencode(const char *);
struct yahoo_conn *yahoo_new_conn(struct yahoo_session *, int, const char *, int);
struct yahoo_conn *yahoo_getconn_type(struct yahoo_session *, int);
struct yahoo_conn *yahoo_find_conn(struct yahoo_session *, int);
int yahoo_write(struct yahoo_session *, struct yahoo_conn *, void *, int);
int yahoo_write_cmd(struct yahoo_session *, struct yahoo_conn *, int, const char *, void *, guint);
void yahoo_close(struct yahoo_session *, struct yahoo_conn *);

#define YAHOO_SERVICE_LOGON            1
#define YAHOO_SERVICE_LOGOFF           2
#define YAHOO_SERVICE_ISAWAY           3
#define YAHOO_SERVICE_ISBACK           4
#define YAHOO_SERVICE_IDLE             5
#define YAHOO_SERVICE_MESSAGE          6
#define YAHOO_SERVICE_IDACT            7
#define YAHOO_SERVICE_IDDEACT          8
#define YAHOO_SERVICE_NEWMAIL         11
#define YAHOO_SERVICE_NEWPERSONALMAIL 14
#define YAHOO_SERVICE_NEWCONTACT      15
#define YAHOO_SERVICE_PING            18

#define YAHOO_MESSAGE_NORMAL  1
#define YAHOO_MESSAGE_BOUNCE  2

void yahoo_storeint(guchar *, guint);
int yahoo_makeint(guchar *);

struct yahoo_packet {
	char version[8];
	guchar len[4];
	guchar service[4];

	guchar conn_id[4];
	guchar magic_id[4];
	guchar address[4];
	guchar msgtype[4];

	char nick1[36];
	char nick2[36];

	char content[1024];
};

#endif
