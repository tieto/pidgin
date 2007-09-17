/**
 * @file iq.h JabberID handlers
 *
 * purple
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#ifndef _PURPLE_JABBER_IQ_H_
#define _PURPLE_JABBER_IQ_H_

#include "jabber.h"

typedef struct _JabberIq JabberIq;

typedef enum {
	JABBER_IQ_SET,
	JABBER_IQ_GET,
	JABBER_IQ_RESULT,
	JABBER_IQ_ERROR,
	JABBER_IQ_NONE
} JabberIqType;

typedef void (JabberIqHandler)(JabberStream *js, xmlnode *packet);

typedef void (JabberIqCallback)(JabberStream *js, xmlnode *packet, gpointer data);

struct _JabberIq {
	JabberIqType type;
	char *id;
	xmlnode *node;

	JabberIqCallback *callback;
	gpointer callback_data;

	JabberStream *js;
};

JabberIq *jabber_iq_new(JabberStream *js, JabberIqType type);
JabberIq *jabber_iq_new_query(JabberStream *js, JabberIqType type,
		const char *xmlns);

void jabber_iq_parse(JabberStream *js, xmlnode *packet);

void jabber_iq_remove_callback_by_id(JabberStream *js, const char *id);
void jabber_iq_set_callback(JabberIq *iq, JabberIqCallback *cb, gpointer data);
void jabber_iq_set_id(JabberIq *iq, const char *id);

void jabber_iq_send(JabberIq *iq);
void jabber_iq_free(JabberIq *iq);

void jabber_iq_init(void);
void jabber_iq_uninit(void);

void jabber_iq_register_handler(const char *xmlns, JabberIqHandler *func);

#endif /* _PURPLE_JABBER_IQ_H_ */
