#include <account.h>
#include <blist.h>
#include <notify.h>
#include <request.h>
#include <savedstatuses.h>
#include <server.h>
#include <signal.h>
#include <status.h>
#include <util.h>

#include "gntgaim.h"
#include "gntbox.h"
#include "gntcombobox.h"
#include "gntentry.h"
#include "gntlabel.h"
#include "gntline.h"
#include "gnttree.h"

#include "gntblist.h"
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

	GntWidget *context;
	GaimBlistNode *cnode;

	/* XXX: I am KISSing */
	GntWidget *status;          /* Dropdown with the statuses  */
	GntWidget *statustext;      /* Status message */
	int typing;
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

	if (GAIM_BLIST_NODE_IS_BUDDY(node))
	{
		GaimContact *contact = (GaimContact*)node->parent;
		if (contact->online < 1)
			node_remove(list, (GaimBlistNode*)contact);
	}
	draw_tooltip(ggblist);
}

static void
node_update(GaimBuddyList *list, GaimBlistNode *node)
{
	if (list->ui_data == NULL)
		return;

	if (node->ui_data != NULL) {
		gnt_tree_change_text(GNT_TREE(ggblist->tree), node,
				0, get_display_name(node));
	}

	if (GAIM_BLIST_NODE_IS_BUDDY(node)) {
		GaimBuddy *buddy = (GaimBuddy*)node;
		if (gaim_presence_is_online(gaim_buddy_get_presence(buddy)))
			add_node((GaimBlistNode*)buddy, list->ui_data);
		else
			node_remove(gaim_get_blist(), node);

		node_update(list, node->parent);
	} else if (GAIM_BLIST_NODE_IS_CHAT(node)) {
		add_chat((GaimChat *)node, list->ui_data);
	}
}

static void
new_list(GaimBuddyList *list)
{
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

	field = gaim_request_field_string_new("group", _("Group"), grp->name, FALSE);
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
	NULL,
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
	gnt_tree_remove(GNT_TREE(ggblist->tree), group);
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

	gnt_tree_remove(GNT_TREE(ggblist->tree), chat);
	node->ui_data = gnt_tree_add_row_after(GNT_TREE(ggblist->tree), chat,
				gnt_tree_create_row(GNT_TREE(ggblist->tree), get_display_name(node)),
				group, NULL);

	if (gaim_blist_node_get_bool((GaimBlistNode*)chat, "gnt-autojoin"))
		serv_join_chat(gaim_account_get_connection(chat->account), chat->components);
}

static void
add_contact(GaimContact *contact, GGBlist *ggblist)
{
	GaimGroup *group;
	GaimBlistNode *node = (GaimBlistNode*)contact;

	if (node->ui_data)
		return;
	
	group = (GaimGroup*)node->parent;
	add_node((GaimBlistNode*)group, ggblist);

	node->ui_data = gnt_tree_add_row_after(GNT_TREE(ggblist->tree), contact,
				gnt_tree_create_row(GNT_TREE(ggblist->tree), get_display_name(node)),
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
	add_node((GaimBlistNode*)contact, ggblist);

	gnt_tree_remove(GNT_TREE(ggblist->tree), buddy);
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
		gaim_conversation_new(GAIM_CONV_TYPE_IM,
				gaim_buddy_get_account(buddy),
				gaim_buddy_get_name(buddy));
	}
	else if (GAIM_BLIST_NODE_IS_CHAT(node))
	{
		GaimChat *chat = (GaimChat*)node;
		serv_join_chat(chat->account->gc, chat->components);
	}
}

static void
remove_context_menu(GGBlist *ggblist)
{
	if (ggblist->context)
		gnt_widget_destroy(ggblist->context->parent);
	ggblist->context = NULL;
	ggblist->cnode = NULL;
}

static void
gnt_append_menu_action(GntTree *tree, GaimMenuAction *action, gpointer parent)
{
	GList *list;
	if (action == NULL)
		return;

	gnt_tree_add_row_last(tree, action,
			gnt_tree_create_row(tree, action->label), parent);
	for (list = action->children; list; list = list->next)
		gnt_append_menu_action(tree, list->data, action);
}

static void
append_proto_menu(GntTree *tree, GaimConnection *gc, GaimBlistNode *node)
{
	GList *list;
	GaimPluginProtocolInfo *prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if(!prpl_info || !prpl_info->blist_node_menu)
		return;

	for(list = prpl_info->blist_node_menu(node); list;
			list = g_list_delete_link(list, list))
	{
		GaimMenuAction *act = (GaimMenuAction *) list->data;
		gnt_append_menu_action(tree, act, NULL);
	}
}

static void
add_custom_action(GntTree *tree, const char *label, GaimCallback callback,
		gpointer data)
{
	GaimMenuAction *action = gaim_menu_action_new(label, callback, data, NULL);
	gnt_append_menu_action(tree, action, NULL);
	g_signal_connect_swapped(G_OBJECT(tree), "destroy",
			G_CALLBACK(gaim_menu_action_free), action);
}

static void
context_menu_toggle(GntTree *tree, GaimMenuAction *action, gpointer null)
{
	gboolean sel = gnt_tree_get_choice(tree, action);
	gaim_blist_node_set_bool(action->data, "gnt-autojoin", sel);
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
chat_components_edit(GaimChat *chat, GaimBlistNode *null)
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
create_chat_menu(GntTree *tree, GaimChat *chat)
{
	GaimMenuAction *action = gaim_menu_action_new(_("Auto-join"), NULL, chat, NULL);

	gnt_tree_add_choice(tree, action, gnt_tree_create_row(tree, action->label), NULL, NULL);
	gnt_tree_set_choice(tree, action, gaim_blist_node_get_bool((GaimBlistNode*)chat, "gnt-autojoin"));

	g_signal_connect_swapped(G_OBJECT(tree), "destroy",
			G_CALLBACK(gaim_menu_action_free), action);

	add_custom_action(tree, _("Edit Settings"), (GaimCallback)chat_components_edit, chat);
	
	g_signal_connect(G_OBJECT(tree), "toggled", G_CALLBACK(context_menu_toggle), NULL);
}

static void
gg_add_buddy(GaimGroup *grp, GaimBlistNode *node)
{
	gaim_blist_request_add_buddy(NULL, NULL, grp->name, NULL);
}

static void
gg_add_group(GaimGroup *grp, GaimBlistNode *node)
{
	gaim_blist_request_add_group();
}

static void
gg_add_chat(GaimGroup *grp, GaimBlistNode *node)
{
	gaim_blist_request_add_chat(NULL, grp, NULL, NULL);
}

static void
create_group_menu(GntTree *tree, GaimGroup *group)
{
	add_custom_action(tree, _("Add Buddy"),
			GAIM_CALLBACK(gg_add_buddy), group);
	add_custom_action(tree, _("Add Chat"),
			GAIM_CALLBACK(gg_add_chat), group);
	add_custom_action(tree, _("Add Group"),
			GAIM_CALLBACK(gg_add_group), group);
}

static void
gg_blist_get_buddy_info_cb(GaimBuddy *buddy, GaimBlistNode *null)
{
	serv_get_info(buddy->account->gc, gaim_buddy_get_name(buddy));
}

static void
create_buddy_menu(GntTree *tree, GaimBuddy *buddy)
{
	GaimPluginProtocolInfo *prpl_info;

	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(buddy->account->gc->prpl);
	if (prpl_info && prpl_info->get_info)
	{
		add_custom_action(tree, _("Get Info"),
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
	append_proto_menu(tree,
			gaim_account_get_connection(gaim_buddy_get_account(buddy)),
			(GaimBlistNode*)buddy);
}

static void
append_extended_menu(GntTree *tree, GaimBlistNode *node)
{
	GList *iter;

	for (iter = gaim_blist_node_get_extended_menu(node);
			iter; iter = g_list_delete_link(iter, iter))
	{
		gnt_append_menu_action(tree, iter->data, NULL);
	}
}

static void
context_menu_callback(GntTree *tree, GGBlist *ggblist)
{
	GaimMenuAction *action = gnt_tree_get_selection_data(tree);
	GaimBlistNode *node = ggblist->cnode;

	if (action)
	{
		void (*callback)(GaimBlistNode *, gpointer);
		callback = (void (*)(GaimBlistNode *, gpointer))action->callback;
		if (callback)
			callback(node, action->data);
		else
			return;
	}

	remove_context_menu(ggblist);
}

static void
rename_blist_node(GaimBlistNode *node, const char *newname)
{
	const char *name = newname;
	if (name && !*name)
		name = NULL;

	if (GAIM_BLIST_NODE_IS_CONTACT(node))
		gaim_blist_alias_contact((GaimContact*)node, name);
	else if (GAIM_BLIST_NODE_IS_BUDDY(node))
		gaim_blist_alias_buddy((GaimBuddy*)node, name);
	else if (GAIM_BLIST_NODE_IS_CHAT(node))
		gaim_blist_alias_chat((GaimChat*)node, name);
	else if (GAIM_BLIST_NODE_IS_GROUP(node))
		gaim_blist_rename_group((GaimGroup*)node, name);
	else
		g_return_if_reached();
}

static void
gg_blist_rename_node_cb(GaimBlistNode *node, GaimBlistNode *null)
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
		gaim_blist_remove_contact((GaimContact*)node);
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
gg_blist_remove_node_cb(GaimBlistNode *node, GaimBlistNode *null)
{
	char *primary;
	const char *name, *sec = NULL;

	/* XXX: could be a contact */
	if (GAIM_BLIST_NODE_IS_CONTACT(node))
		name = gaim_contact_get_alias((GaimContact*)node);
	else if (GAIM_BLIST_NODE_IS_BUDDY(node))
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
draw_context_menu(GGBlist *ggblist)
{
	GaimBlistNode *node = NULL;
	GntWidget *context = NULL, *window = NULL;
	GntTree *tree = NULL;
	int x, y, top, width;
	char *title = NULL;

	tree = GNT_TREE(ggblist->tree);

	if (ggblist->context)
	{
		remove_context_menu(ggblist);
	}

	node = gnt_tree_get_selection_data(tree);

	if (node == NULL)
		return;
	if (ggblist->tooltip)
		remove_tooltip(ggblist);

	ggblist->cnode = node;
	ggblist->context = context = gnt_tree_new();
	GNT_WIDGET_SET_FLAGS(context, GNT_WIDGET_NO_BORDER);
	gnt_widget_set_name(context, "context menu");
	g_signal_connect(G_OBJECT(context), "activate", G_CALLBACK(context_menu_callback), ggblist);

	if (GAIM_BLIST_NODE_IS_CONTACT(node)) {
		create_buddy_menu(GNT_TREE(context),
			gaim_contact_get_priority_buddy((GaimContact*)node));
		title = g_strdup(gaim_contact_get_alias((GaimContact*)node));
	} else if (GAIM_BLIST_NODE_IS_BUDDY(node)) {
		GaimBuddy *buddy = (GaimBuddy *)node;
		create_buddy_menu(GNT_TREE(context), buddy);
		title = g_strdup(gaim_buddy_get_name(buddy));
	} else if (GAIM_BLIST_NODE_IS_CHAT(node)) {
		GaimChat *chat = (GaimChat*)node;
		create_chat_menu(GNT_TREE(context), chat);
		title = g_strdup(gaim_chat_get_name(chat));
	} else if (GAIM_BLIST_NODE_IS_GROUP(node)) {
		GaimGroup *group = (GaimGroup *)node;
		create_group_menu(GNT_TREE(context), group);
		title = g_strdup(group->name);
	}

	append_extended_menu(GNT_TREE(context), node);

	/* These are common for everything */
	add_custom_action(GNT_TREE(context), _("Rename"),
			GAIM_CALLBACK(gg_blist_rename_node_cb), node);
	add_custom_action(GNT_TREE(context), _("Remove"),
			GAIM_CALLBACK(gg_blist_remove_node_cb), node);

	window = gnt_vbox_new(FALSE);
	GNT_WIDGET_SET_FLAGS(window, GNT_WIDGET_TRANSIENT);
	gnt_box_set_toplevel(GNT_BOX(window), TRUE);
	gnt_box_set_title(GNT_BOX(window), title);

	gnt_widget_set_size(context, 0, g_list_length(GNT_TREE(context)->list));
	gnt_box_add_widget(GNT_BOX(window), context);

	/* Set the position for the popup */
	gnt_widget_get_position(GNT_WIDGET(tree), &x, &y);
	gnt_widget_get_size(GNT_WIDGET(tree), &width, NULL);
	top = gnt_tree_get_selection_visible_line(tree);

	x += width;
	y += top - 1;

	gnt_widget_set_position(window, x, y);
	gnt_widget_draw(window);
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

static void
draw_tooltip(GGBlist *ggblist)
{
	GaimBlistNode *node;
	int x, y, top, width;
	GString *str;
	GntTree *tree;
	GntWidget *widget, *box;
	char *title = NULL;

	widget = ggblist->tree;
	tree = GNT_TREE(widget);

	if (!gnt_widget_has_focus(ggblist->tree))
		return;

	if (ggblist->context)
		return;

	if (ggblist->tooltip)
	{
		/* XXX: Once we can properly redraw on expose events, this can be removed at the end
		 * to avoid the blinking*/
		remove_tooltip(ggblist);
	}

	node = gnt_tree_get_selection_data(tree);
	if (!node)
		return;

	str = g_string_new("");

	if (GAIM_BLIST_NODE_IS_CONTACT(node)) {
		GaimBuddy *pr = gaim_contact_get_priority_buddy((GaimContact*)node);
		title = g_strdup(gaim_contact_get_alias((GaimContact*)node));
		tooltip_for_buddy(pr, str);
		for (node = node->child; node; node = node->next) {
			if (node == (GaimBlistNode*)pr || !GAIM_BUDDY_IS_ONLINE((GaimBuddy*)node))
				continue;
			str = g_string_append(str, "\n----------\n");
			g_string_append_printf(str, _("Nickname: %s\n"), gaim_buddy_get_name((GaimBuddy*)node));
			tooltip_for_buddy((GaimBuddy*)node, str);
		}
	} else if (GAIM_BLIST_NODE_IS_BUDDY(node)) {
		GaimBuddy *buddy = (GaimBuddy *)node;
		tooltip_for_buddy(buddy, str);
		title = g_strdup(gaim_buddy_get_name(buddy));
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
		return;
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

	gnt_box_add_widget(GNT_BOX(box), gnt_label_new(str->str));

	gnt_widget_set_position(box, x, y);
	GNT_WIDGET_UNSET_FLAGS(box, GNT_WIDGET_CAN_TAKE_FOCUS);
	GNT_WIDGET_SET_FLAGS(box, GNT_WIDGET_TRANSIENT);
	gnt_widget_draw(box);

	g_free(title);
	g_string_free(str, TRUE);
	ggblist->tooltip = box;
	ggblist->tnode = node;

	gnt_widget_set_name(ggblist->tooltip, "tooltip");
}

static void
selection_changed(GntWidget *widget, gpointer old, gpointer current, GGBlist *ggblist)
{
	draw_tooltip(ggblist);
}

static gboolean
key_pressed(GntWidget *widget, const char *text, GGBlist *ggblist)
{
	gboolean stop = FALSE, ret = FALSE;
	if (text[0] == 27 && text[1] == 0)
	{
		/* Escape was pressed */
		remove_peripherals(ggblist);
		stop = TRUE;
		ret = TRUE;
	}

	if (ggblist->context)
	{
		ret = gnt_widget_key_pressed(ggblist->context, text);
		stop = TRUE;
	}

	if (text[0] == 27)
	{
		if (strcmp(text + 1, GNT_KEY_POPUP) == 0)
		{
			draw_context_menu(ggblist);
			stop = TRUE;
			ret = TRUE;
		}
	}

	if (stop)
		g_signal_stop_emission_by_name(G_OBJECT(widget), "key_pressed");

	return ret;
}

static void
update_buddy_display(GaimBuddy *buddy, GGBlist *ggblist)
{
	GaimContact *contact;
	
	contact = gaim_buddy_get_contact(buddy);

	gnt_tree_change_text(GNT_TREE(ggblist->tree), buddy, 0, get_display_name((GaimBlistNode*)buddy));
	gnt_tree_change_text(GNT_TREE(ggblist->tree), contact, 0, get_display_name((GaimBlistNode*)contact));

	if (ggblist->tnode == (GaimBlistNode*)buddy)
		draw_tooltip(ggblist);

	if (gaim_presence_is_idle(gaim_buddy_get_presence(buddy))) {
		gnt_tree_set_row_flags(GNT_TREE(ggblist->tree), buddy, GNT_TEXT_FLAG_DIM);
		gnt_tree_set_row_flags(GNT_TREE(ggblist->tree), contact, GNT_TEXT_FLAG_DIM);
	} else {
		gnt_tree_set_row_flags(GNT_TREE(ggblist->tree), buddy, 0);
		gnt_tree_set_row_flags(GNT_TREE(ggblist->tree), contact, 0);
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
		remove_context_menu(ggblist);
}

static void
size_changed_cb(GntWidget *w, int width, int height)
{
	gaim_prefs_set_int(PREF_ROOT "/size/width", width);
	gaim_prefs_set_int(PREF_ROOT "/size/height", height);
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
	while (node)
	{
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

void gg_blist_init()
{
	gaim_prefs_add_none(PREF_ROOT);
	gaim_prefs_add_none(PREF_ROOT "/size");
	gaim_prefs_add_int(PREF_ROOT "/size/width", 20);
	gaim_prefs_add_int(PREF_ROOT "/size/height", 17);
	gaim_prefs_add_none(PREF_ROOT "/position");
	gaim_prefs_add_int(PREF_ROOT "/position/x", 0);
	gaim_prefs_add_int(PREF_ROOT "/position/y", 0);

	gg_blist_show();

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
	/* Block the signals we don't want to emit */
	GList *list;
	GaimStatusPrimitive prim;
	const char *message;

	if (!ggblist)
		return;

	g_signal_handlers_block_matched(ggblist->status, G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, status_selection_changed, NULL);
	g_signal_handlers_block_matched(ggblist->statustext, G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, status_text_changed, NULL);

	prim = gaim_savedstatus_get_type(now);
	message = gaim_savedstatus_get_message(now);

	list = g_object_get_data(G_OBJECT(ggblist->status), "list of statuses");
	for (; list; list = list->next)
	{
		StatusBoxItem *item = list->data;
		if (item->type == STATUS_PRIMITIVE && item->u.prim == prim)
		{
			gnt_combo_box_set_selected(GNT_COMBO_BOX(ggblist->status), item);
			gnt_entry_set_text(GNT_ENTRY(ggblist->statustext), message);
			gnt_widget_draw(ggblist->status);
			break;
		}
	}

	g_signal_handlers_unblock_matched(ggblist->status, G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, status_selection_changed, NULL);
	g_signal_handlers_unblock_matched(ggblist->statustext, G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, status_text_changed, NULL);
}

static int
blist_node_compare(GaimBlistNode *n1, GaimBlistNode *n2)
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
			/* XXX: reordering existing rows don't do well in GntTree */
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

void gg_blist_show()
{
	if (ggblist)
		return;

	ggblist = g_new0(GGBlist, 1);

	gaim_get_blist()->ui_data = ggblist;

	ggblist->window = gnt_vbox_new(FALSE);
	gnt_widget_set_name(ggblist->window, "buddylist");
	gnt_box_set_toplevel(GNT_BOX(ggblist->window), TRUE);
	gnt_box_set_title(GNT_BOX(ggblist->window), _("Buddy List"));
	gnt_box_set_pad(GNT_BOX(ggblist->window), 0);

	ggblist->tree = gnt_tree_new();
	gnt_tree_set_compare_func(GNT_TREE(ggblist->tree), (GCompareFunc)blist_node_compare);
	GNT_WIDGET_SET_FLAGS(ggblist->tree, GNT_WIDGET_NO_BORDER);
	gnt_tree_set_col_width(GNT_TREE(ggblist->tree), 0, 25);
	gnt_widget_set_size(ggblist->tree, gaim_prefs_get_int(PREF_ROOT "/size/width"),
			gaim_prefs_get_int(PREF_ROOT "/size/height"));
	gnt_widget_set_position(ggblist->window, gaim_prefs_get_int(PREF_ROOT "/position/x"),
			gaim_prefs_get_int(PREF_ROOT "/position/y"));

	gnt_box_add_widget(GNT_BOX(ggblist->window), ggblist->tree);

	ggblist->status = gnt_combo_box_new();
	gnt_box_add_widget(GNT_BOX(ggblist->window), ggblist->status);
	ggblist->statustext = gnt_entry_new(NULL);
	gnt_box_add_widget(GNT_BOX(ggblist->window), ggblist->statustext);

	populate_status_dropdown();

	gnt_widget_show(ggblist->window);

	gaim_signal_connect(gaim_blist_get_handle(), "buddy-status-changed", gg_blist_get_handle(),
				GAIM_CALLBACK(buddy_status_changed), ggblist);
	gaim_signal_connect(gaim_blist_get_handle(), "buddy-idle-changed", gg_blist_get_handle(),
				GAIM_CALLBACK(buddy_idle_changed), ggblist);

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

