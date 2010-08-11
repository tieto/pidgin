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
#include "pidgin.h"
#include "gtkutils.h"

#include "debug.h"

#include "gevolution.h"

static GtkWidget *
add_pref_box(GtkSizeGroup *sg, GtkWidget *parent, const char *text,
			 GtkWidget *widget)
{
	GtkWidget *hbox;
	GtkWidget *label;

	hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(parent), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new_with_mnemonic(text);
	gtk_size_group_add_widget(sg, label);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0);
	gtk_widget_show(widget);

	return hbox;
}

static gint
delete_win_cb(GtkWidget *w, GdkEvent *event, GevoNewPersonDialog *dialog)
{
	gtk_widget_destroy(dialog->win);

	g_object_unref(dialog->book);
	g_free(dialog);

	return 0;
}

static void
cancel_cb(GtkWidget *w, GevoNewPersonDialog *dialog)
{
	delete_win_cb(NULL, NULL, dialog);
}

static void
username_changed_cb(GtkEntry *entry, GevoNewPersonDialog *dialog)
{
	gtk_widget_set_sensitive(dialog->add_button,
							 *gtk_entry_get_text(entry) != '\0');
}

static void
person_info_changed_cb(GtkEntry *entry, GevoNewPersonDialog *dialog)
{
	gtk_widget_set_sensitive(dialog->add_button,
		(*gtk_entry_get_text(GTK_ENTRY(dialog->firstname)) != '\0' ||
		 *gtk_entry_get_text(GTK_ENTRY(dialog->lastname))  != '\0'));
}

static void
add_cb(GtkWidget *w, GevoNewPersonDialog *dialog)
{
	EContact *contact = NULL;
	const char *username;
	const char *firstname;
	const char *lastname;
	const char *email;
	const char *im_service;
	gboolean new_contact = FALSE;
	EContactField field = 0;
	EContactName *name = NULL;
	char *full_name = NULL;

	if (dialog->person_only)
		username = dialog->buddy->name;
	else
		username = gtk_entry_get_text(GTK_ENTRY(dialog->username));

	firstname  = gtk_entry_get_text(GTK_ENTRY(dialog->firstname));
	lastname   = gtk_entry_get_text(GTK_ENTRY(dialog->lastname));
	email      = gtk_entry_get_text(GTK_ENTRY(dialog->email));

	if (*firstname || *lastname)
	{
		if (dialog->contact == NULL)
		{
			char *file_as;

			dialog->contact = e_contact_new();

			if (*lastname && *firstname)
				file_as = g_strdup_printf("%s, %s", lastname, firstname);
			else if (*lastname)
				file_as = g_strdup(lastname);
			else
				file_as = g_strdup(firstname);

			e_contact_set(dialog->contact, E_CONTACT_FILE_AS, file_as);

			g_free(file_as);

			new_contact = TRUE;
		}

		contact = dialog->contact;

		name = e_contact_name_new();

		name->given  = g_strdup(firstname);
		name->family = g_strdup(lastname);

		full_name = e_contact_name_to_string(name);
		e_contact_set(contact, E_CONTACT_FULL_NAME, full_name);

		im_service = purple_account_get_protocol_id(dialog->account);

		if (*email)
			e_contact_set(contact, E_CONTACT_EMAIL_1, (gpointer)email);

		if (!strcmp(im_service, "prpl-oscar"))
		{
			if (isdigit(*username))
				field = E_CONTACT_IM_ICQ;
			else
				field = E_CONTACT_IM_AIM;
		}
		else if (!strcmp(im_service, "prpl-aim"))
			field = E_CONTACT_IM_AIM;
		else if (!strcmp(im_service, "prpl-icq"))
			field = E_CONTACT_IM_ICQ;
		else if (!strcmp(im_service, "prpl-yahoo"))
			field = E_CONTACT_IM_YAHOO;
		else if (!strcmp(im_service, "prpl-jabber"))
			field = E_CONTACT_IM_JABBER;
		else if (!strcmp(im_service, "prpl-msn"))
			field = E_CONTACT_IM_MSN;
		else if (!strcmp(im_service, "prpl-novell"))
			field = E_CONTACT_IM_GROUPWISE;

		if (field > 0)
		{
			GList *list = g_list_append(NULL, g_strdup(username));

			e_contact_set(contact, field, list);

			g_free(list->data);
			g_list_free(list);
		}

		if (new_contact)
		{
			if (!e_book_add_contact(dialog->book, contact, NULL))
			{
				purple_debug_error("evolution", "Error adding contact to book\n");

				g_object_unref(contact);
				delete_win_cb(NULL, NULL, dialog);
				return;
			}
		}
		else
		{
			if (!e_book_commit_contact(dialog->book, contact, NULL))
			{
				purple_debug_error("evolution", "Error adding contact to book\n");

				g_object_unref(contact);
				delete_win_cb(NULL, NULL, dialog);
				return;
			}
		}

		g_object_unref(contact);
	}

	if (!dialog->person_only)
	{
		const char *group_name;

		group_name = pidgin_text_combo_box_entry_get_text(dialog->group_combo);

		gevo_add_buddy(dialog->account, group_name, username, full_name);
	}

	if (name != NULL)
		e_contact_name_free(name);

	if (full_name != NULL)
		g_free(full_name);

	delete_win_cb(NULL, NULL, dialog);
}

static void
select_account_cb(GObject *w, PurpleAccount *account,
				  GevoNewPersonDialog *dialog)
{
	dialog->account = account;
}

void
gevo_new_person_dialog_show(EBook *book, EContact *contact,
							PurpleAccount *account, const char *username,
							const char *group, PurpleBuddy *buddy,
							gboolean person_only)
{
	GevoNewPersonDialog *dialog;
	GtkWidget *vbox, *vbox2;
	GtkWidget *hbox;
	GtkWidget *bbox;
	GtkWidget *label;
	GtkWidget *button;
	GtkWidget *sep;
	GtkSizeGroup *sg, *sg2;
	const char *str;

	g_return_if_fail(book);
	g_return_if_fail(!person_only || (person_only && buddy));

	dialog = g_new0(GevoNewPersonDialog, 1);

	dialog->account = account;
	dialog->person_only = person_only;
	dialog->buddy = buddy;
	dialog->book = book;
	g_object_ref(book);

	dialog->win = pidgin_create_window(_("New Person"), PIDGIN_HIG_BORDER, "new_person", FALSE);

	g_signal_connect(G_OBJECT(dialog->win), "delete_event",
					 G_CALLBACK(delete_win_cb), dialog);

	/* Setup the vbox */
	vbox = gtk_vbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(dialog->win), vbox);
	gtk_widget_show(vbox);

	/* Label */
	if (person_only)
	{
		label = gtk_label_new(
			_("Please enter the person's information below."));
	}
	else
	{
		label = gtk_label_new(_("Please enter the buddy's username and "
								"account type below."));
	}

	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 0);
	gtk_widget_show(label);

	/* Setup the size groups */
	sg  = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	sg2 = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	if (!person_only)
	{
		/* Add the account type stuff. */
		dialog->accounts_menu =
			pidgin_account_option_menu_new(account, FALSE,
											 G_CALLBACK(select_account_cb),
											 NULL, dialog);
		add_pref_box(sg, vbox, _("Account type:"), dialog->accounts_menu);

		/* Username */
		dialog->username = gtk_entry_new();
		add_pref_box(sg, vbox, _("Username:"), dialog->username);

		if (username != NULL)
			gtk_entry_set_text(GTK_ENTRY(dialog->username), username);

		g_signal_connect(G_OBJECT(dialog->username), "changed",
						 G_CALLBACK(username_changed_cb), dialog);

		/* Group */
		dialog->group_combo = pidgin_text_combo_box_entry_new(NULL,
			gevo_get_groups());
		add_pref_box(sg, vbox, _("Group:"), dialog->group_combo);

		/* Separator */
		sep = gtk_hseparator_new();
		gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);
		gtk_widget_show(sep);

		/* Optional Information section */
		label = gtk_label_new(_("Optional information:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
		gtk_widget_show(label);
	}

	/* Create the parent hbox for this whole thing. */
	hbox = gtk_hbox_new(FALSE, 12);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
	gtk_widget_show(hbox);

#if 0
	/* Now the left side of the hbox */
	vbox2 = gtk_vbox_new(FALSE, 12);
	gtk_box_pack_start(GTK_BOX(hbox), vbox2, FALSE, FALSE, 0);
	gtk_widget_show(vbox2);

	/* Buddy icon button */
	button = gtk_button_new_from_stock(GTK_STOCK_OPEN);
	gtk_box_pack_start(GTK_BOX(vbox2), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	/* Label */
	label = gtk_label_new(_("Buddy Icon"));
	gtk_box_pack_start(GTK_BOX(vbox2), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
#endif

	/* Now the right side. */
	vbox2 = gtk_vbox_new(FALSE, 12);
	gtk_box_pack_start(GTK_BOX(hbox), vbox2, TRUE, TRUE, 0);
	gtk_widget_show(vbox2);

	/* First Name field */
	dialog->firstname = gtk_entry_new();
	add_pref_box(sg2, vbox2, _("First name:"), dialog->firstname);

	if (contact != NULL)
	{
		str = e_contact_get_const(contact, E_CONTACT_GIVEN_NAME);

		if (str != NULL)
			gtk_entry_set_text(GTK_ENTRY(dialog->firstname), str);
	}

	/* Last Name field */
	dialog->lastname = gtk_entry_new();
	add_pref_box(sg2, vbox2, _("Last name:"), dialog->lastname);

	if (contact != NULL)
	{
		str = e_contact_get_const(contact, E_CONTACT_FAMILY_NAME);

		if (str != NULL)
			gtk_entry_set_text(GTK_ENTRY(dialog->lastname), str);
	}

	if (person_only)
	{
		g_signal_connect(G_OBJECT(dialog->firstname), "changed",
						 G_CALLBACK(person_info_changed_cb), dialog);
		g_signal_connect(G_OBJECT(dialog->lastname), "changed",
						 G_CALLBACK(person_info_changed_cb), dialog);
	}

	/* Email address field */
	dialog->email = gtk_entry_new();
	add_pref_box(sg2, vbox2, _("Email:"), dialog->email);

	if (contact != NULL)
	{
		str = e_contact_get_const(contact, E_CONTACT_EMAIL_1);

		if (str != NULL)
			gtk_entry_set_text(GTK_ENTRY(dialog->email), str);
	}

	/* Separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);
	gtk_widget_show(sep);

	/* Button box */
	bbox = gtk_hbutton_box_new();
	gtk_box_set_spacing(GTK_BOX(bbox), 6);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_box_pack_end(GTK_BOX(vbox), bbox, FALSE, TRUE, 0);
	gtk_widget_show(bbox);

	/* Cancel button */
	button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(cancel_cb), dialog);

	/* Add button */
	button = gtk_button_new_from_stock(GTK_STOCK_ADD);
	dialog->add_button = button;
	if (username == NULL || *username == '\0')
		gtk_widget_set_sensitive(button, FALSE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(add_cb), dialog);

	/* Show it. */
	gtk_widget_show(dialog->win);
	g_object_unref(sg);
	g_object_unref(sg2);
}
