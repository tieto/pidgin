/**
 * @file servconn.h Server connection functions
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
#ifndef _MSN_SERVCONN_H_
#define _MSN_SERVCONN_H_

typedef struct _MsnServConn MsnServConn;

#include "msg.h"

typedef gboolean (*MsnServConnCommandCb)(MsnServConn *servconn,
										 const char *cmd, const char **params,
										 size_t param_count);

typedef gboolean (*MsnServConnMsgCb)(MsnServConn *servconn, MsnMessage *msg);

#include "session.h"

typedef enum
{
	MSN_MULTILINE_MSG,
	MSN_MULTILINE_IPG,
	MSN_MULTILINE_NOT

} MsnMultilineType;

struct _MsnServConn
{
	MsnSession *session;

	gboolean connected;

	char *server;
	int port;

	int fd;
	int inpa;

	char *rxqueue;
	int rxlen;

	GSList *msg_queue;

	GSList *txqueue;

	gboolean parsing_multiline;
	MsnMultilineType multiline_type;
	char *msg_passport;
	char *msg_friendly;
	int multiline_len;

	GHashTable *commands;
	GHashTable *msg_types;

	gboolean (*connect_cb)(gpointer, gint, GaimInputCondition);
	void (*failed_read_cb)(gpointer, gint, GaimInputCondition);
	void (*login_cb)(gpointer, gint, GaimInputCondition);

	void *data;
};

MsnServConn *msn_servconn_new(MsnSession *session);

void msn_servconn_destroy(MsnServConn *servconn);

gboolean msn_servconn_connect(MsnServConn *servconn);
void msn_servconn_disconnect(MsnServConn *servconn);

void msn_servconn_set_server(MsnServConn *servconn, const char *server,
							 int port);

const char *msn_servconn_get_server(const MsnServConn *servconn);
int msn_servconn_get_port(const MsnServConn *servconn);

void msn_servconn_set_connect_cb(MsnServConn *servconn,
		gboolean (*connect_cb)(gpointer, gint, GaimInputCondition));

void msn_servconn_set_failed_read_cb(MsnServConn *servconn,
		void (*failed_read_cb)(gpointer, gint, GaimInputCondition));

size_t msn_servconn_write(MsnServConn *servconn, const char *buf,
						  size_t size);

gboolean msn_servconn_send_command(MsnServConn *servconn, const char *command,
								   const char *params);

void msn_servconn_queue_message(MsnServConn *servconn, const char *command,
								MsnMessage *msg);

void msn_servconn_unqueue_message(MsnServConn *servconn, MsnMessage *msg);

void msn_servconn_register_command(MsnServConn *servconn,
								   const char *command,
								   MsnServConnCommandCb cb);

void msn_servconn_register_msg_type(MsnServConn *servconn,
									const char *content_type,
									MsnServConnMsgCb cb);

void msn_servconn_parse_data(gpointer data, gint source,
							 GaimInputCondition cond);

#endif /* _MSN_SERVCONN_H_ */
