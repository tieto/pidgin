/**
 * @file jabber.h The Purple interface to mDNS and peer to peer Jabber.
 *
 * purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */

#ifndef _BONJOUR_JABBER_H_
#define _BONJOUR_JABBER_H_

#include <libxml/parser.h>

#include "xmlnode.h"

#include "account.h"
#include "circbuffer.h"

typedef struct _BonjourJabber
{
	gint port;
	gint socket;
	gint watcher_id;
	PurpleAccount* account;
} BonjourJabber;

typedef struct _BonjourJabberConversation
{
	gint socket;
	guint rx_handler;
	guint tx_handler;
	PurpleCircBuffer *tx_buf;
	gboolean sent_stream_start;
	gboolean recv_stream_start;
	PurpleProxyConnectData *connect_data;
	gpointer stream_data;
	xmlParserCtxt *context;
	xmlnode *current;
} BonjourJabberConversation;

/**
 * Start listening for jabber connections.
 *
 * @return -1 if there was a problem, else returns the listening
 *         port number.
 */
gint bonjour_jabber_start(BonjourJabber *data);

int bonjour_jabber_send_message(BonjourJabber *data, const gchar *to, const gchar *body);

void bonjour_jabber_close_conversation(BonjourJabberConversation *bconv);

void bonjour_jabber_stream_started(PurpleBuddy *pb);

void bonjour_jabber_stream_ended(PurpleBuddy *pb);

void bonjour_jabber_process_packet(PurpleBuddy *pb, xmlnode *packet);

void bonjour_jabber_stop(BonjourJabber *data);

#endif /* _BONJOUR_JABBER_H_ */
