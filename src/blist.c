/*
 * gaim
 *
 * Copyright (C) 2003,      Sean Egan    <sean.egan@binghamton.edu>
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
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
#include "blist.h"
#include "conversation.h"
#include "debug.h"
#include "multi.h"
#include "notify.h"
#include "prefs.h"
#include "privacy.h"
#include "prpl.h"
#include "server.h"
#include "signals.h"
#include "util.h"

#define PATHSIZE 1024

struct gaim_buddy_list *gaimbuddylist = NULL;
static struct gaim_blist_ui_ops *blist_ui_ops = NULL;


/*****************************************************************************
 * Private Utility functions                                                 *
 *****************************************************************************/
static GaimBlistNode *gaim_blist_get_last_sibling(GaimBlistNode *node)
{
	GaimBlistNode *n = node;
	if (!n)
		return NULL;
	while (n->next)
		n = n->next;
	return n;
}

static GaimBlistNode *gaim_blist_get_last_child(GaimBlistNode *node)
{
	if (!node)
		return NULL;
	return gaim_blist_get_last_sibling(node->child);
}

struct _gaim_hbuddy {
	char *name;
	GaimAccount *account;
	GaimBlistNode *group;
};

static guint _gaim_blist_hbuddy_hash (struct _gaim_hbuddy *hb)
{
	return g_str_hash(hb->name);
}

static guint _gaim_blist_hbuddy_equal (struct _gaim_hbuddy *hb1, struct _gaim_hbuddy *hb2)
{
	return ((!strcmp(hb1->name, hb2->name)) && hb1->account == hb2->account && hb1->group == hb2->group);
}

static void _gaim_blist_hbuddy_free_key(struct _gaim_hbuddy *hb)
{
	g_free(hb->name);
	g_free(hb);
}

static void blist_pref_cb(const char *name, GaimPrefType typ, gpointer value, gpointer data)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	GaimBlistNode *gnode, *cnode, *bnode;

	if (!ops)
		return;

	for(gnode = gaimbuddylist->root; gnode; gnode = gnode->next) {
		if(!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;
		for(cnode = gnode->child; cnode; cnode = cnode->next) {
			if(GAIM_BLIST_NODE_IS_CONTACT(cnode)) {
				for(bnode = cnode->child; bnode; bnode = bnode->next) {
					if(!GAIM_BLIST_NODE_IS_BUDDY(bnode))
						continue;
					ops->update(gaimbuddylist, bnode);
				}
			} else if(GAIM_BLIST_NODE_IS_CHAT(cnode)) {
				ops->update(gaimbuddylist, cnode);
			}
		}
	}
}

static GaimContact *gaim_buddy_get_contact(GaimBuddy *buddy)
{
	return (GaimContact*)((GaimBlistNode*)buddy)->parent;
}

static void gaim_contact_compute_priority_buddy(GaimContact *contact) {
	GaimBlistNode *bnode;
	contact->priority = NULL;

	for(bnode = ((GaimBlistNode*)contact)->child; bnode; bnode = bnode->next) {
		GaimBuddy *buddy;
		if(!GAIM_BLIST_NODE_IS_BUDDY(bnode))
			continue;
		buddy = (GaimBuddy*)bnode;
		if(!gaim_account_is_connected(buddy->account))
			continue;

		if(!contact->priority) {
			contact->priority = buddy;
		} else if(GAIM_BUDDY_IS_ONLINE(buddy)) {
			if(!GAIM_BUDDY_IS_ONLINE(contact->priority)) {
				contact->priority = buddy;
			} else if(!(buddy->uc & UC_UNAVAILABLE) && !buddy->idle &&
					(contact->priority->uc & UC_UNAVAILABLE ||
					 contact->priority->idle)) {
				contact->priority = buddy;
			} else if(!buddy->idle && contact->priority->idle) {
				contact->priority = buddy;
			} else if(contact->priority->uc & UC_UNAVAILABLE &&
					contact->priority->idle && (!(buddy->uc & UC_UNAVAILABLE) ||
						!buddy->idle)) {
				contact->priority = buddy;
			}
		}
	}
}


/*****************************************************************************
 * Public API functions                                                      *
 *****************************************************************************/

struct gaim_buddy_list *gaim_blist_new()
{
	struct gaim_buddy_list *gbl = g_new0(struct gaim_buddy_list, 1);

	gbl->ui_ops = gaim_get_blist_ui_ops();

	gbl->buddies = g_hash_table_new_full((GHashFunc)_gaim_blist_hbuddy_hash,
					 (GEqualFunc)_gaim_blist_hbuddy_equal,
					 (GDestroyNotify)_gaim_blist_hbuddy_free_key, NULL);

	if (gbl->ui_ops != NULL && gbl->ui_ops->new_list != NULL)
		gbl->ui_ops->new_list(gbl);

	gaim_prefs_connect_callback("/core/buddies/use_server_alias",
								blist_pref_cb, NULL);


	return gbl;
}

void
gaim_set_blist(struct gaim_buddy_list *list)
{
	gaimbuddylist = list;
}

struct gaim_buddy_list *
gaim_get_blist(void)
{
	return gaimbuddylist;
}

void  gaim_blist_show ()
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	if (ops)
		ops->show(gaimbuddylist);
}

void gaim_blist_destroy()
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	if (ops)
		ops->destroy(gaimbuddylist);
}

void  gaim_blist_set_visible (gboolean show)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	if (ops)
		ops->set_visible(gaimbuddylist, show);
}

void  gaim_blist_update_buddy_status (GaimBuddy *buddy, int status)
{
	struct gaim_blist_ui_ops *ops;

	if (buddy->uc == status)
		return;

	ops = gaimbuddylist->ui_ops;

	if((status & UC_UNAVAILABLE) != (buddy->uc & UC_UNAVAILABLE)) {
		if(status & UC_UNAVAILABLE)
			gaim_signal_emit(gaim_blist_get_handle(), "buddy-away", buddy);
		else
			gaim_signal_emit(gaim_blist_get_handle(), "buddy-back", buddy);
	}

	buddy->uc = status;
	gaim_contact_compute_priority_buddy(gaim_buddy_get_contact(buddy));
	if (ops)
		ops->update(gaimbuddylist, (GaimBlistNode*)buddy);
}

static gboolean presence_update_timeout_cb(GaimBuddy *buddy) {
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	GaimConversation *conv;

	conv = gaim_find_conversation_with_account(buddy->name, buddy->account);

	if(buddy->present == GAIM_BUDDY_SIGNING_ON) {
		buddy->present = GAIM_BUDDY_ONLINE;
	} else if(buddy->present == GAIM_BUDDY_SIGNING_OFF) {
		buddy->present = GAIM_BUDDY_OFFLINE;
		((GaimContact*)((GaimBlistNode*)buddy)->parent)->online--;
		if(((GaimContact*)((GaimBlistNode*)buddy)->parent)->online == 0)
			((GaimGroup *)((GaimBlistNode *)buddy)->parent->parent)->online--;
	}

	buddy->timer = 0;

	if (ops)
		ops->update(gaimbuddylist, (GaimBlistNode*)buddy);

	if (conv) {
		if (buddy->present == GAIM_BUDDY_ONLINE)
			gaim_conversation_update(conv, GAIM_CONV_ACCOUNT_ONLINE);
		else if (buddy->present == GAIM_BUDDY_OFFLINE)
			gaim_conversation_update(conv, GAIM_CONV_ACCOUNT_OFFLINE);
	}

	return FALSE;
}

void gaim_blist_update_buddy_presence(GaimBuddy *buddy, int presence) {
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	gboolean do_something = FALSE;

	if (!GAIM_BUDDY_IS_ONLINE(buddy) && presence) {
		int old_present = buddy->present;
		buddy->present = GAIM_BUDDY_SIGNING_ON;
		gaim_signal_emit(gaim_blist_get_handle(), "buddy-signed-on", buddy);
		do_something = TRUE;

		if(old_present != GAIM_BUDDY_SIGNING_OFF) {
			((GaimContact*)((GaimBlistNode*)buddy)->parent)->online++;
			if(((GaimContact*)((GaimBlistNode*)buddy)->parent)->online == 1)
				((GaimGroup *)((GaimBlistNode *)buddy)->parent->parent)->online++;
		}
	} else if(GAIM_BUDDY_IS_ONLINE(buddy) && !presence) {
		buddy->present = GAIM_BUDDY_SIGNING_OFF;
		gaim_signal_emit(gaim_blist_get_handle(), "buddy-signed-off", buddy);
		do_something = TRUE;
	}

	if(do_something) {
		if(buddy->timer > 0)
			g_source_remove(buddy->timer);
		buddy->timer = g_timeout_add(10000, (GSourceFunc)presence_update_timeout_cb, buddy);

		gaim_contact_compute_priority_buddy(gaim_buddy_get_contact(buddy));
		if (ops)
			ops->update(gaimbuddylist, (GaimBlistNode*)buddy);
	}
}


void  gaim_blist_update_buddy_idle (GaimBuddy *buddy, int idle)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	if(buddy->idle == idle)
		return;

	buddy->idle = idle;
	gaim_contact_compute_priority_buddy(gaim_buddy_get_contact(buddy));
	if (ops)
		ops->update(gaimbuddylist, (GaimBlistNode*)buddy);
}

void  gaim_blist_update_buddy_evil (GaimBuddy *buddy, int warning)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	if(buddy->evil == warning)
		return;

	buddy->evil = warning;
	if (ops)
		ops->update(gaimbuddylist,(GaimBlistNode*)buddy);
}

void gaim_blist_update_buddy_icon(GaimBuddy *buddy) {
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	if(ops)
		ops->update(gaimbuddylist, (GaimBlistNode*)buddy);
}

void  gaim_blist_rename_buddy (GaimBuddy *buddy, const char *name)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	g_free(buddy->name);
	buddy->name = g_strdup(name);
	if (ops)
		ops->update(gaimbuddylist, (GaimBlistNode*)buddy);
}

void gaim_blist_alias_chat(GaimBlistChat *chat, const char *alias)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;

	g_free(chat->alias);

	if(alias && strlen(alias))
		chat->alias = g_strdup(alias);
	else
		chat->alias = NULL;

	if(ops)
		ops->update(gaimbuddylist, (GaimBlistNode*)chat);
}

void  gaim_blist_alias_buddy (GaimBuddy *buddy, const char *alias)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	GaimConversation *conv;

	g_free(buddy->alias);

	if(alias && strlen(alias))
		buddy->alias = g_strdup(alias);
	else
		buddy->alias = NULL;

	if (ops)
		ops->update(gaimbuddylist, (GaimBlistNode*)buddy);

	conv = gaim_find_conversation_with_account(buddy->name, buddy->account);

	if (conv)
		gaim_conversation_autoset_title(conv);
}

void  gaim_blist_server_alias_buddy (GaimBuddy *buddy, const char *alias)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	GaimConversation *conv;

	g_free(buddy->server_alias);

	if(alias && strlen(alias) && g_utf8_validate(alias, -1, NULL))
		buddy->server_alias = g_strdup(alias);
	else
		buddy->server_alias = NULL;

	if (ops)
		ops->update(gaimbuddylist, (GaimBlistNode*)buddy);

	conv = gaim_find_conversation_with_account(buddy->name, buddy->account);

	if (conv)
		gaim_conversation_autoset_title(conv);
}

void gaim_blist_rename_group(GaimGroup *group, const char *name)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	GaimGroup *dest_group;
	GaimBlistNode *prev, *child, *next;
	GSList *accts;

	if(!name || !strlen(name) || !strcmp(name, group->name)) {
		/* nothing to do here */
		return;
	} else if((dest_group = gaim_find_group(name))) {
		/* here we're merging two groups */
		prev = gaim_blist_get_last_child((GaimBlistNode*)dest_group);
		child = ((GaimBlistNode*)group)->child;

		while(child)
		{
			next = child->next;
			if(GAIM_BLIST_NODE_IS_CONTACT(child)) {
				GaimBlistNode *bnode;
				gaim_blist_add_contact((GaimContact *)child, dest_group, prev);
				for(bnode = child->child; bnode; bnode = bnode->next)
					gaim_blist_add_buddy((GaimBuddy*)bnode, (GaimContact*)child,
							NULL, bnode->prev);
				prev = child;
			} else if(GAIM_BLIST_NODE_IS_CHAT(child)) {
				gaim_blist_add_chat((GaimBlistChat *)child, dest_group, prev);
				prev = child;
			} else {
				gaim_debug(GAIM_DEBUG_ERROR, "blist",
						"Unknown child type in group %s\n", group->name);
			}
			child = next;
		}
		for (accts = gaim_group_get_accounts(group); accts; accts = g_slist_remove(accts, accts->data)) {
			GaimAccount *account = accts->data;
			serv_rename_group(account->gc, group, name);
		}
		gaim_blist_remove_group(group);
	} else {
		/* a simple rename */
		for (accts = gaim_group_get_accounts(group); accts; accts = g_slist_remove(accts, accts->data)) {
			GaimAccount *account = accts->data;
			serv_rename_group(account->gc, group, name);
		}
		g_free(group->name);
		group->name = g_strdup(name);
		if (ops)
			ops->update(gaimbuddylist, (GaimBlistNode*)group);
	}
}

GaimBlistChat *gaim_blist_chat_new(GaimAccount *account, const char *alias, GHashTable *components)
{
	GaimBlistChat *chat;
	struct gaim_blist_ui_ops *ops;

	if(!components)
		return NULL;

	chat = g_new0(GaimBlistChat, 1);
	chat->account = account;
	if(alias && strlen(alias))
		chat->alias = g_strdup(alias);
	chat->components = components;
	chat->settings = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, g_free);

	((GaimBlistNode*)chat)->type = GAIM_BLIST_CHAT_NODE;

	ops = gaim_get_blist_ui_ops();

	if (ops != NULL && ops->new_node != NULL)
		ops->new_node((GaimBlistNode *)chat);

	return chat;
}

char *gaim_blist_chat_get_display_name(GaimBlistChat *chat)
{
	char *name;

	if(chat->alias){
		 name = g_strdup(chat->alias);
	}
	else{
		 GList *parts;
		 GaimPlugin *prpl;
		 GaimPluginProtocolInfo *prpl_info;
		 struct proto_chat_entry *pce;

		 prpl = gaim_find_prpl(gaim_account_get_protocol(chat->account));
		 prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);

		 parts = prpl_info->chat_info(chat->account->gc);

		 pce = parts->data;
		 name = g_markup_escape_text(g_hash_table_lookup(chat->components,
														 pce->identifier), -1);
		 g_list_free(parts);
	}

	return name;
}

GaimBuddy *gaim_buddy_new(GaimAccount *account, const char *screenname, const char *alias)
{
	GaimBuddy *b;
	struct gaim_blist_ui_ops *ops;

	b = g_new0(GaimBuddy, 1);
	b->account = account;
	b->name  = g_strdup(screenname);
	b->alias = g_strdup(alias);
	b->settings = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	((GaimBlistNode*)b)->type = GAIM_BLIST_BUDDY_NODE;

	ops = gaim_get_blist_ui_ops();

	if (ops != NULL && ops->new_node != NULL)
		ops->new_node((GaimBlistNode *)b);

	return b;
}

void
gaim_buddy_set_icon(GaimBuddy *buddy, GaimBuddyIcon *icon)
{
	g_return_if_fail(buddy != NULL);

	if (buddy->icon == icon)
		return;

	if (buddy->icon != NULL)
		gaim_buddy_icon_unref(buddy->icon);

	buddy->icon = (icon == NULL ? NULL : gaim_buddy_icon_ref(icon));

	gaim_buddy_icon_cache(icon, buddy);

	gaim_blist_update_buddy_icon(buddy);
}

GaimBuddyIcon *
gaim_buddy_get_icon(const GaimBuddy *buddy)
{
	g_return_val_if_fail(buddy != NULL, NULL);

	return buddy->icon;
}

void gaim_blist_add_chat(GaimBlistChat *chat, GaimGroup *group, GaimBlistNode *node)
{
	GaimBlistNode *n = node, *cnode = (GaimBlistNode*)chat;
	GaimGroup *g = group;
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	gboolean save = FALSE;


	g_return_if_fail(chat != NULL);
	g_return_if_fail(GAIM_BLIST_NODE_IS_CHAT((GaimBlistNode*)chat));

	if (!n) {
		if (!g) {
			g = gaim_group_new(_("Chats"));
			gaim_blist_add_group(g,
					gaim_blist_get_last_sibling(gaimbuddylist->root));
		}
	} else {
		g = (GaimGroup*)n->parent;
	}

	/* if we're moving to overtop of ourselves, do nothing */
	if(cnode == n)
		return;

	if (cnode->parent) {
		/* This chat was already in the list and is
		 * being moved.
		 */
		((GaimGroup *)cnode->parent)->totalsize--;
		if (gaim_account_is_connected(chat->account)) {
			((GaimGroup *)cnode->parent)->online--;
			((GaimGroup *)cnode->parent)->currentsize--;
		}
		if(cnode->next)
			cnode->next->prev = cnode->prev;
		if(cnode->prev)
			cnode->prev->next = cnode->next;
		if(cnode->parent->child == cnode)
			cnode->parent->child = cnode->next;

		ops->remove(gaimbuddylist, cnode);

		save = TRUE;
	}

	if (n) {
		if(n->next)
			n->next->prev = cnode;
		cnode->next = n->next;
		cnode->prev = n;
		cnode->parent = n->parent;
		n->next = cnode;
		((GaimGroup *)n->parent)->totalsize++;
		if (gaim_account_is_connected(chat->account)) {
			((GaimGroup *)n->parent)->online++;
			((GaimGroup *)n->parent)->currentsize++;
		}
	} else {
		if(((GaimBlistNode*)g)->child)
			((GaimBlistNode*)g)->child->prev = cnode;
		cnode->next = ((GaimBlistNode*)g)->child;
		cnode->prev = NULL;
		((GaimBlistNode*)g)->child = cnode;
		cnode->parent = (GaimBlistNode*)g;
		g->totalsize++;
		if (gaim_account_is_connected(chat->account)) {
			g->online++;
			g->currentsize++;
		}
	}

	if (ops)
		ops->update(gaimbuddylist, (GaimBlistNode*)cnode);
	if (save)
		gaim_blist_save();
}

void  gaim_blist_add_buddy (GaimBuddy *buddy, GaimContact *contact, GaimGroup *group, GaimBlistNode *node)
{
	GaimBlistNode *cnode, *bnode;
	GaimGroup *g;
	GaimContact *c;
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	gboolean save = FALSE;
	struct _gaim_hbuddy *hb;

	g_return_if_fail(buddy != NULL);
	g_return_if_fail(GAIM_BLIST_NODE_IS_BUDDY((GaimBlistNode*)buddy));

	bnode = (GaimBlistNode *)buddy;

	/* if we're moving to overtop of ourselves, do nothing */
	if(bnode == node || (!node && bnode->parent &&
				contact && bnode->parent == (GaimBlistNode*)contact
				&& bnode == bnode->parent->child))
		return;

	if(node && GAIM_BLIST_NODE_IS_BUDDY(node)) {
		c = (GaimContact*)node->parent;
		g = (GaimGroup*)node->parent->parent;
	} else if(contact) {
		c = contact;
		g = (GaimGroup*)((GaimBlistNode*)c)->parent;
	} else  {
		if(group) {
			g = group;
		} else {
			g = gaim_group_new(_("Buddies"));
			gaim_blist_add_group(g,
					gaim_blist_get_last_sibling(gaimbuddylist->root));
		}
		c = gaim_contact_new();
		gaim_blist_add_contact(c, g,
				gaim_blist_get_last_child((GaimBlistNode*)g));
	}

	cnode = (GaimBlistNode *)c;

	if(bnode->parent) {
		if(GAIM_BUDDY_IS_ONLINE(buddy)) {
			((GaimContact*)bnode->parent)->online--;
			if(((GaimContact*)bnode->parent)->online == 0)
				((GaimGroup*)bnode->parent->parent)->online--;
		}
		if(gaim_account_is_connected(buddy->account)) {
			((GaimContact*)bnode->parent)->currentsize--;
			if(((GaimContact*)bnode->parent)->currentsize == 0)
				((GaimGroup*)bnode->parent->parent)->currentsize--;
		}
		((GaimContact*)bnode->parent)->totalsize--;
		/* the group totalsize will be taken care of by remove_contact below */

		if(bnode->parent->parent != (GaimBlistNode*)g)
			serv_move_buddy(buddy, (GaimGroup *)bnode->parent->parent, g);

		if(bnode->next)
			bnode->next->prev = bnode->prev;
		if(bnode->prev)
			bnode->prev->next = bnode->next;
		if(bnode->parent->child == bnode)
			bnode->parent->child = bnode->next;

		ops->remove(gaimbuddylist, bnode);

		save = TRUE;

		if(bnode->parent->parent != (GaimBlistNode*)g) {
			hb = g_new(struct _gaim_hbuddy, 1);
			hb->name = normalize(buddy->name);
			hb->account = buddy->account;
			hb->group = bnode->parent->parent;
			g_hash_table_remove(gaimbuddylist->buddies, hb);
			g_free(hb);
		}

		if(!bnode->parent->child) {
			gaim_blist_remove_contact((GaimContact*)bnode->parent);
		} else {
			gaim_contact_compute_priority_buddy((GaimContact*)bnode->parent);
			ops->update(gaimbuddylist, bnode->parent);
		}
	}

	if(node && GAIM_BLIST_NODE_IS_BUDDY(node)) {
		if(node->next)
			node->next->prev = bnode;
		bnode->next = node->next;
		bnode->prev = node;
		bnode->parent = node->parent;
		node->next = bnode;
	} else {
		if(cnode->child)
			cnode->child->prev = bnode;
		bnode->prev = NULL;
		bnode->next = cnode->child;
		cnode->child = bnode;
		bnode->parent = cnode;
	}

	if(GAIM_BUDDY_IS_ONLINE(buddy)) {
		((GaimContact*)bnode->parent)->online++;
		if(((GaimContact*)bnode->parent)->online == 1)
			((GaimGroup*)bnode->parent->parent)->online++;
	}
	if(gaim_account_is_connected(buddy->account)) {
		((GaimContact*)bnode->parent)->currentsize++;
		if(((GaimContact*)bnode->parent)->currentsize == 1)
			((GaimGroup*)bnode->parent->parent)->currentsize++;
	}
	((GaimContact*)bnode->parent)->totalsize++;


	hb = g_new(struct _gaim_hbuddy, 1);
	hb->name = g_strdup(normalize(buddy->name));
	hb->account = buddy->account;
	hb->group = ((GaimBlistNode*)buddy)->parent->parent;

	g_hash_table_replace(gaimbuddylist->buddies, hb, buddy);

	gaim_contact_compute_priority_buddy(gaim_buddy_get_contact(buddy));
	if (ops)
		ops->update(gaimbuddylist, (GaimBlistNode*)buddy);
	if (save)
		gaim_blist_save();
}

GaimContact *gaim_contact_new()
{
	struct gaim_blist_ui_ops *ops;
	GaimContact *c = g_new0(GaimContact, 1);
	((GaimBlistNode*)c)->type = GAIM_BLIST_CONTACT_NODE;

	c->totalsize = c->currentsize = c->online = 0;

	ops = gaim_get_blist_ui_ops();
	if (ops != NULL && ops->new_node != NULL)
		ops->new_node((GaimBlistNode *)c);

	return c;
}

void gaim_contact_set_alias(GaimContact* contact, const char *alias)
{
	g_return_if_fail(contact != NULL);

	if(contact->alias)
		g_free(contact->alias);

	contact->alias = g_strdup(alias);
}

const char *gaim_contact_get_alias(GaimContact* contact)
{
	return contact ? contact->alias : NULL;
}

GaimGroup *gaim_group_new(const char *name)
{
	GaimGroup *g = gaim_find_group(name);

	if (!g) {
		struct gaim_blist_ui_ops *ops;
		g= g_new0(GaimGroup, 1);
		g->name = g_strdup(name);
		g->totalsize = 0;
		g->currentsize = 0;
		g->online = 0;
		g->settings = g_hash_table_new_full(g_str_hash, g_str_equal,
				g_free, g_free);
		((GaimBlistNode*)g)->type = GAIM_BLIST_GROUP_NODE;

		ops = gaim_get_blist_ui_ops();

		if (ops != NULL && ops->new_node != NULL)
			ops->new_node((GaimBlistNode *)g);

	}
	return g;
}

void gaim_blist_add_contact(GaimContact *contact, GaimGroup *group, GaimBlistNode *node)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	GaimGroup *g;
	GaimBlistNode *gnode, *cnode, *bnode;
	gboolean save = FALSE;

	g_return_if_fail(contact != NULL);
	g_return_if_fail(GAIM_BLIST_NODE_IS_CONTACT((GaimBlistNode*)contact));

	if((GaimBlistNode*)contact == node)
		return;

	if(node && (GAIM_BLIST_NODE_IS_CONTACT(node) ||
				GAIM_BLIST_NODE_IS_CHAT(node)))
		g = (GaimGroup*)node->parent;
	else if(group)
		g = group;
	else {
		g = gaim_group_new(_("Buddies"));
		gaim_blist_add_group(g,
				gaim_blist_get_last_sibling(gaimbuddylist->root));
	}

	gnode = (GaimBlistNode*)g;
	cnode = (GaimBlistNode*)contact;

	if(cnode->parent) {
		if(cnode->parent->child == cnode)
			cnode->parent->child = cnode->next;
		if(cnode->prev)
			cnode->prev->next = cnode->next;
		if(cnode->next)
			cnode->next->prev = cnode->prev;


		if(contact->online > 0)
			((GaimGroup*)cnode->parent)->online--;
		if(contact->currentsize > 0)
			((GaimGroup*)cnode->parent)->currentsize--;
		((GaimGroup*)cnode->parent)->totalsize--;

		ops->remove(gaimbuddylist, cnode);

		save = TRUE;

		if(cnode->parent != gnode) {
			for(bnode = cnode->child; bnode; bnode = bnode->next) {
				GaimBuddy *b = (GaimBuddy*)bnode;

				struct _gaim_hbuddy *hb = g_new(struct _gaim_hbuddy, 1);
				hb->name = g_strdup(normalize(b->name));
				hb->account = b->account;
				hb->group = cnode->parent;

				g_hash_table_remove(gaimbuddylist->buddies, hb);

				hb->group = gnode;
				g_hash_table_replace(gaimbuddylist->buddies, hb, b);

				if(b->account->gc)
					serv_move_buddy(b, (GaimGroup*)cnode->parent, g);
			}
		}
	}


	if(node && (GAIM_BLIST_NODE_IS_CONTACT(node) ||
				GAIM_BLIST_NODE_IS_CHAT(node))) {
		if(node->next)
			node->next->prev = cnode;
		cnode->next = node->next;
		cnode->prev = node;
		cnode->parent = node->parent;
		node->next = cnode;
	} else {
		if(gnode->child)
			gnode->child->prev = cnode;
		cnode->prev = NULL;
		cnode->next = gnode->child;
		gnode->child = cnode;
		cnode->parent = gnode;
	}

	if(contact->online > 0)
		g->online++;
	if(contact->currentsize > 0)
		g->currentsize++;
	g->totalsize++;

	if(ops && cnode->child)
		ops->update(gaimbuddylist, cnode);

	for(bnode = cnode->child; bnode; bnode = bnode->next)
		ops->update(gaimbuddylist, bnode);

	if (save)
		gaim_blist_save();
}

void gaim_blist_merge_contact(GaimContact *source, GaimContact *target)
{
	GaimBlistNode *sourcenode = (GaimBlistNode*)source;
	GaimBlistNode *targetnode = (GaimBlistNode*)target;
	GaimBlistNode *child, *child2;

	if(source == target)
		return;

	child = sourcenode->child;

	while(child) {
		child2 = child;
		child = child->next;
		if(GAIM_BLIST_NODE_IS_BUDDY(child2))
			gaim_blist_add_buddy((GaimBuddy*)child2, target, NULL,
					gaim_blist_get_last_child(targetnode));
	}
}

void  gaim_blist_add_group (GaimGroup *group, GaimBlistNode *node)
{
	struct gaim_blist_ui_ops *ops;
	GaimBlistNode *gnode = (GaimBlistNode*)group;
	gboolean save = FALSE;

	g_return_if_fail(group != NULL);
	g_return_if_fail(GAIM_BLIST_NODE_IS_GROUP((GaimBlistNode*)group));

	if (!gaimbuddylist)
		gaimbuddylist = gaim_blist_new();
	ops = gaimbuddylist->ui_ops;

	if (!gaimbuddylist->root) {
		gaimbuddylist->root = gnode;
		return;
	}

	/* if we're moving to overtop of ourselves, do nothing */
	if(gnode == node)
		return;

	if (gaim_find_group(group->name)) {
		/* This is just being moved */

		ops->remove(gaimbuddylist, (GaimBlistNode*)group);

		if(gnode == gaimbuddylist->root)
			gaimbuddylist->root = gnode->next;
		if(gnode->prev)
			gnode->prev->next = gnode->next;
		if(gnode->next)
			gnode->next->prev = gnode->prev;

		save = TRUE;
	}

	if (node && GAIM_BLIST_NODE_IS_GROUP(node)) {
		gnode->next = node->next;
		gnode->prev = node;
		if(node->next)
			node->next->prev = gnode;
		node->next = gnode;
	} else {
		if(gaimbuddylist->root)
			gaimbuddylist->root->prev = gnode;
		gnode->next = gaimbuddylist->root;
		gnode->prev = NULL;
		gaimbuddylist->root = gnode;
	}


	if (ops) {
		ops->update(gaimbuddylist, gnode);
		for(node = gnode->child; node; node = node->next)
			ops->update(gaimbuddylist, node);
	}
	if (save)
		gaim_blist_save();
}

void gaim_blist_remove_contact(GaimContact* contact)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;

	GaimBlistNode *gnode, *cnode = (GaimBlistNode*)contact;

	gnode = cnode->parent;

	if(cnode->child) {
		while(cnode->child) {
			gaim_blist_remove_buddy((GaimBuddy*)cnode->child);
		}
	} else {
		if(ops)
			ops->remove(gaimbuddylist, cnode);

		if(gnode->child == cnode)
			gnode->child = cnode->next;
		if(cnode->prev)
			cnode->prev->next = cnode->next;
		if(cnode->next)
			cnode->next->prev = cnode->prev;

		g_free(contact);
	}
}

void gaim_blist_remove_buddy (GaimBuddy *buddy)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;

	GaimBlistNode *cnode, *node = (GaimBlistNode*)buddy;
	GaimGroup *group;
	struct _gaim_hbuddy hb;

	cnode = node->parent;
	group = (GaimGroup *)cnode->parent;

	if(GAIM_BUDDY_IS_ONLINE(buddy)) {
		((GaimContact*)cnode)->online--;
		if(((GaimContact*)cnode)->online == 0)
			group->online--;
	}
	if(gaim_account_is_connected(buddy->account)) {
		((GaimContact*)cnode)->currentsize--;
		if(((GaimContact*)cnode)->currentsize == 0)
			group->currentsize--;
	}
	((GaimContact*)cnode)->totalsize--;

	if (node->prev)
		node->prev->next = node->next;
	if (node->next)
		node->next->prev = node->prev;
	if(cnode->child == node) {
		cnode->child = node->next;
	}


	hb.name = normalize(buddy->name);
	hb.account = buddy->account;
	hb.group = ((GaimBlistNode*)buddy)->parent->parent;
	g_hash_table_remove(gaimbuddylist->buddies, &hb);

	if(buddy->timer > 0)
		g_source_remove(buddy->timer);

	if (buddy->icon != NULL)
		gaim_buddy_icon_unref(buddy->icon);

	ops->remove(gaimbuddylist, node);
	g_hash_table_destroy(buddy->settings);
	g_free(buddy->name);
	g_free(buddy->alias);
	g_free(buddy);

	if(!cnode->child)
		gaim_blist_remove_contact((GaimContact*)cnode);
}

void  gaim_blist_remove_chat (GaimBlistChat *chat)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;

	GaimBlistNode *gnode, *node = (GaimBlistNode*)chat;
	GaimGroup *group;

	gnode = node->parent;
	group = (GaimGroup *)gnode;

	if(gnode->child == node)
		gnode->child = node->next;
	if (node->prev)
		node->prev->next = node->next;
	if (node->next)
		node->next->prev = node->prev;
	group->totalsize--;
	if (gaim_account_is_connected(chat->account)) {
		group->currentsize--;
		group->online--;
	}

	ops->remove(gaimbuddylist, node);
	g_hash_table_destroy(chat->components);
	g_free(chat->alias);
	g_free(chat);
}

void  gaim_blist_remove_group (GaimGroup *group)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	GaimBlistNode *node = (GaimBlistNode*)group;
	GList *l;

	if(node->child) {
		char *buf;
		int count = 0;
		GaimBlistNode *child = node->child;

		while(child) {
			count++;
			child = child->next;
		}

		buf = g_strdup_printf(ngettext("%d buddy from group %s was not removed "
									   "because its account was not logged in."
									   "  This buddy and the group were not "
									   "removed.\n", 
									   "%d buddies from group %s were not "
									   "removed because their accounts were "
									   "not logged in.  These buddies and "
									   "the group were not removed.\n", count),
							  count, group->name);
		gaim_notify_error(NULL, NULL, _("Group not removed"), buf);
		g_free(buf);
		return;
	}

	if(gaimbuddylist->root == node)
		gaimbuddylist->root = node->next;
	if (node->prev)
		node->prev->next = node->next;
	if (node->next)
		node->next->prev = node->prev;

	for (l = gaim_connections_get_all(); l != NULL; l = l->next)
	{
		GaimConnection *gc = (GaimConnection *)l->data;

		if (gaim_connection_get_state(gc) == GAIM_CONNECTED)
			serv_remove_group(gc, group->name);
	}

	ops->remove(gaimbuddylist, node);
	g_free(group->name);
	g_free(group);
}

GaimBuddy *gaim_contact_get_priority_buddy(GaimContact *contact) {
	return contact->priority;
}

const char *gaim_get_buddy_alias_only(GaimBuddy *b) {
	if(!b)
		return NULL;

	if(b->alias && b->alias[0]) {
		return b->alias;
	}
	else if (b->server_alias != NULL &&
			 gaim_prefs_get_bool("/core/buddies/use_server_alias")) {

		return b->server_alias;
	}

	return NULL;
}

const char *  gaim_get_buddy_alias (GaimBuddy *buddy)
{
	GaimContact *contact;
	const char *ret;

	if(!buddy)
		return _("Unknown");

	contact = (GaimContact*)((GaimBlistNode*)buddy)->parent;

	if(contact && contact->alias)
		return contact->alias;

	ret= gaim_get_buddy_alias_only(buddy);

	return ret ? ret : buddy->name;
}

const char *gaim_blist_chat_get_name(GaimBlistChat *chat)
{
	if(chat->alias && *chat->alias) {
		return chat->alias;
	} else {
		struct proto_chat_entry *pce;
		GList *parts, *tmp;
		char *ret;

		parts = GAIM_PLUGIN_PROTOCOL_INFO(chat->account->gc->prpl)->chat_info(chat->account->gc);
		pce = parts->data;
		ret = g_hash_table_lookup(chat->components, pce->identifier);
		for(tmp = parts; tmp; tmp = tmp->next)
			g_free(tmp->data);
		g_list_free(parts);

		return ret;
	}
}

GaimBuddy *gaim_find_buddy(GaimAccount *account, const char *name)
{
	GaimBuddy *buddy;
	struct _gaim_hbuddy hb;
	GaimBlistNode *group;

	if (!gaimbuddylist)
		return NULL;

	if (!name)
		return NULL;

	hb.name = normalize(name);
	hb.account = account;

	for(group = gaimbuddylist->root; group; group = group->next) {
		hb.group = group;
		if ((buddy = g_hash_table_lookup(gaimbuddylist->buddies, &hb)) != NULL)
			return buddy;
	}

	return NULL;
}

GaimBuddy *gaim_find_buddy_in_group(GaimAccount *account, const char *name,
		GaimGroup *group)
{
	struct _gaim_hbuddy hb;

	if (!gaimbuddylist)
		return NULL;

	if (!name)
		return NULL;

	hb.name = normalize(name);
	hb.account = account;
	hb.group = (GaimBlistNode*)group;

	return g_hash_table_lookup(gaimbuddylist->buddies, &hb);
}

GSList *gaim_find_buddies(GaimAccount *account, const char *name)
{
	struct buddy *buddy;
	struct _gaim_hbuddy hb;
	GaimBlistNode *group;
	GSList *ret = NULL;

	if (!gaimbuddylist)
		return NULL;

	if (!name)
		return NULL;

	hb.name = normalize(name);
	hb.account = account;

	for(group = gaimbuddylist->root; group; group = group->next) {
		hb.group = group;
		if ((buddy = g_hash_table_lookup(gaimbuddylist->buddies, &hb)) != NULL)
			ret = g_slist_append(ret, buddy);
	}

	return ret;
}

GaimGroup *gaim_find_group(const char *name)
{
	GaimBlistNode *node;
	if (!gaimbuddylist)
		return NULL;
	node = gaimbuddylist->root;
	while(node) {
		if (!strcmp(((GaimGroup *)node)->name, name))
			return (GaimGroup *)node;
		node = node->next;
	}
	return NULL;
}

GaimBlistChat *
gaim_blist_find_chat(GaimAccount *account, const char *name)
{
	char *chat_name;
	GaimBlistChat *chat;
	GaimPlugin *prpl;
	GaimPluginProtocolInfo *prpl_info = NULL;
	struct proto_chat_entry *pce;
	GaimBlistNode *node, *group;
	GList *parts;

	g_return_val_if_fail(gaim_get_blist() != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	for (group = gaimbuddylist->root; group != NULL; group = group->next) {
		for (node = group->child; node != NULL; node = node->next) {
			if (GAIM_BLIST_NODE_IS_CHAT(node)) {

				chat = (GaimBlistChat*)node;

				prpl = gaim_find_prpl(gaim_account_get_protocol(chat->account));
				prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);

				parts = prpl_info->chat_info(
					gaim_account_get_connection(chat->account));

				pce = parts->data;
				chat_name = g_hash_table_lookup(chat->components,
												pce->identifier);

				if (chat->account == account &&
					name != NULL && !strcmp(chat_name, name)) {

					return chat;
				}
			}
		}
	}

	return NULL;
}

GaimGroup *
gaim_blist_chat_get_group(GaimBlistChat *chat)
{
	g_return_val_if_fail(chat != NULL, NULL);

	return (GaimGroup *)(((GaimBlistNode *)chat)->parent);
}

GaimGroup *gaim_find_buddys_group(GaimBuddy *buddy)
{
	if (!buddy)
		return NULL;

	if (((GaimBlistNode *)buddy)->parent == NULL)
		return NULL;

	return (GaimGroup *)(((GaimBlistNode*)buddy)->parent->parent);
}

GSList *gaim_group_get_accounts(GaimGroup *g)
{
	GSList *l = NULL;
	GaimBlistNode *gnode, *cnode, *bnode;

	gnode = (GaimBlistNode *)g;

	for(cnode = gnode->child;  cnode; cnode = cnode->next) {
		if (GAIM_BLIST_NODE_IS_CHAT(cnode)) {
			if(!g_slist_find(l, ((GaimBlistChat *)cnode)->account))
				l = g_slist_append(l, ((GaimBlistChat *)cnode)->account);
		} else if(GAIM_BLIST_NODE_IS_CONTACT(cnode)) {
			for(bnode = cnode->child; bnode; bnode = bnode->next) {
				if(GAIM_BLIST_NODE_IS_BUDDY(bnode)) {
					if(!g_slist_find(l, ((GaimBuddy *)bnode)->account))
						l = g_slist_append(l, ((GaimBuddy *)bnode)->account);
				}
			}
		}
	}

	return l;
}

void gaim_blist_add_account(GaimAccount *account)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	GaimBlistNode *gnode, *cnode, *bnode;

	if(!gaimbuddylist)
		return;

	if(!ops)
		return;

	for(gnode = gaimbuddylist->root; gnode; gnode = gnode->next) {
		if(!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;
		for(cnode = gnode->child; cnode; cnode = cnode->next) {
			if(GAIM_BLIST_NODE_IS_CONTACT(cnode)) {
				gboolean recompute = FALSE;
					for(bnode = cnode->child; bnode; bnode = bnode->next) {
						if(GAIM_BLIST_NODE_IS_BUDDY(bnode) &&
								((GaimBuddy*)bnode)->account == account) {
							recompute = TRUE;
							((GaimContact*)cnode)->currentsize++;
							if(((GaimContact*)cnode)->currentsize == 1)
								((GaimGroup*)gnode)->currentsize++;
							if(GAIM_BUDDY_IS_ONLINE((GaimBuddy*)bnode)) {
								((GaimContact*)cnode)->online++;
								if(((GaimContact*)cnode)->online == 1)
									((GaimGroup*)gnode)->online++;
							}
							ops->update(gaimbuddylist, bnode);
						}
					}
					if(recompute) {
						gaim_contact_compute_priority_buddy((GaimContact*)cnode);
						ops->update(gaimbuddylist, cnode);
					}
			} else if(GAIM_BLIST_NODE_IS_CHAT(cnode) &&
					((GaimBlistChat*)cnode)->account == account) {
				((GaimGroup *)gnode)->online++;
				((GaimGroup *)gnode)->currentsize++;
				ops->update(gaimbuddylist, cnode);
			}
		}
		ops->update(gaimbuddylist, gnode);
	}
}

void gaim_blist_remove_account(GaimAccount *account)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	GaimBlistNode *gnode, *cnode, *bnode;

	if (!gaimbuddylist)
		return;

	for(gnode = gaimbuddylist->root; gnode; gnode = gnode->next) {
		if(!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;
		for(cnode = gnode->child; cnode; cnode = cnode->next) {
			if(GAIM_BLIST_NODE_IS_CONTACT(cnode)) {
				gboolean recompute = FALSE;
				for(bnode = cnode->child; bnode; bnode = bnode->next) {
					if(!GAIM_BLIST_NODE_IS_BUDDY(bnode))
						continue;
					if(account == ((GaimBuddy *)bnode)->account) {
						recompute = TRUE;
						if(((GaimBuddy*)bnode)->present == GAIM_BUDDY_ONLINE ||
								((GaimBuddy*)bnode)->present == GAIM_BUDDY_SIGNING_ON) {
							((GaimContact*)cnode)->online--;
							if(((GaimContact*)cnode)->online == 0)
								((GaimGroup*)gnode)->online--;
						}
						((GaimContact*)cnode)->currentsize--;
						if(((GaimContact*)cnode)->currentsize == 0)
							((GaimGroup*)gnode)->currentsize--;

						((GaimBuddy*)bnode)->present = GAIM_BUDDY_OFFLINE;

						((GaimBuddy*)bnode)->uc = 0;
						((GaimBuddy*)bnode)->idle = 0;
						((GaimBuddy*)bnode)->evil = 0;


						if(ops)
							ops->remove(gaimbuddylist, bnode);
					}
				}
				if(recompute) {
					gaim_contact_compute_priority_buddy((GaimContact*)cnode);
					if(ops)
						ops->update(gaimbuddylist, cnode);
				}
			} else if(GAIM_BLIST_NODE_IS_CHAT(cnode) &&
					((GaimBlistChat*)cnode)->account == account) {
				((GaimGroup*)gnode)->currentsize--;
				((GaimGroup*)gnode)->online--;
				if(ops)
					ops->remove(gaimbuddylist, cnode);
			}
		}
	}
}

void parse_toc_buddy_list(GaimAccount *account, char *config)
{
	char *c;
	char current[256];
	GList *bud = NULL;


	if (config != NULL) {

		/* skip "CONFIG:" (if it exists) */
		c = strncmp(config + 6 /* sizeof(struct sflap_hdr) */ , "CONFIG:", strlen("CONFIG:")) ?
			strtok(config, "\n") :
			strtok(config + 6 /* sizeof(struct sflap_hdr) */  + strlen("CONFIG:"), "\n");
		do {
			if (c == NULL)
				break;
			if (*c == 'g') {
				char *utf8 = NULL;
				utf8 = gaim_try_conv_to_utf8(c + 2);
				if (utf8 == NULL) {
					g_strlcpy(current, _("Invalid Groupname"), sizeof(current));
				} else {
					g_strlcpy(current, utf8, sizeof(current));
					g_free(utf8);
				}
				if (!gaim_find_group(current)) {
					GaimGroup *g = gaim_group_new(current);
					gaim_blist_add_group(g,
							gaim_blist_get_last_sibling(gaimbuddylist->root));
				}
			} else if (*c == 'b') { /*&& !gaim_find_buddy(user, c + 2)) {*/
				char nm[80], sw[388], *a, *utf8 = NULL;

				if ((a = strchr(c + 2, ':')) != NULL) {
					*a++ = '\0';		/* nul the : */
				}

				g_strlcpy(nm, c + 2, sizeof(nm));
				if (a) {
					utf8 = gaim_try_conv_to_utf8(a);
					if (utf8 == NULL) {
						gaim_debug(GAIM_DEBUG_ERROR, "toc blist",
								   "Failed to convert alias for "
								   "'%s' to UTF-8\n", nm);
					}
				}
				if (utf8 == NULL) {
					sw[0] = '\0';
				} else {
					/* This can leave a partial sequence at the end,
					 * but who cares? */
					g_strlcpy(sw, utf8, sizeof(sw));
					g_free(utf8);
				}

				if (!gaim_find_buddy(account, nm)) {
					GaimBuddy *b = gaim_buddy_new(account, nm, sw);
					GaimGroup *g = gaim_find_group(current);
					gaim_blist_add_buddy(b, NULL, g,
							gaim_blist_get_last_child((GaimBlistNode*)g));
					bud = g_list_append(bud, g_strdup(nm));
				}
			} else if (*c == 'p') {
				gaim_privacy_permit_add(account, c + 2, TRUE);
			} else if (*c == 'd') {
				gaim_privacy_deny_add(account, c + 2, TRUE);
			} else if (!strncmp("toc", c, 3)) {
				sscanf(c + strlen(c) - 1, "%d", &account->perm_deny);
				gaim_debug(GAIM_DEBUG_MISC, "toc blist",
						   "permdeny: %d\n", account->perm_deny);
				if (account->perm_deny == 0)
					account->perm_deny = 1;
			} else if (*c == 'm') {
				sscanf(c + 2, "%d", &account->perm_deny);
				gaim_debug(GAIM_DEBUG_MISC, "toc blist",
						   "permdeny: %d\n", account->perm_deny);
				if (account->perm_deny == 0)
					account->perm_deny = 1;
			}
		} while ((c = strtok(NULL, "\n")));

		if(account->gc) {
			if(bud) {
				GList *node = bud;
				serv_add_buddies(account->gc, bud);
				while(node) {
					g_free(node->data);
					node = node->next;
				}
			}
			serv_set_permit_deny(account->gc);
		}
		g_list_free(bud);
	}
}

#if 0
/* translate an AIM 3 buddylist (*.lst) to a Gaim buddylist */
static GString *translate_lst(FILE *src_fp)
{
	char line[BUF_LEN], *line2;
	char *name;
	int i;

	GString *dest = g_string_new("m 1\n");

	while (fgets(line, BUF_LEN, src_fp)) {
		line2 = g_strchug(line);
		if (strstr(line2, "group") == line2) {
			name = strpbrk(line2, " \t\n\r\f") + 1;
			dest = g_string_append(dest, "g ");
			for (i = 0; i < strcspn(name, "\n\r"); i++)
				if (name[i] != '\"')
					dest = g_string_append_c(dest, name[i]);
			dest = g_string_append_c(dest, '\n');
		}
		if (strstr(line2, "buddy") == line2) {
			name = strpbrk(line2, " \t\n\r\f") + 1;
			dest = g_string_append(dest, "b ");
			for (i = 0; i < strcspn(name, "\n\r"); i++)
				if (name[i] != '\"')
					dest = g_string_append_c(dest, name[i]);
			dest = g_string_append_c(dest, '\n');
		}
	}

	return dest;
}


/* translate an AIM 4 buddylist (*.blt) to Gaim format */
static GString *translate_blt(FILE *src_fp)
{
	int i;
	char line[BUF_LEN];
	char *buddy;

	GString *dest = g_string_new("m 1\n");

	while (strstr(fgets(line, BUF_LEN, src_fp), "Buddy") == NULL);
	while (strstr(fgets(line, BUF_LEN, src_fp), "list") == NULL);

	while (1) {
		fgets(line, BUF_LEN, src_fp); g_strchomp(line);
		if (strchr(line, '}') != NULL)
			break;

		if (strchr(line, '{') != NULL) {
			/* Syntax starting with "<group> {" */

			dest = g_string_append(dest, "g ");
			buddy = g_strchug(strtok(line, "{"));
			for (i = 0; i < strlen(buddy); i++)
				if (buddy[i] != '\"')
					dest = g_string_append_c(dest, buddy[i]);
			dest = g_string_append_c(dest, '\n');
			while (strchr(fgets(line, BUF_LEN, src_fp), '}') == NULL) {
				gboolean pounce = FALSE;
				char *e;
				g_strchomp(line);
				buddy = g_strchug(line);
				gaim_debug(GAIM_DEBUG_MISC, "AIM 4 blt import",
						   "buddy: \"%s\"\n", buddy);
				dest = g_string_append(dest, "b ");
				if (strchr(buddy, '{') != NULL) {
					/* buddy pounce, etc */
					char *pos = strchr(buddy, '{') - 1;
					*pos = 0;
					pounce = TRUE;
				}
				if ((e = strchr(buddy, '\"')) != NULL) {
					*e = '\0';
					buddy++;
				}
				dest = g_string_append(dest, buddy);
				dest = g_string_append_c(dest, '\n');
				if (pounce)
					do
						fgets(line, BUF_LEN, src_fp);
					while (!strchr(line, '}'));
			}
		} else {

			/* Syntax "group buddy buddy ..." */
			buddy = g_strchug(strtok(line, " \n"));
			dest = g_string_append(dest, "g ");
			if (strchr(buddy, '\"') != NULL) {
				dest = g_string_append(dest, &buddy[1]);
				dest = g_string_append_c(dest, ' ');
				buddy = g_strchug(strtok(NULL, " \n"));
				while (strchr(buddy, '\"') == NULL) {
					dest = g_string_append(dest, buddy);
					dest = g_string_append_c(dest, ' ');
					buddy = g_strchug(strtok(NULL, " \n"));
				}
				buddy[strlen(buddy) - 1] = '\0';
				dest = g_string_append(dest, buddy);
			} else {
				dest = g_string_append(dest, buddy);
			}
			dest = g_string_append_c(dest, '\n');
			while ((buddy = g_strchug(strtok(NULL, " \n"))) != NULL) {
				dest = g_string_append(dest, "b ");
				if (strchr(buddy, '\"') != NULL) {
					dest = g_string_append(dest, &buddy[1]);
					dest = g_string_append_c(dest, ' ');
					buddy = g_strchug(strtok(NULL, " \n"));
					while (strchr(buddy, '\"') == NULL) {
						dest = g_string_append(dest, buddy);
						dest = g_string_append_c(dest, ' ');
						buddy = g_strchug(strtok(NULL, " \n"));
					}
					buddy[strlen(buddy) - 1] = '\0';
					dest = g_string_append(dest, buddy);
				} else {
					dest = g_string_append(dest, buddy);
				}
				dest = g_string_append_c(dest, '\n');
			}
		}
	}

	return dest;
}

static GString *translate_gnomeicu(FILE *src_fp)
{
	char line[BUF_LEN];
	GString *dest = g_string_new("m 1\ng Buddies\n");

	while (strstr(fgets(line, BUF_LEN, src_fp), "NewContacts") == NULL);

	while (fgets(line, BUF_LEN, src_fp)) {
		char *eq;
		g_strchomp(line);
		if (line[0] == '\n' || line[0] == '[')
			break;
		eq = strchr(line, '=');
		if (!eq)
			break;
		*eq = ':';
		eq = strchr(eq, ',');
		if (eq)
			*eq = '\0';
		dest = g_string_append(dest, "b ");
		dest = g_string_append(dest, line);
		dest = g_string_append_c(dest, '\n');
	}

	return dest;
}
#endif

static gchar *get_screenname_filename(const char *name)
{
	gchar **split;
	gchar *good;
	gchar *ret;

	split = g_strsplit(name, G_DIR_SEPARATOR_S, -1);
	good = g_strjoinv(NULL, split);
	g_strfreev(split);

	ret = g_utf8_strup(good, -1);

	g_free(good);

	return ret;
}

static gboolean gaim_blist_read(const char *filename);


static void do_import(GaimAccount *account, const char *filename)
{
	GString *buf = NULL;
	char first[64];
	char path[PATHSIZE];
	int len;
	FILE *f;
	struct stat st;

	if (filename) {
		g_snprintf(path, sizeof(path), "%s", filename);
	} else {
		char *g_screenname = get_screenname_filename(account->username);
		const char *username;
		char *file = gaim_user_dir();
		GaimProtocol prpl_num;
		int protocol;

		prpl_num = gaim_account_get_protocol(account);

		protocol = prpl_num;

		/* TODO Somehow move this checking into prpls */
		if (prpl_num == GAIM_PROTO_OSCAR) {
			if ((username = gaim_account_get_username(account)) != NULL) {
				protocol = (isalpha(*username)
							? GAIM_PROTO_TOC : GAIM_PROTO_ICQ);
			}
		}

		if (file != (char *)NULL) {
			snprintf(path, PATHSIZE, "%s" G_DIR_SEPARATOR_S "%s.%d.blist", file, g_screenname, protocol);
			g_free(g_screenname);
		} else {
			g_free(g_screenname);
			return;
		}
	}

	if (stat(path, &st)) {
		gaim_debug(GAIM_DEBUG_ERROR, "blist import", "Unable to stat %s.\n",
				   path);
		return;
	}

	if (!(f = fopen(path, "r"))) {
		gaim_debug(GAIM_DEBUG_ERROR, "blist import", "Unable to open %s.\n",
				   path);
		return;
	}

	fgets(first, 64, f);

	if ((first[0] == '\n') || (first[0] == '\r' && first[1] == '\n'))
		fgets(first, 64, f);

#if 0
	if (!g_strncasecmp(first, "<xml", strlen("<xml"))) {
		/* new gaim XML buddy list */
		gaim_blist_read(path);
		
		/* We really don't need to bother doing stuf like translating AIM 3 buddy lists anymore */
		
	} else if (!g_strncasecmp(first, "Config {", strlen("Config {"))) {
		/* AIM 4 buddy list */
		gaim_debug(GAIM_DEBUG_MISC, "blist import", "aim 4\n");
		rewind(f);
		buf = translate_blt(f);
	} else if (strstr(first, "group") != NULL) {
		/* AIM 3 buddy list */
		gaim_debug(GAIM_DEBUG_MISC, "blist import", "aim 3\n");
		rewind(f);
		buf = translate_lst(f);
	} else if (!g_strncasecmp(first, "[User]", strlen("[User]"))) {
		/* GnomeICU (hopefully) */
		gaim_debug(GAIM_DEBUG_MISC, "blist import", "gnomeicu\n");
		rewind(f);
		buf = translate_gnomeicu(f);

	} else 
#endif
		if (first[0] == 'm') {
			/* Gaim buddy list - no translation */
			char buf2[BUF_LONG * 2];
			buf = g_string_new("");
			rewind(f);
			while (1) {
				len = fread(buf2, 1, BUF_LONG * 2 - 1, f);
				if (len <= 0)
					break;
			buf2[len] = '\0';
			buf = g_string_append(buf, buf2);
			if (len != BUF_LONG * 2 - 1)
				break;
			}
		}
	
	fclose(f);

	if (buf) {
		buf = g_string_prepend(buf, "toc_set_config {");
		buf = g_string_append(buf, "}\n");
		parse_toc_buddy_list(account, buf->str);
		g_string_free(buf, TRUE);
	}
}

gboolean gaim_group_on_account(GaimGroup *g, GaimAccount *account) {
	GaimBlistNode *cnode, *bnode;
	for(cnode = ((GaimBlistNode *)g)->child; cnode; cnode = cnode->next) {
		if(GAIM_BLIST_NODE_IS_CONTACT(cnode)) {
			for(bnode = cnode->child; bnode; bnode = bnode->next) {
				if(GAIM_BLIST_NODE_IS_BUDDY(bnode)) {
					GaimBuddy *buddy = (GaimBuddy *)bnode;
					if((!account && gaim_account_is_connected(buddy->account))
							|| buddy->account == account)
						return TRUE;
				}
			}
		} else if(GAIM_BLIST_NODE_IS_CHAT(cnode)) {
			GaimBlistChat *chat = (GaimBlistChat *)cnode;
			if((!account && gaim_account_is_connected(chat->account))
					|| chat->account == account)
				return TRUE;
		}
	}
	return FALSE;
}

static gboolean blist_safe_to_write = FALSE;

GaimGroup *blist_parser_group = NULL;
GaimContact *blist_parser_contact = NULL;
static char *blist_parser_account_name = NULL;
static int blist_parser_account_protocol = 0;
static char *blist_parser_chat_alias = NULL;
static char *blist_parser_component_name = NULL;
static char *blist_parser_component_value = NULL;
static char *blist_parser_buddy_name = NULL;
static char *blist_parser_buddy_alias = NULL;
static char *blist_parser_setting_name = NULL;
static char *blist_parser_setting_value = NULL;
static GHashTable *blist_parser_buddy_settings = NULL;
static GHashTable *blist_parser_chat_settings = NULL;
static GHashTable *blist_parser_group_settings = NULL;
static GHashTable *blist_parser_chat_components = NULL;
static int blist_parser_privacy_mode = 0;
static GList *tag_stack = NULL;
enum {
	BLIST_TAG_GAIM,
	BLIST_TAG_BLIST,
	BLIST_TAG_GROUP,
	BLIST_TAG_CHAT,
	BLIST_TAG_COMPONENT,
	BLIST_TAG_CONTACT,
	BLIST_TAG_BUDDY,
	BLIST_TAG_NAME,
	BLIST_TAG_ALIAS,
	BLIST_TAG_SETTING,
	BLIST_TAG_PRIVACY,
	BLIST_TAG_ACCOUNT,
	BLIST_TAG_PERMIT,
	BLIST_TAG_BLOCK,
	BLIST_TAG_IGNORE
};
static gboolean blist_parser_error_occurred = FALSE;

static void blist_start_element_handler (GMarkupParseContext *context,
		const gchar *element_name,
		const gchar **attribute_names,
		const gchar **attribute_values,
		gpointer user_data,
		GError **error) {
	int i;

	if(!strcmp(element_name, "gaim")) {
		tag_stack = g_list_prepend(tag_stack, GINT_TO_POINTER(BLIST_TAG_GAIM));
	} else if(!strcmp(element_name, "blist")) {
		tag_stack = g_list_prepend(tag_stack, GINT_TO_POINTER(BLIST_TAG_BLIST));
	} else if(!strcmp(element_name, "group")) {
		const char *name = NULL;
		tag_stack = g_list_prepend(tag_stack, GINT_TO_POINTER(BLIST_TAG_GROUP));
		for(i=0; attribute_names[i]; i++) {
			if(!strcmp(attribute_names[i], "name")) {
				name = attribute_values[i];
			}
		}
		if(name) {
			blist_parser_group = gaim_group_new(name);
			gaim_blist_add_group(blist_parser_group,
					gaim_blist_get_last_sibling(gaimbuddylist->root));
		}
	} else if(!strcmp(element_name, "contact")) {
		char *alias = NULL;
		tag_stack = g_list_prepend(tag_stack,
				GINT_TO_POINTER(BLIST_TAG_CONTACT));

		for(i=0; attribute_names[i]; i++) {
			if(!strcmp(attribute_names[i], "alias")) {
				g_free(alias);
				alias = g_strdup(attribute_values[i]);
			}
		}

		blist_parser_contact = gaim_contact_new();
		gaim_blist_add_contact(blist_parser_contact, blist_parser_group,
				gaim_blist_get_last_sibling(((GaimBlistNode*)blist_parser_group)->child));

		if(alias) {
			gaim_contact_set_alias(blist_parser_contact, alias);
			g_free(alias);
		}
	} else if(!strcmp(element_name, "chat")) {
		tag_stack = g_list_prepend(tag_stack, GINT_TO_POINTER(BLIST_TAG_CHAT));
		for(i=0; attribute_names[i]; i++) {
			if(!strcmp(attribute_names[i], "account")) {
				g_free(blist_parser_account_name);
				blist_parser_account_name = g_strdup(attribute_values[i]);
			} else if(!strcmp(attribute_names[i], "protocol")) {
				blist_parser_account_protocol = atoi(attribute_values[i]);
			}
		}
	} else if(!strcmp(element_name, "buddy")) {
		tag_stack = g_list_prepend(tag_stack, GINT_TO_POINTER(BLIST_TAG_BUDDY));
		for(i=0; attribute_names[i]; i++) {
			if(!strcmp(attribute_names[i], "account")) {
				g_free(blist_parser_account_name);
				blist_parser_account_name = g_strdup(attribute_values[i]);
			} else if(!strcmp(attribute_names[i], "protocol")) {
				blist_parser_account_protocol = atoi(attribute_values[i]);
			}
		}
	} else if(!strcmp(element_name, "name")) {
		tag_stack = g_list_prepend(tag_stack, GINT_TO_POINTER(BLIST_TAG_NAME));
	} else if(!strcmp(element_name, "alias")) {
		tag_stack = g_list_prepend(tag_stack, GINT_TO_POINTER(BLIST_TAG_ALIAS));
	} else if(!strcmp(element_name, "setting")) {
		tag_stack = g_list_prepend(tag_stack, GINT_TO_POINTER(BLIST_TAG_SETTING));
		for(i=0; attribute_names[i]; i++) {
			if(!strcmp(attribute_names[i], "name")) {
				g_free(blist_parser_setting_name);
				blist_parser_setting_name = g_strdup(attribute_values[i]);
			}
		}
	} else if(!strcmp(element_name, "component")) {
		tag_stack = g_list_prepend(tag_stack, GINT_TO_POINTER(BLIST_TAG_COMPONENT));
		for(i=0; attribute_names[i]; i++) {
			if(!strcmp(attribute_names[i], "name")) {
				g_free(blist_parser_component_name);
				blist_parser_component_name = g_strdup(attribute_values[i]);
			}
		}

	} else if(!strcmp(element_name, "privacy")) {
		tag_stack = g_list_prepend(tag_stack, GINT_TO_POINTER(BLIST_TAG_PRIVACY));
	} else if(!strcmp(element_name, "account")) {
		tag_stack = g_list_prepend(tag_stack, GINT_TO_POINTER(BLIST_TAG_ACCOUNT));
		for(i=0; attribute_names[i]; i++) {
			if(!strcmp(attribute_names[i], "protocol"))
				blist_parser_account_protocol = atoi(attribute_values[i]);
			else if(!strcmp(attribute_names[i], "mode"))
				blist_parser_privacy_mode = atoi(attribute_values[i]);
			else if(!strcmp(attribute_names[i], "name")) {
				g_free(blist_parser_account_name);
				blist_parser_account_name = g_strdup(attribute_values[i]);
			}
		}
	} else if(!strcmp(element_name, "permit")) {
		tag_stack = g_list_prepend(tag_stack, GINT_TO_POINTER(BLIST_TAG_PERMIT));
	} else if(!strcmp(element_name, "block")) {
		tag_stack = g_list_prepend(tag_stack, GINT_TO_POINTER(BLIST_TAG_BLOCK));
	} else if(!strcmp(element_name, "ignore")) {
		tag_stack = g_list_prepend(tag_stack, GINT_TO_POINTER(BLIST_TAG_IGNORE));
	}
}

static void blist_end_element_handler(GMarkupParseContext *context,
		const gchar *element_name, gpointer user_data, GError **error) {
	if(!strcmp(element_name, "gaim")) {
		tag_stack = g_list_delete_link(tag_stack, tag_stack);
	} else if(!strcmp(element_name, "blist")) {
		tag_stack = g_list_delete_link(tag_stack, tag_stack);
	} else if(!strcmp(element_name, "group")) {
		if(blist_parser_group_settings) {
			g_hash_table_destroy(blist_parser_group->settings);
			blist_parser_group->settings = blist_parser_group_settings;
		}
		tag_stack = g_list_delete_link(tag_stack, tag_stack);
		blist_parser_group_settings = NULL;
		blist_parser_group = NULL;
	} else if(!strcmp(element_name, "chat")) {
		GaimAccount *account = gaim_accounts_find(blist_parser_account_name,
				blist_parser_account_protocol);
		if(account) {
			GaimBlistChat *chat = gaim_blist_chat_new(account,
					blist_parser_chat_alias, blist_parser_chat_components);
			gaim_blist_add_chat(chat,blist_parser_group,
					gaim_blist_get_last_child((GaimBlistNode*)blist_parser_group));
			if(blist_parser_chat_settings) {
				g_hash_table_destroy(chat->settings);
				chat->settings = blist_parser_chat_settings;
			}
		}
		g_free(blist_parser_chat_alias);
		blist_parser_chat_alias = NULL;
		g_free(blist_parser_account_name);
		blist_parser_account_name = NULL;
		blist_parser_chat_components = NULL;
		blist_parser_chat_settings = NULL;
		tag_stack = g_list_delete_link(tag_stack, tag_stack);
	} else if(!strcmp(element_name, "contact")) {
		if(blist_parser_contact && !blist_parser_contact->node.child)
			gaim_blist_remove_contact(blist_parser_contact);
		blist_parser_contact = NULL;
		tag_stack = g_list_delete_link(tag_stack, tag_stack);
	} else if(!strcmp(element_name, "buddy")) {
		GaimAccount *account = gaim_accounts_find(blist_parser_account_name,
				blist_parser_account_protocol);
		if(account && !gaim_find_buddy_in_group(account,
					blist_parser_buddy_name, blist_parser_group)) {
			GaimBuddy *b = gaim_buddy_new(account, blist_parser_buddy_name,
					blist_parser_buddy_alias);
			gaim_blist_add_buddy(b,blist_parser_contact, blist_parser_group,
					gaim_blist_get_last_child((GaimBlistNode*)blist_parser_contact));
			if(blist_parser_buddy_settings) {
				g_hash_table_destroy(b->settings);
				b->settings = blist_parser_buddy_settings;
			}
		}
		g_free(blist_parser_buddy_name);
		blist_parser_buddy_name = NULL;
		g_free(blist_parser_buddy_alias);
		blist_parser_buddy_alias = NULL;
		g_free(blist_parser_account_name);
		blist_parser_account_name = NULL;
		blist_parser_buddy_settings = NULL;
		tag_stack = g_list_delete_link(tag_stack, tag_stack);
	} else if(!strcmp(element_name, "name")) {
		tag_stack = g_list_delete_link(tag_stack, tag_stack);
	} else if(!strcmp(element_name, "alias")) {
		tag_stack = g_list_delete_link(tag_stack, tag_stack);
	} else if(!strcmp(element_name, "component")) {
		if(!blist_parser_chat_components)
			blist_parser_chat_components = g_hash_table_new_full(g_str_hash,
					g_str_equal, g_free, g_free);
		if(blist_parser_component_name && blist_parser_component_value) {
			g_hash_table_replace(blist_parser_chat_components,
					g_strdup(blist_parser_component_name),
					g_strdup(blist_parser_component_value));
		}
		g_free(blist_parser_component_name);
		g_free(blist_parser_component_value);
		blist_parser_component_name = NULL;
		blist_parser_component_value = NULL;
		tag_stack = g_list_delete_link(tag_stack, tag_stack);
	} else if(!strcmp(element_name, "setting")) {
		if(GPOINTER_TO_INT(tag_stack->next->data) == BLIST_TAG_BUDDY) {
			if(!blist_parser_buddy_settings)
				blist_parser_buddy_settings = g_hash_table_new_full(g_str_hash,
						g_str_equal, g_free, g_free);
			if(blist_parser_setting_name && blist_parser_setting_value) {
				g_hash_table_replace(blist_parser_buddy_settings,
						g_strdup(blist_parser_setting_name),
						g_strdup(blist_parser_setting_value));
			}
		} else if(GPOINTER_TO_INT(tag_stack->next->data) == BLIST_TAG_CHAT) {
			if(!blist_parser_chat_settings)
				blist_parser_chat_settings = g_hash_table_new_full(g_str_hash,
						g_str_equal, g_free, g_free);
			if(blist_parser_setting_name && blist_parser_setting_value) {
				g_hash_table_replace(blist_parser_chat_settings,
						g_strdup(blist_parser_setting_name),
						g_strdup(blist_parser_setting_value));
			}
		} else if(GPOINTER_TO_INT(tag_stack->next->data) == BLIST_TAG_GROUP) {
			if(!blist_parser_group_settings)
				blist_parser_group_settings = g_hash_table_new_full(g_str_hash,
						g_str_equal, g_free, g_free);
			if(blist_parser_setting_name && blist_parser_setting_value) {
				g_hash_table_replace(blist_parser_group_settings,
						g_strdup(blist_parser_setting_name),
						g_strdup(blist_parser_setting_value));
			}
		}
		g_free(blist_parser_setting_name);
		g_free(blist_parser_setting_value);
		blist_parser_setting_name = NULL;
		blist_parser_setting_value = NULL;
		tag_stack = g_list_delete_link(tag_stack, tag_stack);
	} else if(!strcmp(element_name, "privacy")) {
		tag_stack = g_list_delete_link(tag_stack, tag_stack);
	} else if(!strcmp(element_name, "account")) {
		GaimAccount *account = gaim_accounts_find(blist_parser_account_name,
				blist_parser_account_protocol);
		if(account) {
			account->perm_deny = blist_parser_privacy_mode;
		}
		g_free(blist_parser_account_name);
		blist_parser_account_name = NULL;
		tag_stack = g_list_delete_link(tag_stack, tag_stack);
	} else if(!strcmp(element_name, "permit")) {
		GaimAccount *account = gaim_accounts_find(blist_parser_account_name,
				blist_parser_account_protocol);
		if(account) {
			gaim_privacy_permit_add(account, blist_parser_buddy_name, TRUE);
		}
		g_free(blist_parser_buddy_name);
		blist_parser_buddy_name = NULL;
		tag_stack = g_list_delete_link(tag_stack, tag_stack);
	} else if(!strcmp(element_name, "block")) {
		GaimAccount *account = gaim_accounts_find(blist_parser_account_name,
				blist_parser_account_protocol);
		if(account) {
			gaim_privacy_deny_add(account, blist_parser_buddy_name, TRUE);
		}
		g_free(blist_parser_buddy_name);
		blist_parser_buddy_name = NULL;
		tag_stack = g_list_delete_link(tag_stack, tag_stack);
	} else if(!strcmp(element_name, "ignore")) {
		/* we'll apparently do something with this later */
		tag_stack = g_list_delete_link(tag_stack, tag_stack);
	}
}

static void blist_text_handler(GMarkupParseContext *context, const gchar *text,
		gsize text_len, gpointer user_data, GError **error) {
	switch(GPOINTER_TO_INT(tag_stack->data)) {
		case BLIST_TAG_NAME:
			blist_parser_buddy_name = g_strndup(text, text_len);
			break;
		case BLIST_TAG_ALIAS:
			if(tag_stack->next &&
					GPOINTER_TO_INT(tag_stack->next->data) == BLIST_TAG_BUDDY)
				blist_parser_buddy_alias = g_strndup(text, text_len);
			else if(tag_stack->next &&
					GPOINTER_TO_INT(tag_stack->next->data) == BLIST_TAG_CHAT)
				blist_parser_chat_alias = g_strndup(text, text_len);
			break;
		case BLIST_TAG_PERMIT:
		case BLIST_TAG_BLOCK:
		case BLIST_TAG_IGNORE:
			blist_parser_buddy_name = g_strndup(text, text_len);
			break;
		case BLIST_TAG_COMPONENT:
			blist_parser_component_value = g_strndup(text, text_len);
			break;
		case BLIST_TAG_SETTING:
			blist_parser_setting_value = g_strndup(text, text_len);
			break;
		default:
			break;
	}
}

static void blist_error_handler(GMarkupParseContext *context, GError *error,
		gpointer user_data) {
	blist_parser_error_occurred = TRUE;
	gaim_debug(GAIM_DEBUG_ERROR, "blist import",
			   "Error parsing blist.xml: %s\n", error->message);
}

static GMarkupParser blist_parser = {
	blist_start_element_handler,
	blist_end_element_handler,
	blist_text_handler,
	NULL,
	blist_error_handler
};

static gboolean gaim_blist_read(const char *filename) {
	gchar *contents = NULL;
	gsize length;
	GMarkupParseContext *context;
	GError *error = NULL;

	gaim_debug(GAIM_DEBUG_INFO, "blist import",
			   "Reading %s\n", filename);
	if(!g_file_get_contents(filename, &contents, &length, &error)) {
		gaim_debug(GAIM_DEBUG_ERROR, "blist import",
				   "Error reading blist: %s\n", error->message);
		g_error_free(error);
		return FALSE;
	}

	context = g_markup_parse_context_new(&blist_parser, 0, NULL, NULL);

	if(!g_markup_parse_context_parse(context, contents, length, NULL)) {
		g_markup_parse_context_free(context);
		g_free(contents);
		return FALSE;
	}

	if(!g_markup_parse_context_end_parse(context, NULL)) {
		gaim_debug(GAIM_DEBUG_ERROR, "blist import",
				   "Error parsing %s\n", filename);
		g_markup_parse_context_free(context);
		g_free(contents);
		return FALSE;
	}

	g_markup_parse_context_free(context);
	g_free(contents);

	if(blist_parser_error_occurred)
		return FALSE;

	gaim_debug(GAIM_DEBUG_INFO, "blist import", "Finished reading %s\n",
			   filename);

	return TRUE;
}

void gaim_blist_load() {
	GList *accts;
	char *user_dir = gaim_user_dir();
	char *filename;
	char *msg;

	blist_safe_to_write = TRUE;

	if(!user_dir)
		return;

	filename = g_build_filename(user_dir, "blist.xml", NULL);

	if(g_file_test(filename, G_FILE_TEST_EXISTS)) {
		if(!gaim_blist_read(filename)) {
			msg = g_strdup_printf(_("An error was encountered parsing your "
						"buddy list.  It has not been loaded."));
			gaim_notify_error(NULL, NULL, _("Buddy List Error"), msg);
			g_free(msg);
		}
	} else if(g_list_length(gaim_accounts_get_all())) {
		/* read in the old lists, then save to the new format */
		for(accts = gaim_accounts_get_all(); accts; accts = accts->next) {
			do_import(accts->data, NULL);
		}
		gaim_blist_save();
	}

	g_free(filename);
}

static void blist_print_group_settings(gpointer key, gpointer data,
		gpointer user_data) {
	char *key_val;
	char *data_val;
	FILE *file = user_data;

	if(!key || !data)
		return;

	key_val = g_markup_escape_text(key, -1);
	data_val = g_markup_escape_text(data, -1);

	fprintf(file, "\t\t\t<setting name=\"%s\">%s</setting>\n", key_val,
			data_val);
	g_free(key_val);
	g_free(data_val);
}

static void blist_print_buddy_settings(gpointer key, gpointer data,
		gpointer user_data) {
	char *key_val;
	char *data_val;
	FILE *file = user_data;

	if(!key || !data)
		return;

	key_val = g_markup_escape_text(key, -1);
	data_val = g_markup_escape_text(data, -1);

	fprintf(file, "\t\t\t\t\t<setting name=\"%s\">%s</setting>\n", key_val,
			data_val);
	g_free(key_val);
	g_free(data_val);
}

static void blist_print_cnode_settings(gpointer key, gpointer data,
		gpointer user_data) {
	char *key_val;
	char *data_val;
	FILE *file = user_data;

	if(!key || !data)
		return;

	key_val = g_markup_escape_text(key, -1);
	data_val = g_markup_escape_text(data, -1);

	fprintf(file, "\t\t\t\t<setting name=\"%s\">%s</setting>\n", key_val,
			data_val);
	g_free(key_val);
	g_free(data_val);
}

static void blist_print_chat_components(gpointer key, gpointer data,
		gpointer user_data) {
	char *key_val;
	char *data_val;
	FILE *file = user_data;

	if(!key || !data)
		return;

	key_val = g_markup_escape_text(key, -1);
	data_val = g_markup_escape_text(data, -1);

	fprintf(file, "\t\t\t\t<component name=\"%s\">%s</component>\n", key_val,
			data_val);
	g_free(key_val);
	g_free(data_val);
}

static void print_buddy(FILE *file, GaimBuddy *buddy) {
	char *bud_name = g_markup_escape_text(buddy->name, -1);
	char *bud_alias = NULL;
	char *acct_name = g_markup_escape_text(buddy->account->username, -1);
	if(buddy->alias)
		bud_alias= g_markup_escape_text(buddy->alias, -1);
	fprintf(file, "\t\t\t\t<buddy protocol=\"%d\" "
			"account=\"%s\">\n",
			gaim_account_get_protocol(buddy->account),
			acct_name);
	fprintf(file, "\t\t\t\t\t<name>%s</name>\n", bud_name);
	if(bud_alias) {
		fprintf(file, "\t\t\t\t\t<alias>%s</alias>\n", bud_alias);
	}
	g_hash_table_foreach(buddy->settings, blist_print_buddy_settings, file);
	fprintf(file, "\t\t\t\t</buddy>\n");
	g_free(bud_name);
	g_free(bud_alias);
	g_free(acct_name);
}

static void gaim_blist_write(FILE *file, GaimAccount *exp_acct) {
	GList *accounts;
	GSList *buds;
	GaimBlistNode *gnode, *cnode, *bnode;
	fprintf(file, "<?xml version='1.0' encoding='UTF-8' ?>\n");
	fprintf(file, "<gaim version=\"1\">\n");
	fprintf(file, "\t<blist>\n");

	for(gnode = gaimbuddylist->root; gnode; gnode = gnode->next) {
		GaimGroup *group;

		if(!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;

		group = (GaimGroup *)gnode;
		if(!exp_acct || gaim_group_on_account(group, exp_acct)) {
			char *group_name = g_markup_escape_text(group->name, -1);
			fprintf(file, "\t\t<group name=\"%s\">\n", group_name);
			g_hash_table_foreach(group->settings, blist_print_group_settings, file);
			for(cnode = gnode->child; cnode; cnode = cnode->next) {
				if(GAIM_BLIST_NODE_IS_CONTACT(cnode)) {
					GaimContact *contact = (GaimContact*)cnode;
					fprintf(file, "\t\t\t<contact");
					if(contact->alias) {
						char *alias = g_markup_escape_text(contact->alias, -1);
						fprintf(file, " alias=\"%s\"", alias);
						g_free(alias);
					}
					fprintf(file, ">\n");

					for(bnode = cnode->child; bnode; bnode = bnode->next) {
						if(GAIM_BLIST_NODE_IS_BUDDY(bnode)) {
							GaimBuddy *buddy = (GaimBuddy *)bnode;
							if(!exp_acct || buddy->account == exp_acct) {
								print_buddy(file, buddy);
							}
						}
					}

					fprintf(file, "\t\t\t</contact>\n");
				} else if(GAIM_BLIST_NODE_IS_CHAT(cnode)) {
					GaimBlistChat *chat = (GaimBlistChat *)cnode;
					if(!exp_acct || chat->account == exp_acct) {
						char *acct_name = g_markup_escape_text(chat->account->username, -1);
						fprintf(file, "\t\t\t<chat protocol=\"%d\" account=\"%s\">\n",
								gaim_account_get_protocol(chat->account),
								acct_name);
						if(chat->alias) {
							char *chat_alias = g_markup_escape_text(chat->alias, -1);
							fprintf(file, "\t\t\t\t<alias>%s</alias>\n", chat_alias);
							g_free(chat_alias);
						}
						g_hash_table_foreach(chat->components,
								blist_print_chat_components, file);
						g_hash_table_foreach(chat->settings,
								blist_print_cnode_settings, file);
						fprintf(file, "\t\t\t</chat>\n");
						g_free(acct_name);
					}
				}
			}
			fprintf(file, "\t\t</group>\n");
			g_free(group_name);
		}
	}

	fprintf(file, "\t</blist>\n");
	fprintf(file, "\t<privacy>\n");

	for(accounts = gaim_accounts_get_all();
		accounts != NULL;
		accounts = accounts->next) {

		GaimAccount *account = accounts->data;
		char *acct_name = g_markup_escape_text(account->username, -1);
		if(!exp_acct || account == exp_acct) {
			fprintf(file, "\t\t<account protocol=\"%d\" name=\"%s\" "
					"mode=\"%d\">\n", gaim_account_get_protocol(account),
					acct_name, account->perm_deny);
			for(buds = account->permit; buds; buds = buds->next) {
				char *bud_name = g_markup_escape_text(buds->data, -1);
				fprintf(file, "\t\t\t<permit>%s</permit>\n", bud_name);
				g_free(bud_name);
			}
			for(buds = account->deny; buds; buds = buds->next) {
				char *bud_name = g_markup_escape_text(buds->data, -1);
				fprintf(file, "\t\t\t<block>%s</block>\n", bud_name);
				g_free(bud_name);
			}
			fprintf(file, "\t\t</account>\n");
		}
		g_free(acct_name);
	}

	fprintf(file, "\t</privacy>\n");
	fprintf(file, "</gaim>\n");
}

void gaim_blist_save() {
	FILE *file;
	char *user_dir = gaim_user_dir();
	char *filename;
	char *filename_real;

	if(!user_dir)
		return;
	if(!blist_safe_to_write) {
		gaim_debug(GAIM_DEBUG_WARNING, "blist save",
				   "AHH!! Tried to write the blist before we read it!\n");
		return;
	}

	file = fopen(user_dir, "r");
	if(!file)
		mkdir(user_dir, S_IRUSR | S_IWUSR | S_IXUSR);
	else
		fclose(file);

	filename = g_build_filename(user_dir, "blist.xml.save", NULL);

	if((file = fopen(filename, "w"))) {
		gaim_blist_write(file, NULL);
		fclose(file);
		chmod(filename, S_IRUSR | S_IWUSR);
	} else {
		gaim_debug(GAIM_DEBUG_ERROR, "blist save", "Unable to write %s\n",
				   filename);
	}

	filename_real = g_build_filename(user_dir, "blist.xml", NULL);

	if(rename(filename, filename_real) < 0)
		gaim_debug(GAIM_DEBUG_ERROR, "blist save",
				   "Error renaming %s to %s\n", filename, filename_real);


	g_free(filename);
	g_free(filename_real);
}

void gaim_group_set_setting(GaimGroup *g, const char *key,
		const char *value) {
	if(!g)
		return;
	g_hash_table_replace(g->settings, g_strdup(key), g_strdup(value));
}

char *gaim_group_get_setting(GaimGroup *g, const char *key) {
	if(!g)
		return NULL;
	return g_strdup(g_hash_table_lookup(g->settings, key));
}

void gaim_blist_chat_set_setting(GaimBlistChat *c, const char *key,
		const char *value)
{
	if(!c)
		return;
	g_hash_table_replace(c->settings, g_strdup(key), g_strdup(value));
}

char *gaim_blist_chat_get_setting(GaimBlistChat *c, const char *key)
{
	if(!c)
		return NULL;
	return g_strdup(g_hash_table_lookup(c->settings, key));
}

void gaim_buddy_set_setting(GaimBuddy *b, const char *key,
		const char *value) {
	if(!b)
		return;
	g_hash_table_replace(b->settings, g_strdup(key), g_strdup(value));
}

char *gaim_buddy_get_setting(GaimBuddy *b, const char *key) {
	if(!b)
		return NULL;
	return g_strdup(g_hash_table_lookup(b->settings, key));
}

void gaim_set_blist_ui_ops(struct gaim_blist_ui_ops *ops)
{
	blist_ui_ops = ops;
}

struct gaim_blist_ui_ops *
gaim_get_blist_ui_ops(void)
{
	return blist_ui_ops;
}

int gaim_blist_get_group_size(GaimGroup *group, gboolean offline) {
	if(!group)
		return 0;

	return offline ? group->totalsize : group->currentsize;
}

int gaim_blist_get_group_online_count(GaimGroup *group) {
	if(!group)
		return 0;

	return group->online;
}

void *
gaim_blist_get_handle(void)
{
	static int handle;

	return &handle;
}

void
gaim_blist_init(void)
{
	void *handle = gaim_blist_get_handle();

	gaim_signal_register(handle, "buddy-away",
						 gaim_marshal_VOID__POINTER, NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_BLIST_BUDDY));

	gaim_signal_register(handle, "buddy-back",
						 gaim_marshal_VOID__POINTER, NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_BLIST_BUDDY));

	gaim_signal_register(handle, "buddy-idle",
						 gaim_marshal_VOID__POINTER, NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_BLIST_BUDDY));
	gaim_signal_register(handle, "buddy-unidle",
						 gaim_marshal_VOID__POINTER, NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_BLIST_BUDDY));

	gaim_signal_register(handle, "buddy-signed-on",
						 gaim_marshal_VOID__POINTER, NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_BLIST_BUDDY));

	gaim_signal_register(handle, "buddy-signed-off",
						 gaim_marshal_VOID__POINTER, NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_BLIST_BUDDY));

	gaim_signal_register(handle, "update-idle", gaim_marshal_VOID, NULL, 0);
}

void
gaim_blist_uninit(void)
{
	gaim_signals_unregister_by_instance(gaim_blist_get_handle());
}
