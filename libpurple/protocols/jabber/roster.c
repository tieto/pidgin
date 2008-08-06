/*
 * purple - Jabber Protocol Plugin
 *
 * Copyright (C) 2003, Nathan Walp <faceprint@faceprint.com>
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
#include "internal.h"
#include "debug.h"
#include "server.h"
#include "util.h"

#include "buddy.h"
#include "google.h"
#include "presence.h"
#include "roster.h"
#include "iq.h"

#include <string.h>

void jabber_roster_request(JabberStream *js)
{
	JabberIq *iq;

	iq = jabber_iq_new_query(js, JABBER_IQ_GET, "jabber:iq:roster");

	jabber_iq_send(iq);
}

static void remove_purple_buddies(JabberStream *js, const char *jid)
{
	GSList *buddies, *l;

	buddies = purple_find_buddies(js->gc->account, jid);

	for(l = buddies; l; l = l->next)
		purple_blist_remove_buddy(l->data);

	g_slist_free(buddies);
}

static void add_purple_buddies_to_groups(JabberStream *js, const char *jid,
		const char *alias, GSList *groups)
{
	GSList *buddies, *g2, *l;
	gchar *my_bare_jid;
	GList *pool = NULL;

	buddies = purple_find_buddies(js->gc->account, jid);

	g2 = groups;

	if(!groups) {
		if(!buddies)
			g2 = g_slist_append(g2, g_strdup(_("Buddies")));
		else {
			g_slist_free(buddies);
			return;
		}
	}

	my_bare_jid = g_strdup_printf("%s@%s", js->user->node, js->user->domain);

	while(buddies) {
		PurpleBuddy *b = buddies->data;
		PurpleGroup *g = purple_buddy_get_group(b);

		buddies = g_slist_remove(buddies, b);

		if((l = g_slist_find_custom(g2, g->name, (GCompareFunc)strcmp))) {
			const char *servernick;

			if((servernick = purple_blist_node_get_string((PurpleBlistNode*)b, "servernick")))
				serv_got_alias(js->gc, jid, servernick);

			if(alias && (!b->alias || strcmp(b->alias, alias)))
				purple_blist_alias_buddy(b, alias);
			g_free(l->data);
			g2 = g_slist_delete_link(g2, l);
		} else {
			pool = g_list_prepend(pool, b);
		}
	}

	while(g2) {
		PurpleGroup *g = purple_find_group(g2->data);
		PurpleBuddy *b = NULL;

		if (pool) {
			b = pool->data;
			pool = g_list_delete_link(pool, pool);
		} else {			
			b = purple_buddy_new(js->gc->account, jid, alias);
		}

		if(!g) {
			g = purple_group_new(g2->data);
			purple_blist_add_group(g, NULL);
		}

		purple_blist_add_buddy(b, NULL, g, NULL);
		purple_blist_alias_buddy(b, alias);

		/* If we just learned about ourself, then fake our status,
		 * because we won't be receiving a normal presence message
		 * about ourself. */
		if(!strcmp(b->name, my_bare_jid)) {
			PurplePresence *gpresence;
			PurpleStatus *status;

			gpresence = purple_account_get_presence(js->gc->account);
			status = purple_presence_get_active_status(gpresence);
			jabber_presence_fake_to_self(js, status);
		}

		g_free(g2->data);
		g2 = g_slist_delete_link(g2, g2);
	}

	while (pool) {
		PurpleBuddy *b = pool->data;
		purple_blist_remove_buddy(b);
		pool = g_list_delete_link(pool, pool);
	}

	g_free(my_bare_jid);
	g_slist_free(buddies);
}

void jabber_roster_parse(JabberStream *js, xmlnode *packet)
{
	xmlnode *query, *item, *group;
	const char *from = xmlnode_get_attrib(packet, "from");

	if(from) {
		char *from_norm;
		gboolean invalid;

		from_norm = g_strdup(jabber_normalize(js->gc->account, from));

		if(!from_norm)
			return;

		invalid = g_utf8_collate(from_norm,
				jabber_normalize(js->gc->account,
					purple_account_get_username(js->gc->account)));

		g_free(from_norm);

		if(invalid)
			return;
	}

	query = xmlnode_get_child(packet, "query");
	if(!query)
		return;

	js->currently_parsing_roster_push = TRUE;

	for(item = xmlnode_get_child(query, "item"); item; item = xmlnode_get_next_twin(item))
	{
		const char *jid, *name, *subscription, *ask;
		JabberBuddy *jb;

		subscription = xmlnode_get_attrib(item, "subscription");
		jid = xmlnode_get_attrib(item, "jid");
		name = xmlnode_get_attrib(item, "name");
		ask = xmlnode_get_attrib(item, "ask");

		if(!jid)
			continue;

		if(!(jb = jabber_buddy_find(js, jid, TRUE)))
			continue;

		if(subscription) {
			gint me = -1;
			char *jid_norm;
			const char *username;

			jid_norm = g_strdup(jabber_normalize(js->gc->account, jid));
			username = purple_account_get_username(js->gc->account);
			me = g_utf8_collate(jid_norm,
			                    jabber_normalize(js->gc->account,
			                                     username));
			g_free(jid_norm);

			if(me == 0)
				jb->subscription = JABBER_SUB_BOTH;
			else if(!strcmp(subscription, "none"))
				jb->subscription = JABBER_SUB_NONE;
			else if(!strcmp(subscription, "to"))
				jb->subscription = JABBER_SUB_TO;
			else if(!strcmp(subscription, "from"))
				jb->subscription = JABBER_SUB_FROM;
			else if(!strcmp(subscription, "both"))
				jb->subscription = JABBER_SUB_BOTH;
			else if(!strcmp(subscription, "remove"))
				jb->subscription = JABBER_SUB_REMOVE;
			/* XXX: if subscription is now "from" or "none" we need to
			 * fake a signoff, since we won't get any presence from them
			 * anymore */
			/* YYY: I was going to use this, but I'm not sure it's necessary
			 * anymore, but it's here in case it is. */
			/*
			if ((jb->subscription & JABBER_SUB_FROM) ||
					(jb->subscription & JABBER_SUB_NONE)) {
				purple_prpl_got_user_status(js->gc->account, jid, "offline", NULL);
			}
			*/
		}

		if(ask && !strcmp(ask, "subscribe"))
			jb->subscription |= JABBER_SUB_PENDING;
		else
			jb->subscription &= ~JABBER_SUB_PENDING;

		if(jb->subscription == JABBER_SUB_REMOVE) {
			remove_purple_buddies(js, jid);
		} else {
			GSList *groups = NULL;

			if (js->server_caps & JABBER_CAP_GOOGLE_ROSTER)
				if (!jabber_google_roster_incoming(js, item))
					continue;

			for(group = xmlnode_get_child(item, "group"); group; group = xmlnode_get_next_twin(group)) {
				char *group_name;

				if(!(group_name = xmlnode_get_data(group)))
					group_name = g_strdup("");

				if (g_slist_find_custom(groups, group_name, (GCompareFunc)purple_utf8_strcasecmp) == NULL)
					groups = g_slist_append(groups, group_name);
				else
					g_free(group_name);
			}
			add_purple_buddies_to_groups(js, jid, name, groups);
		}
	}

	js->currently_parsing_roster_push = FALSE;

	/* if we're just now parsing the roster for the first time,
	 * then now would be the time to send our initial presence */
	if(!js->roster_parsed) {
		js->roster_parsed = TRUE;

		jabber_presence_send(js->gc->account, NULL);
	}
}

static void jabber_roster_update(JabberStream *js, const char *name,
		GSList *grps)
{
	PurpleBuddy *b;
	PurpleGroup *g;
	GSList *groups = NULL, *l;
	JabberIq *iq;
	xmlnode *query, *item, *group;

	if (js->currently_parsing_roster_push)
		return;

	if(!(b = purple_find_buddy(js->gc->account, name)))
		return;

	if(grps) {
		groups = grps;
	} else {
		GSList *buddies = purple_find_buddies(js->gc->account, name);
		if(!buddies)
			return;
		while(buddies) {
			b = buddies->data;
			g = purple_buddy_get_group(b);
			groups = g_slist_append(groups, g->name);
			buddies = g_slist_remove(buddies, b);
		}
	}

	iq = jabber_iq_new_query(js, JABBER_IQ_SET, "jabber:iq:roster");

	query = xmlnode_get_child(iq->node, "query");
	item = xmlnode_new_child(query, "item");

	xmlnode_set_attrib(item, "jid", name);

	xmlnode_set_attrib(item, "name", b->alias ? b->alias : "");

	for(l = groups; l; l = l->next) {
		group = xmlnode_new_child(item, "group");
		xmlnode_insert_data(group, l->data, -1);
	}

	if(!grps)
		g_slist_free(groups);
	
	if (js->server_caps & JABBER_CAP_GOOGLE_ROSTER) {
		jabber_google_roster_outgoing(js, query, item);
		xmlnode_set_attrib(query, "xmlns:gr", "google:roster");
		xmlnode_set_attrib(query, "gr:ext", "2");
	}
	jabber_iq_send(iq);
}

void jabber_roster_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy,
		PurpleGroup *group)
{
	JabberStream *js = gc->proto_data;
	char *who;
	JabberBuddy *jb;
	JabberBuddyResource *jbr;
	char *my_bare_jid;

	if(!js->roster_parsed)
		return;

	if(!(who = jabber_get_bare_jid(buddy->name)))
		return;

	jb = jabber_buddy_find(js, buddy->name, FALSE);

	jabber_roster_update(js, who, NULL);

	my_bare_jid = g_strdup_printf("%s@%s", js->user->node, js->user->domain);
	if(!strcmp(who, my_bare_jid)) {
		PurplePresence *gpresence;
		PurpleStatus *status;

		gpresence = purple_account_get_presence(js->gc->account);
		status = purple_presence_get_active_status(gpresence);
		jabber_presence_fake_to_self(js, status);
	} else if(!jb || !(jb->subscription & JABBER_SUB_TO)) {
		jabber_presence_subscription_set(js, who, "subscribe");
	} else if((jbr =jabber_buddy_find_resource(jb, NULL))) {
		purple_prpl_got_user_status(gc->account, who,
				jabber_buddy_state_get_status_id(jbr->state),
				"priority", jbr->priority, jbr->status ? "message" : NULL, jbr->status, NULL);
	}

	g_free(my_bare_jid);
	g_free(who);
}

void jabber_roster_alias_change(PurpleConnection *gc, const char *name, const char *alias)
{
	PurpleBuddy *b = purple_find_buddy(gc->account, name);

	if(b != NULL) {
		purple_blist_alias_buddy(b, alias);

		jabber_roster_update(gc->proto_data, name, NULL);
	}
}

void jabber_roster_group_change(PurpleConnection *gc, const char *name,
		const char *old_group, const char *new_group)
{
	GSList *buddies, *groups = NULL;
	PurpleBuddy *b;
	PurpleGroup *g;

	if(!old_group || !new_group || !strcmp(old_group, new_group))
		return;

	buddies = purple_find_buddies(gc->account, name);
	while(buddies) {
		b = buddies->data;
		g = purple_buddy_get_group(b);
		if(!strcmp(g->name, old_group))
			groups = g_slist_append(groups, (char*)new_group); /* ick */
		else
			groups = g_slist_append(groups, g->name);
		buddies = g_slist_remove(buddies, b);
	}
	jabber_roster_update(gc->proto_data, name, groups);
	g_slist_free(groups);
}

void jabber_roster_group_rename(PurpleConnection *gc, const char *old_name,
		PurpleGroup *group, GList *moved_buddies)
{
	GList *l;
	for(l = moved_buddies; l; l = l->next) {
		PurpleBuddy *buddy = l->data;
		jabber_roster_group_change(gc, buddy->name, old_name, group->name);
	}
}

void jabber_roster_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy,
		PurpleGroup *group) {
	GSList *buddies = purple_find_buddies(gc->account, buddy->name);

	buddies = g_slist_remove(buddies, buddy);
	if(buddies != NULL) {
		PurpleBuddy *tmpbuddy;
		PurpleGroup *tmpgroup;
		GSList *groups = NULL;

		while(buddies) {
			tmpbuddy = buddies->data;
			tmpgroup = purple_buddy_get_group(tmpbuddy);
			groups = g_slist_append(groups, tmpgroup->name);
			buddies = g_slist_remove(buddies, tmpbuddy);
		}

		jabber_roster_update(gc->proto_data, buddy->name, groups);
		g_slist_free(groups);
	} else {
		JabberIq *iq = jabber_iq_new_query(gc->proto_data, JABBER_IQ_SET,
				"jabber:iq:roster");
		xmlnode *query = xmlnode_get_child(iq->node, "query");
		xmlnode *item = xmlnode_new_child(query, "item");

		xmlnode_set_attrib(item, "jid", buddy->name);
		xmlnode_set_attrib(item, "subscription", "remove");

		jabber_iq_send(iq);
	}
}
