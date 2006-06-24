#include <gaim/account.h>
#include <gaim/blist.h>
#include <signal.h>
#include <gaim/util.h>
#include <gaim/server.h>

#include "gntgaim.h"
#include "gntbox.h"
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
static void draw_tooltip(GGBlist *ggblist);

static void
new_node(GaimBlistNode *node)
{
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
		else if (ggblist->tnode == (GaimBlistNode *)group)	/* Need to update the counts */
			draw_tooltip(ggblist);
	}

	if (ggblist->tnode == node)
	{
		remove_tooltip(ggblist);
	}
}

static void
node_update(GaimBuddyList *list, GaimBlistNode *node)
{
	if (GAIM_BLIST_NODE_IS_BUDDY(node))
	{
		GaimBuddy *buddy = (GaimBuddy*)node;
		if (gaim_presence_is_online(gaim_buddy_get_presence(buddy)))
			add_buddy(buddy, list->ui_data);
		else
			node_remove(gaim_get_blist(), node);
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
get_buddy_display_name(GaimBuddy *buddy)
{
	static char text[2096];
	char status[8];
	GaimStatusPrimitive prim;
	GaimPresence *presence;
	GaimStatus *now;

	presence = gaim_buddy_get_presence(buddy);
	now = gaim_presence_get_active_status(presence);

	prim = gaim_status_type_get_primitive(gaim_status_get_type(now));

	switch(prim)
	{
#if 1
		case GAIM_STATUS_OFFLINE:
			strncpy(status, "x", sizeof(status) - 1);
			break;
		case GAIM_STATUS_AVAILABLE:
			strncpy(status, "o", sizeof(status) - 1);
			break;
		default:
			strncpy(status, ".", sizeof(status) - 1);
			break;
#else
		/* XXX: Let's use these some time */
		case GAIM_STATUS_OFFLINE:
			strncpy(status, "⊗", sizeof(status) - 1);
			break;
		case GAIM_STATUS_AVAILABLE:
			/* XXX: Detect idleness */
			strncpy(status, "◯", sizeof(status) - 1);
			break;
		default:
			strncpy(status, "⊖", sizeof(status) - 1);
			break;
#endif
	}

	snprintf(text, sizeof(text) - 1, "%s %s", status, gaim_buddy_get_alias(buddy));

	return text;
}

static void
add_buddy(GaimBuddy *buddy, GGBlist *ggblist)
{
	GaimGroup *group;
	GaimBlistNode *node = (GaimBlistNode *)buddy;
	if (node->ui_data)
		return;

	group = gaim_buddy_get_group(buddy);
	add_group(group, ggblist);

	node->ui_data = gnt_tree_add_row_after(GNT_TREE(ggblist->tree), buddy,
				get_buddy_display_name(buddy), group, NULL);

	if (ggblist->tnode == (GaimBlistNode*)group)
		draw_tooltip(ggblist);
}

static void
buddy_signed_on(GaimBuddy *buddy, GGBlist *ggblist)
{
	add_buddy(buddy, ggblist);
}

static void
buddy_signed_off(GaimBuddy *buddy, GGBlist *ggblist)
{
	node_remove(gaim_get_blist(), (GaimBlistNode*)buddy);
}

GaimBlistUiOps *gg_blist_get_ui_ops()
{
	return &blist_ui_ops;
}

static void
selection_activate(GntWidget *widget, GGBlist *ggblist)
{
	gnt_widget_set_focus(widget, FALSE);
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
	GntWidget *widget, *box, *label;
	char *title = NULL;

	widget = ggblist->tree;
	tree = GNT_TREE(widget);

	if (ggblist->tooltip)
	{
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

	gnt_box_add_widget(GNT_BOX(box), GNT_WIDGET(gnt_label_new(str->str)));

	gnt_widget_set_position(box, x, y);
	gnt_widget_draw(box);

	g_free(title);
	g_string_free(str, TRUE);
	ggblist->tooltip = box;
	ggblist->tnode = node;
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
	gnt_tree_change_text(GNT_TREE(ggblist->tree), buddy, get_buddy_display_name(buddy));
	if (ggblist->tnode == (GaimBlistNode*)buddy)
		draw_tooltip(ggblist);
}

void gg_blist_init()
{
	ggblist = g_new0(GGBlist, 1);

	gaim_get_blist()->ui_data = ggblist;

	ggblist->window = gnt_box_new(FALSE, FALSE);
	gnt_box_set_toplevel(GNT_BOX(ggblist->window), TRUE);
	gnt_box_set_title(GNT_BOX(ggblist->window), _("Buddy List"));
	gnt_box_set_pad(GNT_BOX(ggblist->window), 0);

	ggblist->tree = gnt_tree_new();
	GNT_WIDGET_SET_FLAGS(ggblist->tree, GNT_WIDGET_NO_BORDER);
	gnt_widget_set_size(ggblist->tree, 25, getmaxy(stdscr) - 2);

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
}

