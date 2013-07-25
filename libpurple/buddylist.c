/*
 * purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */
#include "internal.h"
#include "buddylist.h"
#include "conversation.h"
#include "dbus-maybe.h"
#include "debug.h"
#include "notify.h"
#include "pounce.h"
#include "prefs.h"
#include "prpl.h"
#include "server.h"
#include "signals.h"
#include "util.h"
#include "xmlnode.h"

#define PURPLE_BUDDY_LIST_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_BUDDY_LIST, PurpleBuddyListPrivate))

/** Private data for a buddy list. */
typedef struct  {
	GHashTable *buddies;  /**< Every buddy in this list */
} PurpleBuddyListPrivate;

static PurpleBlistUiOps *blist_ui_ops = NULL;

static PurpleBuddyList *purplebuddylist = NULL;

static GObjectClass *parent_class;

/**
 * A hash table used for efficient lookups of buddies by name.
 * PurpleAccount* => GHashTable*, with the inner hash table being
 * struct _purple_hbuddy => PurpleBuddy*
 */
static GHashTable *buddies_cache = NULL;

/**
 * A hash table used for efficient lookups of groups by name.
 * UTF-8 collate-key => PurpleGroup*.
 */
static GHashTable *groups_cache = NULL;

static guint          save_timer = 0;
static gboolean       blist_loaded = FALSE;

PurpleBlistNode *_purple_blist_get_last_child(PurpleBlistNode *node);

/*********************************************************************
 * Private utility functions                                         *
 *********************************************************************/

static PurpleBlistNode *purple_blist_get_last_sibling(PurpleBlistNode *node)
{
	PurpleBlistNode *n = node;
	if (!n)
		return NULL;
	while (n->next)
		n = n->next;
	return n;
}

PurpleBlistNode *_purple_blist_get_last_child(PurpleBlistNode *node)
{
	if (!node)
		return NULL;
	return purple_blist_get_last_sibling(node->child);
}

struct _list_account_buddies {
	GSList *list;
	PurpleAccount *account;
};

struct _purple_hbuddy {
	char *name;
	PurpleAccount *account;
	PurpleBlistNode *group;
};

/* This function must not use purple_normalize */
static guint _purple_blist_hbuddy_hash(struct _purple_hbuddy *hb)
{
	return g_str_hash(hb->name) ^ g_direct_hash(hb->group) ^ g_direct_hash(hb->account);
}

/* This function must not use purple_normalize */
static guint _purple_blist_hbuddy_equal(struct _purple_hbuddy *hb1, struct _purple_hbuddy *hb2)
{
	return (hb1->group == hb2->group &&
	        hb1->account == hb2->account &&
	        g_str_equal(hb1->name, hb2->name));
}

static void _purple_blist_hbuddy_free_key(struct _purple_hbuddy *hb)
{
	g_free(hb->name);
	g_free(hb);
}

static void
purple_blist_buddies_cache_add_account(PurpleAccount *account)
{
	GHashTable *account_buddies = g_hash_table_new_full((GHashFunc)_purple_blist_hbuddy_hash,
						(GEqualFunc)_purple_blist_hbuddy_equal,
						(GDestroyNotify)_purple_blist_hbuddy_free_key, NULL);
	g_hash_table_insert(buddies_cache, account, account_buddies);
}

static void
purple_blist_buddies_cache_remove_account(const PurpleAccount *account)
{
	g_hash_table_remove(buddies_cache, account);
}

/*********************************************************************
 * Writing to disk                                                   *
 *********************************************************************/

static void
value_to_xmlnode(gpointer key, gpointer hvalue, gpointer user_data)
{
	const char *name;
	GValue *value;
	xmlnode *node, *child;
	char buf[21];

	name    = (const char *)key;
	value   = (GValue *)hvalue;
	node    = (xmlnode *)user_data;

	g_return_if_fail(value != NULL);

	child = xmlnode_new_child(node, "setting");
	xmlnode_set_attrib(child, "name", name);

	if (G_VALUE_HOLDS_INT(value)) {
		xmlnode_set_attrib(child, "type", "int");
		g_snprintf(buf, sizeof(buf), "%d", g_value_get_int(value));
		xmlnode_insert_data(child, buf, -1);
	}
	else if (G_VALUE_HOLDS_STRING(value)) {
		xmlnode_set_attrib(child, "type", "string");
		xmlnode_insert_data(child, g_value_get_string(value), -1);
	}
	else if (G_VALUE_HOLDS_BOOLEAN(value)) {
		xmlnode_set_attrib(child, "type", "bool");
		g_snprintf(buf, sizeof(buf), "%d", g_value_get_boolean(value));
		xmlnode_insert_data(child, buf, -1);
	}
}

static void
chat_component_to_xmlnode(gpointer key, gpointer value, gpointer user_data)
{
	const char *name;
	const char *data;
	xmlnode *node, *child;

	name = (const char *)key;
	data = (const char *)value;
	node = (xmlnode *)user_data;

	g_return_if_fail(data != NULL);

	child = xmlnode_new_child(node, "component");
	xmlnode_set_attrib(child, "name", name);
	xmlnode_insert_data(child, data, -1);
}

static xmlnode *
buddy_to_xmlnode(PurpleBuddy *buddy)
{
	xmlnode *node, *child;
	PurpleAccount *account = purple_buddy_get_account(buddy);
	const char *alias = purple_buddy_get_local_alias(buddy);

	node = xmlnode_new("buddy");
	xmlnode_set_attrib(node, "account", purple_account_get_username(account));
	xmlnode_set_attrib(node, "proto", purple_account_get_protocol_id(account));

	child = xmlnode_new_child(node, "name");
	xmlnode_insert_data(child, purple_buddy_get_name(buddy), -1);

	if (alias != NULL)
	{
		child = xmlnode_new_child(node, "alias");
		xmlnode_insert_data(child, alias, -1);
	}

	/* Write buddy settings */
	g_hash_table_foreach(purple_blist_node_get_settings(PURPLE_BLIST_NODE(buddy)),
			value_to_xmlnode, node);

	return node;
}

static xmlnode *
contact_to_xmlnode(PurpleContact *contact)
{
	xmlnode *node, *child;
	PurpleBlistNode *bnode;
	gchar *alias;

	node = xmlnode_new("contact");
	g_object_get(contact, "alias", &alias, NULL);

	if (alias != NULL)
	{
		xmlnode_set_attrib(node, "alias", alias);
	}

	/* Write buddies */
	for (bnode = PURPLE_BLIST_NODE(contact)->child; bnode != NULL; bnode = bnode->next)
	{
		if (purple_blist_node_get_dont_save(bnode))
			continue;
		if (PURPLE_IS_BUDDY(bnode))
		{
			child = buddy_to_xmlnode(PURPLE_BUDDY(bnode));
			xmlnode_insert_child(node, child);
		}
	}

	/* Write contact settings */
	g_hash_table_foreach(purple_blist_node_get_settings(PURPLE_BLIST_NODE(contact)),
			value_to_xmlnode, node);

	g_free(alias);
	return node;
}

static xmlnode *
chat_to_xmlnode(PurpleChat *chat)
{
	xmlnode *node, *child;
	PurpleAccount *account = purple_chat_get_account(chat);
	gchar *alias;

	g_object_get(chat, "alias", &alias, NULL);

	node = xmlnode_new("chat");
	xmlnode_set_attrib(node, "proto", purple_account_get_protocol_id(account));
	xmlnode_set_attrib(node, "account", purple_account_get_username(account));

	if (alias != NULL)
	{
		child = xmlnode_new_child(node, "alias");
		xmlnode_insert_data(child, alias, -1);
	}

	/* Write chat components */
	g_hash_table_foreach(purple_chat_get_components(chat),
			chat_component_to_xmlnode, node);

	/* Write chat settings */
	g_hash_table_foreach(purple_blist_node_get_settings(PURPLE_BLIST_NODE(chat)),
			value_to_xmlnode, node);

	g_free(alias);
	return node;
}

static xmlnode *
group_to_xmlnode(PurpleGroup *group)
{
	xmlnode *node, *child;
	PurpleBlistNode *cnode;

	node = xmlnode_new("group");
	xmlnode_set_attrib(node, "name", purple_group_get_name(group));

	/* Write settings */
	g_hash_table_foreach(purple_blist_node_get_settings(PURPLE_BLIST_NODE(group)),
			value_to_xmlnode, node);

	/* Write contacts and chats */
	for (cnode = PURPLE_BLIST_NODE(group)->child; cnode != NULL; cnode = cnode->next)
	{
		if (purple_blist_node_get_dont_save(cnode))
			continue;
		if (PURPLE_IS_CONTACT(cnode))
		{
			child = contact_to_xmlnode(PURPLE_CONTACT(cnode));
			xmlnode_insert_child(node, child);
		}
		else if (PURPLE_IS_CHAT(cnode))
		{
			child = chat_to_xmlnode(PURPLE_CHAT(cnode));
			xmlnode_insert_child(node, child);
		}
	}

	return node;
}

static xmlnode *
accountprivacy_to_xmlnode(PurpleAccount *account)
{
	xmlnode *node, *child;
	GSList *cur;
	char buf[10];

	node = xmlnode_new("account");
	xmlnode_set_attrib(node, "proto", purple_account_get_protocol_id(account));
	xmlnode_set_attrib(node, "name", purple_account_get_username(account));
	g_snprintf(buf, sizeof(buf), "%d", purple_account_get_privacy_type(account));
	xmlnode_set_attrib(node, "mode", buf);

	for (cur = purple_account_privacy_get_permitted(account); cur; cur = cur->next)
	{
		child = xmlnode_new_child(node, "permit");
		xmlnode_insert_data(child, cur->data, -1);
	}

	for (cur = purple_account_privacy_get_denied(account); cur; cur = cur->next)
	{
		child = xmlnode_new_child(node, "block");
		xmlnode_insert_data(child, cur->data, -1);
	}

	return node;
}

static xmlnode *
blist_to_xmlnode(void)
{
	xmlnode *node, *child, *grandchild;
	PurpleBlistNode *gnode;
	GList *cur;

	node = xmlnode_new("purple");
	xmlnode_set_attrib(node, "version", "1.0");

	/* Write groups */
	child = xmlnode_new_child(node, "blist");
	for (gnode = purplebuddylist->root; gnode != NULL; gnode = gnode->next)
	{
		if (purple_blist_node_get_dont_save(gnode))
			continue;
		if (PURPLE_IS_GROUP(gnode))
		{
			grandchild = group_to_xmlnode(PURPLE_GROUP(gnode));
			xmlnode_insert_child(child, grandchild);
		}
	}

	/* Write privacy settings */
	child = xmlnode_new_child(node, "privacy");
	for (cur = purple_accounts_get_all(); cur != NULL; cur = cur->next)
	{
		grandchild = accountprivacy_to_xmlnode(cur->data);
		xmlnode_insert_child(child, grandchild);
	}

	return node;
}

static void
purple_blist_sync(void)
{
	xmlnode *node;
	char *data;

	if (!blist_loaded)
	{
		purple_debug_error("blist", "Attempted to save buddy list before it "
						 "was read!\n");
		return;
	}

	node = blist_to_xmlnode();
	data = xmlnode_to_formatted_str(node, NULL);
	purple_util_write_data_to_file("blist.xml", data, -1);
	g_free(data);
	xmlnode_free(node);
}

static gboolean
save_cb(gpointer data)
{
	purple_blist_sync();
	save_timer = 0;
	return FALSE;
}

static void
_purple_blist_schedule_save()
{
	if (save_timer == 0)
		save_timer = purple_timeout_add_seconds(5, save_cb, NULL);
}

static void
purple_blist_save_account(PurpleAccount *account)
{
#if 1
	_purple_blist_schedule_save();
#else
	if (account != NULL) {
		/* Save the buddies and privacy data for this account */
	} else {
		/* Save all buddies and privacy data */
	}
#endif
}

static void
purple_blist_save_node(PurpleBlistNode *node)
{
	_purple_blist_schedule_save();
}

void purple_blist_schedule_save()
{
	PurpleBlistUiOps *ops = purple_blist_get_ui_ops();

	/* Save everything */
	if (ops && ops->save_account)
		ops->save_account(NULL);
}

/*********************************************************************
 * Reading from disk                                                 *
 *********************************************************************/

static void
parse_setting(PurpleBlistNode *node, xmlnode *setting)
{
	const char *name = xmlnode_get_attrib(setting, "name");
	const char *type = xmlnode_get_attrib(setting, "type");
	char *value = xmlnode_get_data(setting);

	if (!value)
		return;

	if (!type || purple_strequal(type, "string"))
		purple_blist_node_set_string(node, name, value);
	else if (purple_strequal(type, "bool"))
		purple_blist_node_set_bool(node, name, atoi(value));
	else if (purple_strequal(type, "int"))
		purple_blist_node_set_int(node, name, atoi(value));

	g_free(value);
}

static void
parse_buddy(PurpleGroup *group, PurpleContact *contact, xmlnode *bnode)
{
	PurpleAccount *account;
	PurpleBuddy *buddy;
	char *name = NULL, *alias = NULL;
	const char *acct_name, *proto;
	xmlnode *x;

	acct_name = xmlnode_get_attrib(bnode, "account");
	proto = xmlnode_get_attrib(bnode, "proto");

	if (!acct_name || !proto)
		return;

	account = purple_accounts_find(acct_name, proto);

	if (!account)
		return;

	if ((x = xmlnode_get_child(bnode, "name")))
		name = xmlnode_get_data(x);

	if (!name)
		return;

	if ((x = xmlnode_get_child(bnode, "alias")))
		alias = xmlnode_get_data(x);

	buddy = purple_buddy_new(account, name, alias);
	purple_blist_add_buddy(buddy, contact, group,
			_purple_blist_get_last_child((PurpleBlistNode*)contact));

	for (x = xmlnode_get_child(bnode, "setting"); x; x = xmlnode_get_next_twin(x)) {
		parse_setting((PurpleBlistNode*)buddy, x);
	}

	g_free(name);
	g_free(alias);
}

static void
parse_contact(PurpleGroup *group, xmlnode *cnode)
{
	PurpleContact *contact = purple_contact_new();
	xmlnode *x;
	const char *alias;

	purple_blist_add_contact(contact, group,
			_purple_blist_get_last_child((PurpleBlistNode*)group));

	if ((alias = xmlnode_get_attrib(cnode, "alias"))) {
		purple_contact_set_alias(contact, alias);
	}

	for (x = cnode->child; x; x = x->next) {
		if (x->type != XMLNODE_TYPE_TAG)
			continue;
		if (purple_strequal(x->name, "buddy"))
			parse_buddy(group, contact, x);
		else if (purple_strequal(x->name, "setting"))
			parse_setting(PURPLE_BLIST_NODE(contact), x);
	}

	/* if the contact is empty, don't keep it around.  it causes problems */
	if (!PURPLE_BLIST_NODE(contact)->child)
		purple_blist_remove_contact(contact);
}

static void
parse_chat(PurpleGroup *group, xmlnode *cnode)
{
	PurpleChat *chat;
	PurpleAccount *account;
	const char *acct_name, *proto;
	xmlnode *x;
	char *alias = NULL;
	GHashTable *components;

	acct_name = xmlnode_get_attrib(cnode, "account");
	proto = xmlnode_get_attrib(cnode, "proto");

	if (!acct_name || !proto)
		return;

	account = purple_accounts_find(acct_name, proto);

	if (!account)
		return;

	if ((x = xmlnode_get_child(cnode, "alias")))
		alias = xmlnode_get_data(x);

	components = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	for (x = xmlnode_get_child(cnode, "component"); x; x = xmlnode_get_next_twin(x)) {
		const char *name;
		char *value;

		name = xmlnode_get_attrib(x, "name");
		value = xmlnode_get_data(x);
		g_hash_table_replace(components, g_strdup(name), value);
	}

	chat = purple_chat_new(account, alias, components);
	purple_blist_add_chat(chat, group,
			_purple_blist_get_last_child((PurpleBlistNode*)group));

	for (x = xmlnode_get_child(cnode, "setting"); x; x = xmlnode_get_next_twin(x)) {
		parse_setting((PurpleBlistNode*)chat, x);
	}

	g_free(alias);
}

static void
parse_group(xmlnode *groupnode)
{
	const char *name = xmlnode_get_attrib(groupnode, "name");
	PurpleGroup *group;
	xmlnode *cnode;

	if (!name)
		name = _("Buddies");

	group = purple_group_new(name);
	purple_blist_add_group(group,
			purple_blist_get_last_sibling(purplebuddylist->root));

	for (cnode = groupnode->child; cnode; cnode = cnode->next) {
		if (cnode->type != XMLNODE_TYPE_TAG)
			continue;
		if (purple_strequal(cnode->name, "setting"))
			parse_setting((PurpleBlistNode*)group, cnode);
		else if (purple_strequal(cnode->name, "contact") ||
				purple_strequal(cnode->name, "person"))
			parse_contact(group, cnode);
		else if (purple_strequal(cnode->name, "chat"))
			parse_chat(group, cnode);
	}
}

static void
load_blist(void)
{
	xmlnode *purple, *blist, *privacy;

	blist_loaded = TRUE;

	purple = purple_util_read_xml_from_file("blist.xml", _("buddy list"));

	if (purple == NULL)
		return;

	blist = xmlnode_get_child(purple, "blist");
	if (blist) {
		xmlnode *groupnode;
		for (groupnode = xmlnode_get_child(blist, "group"); groupnode != NULL;
				groupnode = xmlnode_get_next_twin(groupnode)) {
			parse_group(groupnode);
		}
	}

	privacy = xmlnode_get_child(purple, "privacy");
	if (privacy) {
		xmlnode *anode;
		for (anode = privacy->child; anode; anode = anode->next) {
			xmlnode *x;
			PurpleAccount *account;
			int imode;
			const char *acct_name, *proto, *mode;

			acct_name = xmlnode_get_attrib(anode, "name");
			proto = xmlnode_get_attrib(anode, "proto");
			mode = xmlnode_get_attrib(anode, "mode");

			if (!acct_name || !proto || !mode)
				continue;

			account = purple_accounts_find(acct_name, proto);

			if (!account)
				continue;

			imode = atoi(mode);
			purple_account_set_privacy_type(account, (imode != 0 ? imode : PURPLE_ACCOUNT_PRIVACY_ALLOW_ALL));

			for (x = anode->child; x; x = x->next) {
				char *name;
				if (x->type != XMLNODE_TYPE_TAG)
					continue;

				if (purple_strequal(x->name, "permit")) {
					name = xmlnode_get_data(x);
					purple_account_privacy_permit_add(account, name, TRUE);
					g_free(name);
				} else if (purple_strequal(x->name, "block")) {
					name = xmlnode_get_data(x);
					purple_account_privacy_deny_add(account, name, TRUE);
					g_free(name);
				}
			}
		}
	}

	xmlnode_free(purple);

	/* This tells the buddy icon code to do its thing. */
	_purple_buddy_icons_blist_loaded_cb();
}

/*****************************************************************************
 * Public API functions                                                      *
 *****************************************************************************/

void
purple_blist_boot(void)
{
	PurpleBlistUiOps *ui_ops;
	GList *account;
	PurpleBuddyList *gbl = g_object_new(PURPLE_TYPE_BUDDY_LIST, NULL);

	ui_ops = purple_blist_get_ui_ops();

	buddies_cache = g_hash_table_new_full(g_direct_hash, g_direct_equal,
					 NULL, (GDestroyNotify)g_hash_table_destroy);

	groups_cache = g_hash_table_new_full((GHashFunc)g_str_hash,
					 (GEqualFunc)g_str_equal,
					 (GDestroyNotify)g_free, NULL);

	for (account = purple_accounts_get_all(); account != NULL; account = account->next)
	{
		purple_blist_buddies_cache_add_account(account->data);
	}

	if (ui_ops != NULL && ui_ops->new_list != NULL)
		ui_ops->new_list(gbl);

	purplebuddylist = gbl;

	load_blist();
}

PurpleBuddyList *
purple_blist_get_buddy_list()
{
	return purplebuddylist;
}

PurpleBlistNode *
purple_blist_get_root()
{
	return purplebuddylist ? purplebuddylist->root : NULL;
}

static void
append_buddy(gpointer key, gpointer value, gpointer user_data)
{
	GSList **list = user_data;
	*list = g_slist_prepend(*list, value);
}

GSList *
purple_blist_get_buddies()
{
	GSList *buddies = NULL;

	if (!purplebuddylist)
		return NULL;

	g_hash_table_foreach(PURPLE_BUDDY_LIST_GET_PRIVATE(purplebuddylist)->buddies,
			append_buddy, &buddies);
	return buddies;
}

void *
purple_blist_get_ui_data()
{
	return purplebuddylist->ui_data;
}

void
purple_blist_set_ui_data(void *ui_data)
{
	purplebuddylist->ui_data = ui_data;
}

void purple_blist_show()
{
	PurpleBlistUiOps *ops = purple_blist_get_ui_ops();

	if (ops && ops->show)
		ops->show(purplebuddylist);
}

void purple_blist_set_visible(gboolean show)
{
	PurpleBlistUiOps *ops = purple_blist_get_ui_ops();

	if (ops && ops->set_visible)
		ops->set_visible(purplebuddylist, show);
}

void purple_blist_update_buddies_cache(PurpleBuddy *buddy, const char *new_name)
{
	struct _purple_hbuddy *hb, *hb2;
	GHashTable *account_buddies;
	PurpleAccount *account;
	gchar *name;
	PurpleBuddyListPrivate *priv = PURPLE_BUDDY_LIST_GET_PRIVATE(purplebuddylist);

	g_return_if_fail(buddy != NULL);

	account = purple_buddy_get_account(buddy);
	name = (gchar *)purple_buddy_get_name(buddy);

	hb = g_new(struct _purple_hbuddy, 1);
	hb->name = (gchar *)purple_normalize(account, name);
	hb->account = account;
	hb->group = PURPLE_BLIST_NODE(buddy)->parent->parent;
	g_hash_table_remove(priv->buddies, hb);

	account_buddies = g_hash_table_lookup(buddies_cache, account);
	g_hash_table_remove(account_buddies, hb);

	hb->name = g_strdup(purple_normalize(account, new_name));
	g_hash_table_replace(priv->buddies, hb, buddy);

	hb2 = g_new(struct _purple_hbuddy, 1);
	hb2->name = g_strdup(hb->name);
	hb2->account = account;
	hb2->group = PURPLE_BLIST_NODE(buddy)->parent->parent;

	g_hash_table_replace(account_buddies, hb2, buddy);
}

void purple_blist_update_groups_cache(PurpleGroup *group, const char *new_name)
{
		gchar* key = g_utf8_collate_key(purple_group_get_name(group), -1);
		g_hash_table_remove(groups_cache, key);
		g_free(key);

		key = g_utf8_collate_key(new_name, -1);
		g_hash_table_insert(groups_cache, key, group);
}

void purple_blist_add_chat(PurpleChat *chat, PurpleGroup *group, PurpleBlistNode *node)
{
	PurpleBlistNode *cnode = PURPLE_BLIST_NODE(chat);
	PurpleBlistUiOps *ops = purple_blist_get_ui_ops();
	PurpleCountingNode *group_counter;

	g_return_if_fail(chat != NULL);

	if (node == NULL) {
		if (group == NULL)
			group = purple_group_new(_("Chats"));

		/* Add group to blist if isn't already on it. Fixes #2752. */
		if (!purple_blist_find_group(purple_group_get_name(group))) {
			purple_blist_add_group(group,
					purple_blist_get_last_sibling(purplebuddylist->root));
		}
	} else {
		group = PURPLE_GROUP(node->parent);
	}

	/* if we're moving to overtop of ourselves, do nothing */
	if (cnode == node)
		return;

	if (cnode->parent) {
		/* This chat was already in the list and is
		 * being moved.
		 */
		group_counter = PURPLE_COUNTING_NODE(cnode->parent);
		purple_counting_node_change_total_size(group_counter, -1);
		if (purple_account_is_connected(purple_chat_get_account(chat))) {
			purple_counting_node_change_online_count(group_counter, -1);
			purple_counting_node_change_current_size(group_counter, -1);
		}
		if (cnode->next)
			cnode->next->prev = cnode->prev;
		if (cnode->prev)
			cnode->prev->next = cnode->next;
		if (cnode->parent->child == cnode)
			cnode->parent->child = cnode->next;

		if (ops && ops->remove)
			ops->remove(purplebuddylist, cnode);
		/* ops->remove() cleaned up the cnode's ui_data, so we need to
		 * reinitialize it */
		if (ops && ops->new_node)
			ops->new_node(cnode);
	}

	if (node != NULL) {
		if (node->next)
			node->next->prev = cnode;
		cnode->next = node->next;
		cnode->prev = node;
		cnode->parent = node->parent;
		node->next = cnode;
		group_counter = PURPLE_COUNTING_NODE(node->parent);
		purple_counting_node_change_total_size(group_counter, +1);
		if (purple_account_is_connected(purple_chat_get_account(chat))) {
			purple_counting_node_change_online_count(group_counter, +1);
			purple_counting_node_change_current_size(group_counter, +1);
		}
	} else {
		if (((PurpleBlistNode *)group)->child)
			((PurpleBlistNode *)group)->child->prev = cnode;
		cnode->next = ((PurpleBlistNode *)group)->child;
		cnode->prev = NULL;
		((PurpleBlistNode *)group)->child = cnode;
		cnode->parent = PURPLE_BLIST_NODE(group);
		group_counter = PURPLE_COUNTING_NODE(group);
		purple_counting_node_change_total_size(group_counter, +1);
		if (purple_account_is_connected(purple_chat_get_account(chat))) {
			purple_counting_node_change_online_count(group_counter, +1);
			purple_counting_node_change_current_size(group_counter, +1);
		}
	}

	if (ops) {
		if (ops->save_node)
			ops->save_node(cnode);
		if (ops->update)
			ops->update(purplebuddylist, PURPLE_BLIST_NODE(cnode));
	}

	purple_signal_emit(purple_blist_get_handle(), "blist-node-added",
			cnode);
}

void purple_blist_add_buddy(PurpleBuddy *buddy, PurpleContact *contact, PurpleGroup *group, PurpleBlistNode *node)
{
	PurpleBlistNode *cnode, *bnode;
	PurpleCountingNode *contact_counter, *group_counter;
	PurpleGroup *g;
	PurpleContact *c;
	PurpleAccount *account;
	PurpleBlistUiOps *ops = purple_blist_get_ui_ops();
	struct _purple_hbuddy *hb, *hb2;
	GHashTable *account_buddies;
	PurpleBuddyListPrivate *priv = PURPLE_BUDDY_LIST_GET_PRIVATE(purplebuddylist);

	g_return_if_fail(buddy != NULL);
	g_return_if_fail(PURPLE_IS_BUDDY((PurpleBlistNode*)buddy));

	bnode = PURPLE_BLIST_NODE(buddy);
	account = purple_buddy_get_account(buddy);

	/* if we're moving to overtop of ourselves, do nothing */
	if (bnode == node || (!node && bnode->parent &&
				contact && bnode->parent == (PurpleBlistNode*)contact
				&& bnode == bnode->parent->child))
		return;

	if (node && PURPLE_IS_BUDDY(node)) {
		c = (PurpleContact*)node->parent;
		g = (PurpleGroup*)node->parent->parent;
	} else if (contact) {
		c = contact;
		g = PURPLE_GROUP(PURPLE_BLIST_NODE(c)->parent);
	} else {
		g = group;
		if (g == NULL)
			g = purple_group_new(_("Buddies"));
		/* Add group to blist if isn't already on it. Fixes #2752. */
		if (!purple_blist_find_group(purple_group_get_name(g))) {
			purple_blist_add_group(g,
					purple_blist_get_last_sibling(purplebuddylist->root));
		}
		c = purple_contact_new();
		purple_blist_add_contact(c, g,
				_purple_blist_get_last_child((PurpleBlistNode*)g));
	}

	cnode = PURPLE_BLIST_NODE(c);

	if (bnode->parent) {
		contact_counter = PURPLE_COUNTING_NODE(bnode->parent);
		group_counter = PURPLE_COUNTING_NODE(bnode->parent->parent);

		if (PURPLE_BUDDY_IS_ONLINE(buddy)) {
			purple_counting_node_change_online_count(contact_counter, -1);
			if (purple_counting_node_get_online_count(contact_counter) == 0)
				purple_counting_node_change_online_count(group_counter, -1);
		}
		if (purple_account_is_connected(account)) {
			purple_counting_node_change_current_size(contact_counter, -1);
			if (purple_counting_node_get_current_size(contact_counter) == 0)
				purple_counting_node_change_current_size(group_counter, -1);
		}
		purple_counting_node_change_total_size(contact_counter, -1);
		/* the group totalsize will be taken care of by remove_contact below */

		if (bnode->parent->parent != (PurpleBlistNode*)g)
			serv_move_buddy(buddy, (PurpleGroup *)bnode->parent->parent, g);

		if (bnode->next)
			bnode->next->prev = bnode->prev;
		if (bnode->prev)
			bnode->prev->next = bnode->next;
		if (bnode->parent->child == bnode)
			bnode->parent->child = bnode->next;

		if (ops && ops->remove)
			ops->remove(purplebuddylist, bnode);

		if (bnode->parent->parent != (PurpleBlistNode*)g) {
			struct _purple_hbuddy hb;
			hb.name = (gchar *)purple_normalize(account,
					purple_buddy_get_name(buddy));
			hb.account = account;
			hb.group = bnode->parent->parent;
			g_hash_table_remove(priv->buddies, &hb);

			account_buddies = g_hash_table_lookup(buddies_cache, account);
			g_hash_table_remove(account_buddies, &hb);
		}

		if (!bnode->parent->child) {
			purple_blist_remove_contact((PurpleContact*)bnode->parent);
		} else {
			purple_contact_invalidate_priority_buddy((PurpleContact*)bnode->parent);

			if (ops && ops->update)
				ops->update(purplebuddylist, bnode->parent);
		}
	}

	if (node && PURPLE_IS_BUDDY(node)) {
		if (node->next)
			node->next->prev = bnode;
		bnode->next = node->next;
		bnode->prev = node;
		bnode->parent = node->parent;
		node->next = bnode;
	} else {
		if (cnode->child)
			cnode->child->prev = bnode;
		bnode->prev = NULL;
		bnode->next = cnode->child;
		cnode->child = bnode;
		bnode->parent = cnode;
	}

	contact_counter = PURPLE_COUNTING_NODE(bnode->parent);
	group_counter = PURPLE_COUNTING_NODE(bnode->parent->parent);

	if (PURPLE_BUDDY_IS_ONLINE(buddy)) {
		purple_counting_node_change_online_count(contact_counter, +1);
		if (purple_counting_node_get_online_count(contact_counter) == 1)
			purple_counting_node_change_online_count(group_counter, +1);
	}
	if (purple_account_is_connected(account)) {
		purple_counting_node_change_current_size(contact_counter, +1);
		if (purple_counting_node_get_online_count(contact_counter) == 1)
			purple_counting_node_change_current_size(group_counter, +1);
	}
	purple_counting_node_change_total_size(contact_counter, +1);

	hb = g_new(struct _purple_hbuddy, 1);
	hb->name = g_strdup(purple_normalize(account, purple_buddy_get_name(buddy)));
	hb->account = account;
	hb->group = PURPLE_BLIST_NODE(buddy)->parent->parent;

	g_hash_table_replace(priv->buddies, hb, buddy);

	account_buddies = g_hash_table_lookup(buddies_cache, account);

	hb2 = g_new(struct _purple_hbuddy, 1);
	hb2->name = g_strdup(hb->name);
	hb2->account = account;
	hb2->group = ((PurpleBlistNode*)buddy)->parent->parent;

	g_hash_table_replace(account_buddies, hb2, buddy);

	purple_contact_invalidate_priority_buddy(purple_buddy_get_contact(buddy));

	if (ops) {
		if (ops->save_node)
			ops->save_node((PurpleBlistNode*) buddy);
		if (ops->update)
			ops->update(purplebuddylist, PURPLE_BLIST_NODE(buddy));
	}

	/* Signal that the buddy has been added */
	purple_signal_emit(purple_blist_get_handle(), "buddy-added", buddy);

	purple_signal_emit(purple_blist_get_handle(), "blist-node-added",
			PURPLE_BLIST_NODE(buddy));
}

void purple_blist_add_contact(PurpleContact *contact, PurpleGroup *group, PurpleBlistNode *node)
{
	PurpleBlistUiOps *ops = purple_blist_get_ui_ops();
	PurpleGroup *g;
	PurpleBlistNode *gnode, *cnode, *bnode;
	PurpleCountingNode *contact_counter, *group_counter;
	PurpleBuddyListPrivate *priv = PURPLE_BUDDY_LIST_GET_PRIVATE(purplebuddylist);

	g_return_if_fail(contact != NULL);

	if (PURPLE_BLIST_NODE(contact) == node)
		return;

	if (node && (PURPLE_IS_CONTACT(node) ||
				PURPLE_IS_CHAT(node)))
		g = PURPLE_GROUP(node->parent);
	else if (group)
		g = group;
	else {
		g = purple_blist_find_group(_("Buddies"));
		if (g == NULL) {
			g = purple_group_new(_("Buddies"));
			purple_blist_add_group(g,
					purple_blist_get_last_sibling(purplebuddylist->root));
		}
	}

	gnode = (PurpleBlistNode*)g;
	cnode = (PurpleBlistNode*)contact;

	if (cnode->parent) {
		if (cnode->parent->child == cnode)
			cnode->parent->child = cnode->next;
		if (cnode->prev)
			cnode->prev->next = cnode->next;
		if (cnode->next)
			cnode->next->prev = cnode->prev;

		if (cnode->parent != gnode) {
			bnode = cnode->child;
			while (bnode) {
				PurpleBlistNode *next_bnode = bnode->next;
				PurpleBuddy *b = PURPLE_BUDDY(bnode);
				PurpleAccount *account = purple_buddy_get_account(b);
				GHashTable *account_buddies;

				struct _purple_hbuddy *hb, *hb2;

				hb = g_new(struct _purple_hbuddy, 1);
				hb->name = g_strdup(purple_normalize(account, purple_buddy_get_name(b)));
				hb->account = account;
				hb->group = cnode->parent;

				g_hash_table_remove(priv->buddies, hb);

				account_buddies = g_hash_table_lookup(buddies_cache, account);
				g_hash_table_remove(account_buddies, hb);

				if (!purple_blist_find_buddy_in_group(account, purple_buddy_get_name(b), g)) {
					hb->group = gnode;
					g_hash_table_replace(priv->buddies, hb, b);

					hb2 = g_new(struct _purple_hbuddy, 1);
					hb2->name = g_strdup(hb->name);
					hb2->account = account;
					hb2->group = gnode;

					g_hash_table_replace(account_buddies, hb2, b);

					if (purple_account_get_connection(account))
						serv_move_buddy(b, (PurpleGroup *)cnode->parent, g);
				} else {
					gboolean empty_contact = FALSE;

					/* this buddy already exists in the group, so we're
					 * gonna delete it instead */
					g_free(hb->name);
					g_free(hb);
					if (purple_account_get_connection(account))
						purple_account_remove_buddy(account, b, PURPLE_GROUP(cnode->parent));

					if (!cnode->child->next)
						empty_contact = TRUE;
					purple_blist_remove_buddy(b);

					/** in purple_blist_remove_buddy(), if the last buddy in a
					 * contact is removed, the contact is cleaned up and
					 * g_free'd, so we mustn't try to reference bnode->next */
					if (empty_contact)
						return;
				}
				bnode = next_bnode;
			}
		}

		contact_counter = PURPLE_COUNTING_NODE(contact);
		group_counter = PURPLE_COUNTING_NODE(cnode->parent);

		if (purple_counting_node_get_online_count(contact_counter) > 0)
			purple_counting_node_change_online_count(group_counter, -1);
		if (purple_counting_node_get_current_size(contact_counter) > 0)
			purple_counting_node_change_current_size(group_counter, -1);
		purple_counting_node_change_total_size(group_counter, -1);

		if (ops && ops->remove)
			ops->remove(purplebuddylist, cnode);

		if (ops && ops->remove_node)
			ops->remove_node(cnode);
	}

	if (node && (PURPLE_IS_CONTACT(node) ||
				PURPLE_IS_CHAT(node))) {
		if (node->next)
			node->next->prev = cnode;
		cnode->next = node->next;
		cnode->prev = node;
		cnode->parent = node->parent;
		node->next = cnode;
	} else {
		if (gnode->child)
			gnode->child->prev = cnode;
		cnode->prev = NULL;
		cnode->next = gnode->child;
		gnode->child = cnode;
		cnode->parent = gnode;
	}

	contact_counter = PURPLE_COUNTING_NODE(contact);
	group_counter = PURPLE_COUNTING_NODE(g);

	if (purple_counting_node_get_online_count(contact_counter) > 0)
		purple_counting_node_change_online_count(group_counter, +1);
	if (purple_counting_node_get_current_size(contact_counter) > 0)
		purple_counting_node_change_current_size(group_counter, +1);
	purple_counting_node_change_total_size(group_counter, +1);

	if (ops && ops->save_node)
	{
		if (cnode->child)
			ops->save_node(cnode);
		for (bnode = cnode->child; bnode; bnode = bnode->next)
			ops->save_node(bnode);
	}

	if (ops && ops->update)
	{
		if (cnode->child)
			ops->update(purplebuddylist, cnode);

		for (bnode = cnode->child; bnode; bnode = bnode->next)
			ops->update(purplebuddylist, bnode);
	}
}

void purple_blist_add_group(PurpleGroup *group, PurpleBlistNode *node)
{
	PurpleBlistUiOps *ops;
	PurpleBlistNode *gnode = (PurpleBlistNode*)group;
	gchar* key;

	g_return_if_fail(group != NULL);
	g_return_if_fail(PURPLE_IS_GROUP((PurpleBlistNode *)group));

	ops = purple_blist_get_ui_ops();

	/* if we're moving to overtop of ourselves, do nothing */
	if (gnode == node) {
		if (!purplebuddylist->root)
			node = NULL;
		else
			return;
	}

	if (purple_blist_find_group(purple_group_get_name(group))) {
		/* This is just being moved */

		if (ops && ops->remove)
			ops->remove(purplebuddylist, (PurpleBlistNode *)group);

		if (gnode == purplebuddylist->root)
			purplebuddylist->root = gnode->next;
		if (gnode->prev)
			gnode->prev->next = gnode->next;
		if (gnode->next)
			gnode->next->prev = gnode->prev;
	} else {
		key = g_utf8_collate_key(purple_group_get_name(group), -1);
		g_hash_table_insert(groups_cache, key, group);
	}

	if (node && PURPLE_IS_GROUP(node)) {
		gnode->next = node->next;
		gnode->prev = node;
		if (node->next)
			node->next->prev = gnode;
		node->next = gnode;
	} else {
		if (purplebuddylist->root)
			purplebuddylist->root->prev = gnode;
		gnode->next = purplebuddylist->root;
		gnode->prev = NULL;
		purplebuddylist->root = gnode;
	}

	if (ops && ops->save_node) {
		ops->save_node(gnode);
		for (node = gnode->child; node; node = node->next)
			ops->save_node(node);
	}

	if (ops && ops->update) {
		ops->update(purplebuddylist, gnode);
		for (node = gnode->child; node; node = node->next)
			ops->update(purplebuddylist, node);
	}

	purple_signal_emit(purple_blist_get_handle(), "blist-node-added",
			gnode);
}

void purple_blist_remove_contact(PurpleContact *contact)
{
	PurpleBlistUiOps *ops = purple_blist_get_ui_ops();
	PurpleBlistNode *node, *gnode;
	PurpleGroup *group;

	g_return_if_fail(contact != NULL);

	node = (PurpleBlistNode *)contact;
	gnode = node->parent;
	group = PURPLE_GROUP(gnode);

	if (node->child) {
		/*
		 * If this contact has children then remove them.  When the last
		 * buddy is removed from the contact, the contact is automatically
		 * deleted.
		 */
		while (node->child->next) {
			purple_blist_remove_buddy((PurpleBuddy*)node->child);
		}
		/*
		 * Remove the last buddy and trigger the deletion of the contact.
		 * It would probably be cleaner if contact-deletion was done after
		 * a timeout?  Or if it had to be done manually, like below?
		 */
		purple_blist_remove_buddy((PurpleBuddy*)node->child);
	} else {
		/* Remove the node from its parent */
		if (gnode->child == node)
			gnode->child = node->next;
		if (node->prev)
			node->prev->next = node->next;
		if (node->next)
			node->next->prev = node->prev;
		purple_counting_node_change_total_size(PURPLE_COUNTING_NODE(group), -1);

		/* Update the UI */
		if (ops && ops->remove)
			ops->remove(purplebuddylist, node);

		if (ops && ops->remove_node)
			ops->remove_node(node);

		purple_signal_emit(purple_blist_get_handle(), "blist-node-removed",
				PURPLE_BLIST_NODE(contact));

		/* Delete the node */
		g_object_unref(contact);
	}
}

void purple_blist_remove_buddy(PurpleBuddy *buddy)
{
	PurpleBlistUiOps *ops = purple_blist_get_ui_ops();
	PurpleBlistNode *node, *cnode, *gnode;
	PurpleCountingNode *contact_counter, *group_counter;
	PurpleContact *contact;
	PurpleGroup *group;
	struct _purple_hbuddy hb;
	GHashTable *account_buddies;
	PurpleAccount *account;

	g_return_if_fail(buddy != NULL);

	account = purple_buddy_get_account(buddy);
	node = PURPLE_BLIST_NODE(buddy);
	cnode = node->parent;
	gnode = (cnode != NULL) ? cnode->parent : NULL;
	contact = (PurpleContact *)cnode;
	group = (PurpleGroup *)gnode;

	/* Remove the node from its parent */
	if (node->prev)
		node->prev->next = node->next;
	if (node->next)
		node->next->prev = node->prev;
	if ((cnode != NULL) && (cnode->child == node))
		cnode->child = node->next;

	/* Adjust size counts */
	if (contact != NULL) {
		contact_counter = PURPLE_COUNTING_NODE(contact);
		group_counter = PURPLE_COUNTING_NODE(group);

		if (PURPLE_BUDDY_IS_ONLINE(buddy)) {
			purple_counting_node_change_online_count(contact_counter, -1);
			if (purple_counting_node_get_online_count(contact_counter) == 0)
				purple_counting_node_set_online_count(group_counter, -1);
		}
		if (purple_account_is_connected(account)) {
			purple_counting_node_change_current_size(contact_counter, -1);
			if (purple_counting_node_get_current_size(contact_counter) == 0)
				purple_counting_node_change_current_size(group_counter, -1);
		}
		purple_counting_node_change_total_size(contact_counter, -1);

		/* Re-sort the contact */
		if (cnode->child && purple_contact_get_priority_buddy(contact) == buddy) {
			purple_contact_invalidate_priority_buddy(contact);

			if (ops && ops->update)
				ops->update(purplebuddylist, cnode);
		}
	}

	/* Remove this buddy from the buddies hash table */
	hb.name = (gchar *)purple_normalize(account, purple_buddy_get_name(buddy));
	hb.account = account;
	hb.group = gnode;
	g_hash_table_remove(PURPLE_BUDDY_LIST_GET_PRIVATE(purplebuddylist)->buddies, &hb);

	account_buddies = g_hash_table_lookup(buddies_cache, account);
	g_hash_table_remove(account_buddies, &hb);

	/* Update the UI */
	if (ops && ops->remove)
		ops->remove(purplebuddylist, node);

	if (ops && ops->remove_node)
		ops->remove_node(node);

	/* Remove this buddy's pounces */
	purple_pounce_destroy_all_by_buddy(buddy);

	/* Signal that the buddy has been removed before freeing the memory for it */
	purple_signal_emit(purple_blist_get_handle(), "buddy-removed", buddy);

	purple_signal_emit(purple_blist_get_handle(), "blist-node-removed",
			PURPLE_BLIST_NODE(buddy));

	g_object_unref(buddy);

	/* If the contact is empty then remove it */
	if ((contact != NULL) && !cnode->child)
		purple_blist_remove_contact(contact);
}

void purple_blist_remove_chat(PurpleChat *chat)
{
	PurpleBlistUiOps *ops = purple_blist_get_ui_ops();
	PurpleBlistNode *node, *gnode;
	PurpleGroup *group;
	PurpleCountingNode *group_counter;

	g_return_if_fail(chat != NULL);

	node = (PurpleBlistNode *)chat;
	gnode = node->parent;
	group = (PurpleGroup *)gnode;

	if (gnode != NULL)
	{
		/* Remove the node from its parent */
		if (gnode->child == node)
			gnode->child = node->next;
		if (node->prev)
			node->prev->next = node->next;
		if (node->next)
			node->next->prev = node->prev;

		/* Adjust size counts */
		group_counter = PURPLE_COUNTING_NODE(group);
		if (purple_account_is_connected(purple_chat_get_account(chat))) {
			purple_counting_node_change_online_count(group_counter, -1);
			purple_counting_node_change_current_size(group_counter, -1);
		}
		purple_counting_node_change_total_size(group_counter, -1);
	}

	/* Update the UI */
	if (ops && ops->remove)
		ops->remove(purplebuddylist, node);

	if (ops && ops->remove_node)
		ops->remove_node(node);

	purple_signal_emit(purple_blist_get_handle(), "blist-node-removed",
			PURPLE_BLIST_NODE(chat));

	/* Delete the node */
	g_object_unref(chat);
}

void purple_blist_remove_group(PurpleGroup *group)
{
	PurpleBlistUiOps *ops = purple_blist_get_ui_ops();
	PurpleBlistNode *node;
	GList *l;
	gchar* key;

	g_return_if_fail(group != NULL);

	node = (PurpleBlistNode *)group;

	/* Make sure the group is empty */
	if (node->child)
		return;

	/* Remove the node from its parent */
	if (purplebuddylist->root == node)
		purplebuddylist->root = node->next;
	if (node->prev)
		node->prev->next = node->next;
	if (node->next)
		node->next->prev = node->prev;

	key = g_utf8_collate_key(purple_group_get_name(group), -1);
	g_hash_table_remove(groups_cache, key);
	g_free(key);

	/* Update the UI */
	if (ops && ops->remove)
		ops->remove(purplebuddylist, node);

	if (ops && ops->remove_node)
		ops->remove_node(node);

	purple_signal_emit(purple_blist_get_handle(), "blist-node-removed",
			PURPLE_BLIST_NODE(group));

	/* Remove the group from all accounts that are online */
	for (l = purple_connections_get_all(); l != NULL; l = l->next)
	{
		PurpleConnection *gc = (PurpleConnection *)l->data;

		if (purple_connection_get_state(gc) == PURPLE_CONNECTION_CONNECTED)
			purple_account_remove_group(purple_connection_get_account(gc), group);
	}

	/* Delete the node */
	g_object_unref(group);
}

PurpleBuddy *purple_blist_find_buddy(PurpleAccount *account, const char *name)
{
	PurpleBuddy *buddy;
	struct _purple_hbuddy hb;
	PurpleBlistNode *group;

	g_return_val_if_fail(purplebuddylist != NULL, NULL);
	g_return_val_if_fail(account != NULL, NULL);
	g_return_val_if_fail((name != NULL) && (*name != '\0'), NULL);

	hb.account = account;
	hb.name = (gchar *)purple_normalize(account, name);

	for (group = purplebuddylist->root; group; group = group->next) {
		if (!group->child)
			continue;

		hb.group = group;
		if ((buddy = g_hash_table_lookup(PURPLE_BUDDY_LIST_GET_PRIVATE(purplebuddylist)->buddies,
				&hb))) {
			return buddy;
		}
	}

	return NULL;
}

PurpleBuddy *purple_blist_find_buddy_in_group(PurpleAccount *account, const char *name,
		PurpleGroup *group)
{
	struct _purple_hbuddy hb;

	g_return_val_if_fail(purplebuddylist != NULL, NULL);
	g_return_val_if_fail(account != NULL, NULL);
	g_return_val_if_fail((name != NULL) && (*name != '\0'), NULL);

	hb.name = (gchar *)purple_normalize(account, name);
	hb.account = account;
	hb.group = (PurpleBlistNode*)group;

	return g_hash_table_lookup(PURPLE_BUDDY_LIST_GET_PRIVATE(purplebuddylist)->buddies,
			&hb);
}

static void find_acct_buddies(gpointer key, gpointer value, gpointer data)
{
	PurpleBuddy *buddy = value;
	GSList **list = data;

	*list = g_slist_prepend(*list, buddy);
}

GSList *purple_blist_find_buddies(PurpleAccount *account, const char *name)
{
	PurpleBuddy *buddy;
	PurpleBlistNode *node;
	GSList *ret = NULL;

	g_return_val_if_fail(purplebuddylist != NULL, NULL);
	g_return_val_if_fail(account != NULL, NULL);

	if ((name != NULL) && (*name != '\0')) {
		struct _purple_hbuddy hb;

		hb.name = (gchar *)purple_normalize(account, name);
		hb.account = account;

		for (node = purplebuddylist->root; node != NULL; node = node->next) {
			if (!node->child)
				continue;

			hb.group = node;
			if ((buddy = g_hash_table_lookup(PURPLE_BUDDY_LIST_GET_PRIVATE(purplebuddylist)->buddies,
					&hb)) != NULL)
				ret = g_slist_prepend(ret, buddy);
		}
	} else {
		GSList *list = NULL;
		GHashTable *buddies = g_hash_table_lookup(buddies_cache, account);
		g_hash_table_foreach(buddies, find_acct_buddies, &list);
		ret = list;
	}

	return ret;
}

PurpleGroup *purple_blist_find_group(const char *name)
{
	gchar* key;
	PurpleGroup *group;

	g_return_val_if_fail(purplebuddylist != NULL, NULL);
	g_return_val_if_fail((name != NULL) && (*name != '\0'), NULL);

	key = g_utf8_collate_key(name, -1);
	group = g_hash_table_lookup(groups_cache, key);
	g_free(key);

	return group;
}

PurpleChat *
purple_blist_find_chat(PurpleAccount *account, const char *name)
{
	char *chat_name;
	PurpleChat *chat;
	PurplePlugin *prpl;
	PurplePluginProtocolInfo *prpl_info = NULL;
	struct proto_chat_entry *pce;
	PurpleBlistNode *node, *group;
	GList *parts;
	char *normname;

	g_return_val_if_fail(purplebuddylist != NULL, NULL);
	g_return_val_if_fail((name != NULL) && (*name != '\0'), NULL);

	if (!purple_account_is_connected(account))
		return NULL;

	prpl = purple_find_prpl(purple_account_get_protocol_id(account));
	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);

	if (prpl_info->find_blist_chat != NULL)
		return prpl_info->find_blist_chat(account, name);

	normname = g_strdup(purple_normalize(account, name));
	for (group = purplebuddylist->root; group != NULL; group = group->next) {
		for (node = group->child; node != NULL; node = node->next) {
			if (PURPLE_IS_CHAT(node)) {

				chat = (PurpleChat*)node;

				if (account != purple_chat_get_account(chat))
					continue;

				parts = prpl_info->chat_info(
					purple_account_get_connection(purple_chat_get_account(chat)));

				pce = parts->data;
				chat_name = g_hash_table_lookup(purple_chat_get_components(chat),
												pce->identifier);
				g_list_foreach(parts, (GFunc)g_free, NULL);
				g_list_free(parts);

				if (purple_chat_get_account(chat) == account && chat_name != NULL &&
					normname != NULL && !strcmp(purple_normalize(account, chat_name), normname)) {
					g_free(normname);
					return chat;
				}
			}
		}
	}

	g_free(normname);
	return NULL;
}

void purple_blist_add_account(PurpleAccount *account)
{
	PurpleBlistUiOps *ops = purple_blist_get_ui_ops();
	PurpleBlistNode *gnode, *cnode, *bnode;
	PurpleCountingNode *contact_counter, *group_counter;

	g_return_if_fail(purplebuddylist != NULL);

	if (!ops || !ops->update)
		return;

	for (gnode = purplebuddylist->root; gnode; gnode = gnode->next) {
		if (!PURPLE_IS_GROUP(gnode))
			continue;
		for (cnode = gnode->child; cnode; cnode = cnode->next) {
			if (PURPLE_IS_CONTACT(cnode)) {
				gboolean recompute = FALSE;
					for (bnode = cnode->child; bnode; bnode = bnode->next) {
						if (PURPLE_IS_BUDDY(bnode) &&
								purple_buddy_get_account(PURPLE_BUDDY(bnode)) == account) {
							recompute = TRUE;
							contact_counter = PURPLE_COUNTING_NODE(cnode);
							group_counter = PURPLE_COUNTING_NODE(gnode);
							purple_counting_node_change_current_size(contact_counter, +1);
							if (purple_counting_node_get_current_size(contact_counter) == 1)
								purple_counting_node_change_current_size(group_counter, +1);
							ops->update(purplebuddylist, bnode);
						}
					}
					if (recompute ||
							purple_blist_node_get_bool(cnode, "show_offline")) {
						purple_contact_invalidate_priority_buddy((PurpleContact*)cnode);
						ops->update(purplebuddylist, cnode);
					}
			} else if (PURPLE_IS_CHAT(cnode) &&
					purple_chat_get_account(PURPLE_CHAT(cnode)) == account) {
				group_counter = PURPLE_COUNTING_NODE(gnode);
				purple_counting_node_change_online_count(group_counter, +1);
				purple_counting_node_change_current_size(group_counter, +1);
				ops->update(purplebuddylist, cnode);
			}
		}
		ops->update(purplebuddylist, gnode);
	}
}

void purple_blist_remove_account(PurpleAccount *account)
{
	PurpleBlistUiOps *ops = purple_blist_get_ui_ops();
	PurpleBlistNode *gnode, *cnode, *bnode;
	PurpleCountingNode *contact_counter, *group_counter;
	PurpleBuddy *buddy;
	PurpleChat *chat;
	PurpleContact *contact;
	PurpleGroup *group;
	GList *list = NULL, *iter = NULL;

	g_return_if_fail(purplebuddylist != NULL);

	for (gnode = purplebuddylist->root; gnode; gnode = gnode->next) {
		if (!PURPLE_IS_GROUP(gnode))
			continue;

		group = (PurpleGroup *)gnode;

		for (cnode = gnode->child; cnode; cnode = cnode->next) {
			if (PURPLE_IS_CONTACT(cnode)) {
				gboolean recompute = FALSE;
				contact = (PurpleContact *)cnode;

				for (bnode = cnode->child; bnode; bnode = bnode->next) {
					if (!PURPLE_IS_BUDDY(bnode))
						continue;

					buddy = (PurpleBuddy *)bnode;
					if (account == purple_buddy_get_account(buddy)) {
						PurplePresence *presence;

						presence = purple_buddy_get_presence(buddy);
						contact_counter = PURPLE_COUNTING_NODE(contact);
						group_counter = PURPLE_COUNTING_NODE(group);

						if(purple_presence_is_online(presence)) {
							purple_counting_node_change_online_count(contact_counter, -1);
							if (purple_counting_node_get_online_count(contact_counter) == 0)
								purple_counting_node_change_online_count(group_counter, -1);

							purple_blist_node_set_int(PURPLE_BLIST_NODE(buddy),
													"last_seen", time(NULL));
						}

						purple_counting_node_change_current_size(contact_counter, -1);
						if (purple_counting_node_get_current_size(contact_counter) == 0)
							purple_counting_node_change_current_size(group_counter, -1);

						if (!g_list_find(list, presence))
							list = g_list_prepend(list, presence);

						if (purple_contact_get_priority_buddy(contact) == buddy)
							purple_contact_invalidate_priority_buddy(contact);
						else
							recompute = TRUE;

						if (ops && ops->remove) {
							ops->remove(purplebuddylist, bnode);
						}
					}
				}
				if (recompute) {
					purple_contact_invalidate_priority_buddy(contact);

					if (ops && ops->update)
						ops->update(purplebuddylist, cnode);
				}
			} else if (PURPLE_IS_CHAT(cnode)) {
				chat = PURPLE_CHAT(cnode);

				if(purple_chat_get_account(chat) == account) {
					group_counter = PURPLE_COUNTING_NODE(group);
					purple_counting_node_change_current_size(group_counter, -1);
					purple_counting_node_change_online_count(group_counter, -1);

					if (ops && ops->remove)
						ops->remove(purplebuddylist, cnode);
				}
			}
		}
	}

	for (iter = list; iter; iter = iter->next)
	{
		purple_presence_set_status_active(iter->data, "offline", TRUE);
	}
	g_list_free(list);
}

void
purple_blist_request_add_buddy(PurpleAccount *account, const char *username,
							 const char *group, const char *alias)
{
	PurpleBlistUiOps *ui_ops;

	ui_ops = purple_blist_get_ui_ops();

	if (ui_ops != NULL && ui_ops->request_add_buddy != NULL)
		ui_ops->request_add_buddy(account, username, group, alias);
}

void
purple_blist_request_add_chat(PurpleAccount *account, PurpleGroup *group,
							const char *alias, const char *name)
{
	PurpleBlistUiOps *ui_ops;

	ui_ops = purple_blist_get_ui_ops();

	if (ui_ops != NULL && ui_ops->request_add_chat != NULL)
		ui_ops->request_add_chat(account, group, alias, name);
}

void
purple_blist_request_add_group(void)
{
	PurpleBlistUiOps *ui_ops;

	ui_ops = purple_blist_get_ui_ops();

	if (ui_ops != NULL && ui_ops->request_add_group != NULL)
		ui_ops->request_add_group();
}

void
purple_blist_set_ui_ops(PurpleBlistUiOps *ops)
{
	gboolean overrode = FALSE;
	blist_ui_ops = ops;

	if (!ops)
		return;

	if (!ops->save_node) {
		ops->save_node = purple_blist_save_node;
		overrode = TRUE;
	}
	if (!ops->remove_node) {
		ops->remove_node = purple_blist_save_node;
		overrode = TRUE;
	}
	if (!ops->save_account) {
		ops->save_account = purple_blist_save_account;
		overrode = TRUE;
	}

	if (overrode && (ops->save_node    != purple_blist_save_node ||
	                 ops->remove_node  != purple_blist_save_node ||
	                 ops->save_account != purple_blist_save_account)) {
		purple_debug_warning("blist", "Only some of the blist saving UI ops "
				"were overridden. This probably is not what you want!\n");
	}
}

PurpleBlistUiOps *
purple_blist_get_ui_ops(void)
{
	return blist_ui_ops;
}


void *
purple_blist_get_handle(void)
{
	static int handle;

	return &handle;
}

void
purple_blist_init(void)
{
	void *handle = purple_blist_get_handle();

	purple_signal_register(handle, "buddy-status-changed",
	                     purple_marshal_VOID__POINTER_POINTER_POINTER,
	                     G_TYPE_NONE, 3, PURPLE_TYPE_BUDDY, PURPLE_TYPE_STATUS, 
	                     PURPLE_TYPE_STATUS);
	purple_signal_register(handle, "buddy-privacy-changed",
	                     purple_marshal_VOID__POINTER, G_TYPE_NONE,
	                     1, PURPLE_TYPE_BUDDY);

	purple_signal_register(handle, "buddy-idle-changed",
	                     purple_marshal_VOID__POINTER_INT_INT, G_TYPE_NONE,
	                     3, PURPLE_TYPE_BUDDY, G_TYPE_INT, G_TYPE_INT);

	purple_signal_register(handle, "buddy-signed-on",
						 purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
						 PURPLE_TYPE_BUDDY);

	purple_signal_register(handle, "buddy-signed-off",
						 purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
						 PURPLE_TYPE_BUDDY);

	purple_signal_register(handle, "buddy-got-login-time",
						 purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
						 PURPLE_TYPE_BUDDY);

	purple_signal_register(handle, "blist-node-added",
						 purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
						 PURPLE_TYPE_BLIST_NODE);

	purple_signal_register(handle, "blist-node-removed",
						 purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
						 PURPLE_TYPE_BLIST_NODE);

	purple_signal_register(handle, "buddy-added",
						 purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
						 PURPLE_TYPE_BUDDY);

	purple_signal_register(handle, "buddy-removed",
						 purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
						 PURPLE_TYPE_BUDDY);

	purple_signal_register(handle, "buddy-icon-changed",
						 purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
						 PURPLE_TYPE_BUDDY);

	purple_signal_register(handle, "update-idle", purple_marshal_VOID,
						 G_TYPE_NONE, 0);

	purple_signal_register(handle, "blist-node-extended-menu",
			     purple_marshal_VOID__POINTER_POINTER, G_TYPE_NONE, 2,
			     PURPLE_TYPE_BLIST_NODE,
			     G_TYPE_POINTER); /* (GList **) */

	purple_signal_register(handle, "blist-node-aliased",
						 purple_marshal_VOID__POINTER_POINTER, G_TYPE_NONE, 2,
						 PURPLE_TYPE_BLIST_NODE, G_TYPE_STRING);

	purple_signal_register(handle, "buddy-caps-changed",
			purple_marshal_VOID__POINTER_INT_INT, G_TYPE_NONE,
			3, PURPLE_TYPE_BUDDY, G_TYPE_INT, G_TYPE_INT);

	purple_signal_connect(purple_accounts_get_handle(), "account-created",
			handle,
			PURPLE_CALLBACK(purple_blist_buddies_cache_add_account),
			NULL);

	purple_signal_connect(purple_accounts_get_handle(), "account-destroying",
			handle,
			PURPLE_CALLBACK(purple_blist_buddies_cache_remove_account),
			NULL);
}

static void
blist_node_destroy(PurpleBlistNode *node)
{
	PurpleBlistUiOps *ui_ops;
	PurpleBlistNode *child, *next_child;

	ui_ops = purple_blist_get_ui_ops();
	child = node->child;
	while (child) {
		next_child = child->next;
		blist_node_destroy(child);
		child = next_child;
	}

	/* Allow the UI to free data */
	node->parent = NULL;
	node->child  = NULL;
	node->next   = NULL;
	node->prev   = NULL;
	if (ui_ops && ui_ops->remove)
		ui_ops->remove(purplebuddylist, node);

	g_object_unref(node);
}

void
purple_blist_uninit(void)
{
	PurpleBlistUiOps *ops = purple_blist_get_ui_ops();
	PurpleBlistNode *node, *next_node;

	/* This happens if we quit before purple_set_blist is called. */
	if (purplebuddylist == NULL)
		return;

	if (save_timer != 0) {
		purple_timeout_remove(save_timer);
		save_timer = 0;
		purple_blist_sync();
	}

	purple_debug(PURPLE_DEBUG_INFO, "blist", "Destroying\n");

	if (ops && ops->destroy)
		ops->destroy(purplebuddylist);

	node = purple_blist_get_root();
	while (node) {
		next_node = node->next;
		blist_node_destroy(node);
		node = next_node;
	}
	purplebuddylist->root = NULL;

	g_hash_table_destroy(buddies_cache);
	g_hash_table_destroy(groups_cache);

	buddies_cache = NULL;
	groups_cache = NULL;

	g_object_unref(purplebuddylist);
	purplebuddylist = NULL;

	purple_signals_disconnect_by_handle(purple_blist_get_handle());
	purple_signals_unregister_by_instance(purple_blist_get_handle());
}

/**************************************************************************
 * GObject code
 **************************************************************************/

/* GObject initialization function */
static void
purple_buddy_list_init(GTypeInstance *instance, gpointer klass)
{
	PurpleBuddyList *blist = PURPLE_BUDDY_LIST(instance);

	PURPLE_DBUS_REGISTER_POINTER(blist, PurpleBuddyList);

	PURPLE_BUDDY_LIST_GET_PRIVATE(blist)->buddies = g_hash_table_new_full(
					 (GHashFunc)_purple_blist_hbuddy_hash,
					 (GEqualFunc)_purple_blist_hbuddy_equal,
					 (GDestroyNotify)_purple_blist_hbuddy_free_key, NULL);
}

/* GObject dispose function */
static void
purple_buddy_list_dispose(GObject *object)
{
	PURPLE_DBUS_UNREGISTER_POINTER(object);

	G_OBJECT_CLASS(parent_class)->dispose(object);
}

/* GObject finalize function */
static void
purple_buddy_list_finalize(GObject *object)
{
	g_hash_table_destroy(PURPLE_BUDDY_LIST_GET_PRIVATE(object)->buddies);

	G_OBJECT_CLASS(parent_class)->finalize(object);
}

/* Class initializer function */
static void purple_buddy_list_class_init(PurpleBuddyListClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	obj_class->dispose = purple_buddy_list_dispose;
	obj_class->finalize = purple_buddy_list_finalize;

	g_type_class_add_private(klass, sizeof(PurpleBuddyListPrivate));
}

GType
purple_buddy_list_get_type(void)
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleBuddyListClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_buddy_list_class_init,
			NULL,
			NULL,
			sizeof(PurpleBuddyList),
			0,
			(GInstanceInitFunc)purple_buddy_list_init,
			NULL,
		};

		type = g_type_register_static(G_TYPE_OBJECT,
				"PurpleBuddyList", &info, 0);
	}

	return type;
}
