/*
 * purple - Jabber Protocol Plugin
 *
 * Copyright (C) 2007, Andreas Monitzer <andy@monitzer.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA	 02111-1307	 USA
 *
 */

#include "pep.h"
#include "iq.h"
#include <string.h>

static GHashTable *pep_handlers = NULL;

void jabber_pep_init(void) {
	if(!pep_handlers) {
		pep_handlers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
		
		/* register PEP handlers */
		jabber_mood_init();
	}
}

void jabber_pep_register_handler(const char *shortname, const char *xmlns, JabberPEPHandler handlerfunc) {
	gchar *notifyns = g_strdup_printf("%s+notify", xmlns);
	jabber_add_feature(shortname, notifyns);
	g_free(notifyns);
	g_hash_table_replace(pep_handlers, g_strdup(xmlns), handlerfunc);
}

void jabber_handle_event(JabberMessage *jm) {
	/* this may be called even when the own server doesn't support pep! */
	JabberPEPHandler *jph;
	GList *itemslist;
	char *jid = jabber_get_bare_jid(jm->from);
	
	for(itemslist = jm->eventitems; itemslist; itemslist = itemslist->next) {
		xmlnode *items = (xmlnode*)itemslist->data;
		const char *nodename = xmlnode_get_attrib(items,"node");
		
		if(nodename && (jph = g_hash_table_lookup(pep_handlers, nodename)))
			jph(jm->js, jid, items);
	}
	
	/* discard items we don't have a handler for */
	g_free(jid);
}

void jabber_pep_publish(JabberStream *js, xmlnode *publish) {
	JabberIq *iq;
	
	if(js->pep != TRUE) /* ignore when there's no PEP support on the server */
		return;
	
	iq = jabber_iq_new(js, JABBER_IQ_SET);
	
	xmlnode *pubsub = xmlnode_new("pubsub");
	xmlnode_set_namespace(pubsub, "http://jabber.org/protocol/pubsub");
	
	xmlnode_insert_child(pubsub, publish);
	
	xmlnode_insert_child(iq->node, pubsub);
	
	jabber_iq_send(iq);
}
