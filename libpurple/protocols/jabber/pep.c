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

#include "internal.h"

#include "pep.h"
#include "iq.h"
#include <string.h>
#include "usermood.h"
#include "usernick.h"

static GHashTable *pep_handlers = NULL;

void jabber_pep_init(void) {
	if(!pep_handlers) {
		pep_handlers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
		
		/* register PEP handlers */
		jabber_mood_init();
		jabber_nick_init();
	}
}

void jabber_pep_init_actions(GList **m) {
	/* register the PEP-specific actions */
	jabber_mood_init_action(m);
	jabber_nick_init_action(m);
}

void jabber_pep_register_handler(const char *shortname, const char *xmlns, JabberPEPHandler handlerfunc) {
	gchar *notifyns = g_strdup_printf("%s+notify", xmlns);
	jabber_add_feature(shortname, notifyns, NULL); /* receiving PEPs is always supported */
	g_free(notifyns);
	g_hash_table_replace(pep_handlers, g_strdup(xmlns), handlerfunc);
}

static void do_pep_iq_request_item_callback(JabberStream *js, xmlnode *packet, gpointer data) {
	const char *from = xmlnode_get_attrib(packet,"from");
	xmlnode *pubsub = xmlnode_get_child_with_namespace(packet,"pubsub","http://jabber.org/protocol/pubsub#event");
	xmlnode *items = NULL;
	JabberPEPHandler *cb = data;
	
	if(pubsub)
		items = xmlnode_get_child(pubsub, "items");
	
	cb(js, from, items);
}

void jabber_pep_request_item(JabberStream *js, const char *to, const char *node, const char *id, JabberPEPHandler cb) {
	JabberIq *iq = jabber_iq_new(js, JABBER_IQ_GET);
	xmlnode *pubsub, *items, *item;
	
	xmlnode_set_attrib(iq->node,"to",to);
	pubsub = xmlnode_new_child(iq->node,"pubsub");
	
	xmlnode_set_namespace(pubsub,"http://jabber.org/protocol/pubsub");
	
	items = xmlnode_new_child(pubsub, "items");
	xmlnode_set_attrib(items,"node",node);
	
	item = xmlnode_new_child(items, "item");
	if(id)
		xmlnode_set_attrib(item, "id", id);
	
	jabber_iq_set_callback(iq,do_pep_iq_request_item_callback,(gpointer)cb);
	
	jabber_iq_send(iq);
}

gboolean jabber_pep_namespace_only_when_pep_enabled_cb(JabberStream *js, const gchar *shortname, const gchar *namespace) {
	return js->pep;
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
	xmlnode *pubsub;

	if(js->pep != TRUE) {
		/* ignore when there's no PEP support on the server */
		xmlnode_free(publish);
		return;
	}

	iq = jabber_iq_new(js, JABBER_IQ_SET);

	pubsub = xmlnode_new("pubsub");
	xmlnode_set_namespace(pubsub, "http://jabber.org/protocol/pubsub");

	xmlnode_insert_child(pubsub, publish);

	xmlnode_insert_child(iq->node, pubsub);

	jabber_iq_send(iq);
}
