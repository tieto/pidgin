/**
 * @file iq.h JabberID handlers
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
#ifndef _GAIM_JABBER_IQ_H_
#define _GAIM_JABBER_IQ_H_

#include "jabber.h"

typedef struct _JabberIq JabberIq;

typedef enum {
	JABBER_IQ_SET,
	JABBER_IQ_GET,
	JABBER_IQ_RESULT,
	JABBER_IQ_ERROR,
	JABBER_IQ_NONE
} JabberIqType;

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

#endif /* _GAIM_JABBER_IQ_H_ */
