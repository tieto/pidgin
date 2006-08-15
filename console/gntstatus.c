#include <gnt.h>
#include <gntbox.h>
#include <gntbutton.h>
#include <gntentry.h>
#include <gntlabel.h>
#include <gnttree.h>

#include <request.h>

#include "gntgaim.h"
#include "gntstatus.h"

static struct
{
	GntWidget *window;
	GntWidget *tree;
} statuses;

static void
reset_status_window(GntWidget *widget, gpointer null)
{
	statuses.window = NULL;
	statuses.tree = NULL;
}

static void
populate_statuses(GntTree *tree)
{
	const GList *list;

	for (list = gaim_savedstatuses_get_all(); list; list = list->next)
	{
		GaimSavedStatus *saved = list->data;
		const char *title, *type, *message;

		if (gaim_savedstatus_is_transient(saved))
			continue;

		title = gaim_savedstatus_get_title(saved);
		type = gaim_primitive_get_name_from_type(gaim_savedstatus_get_type(saved));
		message = gaim_savedstatus_get_message(saved);  /* XXX: Strip possible markups */

		gnt_tree_add_row_last(tree, saved,
				gnt_tree_create_row(tree, title, type, message), NULL);
	}
}

static void
really_delete_status(GaimSavedStatus *saved)
{
	/* XXX: Close any modify dialog opened for the savedstatus */
	if (statuses.tree)
		gnt_tree_remove(GNT_TREE(statuses.tree), saved);

	gaim_savedstatus_delete(gaim_savedstatus_get_title(saved));
}

static void
ask_before_delete(GntWidget *button, gpointer null)
{
	char *ask;
	GaimSavedStatus *saved;

	g_return_if_fail(statuses.tree != NULL);

	saved = gnt_tree_get_selection_data(GNT_TREE(statuses.tree));
	ask = g_strdup_printf(_("Are you sure you want to delete \"%s\""),
			gaim_savedstatus_get_title(saved));

	gaim_request_action(saved, _("Delete Status"), ask, NULL, 0, saved, 2,
			_("Delete"), really_delete_status, _("Cancel"), NULL);
	g_free(ask);
}

static void
use_savedstatus_cb(GntWidget *widget, gpointer null)
{
	g_return_if_fail(statuses.tree != NULL);

	gaim_savedstatus_activate(gnt_tree_get_selection_data(GNT_TREE(statuses.tree)));
}

void gg_savedstatus_show_all()
{
	GntWidget *window, *tree, *box, *button;
	if (statuses.window)
		return;

	statuses.window = window = gnt_vbox_new(FALSE);
	gnt_box_set_toplevel(GNT_BOX(window), TRUE);
	gnt_box_set_title(GNT_BOX(window), _("Saved Statuses"));
	gnt_box_set_fill(GNT_BOX(window), FALSE);
	gnt_box_set_alignment(GNT_BOX(window), GNT_ALIGN_MID);
	gnt_box_set_pad(GNT_BOX(window), 0);

	/* XXX: Add some sorting function to sort alphabetically, perhaps */
	statuses.tree = tree = gnt_tree_new_with_columns(3);
	gnt_tree_set_column_titles(GNT_TREE(tree), _("Title"), _("Type"), _("Message"));
	gnt_tree_set_show_title(GNT_TREE(tree), TRUE);
	gnt_tree_set_col_width(GNT_TREE(tree), 0, 25);
	gnt_tree_set_col_width(GNT_TREE(tree), 1, 12);
	gnt_tree_set_col_width(GNT_TREE(tree), 2, 35);
	gnt_box_add_widget(GNT_BOX(window), tree);

	populate_statuses(GNT_TREE(tree));

	box = gnt_hbox_new(FALSE);
	gnt_box_add_widget(GNT_BOX(window), box);

	button = gnt_button_new(_("Use"));
	gnt_box_add_widget(GNT_BOX(box), button);
	g_signal_connect(G_OBJECT(button), "activate",
			G_CALLBACK(use_savedstatus_cb), NULL);

	button = gnt_button_new(_("Add"));
	gnt_box_add_widget(GNT_BOX(box), button);

	button = gnt_button_new(_("Edit"));
	gnt_box_add_widget(GNT_BOX(box), button);

	button = gnt_button_new(_("Delete"));
	gnt_box_add_widget(GNT_BOX(box), button);
	g_signal_connect(G_OBJECT(button), "activate",
			G_CALLBACK(ask_before_delete), NULL);

	button = gnt_button_new(_("Close"));
	gnt_box_add_widget(GNT_BOX(box), button);
	g_signal_connect_swapped(G_OBJECT(button), "activate",
			G_CALLBACK(gnt_widget_destroy), window);

	g_signal_connect(G_OBJECT(window), "destroy",
			G_CALLBACK(reset_status_window), NULL);
	gnt_widget_show(window);
}

void gg_savedstatus_edit(GaimSavedStatus *saved)
{
}

