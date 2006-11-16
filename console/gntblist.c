/**
 * @file gntblist.c GNT BuddyList API
 * @ingroup gntui
 *
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
 */
#include <account.h>
#include <blist.h>
#include <notify.h>
#include <request.h>
#include <savedstatuses.h>
#include <server.h>
#include <signal.h>
#include <status.h>
#include <util.h>
#include "debug.h"

#include "gntgaim.h"
#include "gntbox.h"
#include "gntcombobox.h"
#include "gntentry.h"
#include "gntlabel.h"
#include "gntline.h"
#include "gntmenu.h"
#include "gntmenuitem.h"
#include "gntmenuitemcheck.h"
#include "gnttree.h"
#include "gntutils.h"
#include "gntwindow.h"

#include "gntblist.h"
#include "gntconv.h"
#include "gntstatus.h"
#include <string.h>

#define PREF_ROOT "/gaim/gnt/blist"
#define TYPING_TIMEOUT 4000

typedef struct
{
	GntWidget *window;
	GntWidget *tree;

	GntWidget *tooltip;
	GaimBlistNode *tnode;		/* Who is the tooltip being displayed for? */
	GList *tagged;          /* A list of tagged blistnodes */

	GntWidget *context;
	GaimBlistNode *cnode;

	/* XXX: I am KISSing */
	GntWidget *status;          /* Dropdown with the statuses  */
	GntWidget *statustext;      /* Status message */
	int typing;

	GntWidget *menu;
	/* These are the menuitems that get regenerated */
	GntMenuItem *accounts;
	GntMenuItem *plugins;
} GGBlist;

typedef enum
{
	STATUS_PRIMITIVE = 0,
	STATUS_SAVED_POPULAR,
	STATUS_SAVED_ALL,
	STATUS_SAVED_NEW
} StatusType;

typedef struct
{
	StatusType type;
	union
	{
		GaimStatusPrimitive prim;
		GaimSavedStatus *saved;
	} u;
} StatusBoxItem;

GGBlist *ggblist;

static void add_buddy(GaimBuddy *buddy, GGBlist *ggblist);
static void add_contact(GaimContact *contact, GGBlist *ggblist);
static void add_group(GaimGroup *group, GGBlist *ggblist);
static void add_chat(GaimChat *chat, GGBlist *ggblist);
static void add_node(GaimBlistNode *node, GGBlist *ggblist);
static void draw_tooltip(GGBlist *ggblist);
static gboolean remove_typing_cb(gpointer null);
static void remove_peripherals(GGBlist *ggblist);
static const char * get_display_name(GaimBlistNode *node);
static void savedstatus_changed(GaimSavedStatus *now, GaimSavedStatus *old);
static void blist_show(GaimBuddyList *list);
static void update_buddy_display(GaimBuddy *buddy, GGBlist *ggblist);

/* Sort functions */
static int blist_node_compare_text(GaimBlistNode *n1, GaimBlistNode *n2);
static int blist_node_compare_status(GaimBlistNode *n1, GaimBlistNode *n2);
static int blist_node_compare_log(GaimBlistNode *n1, GaimBlistNode *n2);

static gboolean
is_contact_online(GaimContact *contact)
{
	GaimBlistNode *node;
	for (node = ((GaimBlistNode*)contact)->child; node; node = node->next) {
		if (GAIM_BUDDY_IS_ONLINE((GaimBuddy*)node))
			return TRUE;
	}
	return FALSE;
}

static gboolean
is_group_online(GaimGroup *group)
{
	GaimBlistNode *node;
	for (node = ((GaimBlistNode*)group)->child; node; node = node->next) {
		if (GAIM_BLIST_NODE_IS_CHAT(node))
			return TRUE;
		else if (is_contact_online((GaimContact*)node))
			return TRUE;
	}
	return FALSE;
}

static void
new_node(GaimBlistNode *node)
{
}

static void add_node(GaimBlistNode *node, GGBlist *ggblist)
{
	if (GAIM_BLIST_NODE_IS_BUDDY(node))
		add_buddy((GaimBuddy*)node, ggblist);
	else if (GAIM_BLIST_NODE_IS_CONTACT(node))
		add_contact((GaimContact*)node, ggblist);
	else if (GAIM_BLIST_NODE_IS_GROUP(node))
		add_group((GaimGroup*)node, ggblist);
	else if (GAIM_BLIST_NODE_IS_CHAT(node))
		add_chat((GaimChat *)node, ggblist);
	draw_tooltip(ggblist);
}

static void
remove_tooltip(GGBlist *ggblist)
{
	gnt_widget_destroy(ggblist->tooltip);
	ggblist->tooltip = NULL;
	ggblist->tnode = NULL;
}

static void
node_remove(GaimBuddyList *list, GaimBlistNode *node)
{
	GGBlist *ggblist = list->ui_data;

	if (ggblist == NULL || node->ui_data == NULL)
		return;

	gnt_tree_remove(GNT_TREE(ggblist->tree), node);
	node->ui_data = NULL;
	if (ggblist->tagged)
		ggblist->tagged = g_list_remove(ggblist->tagged, node);

	if (GAIM_BLIST_NODE_IS_BUDDY(node)) {
		GaimContact *contact = (GaimContact*)node->parent;
		if ((!gaim_prefs_get_bool(PREF_ROOT "/showoffline") && !is_contact_online(contact)) ||
				contact->currentsize < 1)
			node_remove(list, (GaimBlistNode*)contact);
	} else if (GAIM_BLIST_NODE_IS_CONTACT(node)) {
		GaimGroup *group = (GaimGroup*)node->parent;
		if ((!gaim_prefs_get_bool(PREF_ROOT "/showoffline") && !is_group_online(group)) ||
				group->currentsize < 1)
			node_remove(list, node->parent);
		for (node = node->child; node; node = node->next)
			node->ui_data = NULL;
	}

	draw_tooltip(ggblist);
}

static void
node_update(GaimBuddyList *list, GaimBlistNode *node)
{
	/* It really looks like this should never happen ... but it does.
           This will at least emit a warning to the log when it
           happens, so maybe someone will figure it out. */
	g_return_if_fail(node != NULL);

	if (list->ui_data == NULL)
		return;   /* XXX: this is probably the place to auto-join chats */

	if (node->ui_data != NULL) {
		gnt_tree_change_text(GNT_TREE(ggblist->tree), node,
				0, get_display_name(node));
		gnt_tree_sort_row(GNT_TREE(ggblist->tree), node);
	}

	if (GAIM_BLIST_NODE_IS_BUDDY(node)) {
		GaimBuddy *buddy = (GaimBuddy*)node;
		if (gaim_account_is_connected(buddy->account) &&
				(GAIM_BUDDY_IS_ONLINE(buddy) || gaim_prefs_get_bool(PREF_ROOT "/showoffline")))
			add_node((GaimBlistNode*)buddy, list->ui_data);
		else
			node_remove(gaim_get_blist(), node);

		node_update(list, node->parent);
	} else if (GAIM_BLIST_NODE_IS_CHAT(node)) {
		add_chat((GaimChat *)node, list->ui_data);
	} else if (GAIM_BLIST_NODE_IS_CONTACT(node)) {
		GaimContact *contact = (GaimContact*)node;
		if ((!gaim_prefs_get_bool(PREF_ROOT "/showoffline") && !is_contact_online(contact)) ||
				contact->currentsize < 1)
			node_remove(gaim_get_blist(), node);
		else
			add_node(node, list->ui_data);
	} else if (GAIM_BLIST_NODE_IS_GROUP(node)) {
		GaimGroup *group = (GaimGroup*)node;
		if ((!gaim_prefs_get_bool(PREF_ROOT "/showoffline") && !is_group_online(group)) ||
				group->currentsize < 1)
			node_remove(list, node);
	}
}

static void
new_list(GaimBuddyList *list)
{
	if (ggblist)
		return;

	ggblist = g_new0(GGBlist, 1);
	list->ui_data = ggblist;
}

static void
add_buddy_cb(void *data, GaimRequestFields *allfields)
{
	const char *username = gaim_request_fields_get_string(allfields, "screenname");
	const char *alias = gaim_request_fields_get_string(allfields, "alias");
	const char *group = gaim_request_fields_get_string(allfields, "group");
	GaimAccount *account = gaim_request_fields_get_account(allfields, "account");
	const char *error = NULL;
	GaimGroup *grp;
	GaimBuddy *buddy;

	if (!username)
		error = _("You must provide a screename for the buddy.");
	else if (!group)
		error = _("You must provide a group.");
	else if (!account)
		error = _("You must select an account.");

	if (error)
	{
		gaim_notify_error(NULL, _("Error"), _("Error adding buddy"), error);
		return;
	}

	grp = gaim_find_group(group);
	if (!grp)
	{
		grp = gaim_group_new(group);
		gaim_blist_add_group(grp, NULL);
	}

	buddy = gaim_buddy_new(account, username, alias);
	gaim_blist_add_buddy(buddy, NULL, grp, NULL);
	gaim_account_add_buddy(account, buddy);
}

static void
gg_request_add_buddy(GaimAccount *account, const char *username, const char *grp, const char *alias)
{
	GaimRequestFields *fields = gaim_request_fields_new();
	GaimRequestFieldGroup *group = gaim_request_field_group_new(NULL);
	GaimRequestField *field;

	gaim_request_fields_add_group(fields, group);

	field = gaim_request_field_string_new("screenname", _("Screen Name"), username, FALSE);
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_string_new("alias", _("Alias"), alias, FALSE);
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_string_new("group", _("Group"), grp, FALSE);
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_account_new("account", _("Account"), NULL);
	gaim_request_field_account_set_show_all(field, FALSE);
	if (account)
		gaim_request_field_account_set_value(field, account);
	gaim_request_field_group_add_field(group, field);

	gaim_request_fields(NULL, _("Add Buddy"), NULL, _("Please enter buddy information."),
			fields, _("Add"), G_CALLBACK(add_buddy_cb), _("Cancel"), NULL, NULL);
}

static void
add_chat_cb(void *data, GaimRequestFields *allfields)
{
	GaimAccount *account;
	const char *alias, *name, *group;
	GaimChat *chat;
	GaimGroup *grp;
	GHashTable *hash = NULL;
	GaimConnection *gc;

	account = gaim_request_fields_get_account(allfields, "account");
	name = gaim_request_fields_get_string(allfields, "name");
	alias = gaim_request_fields_get_string(allfields, "alias");
	group = gaim_request_fields_get_string(allfields, "group");

	if (!gaim_account_is_connected(account) || !name || !*name)
		return;
	
	if (!group || !*group)
		group = _("Chats");

	gc = gaim_account_get_connection(account);

	if (GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl)->chat_info_defaults != NULL)
		hash = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl)->chat_info_defaults(gc, name);
	
	chat = gaim_chat_new(account, name, hash);

	if (chat != NULL) {
		if ((grp = gaim_find_group(group)) == NULL) {
			grp = gaim_group_new(group);
			gaim_blist_add_group(grp, NULL);
		}
		gaim_blist_add_chat(chat, grp, NULL);
		gaim_blist_alias_chat(chat, alias);
	}
}

static void
gg_request_add_chat(GaimAccount *account, GaimGroup *grp, const char *alias, const char *name)
{
	GaimRequestFields *fields = gaim_request_fields_new();
	GaimRequestFieldGroup *group = gaim_request_field_group_new(NULL);
	GaimRequestField *field;

	gaim_request_fields_add_group(fields, group);

	field = gaim_request_field_account_new("account", _("Account"), NULL);
	gaim_request_field_account_set_show_all(field, FALSE);
	if (account)
		gaim_request_field_account_set_value(field, account);
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_string_new("name", _("Name"), name, FALSE);
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_string_new("alias", _("Alias"), alias, FALSE);
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_string_new("group", _("Group"), grp ? grp->name : NULL, FALSE);
	gaim_request_field_group_add_field(group, field);

	gaim_request_fields(NULL, _("Add Chat"), NULL,
			_("You can edit more information from the context menu later."),
			fields, _("Add"), G_CALLBACK(add_chat_cb), _("Cancel"), NULL, NULL);
}

static void
add_group_cb(gpointer null, const char *group)
{
	GaimGroup *grp;

	if (!group || !*group)
	{
		gaim_notify_error(NULL, _("Error"), _("Error adding group"),
				_("You must give a name for the group to add."));
		return;
	}

	grp = gaim_find_group(group);
	if (!grp)
	{
		grp = gaim_group_new(group);
		gaim_blist_add_group(grp, NULL);
	}
	else
	{
		gaim_notify_error(NULL, _("Error"), _("Error adding group"),
				_("A group with the name already exists."));
	}
}

static void
gg_request_add_group()
{
	gaim_request_input(NULL, _("Add Group"), NULL, _("Enter the name of the group"),
			NULL, FALSE, FALSE, NULL,
			_("Add"), G_CALLBACK(add_group_cb), _("Cancel"), NULL, NULL);
}

static GaimBlistUiOps blist_ui_ops =
{
	new_list,
	new_node,
	blist_show,
	node_update,
	node_remove,
	NULL,
	NULL,
	.request_add_buddy = gg_request_add_buddy,
	.request_add_chat = gg_request_add_chat,
	.request_add_group = gg_request_add_group
};

static gpointer
gg_blist_get_handle()
{
	static int handle;

	return &handle;
}

static void
add_group(GaimGroup *group, GGBlist *ggblist)
{
	GaimBlistNode *node = (GaimBlistNode *)group;
	if (node->ui_data)
		return;
	node->ui_data = gnt_tree_add_row_after(GNT_TREE(ggblist->tree), group,
			gnt_tree_create_row(GNT_TREE(ggblist->tree), get_display_name(node)), NULL, NULL);
}

static const char *
get_display_name(GaimBlistNode *node)
{
	static char text[2096];
	char status[8] = " ";
	const char *name = NULL;

	if (GAIM_BLIST_NODE_IS_CONTACT(node))
		node = (GaimBlistNode*)gaim_contact_get_priority_buddy((GaimContact*)node);  /* XXX: this can return NULL?! */
	
	if (node == NULL)
		return NULL;

	if (GAIM_BLIST_NODE_IS_BUDDY(node))
	{
		GaimBuddy *buddy = (GaimBuddy *)node;
		GaimStatusPrimitive prim;
		GaimPresence *presence;
		GaimStatus *now;
		gboolean ascii = gnt_ascii_only();
		
		presence = gaim_buddy_get_presence(buddy);
		now = gaim_presence_get_active_status(presence);

		prim = gaim_status_type_get_primitive(gaim_status_get_type(now));

		switch(prim)
		{
			case GAIM_STATUS_OFFLINE:
				strncpy(status, ascii ? "x" : "⊗", sizeof(status) - 1);
				break;
			case GAIM_STATUS_AVAILABLE:
				strncpy(status, ascii ? "o" : "◯", sizeof(status) - 1);
				break;
			default:
				strncpy(status, ascii ? "." : "⊖", sizeof(status) - 1);
				break;
		}
		name = gaim_buddy_get_alias(buddy);
	}
	else if (GAIM_BLIST_NODE_IS_CHAT(node))
	{
		GaimChat *chat = (GaimChat*)node;
		name = gaim_chat_get_name(chat);

		strncpy(status, "~", sizeof(status) - 1);
	}
	else if (GAIM_BLIST_NODE_IS_GROUP(node))
		return ((GaimGroup*)node)->name;

	snprintf(text, sizeof(text) - 1, "%s %s", status, name);

	return text;
}

static void
add_chat(GaimChat *chat, GGBlist *ggblist)
{
	GaimGroup *group;
	GaimBlistNode *node = (GaimBlistNode *)chat;
	if (node->ui_data)
		return;
	if (!gaim_account_is_connected(chat->account))
		return;

	group = gaim_chat_get_group(chat);
	add_node((GaimBlistNode*)group, ggblist);

	node->ui_data = gnt_tree_add_row_after(GNT_TREE(ggblist->tree), chat,
				gnt_tree_create_row(GNT_TREE(ggblist->tree), get_display_name(node)),
				group, NULL);

	/* XXX: This causes problem because you can close a chat window, hide the buddylist.
	 * When you show the buddylist, you automatically join the chat again. */
	if (gaim_blist_node_get_bool((GaimBlistNode*)chat, "gnt-autojoin"))
		serv_join_chat(gaim_account_get_connection(chat->account), chat->components);
}

static void
add_contact(GaimContact *contact, GGBlist *ggblist)
{
	GaimGroup *group;
	GaimBlistNode *node = (GaimBlistNode*)contact;
	const char *name;

	if (node->ui_data)
		return;
	
	name = get_display_name(node);
	if (name == NULL)
		return;
	
	group = (GaimGroup*)node->parent;
	add_node((GaimBlistNode*)group, ggblist);

	node->ui_data = gnt_tree_add_row_after(GNT_TREE(ggblist->tree), contact,
				gnt_tree_create_row(GNT_TREE(ggblist->tree), name),
				group, NULL);

	gnt_tree_set_expanded(GNT_TREE(ggblist->tree), contact, FALSE);
}

static void
add_buddy(GaimBuddy *buddy, GGBlist *ggblist)
{
	GaimContact *contact;
	GaimBlistNode *node = (GaimBlistNode *)buddy;
	if (node->ui_data)
		return;

	contact = (GaimContact*)node->parent;
	if (!contact)   /* When a new buddy is added and show-offline is set */
		return;
	add_node((GaimBlistNode*)contact, ggblist);

	node->ui_data = gnt_tree_add_row_after(GNT_TREE(ggblist->tree), buddy,
				gnt_tree_create_row(GNT_TREE(ggblist->tree), get_display_name(node)),
				contact, NULL);
	if (gaim_presence_is_idle(gaim_buddy_get_presence(buddy))) {
		gnt_tree_set_row_flags(GNT_TREE(ggblist->tree), buddy, GNT_TEXT_FLAG_DIM);
		gnt_tree_set_row_flags(GNT_TREE(ggblist->tree), contact, GNT_TEXT_FLAG_DIM);
	} else {
		gnt_tree_set_row_flags(GNT_TREE(ggblist->tree), buddy, 0);
		gnt_tree_set_row_flags(GNT_TREE(ggblist->tree), contact, 0);
	}
}

#if 0
static void
buddy_signed_on(GaimBuddy *buddy, GGBlist *ggblist)
{
	add_node((GaimBlistNode*)buddy, ggblist);
}

static void
buddy_signed_off(GaimBuddy *buddy, GGBlist *ggblist)
{
	node_remove(gaim_get_blist(), (GaimBlistNode*)buddy);
}
#endif

GaimBlistUiOps *gg_blist_get_ui_ops()
{
	return &blist_ui_ops;
}

static void
selection_activate(GntWidget *widget, GGBlist *ggblist)
{
	GntTree *tree = GNT_TREE(ggblist->tree);
	GaimBlistNode *node = gnt_tree_get_selection_data(tree);

	if (!node)
		return;
	
	if (GAIM_BLIST_NODE_IS_CONTACT(node))
		node = (GaimBlistNode*)gaim_contact_get_priority_buddy((GaimContact*)node);

	if (GAIM_BLIST_NODE_IS_BUDDY(node))
	{
		GaimBuddy *buddy = (GaimBuddy *)node;
		GaimConversation *conv =  gaim_conversation_new(GAIM_CONV_TYPE_IM,
					gaim_buddy_get_account(buddy),
					gaim_buddy_get_name(buddy));
		gg_conversation_set_active(conv);
	}
	else if (GAIM_BLIST_NODE_IS_CHAT(node))
	{
		GaimChat *chat = (GaimChat*)node;
		serv_join_chat(chat->account->gc, chat->components);
	}
}

static void
context_menu_callback(GntMenuItem *item, gpointer data)
{
	GaimMenuAction *action = data;
	GaimBlistNode *node = ggblist->cnode;
	if (action) {
		void (*callback)(GaimBlistNode *, gpointer);
		callback = (void (*)(GaimBlistNode *, gpointer))action->callback;
		if (callback)
			callback(action->data, node);
		else
			return;
	}
}

static void
gnt_append_menu_action(GntMenu *menu, GaimMenuAction *action, gpointer parent)
{
	GList *list;
	GntMenuItem *item;

	if (action == NULL)
		return;

	item = gnt_menuitem_new(action->label);
	if (action->callback)
		gnt_menuitem_set_callback(GNT_MENUITEM(item), context_menu_callback, action);
	gnt_menu_add_item(menu, GNT_MENUITEM(item));

	if (action->children) {
		GntWidget *sub = gnt_menu_new(GNT_MENU_POPUP);
		gnt_menuitem_set_submenu(item, GNT_MENU(sub));
		for (list = action->children; list; list = list->next)
			gnt_append_menu_action(GNT_MENU(sub), list->data, action);
	}
}

static void
append_proto_menu(GntMenu *menu, GaimConnection *gc, GaimBlistNode *node)
{
	GList *list;
	GaimPluginProtocolInfo *prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if(!prpl_info || !prpl_info->blist_node_menu)
		return;

	for(list = prpl_info->blist_node_menu(node); list;
			list = g_list_delete_link(list, list))
	{
		GaimMenuAction *act = (GaimMenuAction *) list->data;
		act->data = node;
		gnt_append_menu_action(menu, act, NULL);
	}
}

static void
add_custom_action(GntMenu *menu, const char *label, GaimCallback callback,
		gpointer data)
{
	GaimMenuAction *action = gaim_menu_action_new(label, callback, data, NULL);
	gnt_append_menu_action(menu, action, NULL);
	g_signal_connect_swapped(G_OBJECT(menu), "destroy",
			G_CALLBACK(gaim_menu_action_free), action);
}

static void
chat_components_edit_ok(GaimChat *chat, GaimRequestFields *allfields)
{
	GList *groups, *fields;

	for (groups = gaim_request_fields_get_groups(allfields); groups; groups = groups->next) {
		fields = gaim_request_field_group_get_fields(groups->data);
		for (; fields; fields = fields->next) {
			GaimRequestField *field = fields->data;
			const char *id;
			char *val;

			id = gaim_request_field_get_id(field);
			if (gaim_request_field_get_type(field) == GAIM_REQUEST_FIELD_INTEGER)
				val = g_strdup_printf("%d", gaim_request_field_int_get_value(field));
			else
				val = g_strdup(gaim_request_field_string_get_value(field));

			g_hash_table_replace(chat->components, g_strdup(id), val);  /* val should not be free'd */
		}
	}
}

static void
chat_components_edit(GaimChat *chat, GaimBlistNode *selected)
{
	GaimRequestFields *fields = gaim_request_fields_new();
	GaimRequestFieldGroup *group = gaim_request_field_group_new(NULL);
	GaimRequestField *field;
	GList *parts, *iter;
	struct proto_chat_entry *pce;

	gaim_request_fields_add_group(fields, group);

	parts = GAIM_PLUGIN_PROTOCOL_INFO(chat->account->gc->prpl)->chat_info(chat->account->gc);

	for (iter = parts; iter; iter = iter->next) {
		pce = iter->data;
		if (pce->is_int) {
			int val;
			const char *str = g_hash_table_lookup(chat->components, pce->identifier);
			if (!str || sscanf(str, "%d", &val) != 1)
				val = pce->min;
			field = gaim_request_field_int_new(pce->identifier, pce->label, val);
		} else {
			field = gaim_request_field_string_new(pce->identifier, pce->label,
					g_hash_table_lookup(chat->components, pce->identifier), FALSE);
		}

		gaim_request_field_group_add_field(group, field);
		g_free(pce);
	}

	g_list_free(parts);

	gaim_request_fields(NULL, _("Edit Chat"), NULL, _("Please Update the necessary fields."),
			fields, _("Edit"), G_CALLBACK(chat_components_edit_ok), _("Cancel"), NULL, chat);
}

static void
autojoin_toggled(GntMenuItem *item, gpointer data)
{
	GaimMenuAction *action = data;
	gaim_blist_node_set_bool(action->data, "gnt-autojoin",
				gnt_menuitem_check_get_checked(GNT_MENUITEM_CHECK(item)));
}

static void
create_chat_menu(GntMenu *menu, GaimChat *chat)
{
	GaimMenuAction *action = gaim_menu_action_new(_("Auto-join"), NULL, chat, NULL);
	GntMenuItem *check = gnt_menuitem_check_new(action->label);
	gnt_menuitem_check_set_checked(GNT_MENUITEM_CHECK(check),
				gaim_blist_node_get_bool((GaimBlistNode*)chat, "gnt-autojoin"));
	gnt_menu_add_item(menu, check);
	gnt_menuitem_set_callback(check, autojoin_toggled, action);
	g_signal_connect_swapped(G_OBJECT(menu), "destroy",
			G_CALLBACK(gaim_menu_action_free), action);

	add_custom_action(menu, _("Edit Settings"), (GaimCallback)chat_components_edit, chat);
}

static void
gg_add_buddy(GaimGroup *grp, GaimBlistNode *selected)
{
	gaim_blist_request_add_buddy(NULL, NULL, grp ? grp->name : NULL, NULL);
}

static void
gg_add_group(GaimGroup *grp, GaimBlistNode *selected)
{
	gaim_blist_request_add_group();
}

static void
gg_add_chat(GaimGroup *grp, GaimBlistNode *selected)
{
	gaim_blist_request_add_chat(NULL, grp, NULL, NULL);
}

static void
create_group_menu(GntMenu *menu, GaimGroup *group)
{
	add_custom_action(menu, _("Add Buddy"),
			GAIM_CALLBACK(gg_add_buddy), group);
	add_custom_action(menu, _("Add Chat"),
			GAIM_CALLBACK(gg_add_chat), group);
	add_custom_action(menu, _("Add Group"),
			GAIM_CALLBACK(gg_add_group), group);
}

static void
gg_blist_get_buddy_info_cb(GaimBuddy *buddy, GaimBlistNode *selected)
{
	serv_get_info(buddy->account->gc, gaim_buddy_get_name(buddy));
}

static void
create_buddy_menu(GntMenu *menu, GaimBuddy *buddy)
{
	GaimPluginProtocolInfo *prpl_info;

	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(buddy->account->gc->prpl);
	if (prpl_info && prpl_info->get_info)
	{
		add_custom_action(menu, _("Get Info"),
				GAIM_CALLBACK(gg_blist_get_buddy_info_cb), buddy);
	}

#if 0
	add_custom_action(tree, _("Add Buddy Pounce"),
			GAIM_CALLBACK(gg_blist_add_buddy_pounce_cb)), buddy);

	if (prpl_info && prpl_info->send_file)
	{
		if (!prpl_info->can_receive_file ||
			prpl_info->can_receive_file(buddy->account->gc, buddy->name))
			add_custom_action(tree, _("Send File"),
					GAIM_CALLBACK(gg_blist_show_file_cb)), buddy);
	}

	add_custom_action(tree, _("View Log"),
			GAIM_CALLBACK(gg_blist_view_log_cb)), buddy);
#endif

	/* Protocol actions */
	append_proto_menu(menu,
			gaim_account_get_connection(gaim_buddy_get_account(buddy)),
			(GaimBlistNode*)buddy);
}

static void
append_extended_menu(GntMenu *menu, GaimBlistNode *node)
{
	GList *iter;

	for (iter = gaim_blist_node_get_extended_menu(node);
			iter; iter = g_list_delete_link(iter, iter))
	{
		gnt_append_menu_action(menu, iter->data, NULL);
	}
}

/* Xerox'd from gtkdialogs.c:gaim_gtkdialogs_remove_contact_cb */
static void
remove_contact(GaimContact *contact)
{
	GaimBlistNode *bnode, *cnode;
	GaimGroup *group;

	cnode = (GaimBlistNode *)contact;
	group = (GaimGroup*)cnode->parent;
	for (bnode = cnode->child; bnode; bnode = bnode->next) {
		GaimBuddy *buddy = (GaimBuddy*)bnode;
		if (gaim_account_is_connected(buddy->account))
			gaim_account_remove_buddy(buddy->account, buddy, group);
	}
	gaim_blist_remove_contact(contact);
}

static void
rename_blist_node(GaimBlistNode *node, const char *newname)
{
	const char *name = newname;
	if (name && !*name)
		name = NULL;

	if (GAIM_BLIST_NODE_IS_CONTACT(node)) {
		GaimContact *contact = (GaimContact*)node;
		GaimBuddy *buddy = gaim_contact_get_priority_buddy(contact);
		gaim_blist_alias_contact(contact, name);
		gaim_blist_alias_buddy(buddy, name);
		serv_alias_buddy(buddy);
	} else if (GAIM_BLIST_NODE_IS_BUDDY(node)) {
		gaim_blist_alias_buddy((GaimBuddy*)node, name);
		serv_alias_buddy((GaimBuddy*)node);
	} else if (GAIM_BLIST_NODE_IS_CHAT(node))
		gaim_blist_alias_chat((GaimChat*)node, name);
	else if (GAIM_BLIST_NODE_IS_GROUP(node) && (name != NULL))
		gaim_blist_rename_group((GaimGroup*)node, name);
	else
		g_return_if_reached();
}

static void
gg_blist_rename_node_cb(GaimBlistNode *node, GaimBlistNode *selected)
{
	const char *name = NULL;
	char *prompt;

	if (GAIM_BLIST_NODE_IS_CONTACT(node))
		name = gaim_contact_get_alias((GaimContact*)node);
	else if (GAIM_BLIST_NODE_IS_BUDDY(node))
		name = gaim_buddy_get_contact_alias((GaimBuddy*)node);
	else if (GAIM_BLIST_NODE_IS_CHAT(node))
		name = gaim_chat_get_name((GaimChat*)node);
	else if (GAIM_BLIST_NODE_IS_GROUP(node))
		name = ((GaimGroup*)node)->name;
	else
		g_return_if_reached();

	prompt = g_strdup_printf(_("Please enter the new name for %s"), name);

	gaim_request_input(node, _("Rename"), prompt, _("Enter empty string to reset the name."),
			name, FALSE, FALSE, NULL, _("Rename"), G_CALLBACK(rename_blist_node),
			_("Cancel"), NULL, node);

	g_free(prompt);
}

/* Xeroxed from gtkdialogs.c:gaim_gtkdialogs_remove_group_cb*/
static void
remove_group(GaimGroup *group)
{
	GaimBlistNode *cnode, *bnode;

	cnode = ((GaimBlistNode*)group)->child;

	while (cnode) {
		if (GAIM_BLIST_NODE_IS_CONTACT(cnode)) {
			bnode = cnode->child;
			cnode = cnode->next;
			while (bnode) {
				GaimBuddy *buddy;
				if (GAIM_BLIST_NODE_IS_BUDDY(bnode)) {
					buddy = (GaimBuddy*)bnode;
					bnode = bnode->next;
					if (gaim_account_is_connected(buddy->account)) {
						gaim_account_remove_buddy(buddy->account, buddy, group);
						gaim_blist_remove_buddy(buddy);
					}
				} else {
					bnode = bnode->next;
				}
			}
		} else if (GAIM_BLIST_NODE_IS_CHAT(cnode)) {
			GaimChat *chat = (GaimChat *)cnode;
			cnode = cnode->next;
			if (gaim_account_is_connected(chat->account))
				gaim_blist_remove_chat(chat);
		} else {
			cnode = cnode->next;
		}
	}

	gaim_blist_remove_group(group);
}

static void
gg_blist_remove_node(GaimBlistNode *node)
{
	if (GAIM_BLIST_NODE_IS_CONTACT(node)) {
		remove_contact((GaimContact*)node);
	} else if (GAIM_BLIST_NODE_IS_BUDDY(node)) {
		GaimBuddy *buddy = (GaimBuddy*)node;
		GaimGroup *group = gaim_buddy_get_group(buddy);
		gaim_account_remove_buddy(gaim_buddy_get_account(buddy), buddy, group);
		gaim_blist_remove_buddy(buddy);
	} else if (GAIM_BLIST_NODE_IS_CHAT(node)) {
		gaim_blist_remove_chat((GaimChat*)node);
	} else if (GAIM_BLIST_NODE_IS_GROUP(node)) {
		remove_group((GaimGroup*)node);
	}
}

static void
gg_blist_remove_node_cb(GaimBlistNode *node, GaimBlistNode *selected)
{
	char *primary;
	const char *name, *sec = NULL;

	/* XXX: could be a contact */
	if (GAIM_BLIST_NODE_IS_CONTACT(node)) {
		GaimContact *c = (GaimContact*)node;
		name = gaim_contact_get_alias(c);
		if (c->totalsize > 1)
			sec = _("Removing this contact will also remove all the buddies in the contact");
	} else if (GAIM_BLIST_NODE_IS_BUDDY(node))
		name = gaim_buddy_get_name((GaimBuddy*)node);
	else if (GAIM_BLIST_NODE_IS_CHAT(node))
		name = gaim_chat_get_name((GaimChat*)node);
	else if (GAIM_BLIST_NODE_IS_GROUP(node))
	{
		name = ((GaimGroup*)node)->name;
		sec = _("Removing this group will also remove all the buddies in the group");
	}
	else
		return;

	primary = g_strdup_printf(_("Are you sure you want to remove %s?"), name);

	/* XXX: anything to do with the returned ui-handle? */
	gaim_request_action(node, _("Confirm Remove"),
			primary, sec,
			1, node, 2,
			_("Remove"), gg_blist_remove_node,
			_("Cancel"), NULL);
	g_free(primary);
}

static void
gg_blist_toggle_tag_buddy(GaimBlistNode *node)
{
	GList *iter;
	if (ggblist->tagged && (iter = g_list_find(ggblist->tagged, node)) != NULL) {
		ggblist->tagged = g_list_delete_link(ggblist->tagged, iter);
	} else {
		ggblist->tagged = g_list_prepend(ggblist->tagged, node);
	}
	if (GAIM_BLIST_NODE_IS_CONTACT(node))
		node = (GaimBlistNode*)gaim_contact_get_priority_buddy((GaimContact*)node);
	update_buddy_display((GaimBuddy*)node, ggblist);
	gaim_debug_info("blist", "Tagged buddy\n");
}

static void
gg_blist_place_tagged(GaimBlistNode *target)
{
	GaimGroup *tg = NULL;
	GaimContact *tc = NULL;

	if (GAIM_BLIST_NODE_IS_GROUP(target))
		tg = (GaimGroup*)target;
	else
		tc = (GaimContact*)target;

	if (ggblist->tagged) {
		GList *list = ggblist->tagged;
		ggblist->tagged = NULL;

		while (list) {
			GaimBlistNode *node = list->data;
			list = g_list_delete_link(list, list);
			if (tg) {
				if (GAIM_BLIST_NODE_IS_CONTACT(node))
					gaim_blist_add_contact((GaimContact*)node, tg, NULL);
				else
					gaim_blist_add_buddy((GaimBuddy*)node, NULL, tg, NULL);
			} else {
				if (GAIM_BLIST_NODE_IS_BUDDY(node))
					gaim_blist_add_buddy((GaimBuddy*)node, tc,
						gaim_buddy_get_group(gaim_contact_get_priority_buddy(tc)), NULL);
				else if (GAIM_BLIST_NODE_IS_CONTACT(node))
					gaim_blist_merge_contact((GaimContact*)node, target);
			}
		}
	}
	gaim_debug_info("blist", "Placed buddy\n");
}

static void
context_menu_destroyed(GntWidget *widget, GGBlist *ggblist)
{
	ggblist->context = NULL;
}

static void
draw_context_menu(GGBlist *ggblist)
{
	GaimBlistNode *node = NULL;
	GntWidget *context = NULL;
	GntTree *tree = NULL;
	int x, y, top, width;
	char *title = NULL;

	tree = GNT_TREE(ggblist->tree);

	node = gnt_tree_get_selection_data(tree);

	if (ggblist->tooltip)
		remove_tooltip(ggblist);

	ggblist->cnode = node;

	ggblist->context = context = gnt_menu_new(GNT_MENU_POPUP);
	g_signal_connect(G_OBJECT(context), "destroy", G_CALLBACK(context_menu_destroyed), ggblist);

	if (!node) {
		create_group_menu(GNT_MENU(context), NULL);
		title = g_strdup(_("Buddy List"));
	} else if (GAIM_BLIST_NODE_IS_CONTACT(node)) {
		create_buddy_menu(GNT_MENU(context),
			gaim_contact_get_priority_buddy((GaimContact*)node));
		title = g_strdup(gaim_contact_get_alias((GaimContact*)node));
	} else if (GAIM_BLIST_NODE_IS_BUDDY(node)) {
		GaimBuddy *buddy = (GaimBuddy *)node;
		create_buddy_menu(GNT_MENU(context), buddy);
		title = g_strdup(gaim_buddy_get_name(buddy));
	} else if (GAIM_BLIST_NODE_IS_CHAT(node)) {
		GaimChat *chat = (GaimChat*)node;
		create_chat_menu(GNT_MENU(context), chat);
		title = g_strdup(gaim_chat_get_name(chat));
	} else if (GAIM_BLIST_NODE_IS_GROUP(node)) {
		GaimGroup *group = (GaimGroup *)node;
		create_group_menu(GNT_MENU(context), group);
		title = g_strdup(group->name);
	}

	append_extended_menu(GNT_MENU(context), node);

	/* These are common for everything */
	if (node) {
		add_custom_action(GNT_MENU(context), _("Rename"),
				GAIM_CALLBACK(gg_blist_rename_node_cb), node);
		add_custom_action(GNT_MENU(context), _("Remove"),
				GAIM_CALLBACK(gg_blist_remove_node_cb), node);

		if (ggblist->tagged && (GAIM_BLIST_NODE_IS_CONTACT(node)
				|| GAIM_BLIST_NODE_IS_GROUP(node))) {
			add_custom_action(GNT_MENU(context), _("Place tagged"),
					GAIM_CALLBACK(gg_blist_place_tagged), node);
		}
		
		if (GAIM_BLIST_NODE_IS_BUDDY(node) || GAIM_BLIST_NODE_IS_CONTACT(node)) {
			add_custom_action(GNT_MENU(context), _("Toggle Tag"),
					GAIM_CALLBACK(gg_blist_toggle_tag_buddy), node);
		}
	}

	/* Set the position for the popup */
	gnt_widget_get_position(GNT_WIDGET(tree), &x, &y);
	gnt_widget_get_size(GNT_WIDGET(tree), &width, NULL);
	top = gnt_tree_get_selection_visible_line(tree);

	x += width;
	y += top - 1;

	gnt_widget_set_position(context, x, y);
	gnt_screen_menu_show(GNT_MENU(context));
	g_free(title);
}

static void
tooltip_for_buddy(GaimBuddy *buddy, GString *str)
{
	GaimPlugin *prpl;
	GaimPluginProtocolInfo *prpl_info;
	GaimAccount *account;

	account = gaim_buddy_get_account(buddy);
	
	g_string_append_printf(str, _("Account: %s (%s)"),
			gaim_account_get_username(account),
			gaim_account_get_protocol_name(account));
	
	prpl = gaim_find_prpl(gaim_account_get_protocol_id(account));
	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);
	if (prpl_info && prpl_info->tooltip_text) {
		GString *tip = g_string_new("");
		char *strip, *br;
		prpl_info->tooltip_text(buddy, tip, TRUE);

		br = gaim_strreplace(tip->str, "\n", "<br>");
		strip = gaim_markup_strip_html(br);
		g_string_append(str, strip);
		g_string_free(tip, TRUE);
		g_free(strip);
		g_free(br);
	}

	if (gaim_prefs_get_bool("/gaim/gnt/blist/idletime")) {
		GaimPresence *pre = gaim_buddy_get_presence(buddy);
		if (gaim_presence_is_idle(pre)) {
			time_t idle = gaim_presence_get_idle_time(pre);
			if (idle > 0) {
				char *st = gaim_str_seconds_to_string(time(NULL) - idle);
				g_string_append_printf(str, _("\nIdle: %s"), st);
				g_free(st);
			}
		}
	}
}

static GString*
make_sure_text_fits(GString *string)
{
	int maxw = getmaxx(stdscr) - 3;
	char *str = gnt_util_onscreen_fit_string(string->str, maxw);
	string = g_string_assign(string, str);
	g_free(str);
	return string;
}

static gboolean
draw_tooltip_real(GGBlist *ggblist)
{
	GaimBlistNode *node;
	int x, y, top, width, w, h;
	GString *str;
	GntTree *tree;
	GntWidget *widget, *box, *tv;
	char *title = NULL;
	int lastseen = 0;

	widget = ggblist->tree;
	tree = GNT_TREE(widget);

	if (!gnt_widget_has_focus(ggblist->tree) || 
			(ggblist->context && !GNT_WIDGET_IS_FLAG_SET(ggblist->context, GNT_WIDGET_INVISIBLE)))
		return FALSE;

	if (ggblist->tooltip)
	{
		/* XXX: Once we can properly redraw on expose events, this can be removed at the end
		 * to avoid the blinking*/
		remove_tooltip(ggblist);
	}

	node = gnt_tree_get_selection_data(tree);
	if (!node)
		return FALSE;

	str = g_string_new("");

	if (GAIM_BLIST_NODE_IS_CONTACT(node)) {
		GaimBuddy *pr = gaim_contact_get_priority_buddy((GaimContact*)node);
		gboolean offline = !GAIM_BUDDY_IS_ONLINE(pr);
		gboolean showoffline = gaim_prefs_get_bool(PREF_ROOT "/showoffline");
		const char *alias = gaim_contact_get_alias((GaimContact*)node);
		const char *name = gaim_buddy_get_name(pr);

		title = g_strdup(alias);
		if (g_utf8_collate(alias, name))
			g_string_append_printf(str, _("Nickname: %s\n"), gaim_buddy_get_name(pr));
		tooltip_for_buddy(pr, str);
		for (node = node->child; node; node = node->next) {
			GaimBuddy *buddy = (GaimBuddy*)node;
			if (offline) {
				int value = gaim_blist_node_get_int(node, "last_seen");
				if (value > lastseen)
					lastseen = value;
			}
			if (node == (GaimBlistNode*)pr)
				continue;
			if (!gaim_account_is_connected(buddy->account))
				continue;
			if (!showoffline && !GAIM_BUDDY_IS_ONLINE(buddy))
				continue;
			str = g_string_append(str, "\n----------\n");
			g_string_append_printf(str, _("Nickname: %s\n"), gaim_buddy_get_name(buddy));
			tooltip_for_buddy(buddy, str);
		}
	} else if (GAIM_BLIST_NODE_IS_BUDDY(node)) {
		GaimBuddy *buddy = (GaimBuddy *)node;
		tooltip_for_buddy(buddy, str);
		title = g_strdup(gaim_buddy_get_name(buddy));
		if (!GAIM_BUDDY_IS_ONLINE((GaimBuddy*)node))
			lastseen = gaim_blist_node_get_int(node, "last_seen");
	} else if (GAIM_BLIST_NODE_IS_GROUP(node)) {
		GaimGroup *group = (GaimGroup *)node;

		g_string_append_printf(str, _("Online: %d\nTotal: %d"),
						gaim_blist_get_group_online_count(group),
						gaim_blist_get_group_size(group, FALSE));

		title = g_strdup(group->name);
	} else if (GAIM_BLIST_NODE_IS_CHAT(node)) {
		GaimChat *chat = (GaimChat *)node;
		GaimAccount *account = chat->account;

		g_string_append_printf(str, _("Account: %s (%s)"),
				gaim_account_get_username(account),
				gaim_account_get_protocol_name(account));

		title = g_strdup(gaim_chat_get_name(chat));
	} else {
		g_string_free(str, TRUE);
		return FALSE;
	}

	if (lastseen > 0) {
		char *tmp = gaim_str_seconds_to_string(time(NULL) - lastseen);
		g_string_append_printf(str, _("\nLast Seen: %s ago"), tmp);
		g_free(tmp);
	}

	gnt_widget_get_position(widget, &x, &y);
	gnt_widget_get_size(widget, &width, NULL);
	top = gnt_tree_get_selection_visible_line(tree);

	x += width;
	y += top - 1;

	box = gnt_box_new(FALSE, FALSE);
	gnt_box_set_toplevel(GNT_BOX(box), TRUE);
	GNT_WIDGET_SET_FLAGS(box, GNT_WIDGET_NO_SHADOW);
	gnt_box_set_title(GNT_BOX(box), title);

	str = make_sure_text_fits(str);
	gnt_util_get_text_bound(str->str, &w, &h);
	h = MAX(2, h);
	tv = gnt_text_view_new();
	gnt_widget_set_size(tv, w + 1, h);
	gnt_box_add_widget(GNT_BOX(box), tv);

	gnt_widget_set_position(box, x, y);
	GNT_WIDGET_UNSET_FLAGS(box, GNT_WIDGET_CAN_TAKE_FOCUS);
	GNT_WIDGET_SET_FLAGS(box, GNT_WIDGET_TRANSIENT);
	gnt_widget_draw(box);

	gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(tv), str->str, GNT_TEXT_FLAG_NORMAL);
	gnt_text_view_scroll(GNT_TEXT_VIEW(tv), 0);

	g_free(title);
	g_string_free(str, TRUE);
	ggblist->tooltip = box;
	ggblist->tnode = node;

	gnt_widget_set_name(ggblist->tooltip, "tooltip");
	return FALSE;
}

static void
draw_tooltip(GGBlist *ggblist)
{
	/* When an account has signed off, it removes one buddy at a time.
	 * Drawing the tooltip after removing each buddy is expensive. On
	 * top of that, if the selected buddy belongs to the disconnected
	 * account, then retreiving the tooltip for that causes crash. So
	 * let's make sure we wait for all the buddies to be removed first.*/
	int id = g_timeout_add(0, (GSourceFunc)draw_tooltip_real, ggblist);
	g_object_set_data_full(G_OBJECT(ggblist->window), "draw_tooltip_calback",
				GINT_TO_POINTER(id), (GDestroyNotify)g_source_remove);
}

static void
selection_changed(GntWidget *widget, gpointer old, gpointer current, GGBlist *ggblist)
{
	draw_tooltip(ggblist);
}

static gboolean
context_menu(GntWidget *widget, GGBlist *ggblist)
{
	draw_context_menu(ggblist);
	return TRUE;
}

static gboolean
key_pressed(GntWidget *widget, const char *text, GGBlist *ggblist)
{
	if (text[0] == 27 && text[1] == 0) {
		/* Escape was pressed */
		remove_peripherals(ggblist);
	} else if (strcmp(text, GNT_KEY_CTRL_O) == 0) {
		gaim_prefs_set_bool(PREF_ROOT "/showoffline",
				!gaim_prefs_get_bool(PREF_ROOT "/showoffline"));
	} else if (strcmp(text, "t") == 0) {
		gg_blist_toggle_tag_buddy(gnt_tree_get_selection_data(GNT_TREE(ggblist->tree)));
	} else if (strcmp(text, "a") == 0) {
		gg_blist_place_tagged(gnt_tree_get_selection_data(GNT_TREE(ggblist->tree)));
	} else
		return FALSE;

	return TRUE;
}

static void
update_buddy_display(GaimBuddy *buddy, GGBlist *ggblist)
{
	GaimContact *contact;
	GntTextFormatFlags bflag = 0, cflag = 0;
	
	contact = gaim_buddy_get_contact(buddy);

	gaim_debug_fatal("sadrul", "updating display for %s\n", gaim_buddy_get_name(buddy));
	g_printerr("sadrul:  updating display for %s\n", gaim_buddy_get_name(buddy));

	gnt_tree_change_text(GNT_TREE(ggblist->tree), buddy, 0, get_display_name((GaimBlistNode*)buddy));
	gnt_tree_change_text(GNT_TREE(ggblist->tree), contact, 0, get_display_name((GaimBlistNode*)contact));

	if (ggblist->tagged && g_list_find(ggblist->tagged, buddy))
		bflag |= GNT_TEXT_FLAG_BOLD;
	if (ggblist->tagged && g_list_find(ggblist->tagged, contact))
		cflag |= GNT_TEXT_FLAG_BOLD;

	if (ggblist->tnode == (GaimBlistNode*)buddy)
		draw_tooltip(ggblist);

	if (gaim_presence_is_idle(gaim_buddy_get_presence(buddy))) {
		gnt_tree_set_row_flags(GNT_TREE(ggblist->tree), buddy, bflag | GNT_TEXT_FLAG_DIM);
		gnt_tree_set_row_flags(GNT_TREE(ggblist->tree), contact, cflag | GNT_TEXT_FLAG_DIM);
	} else {
		gnt_tree_set_row_flags(GNT_TREE(ggblist->tree), buddy, bflag);
		gnt_tree_set_row_flags(GNT_TREE(ggblist->tree), contact, cflag);
	}
}

static void
buddy_status_changed(GaimBuddy *buddy, GaimStatus *old, GaimStatus *now, GGBlist *ggblist)
{
	update_buddy_display(buddy, ggblist);
}

static void
buddy_idle_changed(GaimBuddy *buddy, int old, int new, GGBlist *ggblist)
{
	update_buddy_display(buddy, ggblist);
}

static void
remove_peripherals(GGBlist *ggblist)
{
	if (ggblist->tooltip)
		remove_tooltip(ggblist);
	else if (ggblist->context)
		gnt_widget_destroy(ggblist->context);
}

static void
size_changed_cb(GntWidget *w, int wi, int h)
{
	int width, height;
	gnt_widget_get_size(w, &width, &height);
	gaim_prefs_set_int(PREF_ROOT "/size/width", width);
	gaim_prefs_set_int(PREF_ROOT "/size/height", height);
	gnt_tree_set_col_width(GNT_TREE(ggblist->tree), 0, width - 1);
}

static void
save_position_cb(GntWidget *w, int x, int y)
{
	gaim_prefs_set_int(PREF_ROOT "/position/x", x);
	gaim_prefs_set_int(PREF_ROOT "/position/y", y);
}

static void
reset_blist_window(GntWidget *window, gpointer null)
{
	GaimBlistNode *node;
	gaim_signals_disconnect_by_handle(gg_blist_get_handle());
	gaim_get_blist()->ui_data = NULL;

	node = gaim_blist_get_root();
	while (node) {
		node->ui_data = NULL;
		node = gaim_blist_node_next(node, TRUE);
	}

	if (ggblist->typing)
		g_source_remove(ggblist->typing);
	remove_peripherals(ggblist);
	g_free(ggblist);
	ggblist = NULL;
}

static void
populate_buddylist()
{
	GaimBlistNode *node;
	GaimBuddyList *list;

	if (strcmp(gaim_prefs_get_string(PREF_ROOT "/sort_type"), "text") == 0) {
		gnt_tree_set_compare_func(GNT_TREE(ggblist->tree),
			(GCompareFunc)blist_node_compare_text);
	} else if (strcmp(gaim_prefs_get_string(PREF_ROOT "/sort_type"), "status") == 0) {
		gnt_tree_set_compare_func(GNT_TREE(ggblist->tree),
			(GCompareFunc)blist_node_compare_status);
	} else if (strcmp(gaim_prefs_get_string(PREF_ROOT "/sort_type"), "log") == 0) {
		gnt_tree_set_compare_func(GNT_TREE(ggblist->tree),
			(GCompareFunc)blist_node_compare_log);
	}
	
	list = gaim_get_blist();
	node = gaim_blist_get_root();
	while (node)
	{
		node_update(list, node);
		node = gaim_blist_node_next(node, FALSE);
	}
}

static void
destroy_status_list(GList *list)
{
	g_list_foreach(list, (GFunc)g_free, NULL);
	g_list_free(list);
}

static void
populate_status_dropdown()
{
	int i;
	GList *iter;
	GList *items = NULL;
	StatusBoxItem *item = NULL;

	/* First the primitives */
	GaimStatusPrimitive prims[] = {GAIM_STATUS_AVAILABLE, GAIM_STATUS_AWAY,
			GAIM_STATUS_INVISIBLE, GAIM_STATUS_OFFLINE, GAIM_STATUS_UNSET};

	gnt_combo_box_remove_all(GNT_COMBO_BOX(ggblist->status));

	for (i = 0; prims[i] != GAIM_STATUS_UNSET; i++)
	{
		item = g_new0(StatusBoxItem, 1);
		item->type = STATUS_PRIMITIVE;
		item->u.prim = prims[i];
		items = g_list_prepend(items, item);
		gnt_combo_box_add_data(GNT_COMBO_BOX(ggblist->status), item,
				gaim_primitive_get_name_from_type(prims[i]));
	}

	/* Now the popular statuses */
	for (iter = gaim_savedstatuses_get_popular(6); iter; iter = iter->next)
	{
		item = g_new0(StatusBoxItem, 1);
		item->type = STATUS_SAVED_POPULAR;
		item->u.saved = iter->data;
		items = g_list_prepend(items, item);
		gnt_combo_box_add_data(GNT_COMBO_BOX(ggblist->status), item,
				gaim_savedstatus_get_title(iter->data));
	}

	/* New savedstatus */
	item = g_new0(StatusBoxItem, 1);
	item->type = STATUS_SAVED_NEW;
	items = g_list_prepend(items, item);
	gnt_combo_box_add_data(GNT_COMBO_BOX(ggblist->status), item,
			_("New..."));

	/* More savedstatuses */
	item = g_new0(StatusBoxItem, 1);
	item->type = STATUS_SAVED_ALL;
	items = g_list_prepend(items, item);
	gnt_combo_box_add_data(GNT_COMBO_BOX(ggblist->status), item,
			_("Saved..."));

	/* The keys for the combobox are created here, and never used
	 * anywhere else. So make sure the keys are freed when the widget
	 * is destroyed. */
	g_object_set_data_full(G_OBJECT(ggblist->status), "list of statuses",
			items, (GDestroyNotify)destroy_status_list);
}

static void
redraw_blist(const char *name, GaimPrefType type, gconstpointer val, gpointer data)
{
	GaimBlistNode *node, *sel;
	if (ggblist == NULL || ggblist->window == NULL)
		return;

	sel = gnt_tree_get_selection_data(GNT_TREE(ggblist->tree));
	gnt_tree_remove_all(GNT_TREE(ggblist->tree));
	node = gaim_blist_get_root();
	for (; node; node = gaim_blist_node_next(node, TRUE))
		node->ui_data = NULL;
	populate_buddylist();
	gnt_tree_set_selected(GNT_TREE(ggblist->tree), sel);
	draw_tooltip(ggblist);
}

void gg_blist_init()
{
	gaim_prefs_add_none(PREF_ROOT);
	gaim_prefs_add_none(PREF_ROOT "/size");
	gaim_prefs_add_int(PREF_ROOT "/size/width", 20);
	gaim_prefs_add_int(PREF_ROOT "/size/height", 17);
	gaim_prefs_add_none(PREF_ROOT "/position");
	gaim_prefs_add_int(PREF_ROOT "/position/x", 0);
	gaim_prefs_add_int(PREF_ROOT "/position/y", 0);
	gaim_prefs_add_bool(PREF_ROOT "/idletime", TRUE);
	gaim_prefs_add_bool(PREF_ROOT "/showoffline", FALSE);
	gaim_prefs_add_string(PREF_ROOT "/sort_type", "text");

	gaim_prefs_connect_callback(gg_blist_get_handle(),
			PREF_ROOT "/showoffline", redraw_blist, NULL);
	gaim_prefs_connect_callback(gg_blist_get_handle(),
			PREF_ROOT "/sort_type", redraw_blist, NULL);

	return;
}

static gboolean
remove_typing_cb(gpointer null)
{
	GaimSavedStatus *current;
	const char *message, *newmessage;
	GaimStatusPrimitive prim, newprim;
	StatusBoxItem *item;

	current = gaim_savedstatus_get_current();
	message = gaim_savedstatus_get_message(current);
	prim = gaim_savedstatus_get_type(current);

	newmessage = gnt_entry_get_text(GNT_ENTRY(ggblist->statustext));
	item = gnt_combo_box_get_selected_data(GNT_COMBO_BOX(ggblist->status));
	g_return_val_if_fail(item->type == STATUS_PRIMITIVE, FALSE);
	newprim = item->u.prim;

	if (newprim != prim || ((message && !newmessage) ||
				(!message && newmessage) ||
				(message && newmessage && g_utf8_collate(message, newmessage) != 0)))
	{
		GaimSavedStatus *status = gaim_savedstatus_find_transient_by_type_and_message(newprim, newmessage);
									/* Holy Crap! That's a LAWNG function name */
		if (status == NULL)
		{
			status = gaim_savedstatus_new(NULL, newprim);
			gaim_savedstatus_set_message(status, newmessage);
		}

		gaim_savedstatus_activate(status);
	}

	gnt_box_give_focus_to_child(GNT_BOX(ggblist->window), ggblist->tree);
	if (ggblist->typing)
		g_source_remove(ggblist->typing);
	ggblist->typing = 0;
	return FALSE;
}

static void
status_selection_changed(GntComboBox *box, StatusBoxItem *old, StatusBoxItem *now, gpointer null)
{
	gnt_entry_set_text(GNT_ENTRY(ggblist->statustext), NULL);
	if (now->type == STATUS_SAVED_POPULAR)
	{
		/* Set the status immediately */
		gaim_savedstatus_activate(now->u.saved);
	}
	else if (now->type == STATUS_PRIMITIVE)
	{
		/* Move the focus to the entry box */
		/* XXX: Make sure the selected status can have a message */
		gnt_box_move_focus(GNT_BOX(ggblist->window), 1);
		ggblist->typing = g_timeout_add(TYPING_TIMEOUT, (GSourceFunc)remove_typing_cb, NULL);
	}
	else if (now->type == STATUS_SAVED_ALL)
	{
		/* Restore the selection to reflect current status. */
		savedstatus_changed(gaim_savedstatus_get_current(), NULL);
		gnt_box_give_focus_to_child(GNT_BOX(ggblist->window), ggblist->tree);
		gg_savedstatus_show_all();
	}
	else if (now->type == STATUS_SAVED_NEW)
	{
		savedstatus_changed(gaim_savedstatus_get_current(), NULL);
		gnt_box_give_focus_to_child(GNT_BOX(ggblist->window), ggblist->tree);
		gg_savedstatus_edit(NULL);
	}
	else
		g_return_if_reached();
}

static gboolean
status_text_changed(GntEntry *entry, const char *text, gpointer null)
{
	if ((text[0] == 27 || (text[0] == '\t' && text[1] == '\0')) && ggblist->typing == 0)
		return FALSE;

	if (ggblist->typing)
		g_source_remove(ggblist->typing);
	ggblist->typing = 0;

	if (text[0] == '\r' && text[1] == 0)
	{
		/* Set the status only after you press 'Enter' */
		remove_typing_cb(NULL);
		return TRUE;
	}

	ggblist->typing = g_timeout_add(TYPING_TIMEOUT, (GSourceFunc)remove_typing_cb, NULL);
	return FALSE;
}

static void
savedstatus_changed(GaimSavedStatus *now, GaimSavedStatus *old)
{
	GList *list;
	GaimStatusPrimitive prim;
	const char *message;

	if (!ggblist)
		return;

	/* Block the signals we don't want to emit */
	g_signal_handlers_block_matched(ggblist->status, G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, status_selection_changed, NULL);
	g_signal_handlers_block_matched(ggblist->statustext, G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, status_text_changed, NULL);

	prim = gaim_savedstatus_get_type(now);
	message = gaim_savedstatus_get_message(now);

	/* Rebuild the status dropdown */
	populate_status_dropdown();

	list = g_object_get_data(G_OBJECT(ggblist->status), "list of statuses");
	for (; list; list = list->next)
	{
		StatusBoxItem *item = list->data;
		if (item->type == STATUS_PRIMITIVE && item->u.prim == prim)
		{
			char *mess = gaim_unescape_html(message);
			gnt_combo_box_set_selected(GNT_COMBO_BOX(ggblist->status), item);
			gnt_entry_set_text(GNT_ENTRY(ggblist->statustext), mess);
			gnt_widget_draw(ggblist->status);
			g_free(mess);
			break;
		}
	}

	g_signal_handlers_unblock_matched(ggblist->status, G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, status_selection_changed, NULL);
	g_signal_handlers_unblock_matched(ggblist->statustext, G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, status_text_changed, NULL);
}

static int
blist_node_compare_text(GaimBlistNode *n1, GaimBlistNode *n2)
{
	const char *s1, *s2;
	char *us1, *us2;
	int ret;
	
	g_return_val_if_fail(n1->type == n2->type, -1);
	
	switch (n1->type)
	{
		case GAIM_BLIST_GROUP_NODE:
			s1 = ((GaimGroup*)n1)->name;
			s2 = ((GaimGroup*)n2)->name;
			break;
		case GAIM_BLIST_CHAT_NODE:
			s1 = gaim_chat_get_name((GaimChat*)n1);
			s2 = gaim_chat_get_name((GaimChat*)n2);
			break;
		case GAIM_BLIST_BUDDY_NODE:
			return gaim_presence_compare(gaim_buddy_get_presence((GaimBuddy*)n1),
					gaim_buddy_get_presence((GaimBuddy*)n2));
			break;
		case GAIM_BLIST_CONTACT_NODE:
			s1 = gaim_contact_get_alias((GaimContact*)n1);
			s2 = gaim_contact_get_alias((GaimContact*)n2);
			break;
		default:
			return -1;
	}

	us1 = g_utf8_strup(s1, -1);
	us2 = g_utf8_strup(s2, -1);
	ret = g_utf8_collate(us1, us2);
	g_free(us1);
	g_free(us2);

	return ret;
}

static int
blist_node_compare_status(GaimBlistNode *n1, GaimBlistNode *n2)
{
	int ret;

	g_return_val_if_fail(n1->type == n2->type, -1);

	switch (n1->type) {
		case GAIM_BLIST_CONTACT_NODE:
			n1 = (GaimBlistNode*)gaim_contact_get_priority_buddy((GaimContact*)n1);
			n2 = (GaimBlistNode*)gaim_contact_get_priority_buddy((GaimContact*)n2);
			/* now compare the presence of the priority buddies */
		case GAIM_BLIST_BUDDY_NODE:
			ret = gaim_presence_compare(gaim_buddy_get_presence((GaimBuddy*)n1),
					gaim_buddy_get_presence((GaimBuddy*)n2));
			if (ret != 0)
				return ret;
			break;
		default:
			break;
	}

	/* Sort alphabetically if presence is not comparable */
	ret = blist_node_compare_text(n1, n2);

	return ret;
}

static int
get_contact_log_size(GaimBlistNode *c)
{
	int log = 0;
	GaimBlistNode *node;

	for (node = c->child; node; node = node->next) {
		GaimBuddy *b = (GaimBuddy*)node;
		log += gaim_log_get_total_size(GAIM_LOG_IM, b->name, b->account);
	}

	return log;
}

static int
blist_node_compare_log(GaimBlistNode *n1, GaimBlistNode *n2)
{
	int ret;
	GaimBuddy *b1, *b2;

	g_return_val_if_fail(n1->type == n2->type, -1);

	switch (n1->type) {
		case GAIM_BLIST_BUDDY_NODE:
			b1 = (GaimBuddy*)n1;
			b2 = (GaimBuddy*)n2;
			ret = gaim_log_get_total_size(GAIM_LOG_IM, b2->name, b2->account) - 
					gaim_log_get_total_size(GAIM_LOG_IM, b1->name, b1->account);
			if (ret != 0)
				return ret;
			break;
		case GAIM_BLIST_CONTACT_NODE:
			ret = get_contact_log_size(n2) - get_contact_log_size(n1);
			if (ret != 0)
				return ret;
			break;
		default:
			break;
	}
	ret = blist_node_compare_text(n1, n2);
	return ret;
}

static gboolean
blist_clicked(GntTree *tree, GntMouseEvent event, int x, int y, gpointer ggblist)
{
	if (event == GNT_RIGHT_MOUSE_DOWN) {
		draw_context_menu(ggblist);
	}
	return FALSE;
}

static void
plugin_action(GntMenuItem *item, gpointer data)
{
	GaimPluginAction *action = data;
	if (action && action->callback)
		action->callback(action);
}

static void
build_plugin_actions(GntMenuItem *item, GaimPlugin *plugin, gpointer context)
{
	GntWidget *sub = gnt_menu_new(GNT_MENU_POPUP);
	GList *actions;
	GntMenuItem *menuitem;

	gnt_menuitem_set_submenu(item, GNT_MENU(sub));
	for (actions = GAIM_PLUGIN_ACTIONS(plugin, context); actions;
			actions = g_list_delete_link(actions, actions)) {
		if (actions->data) {
			GaimPluginAction *action = actions->data;
			action->plugin = plugin;
			action->context = context;
			menuitem = gnt_menuitem_new(action->label);
			gnt_menu_add_item(GNT_MENU(sub), menuitem);

			gnt_menuitem_set_callback(menuitem, plugin_action, action);
			g_object_set_data_full(G_OBJECT(menuitem), "plugin_action",
								   action, (GDestroyNotify)gaim_plugin_action_free);
		}
	}
}

static void
reconstruct_plugins_menu()
{
	GntWidget *sub;
	GntMenuItem *plg;
	GList *iter;

	if (!ggblist)
		return;

	if (ggblist->plugins == NULL)
		ggblist->plugins = gnt_menuitem_new(_("Plugins"));

	plg = ggblist->plugins;
	sub = gnt_menu_new(GNT_MENU_POPUP);
	gnt_menuitem_set_submenu(plg, GNT_MENU(sub));

	for (iter = gaim_plugins_get_loaded(); iter; iter = iter->next) {
		GaimPlugin *plugin = iter->data;
		GntMenuItem *item;
		if (GAIM_IS_PROTOCOL_PLUGIN(plugin))
			continue;

		if (!GAIM_PLUGIN_HAS_ACTIONS(plugin))
			continue;

		item = gnt_menuitem_new(_(plugin->info->name));
		gnt_menu_add_item(GNT_MENU(sub), item);
		build_plugin_actions(item, plugin, NULL);
	}
}

static void
reconstruct_accounts_menu()
{
	GntWidget *sub;
	GntMenuItem *acc, *item;
	GList *iter;

	if (!ggblist)
		return;

	if (ggblist->accounts == NULL)
		ggblist->accounts = gnt_menuitem_new(_("Accounts"));

	acc = ggblist->accounts;
	sub = gnt_menu_new(GNT_MENU_POPUP);
	gnt_menuitem_set_submenu(acc, GNT_MENU(sub));

	for (iter = gaim_accounts_get_all_active(); iter;
			iter = g_list_delete_link(iter, iter)) {
		GaimAccount *account = iter->data;
		GaimConnection *gc = gaim_account_get_connection(account);
		GaimPlugin *prpl;
		
		if (!gc || !GAIM_CONNECTION_IS_CONNECTED(gc))
			continue;
		prpl = gc->prpl;

		if (GAIM_PLUGIN_HAS_ACTIONS(prpl)) {
			item = gnt_menuitem_new(gaim_account_get_username(account));
			gnt_menu_add_item(GNT_MENU(sub), item);
			build_plugin_actions(item, prpl, gc);
		}
	}
}

static void show_offline_cb(GntMenuItem *item, gpointer n)
{
	gaim_prefs_set_bool(PREF_ROOT "/showoffline",
		!gaim_prefs_get_bool(PREF_ROOT "/showoffline"));
}

static void sort_blist_change_cb(GntMenuItem *item, gpointer n)
{
	gaim_prefs_set_string(PREF_ROOT "/sort_type", n);
}

/* XXX: send_im_select* -- Xerox */
static void
send_im_select_cb(gpointer data, GaimRequestFields *fields)
{
	GaimAccount *account;
	const char *username;

	account  = gaim_request_fields_get_account(fields, "account");
	username = gaim_request_fields_get_string(fields,  "screenname");

	gaim_conversation_new(GAIM_CONV_TYPE_IM, account, username);
}

static void
send_im_select(GntMenuItem *item, gpointer n)
{
	GaimRequestFields *fields;
	GaimRequestFieldGroup *group;
	GaimRequestField *field;

	fields = gaim_request_fields_new();

	group = gaim_request_field_group_new(NULL);
	gaim_request_fields_add_group(fields, group);

	field = gaim_request_field_string_new("screenname", _("_Name"), NULL, FALSE);
	gaim_request_field_set_type_hint(field, "screenname");
	gaim_request_field_set_required(field, TRUE);
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_account_new("account", _("_Account"), NULL);
	gaim_request_field_set_type_hint(field, "account");
	gaim_request_field_set_visible(field,
		(gaim_connections_get_all() != NULL &&
		 gaim_connections_get_all()->next != NULL));
	gaim_request_field_set_required(field, TRUE);
	gaim_request_field_group_add_field(group, field);

	gaim_request_fields(gaim_get_blist(), _("New Instant Message"),
						NULL,
						_("Please enter the screen name or alias of the person "
						  "you would like to IM."),
						fields,
						_("OK"), G_CALLBACK(send_im_select_cb),
						_("Cancel"), NULL,
						NULL);
}

static void
create_menu()
{
	GntWidget *menu, *sub;
	GntMenuItem *item;
	GntWindow *window;

	if (!ggblist)
		return;

	window = GNT_WINDOW(ggblist->window);
	ggblist->menu = menu = gnt_menu_new(GNT_MENU_TOPLEVEL);
	gnt_window_set_menu(window, GNT_MENU(menu));

	item = gnt_menuitem_new(_("Options"));
	gnt_menu_add_item(GNT_MENU(menu), item);

	sub = gnt_menu_new(GNT_MENU_POPUP);
	gnt_menuitem_set_submenu(item, GNT_MENU(sub));

	item = gnt_menuitem_new(_("Send IM..."));
	gnt_menu_add_item(GNT_MENU(sub), item);
	gnt_menuitem_set_callback(GNT_MENUITEM(item), send_im_select, NULL);

	item = gnt_menuitem_new(_("Toggle offline buddies"));
	gnt_menu_add_item(GNT_MENU(sub), item);
	gnt_menuitem_set_callback(GNT_MENUITEM(item), show_offline_cb, NULL);

	item = gnt_menuitem_new(_("Sort by status"));
	gnt_menu_add_item(GNT_MENU(sub), item);
	gnt_menuitem_set_callback(GNT_MENUITEM(item), sort_blist_change_cb, "status");

	item = gnt_menuitem_new(_("Sort alphabetically"));
	gnt_menu_add_item(GNT_MENU(sub), item);
	gnt_menuitem_set_callback(GNT_MENUITEM(item), sort_blist_change_cb, "text");

	item = gnt_menuitem_new(_("Sort by log size"));
	gnt_menu_add_item(GNT_MENU(sub), item);
	gnt_menuitem_set_callback(GNT_MENUITEM(item), sort_blist_change_cb, "log");

	reconstruct_accounts_menu();
	gnt_menu_add_item(GNT_MENU(menu), ggblist->accounts);

	reconstruct_plugins_menu();
	gnt_menu_add_item(GNT_MENU(menu), ggblist->plugins);
}

void gg_blist_show()
{
	blist_show(gaim_get_blist());
}

static void
blist_show(GaimBuddyList *list)
{
	if (ggblist == NULL)
		new_list(list);
	else if (ggblist->window)
		return;

	ggblist->window = gnt_vwindow_new(FALSE);
	gnt_widget_set_name(ggblist->window, "buddylist");
	gnt_box_set_toplevel(GNT_BOX(ggblist->window), TRUE);
	gnt_box_set_title(GNT_BOX(ggblist->window), _("Buddy List"));
	gnt_box_set_pad(GNT_BOX(ggblist->window), 0);

	ggblist->tree = gnt_tree_new();

	GNT_WIDGET_SET_FLAGS(ggblist->tree, GNT_WIDGET_NO_BORDER);
	gnt_widget_set_size(ggblist->tree, gaim_prefs_get_int(PREF_ROOT "/size/width"),
			gaim_prefs_get_int(PREF_ROOT "/size/height"));
	gnt_widget_set_position(ggblist->window, gaim_prefs_get_int(PREF_ROOT "/position/x"),
			gaim_prefs_get_int(PREF_ROOT "/position/y"));

	gnt_tree_set_col_width(GNT_TREE(ggblist->tree), 0,
			gaim_prefs_get_int(PREF_ROOT "/size/width") - 1);

	gnt_box_add_widget(GNT_BOX(ggblist->window), ggblist->tree);

	ggblist->status = gnt_combo_box_new();
	gnt_box_add_widget(GNT_BOX(ggblist->window), ggblist->status);
	ggblist->statustext = gnt_entry_new(NULL);
	gnt_box_add_widget(GNT_BOX(ggblist->window), ggblist->statustext);

	gnt_widget_show(ggblist->window);

	gaim_signal_connect(gaim_connections_get_handle(), "signed-on", gg_blist_get_handle(),
				GAIM_CALLBACK(reconstruct_accounts_menu), NULL);
	gaim_signal_connect(gaim_connections_get_handle(), "signed-off", gg_blist_get_handle(),
				GAIM_CALLBACK(reconstruct_accounts_menu), NULL);
	gaim_signal_connect(gaim_blist_get_handle(), "buddy-status-changed", gg_blist_get_handle(),
				GAIM_CALLBACK(buddy_status_changed), ggblist);
	gaim_signal_connect(gaim_blist_get_handle(), "buddy-idle-changed", gg_blist_get_handle(),
				GAIM_CALLBACK(buddy_idle_changed), ggblist);

	gaim_signal_connect(gaim_plugins_get_handle(), "plugin-load", gg_blist_get_handle(),
				GAIM_CALLBACK(reconstruct_plugins_menu), NULL);
	gaim_signal_connect(gaim_plugins_get_handle(), "plugin-unload", gg_blist_get_handle(),
				GAIM_CALLBACK(reconstruct_plugins_menu), NULL);

#if 0
	gaim_signal_connect(gaim_blist_get_handle(), "buddy-signed-on", gg_blist_get_handle(),
				GAIM_CALLBACK(buddy_signed_on), ggblist);
	gaim_signal_connect(gaim_blist_get_handle(), "buddy-signed-off", gg_blist_get_handle(),
				GAIM_CALLBACK(buddy_signed_off), ggblist);

	/* These I plan to use to indicate unread-messages etc. */
	gaim_signal_connect(gaim_conversations_get_handle(), "received-im-msg", gg_blist_get_handle(),
				GAIM_CALLBACK(received_im_msg), list);
	gaim_signal_connect(gaim_conversations_get_handle(), "sent-im-msg", gg_blist_get_handle(),
				GAIM_CALLBACK(sent_im_msg), NULL);

	gaim_signal_connect(gaim_conversations_get_handle(), "received-chat-msg", gg_blist_get_handle(),
				GAIM_CALLBACK(received_chat_msg), list);
#endif

	g_signal_connect(G_OBJECT(ggblist->tree), "selection_changed", G_CALLBACK(selection_changed), ggblist);
	g_signal_connect(G_OBJECT(ggblist->tree), "key_pressed", G_CALLBACK(key_pressed), ggblist);
	g_signal_connect(G_OBJECT(ggblist->tree), "context-menu", G_CALLBACK(context_menu), ggblist);
	g_signal_connect_after(G_OBJECT(ggblist->tree), "clicked", G_CALLBACK(blist_clicked), ggblist);
	g_signal_connect(G_OBJECT(ggblist->tree), "activate", G_CALLBACK(selection_activate), ggblist);
	g_signal_connect_data(G_OBJECT(ggblist->tree), "gained-focus", G_CALLBACK(draw_tooltip),
				ggblist, 0, G_CONNECT_AFTER | G_CONNECT_SWAPPED);
	g_signal_connect_data(G_OBJECT(ggblist->tree), "lost-focus", G_CALLBACK(remove_peripherals),
				ggblist, 0, G_CONNECT_AFTER | G_CONNECT_SWAPPED);
	g_signal_connect(G_OBJECT(ggblist->tree), "size_changed", G_CALLBACK(size_changed_cb), NULL);
	g_signal_connect(G_OBJECT(ggblist->window), "position_set", G_CALLBACK(save_position_cb), NULL);
	g_signal_connect(G_OBJECT(ggblist->window), "destroy", G_CALLBACK(reset_blist_window), NULL);

	/* Status signals */
	gaim_signal_connect(gaim_savedstatuses_get_handle(), "savedstatus-changed", gg_blist_get_handle(),
				GAIM_CALLBACK(savedstatus_changed), NULL);
	g_signal_connect(G_OBJECT(ggblist->status), "selection_changed",
				G_CALLBACK(status_selection_changed), NULL);
	g_signal_connect(G_OBJECT(ggblist->statustext), "key_pressed",
				G_CALLBACK(status_text_changed), NULL);

	create_menu();

	populate_buddylist();

	savedstatus_changed(gaim_savedstatus_get_current(), NULL);
}

void gg_blist_uninit()
{
	if (ggblist == NULL)
		return;

	gnt_widget_destroy(ggblist->window);
	g_free(ggblist);
	ggblist = NULL;
}

gboolean gg_blist_get_position(int *x, int *y)
{
	if (!ggblist || !ggblist->window)
		return FALSE;
	gnt_widget_get_position(ggblist->window, x, y);
	return TRUE;
}

void gg_blist_set_position(int x, int y)
{
	gnt_widget_set_position(ggblist->window, x, y);
}

gboolean gg_blist_get_size(int *width, int *height)
{
	if (!ggblist || !ggblist->window)
		return FALSE;
	gnt_widget_get_size(ggblist->window, width, height);
	return TRUE;
}

void gg_blist_set_size(int width, int height)
{
	gnt_widget_set_size(ggblist->window, width, height);
}

