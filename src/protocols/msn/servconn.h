/**
 * @file servconn.h Server connection functions
 *
 * gaim
 *
 * Copyright (C) 2003-2004 Christian Hammond <chipx86@gnupdate.org>
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
#ifndef _MSN_SERVCONN_H_
#define _MSN_SERVCONN_H_

typedef struct _MsnServConn MsnServConn;

#include "cmdproc.h"
#include "proxy.h"

#include "msg.h"
#include "httpmethod.h"

#include "session.h"

typedef enum
{
	MSN_SERVER_NS,
	MSN_SERVER_SB,
	MSN_SERVER_NX,
	MSN_SERVER_DC,
	MSN_SERVER_HT

} MsnServerType;

struct _MsnServConn
{
	MsnServerType type;
	MsnSession *session;
	MsnCmdProc *cmdproc;

	gboolean connected;
	gboolean processing;
	gboolean wasted;

	int num;

	MsnHttpMethodData *http_data;

	int fd;
	int inpa;

	char *rx_buf;
	int rx_len;

	int payload_len;

	gboolean (*connect_cb)(MsnServConn *servconn);
	void (*disconnect_cb)(MsnServConn *servconn);

	void *data;
};

MsnServConn *msn_servconn_new(MsnSession *session, MsnServerType type);

void msn_servconn_destroy(MsnServConn *servconn);

gboolean msn_servconn_connect(MsnServConn *servconn, const char *host,
							  int port);
void msn_servconn_disconnect(MsnServConn *servconn);

void msn_servconn_set_connect_cb(MsnServConn *servconn,
		gboolean (*connect_cb)(MsnServConn *servconn));

void msn_servconn_set_disconnect_cb(MsnServConn *servconn,
		void (*disconnect_cb)(MsnServConn *servconn));

size_t msn_servconn_write(MsnServConn *servconn, const char *buf,
						  size_t size);

#endif /* _MSN_SERVCONN_H_ */
