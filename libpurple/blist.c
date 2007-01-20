/*
 * gaim
 *
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
 *
 */
#include "internal.h"
#include "blist.h"
#include "conversation.h"
#include "dbus-maybe.h"
#include "debug.h"
#include "notify.h"
#include "prefs.h"
#include "privacy.h"
#include "prpl.h"
#include "server.h"
#include "signals.h"
#include "util.h"
#include "value.h"
#include "xmlnode.h"

#define PATHSIZE 1024

static GaimBlistUiOps *blist_ui_ops = NULL;

static GaimBuddyList *gaimbuddylist = NULL;
static guint          save_timer = 0;
static gboolean       blist_loaded = FALSE;


/*********************************************************************
 * Private utility functions                                         *
 *********************************************************************/

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

struct _list_account_buddies {
	GSList *list;
	GaimAccount *account;
};

struct _gaim_hbuddy {
	char *name;
	GaimAccount *account;
	GaimBlistNode *group;
};

static guint _gaim_blist_hbuddy_hash(struct _gaim_hbuddy *hb)
{
	return g_str_hash(hb->name);
}

static guint _gaim_blist_hbuddy_equal(struct _gaim_hbuddy *hb1, struct _gaim_hbuddy *hb2)
{
	return ((!strcmp(hb1->name, hb2->name)) && hb1->account == hb2->account && hb1->group == hb2->group);
}

static void _gaim_blist_hbuddy_free_key(struct _gaim_hbuddy *hb)
{
	g_free(hb->name);
	g_free(hb);
}


/*********************************************************************
 * Writing to disk                                                   *
 *********************************************************************/

static void
value_to_xmlnode(gpointer key, gpointer hvalue, gpointer user_data)
{
	const char *name;
	GaimValue *value;
	xmlnode *node, *child;
	char buf[20];

	name    = (const char *)key;
	value   = (GaimValue *)hvalue;
	node    = (xmlnode *)user_data;

	g_return_if_fail(value != NULL);

	child = xmlnode_new_child(node, "setting");
	xmlnode_set_attrib(child, "name", name);

	if (gaim_value_get_type(value) == GAIM_TYPE_INT) {
		xmlnode_set_attrib(child, "type", "int");
		snprintf(buf, sizeof(buf), "%d", gaim_value_get_int(value));
		xmlnode_insert_data(child, buf, -1);
	}
	else if (gaim_value_get_type(value) == GAIM_TYPE_STRING) {
		xmlnode_set_attrib(child, "type", "string");
		xmlnode_insert_data(child, gaim_value_get_string(value), -1);
	}
	else if (gaim_value_get_type(value) == GAIM_TYPE_BOOLEAN) {
		xmlnode_set_attrib(child, "type", "bool");
		snprintf(buf, sizeof(buf), "%d", gaim_value_get_boolean(value));
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
buddy_to_xmlnode(GaimBlistNode *bnode)
{
	xmlnode *node, *child;
	GaimBuddy *buddy;

	buddy = (GaimBuddy *)bnode;

	node = xmlnode_new("buddy");
	xmlnode_set_attrib(node, "account", gaim_account_get_username(buddy->account));
	xmlnode_set_attrib(node, "proto", gaim_account_get_protocol_id(buddy->account));

	child = xmlnode_new_child(node, "name");
	xmlnode_insert_data(child, buddy->name, -1);

	if (buddy->alias != NULL)
	{
		child = xmlnode_new_child(node, "alias");
		xmlnode_insert_data(child, buddy->alias, -1);
	}

	/* Write buddy settings */
	g_hash_table_foreach(buddy->node.settings, value_to_xmlnode, node);

	return node;
}

static xmlnode *
contact_to_xmlnode(GaimBlistNode *cnode)
{
	xmlnode *node, *child;
	GaimContact *contact;
	GaimBlistNode *bnode;

	contact = (GaimContact *)cnode;

	node = xmlnode_new("contact");

	if (contact->alias != NULL)
	{
		xmlnode_set_attrib(node, "alias", contact->alias);
	}

	/* Write buddies */
	for (bnode = cnode->child; bnode != NULL; bnode = bnode->next)
	{
		if (!GAIM_BLIST_NODE_SHOULD_SAVE(bnode))
			continue;
		if (GAIM_BLIST_NODE_IS_BUDDY(bnode))
		{
			child = buddy_to_xmlnode(bnode);
			xmlnode_insert_child(node, child);
		}
	}

	/* Write contact settings */
	g_hash_table_foreach(cnode->settings, value_to_xmlnode, node);

	return node;
}

static xmlnode *
chat_to_xmlnode(GaimBlistNode *cnode)
{
	xmlnode *node, *child;
	GaimChat *chat;

	chat = (GaimChat *)cnode;

	node = xmlnode_new("chat");
	xmlnode_set_attrib(node, "proto", gaim_account_get_protocol_id(chat->account));
	xmlnode_set_attrib(node, "account", gaim_account_get_username(chat->account));

	if (chat->alias != NULL)
	{
		child = xmlnode_new_child(node, "alias");
		xmlnode_insert_data(child, chat->alias, -1);
	}

	/* Write chat components */
	g_hash_table_foreach(chat->components, chat_component_to_xmlnode, node);

	/* Write chat settings */
	g_hash_table_foreach(chat->node.settings, value_to_xmlnode, node);

	return node;
}

static xmlnode *
group_to_xmlnode(GaimBlistNode *gnode)
{
	xmlnode *node, *child;
	GaimGroup *group;
	GaimBlistNode *cnode;

	group = (GaimGroup *)gnode;

	node = xmlnode_new("group");
	xmlnode_set_attrib(node, "name", group->name);

	/* Write settings */
	g_hash_table_foreach(group->node.settings, value_to_xmlnode, node);

	/* Write contacts and chats */
	for (cnode = gnode->child; cnode != NULL; cnode = cnode->next)
	{
		if (!GAIM_BLIST_NODE_SHOULD_SAVE(cnode))
			continue;
		if (GAIM_BLIST_NODE_IS_CONTACT(cnode))
		{
			child = contact_to_xmlnode(cnode);
			xmlnode_insert_child(node, child);
		}
		else if (GAIM_BLIST_NODE_IS_CHAT(cnode))
		{
			child = chat_to_xmlnode(cnode);
			xmlnode_insert_child(node, child);
		}
	}

	return node;
}

static xmlnode *
accountprivacy_to_xmlnode(GaimAccount *account)
{
	xmlnode *node, *child;
	GSList *cur;
	char buf[10];

	node = xmlnode_new("account");
	xmlnode_set_attrib(node, "proto", gaim_account_get_protocol_id(account));
	xmlnode_set_attrib(node, "name", gaim_account_get_username(account));
	snprintf(buf, sizeof(buf), "%d", account->perm_deny);
	xmlnode_set_attrib(node, "mode", buf);

	for (cur = account->permit; cur; cur = cur->next)
	{
		child = xmlnode_new_child(node, "permit");
		xmlnode_insert_data(child, cur->data, -1);
	}

	for (cur = account->deny; cur; cur = cur->next)
	{
		child = xmlnode_new_child(node, "block");
		xmlnode_insert_data(child, cur->data, -1);
	}

	return node;
}

static xmlnode *
blist_to_xmlnode()
{
	xmlnode *node, *child, *grandchild;
	GaimBlistNode *gnode;
	GList *cur;

	node = xmlnode_new("gaim");
	xmlnode_set_attrib(node, "version", "1.0");

	/* Write groups */
	child = xmlnode_new_child(node, "blist");
	for (gnode = gaimbuddylist->root; gnode != NULL; gnode = gnode->next)
	{
		if (!GAIM_BLIST_NODE_SHOULD_SAVE(gnode))
			continue;
		if (GAIM_BLIST_NODE_IS_GROUP(gnode))
		{
			grandchild = group_to_xmlnode(gnode);
			xmlnode_insert_child(child, grandchild);
		}
	}

	/* Write privacy settings */
	child = xmlnode_new_child(node, "privacy");
	for (cur = gaim_accounts_get_all(); cur != NULL; cur = cur->next)
	{
		grandchild = accountprivacy_to_xmlnode(cur->data);
		xmlnode_insert_child(child, grandchild);
	}

	return node;
}

static void
gaim_blist_sync()
{
	xmlnode *node;
	char *data;

	if (!blist_loaded)
	{
		gaim_debug_error("blist", "Attempted to save buddy list before it "
						 "was read!\n");
		return;
	}

	node = blist_to_xmlnode();
	data = xmlnode_to_formatted_str(node, NULL);
	gaim_util_write_data_to_file("blist.xml", data, -1);
	g_free(data);
	xmlnode_free(node);
}

static gboolean
save_cb(gpointer data)
{
	gaim_blist_sync();
	save_timer = 0;
	return FALSE;
}

void
gaim_blist_schedule_save()
{
	if (save_timer == 0)
		save_timer = gaim_timeout_add(5000, save_cb, NULL);
}


/*********************************************************************
 * Reading from disk                                                 *
 *********************************************************************/

static void
parse_setting(GaimBlistNode *node, xmlnode *setting)
{
	const char *name = xmlnode_get_attrib(setting, "name");
	const char *type = xmlnode_get_attrib(setting, "type");
	char *value = xmlnode_get_data(setting);

	if (!value)
		return;

	if (!type || !strcmp(type, "string"))
		gaim_blist_node_set_string(node, name, value);
	else if (!strcmp(type, "bool"))
		gaim_blist_node_set_bool(node, name, atoi(value));
	else if (!strcmp(type, "int"))
		gaim_blist_node_set_int(node, name, atoi(value));

	g_free(value);
}

static void
parse_buddy(GaimGroup *group, GaimContact *contact, xmlnode *bnode)
{
	GaimAccount *account;
	GaimBuddy *buddy;
	char *name = NULL, *alias = NULL;
	const char *acct_name, *proto, *protocol;
	xmlnode *x;

	acct_name = xmlnode_get_attrib(bnode, "account");
	protocol = xmlnode_get_attrib(bnode, "protocol");
	proto = xmlnode_get_attrib(bnode, "proto");

	if (!acct_name || (!proto && !protocol))
		return;

	account = gaim_accounts_find(acct_name, proto ? proto : protocol);

	if (!account)
		return;

	if ((x = xmlnode_get_child(bnode, "name")))
		name = xmlnode_get_data(x);

	if (!name)
		return;

	if ((x = xmlnode_get_child(bnode, "alias")))
		alias = xmlnode_get_data(x);

	buddy = gaim_buddy_new(account, name, alias);
	gaim_blist_add_buddy(buddy, contact, group,
			gaim_blist_get_last_child((GaimBlistNode*)contact));

	for (x = xmlnode_get_child(bnode, "setting"); x; x = xmlnode_get_next_twin(x)) {
		parse_setting((GaimBlistNode*)buddy, x);
	}

	g_free(name);
	g_free(alias);
}

static void
parse_contact(GaimGroup *group, xmlnode *cnode)
{
	GaimContact *contact = gaim_contact_new();
	xmlnode *x;
	const char *alias;

	gaim_blist_add_contact(contact, group,
			gaim_blist_get_last_child((GaimBlistNode*)group));

	if ((alias = xmlnode_get_attrib(cnode, "alias"))) {
		gaim_contact_set_alias(contact, alias);
	}

	for (x = cnode->child; x; x = x->next) {
		if (x->type != XMLNODE_TYPE_TAG)
			continue;
		if (!strcmp(x->name, "buddy"))
			parse_buddy(group, contact, x);
		else if (!strcmp(x->name, "setting"))
			parse_setting((GaimBlistNode*)contact, x);
	}

	/* if the contact is empty, don't keep it around.  it causes problems */
	if (!((GaimBlistNode*)contact)->child)
		gaim_blist_remove_contact(contact);
}

static void
parse_chat(GaimGroup *group, xmlnode *cnode)
{
	GaimChat *chat;
	GaimAccount *account;
	const char *acct_name, *proto, *protocol;
	xmlnode *x;
	char *alias = NULL;
	GHashTable *components;

	acct_name = xmlnode_get_attrib(cnode, "account");
	protocol = xmlnode_get_attrib(cnode, "protocol");
	proto = xmlnode_get_attrib(cnode, "proto");

	if (!acct_name || (!proto && !protocol))
		return;

	account = gaim_accounts_find(acct_name, proto ? proto : protocol);

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

	chat = gaim_chat_new(account, alias, components);
	gaim_blist_add_chat(chat, group,
			gaim_blist_get_last_child((GaimBlistNode*)group));

	for (x = xmlnode_get_child(cnode, "setting"); x; x = xmlnode_get_next_twin(x)) {
		parse_setting((GaimBlistNode*)chat, x);
	}

	g_free(alias);
}

static void
parse_group(xmlnode *groupnode)
{
	const char *name = xmlnode_get_attrib(groupnode, "name");
	GaimGroup *group;
	xmlnode *cnode;

	if (!name)
		name = _("Buddies");

	group = gaim_group_new(name);
	gaim_blist_add_group(group,
			gaim_blist_get_last_sibling(gaimbuddylist->root));

	for (cnode = groupnode->child; cnode; cnode = cnode->next) {
		if (cnode->type != XMLNODE_TYPE_TAG)
			continue;
		if (!strcmp(cnode->name, "setting"))
			parse_setting((GaimBlistNode*)group, cnode);
		else if (!strcmp(cnode->name, "contact") ||
				!strcmp(cnode->name, "person"))
			parse_contact(group, cnode);
		else if (!strcmp(cnode->name, "chat"))
			parse_chat(group, cnode);
	}
}

/* TODO: Make static and rename to load_blist */
void
gaim_blist_load()
{
	xmlnode *gaim, *blist, *privacy;

	blist_loaded = TRUE;

	gaim = gaim_util_read_xml_from_file("blist.xml", _("buddy list"));

	if (gaim == NULL)
		return;

	blist = xmlnode_get_child(gaim, "blist");
	if (blist) {
		xmlnode *groupnode;
		for (groupnode = xmlnode_get_child(blist, "group"); groupnode != NULL;
				groupnode = xmlnode_get_next_twin(groupnode)) {
			parse_group(groupnode);
		}
	}

	privacy = xmlnode_get_child(gaim, "privacy");
	if (privacy) {
		xmlnode *anode;
		for (anode = privacy->child; anode; anode = anode->next) {
			xmlnode *x;
			GaimAccount *account;
			int imode;
			const char *acct_name, *proto, *mode, *protocol;

			acct_name = xmlnode_get_attrib(anode, "name");
			protocol = xmlnode_get_attrib(anode, "protocol");
			proto = xmlnode_get_attrib(anode, "proto");
			mode = xmlnode_get_attrib(anode, "mode");

			if (!acct_name || (!proto && !protocol) || !mode)
				continue;

			account = gaim_accounts_find(acct_name, proto ? proto : protocol);

			if (!account)
				continue;

			imode = atoi(mode);
			account->perm_deny = (imode != 0 ? imode : GAIM_PRIVACY_ALLOW_ALL);

			for (x = anode->child; x; x = x->next) {
				char *name;
				if (x->type != XMLNODE_TYPE_TAG)
					continue;

				if (!strcmp(x->name, "permit")) {
					name = xmlnode_get_data(x);
					gaim_privacy_permit_add(account, name, TRUE);
					g_free(name);
				} else if (!strcmp(x->name, "block")) {
					name = xmlnode_get_data(x);
					gaim_privacy_deny_add(account, name, TRUE);
					g_free(name);
				}
			}
		}
	}

	xmlnode_free(gaim);
}


/*********************************************************************
 * Stuff                                                             *
 *********************************************************************/

static void
gaim_contact_compute_priority_buddy(GaimContact *contact)
{
	GaimBlistNode *bnode;
	GaimBuddy *new_priority = NULL;

	g_return_if_fail(contact != NULL);

	contact->priority = NULL;
	for (bnode = ((GaimBlistNode*)contact)->child;
			bnode != NULL;
			bnode = bnode->next)
	{
		GaimBuddy *buddy;

		if (!GAIM_BLIST_NODE_IS_BUDDY(bnode))
			continue;

		buddy = (GaimBuddy*)bnode;

		if (!gaim_account_is_connected(buddy->account))
			continue;
		if (new_priority == NULL)
			new_priority = buddy;
		else
		{
			int cmp;

			cmp = gaim_presence_compare(gaim_buddy_get_presence(new_priority),
			                            gaim_buddy_get_presence(buddy));

			if (cmp > 0 || (cmp == 0 &&
			                gaim_prefs_get_bool("/core/contact/last_match")))
			{
				new_priority = buddy;
			}
		}
	}

	contact->priority = new_priority;
	contact->priority_valid = TRUE;
}


/*****************************************************************************
 * Public API functions                                                      *
 *****************************************************************************/

GaimBuddyList *gaim_blist_new()
{
	GaimBlistUiOps *ui_ops;
	GaimBuddyList *gbl = g_new0(GaimBuddyList, 1);
	GAIM_DBUS_REGISTER_POINTER(gbl, GaimBuddyList);

	ui_ops = gaim_blist_get_ui_ops();

	gbl->buddies = g_hash_table_new_full((GHashFunc)_gaim_blist_hbuddy_hash,
					 (GEqualFunc)_gaim_blist_hbuddy_equal,
					 (GDestroyNotify)_gaim_blist_hbuddy_free_key, NULL);

	if (ui_ops != NULL && ui_ops->new_list != NULL)
		ui_ops->new_list(gbl);

	return gbl;
}

void
gaim_set_blist(GaimBuddyList *list)
{
	gaimbuddylist = list;
}

GaimBuddyList *
gaim_get_blist()
{
	return gaimbuddylist;
}

GaimBlistNode *
gaim_blist_get_root()
{
	return gaimbuddylist ? gaimbuddylist->root : NULL;
}

void gaim_blist_show()
{
	GaimBlistUiOps *ops = gaim_blist_get_ui_ops();

	if (ops && ops->show)
		ops->show(gaimbuddylist);
}

void gaim_blist_destroy()
{
	GaimBlistUiOps *ops = gaim_blist_get_ui_ops();

	gaim_debug(GAIM_DEBUG_INFO, "blist", "Destroying\n");

	if (ops && ops->destroy)
		ops->destroy(gaimbuddylist);
}

void gaim_blist_set_visible(gboolean show)
{
	GaimBlistUiOps *ops = gaim_blist_get_ui_ops();

	if (ops && ops->set_visible)
		ops->set_visible(gaimbuddylist, show);
}

static GaimBlistNode *get_next_node(GaimBlistNode *node, gboolean godeep)
{
	if (node == NULL)
		return NULL;

	if (godeep && node->child)
		return node->child;

	if (node->next)
		return node->next;

	return get_next_node(node->parent, FALSE);
}

GaimBlistNode *gaim_blist_node_next(GaimBlistNode *node, gboolean offline)
{
	GaimBlistNode *ret = node;

	if (offline)
		return get_next_node(ret, TRUE);
	do
	{
		ret = get_next_node(ret, TRUE);
	} while (ret && GAIM_BLIST_NODE_IS_BUDDY(ret) &&
			!gaim_account_is_connected(gaim_buddy_get_account((GaimBuddy *)ret)));

	return ret;
}

void
gaim_blist_update_buddy_status(GaimBuddy *buddy, GaimStatus *old_status)
{
	GaimBlistUiOps *ops = gaim_blist_get_ui_ops();
	GaimPresence *presence;
	GaimStatus *status;

	g_return_if_fail(buddy != NULL);

	presence = gaim_buddy_get_presence(buddy);
	status = gaim_presence_get_active_status(presence);

	gaim_debug_info("blist", "Updating buddy status for %s (%s)\n",
			buddy->name, gaim_account_get_protocol_name(buddy->account));

	if (gaim_status_is_online(status) &&
		!gaim_status_is_online(old_status)) {

		gaim_signal_emit(gaim_blist_get_handle(), "buddy-signed-on", buddy);

		((GaimContact*)((GaimBlistNode*)buddy)->parent)->online++;
		if (((GaimContact*)((GaimBlistNode*)buddy)->parent)->online == 1)
			((GaimGroup *)((GaimBlistNode *)buddy)->parent->parent)->online++;
	} else if (!gaim_status_is_online(status) &&
				gaim_status_is_online(old_status)) {
		gaim_blist_node_set_int(&buddy->node, "last_seen", time(NULL));
		gaim_signal_emit(gaim_blist_get_handle(), "buddy-signed-off", buddy);
		((GaimContact*)((GaimBlistNode*)buddy)->parent)->online--;
		if (((GaimContact*)((GaimBlistNode*)buddy)->parent)->online == 0)
			((GaimGroup *)((GaimBlistNode *)buddy)->parent->parent)->online--;
	} else {
		gaim_signal_emit(gaim_blist_get_handle(),
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
	gaim_contact_invalidate_priority_buddy(gaim_buddy_get_contact(buddy));
	if (ops && ops->update)
		ops->update(gaimbuddylist, (GaimBlistNode *)buddy);
}

void gaim_blist_update_buddy_icon(GaimBuddy *buddy)
{
	GaimBlistUiOps *ops = gaim_blist_get_ui_ops();

	g_return_if_fail(buddy != NULL);

	if (ops && ops->update)
		ops->update(gaimbuddylist, (GaimBlistNode *)buddy);
}

/*
 * TODO: Maybe remove the call to this from server.c and call it
 * from oscar.c and toc.c instead?
 */
void gaim_blist_rename_buddy(GaimBuddy *buddy, const char *name)
{
	GaimBlistUiOps *ops = gaim_blist_get_ui_ops();
	struct _gaim_hbuddy *hb;

	g_return_if_fail(buddy != NULL);

	hb = g_new(struct _gaim_hbuddy, 1);
	hb->name = g_strdup(gaim_normalize(buddy->account, buddy->name));
	hb->account = buddy->account;
	hb->group = ((GaimBlistNode *)buddy)->parent->parent;
	g_hash_table_remove(gaimbuddylist->buddies, hb);

	g_free(hb->name);
	hb->name = g_strdup(gaim_normalize(buddy->account, name));
	g_hash_table_replace(gaimbuddylist->buddies, hb, buddy);

	g_free(buddy->name);
	buddy->name = g_strdup(name);

	gaim_blist_schedule_save();

	if (ops && ops->update)
		ops->update(gaimbuddylist, (GaimBlistNode *)buddy);
}

void gaim_blist_alias_contact(GaimContact *contact, const char *alias)
{
	GaimBlistUiOps *ops = gaim_blist_get_ui_ops();
	GaimConversation *conv;
	GaimBlistNode *bnode;
	char *old_alias;

	g_return_if_fail(contact != NULL);

	old_alias = contact->alias;

	if ((alias != NULL) && (*alias != '\0'))
		contact->alias = g_strdup(alias);
	else
		contact->alias = NULL;

	gaim_blist_schedule_save();

	if (ops && ops->update)
		ops->update(gaimbuddylist, (GaimBlistNode *)contact);

	for(bnode = ((GaimBlistNode *)contact)->child; bnode != NULL; bnode = bnode->next)
	{
		GaimBuddy *buddy = (GaimBuddy *)bnode;

		conv = gaim_find_conversation_with_account(GAIM_CONV_TYPE_IM, buddy->name,
												   buddy->account);
		if (conv)
			gaim_conversation_autoset_title(conv);
	}

	gaim_signal_emit(gaim_blist_get_handle(), "blist-node-aliased",
					 contact, old_alias);
	g_free(old_alias);
}

void gaim_blist_alias_chat(GaimChat *chat, const char *alias)
{
	GaimBlistUiOps *ops = gaim_blist_get_ui_ops();
	char *old_alias;

	g_return_if_fail(chat != NULL);

	old_alias = chat->alias;

	if ((alias != NULL) && (*alias != '\0'))
		chat->alias = g_strdup(alias);
	else
		chat->alias = NULL;

	gaim_blist_schedule_save();

	if (ops && ops->update)
		ops->update(gaimbuddylist, (GaimBlistNode *)chat);

	gaim_signal_emit(gaim_blist_get_handle(), "blist-node-aliased",
					 chat, old_alias);
	g_free(old_alias);
}

void gaim_blist_alias_buddy(GaimBuddy *buddy, const char *alias)
{
	GaimBlistUiOps *ops = gaim_blist_get_ui_ops();
	GaimConversation *conv;
	char *old_alias;

	g_return_if_fail(buddy != NULL);

	old_alias = buddy->alias;

	if ((alias != NULL) && (*alias != '\0'))
		buddy->alias = g_strdup(alias);
	else
		buddy->alias = NULL;

	gaim_blist_schedule_save();

	if (ops && ops->update)
		ops->update(gaimbuddylist, (GaimBlistNode *)buddy);

	conv = gaim_find_conversation_with_account(GAIM_CONV_TYPE_IM, buddy->name,
											   buddy->account);
	if (conv)
		gaim_conversation_autoset_title(conv);

	gaim_signal_emit(gaim_blist_get_handle(), "blist-node-aliased",
					 buddy, old_alias);
	g_free(old_alias);
}

void gaim_blist_server_alias_buddy(GaimBuddy *buddy, const char *alias)
{
	GaimBlistUiOps *ops = gaim_blist_get_ui_ops();
	GaimConversation *conv;
	char *old_alias;

	g_return_if_fail(buddy != NULL);

	old_alias = buddy->server_alias;

	if ((alias != NULL) && (*alias != '\0') && g_utf8_validate(alias, -1, NULL))
		buddy->server_alias = g_strdup(alias);
	else
		buddy->server_alias = NULL;

	gaim_blist_schedule_save();

	if (ops && ops->update)
		ops->update(gaimbuddylist, (GaimBlistNode *)buddy);

	conv = gaim_find_conversation_with_account(GAIM_CONV_TYPE_IM, buddy->name,
											   buddy->account);
	if (conv)
		gaim_conversation_autoset_title(conv);

	gaim_signal_emit(gaim_blist_get_handle(), "blist-node-aliased",
					 buddy, old_alias);
	g_free(old_alias);
}

/*
 * TODO: If merging, prompt the user if they want to merge.
 */
void gaim_blist_rename_group(GaimGroup *source, const char *new_name)
{
	GaimBlistUiOps *ops = gaim_blist_get_ui_ops();
	GaimGroup *dest;
	gchar *old_name;
	GList *moved_buddies = NULL;
	GSList *accts;

	g_return_if_fail(source != NULL);
	g_return_if_fail(new_name != NULL);

	if (*new_name == '\0' || !strcmp(new_name, source->name))
		return;

	dest = gaim_find_group(new_name);
	if (dest != NULL) {
		/* We're merging two groups */
		GaimBlistNode *prev, *child, *next;

		prev = gaim_blist_get_last_child((GaimBlistNode*)dest);
		child = ((GaimBlistNode*)source)->child;

		/*
		 * TODO: This seems like a dumb way to do this... why not just
		 * append all children from the old group to the end of the new
		 * one?  PRPLs might be expecting to receive an add_buddy() for
		 * each moved buddy...
		 */
		while (child)
		{
			next = child->next;
			if (GAIM_BLIST_NODE_IS_CONTACT(child)) {
				GaimBlistNode *bnode;
				gaim_blist_add_contact((GaimContact *)child, dest, prev);
				for (bnode = child->child; bnode != NULL; bnode = bnode->next) {
					gaim_blist_add_buddy((GaimBuddy *)bnode, (GaimContact *)child,
							NULL, bnode->prev);
					moved_buddies = g_list_append(moved_buddies, bnode);
				}
				prev = child;
			} else if (GAIM_BLIST_NODE_IS_CHAT(child)) {
				gaim_blist_add_chat((GaimChat *)child, dest, prev);
				prev = child;
			} else {
				gaim_debug(GAIM_DEBUG_ERROR, "blist",
						"Unknown child type in group %s\n", source->name);
			}
			child = next;
		}

		/* Make a copy of the old group name and then delete the old group */
		old_name = g_strdup(source->name);
		gaim_blist_remove_group(source);
		source = dest;
	} else {
		/* A simple rename */
		GaimBlistNode *cnode, *bnode;

		/* Build a GList of all buddies in this group */
		for (cnode = ((GaimBlistNode *)source)->child; cnode != NULL; cnode = cnode->next) {
			if (GAIM_BLIST_NODE_IS_CONTACT(cnode))
				for (bnode = cnode->child; bnode != NULL; bnode = bnode->next)
					moved_buddies = g_list_append(moved_buddies, bnode);
		}

		old_name = source->name;
		source->name = g_strdup(new_name);
	}

	/* Save our changes */
	gaim_blist_schedule_save();

	/* Update the UI */
	if (ops && ops->update)
		ops->update(gaimbuddylist, (GaimBlistNode*)source);

	/* Notify all PRPLs */
	/* TODO: Is this condition needed?  Seems like it would always be TRUE */
	if(old_name && source && strcmp(source->name, old_name)) {
		for (accts = gaim_group_get_accounts(source); accts; accts = g_slist_remove(accts, accts->data)) {
			GaimAccount *account = accts->data;
			GaimPluginProtocolInfo *prpl_info = NULL;
			GList *l = NULL, *buddies = NULL;

			if(account->gc && account->gc->prpl)
				prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(account->gc->prpl);

			if(!prpl_info)
				continue;

			for(l = moved_buddies; l; l = l->next) {
				GaimBuddy *buddy = (GaimBuddy *)l->data;

				if(buddy && buddy->account == account)
					buddies = g_list_append(buddies, (GaimBlistNode *)buddy);
			}

			if(prpl_info->rename_group) {
				prpl_info->rename_group(account->gc, old_name, source, buddies);
			} else {
				GList *cur, *groups = NULL;

				/* Make a list of what the groups each buddy is in */
				for(cur = buddies; cur; cur = cur->next) {
					GaimBlistNode *node = (GaimBlistNode *)cur->data;
					groups = g_list_prepend(groups, node->parent->parent);
				}

				gaim_account_remove_buddies(account, buddies, groups);
				g_list_free(groups);
				gaim_account_add_buddies(account, buddies);
			}

			g_list_free(buddies);
		}
	}
	g_list_free(moved_buddies);
	g_free(old_name);
}

static void gaim_blist_node_initialize_settings(GaimBlistNode *node);

GaimChat *gaim_chat_new(GaimAccount *account, const char *alias, GHashTable *components)
{
	GaimBlistUiOps *ops = gaim_blist_get_ui_ops();
	GaimChat *chat;

	g_return_val_if_fail(account != NULL, FALSE);
	g_return_val_if_fail(components != NULL, FALSE);

	chat = g_new0(GaimChat, 1);
	chat->account = account;
	if ((alias != NULL) && (*alias != '\0'))
		chat->alias = g_strdup(alias);
	chat->components = components;
	gaim_blist_node_initialize_settings((GaimBlistNode *)chat);
	((GaimBlistNode *)chat)->type = GAIM_BLIST_CHAT_NODE;

	if (ops != NULL && ops->new_node != NULL)
		ops->new_node((GaimBlistNode *)chat);

	GAIM_DBUS_REGISTER_POINTER(chat, GaimChat);
	return chat;
}

GaimBuddy *gaim_buddy_new(GaimAccount *account, const char *screenname, const char *alias)
{
	GaimBlistUiOps *ops = gaim_blist_get_ui_ops();
	GaimBuddy *buddy;

	g_return_val_if_fail(account != NULL, FALSE);
	g_return_val_if_fail(screenname != NULL, FALSE);

	buddy = g_new0(GaimBuddy, 1);
	buddy->account  = account;
	buddy->name     = g_strdup(screenname);
	buddy->alias    = g_strdup(alias);
	buddy->presence = gaim_presence_new_for_buddy(buddy);
	((GaimBlistNode *)buddy)->type = GAIM_BLIST_BUDDY_NODE;

	gaim_presence_set_status_active(buddy->presence, "offline", TRUE);

	gaim_blist_node_initialize_settings((GaimBlistNode *)buddy);

	if (ops && ops->new_node)
		ops->new_node((GaimBlistNode *)buddy);

	GAIM_DBUS_REGISTER_POINTER(buddy, GaimBuddy);
	return buddy;
}

void
gaim_buddy_set_icon(GaimBuddy *buddy, GaimBuddyIcon *icon)
{
	g_return_if_fail(buddy != NULL);

	if (buddy->icon != icon) {
		if (buddy->icon != NULL)
			gaim_buddy_icon_unref(buddy->icon);
		
		buddy->icon = (icon != NULL ? gaim_buddy_icon_ref(icon) : NULL);
	}

	if (buddy->icon)
		gaim_buddy_icon_cache(icon, buddy);
	else
		gaim_buddy_icon_uncache(buddy);

	gaim_blist_schedule_save();

	gaim_signal_emit(gaim_blist_get_handle(), "buddy-icon-changed", buddy);

	gaim_blist_update_buddy_icon(buddy);
}

GaimAccount *
gaim_buddy_get_account(const GaimBuddy *buddy)
{
	g_return_val_if_fail(buddy != NULL, NULL);

	return buddy->account;
}

const char *
gaim_buddy_get_name(const GaimBuddy *buddy)
{
	g_return_val_if_fail(buddy != NULL, NULL);

	return buddy->name;
}

GaimBuddyIcon *
gaim_buddy_get_icon(const GaimBuddy *buddy)
{
	g_return_val_if_fail(buddy != NULL, NULL);

	return buddy->icon;
}

void gaim_blist_add_chat(GaimChat *chat, GaimGroup *group, GaimBlistNode *node)
{
	GaimBlistNode *cnode = (GaimBlistNode*)chat;
	GaimBlistUiOps *ops = gaim_blist_get_ui_ops();

	g_return_if_fail(chat != NULL);
	g_return_if_fail(GAIM_BLIST_NODE_IS_CHAT((GaimBlistNode *)chat));

	if (node == NULL) {
		if (group == NULL) {
			group = gaim_group_new(_("Chats"));
			gaim_blist_add_group(group,
					gaim_blist_get_last_sibling(gaimbuddylist->root));
		}
	} else {
		group = (GaimGroup*)node->parent;
	}

	/* if we're moving to overtop of ourselves, do nothing */
	if (cnode == node)
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
		if (cnode->next)
			cnode->next->prev = cnode->prev;
		if (cnode->prev)
			cnode->prev->next = cnode->next;
		if (cnode->parent->child == cnode)
			cnode->parent->child = cnode->next;

		if (ops && ops->remove)
			ops->remove(gaimbuddylist, cnode);
		/* ops->remove() cleaned up the cnode's ui_data, so we need to
		 * reinitialize it */
		if (ops && ops->new_node)
			ops->new_node(cnode);

		gaim_blist_schedule_save();
	}

	if (node != NULL) {
		if (node->next)
			node->next->prev = cnode;
		cnode->next = node->next;
		cnode->prev = node;
		cnode->parent = node->parent;
		node->next = cnode;
		((GaimGroup *)node->parent)->totalsize++;
		if (gaim_account_is_connected(chat->account)) {
			((GaimGroup *)node->parent)->online++;
			((GaimGroup *)node->parent)->currentsize++;
		}
	} else {
		if (((GaimBlistNode *)group)->child)
			((GaimBlistNode *)group)->child->prev = cnode;
		cnode->next = ((GaimBlistNode *)group)->child;
		cnode->prev = NULL;
		((GaimBlistNode *)group)->child = cnode;
		cnode->parent = (GaimBlistNode *)group;
		group->totalsize++;
		if (gaim_account_is_connected(chat->account)) {
			group->online++;
			group->currentsize++;
		}
	}

	gaim_blist_schedule_save();

	if (ops && ops->update)
		ops->update(gaimbuddylist, (GaimBlistNode *)cnode);
}

void gaim_blist_add_buddy(GaimBuddy *buddy, GaimContact *contact, GaimGroup *group, GaimBlistNode *node)
{
	GaimBlistNode *cnode, *bnode;
	GaimGroup *g;
	GaimContact *c;
	GaimBlistUiOps *ops = gaim_blist_get_ui_ops();
	struct _gaim_hbuddy *hb;

	g_return_if_fail(buddy != NULL);
	g_return_if_fail(GAIM_BLIST_NODE_IS_BUDDY((GaimBlistNode*)buddy));

	bnode = (GaimBlistNode *)buddy;

	/* if we're moving to overtop of ourselves, do nothing */
	if (bnode == node || (!node && bnode->parent &&
				contact && bnode->parent == (GaimBlistNode*)contact
				&& bnode == bnode->parent->child))
		return;

	if (node && GAIM_BLIST_NODE_IS_BUDDY(node)) {
		c = (GaimContact*)node->parent;
		g = (GaimGroup*)node->parent->parent;
	} else if (contact) {
		c = contact;
		g = (GaimGroup *)((GaimBlistNode *)c)->parent;
	} else {
		if (group) {
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

	if (bnode->parent) {
		if (GAIM_BUDDY_IS_ONLINE(buddy)) {
			((GaimContact*)bnode->parent)->online--;
			if (((GaimContact*)bnode->parent)->online == 0)
				((GaimGroup*)bnode->parent->parent)->online--;
		}
		if (gaim_account_is_connected(buddy->account)) {
			((GaimContact*)bnode->parent)->currentsize--;
			if (((GaimContact*)bnode->parent)->currentsize == 0)
				((GaimGroup*)bnode->parent->parent)->currentsize--;
		}
		((GaimContact*)bnode->parent)->totalsize--;
		/* the group totalsize will be taken care of by remove_contact below */

		if (bnode->parent->parent != (GaimBlistNode*)g)
			serv_move_buddy(buddy, (GaimGroup *)bnode->parent->parent, g);

		if (bnode->next)
			bnode->next->prev = bnode->prev;
		if (bnode->prev)
			bnode->prev->next = bnode->next;
		if (bnode->parent->child == bnode)
			bnode->parent->child = bnode->next;

		if (ops && ops->remove)
			ops->remove(gaimbuddylist, bnode);

		gaim_blist_schedule_save();

		if (bnode->parent->parent != (GaimBlistNode*)g) {
			hb = g_new(struct _gaim_hbuddy, 1);
			hb->name = g_strdup(gaim_normalize(buddy->account, buddy->name));
			hb->account = buddy->account;
			hb->group = bnode->parent->parent;
			g_hash_table_remove(gaimbuddylist->buddies, hb);
			g_free(hb->name);
			g_free(hb);
		}

		if (!bnode->parent->child) {
			gaim_blist_remove_contact((GaimContact*)bnode->parent);
		} else {
			gaim_contact_invalidate_priority_buddy((GaimContact*)bnode->parent);
			if (ops && ops->update)
				ops->update(gaimbuddylist, bnode->parent);
		}
	}

	if (node && GAIM_BLIST_NODE_IS_BUDDY(node)) {
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

	if (GAIM_BUDDY_IS_ONLINE(buddy)) {
		((GaimContact*)bnode->parent)->online++;
		if (((GaimContact*)bnode->parent)->online == 1)
			((GaimGroup*)bnode->parent->parent)->online++;
	}
	if (gaim_account_is_connected(buddy->account)) {
		((GaimContact*)bnode->parent)->currentsize++;
		if (((GaimContact*)bnode->parent)->currentsize == 1)
			((GaimGroup*)bnode->parent->parent)->currentsize++;
	}
	((GaimContact*)bnode->parent)->totalsize++;

	hb = g_new(struct _gaim_hbuddy, 1);
	hb->name = g_strdup(gaim_normalize(buddy->account, buddy->name));
	hb->account = buddy->account;
	hb->group = ((GaimBlistNode*)buddy)->parent->parent;

	g_hash_table_replace(gaimbuddylist->buddies, hb, buddy);

	gaim_contact_invalidate_priority_buddy(gaim_buddy_get_contact(buddy));

	gaim_blist_schedule_save();

	if (ops && ops->update)
		ops->update(gaimbuddylist, (GaimBlistNode*)buddy);

	/* Signal that the buddy has been added */
	gaim_signal_emit(gaim_blist_get_handle(), "buddy-added", buddy);
}

GaimContact *gaim_contact_new()
{
	GaimBlistUiOps *ops = gaim_blist_get_ui_ops();

	GaimContact *contact = g_new0(GaimContact, 1);
	contact->totalsize = 0;
	contact->currentsize = 0;
	contact->online = 0;
	gaim_blist_node_initialize_settings((GaimBlistNode *)contact);
	((GaimBlistNode *)contact)->type = GAIM_BLIST_CONTACT_NODE;

	if (ops && ops->new_node)
		ops->new_node((GaimBlistNode *)contact);

	GAIM_DBUS_REGISTER_POINTER(contact, GaimContact);
	return contact;
}

void gaim_contact_set_alias(GaimContact *contact, const char *alias)
{
	GaimBlistUiOps *ops = gaim_blist_get_ui_ops();
	char *old_alias;

	g_return_if_fail(contact != NULL);

	old_alias = contact->alias;

	if ((alias != NULL) && (*alias != '\0'))
		contact->alias = g_strdup(alias);
	else
		contact->alias = NULL;

	gaim_blist_schedule_save();

	if (ops && ops->update)
		ops->update(gaimbuddylist, (GaimBlistNode*)contact);

	gaim_signal_emit(gaim_blist_get_handle(), "blist-node-aliased",
					 contact, old_alias);
	g_free(old_alias);
}

const char *gaim_contact_get_alias(GaimContact* contact)
{
	g_return_val_if_fail(contact != NULL, NULL);

	if (contact->alias)
		return contact->alias;

	return gaim_buddy_get_alias(gaim_contact_get_priority_buddy(contact));
}

gboolean gaim_contact_on_account(GaimContact *c, GaimAccount *account)
{
	GaimBlistNode *bnode, *cnode = (GaimBlistNode *) c;

	g_return_val_if_fail(c != NULL, FALSE);
	g_return_val_if_fail(account != NULL, FALSE);

	for (bnode = cnode->child; bnode; bnode = bnode->next) {
		GaimBuddy *buddy;

		if (! GAIM_BLIST_NODE_IS_BUDDY(bnode))
			continue;

		buddy = (GaimBuddy *)bnode;
		if (buddy->account == account)
			return TRUE;
	}
	return FALSE;
}

void gaim_contact_invalidate_priority_buddy(GaimContact *contact)
{
	g_return_if_fail(contact != NULL);

	contact->priority_valid = FALSE;
}

GaimGroup *gaim_group_new(const char *name)
{
	GaimBlistUiOps *ops = gaim_blist_get_ui_ops();
	GaimGroup *group;

	g_return_val_if_fail(name  != NULL, NULL);
	g_return_val_if_fail(*name != '\0', NULL);

	group = gaim_find_group(name);
	if (group != NULL)
		return group;

	group = g_new0(GaimGroup, 1);
	group->name = g_strdup(name);
	group->totalsize = 0;
	group->currentsize = 0;
	group->online = 0;
	gaim_blist_node_initialize_settings((GaimBlistNode *)group);
	((GaimBlistNode *)group)->type = GAIM_BLIST_GROUP_NODE;

	if (ops && ops->new_node)
		ops->new_node((GaimBlistNode *)group);

	GAIM_DBUS_REGISTER_POINTER(group, GaimGroup);
	return group;
}

void gaim_blist_add_contact(GaimContact *contact, GaimGroup *group, GaimBlistNode *node)
{
	GaimBlistUiOps *ops = gaim_blist_get_ui_ops();
	GaimGroup *g;
	GaimBlistNode *gnode, *cnode, *bnode;

	g_return_if_fail(contact != NULL);
	g_return_if_fail(GAIM_BLIST_NODE_IS_CONTACT((GaimBlistNode*)contact));

	if ((GaimBlistNode*)contact == node)
		return;

	if (node && (GAIM_BLIST_NODE_IS_CONTACT(node) ||
				GAIM_BLIST_NODE_IS_CHAT(node)))
		g = (GaimGroup*)node->parent;
	else if (group)
		g = group;
	else {
		g = gaim_group_new(_("Buddies"));
		gaim_blist_add_group(g,
				gaim_blist_get_last_sibling(gaimbuddylist->root));
	}

	gnode = (GaimBlistNode*)g;
	cnode = (GaimBlistNode*)contact;

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
				GaimBlistNode *next_bnode = bnode->next;
				GaimBuddy *b = (GaimBuddy*)bnode;

				struct _gaim_hbuddy *hb = g_new(struct _gaim_hbuddy, 1);
				hb->name = g_strdup(gaim_normalize(b->account, b->name));
				hb->account = b->account;
				hb->group = cnode->parent;

				g_hash_table_remove(gaimbuddylist->buddies, hb);

				if (!gaim_find_buddy_in_group(b->account, b->name, g)) {
					hb->group = gnode;
					g_hash_table_replace(gaimbuddylist->buddies, hb, b);

					if (b->account->gc)
						serv_move_buddy(b, (GaimGroup *)cnode->parent, g);
				} else {
					gboolean empty_contact = FALSE;

					/* this buddy already exists in the group, so we're
					 * gonna delete it instead */
					g_free(hb->name);
					g_free(hb);
					if (b->account->gc)
						gaim_account_remove_buddy(b->account, b, (GaimGroup *)cnode->parent);

					if (!cnode->child->next)
						empty_contact = TRUE;
					gaim_blist_remove_buddy(b);

					/** in gaim_blist_remove_buddy(), if the last buddy in a
					 * contact is removed, the contact is cleaned up and
					 * g_free'd, so we mustn't try to reference bnode->next */
					if (empty_contact)
						return;
				}
				bnode = next_bnode;
			}
		}

		if (contact->online > 0)
			((GaimGroup*)cnode->parent)->online--;
		if (contact->currentsize > 0)
			((GaimGroup*)cnode->parent)->currentsize--;
		((GaimGroup*)cnode->parent)->totalsize--;

		if (ops && ops->remove)
			ops->remove(gaimbuddylist, cnode);

		gaim_blist_schedule_save();
	}

	if (node && (GAIM_BLIST_NODE_IS_CONTACT(node) ||
				GAIM_BLIST_NODE_IS_CHAT(node))) {
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

	if (contact->online > 0)
		g->online++;
	if (contact->currentsize > 0)
		g->currentsize++;
	g->totalsize++;

	gaim_blist_schedule_save();

	if (ops && ops->update)
	{
		if (cnode->child)
			ops->update(gaimbuddylist, cnode);

		for (bnode = cnode->child; bnode; bnode = bnode->next)
			ops->update(gaimbuddylist, bnode);
	}
}

void gaim_blist_merge_contact(GaimContact *source, GaimBlistNode *node)
{
	GaimBlistNode *sourcenode = (GaimBlistNode*)source;
	GaimBlistNode *targetnode;
	GaimBlistNode *prev, *cur, *next;
	GaimContact *target;

	g_return_if_fail(source != NULL);
	g_return_if_fail(node != NULL);

	if (GAIM_BLIST_NODE_IS_CONTACT(node)) {
		target = (GaimContact *)node;
		prev = gaim_blist_get_last_child(node);
	} else if (GAIM_BLIST_NODE_IS_BUDDY(node)) {
		target = (GaimContact *)node->parent;
		prev = node;
	} else {
		return;
	}

	if (source == target || !target)
		return;

	targetnode = (GaimBlistNode *)target;
	next = sourcenode->child;

	while (next) {
		cur = next;
		next = cur->next;
		if (GAIM_BLIST_NODE_IS_BUDDY(cur)) {
			gaim_blist_add_buddy((GaimBuddy *)cur, target, NULL, prev);
			prev = cur;
		}
	}
}

void gaim_blist_add_group(GaimGroup *group, GaimBlistNode *node)
{
	GaimBlistUiOps *ops;
	GaimBlistNode *gnode = (GaimBlistNode*)group;

	g_return_if_fail(group != NULL);
	g_return_if_fail(GAIM_BLIST_NODE_IS_GROUP((GaimBlistNode *)group));

	ops = gaim_blist_get_ui_ops();

	if (!gaimbuddylist->root) {
		gaimbuddylist->root = gnode;
		return;
	}

	/* if we're moving to overtop of ourselves, do nothing */
	if (gnode == node)
		return;

	if (gaim_find_group(group->name)) {
		/* This is just being moved */

		if (ops && ops->remove)
			ops->remove(gaimbuddylist, (GaimBlistNode *)group);

		if (gnode == gaimbuddylist->root)
			gaimbuddylist->root = gnode->next;
		if (gnode->prev)
			gnode->prev->next = gnode->next;
		if (gnode->next)
			gnode->next->prev = gnode->prev;
	}

	if (node && GAIM_BLIST_NODE_IS_GROUP(node)) {
		gnode->next = node->next;
		gnode->prev = node;
		if (node->next)
			node->next->prev = gnode;
		node->next = gnode;
	} else {
		if (gaimbuddylist->root)
			gaimbuddylist->root->prev = gnode;
		gnode->next = gaimbuddylist->root;
		gnode->prev = NULL;
		gaimbuddylist->root = gnode;
	}

	gaim_blist_schedule_save();

	if (ops && ops->update) {
		ops->update(gaimbuddylist, gnode);
		for (node = gnode->child; node; node = node->next)
			ops->update(gaimbuddylist, node);
	}
}

void gaim_blist_remove_contact(GaimContact *contact)
{
	GaimBlistUiOps *ops = gaim_blist_get_ui_ops();
	GaimBlistNode *node, *gnode;

	g_return_if_fail(contact != NULL);

	node = (GaimBlistNode *)contact;
	gnode = node->parent;

	if (node->child) {
		/*
		 * If this contact has children then remove them.  When the last
		 * buddy is removed from the contact, the contact is automatically
		 * deleted.
		 */
		while (node->child->next) {
			gaim_blist_remove_buddy((GaimBuddy*)node->child);
		}
		/*
		 * Remove the last buddy and trigger the deletion of the contact.
		 * It would probably be cleaner if contact-deletion was done after
		 * a timeout?  Or if it had to be done manually, like below?
		 */
		gaim_blist_remove_buddy((GaimBuddy*)node->child);
	} else {
		/* Remove the node from its parent */
		if (gnode->child == node)
			gnode->child = node->next;
		if (node->prev)
			node->prev->next = node->next;
		if (node->next)
			node->next->prev = node->prev;

		gaim_blist_schedule_save();

		/* Update the UI */
		if (ops && ops->remove)
			ops->remove(gaimbuddylist, node);

		/* Delete the node */
		g_hash_table_destroy(contact->node.settings);
		GAIM_DBUS_UNREGISTER_POINTER(contact);
		g_free(contact);
	}
}

void gaim_blist_remove_buddy(GaimBuddy *buddy)
{
	GaimBlistUiOps *ops = gaim_blist_get_ui_ops();
	GaimBlistNode *node, *cnode, *gnode;
	GaimContact *contact;
	GaimGroup *group;
	struct _gaim_hbuddy hb;

	g_return_if_fail(buddy != NULL);

	node = (GaimBlistNode *)buddy;
	cnode = node->parent;
	gnode = cnode->parent;
	contact = (GaimContact *)cnode;
	group = (GaimGroup *)gnode;

	/* Delete any buddy icon. */
	gaim_buddy_icon_uncache(buddy);

	/* Remove the node from its parent */
	if (node->prev)
		node->prev->next = node->next;
	if (node->next)
		node->next->prev = node->prev;
	if (cnode->child == node)
		cnode->child = node->next;

	/* Adjust size counts */
	if (GAIM_BUDDY_IS_ONLINE(buddy)) {
		contact->online--;
		if (contact->online == 0)
			group->online--;
	}
	if (gaim_account_is_connected(buddy->account)) {
		contact->currentsize--;
		if (contact->currentsize == 0)
			group->currentsize--;
	}
	contact->totalsize--;

	gaim_blist_schedule_save();

	/* Re-sort the contact */
	if (cnode->child && contact->priority == buddy) {
		gaim_contact_invalidate_priority_buddy(contact);
		if (ops && ops->update)
			ops->update(gaimbuddylist, cnode);
	}

	/* Remove this buddy from the buddies hash table */
	hb.name = g_strdup(gaim_normalize(buddy->account, buddy->name));
	hb.account = buddy->account;
	hb.group = ((GaimBlistNode*)buddy)->parent->parent;
	g_hash_table_remove(gaimbuddylist->buddies, &hb);
	g_free(hb.name);

	/* Update the UI */
	if (ops && ops->remove)
		ops->remove(gaimbuddylist, node);

	/* Signal that the buddy has been removed before freeing the memory for it */
	gaim_signal_emit(gaim_blist_get_handle(), "buddy-removed", buddy);

	/* Delete the node */
	if (buddy->icon != NULL)
		gaim_buddy_icon_unref(buddy->icon);
	g_hash_table_destroy(buddy->node.settings);
	gaim_presence_remove_buddy(buddy->presence, buddy);
	gaim_presence_destroy(buddy->presence);
	g_free(buddy->name);
	g_free(buddy->alias);
	g_free(buddy->server_alias);

	GAIM_DBUS_UNREGISTER_POINTER(buddy);
	g_free(buddy);

	/* FIXME: Once GaimBuddy is a GObject, timeout callbacks can
	 * g_object_ref() it when connecting the callback and
	 * g_object_unref() it in the handler.  That way, it won't
	 * get freed while the timeout is pending and this line can
	 * be removed. */
	while (g_source_remove_by_user_data((gpointer *)buddy));

	/* If the contact is empty then remove it */
	if (!cnode->child)
		gaim_blist_remove_contact(contact);
}

void gaim_blist_remove_chat(GaimChat *chat)
{
	GaimBlistUiOps *ops = gaim_blist_get_ui_ops();
	GaimBlistNode *node, *gnode;
	GaimGroup *group;

	g_return_if_fail(chat != NULL);

	node = (GaimBlistNode *)chat;
	gnode = node->parent;
	group = (GaimGroup *)gnode;

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
		if (gaim_account_is_connected(chat->account)) {
			group->online--;
			group->currentsize--;
		}
		group->totalsize--;

		gaim_blist_schedule_save();
	}

	/* Update the UI */
	if (ops && ops->remove)
		ops->remove(gaimbuddylist, node);

	/* Delete the node */
	g_hash_table_destroy(chat->components);
	g_hash_table_destroy(chat->node.settings);
	g_free(chat->alias);
	GAIM_DBUS_UNREGISTER_POINTER(chat);
	g_free(chat);
}

void gaim_blist_remove_group(GaimGroup *group)
{
	GaimBlistUiOps *ops = gaim_blist_get_ui_ops();
	GaimBlistNode *node;
	GList *l;

	g_return_if_fail(group != NULL);

	node = (GaimBlistNode *)group;

	/* Make sure the group is empty */
	if (node->child) {
		char *buf;
		int count = 0;
		GaimBlistNode *child;

		for (child = node->child; child != NULL; child = child->next)
			count++;

		buf = g_strdup_printf(ngettext("%d buddy from group %s was not removed "
									   "because it belongs to an account which is "
									   "disabled or offline.  This buddy and the "
									   "group were not removed.\n",
									   "%d buddies from group %s were not "
									   "removed because they belong to accounts "
									   "which are currently disabled or offline.  "
									   "These buddies and the group were not "
									   "removed.\n", count),
							  count, group->name);
		gaim_notify_error(NULL, NULL, _("Group not removed"), buf);
		g_free(buf);
		return;
	}

	/* Remove the node from its parent */
	if (gaimbuddylist->root == node)
		gaimbuddylist->root = node->next;
	if (node->prev)
		node->prev->next = node->next;
	if (node->next)
		node->next->prev = node->prev;

	gaim_blist_schedule_save();

	/* Update the UI */
	if (ops && ops->remove)
		ops->remove(gaimbuddylist, node);

	/* Remove the group from all accounts that are online */
	for (l = gaim_connections_get_all(); l != NULL; l = l->next)
	{
		GaimConnection *gc = (GaimConnection *)l->data;

		if (gaim_connection_get_state(gc) == GAIM_CONNECTED)
			gaim_account_remove_group(gaim_connection_get_account(gc), group);
	}

	/* Delete the node */
	g_hash_table_destroy(group->node.settings);
	g_free(group->name);
	GAIM_DBUS_UNREGISTER_POINTER(group);
	g_free(group);
}

GaimBuddy *gaim_contact_get_priority_buddy(GaimContact *contact)
{
	g_return_val_if_fail(contact != NULL, NULL);

	if (!contact->priority_valid)
		gaim_contact_compute_priority_buddy(contact);

	return contact->priority;
}

const char *gaim_buddy_get_alias_only(GaimBuddy *buddy)
{
	g_return_val_if_fail(buddy != NULL, NULL);

	if ((buddy->alias != NULL) && (*buddy->alias != '\0')) {
		return buddy->alias;
	} else if ((buddy->server_alias != NULL) &&
		   (*buddy->server_alias != '\0')) {

		return buddy->server_alias;
	}

	return NULL;
}


const char *gaim_buddy_get_contact_alias(GaimBuddy *buddy)
{
	GaimContact *c;

	g_return_val_if_fail(buddy != NULL, NULL);

	/* Search for an alias for the buddy. In order of precedence: */
	/* The buddy alias */
	if (buddy->alias != NULL)
		return buddy->alias;

	/* The contact alias */
	c = gaim_buddy_get_contact(buddy);
	if ((c != NULL) && (c->alias != NULL))
		return c->alias;

	/* The server alias */
	if ((buddy->server_alias) && (*buddy->server_alias))
		return buddy->server_alias;

	/* The buddy's user name (i.e. no alias) */
	return buddy->name;
}


const char *gaim_buddy_get_alias(GaimBuddy *buddy)
{
	g_return_val_if_fail(buddy != NULL, NULL);

	/* Search for an alias for the buddy. In order of precedence: */
	/* The buddy alias */
	if (buddy->alias != NULL)
		return buddy->alias;

	/* The server alias */
	if ((buddy->server_alias) && (*buddy->server_alias))
		return buddy->server_alias;

	/* The buddy's user name (i.e. no alias) */
	return buddy->name;
}

const char *gaim_buddy_get_server_alias(GaimBuddy *buddy)
{
        g_return_val_if_fail(buddy != NULL, NULL);

	if ((buddy->server_alias) && (*buddy->server_alias))
	    return buddy->server_alias;

	return NULL;
}

const char *gaim_buddy_get_local_alias(GaimBuddy *buddy)
{
	GaimContact *c;

	g_return_val_if_fail(buddy != NULL, NULL);

	/* Search for an alias for the buddy. In order of precedence: */
	/* The buddy alias */
	if (buddy->alias != NULL)
		return buddy->alias;

	/* The contact alias */
	c = gaim_buddy_get_contact(buddy);
	if ((c != NULL) && (c->alias != NULL))
		return c->alias;

	/* The buddy's user name (i.e. no alias) */
	return buddy->name;
}

const char *gaim_chat_get_name(GaimChat *chat)
{
	struct proto_chat_entry *pce;
	GList *parts;
	char *ret;

	g_return_val_if_fail(chat != NULL, NULL);

	if ((chat->alias != NULL) && (*chat->alias != '\0'))
		return chat->alias;

	parts = GAIM_PLUGIN_PROTOCOL_INFO(chat->account->gc->prpl)->chat_info(chat->account->gc);
	pce = parts->data;
	ret = g_hash_table_lookup(chat->components, pce->identifier);
	g_list_foreach(parts, (GFunc)g_free, NULL);
	g_list_free(parts);

	return ret;
}

GaimBuddy *gaim_find_buddy(GaimAccount *account, const char *name)
{
	GaimBuddy *buddy;
	struct _gaim_hbuddy hb;
	GaimBlistNode *group;

	g_return_val_if_fail(gaimbuddylist != NULL, NULL);
	g_return_val_if_fail(account != NULL, NULL);
	g_return_val_if_fail((name != NULL) && (*name != '\0'), NULL);

	hb.account = account;
	hb.name = g_strdup(gaim_normalize(account, name));

	for (group = gaimbuddylist->root; group; group = group->next) {
		hb.group = group;
		if ((buddy = g_hash_table_lookup(gaimbuddylist->buddies, &hb))) {
			g_free(hb.name);
			return buddy;
		}
	}
	g_free(hb.name);

	return NULL;
}

GaimBuddy *gaim_find_buddy_in_group(GaimAccount *account, const char *name,
		GaimGroup *group)
{
	struct _gaim_hbuddy hb;
	GaimBuddy *ret;

	g_return_val_if_fail(gaimbuddylist != NULL, NULL);
	g_return_val_if_fail(account != NULL, NULL);
	g_return_val_if_fail((name != NULL) && (*name != '\0'), NULL);

	hb.name = g_strdup(gaim_normalize(account, name));
	hb.account = account;
	hb.group = (GaimBlistNode*)group;

	ret = g_hash_table_lookup(gaimbuddylist->buddies, &hb);
	g_free(hb.name);

	return ret;
}

static void find_acct_buddies(gpointer key, gpointer value, gpointer data)
{
	struct _gaim_hbuddy *hb = key;
	GaimBuddy *buddy = value;
	struct _list_account_buddies *ab = data;

	if (hb->account == ab->account) {
		ab->list = g_slist_prepend(ab->list, buddy);
	}
}

GSList *gaim_find_buddies(GaimAccount *account, const char *name)
{
	GaimBuddy *buddy;
	GaimBlistNode *node;
	GSList *ret = NULL;

	g_return_val_if_fail(gaimbuddylist != NULL, NULL);
	g_return_val_if_fail(account != NULL, NULL);


	if ((name != NULL) && (*name != '\0')) {
		struct _gaim_hbuddy hb;

		hb.name = g_strdup(gaim_normalize(account, name));
		hb.account = account;

		for (node = gaimbuddylist->root; node != NULL; node = node->next) {
			hb.group = node;
			if ((buddy = g_hash_table_lookup(gaimbuddylist->buddies, &hb)) != NULL)
				ret = g_slist_prepend(ret, buddy);
		}
		g_free(hb.name);
	} else {
		struct _list_account_buddies *ab = g_new0(struct _list_account_buddies, 1);
		ab->account = account;
		g_hash_table_foreach(gaimbuddylist->buddies, find_acct_buddies, ab);
		ret = ab->list;
		g_free(ab);
	}

	return ret;
}

GaimGroup *gaim_find_group(const char *name)
{
	GaimBlistNode *node;

	g_return_val_if_fail(gaimbuddylist != NULL, NULL);
	g_return_val_if_fail((name != NULL) && (*name != '\0'), NULL);

	for (node = gaimbuddylist->root; node != NULL; node = node->next) {
		if (!strcmp(((GaimGroup *)node)->name, name))
			return (GaimGroup *)node;
	}

	return NULL;
}

GaimChat *
gaim_blist_find_chat(GaimAccount *account, const char *name)
{
	char *chat_name;
	GaimChat *chat;
	GaimPlugin *prpl;
	GaimPluginProtocolInfo *prpl_info = NULL;
	struct proto_chat_entry *pce;
	GaimBlistNode *node, *group;
	GList *parts;

	g_return_val_if_fail(gaimbuddylist != NULL, NULL);
	g_return_val_if_fail((name != NULL) && (*name != '\0'), NULL);

	if (!gaim_account_is_connected(account))
		return NULL;

	prpl = gaim_find_prpl(gaim_account_get_protocol_id(account));
	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);

	if (prpl_info->find_blist_chat != NULL)
		return prpl_info->find_blist_chat(account, name);

	for (group = gaimbuddylist->root; group != NULL; group = group->next) {
		for (node = group->child; node != NULL; node = node->next) {
			if (GAIM_BLIST_NODE_IS_CHAT(node)) {

				chat = (GaimChat*)node;

				if (account != chat->account)
					continue;

				parts = prpl_info->chat_info(
					gaim_account_get_connection(chat->account));

				pce = parts->data;
				chat_name = g_hash_table_lookup(chat->components,
												pce->identifier);

				if (chat->account == account && chat_name != NULL &&
					name != NULL && !strcmp(chat_name, name)) {

					return chat;
				}
			}
		}
	}

	return NULL;
}

GaimGroup *
gaim_chat_get_group(GaimChat *chat)
{
	g_return_val_if_fail(chat != NULL, NULL);

	return (GaimGroup *)(((GaimBlistNode *)chat)->parent);
}

GaimContact *gaim_buddy_get_contact(GaimBuddy *buddy)
{
	g_return_val_if_fail(buddy != NULL, NULL);

	return (GaimContact*)((GaimBlistNode*)buddy)->parent;
}

GaimPresence *gaim_buddy_get_presence(const GaimBuddy *buddy)
{
	g_return_val_if_fail(buddy != NULL, NULL);
	return buddy->presence;
}

GaimGroup *gaim_buddy_get_group(GaimBuddy *buddy)
{
	g_return_val_if_fail(buddy != NULL, NULL);

	if (((GaimBlistNode *)buddy)->parent == NULL)
		return NULL;

	return (GaimGroup *)(((GaimBlistNode*)buddy)->parent->parent);
}

GSList *gaim_group_get_accounts(GaimGroup *group)
{
	GSList *l = NULL;
	GaimBlistNode *gnode, *cnode, *bnode;

	gnode = (GaimBlistNode *)group;

	for (cnode = gnode->child;  cnode; cnode = cnode->next) {
		if (GAIM_BLIST_NODE_IS_CHAT(cnode)) {
			if (!g_slist_find(l, ((GaimChat *)cnode)->account))
				l = g_slist_append(l, ((GaimChat *)cnode)->account);
		} else if (GAIM_BLIST_NODE_IS_CONTACT(cnode)) {
			for (bnode = cnode->child; bnode; bnode = bnode->next) {
				if (GAIM_BLIST_NODE_IS_BUDDY(bnode)) {
					if (!g_slist_find(l, ((GaimBuddy *)bnode)->account))
						l = g_slist_append(l, ((GaimBuddy *)bnode)->account);
				}
			}
		}
	}

	return l;
}

void gaim_blist_add_account(GaimAccount *account)
{
	GaimBlistUiOps *ops = gaim_blist_get_ui_ops();
	GaimBlistNode *gnode, *cnode, *bnode;

	g_return_if_fail(gaimbuddylist != NULL);

	if (!ops || !ops->update)
		return;

	for (gnode = gaimbuddylist->root; gnode; gnode = gnode->next) {
		if (!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;
		for (cnode = gnode->child; cnode; cnode = cnode->next) {
			if (GAIM_BLIST_NODE_IS_CONTACT(cnode)) {
				gboolean recompute = FALSE;
					for (bnode = cnode->child; bnode; bnode = bnode->next) {
						if (GAIM_BLIST_NODE_IS_BUDDY(bnode) &&
								((GaimBuddy*)bnode)->account == account) {
							recompute = TRUE;
							((GaimContact*)cnode)->currentsize++;
							if (((GaimContact*)cnode)->currentsize == 1)
								((GaimGroup*)gnode)->currentsize++;
							ops->update(gaimbuddylist, bnode);
						}
					}
					if (recompute ||
							gaim_blist_node_get_bool(cnode, "show_offline")) {
						gaim_contact_invalidate_priority_buddy((GaimContact*)cnode);
						ops->update(gaimbuddylist, cnode);
					}
			} else if (GAIM_BLIST_NODE_IS_CHAT(cnode) &&
					((GaimChat*)cnode)->account == account) {
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
	GaimBlistUiOps *ops = gaim_blist_get_ui_ops();
	GaimBlistNode *gnode, *cnode, *bnode;
	GaimBuddy *buddy;
	GaimChat *chat;
	GaimContact *contact;
	GaimGroup *group;
	GList *list = NULL, *iter = NULL;

	g_return_if_fail(gaimbuddylist != NULL);

	for (gnode = gaimbuddylist->root; gnode; gnode = gnode->next) {
		if (!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;

		group = (GaimGroup *)gnode;

		for (cnode = gnode->child; cnode; cnode = cnode->next) {
			if (GAIM_BLIST_NODE_IS_CONTACT(cnode)) {
				gboolean recompute = FALSE;
				contact = (GaimContact *)cnode;

				for (bnode = cnode->child; bnode; bnode = bnode->next) {
					if (!GAIM_BLIST_NODE_IS_BUDDY(bnode))
						continue;

					buddy = (GaimBuddy *)bnode;
					if (account == buddy->account) {
						GaimPresence *presence;
						recompute = TRUE;

						presence = gaim_buddy_get_presence(buddy);

						if(gaim_presence_is_online(presence)) {
							contact->online--;
							if (contact->online == 0)
								group->online--;

							gaim_blist_node_set_int(&buddy->node,
													"last_seen", time(NULL));
						}

						contact->currentsize--;
						if (contact->currentsize == 0)
							group->currentsize--;

						if (!g_list_find(list, presence))
							list = g_list_prepend(list, presence);

						if (ops && ops->remove)
							ops->remove(gaimbuddylist, bnode);
					}
				}
				if (recompute) {
					gaim_contact_invalidate_priority_buddy(contact);
					if (ops && ops->update)
						ops->update(gaimbuddylist, cnode);
				}
			} else if (GAIM_BLIST_NODE_IS_CHAT(cnode)) {
				chat = (GaimChat *)cnode;

				if(chat->account == account) {
					group->currentsize--;
					group->online--;

					if (ops && ops->remove)
						ops->remove(gaimbuddylist, cnode);
				}
			}
		}
	}

	for (iter = list; iter; iter = iter->next)
	{
		gaim_presence_set_status_active(iter->data, "offline", TRUE);
	}
	g_list_free(list);
}

gboolean gaim_group_on_account(GaimGroup *g, GaimAccount *account)
{
	GaimBlistNode *cnode;
	for (cnode = ((GaimBlistNode *)g)->child; cnode; cnode = cnode->next) {
		if (GAIM_BLIST_NODE_IS_CONTACT(cnode)) {
			if(gaim_contact_on_account((GaimContact *) cnode, account))
				return TRUE;
		} else if (GAIM_BLIST_NODE_IS_CHAT(cnode)) {
			GaimChat *chat = (GaimChat *)cnode;
			if ((!account && gaim_account_is_connected(chat->account))
					|| chat->account == account)
				return TRUE;
		}
	}
	return FALSE;
}

void
gaim_blist_request_add_buddy(GaimAccount *account, const char *username,
							 const char *group, const char *alias)
{
	GaimBlistUiOps *ui_ops;

	ui_ops = gaim_blist_get_ui_ops();

	if (ui_ops != NULL && ui_ops->request_add_buddy != NULL)
		ui_ops->request_add_buddy(account, username, group, alias);
}

void
gaim_blist_request_add_chat(GaimAccount *account, GaimGroup *group,
							const char *alias, const char *name)
{
	GaimBlistUiOps *ui_ops;

	ui_ops = gaim_blist_get_ui_ops();

	if (ui_ops != NULL && ui_ops->request_add_chat != NULL)
		ui_ops->request_add_chat(account, group, alias, name);
}

void
gaim_blist_request_add_group(void)
{
	GaimBlistUiOps *ui_ops;

	ui_ops = gaim_blist_get_ui_ops();

	if (ui_ops != NULL && ui_ops->request_add_group != NULL)
		ui_ops->request_add_group();
}

static void
gaim_blist_node_setting_free(gpointer data)
{
	GaimValue *value;

	value = (GaimValue *)data;

	gaim_value_destroy(value);
}

static void gaim_blist_node_initialize_settings(GaimBlistNode *node)
{
	if (node->settings)
		return;

	node->settings = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
			(GDestroyNotify)gaim_blist_node_setting_free);
}

void gaim_blist_node_remove_setting(GaimBlistNode *node, const char *key)
{
	g_return_if_fail(node != NULL);
	g_return_if_fail(node->settings != NULL);
	g_return_if_fail(key != NULL);

	g_hash_table_remove(node->settings, key);

	gaim_blist_schedule_save();
}

void
gaim_blist_node_set_flags(GaimBlistNode *node, GaimBlistNodeFlags flags)
{
	g_return_if_fail(node != NULL);

	node->flags = flags;
}

GaimBlistNodeFlags
gaim_blist_node_get_flags(GaimBlistNode *node)
{
	g_return_val_if_fail(node != NULL, 0);

	return node->flags;
}

void
gaim_blist_node_set_bool(GaimBlistNode* node, const char *key, gboolean data)
{
	GaimValue *value;

	g_return_if_fail(node != NULL);
	g_return_if_fail(node->settings != NULL);
	g_return_if_fail(key != NULL);

	value = gaim_value_new(GAIM_TYPE_BOOLEAN);
	gaim_value_set_boolean(value, data);

	g_hash_table_replace(node->settings, g_strdup(key), value);

	gaim_blist_schedule_save();
}

gboolean
gaim_blist_node_get_bool(GaimBlistNode* node, const char *key)
{
	GaimValue *value;

	g_return_val_if_fail(node != NULL, FALSE);
	g_return_val_if_fail(node->settings != NULL, FALSE);
	g_return_val_if_fail(key != NULL, FALSE);

	value = g_hash_table_lookup(node->settings, key);

	if (value == NULL)
		return FALSE;

	g_return_val_if_fail(gaim_value_get_type(value) == GAIM_TYPE_BOOLEAN, FALSE);

	return gaim_value_get_boolean(value);
}

void
gaim_blist_node_set_int(GaimBlistNode* node, const char *key, int data)
{
	GaimValue *value;

	g_return_if_fail(node != NULL);
	g_return_if_fail(node->settings != NULL);
	g_return_if_fail(key != NULL);

	value = gaim_value_new(GAIM_TYPE_INT);
	gaim_value_set_int(value, data);

	g_hash_table_replace(node->settings, g_strdup(key), value);

	gaim_blist_schedule_save();
}

int
gaim_blist_node_get_int(GaimBlistNode* node, const char *key)
{
	GaimValue *value;

	g_return_val_if_fail(node != NULL, 0);
	g_return_val_if_fail(node->settings != NULL, 0);
	g_return_val_if_fail(key != NULL, 0);

	value = g_hash_table_lookup(node->settings, key);

	if (value == NULL)
		return 0;

	g_return_val_if_fail(gaim_value_get_type(value) == GAIM_TYPE_INT, 0);

	return gaim_value_get_int(value);
}

void
gaim_blist_node_set_string(GaimBlistNode* node, const char *key, const char *data)
{
	GaimValue *value;

	g_return_if_fail(node != NULL);
	g_return_if_fail(node->settings != NULL);
	g_return_if_fail(key != NULL);

	value = gaim_value_new(GAIM_TYPE_STRING);
	gaim_value_set_string(value, data);

	g_hash_table_replace(node->settings, g_strdup(key), value);

	gaim_blist_schedule_save();
}

const char *
gaim_blist_node_get_string(GaimBlistNode* node, const char *key)
{
	GaimValue *value;

	g_return_val_if_fail(node != NULL, NULL);
	g_return_val_if_fail(node->settings != NULL, NULL);
	g_return_val_if_fail(key != NULL, NULL);

	value = g_hash_table_lookup(node->settings, key);

	if (value == NULL)
		return NULL;

	g_return_val_if_fail(gaim_value_get_type(value) == GAIM_TYPE_STRING, NULL);

	return gaim_value_get_string(value);
}

GList *
gaim_blist_node_get_extended_menu(GaimBlistNode *n)
{
	GList *menu = NULL;

	g_return_val_if_fail(n != NULL, NULL);

	gaim_signal_emit(gaim_blist_get_handle(),
			"blist-node-extended-menu",
			n, &menu);
	return menu;
}

int gaim_blist_get_group_size(GaimGroup *group, gboolean offline)
{
	if (!group)
		return 0;

	return offline ? group->totalsize : group->currentsize;
}

int gaim_blist_get_group_online_count(GaimGroup *group)
{
	if (!group)
		return 0;

	return group->online;
}

void
gaim_blist_set_ui_ops(GaimBlistUiOps *ops)
{
	blist_ui_ops = ops;
}

GaimBlistUiOps *
gaim_blist_get_ui_ops(void)
{
	return blist_ui_ops;
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

	gaim_signal_register(handle, "buddy-status-changed",
	                     gaim_marshal_VOID__POINTER_POINTER_POINTER, NULL,
	                     3,
	                     gaim_value_new(GAIM_TYPE_SUBTYPE,
	                                    GAIM_SUBTYPE_BLIST_BUDDY),
	                     gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_STATUS),
	                     gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_STATUS));
	gaim_signal_register(handle, "buddy-privacy-changed",
	                     gaim_marshal_VOID__POINTER, NULL,
	                     1,
	                     gaim_value_new(GAIM_TYPE_SUBTYPE,
	                                    GAIM_SUBTYPE_BLIST_BUDDY));

	gaim_signal_register(handle, "buddy-idle-changed",
	                     gaim_marshal_VOID__POINTER_INT_INT, NULL,
	                     3,
	                     gaim_value_new(GAIM_TYPE_SUBTYPE,
	                                    GAIM_SUBTYPE_BLIST_BUDDY),
	                     gaim_value_new(GAIM_TYPE_INT),
	                     gaim_value_new(GAIM_TYPE_INT));


	gaim_signal_register(handle, "buddy-signed-on",
						 gaim_marshal_VOID__POINTER, NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_BLIST_BUDDY));

	gaim_signal_register(handle, "buddy-signed-off",
						 gaim_marshal_VOID__POINTER, NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_BLIST_BUDDY));

	gaim_signal_register(handle, "buddy-got-login-time",
						 gaim_marshal_VOID__POINTER, NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_BLIST_BUDDY));

	gaim_signal_register(handle, "buddy-added",
						 gaim_marshal_VOID__POINTER, NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_BLIST_BUDDY));

	gaim_signal_register(handle, "buddy-removed",
						 gaim_marshal_VOID__POINTER, NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_BLIST_BUDDY));

	gaim_signal_register(handle, "buddy-icon-changed",
						 gaim_marshal_VOID__POINTER, NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_BLIST_BUDDY));

	gaim_signal_register(handle, "update-idle", gaim_marshal_VOID, NULL, 0);

	gaim_signal_register(handle, "blist-node-extended-menu",
			     gaim_marshal_VOID__POINTER_POINTER, NULL, 2,
			     gaim_value_new(GAIM_TYPE_SUBTYPE,
					    GAIM_SUBTYPE_BLIST_NODE),
			     gaim_value_new(GAIM_TYPE_BOXED, "GList **"));

	gaim_signal_register(handle, "blist-node-aliased",
						 gaim_marshal_VOID__POINTER_POINTER, NULL, 2,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_BLIST_NODE),
						 gaim_value_new(GAIM_TYPE_STRING));
}

void
gaim_blist_uninit(void)
{
	if (save_timer != 0)
	{
		gaim_timeout_remove(save_timer);
		save_timer = 0;
		gaim_blist_sync();
	}

	gaim_signals_unregister_by_instance(gaim_blist_get_handle());
}
