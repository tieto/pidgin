/**
 * @file msn.h The MSN protocol plugin
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
#ifndef _MSN_H_
#define _MSN_H_

#include "internal.h"

#include "account.h"
#include "accountopt.h"
#include "blist.h"
#include "connection.h"
#include "conversation.h"
#include "debug.h"
#include "md5.h"
#include "notify.h"
#include "privacy.h"
#include "proxy.h"
#include "prpl.h"
#include "request.h"
#include "servconn.h"
#include "sha.h"
#include "sslconn.h"
#include "util.h"

#include "ft.h"

/* XXX */
#include "gaim.h"

#define MSN_BUF_LEN 8192

#define USEROPT_MSNSERVER 3
#define MSN_SERVER "messenger.hotmail.com"
#define USEROPT_MSNPORT 4
#define MSN_PORT 1863

#define MSN_TYPING_RECV_TIMEOUT 6
#define MSN_TYPING_SEND_TIMEOUT	4

#define HOTMAIL_URL "http://www.hotmail.com/cgi-bin/folders"
#define PASSPORT_URL "http://lc1.law13.hotmail.passport.com/cgi-bin/dologin?login="
#define PROFILE_URL "http://members.msn.com/"

#define USEROPT_HOTMAIL 0


#define MSN_FT_GUID "{5D3E02AB-6190-11d3-BBBB-00C04F795683}"

#define MSN_CONNECT_STEPS 8

#define MSN_CLIENTINFO \
	"Client-Name: Gaim/" VERSION "\r\n" \
	"Chat-Logging: Y\r\n"


typedef enum
{
	MSN_LIST_FL_OP = 0x01,
	MSN_LIST_AL_OP = 0x02,
	MSN_LIST_BL_OP = 0x04,
	MSN_LIST_RL_OP = 0x08

} MsnListOp;

typedef enum
{
	MSN_CLIENT_CAP_WIN_MOBILE = 0x001,
	MSN_CLIENT_CAP_UNKNOWN_1  = 0x002,
	MSN_CLIENT_CAP_VIEW_INK   = 0x004,
	MSN_CLIENT_CAP_SEND_INK   = 0x008,
	MSN_CLIENT_CAP_VIDEO_CHAT = 0x010,
	MSN_CLIENT_CAP_BASE       = 0x020,
	MSN_CLIENT_CAP_MSNMOBILE  = 0x040,
	MSN_CLIENT_CAP_MSNDIRECT  = 0x080,
	MSN_CLIENT_CAP_WEBMSGR    = 0x100

} MsnClientCaps;

typedef enum
{
	MSN_CLIENT_VER_5_0 = 0x00,
	MSN_CLIENT_VER_6_0 = 0x10,
	MSN_CLIENT_VER_6_1 = 0x20,
	MSN_CLIENT_VER_6_2 = 0x30,
	MSN_CLIENT_VER_7_0 = 0x40

} MsnClientVerId;

#define MSN_CLIENT_ID_VERSION      MSN_CLIENT_VER_6_0
#define MSN_CLIENT_ID_RESERVED_1   0x00
#define MSN_CLIENT_ID_RESERVED_2   0x00
#define MSN_CLIENT_ID_CAPABILITIES MSN_CLIENT_CAP_BASE

#define MSN_CLIENT_ID \
	((MSN_CLIENT_ID_VERSION    << 24) | \
	 (MSN_CLIENT_ID_RESERVED_1 << 16) | \
	 (MSN_CLIENT_ID_RESERVED_2 <<  8) | \
	 (MSN_CLIENT_ID_CAPABILITIES))

#endif /* _MSN_H_ */
