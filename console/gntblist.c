#include <gaim/account.h>
#include <gaim/blist.h>
#include <signal.h>
#include <gaim/util.h>
#include <gaim/server.h>

#include "gntgaim.h"
#include "gntbox.h"
#include "gnttree.h"

#define	TAB_SIZE 3

/**
 * NOTES:
 *
 * 1. signal-callbacks should check for module_in_focus() before redrawing anything.
 * 2. call module_lost_focus() before opening a new window, and module_gained_focus() when
 * 		the new window is closed. This is to make sure the signal callbacks don't screw up
 * 		the display.
 */

static GaimBlistUiOps blist_ui_ops = 
{
	NULL,
	NULL,
	NULL,
	NULL,		/* This doesn't do crap */
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

typedef struct
{
	GntWidget *window;
	GntWidget *tree;
} GGBlist;

GGBlist *ggblist;

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
buddy_signed_on(GaimBuddy *buddy, GGBlist *ggblist)
{
	GaimGroup *group = gaim_buddy_get_group(buddy);
	char *text;

	add_group(group, ggblist);

	text = g_strdup_printf("%*s%s", TAB_SIZE, "", gaim_buddy_get_alias(buddy));
	gnt_tree_add_row_after(GNT_TREE(ggblist->tree), buddy, text, group, NULL);
	g_free(text);
}

static void
buddy_signed_off(GaimBuddy *buddy, GGBlist *ggblist)
{
	gnt_tree_remove(GNT_TREE(ggblist->tree), buddy);
}

GaimBlistUiOps *gg_get_blist_ui_ops()
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

	ggblist->window = gnt_box_new(FALSE, FALSE);
	gnt_box_set_title(GNT_BOX(ggblist->window), _("Buddy List"));

	ggblist->tree = gnt_tree_new();
	gnt_widget_set_size(ggblist->tree, 25, getmaxy(stdscr));

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

