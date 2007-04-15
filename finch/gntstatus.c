/**
 * @file gntstatus.c GNT Status API
 * @ingroup gntui
 *
 * finch
 *
 * Finch is the legal property of its developers, whose names are too numerous
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
#include <gnt.h>
#include <gntbox.h>
#include <gntbutton.h>
#include <gntcombobox.h>
#include <gntentry.h>
#include <gntlabel.h>
#include <gntline.h>
#include <gnttree.h>

#include <notify.h>
#include <request.h>

#include "finch.h"
#include "gntstatus.h"

static struct
{
	GntWidget *window;
	GntWidget *tree;
} statuses;

typedef struct
{
	PurpleSavedStatus *saved;
	GntWidget *window;
	GntWidget *title;
	GntWidget *type;
	GntWidget *message;
	GntWidget *tree;
	GHashTable *hash;  /* list of windows for substatuses */
} EditStatus;

typedef struct
{
	PurpleAccount *account;
	const PurpleStatusType *type;
	char *message;
} RowInfo;

typedef struct
{
	GntWidget *window;
	GntWidget *type;
	GntWidget *message;

	EditStatus *parent;
	RowInfo *key;
} EditSubStatus;

static GList *edits;  /* List of opened edit-status dialogs */

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

	for (list = purple_savedstatuses_get_all(); list; list = list->next)
	{
		PurpleSavedStatus *saved = list->data;
		const char *title, *type, *message;

		if (purple_savedstatus_is_transient(saved))
			continue;

		title = purple_savedstatus_get_title(saved);
		type = purple_primitive_get_name_from_type(purple_savedstatus_get_type(saved));
		message = purple_savedstatus_get_message(saved);  /* XXX: Strip possible markups */

		gnt_tree_add_row_last(tree, saved,
				gnt_tree_create_row(tree, title, type, message), NULL);
	}
}

static void
really_delete_status(PurpleSavedStatus *saved)
{
	GList *iter;

	for (iter = edits; iter; iter = iter->next)
	{
		EditStatus *edit = iter->data;
		if (edit->saved == saved)
		{
			gnt_widget_destroy(edit->window);
			break;
		}
	}

	if (statuses.tree)
		gnt_tree_remove(GNT_TREE(statuses.tree), saved);

	purple_savedstatus_delete(purple_savedstatus_get_title(saved));
}

static void
ask_before_delete(GntWidget *button, gpointer null)
{
	char *ask;
	PurpleSavedStatus *saved;

	g_return_if_fail(statuses.tree != NULL);

	saved = gnt_tree_get_selection_data(GNT_TREE(statuses.tree));
	ask = g_strdup_printf(_("Are you sure you want to delete \"%s\""),
			purple_savedstatus_get_title(saved));

	purple_request_action(saved, _("Delete Status"), ask, NULL, 0, saved, 2,
			_("Delete"), really_delete_status, _("Cancel"), NULL);
	g_free(ask);
}

static void
use_savedstatus_cb(GntWidget *widget, gpointer null)
{
	g_return_if_fail(statuses.tree != NULL);

	purple_savedstatus_activate(gnt_tree_get_selection_data(GNT_TREE(statuses.tree)));
}

static void
edit_savedstatus_cb(GntWidget *widget, gpointer null)
{
	g_return_if_fail(statuses.tree != NULL);

	finch_savedstatus_edit(gnt_tree_get_selection_data(GNT_TREE(statuses.tree)));
}

void finch_savedstatus_show_all()
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
	g_signal_connect_swapped(G_OBJECT(button), "activate",
			G_CALLBACK(finch_savedstatus_edit), NULL);

	button = gnt_button_new(_("Edit"));
	gnt_box_add_widget(GNT_BOX(box), button);
	g_signal_connect(G_OBJECT(button), "activate",
			G_CALLBACK(edit_savedstatus_cb), NULL);

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

static void
destroy_substatus_win(PurpleAccount *account, EditSubStatus *sub, gpointer null)
{
	gnt_widget_destroy(sub->window);   /* the "destroy" callback will remove entry from the hashtable */
}

static void
free_key(gpointer key, gpointer n)
{
	RowInfo *row = key;
	g_free(row->message);
	g_free(key);
}


static void
update_edit_list(GntWidget *widget, EditStatus *edit)
{
	edits = g_list_remove(edits, edit);
	purple_notify_close_with_handle(edit);
	g_hash_table_foreach(edit->hash, (GHFunc)destroy_substatus_win, NULL);
	g_list_foreach((GList*)gnt_tree_get_rows(GNT_TREE(edit->tree)), free_key, NULL);
	g_free(edit);
}

static void
set_substatuses(EditStatus *edit)
{
	const GList *iter;
	for (iter = gnt_tree_get_rows(GNT_TREE(edit->tree)); iter; iter = iter->next) {
		RowInfo *key = iter->data;
		if (gnt_tree_get_choice(GNT_TREE(edit->tree), key)) {
			purple_savedstatus_set_substatus(edit->saved, key->account, key->type, key->message);
		}
	}
}


static void
use_trans_status_cb(GntWidget *button, EditStatus *edit)
{
	const char *message;
	PurpleStatusPrimitive prim;
	PurpleSavedStatus *saved;

	message = gnt_entry_get_text(GNT_ENTRY(edit->message));
	prim = GPOINTER_TO_INT(gnt_combo_box_get_selected_data(GNT_COMBO_BOX(edit->type)));

	saved = purple_savedstatus_find_transient_by_type_and_message(prim, message);
	if (saved == NULL) {
		saved = purple_savedstatus_new(NULL, prim);
		edit->saved = saved;
		set_substatuses(edit);
	}
	purple_savedstatus_set_message(saved, message);
	purple_savedstatus_activate(saved);
	gnt_widget_destroy(edit->window);
}

static void
save_savedstatus_cb(GntWidget *button, EditStatus *edit)
{
	const char *title, *message;
	PurpleStatusPrimitive prim;
	PurpleSavedStatus *find;

	title = gnt_entry_get_text(GNT_ENTRY(edit->title));
	message = gnt_entry_get_text(GNT_ENTRY(edit->message));
	if (!message || !*message)
		message = NULL;

	prim = GPOINTER_TO_INT(gnt_combo_box_get_selected_data(GNT_COMBO_BOX(edit->type)));

	if (!title || !*title)
	{
		purple_notify_error(edit, _("Error"), _("Invalid title"),
				_("Please enter a non-empty title for the status."));
		return;
	}

	find = purple_savedstatus_find(title);
	if (find && find != edit->saved)
	{
		purple_notify_error(edit, _("Error"), _("Duplicate title"),
				_("Please enter a different title for the status."));
		return;
	}
	
	if (edit->saved == NULL)
	{
		edit->saved = purple_savedstatus_new(title, prim);
		purple_savedstatus_set_message(edit->saved, message);
		set_substatuses(edit);
		if (statuses.tree)
			gnt_tree_add_row_last(GNT_TREE(statuses.tree), edit->saved,
					gnt_tree_create_row(GNT_TREE(statuses.tree), title,
						purple_primitive_get_name_from_type(prim), message), NULL);
	}
	else
	{
		purple_savedstatus_set_title(edit->saved, title);
		purple_savedstatus_set_type(edit->saved, prim);
		purple_savedstatus_set_message(edit->saved, message);
		if (statuses.tree)
		{
			gnt_tree_change_text(GNT_TREE(statuses.tree), edit->saved, 0, title);
			gnt_tree_change_text(GNT_TREE(statuses.tree), edit->saved, 1,
						purple_primitive_get_name_from_type(prim));
			gnt_tree_change_text(GNT_TREE(statuses.tree), edit->saved, 2, message);
		}
	}

	if (g_object_get_data(G_OBJECT(button), "use"))
		purple_savedstatus_activate(edit->saved);

	gnt_widget_destroy(edit->window);
}

static void
add_substatus(EditStatus *edit, PurpleAccount *account)
{
	char *name;
	const char *type = NULL, *message = NULL;
	PurpleSavedStatusSub *sub = NULL;
	RowInfo *key;

	if (!edit || !edit->tree)
		return;

	if (edit->saved)
		sub = purple_savedstatus_get_substatus(edit->saved, account);

	key = g_new0(RowInfo, 1);
	key->account = account;

	if (sub)
	{
		key->type = purple_savedstatus_substatus_get_type(sub);
		type = purple_status_type_get_name(key->type);
		message = purple_savedstatus_substatus_get_message(sub);
		key->message = g_strdup(message);
	}

	name = g_strdup_printf("%s (%s)", purple_account_get_username(account),
			purple_account_get_protocol_name(account));
	gnt_tree_add_choice(GNT_TREE(edit->tree), key,
			gnt_tree_create_row(GNT_TREE(edit->tree),
				name, type ? type : "", message ? message : ""), NULL, NULL);

	if (sub)
		gnt_tree_set_choice(GNT_TREE(edit->tree), key, TRUE);
	g_free(name);
}

static void
substatus_window_destroy_cb(GntWidget *window, EditSubStatus *sub)
{
	g_hash_table_remove(sub->parent->hash, sub->key->account);
	g_free(sub);
}

static void
save_substatus_cb(GntWidget *widget, EditSubStatus *sub)
{
	PurpleSavedStatus *saved = sub->parent->saved;
	RowInfo *row = sub->key;
	const char *message;
	PurpleStatusType *type;

	type = gnt_combo_box_get_selected_data(GNT_COMBO_BOX(sub->type));
	message = gnt_entry_get_text(GNT_ENTRY(sub->message));

	row->type = type;
	row->message = g_strdup(message);

	if (saved)    /* Save the substatus if the savedstatus actually exists. */
		purple_savedstatus_set_substatus(saved, row->account, type, message);

	gnt_tree_set_choice(GNT_TREE(sub->parent->tree), row, TRUE);
	gnt_tree_change_text(GNT_TREE(sub->parent->tree), row, 1,
			purple_status_type_get_name(type));
	gnt_tree_change_text(GNT_TREE(sub->parent->tree), row, 2, message);
	
	gnt_widget_destroy(sub->window);
}

static gboolean
popup_substatus(GntTree *tree, const char *key, EditStatus *edit)
{
	if (key[0] == ' ' && key[1] == 0)
	{
		EditSubStatus *sub;
		GntWidget *window, *combo, *entry, *box, *button, *l;
		PurpleSavedStatusSub *substatus = NULL;
		const GList *iter;
		char *name;
		RowInfo *selected = gnt_tree_get_selection_data(tree);
		PurpleAccount *account = selected->account;

		if (gnt_tree_get_choice(tree, selected))
		{
			/* There was a savedstatus for this account. Now remove it. */
			g_free(selected->message);
			selected->type = NULL;
			selected->message = NULL;
			/* XXX: should we really be saving it right now? */
			purple_savedstatus_unset_substatus(edit->saved, account);
			gnt_tree_change_text(tree, account, 1, NULL);
			gnt_tree_change_text(tree, account, 2, NULL);
			return FALSE;
		}

		if (g_hash_table_lookup(edit->hash, account))
			return TRUE;

		if (edit->saved)
			substatus = purple_savedstatus_get_substatus(edit->saved, account);

		sub = g_new0(EditSubStatus, 1);
		sub->parent = edit;
		sub->key = selected;

		sub->window = window = gnt_vbox_new(FALSE);
		gnt_box_set_toplevel(GNT_BOX(window), TRUE);
		gnt_box_set_title(GNT_BOX(window), _("Substatus"));  /* XXX: a better title */

		box = gnt_hbox_new(FALSE);
		gnt_box_add_widget(GNT_BOX(box), gnt_label_new(_("Account:")));
		name = g_strdup_printf("%s (%s)", purple_account_get_username(account),
				purple_account_get_protocol_name(account));
		gnt_box_add_widget(GNT_BOX(box), gnt_label_new(name));
		g_free(name);
		gnt_box_add_widget(GNT_BOX(window), box);

		box = gnt_hbox_new(FALSE);
		gnt_box_add_widget(GNT_BOX(box), (l = gnt_label_new(_("Status:"))));
		gnt_widget_set_size(l, 0, 1);   /* I don't like having to do this */
		sub->type = combo = gnt_combo_box_new();
		gnt_box_add_widget(GNT_BOX(box), combo);
		gnt_box_add_widget(GNT_BOX(window), box);

		for (iter = purple_account_get_status_types(account); iter; iter = iter->next)
		{
			PurpleStatusType *type = iter->data;
			if (!purple_status_type_is_user_settable(type))
				continue;
			gnt_combo_box_add_data(GNT_COMBO_BOX(combo), type, purple_status_type_get_name(type));
		}

		box = gnt_hbox_new(FALSE);
		gnt_box_add_widget(GNT_BOX(box), gnt_label_new(_("Message:")));
		sub->message = entry = gnt_entry_new(substatus ? purple_savedstatus_substatus_get_message(substatus) : NULL);
		gnt_box_add_widget(GNT_BOX(box), entry);
		gnt_box_add_widget(GNT_BOX(window), box);

		box  = gnt_hbox_new(FALSE);
		button = gnt_button_new(_("Cancel"));
		g_signal_connect_swapped(G_OBJECT(button), "activate", G_CALLBACK(gnt_widget_destroy), window);
		gnt_box_add_widget(GNT_BOX(box), button);
		button = gnt_button_new(_("Save"));
		g_signal_connect(G_OBJECT(button), "activate", G_CALLBACK(save_substatus_cb), sub);
		gnt_box_add_widget(GNT_BOX(box), button);
		gnt_box_add_widget(GNT_BOX(window), box);

		gnt_widget_show(window);

		g_hash_table_insert(edit->hash, account, sub);

		g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(substatus_window_destroy_cb), sub);

		return TRUE;
	}
	return FALSE;
}

void finch_savedstatus_edit(PurpleSavedStatus *saved)
{
	EditStatus *edit;
	GntWidget *window, *box, *button, *entry, *combo, *label, *tree;
	PurpleStatusPrimitive prims[] = {PURPLE_STATUS_AVAILABLE, PURPLE_STATUS_AWAY,
		PURPLE_STATUS_INVISIBLE, PURPLE_STATUS_OFFLINE, PURPLE_STATUS_UNSET}, current;
	GList *iter;
	int i;

	if (saved)
	{
		GList *iter;
		for (iter = edits; iter; iter = iter->next)
		{
			edit = iter->data;
			if (edit->saved == saved)
				return;
		}
	}

	edit = g_new0(EditStatus, 1);
	edit->saved = saved;
	edit->window = window = gnt_vbox_new(FALSE);
	gnt_box_set_toplevel(GNT_BOX(window), TRUE);
	gnt_box_set_title(GNT_BOX(window), _("Edit Status"));
	gnt_box_set_fill(GNT_BOX(window), TRUE);
	gnt_box_set_alignment(GNT_BOX(window), GNT_ALIGN_LEFT);
	gnt_box_set_pad(GNT_BOX(window), 0);

	edits = g_list_append(edits, edit);

	/* Title */
	box = gnt_hbox_new(FALSE);
	gnt_box_set_alignment(GNT_BOX(box), GNT_ALIGN_LEFT);
	gnt_box_add_widget(GNT_BOX(window), box);
	gnt_box_add_widget(GNT_BOX(box), gnt_label_new(_("Title")));

	edit->title = entry = gnt_entry_new(saved ? purple_savedstatus_get_title(saved) : NULL);
	gnt_box_add_widget(GNT_BOX(box), entry);

	/* Type */
	box = gnt_hbox_new(FALSE);
	gnt_box_add_widget(GNT_BOX(window), box);
	gnt_box_add_widget(GNT_BOX(box), label = gnt_label_new(_("Status")));
	gnt_widget_set_size(label, 0, 1);

	edit->type = combo = gnt_combo_box_new();
	gnt_box_add_widget(GNT_BOX(box), combo);
	current = saved ? purple_savedstatus_get_type(saved) : PURPLE_STATUS_UNSET;
	for (i = 0; prims[i] != PURPLE_STATUS_UNSET; i++)
	{
		gnt_combo_box_add_data(GNT_COMBO_BOX(combo), GINT_TO_POINTER(prims[i]),
				purple_primitive_get_name_from_type(prims[i]));
		if (prims[i] == current)
			gnt_combo_box_set_selected(GNT_COMBO_BOX(combo), GINT_TO_POINTER(current));
	}

	/* Message */
	box = gnt_hbox_new(FALSE);
	gnt_box_add_widget(GNT_BOX(window), box);
	gnt_box_add_widget(GNT_BOX(box), gnt_label_new(_("Message")));

	edit->message = entry = gnt_entry_new(saved ? purple_savedstatus_get_message(saved) : NULL);
	gnt_box_add_widget(GNT_BOX(window), entry);

	gnt_box_add_widget(GNT_BOX(window), gnt_hline_new());
	gnt_box_add_widget(GNT_BOX(window), gnt_label_new(_("Use different status for following accounts")));

	edit->hash = g_hash_table_new(g_direct_hash, g_direct_equal);
	edit->tree = tree = gnt_tree_new_with_columns(3);
	gnt_box_add_widget(GNT_BOX(window), tree);
	gnt_tree_set_show_title(GNT_TREE(tree), TRUE);
	gnt_tree_set_column_titles(GNT_TREE(tree), _("Account"), _("Status"), _("Message"));
	gnt_tree_set_col_width(GNT_TREE(tree), 0, 30);
	gnt_tree_set_col_width(GNT_TREE(tree), 1, 10);
	gnt_tree_set_col_width(GNT_TREE(tree), 2, 30);

	for (iter = purple_accounts_get_all(); iter; iter = iter->next)
	{
		add_substatus(edit, iter->data);
	}

	g_signal_connect(G_OBJECT(tree), "key_pressed", G_CALLBACK(popup_substatus), edit);

	/* The buttons */
	box = gnt_hbox_new(FALSE);
	gnt_box_add_widget(GNT_BOX(window), box);

	/* Use */
	button = gnt_button_new(_("Use"));
	gnt_box_add_widget(GNT_BOX(box), button);
	g_signal_connect(G_OBJECT(button), "activate", G_CALLBACK(use_trans_status_cb), edit);

	/* Save */
	button = gnt_button_new(_("Save"));
	gnt_box_add_widget(GNT_BOX(box), button);
	g_object_set_data(G_OBJECT(button), "use", NULL);
	g_signal_connect(G_OBJECT(button), "activate", G_CALLBACK(save_savedstatus_cb), edit);

	/* Save & Use */
	button = gnt_button_new(_("Save & Use"));
	gnt_box_add_widget(GNT_BOX(box), button);
	g_object_set_data(G_OBJECT(button), "use", GINT_TO_POINTER(TRUE));
	g_signal_connect(G_OBJECT(button), "activate", G_CALLBACK(save_savedstatus_cb), edit);

	/* Cancel */
	button = gnt_button_new(_("Cancel"));
	gnt_box_add_widget(GNT_BOX(box), button);
	g_signal_connect_swapped(G_OBJECT(button), "activate",
			G_CALLBACK(gnt_widget_destroy), window);

	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(update_edit_list), edit);

	gnt_widget_show(window);
}

