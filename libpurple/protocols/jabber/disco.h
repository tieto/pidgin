/**
 * @file disco.h Jabber Service Discovery
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
#ifndef _PURPLE_JABBER_DISCO_H_
#define _PURPLE_JABBER_DISCO_H_

#include "jabber.h"

typedef struct _JabberDiscoItem {
	const char *jid;  /* MUST */
	const char *node; /* SHOULD */
	const char *name; /* MAY */
} JabberDiscoItem;

typedef void (JabberDiscoInfoCallback)(JabberStream *js, const char *who,
		JabberCapabilities capabilities, gpointer data);

typedef void (JabberDiscoItemsCallback)(JabberStream *js, const char *jid,
		const char *node, GSList *items, gpointer data);

void jabber_disco_info_parse(JabberStream *js, xmlnode *packet);
void jabber_disco_items_parse(JabberStream *js, xmlnode *packet);

void jabber_disco_items_server(JabberStream *js);

void jabber_disco_info_do(JabberStream *js, const char *who,
		JabberDiscoInfoCallback *callback, gpointer data);

PurpleDiscoList *jabber_disco_get_list(PurpleConnection *gc);
void jabber_disco_cancel(PurpleDiscoList *list);

int jabber_disco_service_register(PurpleConnection *gc, PurpleDiscoService *service);


void jabber_disco_items_do(JabberStream *js, const char *jid, const char *node,
		JabberDiscoItemsCallback *callback, gpointer data);
void jabber_disco_item_free(JabberDiscoItem *);

#endif /* _PURPLE_JABBER_DISCO_H_ */
