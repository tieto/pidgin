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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <direct.h>
#endif
#include <ctype.h>
#include "gaim.h"
#include "prpl.h"
#include "blist.h"
#include "notify.h"
#include "prefs.h"

#ifdef _WIN32
#include "win32dep.h"
#endif

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

/*****************************************************************************
 * Public API functions                                                      *
 *****************************************************************************/

struct gaim_buddy_list *gaim_blist_new()
{
	struct gaim_buddy_list *gbl = g_new0(struct gaim_buddy_list, 1);

	gbl->ui_ops = gaim_get_blist_ui_ops();

	gbl->buddies = g_hash_table_new ((GHashFunc)_gaim_blist_hbuddy_hash, 
					 (GEqualFunc)_gaim_blist_hbuddy_equal);

	if (gbl->ui_ops != NULL && gbl->ui_ops->new_list != NULL)
		gbl->ui_ops->new_list(gbl);

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

void  gaim_blist_update_buddy_status (struct buddy *buddy, int status)
{
	struct gaim_blist_ui_ops *ops;

	if (buddy->uc == status)
		return;

	ops = gaimbuddylist->ui_ops;

	if((status & UC_UNAVAILABLE) != (buddy->uc & UC_UNAVAILABLE)) {
		if(status & UC_UNAVAILABLE)
			gaim_event_broadcast(event_buddy_away, buddy->account->gc, buddy->name);
		else
			gaim_event_broadcast(event_buddy_back, buddy->account->gc, buddy->name);
	}

	buddy->uc = status;
	if (ops)
		ops->update(gaimbuddylist, (GaimBlistNode*)buddy);
}

static gboolean presence_update_timeout_cb(struct buddy *buddy) {
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;

	if(buddy->present == GAIM_BUDDY_SIGNING_ON) {
		buddy->present = GAIM_BUDDY_ONLINE;
		gaim_event_broadcast(event_buddy_signon, buddy->account->gc, buddy->name);
	} else if(buddy->present == GAIM_BUDDY_SIGNING_OFF) {
		buddy->present = GAIM_BUDDY_OFFLINE;
	}

	buddy->timer = 0;

	if (ops)
		ops->update(gaimbuddylist, (GaimBlistNode*)buddy);

	return FALSE;
}

void gaim_blist_update_buddy_presence(struct buddy *buddy, int presence) {
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	gboolean do_timer = FALSE;

	if (!GAIM_BUDDY_IS_ONLINE(buddy) && presence) {
		buddy->present = GAIM_BUDDY_SIGNING_ON;
		gaim_event_broadcast(event_buddy_signon, buddy->account->gc, buddy->name);
		do_timer = TRUE;
		((struct group *)((GaimBlistNode *)buddy)->parent)->online++;
	} else if(GAIM_BUDDY_IS_ONLINE(buddy) && !presence) {
		buddy->present = GAIM_BUDDY_SIGNING_OFF;
		gaim_event_broadcast(event_buddy_signoff, buddy->account->gc, buddy->name);
		do_timer = TRUE;
		((struct group *)((GaimBlistNode *)buddy)->parent)->online--;
	}

	if(do_timer) {
		if(buddy->timer > 0)
			g_source_remove(buddy->timer);
		buddy->timer = g_timeout_add(10000, (GSourceFunc)presence_update_timeout_cb, buddy);
	}

	if (ops)
		ops->update(gaimbuddylist, (GaimBlistNode*)buddy);
}


void  gaim_blist_update_buddy_idle (struct buddy *buddy, int idle)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	buddy->idle = idle;
	if (ops)
		ops->update(gaimbuddylist, (GaimBlistNode*)buddy);
}
void  gaim_blist_update_buddy_evil (struct buddy *buddy, int warning)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	buddy->evil = warning;
	if (ops)
		ops->update(gaimbuddylist,(GaimBlistNode*)buddy);
}
void gaim_blist_update_buddy_icon(struct buddy *buddy) {
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	if(ops)
		ops->update(gaimbuddylist, (GaimBlistNode*)buddy);
}
void  gaim_blist_rename_buddy (struct buddy *buddy, const char *name)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	g_free(buddy->name);
	buddy->name = g_strdup(name);
	if (ops)
		ops->update(gaimbuddylist, (GaimBlistNode*)buddy);
}

void gaim_blist_alias_chat(struct chat *chat, const char *alias)
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

void  gaim_blist_alias_buddy (struct buddy *buddy, const char *alias)
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

void gaim_blist_rename_group(struct group *group, const char *name)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	struct group *dest_group;
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
			if(GAIM_BLIST_NODE_IS_BUDDY(child)) {
				gaim_blist_add_buddy((struct buddy *)child, dest_group, prev);
				prev = child;
			} else if(GAIM_BLIST_NODE_IS_CHAT(child)) {
				gaim_blist_add_chat((struct chat *)child, dest_group, prev);
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

struct chat *gaim_chat_new(GaimAccount *account, const char *alias, GHashTable *components)
{
	struct chat *chat;
	struct gaim_blist_ui_ops *ops;

	if(!components)
		return NULL;

	chat = g_new0(struct chat, 1);
	chat->account = account;
	if(alias && strlen(alias))
		chat->alias = g_strdup(alias);
	chat->components = components;

	((GaimBlistNode*)chat)->type = GAIM_BLIST_CHAT_NODE;

	ops = gaim_get_blist_ui_ops();

	if (ops != NULL && ops->new_node != NULL)
		ops->new_node((GaimBlistNode *)chat);

	return chat;
}

struct buddy *gaim_buddy_new(GaimAccount *account, const char *screenname, const char *alias)
{
	struct buddy *b;
	struct gaim_blist_ui_ops *ops;

	b = g_new0(struct buddy, 1);
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

void gaim_blist_add_chat(struct chat *chat, struct group *group, GaimBlistNode *node)
{
	GaimBlistNode *n = node, *cnode = (GaimBlistNode*)chat;
	struct group *g = group;
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	gboolean save = FALSE;

	if (!n) {
		if (!g) {
			g = gaim_group_new(_("Chats"));
			gaim_blist_add_group(g,
					gaim_blist_get_last_sibling(gaimbuddylist->root));
		}
	} else {
		g = (struct group*)n->parent;
	}

	/* if we're moving to overtop of ourselves, do nothing */
	if(cnode == n)
		return;

	if (cnode->parent) {
		/* This chat was already in the list and is
		 * being moved.
		 */
		((struct group *)cnode->parent)->totalsize--;
		if (chat->account->gc) {
			((struct group *)cnode->parent)->online--;
			((struct group *)cnode->parent)->currentsize--;
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
		((struct group *)n->parent)->totalsize++;
		if (chat->account->gc) {
			((struct group *)n->parent)->online++;
			((struct group *)n->parent)->currentsize++;
		}
	} else {
		if(((GaimBlistNode*)g)->child)
			((GaimBlistNode*)g)->child->prev = cnode;
		cnode->next = ((GaimBlistNode*)g)->child;
		cnode->prev = NULL;
		((GaimBlistNode*)g)->child = cnode;
		cnode->parent = (GaimBlistNode*)g;
		g->totalsize++;
		if (chat->account->gc) {
			g->online++;
			g->currentsize++;
		}
	}

	if (ops)
		ops->update(gaimbuddylist, (GaimBlistNode*)cnode);
	if (save)
		gaim_blist_save();
}

void  gaim_blist_add_buddy (struct buddy *buddy, struct group *group, GaimBlistNode *node)
{
	GaimBlistNode *n = node, *bnode = (GaimBlistNode*)buddy;
	struct group *g = group;
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	struct _gaim_hbuddy *hb;
	gboolean save = FALSE;

	if (!n) {
		if (!g) {
			g = gaim_group_new(_("Buddies"));
			gaim_blist_add_group(g,
					gaim_blist_get_last_sibling(gaimbuddylist->root));
		}
	} else {
		g = (struct group*)n->parent;
	}

	/* if we're moving to overtop of ourselves, do nothing */
	if(bnode == n)
		return;

	if (bnode->parent) {
		/* This buddy was already in the list and is
		 * being moved.
		 */
		((struct group *)bnode->parent)->totalsize--;
		if (buddy->account->gc)
			((struct group *)bnode->parent)->currentsize--;
		if (GAIM_BUDDY_IS_ONLINE(buddy))
			((struct group *)bnode->parent)->online--;

		if(bnode->next)
			bnode->next->prev = bnode->prev;
		if(bnode->prev)
			bnode->prev->next = bnode->next;
		if(bnode->parent->child == bnode)
			bnode->parent->child = bnode->next;

		ops->remove(gaimbuddylist, bnode);

		if (bnode->parent != ((GaimBlistNode*)g)) {
			serv_move_buddy(buddy, (struct group*)bnode->parent, g);
		}
		save = TRUE;
	}

	if (n) {
		if(n->next)
			n->next->prev = (GaimBlistNode*)buddy;
		((GaimBlistNode*)buddy)->next = n->next;
		((GaimBlistNode*)buddy)->prev = n;
		((GaimBlistNode*)buddy)->parent = n->parent;
		n->next = (GaimBlistNode*)buddy;
		((struct group *)n->parent)->totalsize++;
		if (buddy->account->gc)
			((struct group *)n->parent)->currentsize++;
		if (GAIM_BUDDY_IS_ONLINE(buddy))
			((struct group *)n->parent)->online++;
	} else {
		if(((GaimBlistNode*)g)->child)
			((GaimBlistNode*)g)->child->prev = (GaimBlistNode*)buddy;
		((GaimBlistNode*)buddy)->prev = NULL;
		((GaimBlistNode*)buddy)->next = ((GaimBlistNode*)g)->child;
		((GaimBlistNode*)g)->child = (GaimBlistNode*)buddy;
		((GaimBlistNode*)buddy)->parent = (GaimBlistNode*)g;
		g->totalsize++;
		if (buddy->account->gc)
			g->currentsize++;
		if (GAIM_BUDDY_IS_ONLINE(buddy))
			g->online++;
	}

	hb = g_malloc(sizeof(struct _gaim_hbuddy));
	hb->name = g_strdup(normalize(buddy->name));
	hb->account = buddy->account;
	hb->group = ((GaimBlistNode*)buddy)->parent;

	if (g_hash_table_lookup(gaimbuddylist->buddies, (gpointer)hb)) {
		/* This guy already exists */
		g_free(hb->name);
		g_free(hb);
	} else {
		g_hash_table_insert(gaimbuddylist->buddies, (gpointer)hb, (gpointer)buddy);
	}

	if (ops)
		ops->update(gaimbuddylist, (GaimBlistNode*)buddy);
	if (save)
		gaim_blist_save();
}

struct group *gaim_group_new(const char *name)
{
	struct group *g = gaim_find_group(name);

	if (!g) {
		struct gaim_blist_ui_ops *ops;
		g= g_new0(struct group, 1);
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

void  gaim_blist_add_group (struct group *group, GaimBlistNode *node)
{
	struct gaim_blist_ui_ops *ops;
	GaimBlistNode *gnode = (GaimBlistNode*)group;
	gboolean save = FALSE;

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

	if (node) {
		gnode->next = node->next;
		gnode->prev = node;
		if(node->next)
			node->next->prev = gnode;
		node->next = gnode;
	} else {
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

void  gaim_blist_remove_buddy (struct buddy *buddy)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;

	GaimBlistNode *gnode, *node = (GaimBlistNode*)buddy;
	struct group *group;
	struct _gaim_hbuddy hb, *key;
	struct buddy *val;

	gnode = node->parent;
	group = (struct group *)gnode;

	if(gnode->child == node)
		gnode->child = node->next;
	if (node->prev)
		node->prev->next = node->next;
	if (node->next)
		node->next->prev = node->prev;
	group->totalsize--;
	if (buddy->account->gc)
		group->currentsize--;
	if (GAIM_BUDDY_IS_ONLINE(buddy))
		group->online--;

	hb.name = normalize(buddy->name);
	hb.account = buddy->account;
	hb.group = ((GaimBlistNode*)buddy)->parent;
	if (g_hash_table_lookup_extended(gaimbuddylist->buddies, &hb, (gpointer *)&key, (gpointer *)&val)) {
		g_hash_table_remove(gaimbuddylist->buddies, &hb);
		g_free(key->name);
		g_free(key);
	}

	if(buddy->timer > 0)
		g_source_remove(buddy->timer);

	ops->remove(gaimbuddylist, node);
	g_hash_table_destroy(buddy->settings);
	g_free(buddy->name);
	g_free(buddy->alias);
	g_free(buddy);
}

void  gaim_blist_remove_chat (struct chat *chat)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;

	GaimBlistNode *gnode, *node = (GaimBlistNode*)chat;
	struct group *group;

	gnode = node->parent;
	group = (struct group *)gnode;

	if(gnode->child == node)
		gnode->child = node->next;
	if (node->prev)
		node->prev->next = node->next;
	if (node->next)
		node->next->prev = node->prev;
	group->totalsize--;
	if (chat->account->gc) {
		group->currentsize--;
		group->online--;
	}

	ops->remove(gaimbuddylist, node);
	g_hash_table_destroy(chat->components);
	g_free(chat->alias);
	g_free(chat);
}

void  gaim_blist_remove_group (struct group *group)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	GaimBlistNode *node = (GaimBlistNode*)group;

	if(node->child) {
		char *buf;
		int count = 0;
		GaimBlistNode *child = node->child;

		while(child) {
			count++;
			child = child->next;
		}

		buf = g_strdup_printf(_("%d buddies from group %s were not "
					"removed because their accounts were not logged in.  These "
					"buddies and the group were not removed.\n"),
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

	ops->remove(gaimbuddylist, node);
	g_free(group->name);
	g_free(group);
}

char *gaim_get_buddy_alias_only(struct buddy *b) {
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

char *  gaim_get_buddy_alias (struct buddy *buddy)
{
	char *ret = gaim_get_buddy_alias_only(buddy);
        if(!ret)
                return buddy ? buddy->name : _("Unknown");
        return ret;

}

struct buddy *gaim_find_buddy(GaimAccount *account, const char *name)
{
	static struct buddy *buddy = NULL;
	struct _gaim_hbuddy hb;
	GaimBlistNode *group;
	const char *n = NULL;

	if (!gaimbuddylist)
		return NULL;
	
	if (!name && !buddy);

	if (name) {
		group = gaimbuddylist->root;
		n = name;
	} else {
		group = ((GaimBlistNode*)buddy)->parent->next;
		n = buddy->name;
	}

	while (group) {
		hb.name = normalize(n);
		hb.account = account;
		hb.group = group;
		if ((buddy = g_hash_table_lookup(gaimbuddylist->buddies, &hb)) != NULL)
			return buddy;
		group = ((GaimBlistNode*)group)->next;
	}
	return NULL;
}

struct group *gaim_find_group(const char *name)
{
	GaimBlistNode *node;
	if (!gaimbuddylist)
		return NULL;
	node = gaimbuddylist->root;
	while(node) {
		if (!strcmp(((struct group*)node)->name, name))
			return (struct group*)node;
		node = node->next;
	}
	return NULL;
}
struct group *gaim_find_buddys_group(struct buddy *buddy)
{
	if (!buddy)
		return NULL;
	return (struct group*)(((GaimBlistNode*)buddy)->parent);
}

GSList *gaim_group_get_accounts(struct group *g)
{
	GSList *l = NULL;
	GaimBlistNode *child = ((GaimBlistNode *)g)->child;

	while (child) {
		if (!g_slist_find(l, ((struct buddy*)child)->account))
			l = g_slist_append(l, ((struct buddy*)child)->account);
		child = child->next;
	}
	return l;
}

void gaim_blist_add_account(GaimAccount *account)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	GaimBlistNode *group, *buddy;

	if(!gaimbuddylist)
		return;

	for(group = gaimbuddylist->root; group; group = group->next) {
		if(!GAIM_BLIST_NODE_IS_GROUP(group))
			continue;
		for(buddy = group->child; buddy; buddy = buddy->next) {
			if(GAIM_BLIST_NODE_IS_BUDDY(buddy)) {
				if (account == ((struct buddy*)buddy)->account) {
					((struct group *)group)->currentsize++;
					if(ops)
						ops->update(gaimbuddylist, buddy);
				}
			} else if(GAIM_BLIST_NODE_IS_CHAT(buddy)) {
				if (account == ((struct chat*)buddy)->account) {
					((struct group *)group)->online++;
					((struct group *)group)->currentsize++;
					if(ops)
						ops->update(gaimbuddylist, buddy);
				}
			}
		}
	}
}

void gaim_blist_remove_account(GaimAccount *account)
{
	struct gaim_blist_ui_ops *ops = gaimbuddylist->ui_ops;
	GaimBlistNode *group, *buddy;

	if (!gaimbuddylist)
		return;

	for(group = gaimbuddylist->root; group; group = group->next) {
		if(!GAIM_BLIST_NODE_IS_GROUP(group))
			continue;
		for(buddy = group->child; buddy; buddy = buddy->next) {
			if(GAIM_BLIST_NODE_IS_BUDDY(buddy)) {
				if (account == ((struct buddy*)buddy)->account) {
					if (GAIM_BUDDY_IS_ONLINE((struct buddy*)buddy))
						((struct group *)group)->online--;
					((struct buddy*)buddy)->present = GAIM_BUDDY_OFFLINE;
					((struct group *)group)->currentsize--;
					if(ops)
						ops->remove(gaimbuddylist, buddy);
				}
			} else if(GAIM_BLIST_NODE_IS_CHAT(buddy)) {
				if (account == ((struct chat*)buddy)->account) {
					((struct group *)group)->online--;
					((struct group *)group)->currentsize--;
					if(ops)
						ops->remove(gaimbuddylist, buddy);
				}
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
					struct group *g = gaim_group_new(current);
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
					struct buddy *b = gaim_buddy_new(account, nm, sw);
					struct group *g = gaim_find_group(current);
					gaim_blist_add_buddy(b, g,
							gaim_blist_get_last_child((GaimBlistNode*)g));
					bud = g_list_append(bud, g_strdup(nm));
				}
			} else if (*c == 'p') {
				gaim_privacy_permit_add(account, c + 2);
			} else if (*c == 'd') {
				gaim_privacy_deny_add(account, c + 2);
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
		char *file = gaim_user_dir();
		int protocol = (account->protocol == GAIM_PROTO_OSCAR) ? (isalpha(account->username[0]) ? GAIM_PROTO_TOC : GAIM_PROTO_ICQ): account->protocol;

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

gboolean gaim_group_on_account(struct group *g, GaimAccount *account) {
	GaimBlistNode *bnode;
	for(bnode = g->node.child; bnode; bnode = bnode->next) {
		struct buddy *b = (struct buddy *)bnode;
		if(!GAIM_BLIST_NODE_IS_BUDDY(bnode))
			continue;
		if((!account && b->account->gc) || b->account == account)
			return TRUE;
	}
	return FALSE;
}

static gboolean blist_safe_to_write = FALSE;

static char *blist_parser_group_name = NULL;
static char *blist_parser_person_name = NULL;
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
	BLIST_TAG_PERSON,
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
		tag_stack = g_list_prepend(tag_stack, GINT_TO_POINTER(BLIST_TAG_GROUP));
		for(i=0; attribute_names[i]; i++) {
			if(!strcmp(attribute_names[i], "name")) {
				g_free(blist_parser_group_name);
				blist_parser_group_name = g_strdup(attribute_values[i]);
			}
		}
		if(blist_parser_group_name) {
			struct group *g = gaim_group_new(blist_parser_group_name);
			gaim_blist_add_group(g,
					gaim_blist_get_last_sibling(gaimbuddylist->root));
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
	} else if(!strcmp(element_name, "person")) {
		tag_stack = g_list_prepend(tag_stack, GINT_TO_POINTER(BLIST_TAG_PERSON));
		for(i=0; attribute_names[i]; i++) {
			if(!strcmp(attribute_names[i], "name")) {
				g_free(blist_parser_person_name);
				blist_parser_person_name = g_strdup(attribute_values[i]);
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
			struct group *g = gaim_find_group(blist_parser_group_name);
			g_hash_table_destroy(g->settings);
			g->settings = blist_parser_group_settings;
		}
		tag_stack = g_list_delete_link(tag_stack, tag_stack);
		blist_parser_group_settings = NULL;
	} else if(!strcmp(element_name, "chat")) {
		GaimAccount *account = gaim_account_find(blist_parser_account_name,
				blist_parser_account_protocol);
		if(account) {
			struct chat *chat = gaim_chat_new(account, blist_parser_chat_alias, blist_parser_chat_components);
			struct group *g = gaim_find_group(blist_parser_group_name);
			gaim_blist_add_chat(chat,g,
					gaim_blist_get_last_child((GaimBlistNode*)g));
		}
		g_free(blist_parser_chat_alias);
		blist_parser_chat_alias = NULL;
		g_free(blist_parser_account_name);
		blist_parser_account_name = NULL;
		blist_parser_chat_components = NULL;
		tag_stack = g_list_delete_link(tag_stack, tag_stack);
	} else if(!strcmp(element_name, "person")) {
		g_free(blist_parser_person_name);
		blist_parser_person_name = NULL;
		tag_stack = g_list_delete_link(tag_stack, tag_stack);
	} else if(!strcmp(element_name, "buddy")) {
		GaimAccount *account = gaim_account_find(blist_parser_account_name,
				blist_parser_account_protocol);
		if(account) {
			struct buddy *b = gaim_buddy_new(account, blist_parser_buddy_name, blist_parser_buddy_alias);
			struct group *g = gaim_find_group(blist_parser_group_name);
			gaim_blist_add_buddy(b,g,
					gaim_blist_get_last_child((GaimBlistNode*)g));
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
		GaimAccount *account = gaim_account_find(blist_parser_account_name,
				blist_parser_account_protocol);
		if(account) {
			account->perm_deny = blist_parser_privacy_mode;
		}
		g_free(blist_parser_account_name);
		blist_parser_account_name = NULL;
		tag_stack = g_list_delete_link(tag_stack, tag_stack);
	} else if(!strcmp(element_name, "permit")) {
		GaimAccount *account = gaim_account_find(blist_parser_account_name,
				blist_parser_account_protocol);
		if(account) {
			gaim_privacy_permit_add(account, blist_parser_buddy_name);
		}
		g_free(blist_parser_buddy_name);
		blist_parser_buddy_name = NULL;
		tag_stack = g_list_delete_link(tag_stack, tag_stack);
	} else if(!strcmp(element_name, "block")) {
		GaimAccount *account = gaim_account_find(blist_parser_account_name,
				blist_parser_account_protocol);
		if(account) {
			gaim_privacy_deny_add(account, blist_parser_buddy_name);
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
		/* rob wants to inform the user that their buddy lists are
		 * being converted */
		msg = g_strdup_printf(_("Gaim is converting your old buddy lists "
					"to a new format, which will now be located at %s"),
				filename);
		gaim_notify_info(NULL, NULL, _("Converting Buddy List"), msg);
		g_free(msg);

		/* now, let gtk actually display the dialog before we start anything */
		while(gtk_events_pending())
			gtk_main_iteration();

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

static void gaim_blist_write(FILE *file, GaimAccount *exp_acct) {
	GList *accounts;
	GSList *buds;
	GaimBlistNode *gnode,*bnode;
	struct group *group;
	struct buddy *bud;
	fprintf(file, "<?xml version='1.0' encoding='UTF-8' ?>\n");
	fprintf(file, "<gaim version=\"1\">\n");
	fprintf(file, "\t<blist>\n");

	for(gnode = gaimbuddylist->root; gnode; gnode = gnode->next) {
		if(!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;
		group = (struct group *)gnode;
		if(!exp_acct || gaim_group_on_account(group, exp_acct)) {
			char *group_name = g_markup_escape_text(group->name, -1);
			fprintf(file, "\t\t<group name=\"%s\">\n", group_name);
			g_hash_table_foreach(group->settings, blist_print_group_settings, file);
			for(bnode = gnode->child; bnode; bnode = bnode->next) {
				if(GAIM_BLIST_NODE_IS_BUDDY(bnode)) {
					bud = (struct buddy *)bnode;
					if(!exp_acct || bud->account == exp_acct) {
						char *bud_name = g_markup_escape_text(bud->name, -1);
						char *bud_alias = NULL;
						char *acct_name = g_markup_escape_text(bud->account->username, -1);
						if(bud->alias)
							bud_alias= g_markup_escape_text(bud->alias, -1);
						fprintf(file, "\t\t\t<person name=\"%s\">\n",
								bud_alias ? bud_alias : bud_name);
						fprintf(file, "\t\t\t\t<buddy protocol=\"%d\" "
								"account=\"%s\">\n", bud->account->protocol,
								acct_name);
						fprintf(file, "\t\t\t\t\t<name>%s</name>\n", bud_name);
						if(bud_alias) {
							fprintf(file, "\t\t\t\t\t<alias>%s</alias>\n",
									bud_alias);
						}
						g_hash_table_foreach(bud->settings,
								blist_print_buddy_settings, file);
						fprintf(file, "\t\t\t\t</buddy>\n");
						fprintf(file, "\t\t\t</person>\n");
						g_free(bud_name);
						g_free(bud_alias);
						g_free(acct_name);
					}
				} else if(GAIM_BLIST_NODE_IS_CHAT(bnode)) {
					struct chat *chat = (struct chat *)bnode;
					if(!exp_acct || chat->account == exp_acct) {
						char *acct_name = g_markup_escape_text(chat->account->username, -1);
						fprintf(file, "\t\t\t<chat protocol=\"%d\" account=\"%s\">\n", chat->account->protocol, acct_name);
						if(chat->alias) {
							char *chat_alias = g_markup_escape_text(chat->alias, -1);
							fprintf(file, "\t\t\t\t<alias>%s</alias>\n", chat_alias);
							g_free(chat_alias);
						}
						g_hash_table_foreach(chat->components,
								blist_print_chat_components, file);
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
					"mode=\"%d\">\n", account->protocol, acct_name, account->perm_deny);
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

gboolean gaim_privacy_permit_add(GaimAccount *account, const char *who) {
	GSList *d = account->permit;
	char *n = g_strdup(normalize(who));
	while(d) {
		if(!gaim_utf8_strcasecmp(n, normalize(d->data)))
			break;
		d = d->next;
	}
	g_free(n);
	if(!d) {
		account->permit = g_slist_append(account->permit, g_strdup(who));
		return TRUE;
	}

	return FALSE;
}

gboolean gaim_privacy_permit_remove(GaimAccount *account, const char *who) {
	GSList *d = account->permit;
	char *n = g_strdup(normalize(who));
	while(d) {
		if(!gaim_utf8_strcasecmp(n, normalize(d->data)))
			break;
		d = d->next;
	}
	g_free(n);
	if(d) {
		account->permit = g_slist_remove(account->permit, d->data);
		g_free(d->data);
		return TRUE;
	}
	return FALSE;
}

gboolean gaim_privacy_deny_add(GaimAccount *account, const char *who) {
	GSList *d = account->deny;
	char *n = g_strdup(normalize(who));
	while(d) {
		if(!gaim_utf8_strcasecmp(n, normalize(d->data)))
			break;
		d = d->next;
	}
	g_free(n);
	if(!d) {
		account->deny = g_slist_append(account->deny, g_strdup(who));
		return TRUE;
	}

	return FALSE;
}

gboolean gaim_privacy_deny_remove(GaimAccount *account, const char *who) {
	GSList *d = account->deny;
	char *n = g_strdup(normalize(who));
	while(d) {
		if(!gaim_utf8_strcasecmp(n, normalize(d->data)))
			break;
		d = d->next;
	}
	g_free(n);
	if(d) {
		account->deny = g_slist_remove(account->deny, d->data);
		g_free(d->data);
		return TRUE;
	}
	return FALSE;
}

void gaim_group_set_setting(struct group *g, const char *key,
		const char *value) {
	if(!g)
		return;
	g_hash_table_replace(g->settings, g_strdup(key), g_strdup(value));
}

char *gaim_group_get_setting(struct group *g, const char *key) {
	if(!g)
		return NULL;
	return g_strdup(g_hash_table_lookup(g->settings, key));
}

void gaim_buddy_set_setting(struct buddy *b, const char *key,
		const char *value) {
	if(!b)
		return;
	g_hash_table_replace(b->settings, g_strdup(key), g_strdup(value));
}

char *gaim_buddy_get_setting(struct buddy *b, const char *key) {
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

int gaim_blist_get_group_size(struct group *group, gboolean offline) {
	if(!group)
		return 0;

	return offline ? group->totalsize : group->currentsize;
}

int gaim_blist_get_group_online_count(struct group *group) {
	if(!group)
		return 0;

	return group->online;
}


