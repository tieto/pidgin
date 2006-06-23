#include <gaim/account.h>
#include <gaim/blist.h>
#include <signal.h>
#include <gaim/util.h>
#include <gaim/server.h>

#include "gntgaim.h"
#include "gntbox.h"
#include "gnttree.h"

#include "gntblist.h"

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
				gaim_buddy_get_alias(buddy), group, NULL);
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
selection_changed(GntWidget *widget, int old, int current, GGBlist *ggblist)
{
	GaimBlistNode *node;
	GntTree *tree = GNT_TREE(widget);
	int x, y, top, width;
	GString *str;
	GaimPlugin *prpl;
	GaimPluginProtocolInfo *prpl_info;
	GaimAccount *account;
	GntWidget *box, *label;
	char *title;

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
	
	g_string_free(str, TRUE);
	ggblist->tooltip = box;
	ggblist->tnode = node;
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
	
	gaim_signal_connect(gaim_blist_get_handle(), "buddy-signed-on", gg_blist_get_handle(),
				GAIM_CALLBACK(buddy_signed_on), ggblist);
	gaim_signal_connect(gaim_blist_get_handle(), "buddy-signed-off", gg_blist_get_handle(),
				GAIM_CALLBACK(buddy_signed_off), ggblist);

#if 0
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

