/**
 * @file msn.h The MSN protocol plugin
 *
 * gaim
 *
 * Copyright (C) 2003, Christian Hammond <chipx86@gnupdate.org>
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
#include "prpl.h"
#include "proxy.h"
#include "md5.h"

#ifdef _WIN32
#include "win32dep.h"
#include "stdint.h"
#endif

#include "msg.h"
#include "switchboard.h"

#define MSN_BUF_LEN 8192
#define MIME_HEADER	"MIME-Version: 1.0\r\n" \
			"Content-Type: text/plain; charset=UTF-8\r\n" \
			"User-Agent: Gaim/" VERSION "\r\n" \
			"X-MMS-IM-Format: FN=Arial; EF=; CO=0; PF=0\r\n\r\n"

#define HOTMAIL_URL "http://www.hotmail.com/cgi-bin/folders"
#define PASSPORT_URL "http://lc1.law13.hotmail.passport.com/cgi-bin/dologin?login="

#define MSN_ONLINE  1
#define MSN_BUSY    2
#define MSN_IDLE    3
#define MSN_BRB     4
#define MSN_AWAY    5
#define MSN_PHONE   6
#define MSN_LUNCH   7
#define MSN_OFFLINE 8
#define MSN_HIDDEN  9

#define USEROPT_HOTMAIL 0

#define USEROPT_MSNSERVER 3
#define MSN_SERVER "messenger.hotmail.com"
#define USEROPT_MSNPORT 4
#define MSN_PORT 1863

#define MSN_TYPING_RECV_TIMEOUT 6
#define MSN_TYPING_SEND_TIMEOUT	4

#define MSN_FT_GUID "{5D3E02AB-6190-11d3-BBBB-00C04F795683}"

#define GET_NEXT(tmp) \
	while (*(tmp) && *(tmp) != ' ' && *(tmp) != '\r') \
		(tmp)++; \
	*(tmp)++ = 0; \
	while (*(tmp) && *(tmp) == ' ') \
		(tmp)++;


struct msn_xfer_data {
	int inpa;

	uint32_t cookie;
	uint32_t authcookie;

	gboolean transferring;

	char *rxqueue;
	int rxlen;
	gboolean msg;
	char *msguser;
	int msglen;
};

struct msn_data {
	int fd;
	uint32_t trId;
	int inpa;

	char *rxqueue;
	int rxlen;
	gboolean msg;
	char *msguser;
	int msglen;

	GSList *switches;
	GSList *fl;
	GSList *permit;
	GSList *deny;
	GSList *file_transfers;

	char *kv;
	char *sid;
	char *mspauth;
	unsigned long sl;
	char *passport;
};

struct msn_buddy {
	char *user;
	char *friend;
};

/**
 * Processes a file transfer message.
 *
 * @param ms  The switchboard.
 * @param msg The message.
 */
void msn_process_ft_msg(struct msn_switchboard *ms, char *msg);

char *handle_errcode(char *buf, gboolean show);
char *url_decode(const char *msg);

#endif /* _MSN_H_ */
