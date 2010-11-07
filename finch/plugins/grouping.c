/**
 * @file grouping.c  Provides different grouping options.
 *
 * Copyright (C) 2008 Sadrul Habib Chowdhury <sadrul@users.sourceforge.net>
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

#define PURPLE_PLUGIN

#include "internal.h"
#include "purple.h"

#include "gntblist.h"
#include "gntplugin.h"

#include "gnttree.h"

/**
 * Online/Offline
 */
static PurpleBlistNode online = {.type = PURPLE_BLIST_OTHER_NODE},
					   offline = {.type = PURPLE_BLIST_OTHER_NODE};

static gboolean on_offline_init()
{
	GntTree *tree = finch_blist_get_tree();

	gnt_tree_add_row_after(tree, &online,
			gnt_tree_create_row(tree, _("Online")), NULL, NULL);
	gnt_tree_add_row_after(tree, &offline,
			gnt_tree_create_row(tree, _("Offline")), NULL, &online);

	return TRUE;
}

static gboolean on_offline_can_add_node(PurpleBlistNode *node)
{
	switch (purple_blist_node_get_type(node)) {
		case PURPLE_BLIST_CONTACT_NODE:
			{
				PurpleContact *contact = (PurpleContact*)node;
				if (contact->currentsize > 0)
					return TRUE;
				return FALSE;
			}
			break;
		case PURPLE_BLIST_BUDDY_NODE:
			{
				PurpleBuddy *buddy = (PurpleBuddy*)node;
				if (PURPLE_BUDDY_IS_ONLINE(buddy))
					return TRUE;
				if (purple_prefs_get_bool("/finch/blist/showoffline") &&
						purple_account_is_connected(purple_buddy_get_account(buddy)))
					return TRUE;
				return FALSE;
			}
			break;
		case PURPLE_BLIST_CHAT_NODE:
			{
				PurpleChat *chat = (PurpleChat*)node;
				return purple_account_is_connected(purple_chat_get_account(chat));
			}
			break;
		default:
			return FALSE;
	}
}

static gpointer on_offline_find_parent(PurpleBlistNode *node)
{
	gpointer ret = NULL;

	switch (purple_blist_node_get_type(node)) {
		case PURPLE_BLIST_CONTACT_NODE:
			node = (PurpleBlistNode*)purple_contact_get_priority_buddy((PurpleContact*)node);
			ret = PURPLE_BUDDY_IS_ONLINE((PurpleBuddy*)node) ? &online : &offline;
			break;
		case PURPLE_BLIST_BUDDY_NODE:
			ret = purple_blist_node_get_parent(node);
			finch_blist_manager_add_node(ret);
			break;
		case PURPLE_BLIST_CHAT_NODE:
			ret = &online;
			break;
		default:
			break;
	}
	return ret;
}

static gboolean on_offline_create_tooltip(gpointer selected_row, GString **body, char **tool_title)
{
	static FinchBlistManager *def = NULL;
	PurpleBlistNode *node = selected_row;

	if (def == NULL)
		def = finch_blist_manager_find("default");

	if (purple_blist_node_get_type(node) == PURPLE_BLIST_OTHER_NODE) {
		/* There should be some easy way of getting the total online count,
		 * or total number of chats. Doing a loop here will probably be pretty
		 * expensive. */
		if (body)
			*body = g_string_new(node == &online ? _("Online Buddies") : _("Offline Buddies"));
		return TRUE;
	} else {
		return def ? def->create_tooltip(selected_row, body, tool_title) : FALSE;
	}
}

static FinchBlistManager on_offline =
{
	"on-offline",
	N_("Online/Offline"),
	on_offline_init,
	NULL,
	on_offline_can_add_node,
	on_offline_find_parent,
	on_offline_create_tooltip,
	{NULL, NULL, NULL, NULL}
};

/**
 * Meebo-like Grouping.
 */
static PurpleBlistNode meebo = {.type = PURPLE_BLIST_OTHER_NODE};
static gboolean meebo_init()
{
	GntTree *tree = finch_blist_get_tree();
	if (!g_list_find(gnt_tree_get_rows(tree), &meebo)) {
		gnt_tree_add_row_last(tree, &meebo,
				gnt_tree_create_row(tree, _("Offline")), NULL);
	}
	return TRUE;
}

static gpointer meebo_find_parent(PurpleBlistNode *node)
{
	static FinchBlistManager *def = NULL;
	if (def == NULL)
		def = finch_blist_manager_find("default");

	if (PURPLE_BLIST_NODE_IS_CONTACT(node)) {
		PurpleBuddy *buddy = purple_contact_get_priority_buddy((PurpleContact*)node);
		if (buddy && !PURPLE_BUDDY_IS_ONLINE(buddy)) {
			return &meebo;
		}
	}
	return def->find_parent(node);
}

static FinchBlistManager meebo_group =
{
	"meebo",
	N_("Meebo"),
	meebo_init,
	NULL,
	NULL,
	meebo_find_parent,
	NULL,
	{NULL, NULL, NULL, NULL}
};

/**
 * No Grouping.
 */
static gboolean no_group_init()
{
	GntTree *tree = finch_blist_get_tree();
	g_object_set(G_OBJECT(tree), "expander-level", 0, NULL);
	return TRUE;
}

static gboolean no_group_uninit()
{
	GntTree *tree = finch_blist_get_tree();
	g_object_set(G_OBJECT(tree), "expander-level", 1, NULL);
	return TRUE;
}

static gboolean no_group_can_add_node(PurpleBlistNode *node)
{
	return on_offline_can_add_node(node);   /* These happen to be the same */
}

static gpointer no_group_find_parent(PurpleBlistNode *node)
{
	gpointer ret = NULL;

	switch (purple_blist_node_get_type(node)) {
		case PURPLE_BLIST_BUDDY_NODE:
			ret = purple_blist_node_get_parent(node);
			finch_blist_manager_add_node(ret);
			break;
		default:
			break;
	}
	return ret;
}

static FinchBlistManager no_group =
{
	"no-group",
	N_("No Grouping"),
	no_group_init,
	no_group_uninit,
	no_group_can_add_node,
	no_group_find_parent,
	NULL,
	{NULL, NULL, NULL, NULL}
};

static gboolean
plugin_load(PurplePlugin *plugin)
{
	finch_blist_install_manager(&on_offline);
	finch_blist_install_manager(&meebo_group);
	finch_blist_install_manager(&no_group);
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	finch_blist_uninstall_manager(&on_offline);
	finch_blist_uninstall_manager(&meebo_group);
	finch_blist_uninstall_manager(&no_group);
	return TRUE;
}

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,
	FINCH_PLUGIN_TYPE,
	0,
	NULL,
	PURPLE_PRIORITY_DEFAULT,
	"grouping",
	N_("Grouping"),
	VERSION,
	N_("Provides alternate buddylist grouping options."),
	N_("Provides alternate buddylist grouping options."),
	"Sadrul H Chowdhury <sadrul@users.sourceforge.net>",
	PURPLE_WEBSITE,
	plugin_load,
	plugin_unload,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,NULL,NULL,NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
}

PURPLE_INIT_PLUGIN(ignore, init_plugin, info)


