/*
 * gaim - Jabber Protocol Plugin
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include "internal.h"
#include "debug.h"
#include "server.h"
#include "util.h"

#include "buddy.h"
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

static void remove_gaim_buddies(JabberStream *js, const char *jid)
{
	GSList *buddies, *l;

	buddies = gaim_find_buddies(js->gc->account, jid);

	for(l = buddies; l; l = l->next)
		gaim_blist_remove_buddy(l->data);

	g_slist_free(buddies);
}

static void add_gaim_buddies_in_groups(JabberStream *js, const char *jid,
		const char *alias, GSList *groups)
{
	GSList *buddies, *g2, *l;
	gchar *my_bare_jid;

	buddies = gaim_find_buddies(js->gc->account, jid);

	g2 = groups;

	if(!groups) {
		if(!buddies)
			g2 = g_slist_append(g2, g_strdup(_("Buddies")));
		else
			return;
	}

	my_bare_jid = g_strdup_printf("%s@%s", js->user->node, js->user->domain);

	while(buddies) {
		GaimBuddy *b = buddies->data;
		GaimGroup *g = gaim_buddy_get_group(b);

		buddies = g_slist_remove(buddies, b);

		if((l = g_slist_find_custom(g2, g->name, (GCompareFunc)strcmp))) {
			const char *servernick;

			if((servernick = gaim_blist_node_get_string((GaimBlistNode*)b, "servernick")))
				serv_got_alias(js->gc, jid, servernick);

			if(alias && (!b->alias || strcmp(b->alias, alias)))
				gaim_blist_alias_buddy(b, alias);
			g_free(l->data);
			g2 = g_slist_delete_link(g2, l);
		} else {
			gaim_blist_remove_buddy(b);
		}
	}

	while(g2) {
		GaimBuddy *b = gaim_buddy_new(js->gc->account, jid, alias);
		GaimGroup *g = gaim_find_group(g2->data);

		if(!g) {
			g = gaim_group_new(g2->data);
			gaim_blist_add_group(g, NULL);
		}

		gaim_blist_add_buddy(b, NULL, g, NULL);
		gaim_blist_alias_buddy(b, alias);

		/* If we just learned about ourself, then fake our status,
		 * because we won't be receiving a normal presence message
		 * about ourself. */
		if(!strcmp(b->name, my_bare_jid)) {
			GaimPresence *gpresence;
			GaimStatus *status;

			gpresence = gaim_account_get_presence(js->gc->account);
			status = gaim_presence_get_active_status(gpresence);
			jabber_presence_fake_to_self(js, status);
		}

		g_free(g2->data);
		g2 = g_slist_delete_link(g2, g2);
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
					gaim_account_get_username(js->gc->account)));

		g_free(from_norm);

		if(invalid)
			return;
	}

	query = xmlnode_get_child(packet, "query");
	if(!query)
		return;

	js->roster_parsed = TRUE;

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
			username = gaim_account_get_username(js->gc->account);
			me = g_utf8_collate(jid_norm,
			                    jabber_normalize(js->gc->account,
			                                     username));

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
				gaim_prpl_got_user_status(js->gc->account, jid, "offline", NULL);
			}
			*/
		}

		if(ask && !strcmp(ask, "subscribe"))
			jb->subscription |= JABBER_SUB_PENDING;
		else
			jb->subscription &= ~JABBER_SUB_PENDING;

		if(jb->subscription == JABBER_SUB_REMOVE) {
			remove_gaim_buddies(js, jid);
		} else {
			GSList *groups = NULL;
			for(group = xmlnode_get_child(item, "group"); group; group = xmlnode_get_next_twin(group)) {
				char *group_name;

				if(!(group_name = xmlnode_get_data(group)))
					group_name = g_strdup("");

				if (g_slist_find_custom(groups, group_name, (GCompareFunc)gaim_utf8_strcasecmp) == NULL)
					groups = g_slist_append(groups, group_name);
			}
			add_gaim_buddies_in_groups(js, jid, name, groups);
		}
	}
}

static void jabber_roster_update(JabberStream *js, const char *name,
		GSList *grps)
{
	GaimBuddy *b;
	GaimGroup *g;
	GSList *groups = NULL, *l;
	JabberIq *iq;
	xmlnode *query, *item, *group;

	if(grps) {
		groups = grps;
	} else {
		GSList *buddies = gaim_find_buddies(js->gc->account, name);
		if(!buddies)
			return;
		while(buddies) {
			b = buddies->data;
			g = gaim_buddy_get_group(b);
			groups = g_slist_append(groups, g->name);
			buddies = g_slist_remove(buddies, b);
		}
	}

	if(!(b = gaim_find_buddy(js->gc->account, name)))
		return;

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

	jabber_iq_send(iq);
}

void jabber_roster_add_buddy(GaimConnection *gc, GaimBuddy *buddy,
		GaimGroup *group)
{
	JabberStream *js = gc->proto_data;
	char *who;
	GSList *groups = NULL;
	JabberBuddy *jb;
	JabberBuddyResource *jbr;
	char *my_bare_jid;

	if(!js->roster_parsed)
		return;

	if(!(who = jabber_get_bare_jid(buddy->name)))
		return;

	jb = jabber_buddy_find(js, buddy->name, FALSE);

	if(!jb || !(jb->subscription & JABBER_SUB_TO)) {
		groups = g_slist_append(groups, group->name);
	}

	jabber_roster_update(js, who, groups);

	my_bare_jid = g_strdup_printf("%s@%s", js->user->node, js->user->domain);
	if(!strcmp(who, my_bare_jid)) {
		GaimPresence *gpresence;
		GaimStatus *status;

		gpresence = gaim_account_get_presence(js->gc->account);
		status = gaim_presence_get_active_status(gpresence);
		jabber_presence_fake_to_self(js, status);
	} else if(!jb || !(jb->subscription & JABBER_SUB_TO)) {
		jabber_presence_subscription_set(js, who, "subscribe");
	} else if((jbr =jabber_buddy_find_resource(jb, NULL))) {
		gaim_prpl_got_user_status(gc->account, who,
				jabber_buddy_state_get_status_id(jbr->state),
				"priority", jbr->priority, jbr->status ? "message" : NULL, jbr->status, NULL);
	}

	g_free(my_bare_jid);
	g_free(who);
}

void jabber_roster_alias_change(GaimConnection *gc, const char *name, const char *alias)
{
	GaimBuddy *b = gaim_find_buddy(gc->account, name);

	gaim_blist_alias_buddy(b, alias);

	jabber_roster_update(gc->proto_data, name, NULL);
}

void jabber_roster_group_change(GaimConnection *gc, const char *name,
		const char *old_group, const char *new_group)
{
	GSList *buddies, *groups = NULL;
	GaimBuddy *b;
	GaimGroup *g;

	if(!old_group || !new_group || !strcmp(old_group, new_group))
		return;

	buddies = gaim_find_buddies(gc->account, name);
	while(buddies) {
		b = buddies->data;
		g = gaim_buddy_get_group(b);
		if(!strcmp(g->name, old_group))
			groups = g_slist_append(groups, (char*)new_group); /* ick */
		else
			groups = g_slist_append(groups, g->name);
		buddies = g_slist_remove(buddies, b);
	}
	jabber_roster_update(gc->proto_data, name, groups);
	g_slist_free(groups);
}

void jabber_roster_group_rename(GaimConnection *gc, const char *old_name,
		GaimGroup *group, GList *moved_buddies)
{
	GList *l;
	for(l = moved_buddies; l; l = l->next) {
		GaimBuddy *buddy = l->data;
		jabber_roster_group_change(gc, buddy->name, old_name, group->name);
	}
}

void jabber_roster_remove_buddy(GaimConnection *gc, GaimBuddy *buddy,
		GaimGroup *group) {
	GSList *buddies = gaim_find_buddies(gc->account, buddy->name);
	GSList *groups = NULL;

	buddies = g_slist_remove(buddies, buddy);
	if(g_slist_length(buddies)) {
		GaimBuddy *tmpbuddy;
		GaimGroup *tmpgroup;

		while(buddies) {
			tmpbuddy = buddies->data;
			tmpgroup = gaim_buddy_get_group(tmpbuddy);
			groups = g_slist_append(groups, tmpgroup->name);
			buddies = g_slist_remove(buddies, tmpbuddy);
		}

		jabber_roster_update(gc->proto_data, buddy->name, groups);
	} else {
		JabberIq *iq = jabber_iq_new_query(gc->proto_data, JABBER_IQ_SET,
				"jabber:iq:roster");
		xmlnode *query = xmlnode_get_child(iq->node, "query");
		xmlnode *item = xmlnode_new_child(query, "item");

		xmlnode_set_attrib(item, "jid", buddy->name);
		xmlnode_set_attrib(item, "subscription", "remove");

		jabber_iq_send(iq);
	}

	if(buddies)
		g_slist_free(buddies);
	if(groups)
		g_slist_free(groups);
}
