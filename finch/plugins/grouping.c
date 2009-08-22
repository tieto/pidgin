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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA
 */

#define PURPLE_PLUGIN

#include "internal.h"
#include "purple.h"

#include "gntblist.h"
#include "gntplugin.h"

#include "gnttree.h"

static FinchBlistManager *default_manager;

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
			node = PURPLE_BLIST_NODE(purple_contact_get_priority_buddy(PURPLE_CONTACT(node)));
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
	PurpleBlistNode *node = selected_row;

	if (purple_blist_node_get_type(node) == PURPLE_BLIST_OTHER_NODE) {
		/* There should be some easy way of getting the total online count,
		 * or total number of chats. Doing a loop here will probably be pretty
		 * expensive. */
		if (body)
			*body = g_string_new(node == &online ? _("Online Buddies") : _("Offline Buddies"));
		return TRUE;
	} else {
		return default_manager ? default_manager->create_tooltip(selected_row, body, tool_title) : FALSE;
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
	if (PURPLE_BLIST_NODE_IS_CONTACT(node)) {
		PurpleBuddy *buddy = purple_contact_get_priority_buddy((PurpleContact*)node);
		if (buddy && !PURPLE_BUDDY_IS_ONLINE(buddy)) {
			return &meebo;
		}
	}
	return default_manager->find_parent(node);
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

/**
 * Nested Grouping
 */
static GHashTable *groups;

static gboolean
nested_group_init(void)
{
	groups = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, g_free);
	return TRUE;
}

static gboolean
nested_group_uninit(void)
{
	g_hash_table_destroy(groups);
	groups = NULL;
	return TRUE;
}

static gpointer
nested_group_find_parent(PurpleBlistNode *node)
{
	char *name;
	PurpleGroup *group;
	char *sep;
	PurpleBlistNode *ret, *parent;
	GntTree *tree;

	if (!PURPLE_BLIST_NODE_IS_GROUP(node))
		return default_manager->find_parent(node);

	group = (PurpleGroup *)node;
	name = g_strdup(purple_group_get_name(group));
	if (!(sep = strchr(name, '/'))) {
		g_free(name);
		return default_manager->find_parent(node);
	}

	tree = finch_blist_get_tree();
	parent = NULL;

	while (sep) {
		*sep = 0;
		if (*(sep + 1) && (ret = (PurpleBlistNode *)purple_find_group(name))) {
			finch_blist_manager_add_node(ret);
			parent = ret;
		} else if (!(ret = g_hash_table_lookup(groups, name))) {
			ret = g_new0(PurpleBlistNode, 1);
			g_hash_table_insert(groups, g_strdup(name), ret);
			ret->type = PURPLE_BLIST_OTHER_NODE;
			gnt_tree_add_row_last(tree, ret,
					gnt_tree_create_row(tree, name), parent);
			parent = ret;
		}
		*sep = '/';
		sep = strchr(sep + 1, '/');
	}

	g_free(name);
	return ret;
}

static gboolean
nested_group_create_tooltip(gpointer selected_row, GString **body, char **title)
{
	PurpleBlistNode *node = selected_row;
	if (!node ||
			purple_blist_node_get_type(node) != PURPLE_BLIST_OTHER_NODE)
		return default_manager->create_tooltip(selected_row, body, title);
	if (body)
		*body = g_string_new(_("Nested Subgroup"));  /* Perhaps list the child groups/subgroups? */
	return TRUE;
}

static gboolean
nested_group_can_add_node(PurpleBlistNode *node)
{
	PurpleBlistNode *group;
	int len;

	if (!PURPLE_BLIST_NODE_IS_GROUP(node))
		return default_manager->can_add_node(node);

	if (default_manager->can_add_node(node))
		return TRUE;

	len = strlen(purple_group_get_name((PurpleGroup*)node));
	group = purple_blist_get_root();
	for (; group; group = purple_blist_node_get_sibling_next(group)) {
		if (group == node)
			continue;
		if (strncmp(purple_group_get_name((PurpleGroup *)node),
					purple_group_get_name((PurpleGroup *)group), len) == 0 &&
				default_manager->can_add_node(group))
			return TRUE;
	}
	return FALSE;
}

static FinchBlistManager nested_group =
{
	"nested",
	N_("Nested Grouping (experimental)"),
	.init = nested_group_init,
	.uninit = nested_group_uninit,
	.find_parent = nested_group_find_parent,
	.create_tooltip = nested_group_create_tooltip,
	.can_add_node = nested_group_can_add_node,
};

static gboolean
plugin_load(PurplePlugin *plugin)
{
	default_manager = finch_blist_manager_find("default");

	finch_blist_install_manager(&on_offline);
	finch_blist_install_manager(&meebo_group);
	finch_blist_install_manager(&no_group);
	finch_blist_install_manager(&nested_group);
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	finch_blist_uninstall_manager(&on_offline);
	finch_blist_uninstall_manager(&meebo_group);
	finch_blist_uninstall_manager(&no_group);
	finch_blist_uninstall_manager(&nested_group);
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

PURPLE_INIT_PLUGIN(grouping, init_plugin, info)


