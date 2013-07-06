/*
 * Evolution integration plugin for Purple
 *
 * Copyright (C) 2003 Christian Hammond.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02111-1301, USA.
 */
#include "internal.h"
#include "gtkblist.h"
#include "pidgin.h"
#include "gtkutils.h"

#include "debug.h"

#include "gevolution.h"

#include <stdlib.h>

enum
{
	COLUMN_NAME,
	COLUMN_PRPL_ICON,
	COLUMN_USERNAME,
	COLUMN_DATA,
	NUM_COLUMNS
};

static gint
delete_win_cb(GtkWidget *w, GdkEvent *event, GevoAddBuddyDialog *dialog)
{
	gtk_widget_destroy(dialog->win);

	if (dialog->contacts != NULL)
	{
		g_list_foreach(dialog->contacts, (GFunc)g_object_unref, NULL);
		g_list_free(dialog->contacts);
	}

	if (dialog->book != NULL)
		g_object_unref(dialog->book);

	gevo_addrbooks_model_unref(dialog->addrbooks);

	g_free(dialog->username);

	g_free(dialog);

	return 0;
}

static void
new_person_cb(GtkWidget *w, GevoAddBuddyDialog *dialog)
{
	const char *group_name;

	group_name =
		pidgin_text_combo_box_entry_get_text(dialog->group_combo);

	gevo_new_person_dialog_show(dialog->book, NULL, dialog->account, dialog->username,
								(*group_name ? group_name : NULL),
								NULL, FALSE);

	delete_win_cb(NULL, NULL, dialog);
}

static void
cancel_cb(GtkWidget *w, GevoAddBuddyDialog *dialog)
{
	delete_win_cb(NULL, NULL, dialog);
}

static void
select_buddy_cb(GtkWidget *w, GevoAddBuddyDialog *dialog)
{
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	const char *group_name;
	const char *fullname;
	const char *username;
	EContact *contact;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->treeview));

	if (!gtk_tree_selection_get_selected(selection, NULL, &iter))
		return;

	gtk_tree_model_get(GTK_TREE_MODEL(dialog->model), &iter,
					   COLUMN_NAME, &fullname,
					   COLUMN_USERNAME, &username,
					   COLUMN_DATA, &contact,
					   -1);

	group_name =
		pidgin_text_combo_box_entry_get_text(dialog->group_combo);

	if (username == NULL || *username == '\0')
	{
		gevo_new_person_dialog_show(dialog->book, NULL, dialog->account, dialog->username,
									(*group_name ? group_name : NULL),
									NULL, FALSE);
	}
	else
	{
		gevo_add_buddy(dialog->account, group_name, username, fullname);
	}

	delete_win_cb(NULL, NULL, dialog);
}

static void
add_columns(GevoAddBuddyDialog *dialog)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* Name column */
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Name"));
	gtk_tree_view_insert_column(GTK_TREE_VIEW(dialog->treeview), column, -1);
	gtk_tree_view_column_set_sort_column_id(column, COLUMN_NAME);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer,
									   "text", COLUMN_NAME);

	/* Account column */
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Instant Messaging"));
	gtk_tree_view_insert_column(GTK_TREE_VIEW(dialog->treeview), column, -1);
	gtk_tree_view_column_set_sort_column_id(column, COLUMN_USERNAME);

	/* Protocol icon */
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_add_attribute(column, renderer,
									   "pixbuf", COLUMN_PRPL_ICON);

	/* Account name */
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer,
									   "text", COLUMN_USERNAME);
}

static void
add_ims(GevoAddBuddyDialog *dialog, EContact *contact, const char *name,
		GList *list, const char *id)
{
	PurpleAccount *account = NULL;
	GList *l;
	GtkTreeIter iter;
	GdkPixbuf *pixbuf;

	if (list == NULL)
		return;

	for (l = purple_connections_get_all(); l != NULL; l = l->next)
	{
		PurpleConnection *gc = (PurpleConnection *)l->data;

		account = purple_connection_get_account(gc);

		if (!strcmp(purple_account_get_protocol_id(account), id))
			break;

		account = NULL;
	}

	if (account == NULL)
		return;

	pixbuf = pidgin_create_prpl_icon(account, PIDGIN_PRPL_ICON_SMALL);

	for (l = list; l != NULL; l = l->next)
	{
		char *account_name = (char *)l->data;

		if (account_name == NULL)
			continue;

		if (purple_find_buddy(dialog->account, account_name) != NULL)
			continue;

		gtk_list_store_append(dialog->model, &iter);

		gtk_list_store_set(dialog->model, &iter,
						   COLUMN_NAME, name,
						   COLUMN_PRPL_ICON, pixbuf,
						   COLUMN_USERNAME, account_name,
						   COLUMN_DATA, contact,
						   -1);

		if (!strcmp(purple_account_get_protocol_id(account),
					purple_account_get_protocol_id(dialog->account)) &&
			dialog->username != NULL &&
			!strcmp(account_name, dialog->username))
		{
			GtkTreeSelection *selection;

			/* This is it. Select it. */
			selection = gtk_tree_view_get_selection(
				GTK_TREE_VIEW(dialog->treeview));

			gtk_tree_selection_select_iter(selection, &iter);
		}
	}

	if (pixbuf != NULL)
		g_object_unref(G_OBJECT(pixbuf));

	g_list_foreach(list, (GFunc)g_free, NULL);
	g_list_free(list);
}

static void
populate_treeview(GevoAddBuddyDialog *dialog, const gchar *uri)
{
	EBookQuery *query;
	EBook *book;
	gboolean status;
	GList *cards, *c;
	GError *err = NULL;

	if (dialog->book != NULL)
	{
		g_object_unref(dialog->book);
		dialog->book = NULL;
	}

	if (dialog->contacts != NULL)
	{
		g_list_foreach(dialog->contacts, (GFunc)g_object_unref, NULL);
		g_list_free(dialog->contacts);
		dialog->contacts = NULL;
	}

	gtk_list_store_clear(dialog->model);

	if (!gevo_load_addressbook(uri, &book, &err))
	{
		purple_debug_error("evolution",
						 "Error retrieving default addressbook: %s\n", err->message);
		g_error_free(err);

		return;
	}

	query = e_book_query_field_exists(E_CONTACT_FULL_NAME);

	if (query == NULL)
	{
		purple_debug_error("evolution", "Error in creating query\n");

		g_object_unref(book);

		return;
	}

	status = e_book_get_contacts(book, query, &cards, NULL);

	e_book_query_unref(query);

	if (!status)
	{
		purple_debug_error("evolution", "Error %d in getting card list\n",
						 status);

		g_object_unref(book);

		return;
	}

	for (c = cards; c != NULL; c = c->next)
	{
		EContact *contact = E_CONTACT(c->data);
		const char *name;
		GList *aims, *jabbers, *yahoos, *msns, *icqs, *novells, *ggs;

		name = e_contact_get_const(contact, E_CONTACT_FULL_NAME);

		aims    = e_contact_get(contact, E_CONTACT_IM_AIM);
		jabbers = e_contact_get(contact, E_CONTACT_IM_JABBER);
		yahoos  = e_contact_get(contact, E_CONTACT_IM_YAHOO);
		msns    = e_contact_get(contact, E_CONTACT_IM_MSN);
		icqs    = e_contact_get(contact, E_CONTACT_IM_ICQ);
		novells = e_contact_get(contact, E_CONTACT_IM_GROUPWISE);
		ggs     = e_contact_get(contact, E_CONTACT_IM_GADUGADU);

		if (aims == NULL && jabbers == NULL && yahoos == NULL &&
			msns == NULL && icqs == NULL && novells == NULL &&
			ggs == NULL)
		{
			GtkTreeIter iter;

			gtk_list_store_append(dialog->model, &iter);

			gtk_list_store_set(dialog->model, &iter,
							   COLUMN_NAME, name,
							   COLUMN_DATA, contact,
							   -1);
		}
		else
		{
			add_ims(dialog, contact, name, aims,    "prpl-aim");
			add_ims(dialog, contact, name, jabbers, "prpl-jabber");
			add_ims(dialog, contact, name, yahoos,  "prpl-yahoo");
			add_ims(dialog, contact, name, msns,    "prpl-msn");
			add_ims(dialog, contact, name, icqs,    "prpl-icq");
			add_ims(dialog, contact, name, novells, "prpl-novell");
			add_ims(dialog, contact, name, ggs,     "prpl-gg");
		}
	}

	dialog->contacts = cards;
	dialog->book = book;
}

static void
addrbook_change_cb(GtkComboBox *combo, GevoAddBuddyDialog *dialog)
{
	GtkTreeIter iter;
	const char *esource_uri;

	if (!gtk_combo_box_get_active_iter(combo, &iter))
		return;

	gtk_tree_model_get(GTK_TREE_MODEL(dialog->addrbooks), &iter,
					   ADDRBOOK_COLUMN_URI, &esource_uri,
					   -1);

	populate_treeview(dialog, esource_uri);
}

static void
selected_cb(GtkTreeSelection *sel, GevoAddBuddyDialog *dialog)
{
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->treeview));
	gtk_widget_set_sensitive(dialog->select_button,
							 gtk_tree_selection_get_selected(selection, NULL, NULL));
}

static void
search_changed_cb(GtkEntry *entry, GevoAddBuddyDialog *dialog)
{
	const char *text = gtk_entry_get_text(entry);
	GList *l;

	gtk_list_store_clear(dialog->model);

	for (l = dialog->contacts; l != NULL; l = l->next)
	{
		EContact *contact = E_CONTACT(l->data);
		const char *name;
		GList *aims, *jabbers, *yahoos, *msns, *icqs, *novells, *ggs;

		name = e_contact_get_const(contact, E_CONTACT_FULL_NAME);

		if (text != NULL && *text != '\0' && name != NULL &&
			g_ascii_strncasecmp(name, text, strlen(text)))
		{
			continue;
		}

		aims    = e_contact_get(contact, E_CONTACT_IM_AIM);
		jabbers = e_contact_get(contact, E_CONTACT_IM_JABBER);
		yahoos  = e_contact_get(contact, E_CONTACT_IM_YAHOO);
		msns    = e_contact_get(contact, E_CONTACT_IM_MSN);
		icqs    = e_contact_get(contact, E_CONTACT_IM_ICQ);
		novells = e_contact_get(contact, E_CONTACT_IM_GROUPWISE);
		ggs     = e_contact_get(contact, E_CONTACT_IM_GADUGADU);

		if (aims == NULL && jabbers == NULL && yahoos == NULL &&
			msns == NULL && icqs == NULL && novells == NULL &&
			ggs == NULL)
		{
			GtkTreeIter iter;

			gtk_list_store_append(dialog->model, &iter);

			gtk_list_store_set(dialog->model, &iter,
							   COLUMN_NAME, name,
							   COLUMN_DATA, contact,
							   -1);
		}
		else
		{
			add_ims(dialog, contact, name, aims,    "prpl-aim");
			add_ims(dialog, contact, name, jabbers, "prpl-jabber");
			add_ims(dialog, contact, name, yahoos,  "prpl-yahoo");
			add_ims(dialog, contact, name, msns,    "prpl-msn");
			add_ims(dialog, contact, name, icqs,    "prpl-icq");
			add_ims(dialog, contact, name, novells, "prpl-novell");
			add_ims(dialog, contact, name, ggs,     "prpl-gg");
		}
	}
}

static void
clear_cb(GtkWidget *w, GevoAddBuddyDialog *dialog)
{
	static gboolean lock = FALSE;

	if (lock)
		return;

	lock = TRUE;
	gtk_entry_set_text(GTK_ENTRY(dialog->search_field), "");
	lock = FALSE;
}

void
gevo_add_buddy_dialog_show(PurpleAccount *account, const char *username,
						   const char *group, const char *alias)
{
	GevoAddBuddyDialog *dialog;
	GtkWidget *button;
	GtkWidget *label;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *bbox;
	GtkWidget *sep;
	GtkTreeSelection *selection;
	GtkCellRenderer *cell;

	dialog = g_new0(GevoAddBuddyDialog, 1);

	dialog->account =
		(account != NULL
		 ? account
		 : purple_connection_get_account(purple_connections_get_all()->data));

	if (username != NULL)
		dialog->username = g_strdup(username);

	dialog->win = pidgin_create_window(_("Add Buddy"), PIDGIN_HIG_BORDER, "add_buddy", TRUE);
	gtk_widget_set_size_request(dialog->win, -1, 400);

	g_signal_connect(G_OBJECT(dialog->win), "delete_event",
					 G_CALLBACK(delete_win_cb), dialog);

	/* Setup the vbox */
	vbox = gtk_vbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(dialog->win), vbox);
	gtk_widget_show(vbox);

	/* Add the label. */
	label = gtk_label_new(_("Select a person from your address book below, "
							"or add a new person."));
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 0);
	gtk_widget_show(label);

	/* Add the search hbox */
	hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
	gtk_widget_show(hbox);

	/* "Search" */
	label = gtk_label_new(_("Search"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	/* Addressbooks */
	dialog->addrbooks = gevo_addrbooks_model_new();

	dialog->addrbooks_combo = gtk_combo_box_new_with_model(
			dialog->addrbooks);
	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(dialog->addrbooks_combo),
							   cell, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(dialog->addrbooks_combo),
								   cell,
								   "text", ADDRBOOK_COLUMN_NAME,
								   NULL);
	gtk_box_pack_start(GTK_BOX(hbox), dialog->addrbooks_combo, FALSE,
					   FALSE, 0);
	gtk_widget_show(dialog->addrbooks_combo);

	/* Search field */
	dialog->search_field = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), dialog->search_field, TRUE, TRUE, 0);
	gtk_widget_show(dialog->search_field);

	g_signal_connect(G_OBJECT(dialog->search_field), "changed",
					 G_CALLBACK(search_changed_cb), dialog);

	/* Clear button */
	button = gtk_button_new_from_stock(GTK_STOCK_CLEAR);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(clear_cb), dialog);

	/* Create the list model for the treeview. */
	dialog->model = gtk_list_store_new(NUM_COLUMNS,
									   G_TYPE_STRING, GDK_TYPE_PIXBUF,
									   G_TYPE_STRING, G_TYPE_POINTER);

	/* Now for the treeview */
	dialog->treeview =
		gtk_tree_view_new_with_model(GTK_TREE_MODEL(dialog->model));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(dialog->treeview), TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), 
		pidgin_make_scrollable(dialog->treeview, GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS, GTK_SHADOW_IN, -1, -1), 
		TRUE, TRUE, 0);
	gtk_widget_show(dialog->treeview);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->treeview));

	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);

	g_signal_connect(G_OBJECT(selection), "changed",
					 G_CALLBACK(selected_cb), dialog);

	add_columns(dialog);

	/*
	 * Catch addressbook selection and populate treeview with the first
	 * addressbook
	 */
	gevo_addrbooks_model_populate(dialog->addrbooks);
	g_signal_connect(G_OBJECT(dialog->addrbooks_combo), "changed",
							  G_CALLBACK(addrbook_change_cb), dialog);
	gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->addrbooks_combo), 0);

	/* Group box */
	dialog->group_combo =
		pidgin_text_combo_box_entry_new(group, gevo_get_groups());
	pidgin_add_widget_to_vbox(GTK_BOX(vbox), _("Group:"), NULL,
							  dialog->group_combo, TRUE, NULL);
	gtk_widget_show_all(dialog->group_combo);

	/* Cool. Now we only have a little left... */

	/* Separator. */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);
	gtk_widget_show(sep);

	/* Button box */
	bbox = gtk_hbutton_box_new();
	gtk_box_set_spacing(GTK_BOX(bbox), 6);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_box_pack_end(GTK_BOX(vbox), bbox, FALSE, TRUE, 0);
	gtk_widget_show(bbox);

	/* "New Person" button */
	button = pidgin_pixbuf_button_from_stock(_("New Person"), GTK_STOCK_NEW,
										   PIDGIN_BUTTON_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(new_person_cb), dialog);

	/* "Cancel" button */
	button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(cancel_cb), dialog);

	/* "Select Buddy" button */
	button = pidgin_pixbuf_button_from_stock(_("Select Buddy"), GTK_STOCK_APPLY,
										   PIDGIN_BUTTON_HORIZONTAL);
	dialog->select_button = button;
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_set_sensitive(button, FALSE);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(select_buddy_cb), dialog);

	/* Show it. */
	gtk_widget_show(dialog->win);
}

void
gevo_add_buddy_dialog_add_person(GevoAddBuddyDialog *dialog,
								 EContact *contact, const char *name,
								 PurpleAccount *account, const char *screenname)
{
	GdkPixbuf *pixbuf;
	GtkTreeIter iter;

	pixbuf = pidgin_create_prpl_icon(account, PIDGIN_PRPL_ICON_SMALL);

	gtk_list_store_append(dialog->model, &iter);

	gtk_list_store_set(dialog->model, &iter,
					   COLUMN_NAME, name,
					   COLUMN_PRPL_ICON, pixbuf,
					   COLUMN_DATA, contact,
					   COLUMN_USERNAME, screenname,
					   -1);

	if (contact != NULL)
		dialog->contacts = g_list_append(dialog->contacts, contact);

	if (pixbuf != NULL)
		g_object_unref(G_OBJECT(pixbuf));
}
