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
 */
#include "gtkinternal.h"

#include "account.h"
#include "debug.h"
#include "notify.h"
#include "prefs.h"
#include "stock.h"
#include "util.h"

#include "gtkblist.h"
#include "gtkutils.h"

#include "ui.h"

struct signon_meter {
	GaimAccount *account;
	GtkWidget *button;
	GtkWidget *progress;
	GtkWidget *status;
};

struct meter_window {
		GtkWidget *window;
		GtkWidget *table;
		gint rows;
		gint active_count;
		GSList *meters;
} *meter_win = NULL;

static void kill_meter(struct signon_meter *meter, const char *text);

static void cancel_signon(GtkWidget *button, struct signon_meter *meter)
{
	if (meter->account->gc != NULL) {
		meter->account->gc->wants_to_die = TRUE;
		gaim_connection_destroy(meter->account->gc);
	}
	else {
		kill_meter(meter, _("Done."));

		if (gaim_connections_get_all() == NULL) {
			destroy_all_dialogs();

			gaim_blist_destroy();

			show_login();
		}
	}
}

static void cancel_all () {
	GSList *m = meter_win ? meter_win->meters : NULL;
	struct signon_meter *meter;

	while (m) {
		meter = m->data;
		cancel_signon(NULL, meter);
		m = m->next;
	}
}

static gint meter_destroy(GtkWidget *window, GdkEvent *evt, struct signon_meter *meter)
{
	return TRUE;
}

static struct signon_meter *find_signon_meter(GaimConnection *gc)
{
	GSList *m = meter_win ? meter_win->meters : NULL;
	struct signon_meter *meter;

	while (m) {
		meter = m->data;
		if (meter->account == gaim_connection_get_account(gc))
			return m->data;
		m = m->next;
	}

	return NULL;
}

static GtkWidget* create_meter_pixmap (GaimConnection *gc)
{
	GdkPixbuf *pb = create_prpl_icon(gc->account);
	GdkPixbuf *scale = gdk_pixbuf_scale_simple(pb, 30,30,GDK_INTERP_BILINEAR);
	GtkWidget *image =
		gtk_image_new_from_pixbuf(scale);
	g_object_unref(G_OBJECT(pb));
	g_object_unref(G_OBJECT(scale));
	return image;
}



static struct signon_meter *
new_meter(GaimConnection *gc, GtkWidget *widget,
			   GtkWidget *table, gint *rows)
{
	GtkWidget *graphic;
	GtkWidget *label;
	GtkWidget *nest_vbox;
	GString *name_to_print;
	struct signon_meter *meter;


	meter = g_new0(struct signon_meter, 1);

	meter->account = gaim_connection_get_account(gc);
	name_to_print = g_string_new(gaim_account_get_username(meter->account));

	(*rows)++;
	gtk_table_resize (GTK_TABLE(table), *rows, 4);

	graphic = create_meter_pixmap(gc);

	nest_vbox = gtk_vbox_new (FALSE, 0);

	g_string_prepend(name_to_print, _("Signon: "));
	label = gtk_label_new (name_to_print->str);
	g_string_free(name_to_print, TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

	meter->status = gtk_label_new("");
	gtk_misc_set_alignment(GTK_MISC(meter->status), 0, 0.5);
	gtk_widget_set_size_request(meter->status, 250, -1);

	meter->progress = gtk_progress_bar_new ();

	meter->button = gaim_pixbuf_button_from_stock (_("Cancel"), GTK_STOCK_CANCEL, GAIM_BUTTON_HORIZONTAL);
	g_signal_connect(G_OBJECT (meter->button), "clicked",
					 G_CALLBACK (cancel_signon), meter);

	gtk_table_attach (GTK_TABLE (table), graphic, 0, 1, *rows, *rows+1, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
	gtk_table_attach (GTK_TABLE (table), nest_vbox, 1, 2, *rows, *rows+1, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
		gtk_box_pack_start (GTK_BOX (nest_vbox), GTK_WIDGET (label), FALSE, FALSE, 0);
		gtk_box_pack_start (GTK_BOX (nest_vbox), GTK_WIDGET (meter->status), FALSE, FALSE, 0);
	gtk_table_attach (GTK_TABLE (table), meter->progress, 2, 3, *rows, *rows+1, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
	gtk_table_attach (GTK_TABLE (table), meter->button, 3, 4, *rows, *rows+1, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);

	gtk_widget_show_all (GTK_WIDGET (meter_win->window));

	meter_win->active_count++;

	return meter;
}

static void kill_meter(struct signon_meter *meter, const char *text) {
	gtk_widget_set_sensitive (meter->button, FALSE);
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(meter->progress), 1);
	gtk_label_set_text(GTK_LABEL(meter->status), text);
	meter_win->active_count--;
	if (meter_win->active_count == 0) {
		gtk_widget_destroy(meter_win->window);
		g_free (meter_win);
		meter_win = NULL;
	}
}

static void gaim_gtk_connection_connect_progress(GaimConnection *gc,
		const char *text, size_t step, size_t step_count)
{
	struct signon_meter *meter;

	if(!meter_win) {
		GtkWidget *vbox;
		GtkWidget *cancel_button;

		if(mainwindow)
			gtk_widget_hide(mainwindow);

		meter_win = g_new0(struct meter_window, 1);
		meter_win->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_resizable(GTK_WINDOW(meter_win->window), FALSE);
		gtk_window_set_role(GTK_WINDOW(meter_win->window), "signon");
		gtk_container_set_border_width(GTK_CONTAINER(meter_win->window), 5);
		gtk_window_set_title(GTK_WINDOW(meter_win->window), _("Signon"));
		gtk_widget_realize(meter_win->window);

		vbox = gtk_vbox_new (FALSE, 0);
		gtk_container_add(GTK_CONTAINER(meter_win->window), vbox);

		meter_win->table = gtk_table_new(1, 4, FALSE);
		gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(meter_win->table),
				FALSE, FALSE, 0);
		gtk_container_set_border_width(GTK_CONTAINER(meter_win->table), 5);
		gtk_table_set_row_spacings(GTK_TABLE(meter_win->table), 5);
		gtk_table_set_col_spacings(GTK_TABLE(meter_win->table), 10);

		cancel_button = gaim_pixbuf_button_from_stock(_("Cancel All"),
				GTK_STOCK_QUIT, GAIM_BUTTON_HORIZONTAL);
		g_signal_connect_swapped(G_OBJECT(cancel_button), "clicked",
				G_CALLBACK(cancel_all), NULL);
		gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(cancel_button),
				FALSE, FALSE, 0);

		g_signal_connect(G_OBJECT(meter_win->window), "delete_event",
				G_CALLBACK(meter_destroy), NULL);
	}

	meter = find_signon_meter(gc);
	if(!meter) {
		meter = new_meter(gc, meter_win->window, meter_win->table,
				&meter_win->rows);

		meter_win->meters = g_slist_append(meter_win->meters, meter);
	}

	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(meter->progress),
			(float)step / (float)step_count);
	gtk_label_set_text(GTK_LABEL(meter->status), text);
}

static void gaim_gtk_connection_connected(GaimConnection *gc)
{
	struct signon_meter *meter = find_signon_meter(gc);

	gaim_setup(gc);

	do_away_menu();
	gaim_gtk_blist_update_protocol_actions();

	if(meter)
		kill_meter(meter, _("Done."));
}

static void gaim_gtk_connection_disconnected(GaimConnection *gc)
{
	struct signon_meter *meter = find_signon_meter(gc);

	do_away_menu();
	gaim_gtk_blist_update_protocol_actions();

	if(meter)
		kill_meter(meter, _("Done."));

	if (gaim_connections_get_all() != NULL)
		return;

	destroy_all_dialogs();

	gaim_blist_destroy();

	show_login();
}

static void gaim_gtk_connection_notice(GaimConnection *gc,
		const char *text)
{
}

struct disconnect_window {
	GtkWidget *window;
	GtkWidget *treeview;
	GtkWidget *sw;
	GtkWidget *label;
	GtkWidget *reconnect_btn;
};
struct disconnect_window *disconnect_window = NULL;

static void disconnect_window_hide()
{
	gtk_widget_destroy(disconnect_window->window);
	g_free(disconnect_window);
	disconnect_window = NULL;
}

static void disconnect_response_cb(GtkDialog *dialog, gint id, GtkWidget *widget)
{
	GtkTreeIter iter;
	GtkTreeSelection *sel = NULL;
	GtkTreeModel *model = NULL;
	GaimAccount *account = NULL;
	
	switch(id) {
	case GTK_RESPONSE_DELETE_EVENT:
	case GTK_RESPONSE_CLOSE:
		disconnect_window_hide();
		break;
	case GTK_RESPONSE_APPLY: //Reconnect All
		model = gtk_tree_view_get_model(GTK_TREE_VIEW(disconnect_window->treeview));
		if (gtk_tree_model_get_iter_first(model, &iter)) {
			/* tree rows to be deleted */
			GList *l_del = NULL, *l_del_iter = NULL;
			/* accounts to be connected */
			GList *l_accts = NULL, *l_accts_iter =NULL;
			do {
				GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
				GtkTreeRowReference* del_row = gtk_tree_row_reference_new(model, path);
				l_del = g_list_append(l_del, del_row);
				gtk_tree_path_free(path);

				gtk_tree_model_get(model, &iter, 4, &account, -1);
				if (!gaim_account_is_connected(account) && g_list_find(l_accts, account) == NULL)
					l_accts = g_list_append(l_accts, account); 
			} while (gtk_tree_model_iter_next(model, &iter));

			/* remove all rows */
			l_del_iter = l_del;
			while (l_del_iter != NULL) {
				GtkTreeRowReference* del_row = l_del_iter->data;
				GtkTreePath *path = gtk_tree_row_reference_get_path(del_row);
				if (gtk_tree_model_get_iter(model, &iter, path))
					gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
				gtk_tree_path_free(path);
				gtk_tree_row_reference_free(del_row);
				l_del_iter = l_del_iter->next;
			}
			g_list_free(l_del);

			/* reconnect disconnected accounts */
			l_accts_iter = l_accts;
			while (l_accts_iter != NULL) {
				account = l_accts_iter->data;
				gaim_account_connect(account);
				l_accts_iter = l_accts_iter->next;
			}
			g_list_free(l_accts);

			/* 
			 * if we don't have any rows left,
			 * meaning that nothing was disconnected during our reconnections,
			 * hide the dialog 
			 */
			if (!gtk_tree_model_get_iter_first(model, &iter))
				disconnect_window_hide();
			else {
				gtk_tree_selection_select_iter(sel, &iter);
				if (!gtk_tree_model_iter_next(model, &iter)) {
					gtk_widget_hide_all(disconnect_window->sw);
				}
			}
		}

		break;
	case GTK_RESPONSE_ACCEPT: //Reconnect Selected
		model = gtk_tree_view_get_model(GTK_TREE_VIEW(disconnect_window->treeview));
		/*
		 * If we have more than one account disconnection displayed, then 
		 * the scroll window is visible and we should use the selected 
		 * account to reconnect.
		 */
		if (GTK_WIDGET_VISIBLE(disconnect_window->sw)) {
			sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(disconnect_window->treeview));
			if (!gtk_tree_selection_get_selected(sel, &model, &iter))
				return;
		} else {
			/* There is only one account disconnection, so reconnect to it. */
			if (!gtk_tree_model_get_iter_first(model, &iter))
				return;
		}
		gtk_tree_model_get(model, &iter, 4, &account, -1);
		/* remove all disconnections of the account to be reconnected */
		if (gtk_tree_model_get_iter_first(model, &iter)) {
			GList *l_del = NULL, *l_del_iter = NULL;
			GaimAccount *account2 = NULL;
			do {
				gtk_tree_model_get(model, &iter, 4, &account2, -1);
				if (account2 == account) {
					GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
					GtkTreeRowReference* del_row = gtk_tree_row_reference_new(model, path);
					l_del = g_list_append(l_del, del_row);
					gtk_tree_path_free(path);
				}
			} while (gtk_tree_model_iter_next(model, &iter));

			l_del_iter = l_del;
			while (l_del_iter != NULL) {
				GtkTreeRowReference* del_row = l_del_iter->data;
				GtkTreePath *path = gtk_tree_row_reference_get_path(del_row);
				if (gtk_tree_model_get_iter(model, &iter, path))
					gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
				gtk_tree_path_free(path);
				gtk_tree_row_reference_free(del_row);
				l_del_iter = l_del_iter->next;
			}
			g_list_free(l_del);
		}

		gaim_account_connect(account);

		if (!gtk_tree_model_get_iter_first(model, &iter))
			disconnect_window_hide();
		else {
			gtk_tree_selection_select_iter(sel, &iter);
			if (!gtk_tree_model_iter_next(model, &iter))
				gtk_widget_hide_all(disconnect_window->sw);
		}
		break;
	}
}

static void disconnect_tree_cb(GtkTreeSelection *sel, GtkTreeModel *model) 
{
	GtkTreeIter  iter;

	if (gtk_tree_selection_get_selected (sel, &model, &iter)) {
		const char *label_text;
		GaimAccount *account = NULL;
		gtk_tree_model_get(model, &iter,
			3, &label_text,	
			4, &account, -1);
		gtk_button_set_label(
			GTK_BUTTON(disconnect_window->reconnect_btn),
			gaim_account_is_connected(account)
				? _("_Remove") 
				: _("_Reconnect"));
		gtk_label_set_markup(GTK_LABEL(disconnect_window->label), label_text);
		gtk_dialog_set_response_sensitive(GTK_DIALOG(disconnect_window->window), GTK_RESPONSE_ACCEPT, TRUE);
	} else
		gtk_dialog_set_response_sensitive(GTK_DIALOG(disconnect_window->window), GTK_RESPONSE_ACCEPT, FALSE);
}

static void disconnect_connection_change_cb(GaimConnection *gc, void *data) {
	if (disconnect_window) {
		GaimAccount *account = gaim_connection_get_account(gc); 
		gboolean signed_off = !gaim_account_is_connected(account); 
		GtkTreeIter iter;
		GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(disconnect_window->treeview));
		GdkPixbuf *icon = create_prpl_icon(account);
		GdkPixbuf *scale = gdk_pixbuf_scale_simple(icon, 16, 16, GDK_INTERP_BILINEAR);
		/* mark all disconnections w/ the account type disconnected /w grey icon */
		if (signed_off)
			gdk_pixbuf_saturate_and_pixelate(scale, scale, 0.0, FALSE);
		if (gtk_tree_model_get_iter_first(model, &iter)) {
			GList *l_disc_accts = NULL;
			GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(disconnect_window->treeview));
			GtkTreeIter iter_selected;
			GtkTreePath *sel_path = NULL;
			if (gtk_tree_selection_get_selected(sel, &model, &iter_selected))
				sel_path = gtk_tree_model_get_path(model, &iter_selected);
			do {
				GaimAccount *account2 = NULL;
				gtk_tree_model_get(model, &iter, 4, &account2, -1);
				if (account2 == account) {
					GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
					gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0, scale, -1);
					/* update the reconnect button if appropriate */
					if (sel_path != NULL && gtk_tree_path_compare(path, sel_path) == 0)
						gtk_button_set_label(
							GTK_BUTTON(disconnect_window->reconnect_btn),
							signed_off ? _("_Reconnect") : _("_Remove"));	
					gtk_tree_path_free(path);
				}
				if (!gaim_account_is_connected(account2)
				&& g_list_find(l_disc_accts, account2) == NULL)
					l_disc_accts = g_list_append(l_disc_accts, account2); 
			} while (gtk_tree_model_iter_next(model, &iter));
			
			gtk_tree_path_free(sel_path);

			gtk_dialog_set_response_sensitive(
				GTK_DIALOG(disconnect_window->window),
				GTK_RESPONSE_APPLY,
				g_list_length(l_disc_accts) > 1);
			g_list_free(l_disc_accts);
		}
		if (icon != NULL) g_object_unref(G_OBJECT(icon));
		if (scale  != NULL) g_object_unref(G_OBJECT(scale));
	}
}

static void
gaim_gtk_connection_report_disconnect(GaimConnection *gc, const char *text)
{
	char *label_text = NULL;
	GtkTreeIter new_row_iter, iter;
	GtkListStore *list_store;
	GtkTreeViewColumn *col;
	GtkTreeSelection *sel = NULL;

	label_text = g_strdup_printf(_("<span weight=\"bold\" size=\"larger\">%s has been disconnected.</span>\n\n%s\n%s"),
				     gaim_account_get_username(gaim_connection_get_account(gc)), gaim_date_full(), 
				     text ? text : _("Reason Unknown."));

	/* Build the window if it isn't there yet */
	if (!disconnect_window) {
		GtkWidget *hbox, *vbox, *img;
		GtkCellRenderer *rend, *rend2;

		disconnect_window = g_new0(struct disconnect_window, 1);
		disconnect_window->window = gtk_dialog_new_with_buttons(GAIM_ALERT_TITLE, NULL, GTK_DIALOG_NO_SEPARATOR,
									GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
		g_signal_connect(G_OBJECT(disconnect_window->window), "response", G_CALLBACK(disconnect_response_cb), disconnect_window);

		gtk_container_set_border_width(GTK_CONTAINER(disconnect_window->window), 6);
		gtk_window_set_resizable(GTK_WINDOW(disconnect_window->window), FALSE);
		gtk_dialog_set_has_separator(GTK_DIALOG(disconnect_window->window), FALSE);
		gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(disconnect_window->window)->vbox), 12);
		gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(disconnect_window->window)->vbox), 6);

		hbox = gtk_hbox_new(FALSE, 12);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(disconnect_window->window)->vbox), hbox);
		img = gtk_image_new_from_stock(GAIM_STOCK_DIALOG_ERROR, GTK_ICON_SIZE_DIALOG);
		gtk_misc_set_alignment(GTK_MISC(img), 0, 0);
		gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);

		vbox = gtk_vbox_new(FALSE, 12);
		gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

		disconnect_window->label = gtk_label_new(label_text);

		gtk_label_set_line_wrap(GTK_LABEL(disconnect_window->label), TRUE);
		gtk_misc_set_alignment(GTK_MISC(disconnect_window->label), 0, 0);
		gtk_box_pack_start(GTK_BOX(vbox), disconnect_window->label, FALSE, FALSE, 0);

		disconnect_window->reconnect_btn = gtk_dialog_add_button(
			GTK_DIALOG(disconnect_window->window),
			_("_Reconnect"),
			GTK_RESPONSE_ACCEPT);

		gtk_dialog_add_button(
			GTK_DIALOG(disconnect_window->window),
			_("Reconnect _All"),
			GTK_RESPONSE_APPLY);

		gtk_widget_show_all(disconnect_window->window);


		/* Tree View */
		disconnect_window->sw = gtk_scrolled_window_new(NULL,NULL);
		gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW(disconnect_window->sw), GTK_SHADOW_IN);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(disconnect_window->sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
		gtk_box_pack_start(GTK_BOX(vbox), disconnect_window->sw, TRUE, TRUE, 0);

		list_store = gtk_list_store_new (5, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
		disconnect_window->treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL(list_store));

		rend = gtk_cell_renderer_pixbuf_new();
		rend2 = gtk_cell_renderer_text_new();
		col = gtk_tree_view_column_new();
		gtk_tree_view_column_set_title(col, _("Account"));
		gtk_tree_view_column_pack_start(col, rend, FALSE);
		gtk_tree_view_column_pack_start(col, rend2, FALSE);
		gtk_tree_view_column_set_attributes(col, rend, "pixbuf", 0, NULL);
		gtk_tree_view_column_set_attributes(col, rend2, "text", 1, NULL);
		gtk_tree_view_append_column (GTK_TREE_VIEW(disconnect_window->treeview), col);

		rend = gtk_cell_renderer_text_new();
		col = gtk_tree_view_column_new_with_attributes (_("Time"),
								rend, "text", 2, NULL);
		gtk_tree_view_append_column (GTK_TREE_VIEW(disconnect_window->treeview), col);
		
		g_object_unref(G_OBJECT(list_store));
		gtk_container_add(GTK_CONTAINER(disconnect_window->sw), disconnect_window->treeview);

		sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (disconnect_window->treeview));
		gtk_widget_set_size_request(disconnect_window->treeview, -1, 96);
		g_signal_connect (G_OBJECT (sel), "changed",
				  G_CALLBACK (disconnect_tree_cb), list_store);

		gaim_signal_connect(gaim_connections_get_handle(), "signed-on",
				disconnect_window, GAIM_CALLBACK(disconnect_connection_change_cb), NULL);

		gaim_signal_connect(gaim_connections_get_handle(), "signed-off",
				disconnect_window, GAIM_CALLBACK(disconnect_connection_change_cb), NULL);

	} else
		list_store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(disconnect_window->treeview)));

	/* Add this account to our list of disconnected accounts */
	gtk_list_store_append(list_store, &new_row_iter);
	gtk_list_store_set(list_store, &new_row_iter,
			   0, NULL,
			   1, gaim_account_get_username(gaim_connection_get_account(gc)),
			   2, gaim_date_full(),
			   3, label_text,
			   4, gaim_connection_get_account(gc), -1);

	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(list_store), &iter))
	{
		sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(disconnect_window->treeview));
		gtk_tree_selection_select_iter(sel, &new_row_iter);
		/* if we have more than one disconnected acct, display the treeview */
		if (gtk_tree_model_iter_next(GTK_TREE_MODEL(list_store), &iter))
			gtk_widget_show_all(disconnect_window->sw);
	}

	g_free(label_text);
	gtk_window_present(GTK_WINDOW(disconnect_window->window));
}

static GaimConnectionUiOps conn_ui_ops =
{
	gaim_gtk_connection_connect_progress,
	gaim_gtk_connection_connected,
	gaim_gtk_connection_disconnected,
	gaim_gtk_connection_notice,
	gaim_gtk_connection_report_disconnect
};

GaimConnectionUiOps *
gaim_gtk_connections_get_ui_ops(void)
{
	return &conn_ui_ops;
}

void away_on_login(const char *mesg)
{
	GSList *awy = away_messages;
	struct away_message *a, *message = NULL;
	GaimGtkBuddyList *gtkblist;

	gtkblist = GAIM_GTK_BLIST(gaim_get_blist());

	if (!gtkblist->window) {
		return;
	}

	if (mesg == NULL)
		mesg = gaim_prefs_get_string("/core/away/default_message");
	while (awy) {
		a = (struct away_message *)awy->data;
		if (strcmp(a->name, mesg) == 0) {
			message = a;
			break;
		}
		awy = awy->next;
	}
	if (message == NULL) {
		if(!away_messages)
			return;
		message = away_messages->data;
	}
	do_away_message(NULL, message);
}
