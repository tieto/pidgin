/*
 * gaim
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
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
 *
 */
#include "gtkinternal.h"

#include "debug.h"
#include "multi.h"
#include "notify.h"
#include "prpl.h"
#include "server.h"

#include "gtkblist.h"
#include "gtkimhtml.h"
#include "gtkutils.h"
#include "stock.h"
#include "ui.h"

typedef struct
{
	GaimAccount *account;

	GtkWidget *window;
	GtkWidget *account_menu;
	GtkWidget *entries_box;
	GtkSizeGroup *sg;

	GList *entries;
} GaimGtkJoinChatData;

static void
do_join_chat(GaimGtkJoinChatData *data)
{
	if (data) {
		GHashTable *components = g_hash_table_new_full(g_str_hash, g_str_equal,
				g_free, g_free);
		GList *tmp;

		for (tmp = data->entries; tmp != NULL; tmp = tmp->next) {
			if (g_object_get_data(tmp->data, "is_spin")) {
				g_hash_table_replace(components,
						g_strdup(g_object_get_data(tmp->data, "identifier")),
						g_strdup_printf("%d",
							gtk_spin_button_get_value_as_int(tmp->data)));
			}
			else {
				g_hash_table_replace(components,
						g_strdup(g_object_get_data(tmp->data, "identifier")),
						g_strdup(gtk_entry_get_text(tmp->data)));
			}
		}

		serv_join_chat(gaim_account_get_connection(data->account), components);

		g_hash_table_destroy(components);
	}
}

static void
rebuild_joinchat_entries(GaimGtkJoinChatData *data)
{
	GaimConnection *gc;
	GList *list, *tmp;
	struct proto_chat_entry *pce;
	gboolean focus = TRUE;

	gc = gaim_account_get_connection(data->account);

	while (GTK_BOX(data->entries_box)->children) {
		gtk_container_remove(GTK_CONTAINER(data->entries_box),
				((GtkBoxChild *)GTK_BOX(data->entries_box)->children->data)->widget);
	}

	if (data->entries != NULL)
		g_list_free(data->entries);

	data->entries = NULL;

	list = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl)->chat_info(gc);

	for (tmp = list; tmp; tmp = tmp->next)
	{
		GtkWidget *label;
		GtkWidget *rowbox;

		pce = tmp->data;

		rowbox = gtk_hbox_new(FALSE, 5);
		gtk_box_pack_start(GTK_BOX(data->entries_box), rowbox, FALSE, FALSE, 0);

		label = gtk_label_new(pce->label);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_size_group_add_widget(data->sg, label);
		gtk_box_pack_start(GTK_BOX(rowbox), label, FALSE, FALSE, 0);

		if (pce->is_int) {
			GtkObject *adjust;
			GtkWidget *spin;
			adjust = gtk_adjustment_new(pce->min, pce->min, pce->max,
										1, 10, 10);
			spin = gtk_spin_button_new(GTK_ADJUSTMENT(adjust), 1, 0);
			g_object_set_data(G_OBJECT(spin), "is_spin", GINT_TO_POINTER(TRUE));
			g_object_set_data(G_OBJECT(spin), "identifier", pce->identifier);
			data->entries = g_list_append(data->entries, spin);
			gtk_widget_set_size_request(spin, 50, -1);
			gtk_box_pack_end(GTK_BOX(rowbox), spin, FALSE, FALSE, 0);
		} else {
			GtkWidget *entry = gtk_entry_new();

			gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
			g_object_set_data(G_OBJECT(entry), "identifier", pce->identifier);
			data->entries = g_list_append(data->entries, entry);

			if (pce->def)
				gtk_entry_set_text(GTK_ENTRY(entry), pce->def);

			if (focus) {
				gtk_widget_grab_focus(entry);
				focus = FALSE;
			}

			if (pce->secret)
				gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);

			gtk_box_pack_end(GTK_BOX(rowbox), entry, TRUE, TRUE, 0);
		}

		g_free(pce);
	}

	g_list_free(list);

	gtk_widget_show_all(data->entries_box);
}

static void
join_chat_select_account_cb(GObject *w, GaimAccount *account, GaimGtkJoinChatData *data)
{
	if (gaim_account_get_protocol(data->account) ==
			gaim_account_get_protocol(account)) {
		data->account = account;
	} else {
		data->account = account;
		rebuild_joinchat_entries(data);
	}
}

static gboolean
join_chat_check_account_func(GaimAccount *account)
{
	GaimConnection *gc = gaim_account_get_connection(account);

	return (GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl)->chat_info != NULL);
}

static void do_joinchat(GtkWidget *dialog, int id, GaimGtkJoinChatData *info)
{
	switch(id) {
	case GTK_RESPONSE_OK:
			do_join_chat(info);

		break;
	}

	gtk_widget_destroy(GTK_WIDGET(dialog));
	g_list_free(info->entries);
	g_free(info);
}


void
join_chat()
{
	GtkWidget *hbox, *mainbox;
	GtkWidget *rowbox;
	GtkWidget *bbox;
	GtkWidget *label;
	GList *c;
	GaimGtkBuddyList *gtkblist;
	GaimConnection *gc = NULL;
	GaimGtkJoinChatData *data = NULL;
	GtkWidget *img = NULL;

	for (c = gaim_connections_get_all(); c != NULL; c = c->next) {
		gc = c->data;

		if (GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl)->join_chat)
			break;

		gc = NULL;
	}

	if (gc == NULL) {
		gaim_notify_error(NULL, NULL,
						  _("You are not currently signed on with any "
							"protocols that have the ability to chat."),
						  NULL);

		return;
	}

	gtkblist = GAIM_GTK_BLIST(gaim_get_blist());
	img = gtk_image_new_from_stock(GAIM_STOCK_DIALOG_QUESTION, GTK_ICON_SIZE_DIALOG);
	data = g_new0(GaimGtkJoinChatData, 1);
	data->account = gaim_connection_get_account(gc);

	data->window = gtk_dialog_new_with_buttons(_("Get User Info"), gtkblist->window ? GTK_WINDOW(gtkblist->window) : NULL, 0,
											   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
											   GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(data->window), GTK_RESPONSE_OK);
	gtk_container_set_border_width(GTK_CONTAINER(data->window), 6);
	gtk_window_set_resizable(GTK_WINDOW(data->window), FALSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(data->window), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(data->window)->vbox), 12);
	gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(data->window)->vbox), 6);

	data->sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	hbox = gtk_hbox_new(FALSE, 12);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(data->window)->vbox), hbox);
    gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
    gtk_misc_set_alignment(GTK_MISC(img), 0, 0);

	mainbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(mainbox), 5);
	gtk_container_add(GTK_CONTAINER(hbox), mainbox);

	rowbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(mainbox), rowbox, TRUE, TRUE, 0);

	label = gtk_label_new(_("Join Chat As:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(rowbox), label, FALSE, FALSE, 0);
	gtk_size_group_add_widget(data->sg, label);

	data->account_menu = gaim_gtk_account_option_menu_new(NULL, FALSE,
			G_CALLBACK(join_chat_select_account_cb),
			join_chat_check_account_func, data);
	gtk_box_pack_start(GTK_BOX(rowbox), data->account_menu, TRUE, TRUE, 0);

	data->entries_box = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(mainbox), data->entries_box);
	gtk_container_set_border_width(GTK_CONTAINER(data->entries_box), 0);


	rebuild_joinchat_entries(data);


	bbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(bbox), 12);
	gtk_box_pack_start(GTK_BOX(mainbox), bbox, FALSE, FALSE, 6);

	g_signal_connect(G_OBJECT(data->window), "response", G_CALLBACK(do_joinchat), data);

	g_object_unref(data->sg);

	gtk_widget_show_all(data->window);
}
