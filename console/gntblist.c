#include <gaim/account.h>
#include <gaim/blist.h>
#include <signal.h>
#include <gaim/util.h>
#include <gaim/server.h>

#include "gntgaim.h"
#include "gntbox.h"
#include "gnttree.h"

#include "gntblist.h"

#define	TAB_SIZE 3

typedef struct
{
	GntWidget *window;
	GntWidget *tree;

	GaimBuddyList *list;
} GGBlist;

GGBlist *ggblist;

static void add_buddy(GaimBuddy *buddy, GGBlist *ggblist);
static void add_group(GaimGroup *group, GGBlist *ggblist);

static void
new_node(GaimBlistNode *node)
{
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
	gnt_tree_add_row_after(GNT_TREE(ggblist->tree), group,
			group->name, NULL, NULL);
	node->ui_data = GINT_TO_POINTER(TRUE);
}

static void
add_buddy(GaimBuddy *buddy, GGBlist *ggblist)
{
	char *text;
	GaimGroup *group;
	GaimBlistNode *node = (GaimBlistNode *)buddy;
	if (node->ui_data)
		return;

	node->ui_data = GINT_TO_POINTER(TRUE);
	group = gaim_buddy_get_group(buddy);
	add_group(group, ggblist);

	text = g_strdup_printf("%*s%s", TAB_SIZE, "", gaim_buddy_get_alias(buddy));
	gnt_tree_add_row_after(GNT_TREE(ggblist->tree), buddy, text, group, NULL);
	g_free(text);
}

static void
buddy_signed_on(GaimBuddy *buddy, GGBlist *ggblist)
{
	add_buddy(buddy, ggblist);
}

static void
buddy_signed_off(GaimBuddy *buddy, GGBlist *ggblist)
{
	node_remove(ggblist->list, (GaimBlistNode*)buddy);
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

void gg_blist_init()
{
	ggblist = g_new0(GGBlist, 1);

	gaim_get_blist()->ui_data = ggblist;

	ggblist->window = gnt_box_new(FALSE, FALSE);
	GNT_WIDGET_UNSET_FLAGS(ggblist->window, GNT_WIDGET_NO_BORDER | GNT_WIDGET_NO_SHADOW);
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

	g_signal_connect(G_OBJECT(ggblist->tree), "activate", G_CALLBACK(selection_activate), ggblist);

	/*gaim_signal_connect(gaim_conversations_get_handle(), "received-im-msg", gg_blist_get_handle(),*/
				/*GAIM_CALLBACK(received_im_msg), list);*/
	/*gaim_signal_connect(gaim_conversations_get_handle(), "sent-im-msg", gg_blist_get_handle(),*/
				/*GAIM_CALLBACK(sent_im_msg), NULL);*/

	/*gaim_signal_connect(gaim_conversations_get_handle(), "received-chat-msg", gg_blist_get_handle(),*/
				/*GAIM_CALLBACK(received_chat_msg), list);*/
}

