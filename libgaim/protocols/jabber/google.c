
/**
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

#include "internal.h"
#include "debug.h"
#include "privacy.h"

#include "buddy.h"
#include "google.h"
#include "jabber.h"
#include "presence.h"
#include "iq.h"

static void 
jabber_gmail_parse(JabberStream *js, xmlnode *packet, gpointer nul)
{
	const char *type = xmlnode_get_attrib(packet, "type");
	xmlnode *child;
	xmlnode *message, *sender_node, *subject_node;
	const char *from, *to, *subject, *url, *tid;
	const char *in_str;
	char *to_name;
	int i, count = 1, returned_count;
	
	const char **tos, **froms, **subjects, **urls;
	
	if (strcmp(type, "result"))
		return;
	
	child = xmlnode_get_child(packet, "mailbox");
	if (!child)
		return;

	in_str = xmlnode_get_attrib(child, "total-matched");
	if (in_str && *in_str) 
		count = atoi(in_str);
 
	if (count == 0) 
		return;

	message = xmlnode_get_child(child, "mail-thread-info");
	
	/* Loop once to see how many messages were returned so we can allocate arrays
	 * accordingly */
	if (!message) 
		return;
	for (returned_count = 0; message; returned_count++, message=xmlnode_get_next_twin(message));
	
	froms    = g_new0(const char* , returned_count);
	tos      = g_new0(const char* , returned_count);
	subjects = g_new0(const char* , returned_count);
	urls     = g_new0(const char* , returned_count);
	
	to = xmlnode_get_attrib(packet, "to");
	to_name = jabber_get_bare_jid(to);
	url = xmlnode_get_attrib(child, "url");
	if (!url || !*url)
		url = "http://www.gmail.com";
	
	message= xmlnode_get_child(child, "mail-thread-info");
	for (i=0; message; message = xmlnode_get_next_twin(message), i++) {
		subject_node = xmlnode_get_child(message, "subject");
		sender_node  = xmlnode_get_child(message, "senders");
		sender_node  = xmlnode_get_child(sender_node, "sender");

		while (sender_node && (!xmlnode_get_attrib(sender_node, "unread") || 
		       !strcmp(xmlnode_get_attrib(sender_node, "unread"),"0")))
			sender_node = xmlnode_get_next_twin(sender_node);
		
		if (!sender_node) {
			i--;
			continue;
		}
			
		from = xmlnode_get_attrib(sender_node, "name");
		if (!from || !*from)
			from = xmlnode_get_attrib(sender_node, "address");
		subject = xmlnode_get_data(subject_node);
		/*
		 * url = xmlnode_get_attrib(message, "url");
		 */
		tos[i] = (to_name != NULL ?  to_name : "");
		froms[i] = (from != NULL ?  from : "");
		subjects[i] = (subject != NULL ? subject : "");
		urls[i] = (url != NULL ? url : "");
		
		tid = xmlnode_get_attrib(message, "tid");
		if (tid && 
		    (js->gmail_last_tid == NULL || strcmp(tid, js->gmail_last_tid) > 0)) {
			g_free(js->gmail_last_tid);
			js->gmail_last_tid = g_strdup(tid);
		}
	}

	if (i>0) 
		gaim_notify_emails(js->gc, count, count == returned_count, subjects, froms, tos, 
				   	   urls, NULL, NULL);

	g_free(to_name);
	g_free(tos);
	g_free(froms);
	g_free(subjects);
	g_free(urls);

	in_str = xmlnode_get_attrib(child, "result-time");
	if (in_str && *in_str) {
		g_free(js->gmail_last_time);
		js->gmail_last_time = g_strdup(in_str);
	}
}

void 
jabber_gmail_poke(JabberStream *js, xmlnode *packet) 
{
	const char *type;
	xmlnode *query;
	JabberIq *iq;
	
	/* bail if the user isn't interested */
	if (!gaim_account_get_check_mail(js->gc->account))
		return;

	type = xmlnode_get_attrib(packet, "type");
	

	/* Is this an initial incoming mail notification? If so, send a request for more info */
	if (strcmp(type, "set") || !xmlnode_get_child(packet, "new-mail"))
		return;

	gaim_debug(GAIM_DEBUG_MISC, "jabber",
		   "Got new mail notification. Sending request for more info\n");

	iq = jabber_iq_new_query(js, JABBER_IQ_GET, "google:mail:notify");
	jabber_iq_set_callback(iq, jabber_gmail_parse, NULL);
	query = xmlnode_get_child(iq->node, "query");

	if (js->gmail_last_time)
		xmlnode_set_attrib(query, "newer-than-time", js->gmail_last_time);
	if (js->gmail_last_tid)
		xmlnode_set_attrib(query, "newer-than-tid", js->gmail_last_tid);

	jabber_iq_send(iq);
	return;
}

void jabber_gmail_init(JabberStream *js) {
	JabberIq *iq;

	if (!gaim_account_get_check_mail(js->gc->account)) 
		return;

	iq = jabber_iq_new_query(js, JABBER_IQ_GET, "google:mail:notify");
	jabber_iq_set_callback(iq, jabber_gmail_parse, NULL);
	jabber_iq_send(iq);
}

void jabber_google_roster_init(JabberStream *js)
{
	JabberIq *iq;
	xmlnode *query;

	iq = jabber_iq_new_query(js, JABBER_IQ_GET, "jabber:iq:roster");
	query = xmlnode_get_child(iq->node, "query");
	
	xmlnode_set_attrib(query, "xmlns:gr", "google:roster");
	xmlnode_set_attrib(query, "gr:ext", "2");

	jabber_iq_send(iq);
}

void jabber_google_roster_outgoing(JabberStream *js, xmlnode *query, xmlnode *item)
{
	GaimAccount *account = gaim_connection_get_account(js->gc);
	GSList *list = account->deny;
	const char *jid = xmlnode_get_attrib(item, "jid");
	char *jid_norm = g_strdup(jabber_normalize(account, jid));

	while (list) {
		if (!strcmp(jid_norm, (char*)list->data)) {
			xmlnode_set_attrib(query, "xmlns:gr", "google:roster");
			xmlnode_set_attrib(item, "gr:t", "B");
			xmlnode_set_attrib(query, "xmlns:gr", "google:roster");
			xmlnode_set_attrib(query, "gr:ext", "2");
			return;
		}
		list = list->next;
	}

}

void jabber_google_roster_incoming(JabberStream *js, xmlnode *item)
{
	GaimAccount *account = gaim_connection_get_account(js->gc);
	GSList *list = account->deny;
	const char *jid = xmlnode_get_attrib(item, "jid");
	gboolean on_block_list = FALSE;

	char *jid_norm = g_strdup(jabber_normalize(account, jid));

	const char *grt = xmlnode_get_attrib_with_namespace(item, "t", "google:roster");
	
	while (list) {
		if (!strcmp(jid_norm, (char*)list->data)) {
			on_block_list = TRUE;
			break;
		}
		list = list->next;
	}
	
	if (!on_block_list && (grt && (*grt == 'B' || *grt == 'b'))) {
		gaim_debug_info("jabber", "Blocking %s\n", jid_norm);
		gaim_privacy_deny_add(account, jid_norm, TRUE);
	} else if (on_block_list && (!grt || (*grt != 'B' && *grt != 'b' ))){
		gaim_debug_info("jabber", "Unblocking %s\n", jid_norm);
		gaim_privacy_deny_remove(account, jid_norm, TRUE);
	}
}

void jabber_google_roster_add_deny(GaimConnection *gc, const char *who) 
{
	JabberStream *js;
	GSList *buddies;
	JabberIq *iq;
	xmlnode *query;
	xmlnode *item;
	xmlnode *group;
	GaimBuddy *b;
	JabberBuddy *jb;

	js = (JabberStream*)(gc->proto_data);
	
	if (!js || !js->server_caps & JABBER_CAP_GOOGLE_ROSTER)
		return;

	jb = jabber_buddy_find(js, who, TRUE);

	buddies = gaim_find_buddies(js->gc->account, who);
	if(!buddies)
		return;
	
	b = buddies->data;

	iq = jabber_iq_new_query(js, JABBER_IQ_SET, "jabber:iq:roster");
	
	query = xmlnode_get_child(iq->node, "query");
	item = xmlnode_new_child(query, "item");

	while(buddies) {
		GaimGroup *g;

		b = buddies->data;
		g = gaim_buddy_get_group(b);

		group = xmlnode_new_child(item, "group");
		xmlnode_insert_data(group, g->name, -1);
		
		buddies = buddies->next;
	}

	iq = jabber_iq_new_query(js, JABBER_IQ_SET, "jabber:iq:roster");

	query = xmlnode_get_child(iq->node, "query");
	item = xmlnode_new_child(query, "item");

	xmlnode_set_attrib(item, "jid", who);
	xmlnode_set_attrib(item, "name", b->alias ? b->alias : "");
	xmlnode_set_attrib(item, "gr:t", "B");
	xmlnode_set_attrib(query, "xmlns:gr", "google:roster");
	xmlnode_set_attrib(query, "gr:ext", "2");

	jabber_iq_send(iq);

	/* Synthesize a sign-off */
	if (jb) {
		JabberBuddyResource *jbr;
		GList *l = jb->resources;
		while (l) {
			jbr = l->data;
			printf("ASDFA %s\n", jbr->name);
			jabber_buddy_remove_resource(jb, jbr->name);
			l = l->next;
		}
	}
	gaim_prpl_got_user_status(gaim_connection_get_account(gc), who, "offline", NULL);
}

void jabber_google_roster_rem_deny(GaimConnection *gc, const char *who)
{
	JabberStream *js;
	GSList *buddies;
	JabberIq *iq;
	xmlnode *query;
	xmlnode *item;
	xmlnode *group;
	GaimBuddy *b;

	g_return_if_fail(gc != NULL);
	g_return_if_fail(who != NULL);
	
	js = (JabberStream*)(gc->proto_data);
	
	if (!js || !js->server_caps & JABBER_CAP_GOOGLE_ROSTER)
		return;
	
	buddies = gaim_find_buddies(js->gc->account, who);
	if(!buddies)
		return;
	
	b = buddies->data;

	iq = jabber_iq_new_query(js, JABBER_IQ_SET, "jabber:iq:roster");
	
	query = xmlnode_get_child(iq->node, "query");
	item = xmlnode_new_child(query, "item");

	while(buddies) {
		GaimGroup *g;

		b = buddies->data;
		g = gaim_buddy_get_group(b);

		group = xmlnode_new_child(item, "group");
		xmlnode_insert_data(group, g->name, -1);
		
		buddies = buddies->next;
	}

	iq = jabber_iq_new_query(js, JABBER_IQ_SET, "jabber:iq:roster");

	query = xmlnode_get_child(iq->node, "query");
	item = xmlnode_new_child(query, "item");

	xmlnode_set_attrib(item, "jid", who);
	xmlnode_set_attrib(item, "name", b->alias ? b->alias : "");
	xmlnode_set_attrib(query, "xmlns:gr", "google:roster");
	xmlnode_set_attrib(query, "gr:ext", "2");

	jabber_iq_send(iq);

	/* See if he's online */
	jabber_presence_subscription_set(js, who, "probe");
}
