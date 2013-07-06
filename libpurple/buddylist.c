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
#include "value.h"
#include "xmlnode.h"

static PurpleBListUiOps *blist_ui_ops = NULL;

static PurpleBuddyList *purplebuddylist = NULL;

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

/*********************************************************************
 * Private utility functions                                         *
 *********************************************************************/

static PurpleBListNode *purple_blist_get_last_sibling(PurpleBListNode *node)
{
	PurpleBListNode *n = node;
	if (!n)
		return NULL;
	while (n->next)
		n = n->next;
	return n;
}

static PurpleBListNode *purple_blist_get_last_child(PurpleBListNode *node)
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
	PurpleBListNode *group;
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
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);

	node = xmlnode_new("buddy");
	xmlnode_set_attrib(node, "account", purple_account_get_username(buddy->account));
	xmlnode_set_attrib(node, "proto", purple_account_get_protocol_id(buddy->account));

	child = xmlnode_new_child(node, "name");
	xmlnode_insert_data(child, priv->name, -1);

	if (priv->alias != NULL)
	{
		child = xmlnode_new_child(node, "alias");
		xmlnode_insert_data(child, priv->alias, -1);
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
	PurpleBListNode *bnode;
	PurpleContactPrivate *priv = PURPLE_CONTACT_GET_PRIVATE(contact);

	node = xmlnode_new("contact");

	if (priv->alias != NULL)
	{
		xmlnode_set_attrib(node, "alias", priv->alias);
	}

	/* Write buddies */
	for (bnode = PURPLE_BLIST_NODE(contact)->child; bnode != NULL; bnode = bnode->next)
	{
		if (!PURPLE_BLIST_NODE_SHOULD_SAVE(bnode))
			continue;
		if (PURPLE_IS_BUDDY(bnode))
		{
			child = buddy_to_xmlnode(bnode);
			xmlnode_insert_child(node, child);
		}
	}

	/* Write contact settings */
	g_hash_table_foreach(purple_blist_node_get_settings(PURPLE_BLIST_NODE(contact)),
			value_to_xmlnode, node);

	return node;
}

static xmlnode *
chat_to_xmlnode(PurpleChat *chat)
{
	xmlnode *node, *child;
	PurpleChatPrivate *priv = PURPLE_CHAT_GET_PRIVATE(chat);

	chat = (PurpleChat *)cnode;

	node = xmlnode_new("chat");
	xmlnode_set_attrib(node, "proto", purple_account_get_protocol_id(priv->account));
	xmlnode_set_attrib(node, "account", purple_account_get_username(priv->account));

	if (priv->alias != NULL)
	{
		child = xmlnode_new_child(node, "alias");
		xmlnode_insert_data(child, priv->alias, -1);
	}

	/* Write chat components */
	g_hash_table_foreach(priv->components, chat_component_to_xmlnode, node);

	/* Write chat settings */
	g_hash_table_foreach(purple_blist_node_get_settings(PURPLE_BLIST_NODE(chat)),
			value_to_xmlnode, node);

	return node;
}

static xmlnode *
group_to_xmlnode(PurpleGroup *group)
{
	xmlnode *node, *child;
	PurpleBListNode *cnode;
	PurpleGroupPrivate *priv = PURPLE_GROUP_GET_PRIVATE(group);

	node = xmlnode_new("group");
	xmlnode_set_attrib(node, "name", priv->name);

	/* Write settings */
	g_hash_table_foreach(purple_blist_node_get_settings(PURPLE_BLIST_NODE(group)),
			value_to_xmlnode, node);

	/* Write contacts and chats */
	for (cnode = PURPLE_BLIST_NODE(group)->child; cnode != NULL; cnode = cnode->next)
	{
		if (!PURPLE_BLIST_NODE_SHOULD_SAVE(cnode))
			continue;
		if (PURPLE_IS_CONTACT(cnode))
		{
			child = contact_to_xmlnode(cnode);
			xmlnode_insert_child(node, child);
		}
		else if (PURPLE_IS_CHAT(cnode))
		{
			child = chat_to_xmlnode(cnode);
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
	PurpleBListNode *gnode;
	GList *cur;

	node = xmlnode_new("purple");
	xmlnode_set_attrib(node, "version", "1.0");

	/* Write groups */
	child = xmlnode_new_child(node, "blist");
	for (gnode = purplebuddylist->root; gnode != NULL; gnode = gnode->next)
	{
		if (!PURPLE_BLIST_NODE_SHOULD_SAVE(gnode))
			continue;
		if (PURPLE_IS_GROUP(gnode))
		{
			grandchild = group_to_xmlnode(gnode);
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
purple_blist_save_node(PurpleBListNode *node)
{
	_purple_blist_schedule_save();
}

void purple_blist_schedule_save()
{
	PurpleBListUiOps *ops = purple_blist_get_ui_ops();

	/* Save everything */
	if (ops && ops->save_account)
		ops->save_account(NULL);
}

/*********************************************************************
 * Reading from disk                                                 *
 *********************************************************************/

static void
parse_setting(PurpleBListNode *node, xmlnode *setting)
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
			purple_blist_get_last_child((PurpleBListNode*)contact));

	for (x = xmlnode_get_child(bnode, "setting"); x; x = xmlnode_get_next_twin(x)) {
		parse_setting((PurpleBListNode*)buddy, x);
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
			purple_blist_get_last_child((PurpleBListNode*)group));

	if ((alias = xmlnode_get_attrib(cnode, "alias"))) {
		purple_blist_alias_contact(contact, alias);
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
			purple_blist_get_last_child((PurpleBListNode*)group));

	for (x = xmlnode_get_child(cnode, "setting"); x; x = xmlnode_get_next_twin(x)) {
		parse_setting((PurpleBListNode*)chat, x);
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
			parse_setting((PurpleBListNode*)group, cnode);
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
	PurpleBListUiOps *ui_ops;
	GList *account;
	PurpleBuddyList *gbl = g_object_new(PurpleBuddyList, 1);
	PURPLE_DBUS_REGISTER_POINTER(gbl, PurpleBuddyList);

	ui_ops = purple_blist_get_ui_ops();

	gbl->buddies = g_hash_table_new_full((GHashFunc)_purple_blist_hbuddy_hash,
					 (GEqualFunc)_purple_blist_hbuddy_equal,
					 (GDestroyNotify)_purple_blist_hbuddy_free_key, NULL);

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
purple_get_blist()
{
	return purplebuddylist;
}

PurpleBListNode *
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

	g_hash_table_foreach(purplebuddylist->buddies, append_buddy, &buddies);
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
	PurpleBListUiOps *ops = purple_blist_get_ui_ops();

	if (ops && ops->show)
		ops->show(purplebuddylist);
}

void purple_blist_destroy()
{
	PurpleBListUiOps *ops = purple_blist_get_ui_ops();

	purple_debug(PURPLE_DEBUG_INFO, "blist", "Destroying\n");

	if (ops && ops->destroy)
		ops->destroy(purplebuddylist);
}

void purple_blist_set_visible(gboolean show)
{
	PurpleBListUiOps *ops = purple_blist_get_ui_ops();

	if (ops && ops->set_visible)
		ops->set_visible(purplebuddylist, show);
}

void
purple_blist_update_buddy_status(PurpleBuddy *buddy, PurpleStatus *old_status)
{
	PurpleBListUiOps *ops = purple_blist_get_ui_ops();
	PurplePresence *presence;
	PurpleStatus *status;
	PurpleBListNode *cnode;
	PurpleContact *contact;
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);

	g_return_if_fail(priv != NULL);

	presence = purple_buddy_get_presence(buddy);
	status = purple_presence_get_active_status(presence);

	purple_debug_info("blist", "Updating buddy status for %s (%s)\n",
			priv->name, purple_account_get_protocol_name(priv->account));

	if (purple_status_is_online(status) &&
		!purple_status_is_online(old_status)) {

		purple_signal_emit(purple_blist_get_handle(), "buddy-signed-on", buddy);

		cnode = PURPLE_BLIST_NODE(buddy)->parent;
		contact = PURPLE_CONTACT(cnode);
		purple_contact_set_online_count_relative(contact, +1);
		if (purple_contact_get_online_count(contact) == 1)
			purple_group_set_online_count_relative(PURPLE_GROUP(cnode->parent), +1);
	} else if (!purple_status_is_online(status) &&
				purple_status_is_online(old_status)) {

		purple_blist_node_set_int(PURPLE_BLIST_NODE(buddy), "last_seen", time(NULL));
		purple_signal_emit(purple_blist_get_handle(), "buddy-signed-off", buddy);

		cnode = PURPLE_BLIST_NODE(buddy)->parent;
		contact = PURPLE_CONTACT(cnode);
		purple_contact_set_online_count_relative(contact, -1);
		if (purple_contact_get_online_count(contact) == 0)
			purple_group_set_online_count_relative(PURPLE_GROUP(cnode->parent), -1);
	} else {
		purple_signal_emit(purple_blist_get_handle(),
		                 "buddy-status-changed", buddy, old_status,
		                 status);
	}

	/*
	 * This function used to only call the following two functions if one of
	 * the above signals had been triggered, but that's not good, because
	 * if someone's away message changes and they don't go from away to back
	 * to away then no signal is triggered.
	 *
	 * It's a safe assumption that SOMETHING called this function.  PROBABLY
	 * because something, somewhere changed.  Calling the stuff below
	 * certainly won't hurt anything.  Unless you're on a K6-2 300.
	 */
	purple_contact_invalidate_priority_buddy(purple_buddy_get_contact(buddy));
	if (ops && ops->update)
		ops->update(purplebuddylist, (PurpleBListNode *)buddy);
}

void
purple_blist_update_node_icon(PurpleBListNode *node)
{
	PurpleBListUiOps *ops = purple_blist_get_ui_ops();

	g_return_if_fail(node != NULL);

	if (ops && ops->update)
		ops->update(purplebuddylist, node);
}

/*
 * TODO: Maybe remove the call to this from server.c and call it
 * from oscar.c and toc.c instead?
 */
void purple_blist_rename_buddy(PurpleBuddy *buddy, const char *name)
{
	PurpleBListUiOps *ops = purple_blist_get_ui_ops();
	struct _purple_hbuddy *hb, *hb2;
	GHashTable *account_buddies;
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);

	g_return_if_fail(buddy != NULL);

	hb = g_new(struct _purple_hbuddy, 1);
	hb->name = (gchar *)purple_normalize(priv->account, priv->name);
	hb->account = priv->account;
	hb->group = PURPLE_BLIST_NODE(buddy)->parent->parent;
	g_hash_table_remove(purplebuddylist->buddies, hb);

	account_buddies = g_hash_table_lookup(buddies_cache, priv->account);
	g_hash_table_remove(account_buddies, hb);

	hb->name = g_strdup(purple_normalize(priv->account, name));
	g_hash_table_replace(purplebuddylist->buddies, hb, buddy);

	hb2 = g_new(struct _purple_hbuddy, 1);
	hb2->name = g_strdup(hb->name);
	hb2->account = priv->account;
	hb2->group = PURPLE_BLIST_NODE(buddy)->parent->parent;

	g_hash_table_replace(account_buddies, hb2, buddy);

	g_free(priv->name);
	priv->name = g_strdup(name);

	if (ops && ops->save_node)
		ops->save_node(PURPLE_BLIST_NODE(buddy));

	if (ops && ops->update)
		ops->update(purplebuddylist, PURPLE_BLIST_NODE(buddy));
}

static gboolean
purple_strings_are_different(const char *one, const char *two)
{
	return !((one && two && g_utf8_collate(one, two) == 0) ||
			((one == NULL || *one == '\0') && (two == NULL || *two == '\0')));
}

void purple_blist_alias_contact(PurpleContact *contact, const char *alias)
{
	PurpleBListUiOps *ops = purple_blist_get_ui_ops();
	PurpleIMConversation *im;
	PurpleBListNode *bnode;
	char *old_alias;
	char *new_alias = NULL;
	PurpleContactPrivate *priv = PURPLE_CONTACT_GET_PRIVATE(contact);

	g_return_if_fail(contact != NULL);

	if ((alias != NULL) && (*alias != '\0'))
		new_alias = purple_utf8_strip_unprintables(alias);

	if (!purple_strings_are_different(priv->alias, new_alias)) {
		g_free(new_alias);
		return;
	}

	old_alias = priv->alias;

	if ((new_alias != NULL) && (*new_alias != '\0'))
		priv->alias = new_alias;
	else {
		priv->alias = NULL;
		g_free(new_alias); /* could be "\0" */
	}

	if (ops && ops->save_node)
		ops->save_node(PURPLE_BLIST_NODE(contact));

	if (ops && ops->update)
		ops->update(purplebuddylist, PURPLE_BLIST_NODE(contact));

	for(bnode = PURPLE_BLIST_NODE(contact)->child; bnode != NULL; bnode = bnode->next)
	{
		PurpleBuddy *buddy = PURPLE_BUDDY(bnode);

		im = purple_conversations_find_im_with_account(purple_buddy_get_name(buddy),
												   purple_buddy_get_account(buddy));
		if (im)
			purple_conversation_autoset_title(PURPLE_CONVERSATION(im));
	}

	purple_signal_emit(purple_blist_get_handle(), "blist-node-aliased",
					 contact, old_alias);
	g_free(old_alias);
}

void purple_blist_alias_chat(PurpleChat *chat, const char *alias)
{
	PurpleBListUiOps *ops = purple_blist_get_ui_ops();
	char *old_alias;
	char *new_alias = NULL;
	PurpleChatPrivate *priv = PURPLE_CHAT_GET_PRIVATE(chat);

	g_return_if_fail(priv != NULL);

	if ((alias != NULL) && (*alias != '\0'))
		new_alias = purple_utf8_strip_unprintables(alias);

	if (!purple_strings_are_different(priv->alias, new_alias)) {
		g_free(new_alias);
		return;
	}

	old_alias = priv->alias;

	if ((new_alias != NULL) && (*new_alias != '\0'))
		priv->alias = new_alias;
	else {
		priv->alias = NULL;
		g_free(new_alias); /* could be "\0" */
	}

	if (ops && ops->save_node)
		ops->save_node(PURPLE_BLIST_NODE(chat));

	if (ops && ops->update)
		ops->update(purplebuddylist, PURPLE_BLIST_NODE(chat));

	purple_signal_emit(purple_blist_get_handle(), "blist-node-aliased",
					 chat, old_alias);
	g_free(old_alias);
}

void purple_blist_alias_buddy(PurpleBuddy *buddy, const char *alias)
{
	PurpleBListUiOps *ops = purple_blist_get_ui_ops();
	PurpleIMConversation *im;
	char *old_alias;
	char *new_alias = NULL;
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);

	g_return_if_fail(buddy != NULL);

	if ((alias != NULL) && (*alias != '\0'))
		new_alias = purple_utf8_strip_unprintables(alias);

	if (!purple_strings_are_different(priv->alias, new_alias)) {
		g_free(new_alias);
		return;
	}

	old_alias = priv->alias;

	if ((new_alias != NULL) && (*new_alias != '\0'))
		priv->alias = new_alias;
	else {
		priv->alias = NULL;
		g_free(new_alias); /* could be "\0" */
	}

	if (ops && ops->save_node)
		ops->save_node(PURPLE_BLIST_NODE(buddy));

	if (ops && ops->update)
		ops->update(purplebuddylist, PURPLE_BLIST_NODE(buddy));

	im = purple_conversations_find_im_with_account(priv->name,
											   priv->account);
	if (im)
		purple_conversation_autoset_title(PURPLE_CONVERSATION(im));

	purple_signal_emit(purple_blist_get_handle(), "blist-node-aliased",
					 buddy, old_alias);
	g_free(old_alias);
}

void purple_blist_server_alias_buddy(PurpleBuddy *buddy, const char *alias)
{
	PurpleBListUiOps *ops = purple_blist_get_ui_ops();
	PurpleIMConversation *im;
	char *old_alias;
	char *new_alias = NULL;
	PurpleBuddyPrivate *priv = PURPLE_BUDDY_GET_PRIVATE(buddy);

	g_return_if_fail(priv != NULL);

	if ((alias != NULL) && (*alias != '\0') && g_utf8_validate(alias, -1, NULL))
		new_alias = purple_utf8_strip_unprintables(alias);

	if (!purple_strings_are_different(priv->server_alias, new_alias)) {
		g_free(new_alias);
		return;
	}

	old_alias = priv->server_alias;

	if ((new_alias != NULL) && (*new_alias != '\0'))
		priv->server_alias = new_alias;
	else {
		priv->server_alias = NULL;
		g_free(new_alias); /* could be "\0"; */
	}

	if (ops && ops->save_node)
		ops->save_node(PURPLE_BLIST_NODE(buddy));

	if (ops && ops->update)
		ops->update(purplebuddylist, PURPLE_BLIST_NODE(buddy));

	im = purple_conversations_find_im_with_account(priv->name,
											   priv->account);
	if (im)
		purple_conversation_autoset_title(PURPLE_CONVERSATION(im));

	purple_signal_emit(purple_blist_get_handle(), "blist-node-aliased",
					 buddy, old_alias);
	g_free(old_alias);
}

/*
 * TODO: If merging, prompt the user if they want to merge.
 */
void purple_blist_rename_group(PurpleGroup *source, const char *name)
{
	PurpleBListUiOps *ops = purple_blist_get_ui_ops();
	PurpleGroup *dest;
	gchar *old_name;
	gchar *new_name;
	GList *moved_buddies = NULL;
	GSList *accts;
	PurpleGroupPrivate *priv = PURPLE_GROUP_GET_PRIVATE(source);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(name != NULL);

	new_name = purple_utf8_strip_unprintables(name);

	if (*new_name == '\0' || purple_strequal(new_name, priv->name)) {
		g_free(new_name);
		return;
	}

	dest = purple_find_group(new_name);
	if (dest != NULL && purple_utf8_strcasecmp(priv->name,
				PURPLE_GROUP_GET_PRIVATE(dest)->name) != 0) {
		/* We're merging two groups */
		PurpleBListNode *prev, *child, *next;

		prev = purple_blist_get_last_child((PurpleBListNode*)dest);
		child = PURPLE_BLIST_NODE(source)->child;

		/*
		 * TODO: This seems like a dumb way to do this... why not just
		 * append all children from the old group to the end of the new
		 * one?  PRPLs might be expecting to receive an add_buddy() for
		 * each moved buddy...
		 */
		while (child)
		{
			next = child->next;
			if (PURPLE_IS_CONTACT(child)) {
				PurpleBListNode *bnode;
				purple_blist_add_contact((PurpleContact *)child, dest, prev);
				for (bnode = child->child; bnode != NULL; bnode = bnode->next) {
					purple_blist_add_buddy((PurpleBuddy *)bnode, (PurpleContact *)child,
							NULL, bnode->prev);
					moved_buddies = g_list_append(moved_buddies, bnode);
				}
				prev = child;
			} else if (PURPLE_IS_CHAT(child)) {
				purple_blist_add_chat((PurpleChat *)child, dest, prev);
				prev = child;
			} else {
				purple_debug(PURPLE_DEBUG_ERROR, "blist",
						"Unknown child type in group %s\n", priv->name);
			}
			child = next;
		}

		/* Make a copy of the old group name and then delete the old group */
		old_name = g_strdup(priv->name);
		purple_blist_remove_group(source);
		source = dest;
		g_free(new_name);
	} else {
		/* A simple rename */
		PurpleBListNode *cnode, *bnode;
		gchar* key;

		/* Build a GList of all buddies in this group */
		for (cnode = PURPLE_BLIST_NODE(source)->child; cnode != NULL; cnode = cnode->next) {
			if (PURPLE_IS_CONTACT(cnode))
				for (bnode = cnode->child; bnode != NULL; bnode = bnode->next)
					moved_buddies = g_list_append(moved_buddies, bnode);
		}

		old_name = priv->name;
		priv->name = new_name;

		key = g_utf8_collate_key(old_name, -1);
		g_hash_table_remove(groups_cache, key);
		g_free(key);

		key = g_utf8_collate_key(new_name, -1);
		g_hash_table_insert(groups_cache, key, source);
	}

	/* Save our changes */
	if (ops && ops->save_node)
		ops->save_node(PURPLE_BLIST_NODE(source));

	/* Update the UI */
	if (ops && ops->update)
		ops->update(purplebuddylist, PURPLE_BLIST_NODE(source));

	/* Notify all PRPLs */
	/* TODO: Is this condition needed?  Seems like it would always be TRUE */
	if(old_name && !purple_strequal(priv->name, old_name)) {
		for (accts = purple_group_get_accounts(source); accts; accts = g_slist_remove(accts, accts->data)) {
			PurpleAccount *account = accts->data;
			PurpleConnection *gc = NULL;
			PurplePlugin *prpl = NULL;
			PurplePluginProtocolInfo *prpl_info = NULL;
			GList *l = NULL, *buddies = NULL;

			gc = purple_account_get_connection(account);

			if(gc)
				prpl = purple_connection_get_prpl(gc);

			if(gc && prpl)
				prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);

			if(!prpl_info)
				continue;

			for(l = moved_buddies; l; l = l->next) {
				PurpleBuddy *buddy = PURPLE_BUDDY(l->data);

				if(buddy && purple_buddy_get_account(buddy) == account)
					buddies = g_list_append(buddies, (PurpleBListNode *)buddy);
			}

			if(prpl_info->rename_group) {
				prpl_info->rename_group(gc, old_name, source, buddies);
			} else {
				GList *cur, *groups = NULL;

				/* Make a list of what the groups each buddy is in */
				for(cur = buddies; cur; cur = cur->next) {
					PurpleBListNode *node = (PurpleBListNode *)cur->data;
					groups = g_list_prepend(groups, node->parent->parent);
				}

				purple_account_remove_buddies(account, buddies, groups);
				g_list_free(groups);
				purple_account_add_buddies(account, buddies, NULL);
			}

			g_list_free(buddies);
		}
	}
	g_list_free(moved_buddies);
	g_free(old_name);
}

void purple_blist_add_chat(PurpleChat *chat, PurpleGroup *group, PurpleBListNode *node)
{
	PurpleBListNode *cnode = (PurpleBListNode*)chat;
	PurpleBListUiOps *ops = purple_blist_get_ui_ops();

	g_return_if_fail(chat != NULL);
	g_return_if_fail(PURPLE_IS_CHAT((PurpleBListNode *)chat));

	if (node == NULL) {
		if (group == NULL)
			group = purple_group_new(_("Chats"));

		/* Add group to blist if isn't already on it. Fixes #2752. */
		if (!purple_find_group(purple_group_get_name(group))) {
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
		purple_group_set_total_size_relative(PURPLE_GROUP(cnode->parent), -1);
		if (purple_account_is_connected(purple_chat_get_account(chat))) {
			purple_group_set_online_count_relative(PURPLE_GROUP(cnode->parent), -1);
			purple_group_set_current_size_relative(PURPLE_GRPUP(cnode->parent), -1);
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
		purple_group_set_total_size_relative(PURPLE_GROUP(node->parent), +1);
		if (purple_account_is_connected(purple_chat_get_account(chat))) {
			purple_group_set_online_count_relative(PURPLE_GROUP(node->parent), +1);
			purple_group_set_current_size_relative(PURPLE_GROUP(node->parent), +1);
		}
	} else {
		if (((PurpleBListNode *)group)->child)
			((PurpleBListNode *)group)->child->prev = cnode;
		cnode->next = ((PurpleBListNode *)group)->child;
		cnode->prev = NULL;
		((PurpleBListNode *)group)->child = cnode;
		cnode->parent = (PurpleBListNode *)group;
		purple_group_set_total_size_relative(group, +1);
		if (purple_account_is_connected(purple_chat_get_account(chat))) {
			purple_group_set_online_count_relative(group, +1);
			purple_group_set_current_size_relative(group, +1);
		}
	}

	if (ops && ops->save_node)
		ops->save_node(cnode);

	if (ops && ops->update)
		ops->update(purplebuddylist, (PurpleBListNode *)cnode);

	purple_signal_emit(purple_blist_get_handle(), "blist-node-added",
			cnode);
}

void purple_blist_add_buddy(PurpleBuddy *buddy, PurpleContact *contact, PurpleGroup *group, PurpleBListNode *node)
{
	PurpleBListNode *cnode, *bnode;
	PurpleGroup *g;
	PurpleContact *c;
	PurpleAccount *account;
	PurpleBListUiOps *ops = purple_blist_get_ui_ops();
	struct _purple_hbuddy *hb, *hb2;
	GHashTable *account_buddies;

	g_return_if_fail(buddy != NULL);
	g_return_if_fail(PURPLE_IS_BUDDY((PurpleBListNode*)buddy));

	bnode = PURPLE_BLIST_NODE(buddy);
	account = purple_buddy_get_account(buddy);

	/* if we're moving to overtop of ourselves, do nothing */
	if (bnode == node || (!node && bnode->parent &&
				contact && bnode->parent == (PurpleBListNode*)contact
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
		if (!purple_find_group(purple_group_get_name(g))) {
			purple_blist_add_group(g,
					purple_blist_get_last_sibling(purplebuddylist->root));
		}
		c = purple_contact_new();
		purple_blist_add_contact(c, g,
				purple_blist_get_last_child((PurpleBListNode*)g));
	}

	cnode = PURPLE_BLIST_NODE(c);

	if (bnode->parent) {
		if (PURPLE_IS_BUDDY_ONLINE(buddy)) {
			purple_contact_set_online_count_relative(PURPLE_CONTACT(bnode->parent), -1);
			if (purple_contact_get_online_count(PURPLE_CONTACT(bnode->parent)) == 0)
				purple_group_set_online_count_relative(PURPLE_GROUP(bnode->parent->parent), -1);
		}
		if (purple_account_is_connected(account)) {
			purple_contact_set_current_size_relative(PURPLE_CONTACT(bnode->parent), -1);
			if (purple_contact_get_current_size(PURPLE_CONTACT(bnode->parent)) == 0)
				purple_group_set_current_size_relative(PURPLE_GROUP(bnode->parent->parent), -1);
		}
		purple_contact_set_total_size_relative(PURPLE_CONTACT(bnode->parent), -1);
		/* the group totalsize will be taken care of by remove_contact below */

		if (bnode->parent->parent != (PurpleBListNode*)g)
			serv_move_buddy(buddy, (PurpleGroup *)bnode->parent->parent, g);

		if (bnode->next)
			bnode->next->prev = bnode->prev;
		if (bnode->prev)
			bnode->prev->next = bnode->next;
		if (bnode->parent->child == bnode)
			bnode->parent->child = bnode->next;

		if (ops && ops->remove)
			ops->remove(purplebuddylist, bnode);

		if (bnode->parent->parent != (PurpleBListNode*)g) {
			struct _purple_hbuddy hb;
			hb.name = (gchar *)purple_normalize(account,
					purple_buddy_get_name(buddy));
			hb.account = account;
			hb.group = bnode->parent->parent;
			g_hash_table_remove(purplebuddylist->buddies, &hb);

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

	if (PURPLE_IS_BUDDY_ONLINE(buddy)) {
		purple_contact_set_online_count_relative(PURPLE_CONTACT(bnode->parent), +1);
		if (purple_contact_get_online_count(PURPLE_CONTACT(bnode->parent)) == 1)
			purple_group_set_online_count_relative(PURPLE_GROUP(bnode->parent->parent), +1);
	}
	if (purple_account_is_connected(account)) {
		purple_contact_set_current_size_relative(PURPLE_CONTACT(bnode->parent), +1);
		if (purple_contact_get_online_count(PURPLE_CONTACT(bnode->parent)) == 1)
			purple_group_set_current_size_relative(PURPLE_GROUP(bnode->parent->parent), +1);
	}
	purple_contact_set_total_size_relative(PURPLE_CONTACT(bnode->parent), +1);

	hb = g_new(struct _purple_hbuddy, 1);
	hb->name = g_strdup(purple_normalize(account, purple_buddy_get_name(buddy)));
	hb->account = account;
	hb->group = PURPLE_BLIST_NODE(buddy)->parent->parent;

	g_hash_table_replace(purplebuddylist->buddies, hb, buddy);

	account_buddies = g_hash_table_lookup(buddies_cache, account);

	hb2 = g_new(struct _purple_hbuddy, 1);
	hb2->name = g_strdup(hb->name);
	hb2->account = account;
	hb2->group = ((PurpleBListNode*)buddy)->parent->parent;

	g_hash_table_replace(account_buddies, hb2, buddy);

	purple_contact_invalidate_priority_buddy(purple_buddy_get_contact(buddy));

	if (ops && ops->save_node)
		ops->save_node((PurpleBListNode*) buddy);

	if (ops && ops->update)
		ops->update(purplebuddylist, (PurpleBListNode*)buddy);

	/* Signal that the buddy has been added */
	purple_signal_emit(purple_blist_get_handle(), "buddy-added", buddy);

	purple_signal_emit(purple_blist_get_handle(), "blist-node-added",
			PURPLE_BLIST_NODE(buddy));
}

void purple_blist_add_contact(PurpleContact *contact, PurpleGroup *group, PurpleBListNode *node)
{
	PurpleBListUiOps *ops = purple_blist_get_ui_ops();
	PurpleGroup *g;
	PurpleBListNode *gnode, *cnode, *bnode;

	g_return_if_fail(contact != NULL);

	if (PURPLE_BLIST_NODE(contact) == node)
		return;

	if (node && (PURPLE_IS_CONTACT(node) ||
				PURPLE_IS_CHAT(node)))
		g = PURPLE_GROUP(node->parent);
	else if (group)
		g = group;
	else {
		g = purple_find_group(_("Buddies"));
		if (g == NULL) {
			g = purple_group_new(_("Buddies"));
			purple_blist_add_group(g,
					purple_blist_get_last_sibling(purplebuddylist->root));
		}
	}

	gnode = (PurpleBListNode*)g;
	cnode = (PurpleBListNode*)contact;

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
				PurpleBListNode *next_bnode = bnode->next;
				PurpleBuddy *b = PURPLE_BUDDY(bnode);
				PurpleAccount *account = purple_buddy_get_account(b);
				GHashTable *account_buddies;

				struct _purple_hbuddy *hb, *hb2;

				hb = g_new(struct _purple_hbuddy, 1);
				hb->name = g_strdup(purple_normalize(account, purple_buddy_get_name(b)));
				hb->account = account;
				hb->group = cnode->parent;

				g_hash_table_remove(purplebuddylist->buddies, hb);

				account_buddies = g_hash_table_lookup(buddies_cache, account);
				g_hash_table_remove(account_buddies, hb);

				if (!purple_find_buddy_in_group(account, purple_buddy_get_name(b), g)) {
					hb->group = gnode;
					g_hash_table_replace(purplebuddylist->buddies, hb, b);

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

		if (purple_contact_get_online_count(contact) > 0)
			purple_group_set_online_count_relative(PURPLE_GROUP(cnode->parent), -1);
		if (purple_contact_get_current_size(contact) > 0)
			purple_group_set_current_size_relative(PURPLE_GROUP(cnode->parent), -1);
		purple_group_set_total_size_relative(PURPLE_GROUP(cnode->parent), -1);

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

	if (purple_contact_get_online_count(contact) > 0)
		purple_group_set_online_count_relative(g, +1);
	if (purple_contact_get_current_size(contact) > 0)
		purple_group_set_current_size_relative(g, +1);
	purple_group_set_total_size_relative(g, +1);

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

void purple_blist_merge_contact(PurpleContact *source, PurpleBListNode *node)
{
	PurpleBListNode *sourcenode = (PurpleBListNode*)source;
	PurpleBListNode *prev, *cur, *next;
	PurpleContact *target;

	g_return_if_fail(source != NULL);
	g_return_if_fail(node != NULL);

	if (PURPLE_IS_CONTACT(node)) {
		target = (PurpleContact *)node;
		prev = purple_blist_get_last_child(node);
	} else if (PURPLE_IS_BUDDY(node)) {
		target = (PurpleContact *)node->parent;
		prev = node;
	} else {
		return;
	}

	if (source == target || !target)
		return;

	next = sourcenode->child;

	while (next) {
		cur = next;
		next = cur->next;
		if (PURPLE_IS_BUDDY(cur)) {
			purple_blist_add_buddy((PurpleBuddy *)cur, target, NULL, prev);
			prev = cur;
		}
	}
}

void purple_blist_add_group(PurpleGroup *group, PurpleBListNode *node)
{
	PurpleBListUiOps *ops;
	PurpleBListNode *gnode = (PurpleBListNode*)group;
	gchar* key;

	g_return_if_fail(group != NULL);
	g_return_if_fail(PURPLE_IS_GROUP((PurpleBListNode *)group));

	ops = purple_blist_get_ui_ops();

	/* if we're moving to overtop of ourselves, do nothing */
	if (gnode == node) {
		if (!purplebuddylist->root)
			node = NULL;
		else
			return;
	}

	if (purple_find_group(purple_group_get_name(group))) {
		/* This is just being moved */

		if (ops && ops->remove)
			ops->remove(purplebuddylist, (PurpleBListNode *)group);

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
	PurpleBListUiOps *ops = purple_blist_get_ui_ops();
	PurpleBListNode *node, *gnode;
	PurpleGroup *group;

	g_return_if_fail(contact != NULL);

	node = (PurpleBListNode *)contact;
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
		purple_group_set_total_size_relative(group, -1);

		/* Update the UI */
		if (ops && ops->remove)
			ops->remove(purplebuddylist, node);

		if (ops && ops->remove_node)
			ops->remove_node(node);

		purple_signal_emit(purple_blist_get_handle(), "blist-node-removed",
				PURPLE_BLIST_NODE(contact));

		/* Delete the node */
		purple_contact_destroy(contact);
	}
}

void purple_blist_remove_buddy(PurpleBuddy *buddy)
{
	PurpleBListUiOps *ops = purple_blist_get_ui_ops();
	PurpleBListNode *node, *cnode, *gnode;
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
		if (PURPLE_IS_BUDDY_ONLINE(buddy)) {
			purple_contact_set_online_count_relative(contact, -1);
			if (purple_contact_get_online_count(contact) == 0)
				purple_group_set_online_count(group, -1);
		}
		if (purple_account_is_connected(account)) {
			purple_contact_set_current_size_relative(contact, -1);
			if (purple_contact_get_current_size(contact) == 0)
				purple_group_set_current_size_relative(group, -1);
		}
		purple_contact_set_total_size_relative(contact, -1);

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
	g_hash_table_remove(purplebuddylist->buddies, &hb);

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

	purple_buddy_destroy(buddy);

	/* If the contact is empty then remove it */
	if ((contact != NULL) && !cnode->child)
		purple_blist_remove_contact(contact);
}

void purple_blist_remove_chat(PurpleChat *chat)
{
	PurpleBListUiOps *ops = purple_blist_get_ui_ops();
	PurpleBListNode *node, *gnode;
	PurpleGroup *group;

	g_return_if_fail(chat != NULL);

	node = (PurpleBListNode *)chat;
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
		if (purple_account_is_connected(purple_chat_get_account(chat))) {
			purple_group_set_online_count_relative(group, -1);
			purple_group_set_current_size_relative(group, -1);
		}
		purple_group_set_total_size_relative(group, -1);
	}

	/* Update the UI */
	if (ops && ops->remove)
		ops->remove(purplebuddylist, node);

	if (ops && ops->remove_node)
		ops->remove_node(node);

	purple_signal_emit(purple_blist_get_handle(), "blist-node-removed",
			PURPLE_BLIST_NODE(chat));

	/* Delete the node */
	purple_chat_destroy(chat);
}

void purple_blist_remove_group(PurpleGroup *group)
{
	PurpleBListUiOps *ops = purple_blist_get_ui_ops();
	PurpleBListNode *node;
	GList *l;
	gchar* key;

	g_return_if_fail(group != NULL);

	node = (PurpleBListNode *)group;

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

		if (purple_connection_get_state(gc) == PURPLE_CONNECTED)
			purple_account_remove_group(purple_connection_get_account(gc), group);
	}

	/* Delete the node */
	purple_group_destroy(group);
}

PurpleBuddy *purple_find_buddy(PurpleAccount *account, const char *name)
{
	PurpleBuddy *buddy;
	struct _purple_hbuddy hb;
	PurpleBListNode *group;

	g_return_val_if_fail(purplebuddylist != NULL, NULL);
	g_return_val_if_fail(account != NULL, NULL);
	g_return_val_if_fail((name != NULL) && (*name != '\0'), NULL);

	hb.account = account;
	hb.name = (gchar *)purple_normalize(account, name);

	for (group = purplebuddylist->root; group; group = group->next) {
		if (!group->child)
			continue;

		hb.group = group;
		if ((buddy = g_hash_table_lookup(purplebuddylist->buddies, &hb))) {
			return buddy;
		}
	}

	return NULL;
}

PurpleBuddy *purple_find_buddy_in_group(PurpleAccount *account, const char *name,
		PurpleGroup *group)
{
	struct _purple_hbuddy hb;

	g_return_val_if_fail(purplebuddylist != NULL, NULL);
	g_return_val_if_fail(account != NULL, NULL);
	g_return_val_if_fail((name != NULL) && (*name != '\0'), NULL);

	hb.name = (gchar *)purple_normalize(account, name);
	hb.account = account;
	hb.group = (PurpleBListNode*)group;

	return g_hash_table_lookup(purplebuddylist->buddies, &hb);
}

static void find_acct_buddies(gpointer key, gpointer value, gpointer data)
{
	PurpleBuddy *buddy = value;
	GSList **list = data;

	*list = g_slist_prepend(*list, buddy);
}

GSList *purple_find_buddies(PurpleAccount *account, const char *name)
{
	PurpleBuddy *buddy;
	PurpleBListNode *node;
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
			if ((buddy = g_hash_table_lookup(purplebuddylist->buddies, &hb)) != NULL)
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

PurpleGroup *purple_find_group(const char *name)
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
	PurpleBListNode *node, *group;
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
	PurpleBListUiOps *ops = purple_blist_get_ui_ops();
	PurpleBListNode *gnode, *cnode, *bnode;

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
							purple_contact_set_current_size_relative(PURPLE_CONTACT(cnode), +1);
							if (purple_contact_get_current_size(PURPLE_CONTACT(cnode)) == 1)
								purple_group_set_current_size_relative(PURPLE_GROUP(gnode), +1);
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
				purple_group_set_online_count_relative(PURPLE_GROUP(gnode), +1);
				purple_group_set_current_size_relative(PURPLE_GROUP(gnode), +1);
				ops->update(purplebuddylist, cnode);
			}
		}
		ops->update(purplebuddylist, gnode);
	}
}

void purple_blist_remove_account(PurpleAccount *account)
{
	PurpleBListUiOps *ops = purple_blist_get_ui_ops();
	PurpleBListNode *gnode, *cnode, *bnode;
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

						if(purple_presence_is_online(presence)) {
							purple_contact_set_online_count_relative(contact, -1);
							if (purple_contact_get_online_count(contact) == 0)
								purple_group_set_online_count_relative(group, -1);

							purple_blist_node_set_int(PURPLE_BLIST_NODE(buddy),
													"last_seen", time(NULL));
						}

						purple_contact_set_current_size_relative(contact, -1);
						if (purple_contact_get_current_size(contact) == 0)
							purple_group_set_current_size_relative(group, -1);

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
					purple_group_set_current_size_relative(group, -1);
					purple_group_set_online_count_relative(group, -1);

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
	PurpleBListUiOps *ui_ops;

	ui_ops = purple_blist_get_ui_ops();

	if (ui_ops != NULL && ui_ops->request_add_buddy != NULL)
		ui_ops->request_add_buddy(account, username, group, alias);
}

void
purple_blist_request_add_chat(PurpleAccount *account, PurpleGroup *group,
							const char *alias, const char *name)
{
	PurpleBListUiOps *ui_ops;

	ui_ops = purple_blist_get_ui_ops();

	if (ui_ops != NULL && ui_ops->request_add_chat != NULL)
		ui_ops->request_add_chat(account, group, alias, name);
}

void
purple_blist_request_add_group(void)
{
	PurpleBListUiOps *ui_ops;

	ui_ops = purple_blist_get_ui_ops();

	if (ui_ops != NULL && ui_ops->request_add_group != NULL)
		ui_ops->request_add_group();
}

/* TODO move to group */
int purple_blist_get_group_size(PurpleGroup *group, gboolean offline)
{
	if (!group)
		return 0;

	return offline ? purple_group_get_total_size(group) : purple_group_get_current_size(group);
}

/* TODO remove, move to group */
int purple_blist_get_group_online_count(PurpleGroup *group)
{
	if (!group)
		return 0;

	return purple_group_get_online_count(group);
}

void
purple_blist_set_ui_ops(PurpleBListUiOps *ops)
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

PurpleBListUiOps *
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
	                     purple_marshal_VOID__POINTER_POINTER_POINTER, NULL,
	                     3,
	                     purple_value_new(PURPLE_TYPE_SUBTYPE,
	                                    PURPLE_SUBTYPE_BLIST_BUDDY),
	                     purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_STATUS),
	                     purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_STATUS));
	purple_signal_register(handle, "buddy-privacy-changed",
	                     purple_marshal_VOID__POINTER, NULL,
	                     1,
	                     purple_value_new(PURPLE_TYPE_SUBTYPE,
	                                    PURPLE_SUBTYPE_BLIST_BUDDY));

	purple_signal_register(handle, "buddy-idle-changed",
	                     purple_marshal_VOID__POINTER_INT_INT, NULL,
	                     3,
	                     purple_value_new(PURPLE_TYPE_SUBTYPE,
	                                    PURPLE_SUBTYPE_BLIST_BUDDY),
	                     purple_value_new(PURPLE_TYPE_INT),
	                     purple_value_new(PURPLE_TYPE_INT));


	purple_signal_register(handle, "buddy-signed-on",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_BLIST_BUDDY));

	purple_signal_register(handle, "buddy-signed-off",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_BLIST_BUDDY));

	purple_signal_register(handle, "buddy-got-login-time",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_BLIST_BUDDY));

	purple_signal_register(handle, "blist-node-added",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_BLIST_NODE));

	purple_signal_register(handle, "blist-node-removed",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_BLIST_NODE));

	purple_signal_register(handle, "buddy-added",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_BLIST_BUDDY));

	purple_signal_register(handle, "buddy-removed",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_BLIST_BUDDY));

	purple_signal_register(handle, "buddy-icon-changed",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_BLIST_BUDDY));

	purple_signal_register(handle, "update-idle", purple_marshal_VOID, NULL, 0);

	purple_signal_register(handle, "blist-node-extended-menu",
			     purple_marshal_VOID__POINTER_POINTER, NULL, 2,
			     purple_value_new(PURPLE_TYPE_SUBTYPE,
					    PURPLE_SUBTYPE_BLIST_NODE),
			     purple_value_new(PURPLE_TYPE_BOXED, "GList **"));

	purple_signal_register(handle, "blist-node-aliased",
						 purple_marshal_VOID__POINTER_POINTER, NULL, 2,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_BLIST_NODE),
						 purple_value_new(PURPLE_TYPE_STRING));

	purple_signal_register(handle, "buddy-caps-changed",
			purple_marshal_VOID__POINTER_INT_INT, NULL,
			3, purple_value_new(PURPLE_TYPE_SUBTYPE,
				PURPLE_SUBTYPE_BLIST_BUDDY),
			purple_value_new(PURPLE_TYPE_INT),
			purple_value_new(PURPLE_TYPE_INT));

	purple_signal_connect(purple_accounts_get_handle(), "account-created",
			handle,
			PURPLE_CALLBACK(purple_blist_buddies_cache_add_account),
			NULL);

	purple_signal_connect(purple_accounts_get_handle(), "account-destroying",
			handle,
			PURPLE_CALLBACK(purple_blist_buddies_cache_remove_account),
			NULL);
}

void
purple_blist_uninit(void)
{
	PurpleBListNode *node, *next_node;

	/* This happens if we quit before purple_set_blist is called. */
	if (purplebuddylist == NULL)
		return;

	if (save_timer != 0) {
		purple_timeout_remove(save_timer);
		save_timer = 0;
		purple_blist_sync();
	}

	purple_blist_destroy();

	node = purple_blist_get_root();
	while (node) {
		next_node = node->next;
		purple_blist_node_destroy(node); /* TODO replace by a loop, see original blist_destroy */
		node = next_node;
	}
	purplebuddylist->root = NULL;

	g_hash_table_destroy(PURPLE_BUDDY_LIST_GET_PRIVAT(purplebuddylist)->buddies);
	g_hash_table_destroy(buddies_cache);
	g_hash_table_destroy(groups_cache);

	buddies_cache = NULL;
	groups_cache = NULL;

	PURPLE_DBUS_UNREGISTER_POINTER(purplebuddylist);
	g_free(purplebuddylist);
	purplebuddylist = NULL;

	purple_signals_disconnect_by_handle(purple_blist_get_handle());
	purple_signals_unregister_by_instance(purple_blist_get_handle());
}
