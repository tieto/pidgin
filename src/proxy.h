/**
 * @file proxy.h Proxy functions
 * @ingroup core
 *
 * gaim
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
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

/* this is the export part of the proxy.c file. it does a little
   prototype-ing stuff and redefine some net function to mask them
   with some kind of transparent layer */ 

#ifndef _PROXY_H_
#define _PROXY_H_

#include <sys/types.h>
/*this must happen before sys/socket.h or freebsd won't compile*/

#ifndef _WIN32
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#else
#include <winsock.h>
#endif

#include <glib.h>

#include "core.h"

typedef enum {
	PROXY_USE_GLOBAL = -1,
	PROXY_NONE = 0,
	PROXY_HTTP,
	PROXY_SOCKS4,
	PROXY_SOCKS5,
} proxytype_t;

struct gaim_proxy_info {
	int proxytype;
	char proxyhost[128];
	proxytype_t proxyport;
	char proxyuser[128];
	char proxypass[128];
};

extern struct gaim_proxy_info global_proxy_info;
extern guint proxy_info_is_from_gaimrc;

typedef enum {
	GAIM_INPUT_READ = 1 << 0,
	GAIM_INPUT_WRITE = 1 << 1
} GaimInputCondition;
typedef void (*GaimInputFunction)(gpointer, gint, GaimInputCondition);

extern gint gaim_input_add(int, GaimInputCondition, GaimInputFunction, gpointer);
extern void gaim_input_remove(gint);

extern int proxy_connect(GaimAccount *account, const char *host, int port, GaimInputFunction func, gpointer data);

#endif /* _PROXY_H_ */
