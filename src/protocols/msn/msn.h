/**
 * @file msn.h The MSN protocol plugin
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
#ifndef _MSN_H_
#define _MSN_H_

#include "config.h"

#ifndef _WIN32
#include <unistd.h>
#else
#include <winsock.h>
#include <io.h>
#endif


#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#ifndef _WIN32
#include <netdb.h>
#endif

#include "gaim.h"
#include "account.h"
#include "blist.h"
#include "conversation.h"
#include "connection.h"
#include "debug.h"
#include "md5.h"
#include "proxy.h"
#include "prpl.h"

#ifdef _WIN32
#include "win32dep.h"
#include "stdint.h"
#endif

#define MSN_BUF_LEN 8192

#define USEROPT_MSNSERVER 3
#define MSN_SERVER "messenger.hotmail.com"
#define USEROPT_MSNPORT 4
#define MSN_PORT 1863

#define MSN_TYPING_RECV_TIMEOUT 6
#define MSN_TYPING_SEND_TIMEOUT	4

#define HOTMAIL_URL "http://www.hotmail.com/cgi-bin/folders"
#define PASSPORT_URL "http://lc1.law13.hotmail.passport.com/cgi-bin/dologin?login="

#define USEROPT_HOTMAIL 0


#define MSN_FT_GUID "{5D3E02AB-6190-11d3-BBBB-00C04F795683}"

#define MSN_CONNECT_STEPS 7

#define MSN_CLIENTINFO \
	"Client-Name: Gaim/" VERSION "\r\n" \
	"Chat-Logging: Y\r\n" \
	"Buddy-Icons: 1\r\n"

#endif /* _MSN_H_ */
