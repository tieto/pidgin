/**
 * @file message.h Message handlers
 *
 * gaim
 *
 * Copyright (C) 2003 Nathan Walp <faceprint@faceprint.com>
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
#ifndef _GAIM_JABBER_MESSAGE_H_
#define _GAIM_JABBER_MESSAGE_H_

#include "jabber.h"
#include "xmlnode.h"

typedef struct _JabberMessage {
	JabberStream *js;
	enum {
		JABBER_MESSAGE_NORMAL,
		JABBER_MESSAGE_CHAT,
		JABBER_MESSAGE_GROUPCHAT,
		JABBER_MESSAGE_HEADLINE,
		JABBER_MESSAGE_ERROR,
		JABBER_MESSAGE_GROUPCHAT_INVITE,
		JABBER_MESSAGE_OTHER
	} type;
	time_t sent;
	gboolean delayed;
	char *id;
	char *from;
	char *to;
	char *subject;
	char *body;
	char *xhtml;
	char *password;
	char *error;
	char *thread_id;
	enum {
		JABBER_MESSAGE_EVENT_COMPOSING = 1 << 1
	} events;
	GList *etc;
} JabberMessage;

void jabber_message_free(JabberMessage *jm);

void jabber_message_send(JabberMessage *jm);

void jabber_message_parse(JabberStream *js, xmlnode *packet);
int jabber_message_send_im(GaimConnection *gc, const char *who, const char *msg,
		GaimMessageFlags flags);
int jabber_message_send_chat(GaimConnection *gc, int id, const char *message, GaimMessageFlags flags);

int jabber_send_typing(GaimConnection *gc, const char *who, int typing);


#endif /* _GAIM_JABBER_MESSAGE_H_ */
