#include <account.h>
#include <blist.h>
#include <server.h>
#include <signal.h>
#include <util.h>

#include "gntgaim.h"
#include "gntbox.h"
#include "gntlabel.h"
#include "gnttree.h"

#include "gntblist.h"
#include <string.h>

typedef struct
{
	GntWidget *window;
	GntWidget *tree;

	GntWidget *tooltip;
	GaimBlistNode *tnode;		/* Who is the tooltip being displayed for? */
} GGBlist;

GGBlist *ggblist;

static void add_buddy(GaimBuddy *buddy, GGBlist *ggblist);
static void add_group(GaimGroup *group, GGBlist *ggblist);
static void add_chat(GaimChat *chat, GGBlist *ggblist);
static void add_node(GaimBlistNode *node, GGBlist *ggblist);
static void draw_tooltip(GGBlist *ggblist);

static void
new_node(GaimBlistNode *node)
{
}

static void add_node(GaimBlistNode *node, GGBlist *ggblist)
{
	if (GAIM_BLIST_NODE_IS_BUDDY(node))
		add_buddy((GaimBuddy*)node, ggblist);
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

	if (node->ui_data == NULL)
		return;

	gnt_tree_remove(GNT_TREE(ggblist->tree), node);
	node->ui_data = NULL;

	/* XXX: Depending on the node, we may want to remove the group/contact node if necessary */
	if (GAIM_BLIST_NODE_IS_BUDDY(node))
	{
		GaimGroup *group = gaim_buddy_get_group((GaimBuddy*)node);
		if (gaim_blist_get_group_online_count(group) == 0)
			node_remove(list, (GaimBlistNode*)group);
	}
	draw_tooltip(ggblist);
}

static void
node_update(GaimBuddyList *list, GaimBlistNode *node)
{
	if (GAIM_BLIST_NODE_IS_BUDDY(node))
	{
		GaimBuddy *buddy = (GaimBuddy*)node;
		if (gaim_presence_is_online(gaim_buddy_get_presence(buddy)))
			add_node((GaimBlistNode*)buddy, list->ui_data);
		else
			node_remove(gaim_get_blist(), node);
	}
	else if (GAIM_BLIST_NODE_IS_CHAT(node))
	{
		add_chat((GaimChat *)node, list->ui_data);
	}
}

static void
new_list(GaimBuddyList *list)
{
}

static GaimBlistUiOps blist_ui_ops = 
{
	new_list,
	new_node,
	NULL,
	node_update,		/* This doesn't do crap */
	node_remove,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
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
			group->name, NULL, NULL);
}

static const char *
get_display_name(GaimBlistNode *node)
{
	static char text[2096];
	char status[8] = " ";
	const char *name = NULL;

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

	group = gaim_chat_get_group(chat);
	add_node((GaimBlistNode*)group, ggblist);

	node->ui_data = gnt_tree_add_row_after(GNT_TREE(ggblist->tree), chat,
				get_display_name(node), group, NULL);
}

static void
add_buddy(GaimBuddy *buddy, GGBlist *ggblist)
{
	GaimGroup *group;
	GaimBlistNode *node = (GaimBlistNode *)buddy;
	if (node->ui_data)
		return;

	group = gaim_buddy_get_group(buddy);
	add_node((GaimBlistNode*)group, ggblist);

	node->ui_data = gnt_tree_add_row_after(GNT_TREE(ggblist->tree), buddy,
				get_display_name(node), group, NULL);
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
draw_tooltip(GGBlist *ggblist)
{
	GaimBlistNode *node;
	int x, y, top, width;
	GString *str;
	GaimPlugin *prpl;
	GaimPluginProtocolInfo *prpl_info;
	GaimAccount *account;
	GntTree *tree;
	GntWidget *widget, *box;
	char *title = NULL;

	widget = ggblist->tree;
	tree = GNT_TREE(widget);

	if (!gnt_widget_has_focus(ggblist->tree))
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

	if (GAIM_BLIST_NODE_IS_BUDDY(node))
	{
		GaimBuddy *buddy = (GaimBuddy *)node;
		account = gaim_buddy_get_account(buddy);
		
		g_string_append_printf(str, _("Account: %s"), gaim_account_get_username(account));
		
		prpl = gaim_find_prpl(gaim_account_get_protocol_id(account));
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);
		if (prpl_info && prpl_info->tooltip_text)
		{
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

		title = g_strdup(gaim_buddy_get_name(buddy));
	}
	else if (GAIM_BLIST_NODE_IS_GROUP(node))
	{
		GaimGroup *group = (GaimGroup *)node;

		g_string_append_printf(str, _("Online: %d\nTotal: %d"),
						gaim_blist_get_group_online_count(group),
						gaim_blist_get_group_size(group, FALSE));

		title = g_strdup(group->name);
	}
	else if (GAIM_BLIST_NODE_IS_CHAT(node))
	{
		GaimChat *chat = (GaimChat *)node;
		GaimAccount *account = chat->account;

		g_string_append_printf(str, _("Account: %s"), gaim_account_get_username(account));

		title = g_strdup(gaim_chat_get_name(chat));
	}
	else
	{
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
	if (text[0] == 27 && text[1] == 0)
	{
		/* Escape was pressed */
		if (ggblist->tooltip)
		{
			gnt_widget_destroy(ggblist->tooltip);
			ggblist->tooltip = NULL;
			return TRUE;
		}
	}

	return FALSE;
}

static void
buddy_status_changed(GaimBuddy *buddy, GaimStatus *old, GaimStatus *now, GGBlist *ggblist)
{
	gnt_tree_change_text(GNT_TREE(ggblist->tree), buddy, get_display_name((GaimBlistNode*)buddy));
	if (ggblist->tnode == (GaimBlistNode*)buddy)
		draw_tooltip(ggblist);
}

void gg_blist_init()
{
	ggblist = g_new0(GGBlist, 1);

	gaim_get_blist()->ui_data = ggblist;

	ggblist->window = gnt_box_new(FALSE, FALSE);
	gnt_widget_set_name(ggblist->window, "buddylist");
	gnt_box_set_toplevel(GNT_BOX(ggblist->window), TRUE);
	gnt_box_set_title(GNT_BOX(ggblist->window), _("Buddy List"));
	gnt_box_set_pad(GNT_BOX(ggblist->window), 0);

	ggblist->tree = gnt_tree_new();
	GNT_WIDGET_SET_FLAGS(ggblist->tree, GNT_WIDGET_NO_BORDER);
	gnt_widget_set_size(ggblist->tree, 25, getmaxy(stdscr) - 4);

	gnt_box_add_widget(GNT_BOX(ggblist->window), ggblist->tree);
	gnt_widget_show(ggblist->window);

	gaim_signal_connect(gaim_blist_get_handle(), "buddy-status-changed", gg_blist_get_handle(),
				GAIM_CALLBACK(buddy_status_changed), ggblist);
	
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
	g_signal_connect_data(G_OBJECT(ggblist->tree), "lost-focus", G_CALLBACK(remove_tooltip),
				ggblist, 0, G_CONNECT_AFTER | G_CONNECT_SWAPPED);
}

void gg_blist_uninit()
{
	gnt_widget_destroy(ggblist->window);
	g_free(ggblist);
	ggblist = NULL;
}

void gg_blist_get_position(int *x, int *y)
{
	gnt_widget_get_position(ggblist->window, x, y);
}

void gg_blist_set_position(int x, int y)
{
	gnt_widget_set_position(ggblist->window, x, y);
}

void gg_blist_get_size(int *width, int *height)
{
	gnt_widget_get_size(ggblist->window, width, height);
}

void gg_blist_set_size(int width, int height)
{
	gnt_widget_set_size(ggblist->window, width, height);
}

