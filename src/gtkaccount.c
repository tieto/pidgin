/**
 * @file gtkaccount.c Account Editor dialog
 * @ingroup gtkui
 *
 * gaim
 *
 * Copyright (C) 2002-2003, Christian Hammond <chipx86@gnupdate.org>
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
#include "gtkaccount.h"
#include "account.h"
#include "accountopt.h"
#include "event.h"
#include "prefs.h"
#include "stock.h"
#include "gtkblist.h"

#ifdef _WIN32       
# include <gdk/gdkwin32.h>
#else
# include <unistd.h>
# include <gdk/gdkx.h>
#endif

#include <string.h>

enum
{
	COLUMN_ICON,
	COLUMN_PROTOCOL,
	COLUMN_SCREENNAME,
	COLUMN_ONLINE,
	COLUMN_AUTOLOGIN,
	COLUMN_FILLER,
	COLUMN_DATA,
	NUM_COLUMNS
};

typedef enum
{
	ADD_ACCOUNT_DIALOG,
	MODIFY_ACCOUNT_DIALOG

} AccountPrefsDialogType;

typedef struct
{
	GtkWidget *window;
	GtkWidget *treeview;

	GtkListStore *model;
	GtkTreeIter drag_iter;

	GtkTreeViewColumn *screenname_col;

} AccountsDialog;

typedef struct
{
	AccountPrefsDialogType type;

	GaimAccount *account;

	GtkWidget *window;
	GtkWidget *login_frame;
	GtkWidget *protocol_menu;

	GtkWidget *screenname_entry;
	GtkWidget *password_entry;
	GtkWidget *alias_entry;

	GtkWidget *remember_pass_check;
	GtkWidget *auto_login_check;

	GtkSizeGroup *sg;

} AccountPrefsDialog;


static AccountsDialog *accounts_dialog = NULL;


static char *
proto_name(int proto)
{
	GaimPlugin *p = gaim_find_prpl(proto);

	return ((p && p->info->name) ? _(p->info->name) : _("Unknown"));
}

/**************************************************************************
 * Add/Modify Account dialog
 **************************************************************************/
static void
__set_account_protocol(GtkWidget *item, GaimProtocol protocol,
					   AccountPrefsDialog *dialog)
{
}

static GtkWidget *
__add_pref_box(AccountPrefsDialog *dialog, GtkWidget *parent,
			   const char *text, GtkWidget *widget)
{
	GtkWidget *hbox;
	GtkWidget *label;

	hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(parent), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new(text);
	gtk_size_group_add_widget(dialog->sg, label);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0);
	gtk_widget_show(widget);

	return hbox;
}

static void
__add_login_options(AccountPrefsDialog *dialog, GtkWidget *parent)
{
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *entry;
	GaimPlugin *plugin = NULL;
	GaimPluginProtocolInfo *prpl_info = NULL;
	GaimProtocol protocol;
	GList *user_splits;
	GList *split_entries = NULL;
	GList *l, *l2;
	char *username = NULL;

	if (dialog->login_frame != NULL)
		gtk_widget_destroy(dialog->login_frame);

	if (dialog->account == NULL)
		protocol = GAIM_PROTO_OSCAR;
	else
		protocol = gaim_account_get_protocol(dialog->account);

	if ((plugin = gaim_find_prpl(protocol)) != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(plugin);


	/* Build the login options frame. */
	frame = gaim_gtk_make_frame(parent, _("Login Options"));

	/* cringe */
	dialog->login_frame = gtk_widget_get_parent(gtk_widget_get_parent(frame));

	gtk_box_reorder_child(GTK_BOX(parent), dialog->login_frame, 0);
	gtk_widget_show(dialog->login_frame);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	/* Protocol */
	dialog->protocol_menu = gaim_gtk_protocol_option_menu_new(
			protocol, G_CALLBACK(__set_account_protocol), dialog);

	__add_pref_box(dialog, vbox, _("Protocol:"), dialog->protocol_menu);

	/* Screen Name */
	dialog->screenname_entry = gtk_entry_new();

	__add_pref_box(dialog, vbox, _("Screenname:"), dialog->screenname_entry);

	/* Do the user split thang */
	if (plugin == NULL) /* Yeah right. */
		user_splits = NULL;
	else
		user_splits = prpl_info->user_splits;

	if (dialog->account != NULL)
		username = g_strdup(gaim_account_get_username(dialog->account));

	for (l = user_splits; l != NULL; l = l->next) {
		GaimAccountUserSplit *split = l->data;
		char *buf;

		buf = g_strdup_printf("%s:", gaim_account_user_split_get_text(split));

		entry = gtk_entry_new();

		__add_pref_box(dialog, vbox, buf, entry);

		g_free(buf);

		split_entries = g_list_append(split_entries, entry);
	}

	for (l = g_list_last(split_entries), l2 = g_list_last(user_splits);
		 l != NULL && l2 != NULL;
		 l = l->prev, l2 = l2->prev) {

		GaimAccountUserSplit *split = l2->data;
		GtkWidget *entry = l->data;
		const char *value = NULL;
		char *c;

		if (dialog->account != NULL) {
			c = strrchr(username,
						gaim_account_user_split_get_separator(split));

			if (c != NULL) {
				*c = '\0';
				c++;

				value = c;
			}
		}

		if (value == NULL)
			value = gaim_account_user_split_get_default_value(split);

		if (value != NULL)
			gtk_entry_set_text(GTK_ENTRY(entry), value);
	}

	if (username != NULL)
		gtk_entry_set_text(GTK_ENTRY(dialog->screenname_entry), username);

	g_free(username);

	g_list_free(split_entries);

	/* Password */
	dialog->password_entry = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(dialog->password_entry), FALSE);
	__add_pref_box(dialog, vbox, _("Password:"), dialog->password_entry);

	/* Alias */
	dialog->alias_entry = gtk_entry_new();
	__add_pref_box(dialog, vbox, _("Alias:"), dialog->alias_entry);

	/* Remember Password */
	dialog->remember_pass_check =
		gtk_check_button_new_with_label(_("Remember password"));
	gtk_box_pack_start(GTK_BOX(vbox), dialog->remember_pass_check,
					   FALSE, FALSE, 0);
	gtk_widget_show(dialog->remember_pass_check);

	/* Auto-Login */
	dialog->auto_login_check =
		gtk_check_button_new_with_label(_("Auto-login"));
	gtk_box_pack_start(GTK_BOX(vbox), dialog->auto_login_check,
					   FALSE, FALSE, 0);
	gtk_widget_show(dialog->auto_login_check);

	/* Set the fields. */
	if (dialog->account != NULL) {
		if (gaim_account_get_password(dialog->account))
			gtk_entry_set_text(GTK_ENTRY(dialog->password_entry),
							   gaim_account_get_password(dialog->account));

		if (gaim_account_get_alias(dialog->account))
			gtk_entry_set_text(GTK_ENTRY(dialog->alias_entry),
							   gaim_account_get_alias(dialog->account));

		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON(dialog->remember_pass_check),
				gaim_account_get_remember_password(dialog->account));

		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON(dialog->auto_login_check), FALSE);
	}

	if (prpl_info != NULL && (prpl_info->options & OPT_PROTO_NO_PASSWORD)) {
		gtk_widget_hide(dialog->password_entry);
		gtk_widget_hide(dialog->remember_pass_check);
	}
}

static void
__show_account_prefs(AccountPrefsDialogType type, GaimAccount *account)
{
	AccountPrefsDialog *dialog;
	GtkWidget *win;
	GtkWidget *vbox;
	GtkWidget *bbox;
	GtkWidget *disclosure;
	GtkWidget *sep;
	GtkWidget *button;

	dialog = g_new0(AccountPrefsDialog, 1);

	dialog->account = account;
	dialog->type    = type;
	dialog->sg      = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	GAIM_DIALOG(win);
	dialog->window = win;

	gtk_window_set_role(GTK_WINDOW(win), "account");

	if (type == ADD_ACCOUNT_DIALOG)
		gtk_window_set_title(GTK_WINDOW(win), _("Add Account"));
	else
		gtk_window_set_title(GTK_WINDOW(win), _("Modify Account"));

	gtk_container_set_border_width(GTK_CONTAINER(win), 12);

#if 0
	g_signal_connect(G_OBJECT(win), "delete_event",
					 G_CALLBACK(__account_win_destroy_cb), dialog);
#endif

	/* Setup the vbox */
	vbox = gtk_vbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(win), vbox);
	gtk_widget_show(vbox);

	/* Setup the top frames. */
	__add_login_options(dialog, vbox);
#if 0
	__add_user_options(dialog, vbox);
#endif

	/* Separator... */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);
	gtk_widget_show(sep);

	/* Setup the button box */
	bbox = gtk_hbutton_box_new();
	gtk_box_set_spacing(GTK_BOX(bbox), 6);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_box_pack_end(GTK_BOX(vbox), bbox, FALSE, TRUE, 0);
	gtk_widget_show(bbox);

	/* Cancel button */
	button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	/* OK button */
	button = gtk_button_new_from_stock(GTK_STOCK_OK);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	/* Show the window. */
	gtk_widget_show(win);
}

/**************************************************************************
 * Accounts Dialog
 **************************************************************************/

static void
__signed_on_off_cb(GaimConnection *gc, AccountsDialog *dialog)
{
	GaimAccount *account = gaim_connection_get_account(gc);
	GtkTreeModel *model = GTK_TREE_MODEL(dialog->model);
	GtkTreeIter iter;
	size_t index = g_list_index(gaim_accounts_get_all(), account);

	if (gtk_tree_model_iter_nth_child(model, &iter, NULL, index)) {
		gtk_list_store_set(dialog->model, &iter,
						   COLUMN_ONLINE, gaim_account_is_connected(account),
						   -1);
	}
}

static void
__drag_data_get_cb(GtkWidget *widget, GdkDragContext *ctx,
				   GtkSelectionData *data, guint info, guint time,
				   AccountsDialog *dialog)
{
	if (data->target == gdk_atom_intern("GAIM_ACCOUNT", FALSE)) {
		GtkTreeRowReference *ref;
		GtkTreePath *source_row;
		GtkTreeIter iter;
		GaimAccount *account = NULL;
		GValue val = {0};

		ref = g_object_get_data(G_OBJECT(ctx), "gtk-tree-view-source-row");
		source_row = gtk_tree_row_reference_get_path(ref);

		if (source_row == NULL)
			return;

		gtk_tree_model_get_iter(GTK_TREE_MODEL(dialog->model), &iter,
								source_row);
		gtk_tree_model_get_value(GTK_TREE_MODEL(dialog->model), &iter,
								 COLUMN_DATA, &val);

		dialog->drag_iter = iter;

		account = g_value_get_pointer(&val);

		gtk_selection_data_set(data, gdk_atom_intern("GAIM_ACCOUNT", FALSE),
							   8, (void *)&account, sizeof(account));

		gtk_tree_path_free(source_row);
	}
}

static void
__drag_data_received_cb(GtkWidget *widget, GdkDragContext *ctx,
						guint x, guint y, GtkSelectionData *sd,
						guint info, guint t, AccountsDialog *dialog)
{
	if (sd->target == gdk_atom_intern("GAIM_ACCOUNT", FALSE) && sd->data) {
		size_t dest_index;
		GaimAccount *a = NULL;
		GtkTreePath *path = NULL;
		GtkTreeViewDropPosition position;

		memcpy(&a, sd->data, sizeof(a));

		if (gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(widget), x, y,
											  &path, &position)) {

			GtkTreeIter iter;
			GaimAccount *account;
			GValue val = {0};

			gtk_tree_model_get_iter(GTK_TREE_MODEL(dialog->model), &iter, path);
			gtk_tree_model_get_value(GTK_TREE_MODEL(dialog->model), &iter,
									 COLUMN_DATA, &val);

			account = g_value_get_pointer(&val);

			switch (position) {
				case GTK_TREE_VIEW_DROP_AFTER:
				case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
					gaim_debug(GAIM_DEBUG_MISC, "gtkaccount",
							   "after\n");
					gtk_list_store_move_after(dialog->model,
											  &dialog->drag_iter, &iter);
					dest_index = g_list_index(gaim_accounts_get_all(),
											  account) + 1;
					break;

				case GTK_TREE_VIEW_DROP_BEFORE:
				case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
					gaim_debug(GAIM_DEBUG_MISC, "gtkaccount",
							   "before\n");
					dest_index = g_list_index(gaim_accounts_get_all(),
											  account);

					gaim_debug(GAIM_DEBUG_MISC, "gtkaccount",
							   "iter = %p\n", &iter);
					gaim_debug(GAIM_DEBUG_MISC, "gtkaccount",
							   "account = %s\n",
							   gaim_account_get_username(account));

					/*
					 * Somebody figure out why inserting before the first
					 * account sometimes moves to the end, please :(
					 */
					if (dest_index == 0) {
						gtk_list_store_move_after(dialog->model,
												  &dialog->drag_iter, &iter);
						gtk_list_store_swap(dialog->model, &iter,
											&dialog->drag_iter);
					}
					else {
						gtk_list_store_move_before(dialog->model,
												   &dialog->drag_iter, &iter);
					}

					break;

				default:
					return;
			}

			gaim_accounts_reorder(a, dest_index);
		}
	}
}

static gint
__accedit_win_destroy_cb(GtkWidget *w, GdkEvent *event, AccountsDialog *dialog)
{
	g_free(accounts_dialog);
	accounts_dialog = NULL;

	/* See if we're the main window here. */
	if (GAIM_GTK_BLIST(gaim_get_blist())->window == NULL &&
		mainwindow == NULL && gaim_connections_get_all() == NULL) {

		do_quit();
	}

	return FALSE;
}

static gboolean
__configure_cb(GtkWidget *w, GdkEventConfigure *event, AccountsDialog *dialog)
{
	if (GTK_WIDGET_VISIBLE(w)) {
		int old_width = gaim_prefs_get_int("/gaim/gtk/accounts/dialog/width");
		int col_width;
		int difference;

		gaim_prefs_set_int("/gaim/gtk/accounts/dialog/width",  event->width);
		gaim_prefs_set_int("/gaim/gtk/accounts/dialog/height", event->height);

		col_width = gtk_tree_view_column_get_width(dialog->screenname_col);

		if (col_width == 0)
			return FALSE;

		difference = (MAX(old_width, event->width) -
					  MIN(old_width, event->width));

		if (difference == 0)
			return FALSE;

		if (old_width < event->width)
			gtk_tree_view_column_set_min_width(dialog->screenname_col,
					col_width + difference);
		else
			gtk_tree_view_column_set_max_width(dialog->screenname_col,
					col_width - difference);
	}

	return FALSE;
}

static void
__add_account_cb(GtkWidget *w, AccountsDialog *dialog)
{
	__show_account_prefs(ADD_ACCOUNT_DIALOG, NULL);
}

static void
__modify_account_sel(GtkTreeModel *model, GtkTreePath *path,
					 GtkTreeIter *iter, gpointer data)
{
	GaimAccount *account;

	gtk_tree_model_get(model, iter, COLUMN_DATA, &account, -1);

	if (account != NULL)
		__show_account_prefs(MODIFY_ACCOUNT_DIALOG, account);
}

static void
__modify_account_cb(GtkWidget *w, AccountsDialog *dialog)
{
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->treeview));

	gtk_tree_selection_selected_foreach(selection, __modify_account_sel, NULL);
}

static void
__delete_account_cb(GtkWidget *w, AccountsDialog *dialog)
{

}

static void
__close_accounts_cb(GtkWidget *w, AccountsDialog *dialog)
{
	gtk_widget_destroy(dialog->window);

	__accedit_win_destroy_cb(NULL, NULL, dialog);
}

static void
__online_cb(GtkCellRendererToggle *renderer, gchar *path_str, gpointer data)
{
	AccountsDialog *dialog = (AccountsDialog *)data;
	GaimAccount *account;
	GtkTreeModel *model = GTK_TREE_MODEL(dialog->model);
	GtkTreeIter iter;
	gboolean online;

	gtk_tree_model_get_iter_from_string(model, &iter, path_str);
	gtk_tree_model_get(model, &iter, COLUMN_DATA, &account,
			COLUMN_ONLINE, &online, -1);

	if (online)
		gaim_account_disconnect(account);
	else
		gaim_account_connect(account);
}

static void
__autologin_cb(GtkCellRendererToggle *renderer, gchar *path_str,
			   gpointer data)
{
	
}

static void
__add_columns(GtkWidget *treeview, AccountsDialog *dialog)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* Protocol */
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Protocol"));
	gtk_tree_view_insert_column(GTK_TREE_VIEW(treeview), column, -1);

	/* Icon text */
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_add_attribute(column, renderer,
									   "pixbuf", COLUMN_ICON);

	/* Protocol name */
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer,
									   "text", COLUMN_PROTOCOL);

	/* Screennames */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Screenname"),
				renderer, "text", COLUMN_SCREENNAME, NULL);
	dialog->screenname_col = column;

	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	gtk_tree_view_insert_column(GTK_TREE_VIEW(treeview), column, -1);

	/* Online? */
	renderer = gtk_cell_renderer_toggle_new();
	
	g_signal_connect(G_OBJECT(renderer), "toggled",
					 G_CALLBACK(__online_cb), dialog);

	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview),
												-1, _("Online"),
												renderer,
												"active", COLUMN_ONLINE,
												NULL);

	/* Auto-login? */
	renderer = gtk_cell_renderer_toggle_new();

	g_signal_connect(G_OBJECT(renderer), "toggled",
					 G_CALLBACK(__autologin_cb), dialog);

	column = gtk_tree_view_column_new_with_attributes(_("Auto-login"),
			renderer, "active", COLUMN_AUTOLOGIN, NULL);

	gtk_tree_view_insert_column(GTK_TREE_VIEW(treeview), column, -1);

	/* Filler */
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview),
												-1, "", renderer,
												"visible", COLUMN_FILLER,
												NULL);
}

static void
__populate_accounts_list(AccountsDialog *dialog)
{
	GList *l;
	GaimAccount *account;
	GtkTreeIter iter;
	GdkPixbuf *pixbuf;
	GdkPixbuf *scale;

	gtk_list_store_clear(dialog->model);

	for (l = gaim_accounts_get_all(); l != NULL; l = l->next) {
		account = l->data;

		scale = NULL;

		pixbuf = create_prpl_icon(account);

		if (pixbuf != NULL)
			scale = gdk_pixbuf_scale_simple(pixbuf, 16, 16,
											GDK_INTERP_BILINEAR);

		gtk_list_store_append(dialog->model, &iter);
		gtk_list_store_set(dialog->model, &iter,
				COLUMN_ICON, scale,
				COLUMN_SCREENNAME, gaim_account_get_username(account),
				COLUMN_ONLINE, gaim_account_is_connected(account),
				COLUMN_AUTOLOGIN, FALSE,
				COLUMN_PROTOCOL, proto_name(gaim_account_get_protocol(account)),
				COLUMN_DATA, account,
				COLUMN_FILLER, FALSE,
				-1);

		if (pixbuf != NULL) g_object_unref(G_OBJECT(pixbuf));
		if (scale  != NULL) g_object_unref(G_OBJECT(scale));
	}
}

static GtkWidget *
__create_accounts_list(AccountsDialog *dialog)
{
	GtkWidget *sw;
	GtkWidget *treeview;
	GtkTargetEntry gte[] = {{"GAIM_ACCOUNT", GTK_TARGET_SAME_APP, 0}};

	/* Create the scrolled window. */
	sw = gtk_scrolled_window_new(0, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
								   GTK_POLICY_AUTOMATIC,
								   GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
										GTK_SHADOW_IN);
	gtk_widget_show(sw);

	/* Create the list model. */
	dialog->model = gtk_list_store_new(NUM_COLUMNS, GDK_TYPE_PIXBUF,
									   G_TYPE_STRING, G_TYPE_STRING,
									   G_TYPE_BOOLEAN, G_TYPE_BOOLEAN,
									   G_TYPE_BOOLEAN, G_TYPE_POINTER);

	/* And now the actual treeview */
	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(dialog->model));
	dialog->treeview = treeview;
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);
	gtk_tree_selection_set_mode(
			gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview)),
			GTK_SELECTION_MULTIPLE);

	gtk_container_add(GTK_CONTAINER(sw), treeview);
	gtk_widget_show(treeview);

	__add_columns(treeview, dialog);

	__populate_accounts_list(dialog);

	/* Setup DND. I wanna be an orc! */
	gtk_tree_view_enable_model_drag_source(
			GTK_TREE_VIEW(treeview), GDK_BUTTON1_MASK, gte,
			2, GDK_ACTION_COPY);
	gtk_tree_view_enable_model_drag_dest(
			GTK_TREE_VIEW(treeview), gte, 2,
			GDK_ACTION_COPY | GDK_ACTION_MOVE);

	g_signal_connect(G_OBJECT(treeview), "drag-data-received",
					 G_CALLBACK(__drag_data_received_cb), dialog);
	g_signal_connect(G_OBJECT(treeview), "drag-data-get",
					 G_CALLBACK(__drag_data_get_cb), dialog);

	return sw;
}

void
gaim_gtk_account_dialog_show(void)
{
	AccountsDialog *dialog;
	GtkWidget *win;
	GtkWidget *vbox;
	GtkWidget *bbox;
	GtkWidget *sw;
	GtkWidget *sep;
	GtkWidget *button;
	int width, height;

	if (accounts_dialog != NULL)
		return;

	accounts_dialog = dialog = g_new0(AccountsDialog, 1);

	width  = gaim_prefs_get_int("/gaim/gtk/accounts/dialog/width");
	height = gaim_prefs_get_int("/gaim/gtk/accounts/dialog/height");

	GAIM_DIALOG(win);

	dialog->window = win;

	gtk_window_set_default_size(GTK_WINDOW(win), width, height);
	gtk_window_set_role(GTK_WINDOW(win), "accounts");
	gtk_window_set_title(GTK_WINDOW(win), "Accounts");
	gtk_container_set_border_width(GTK_CONTAINER(win), 12);

	g_signal_connect(G_OBJECT(win), "delete_event",
					 G_CALLBACK(__accedit_win_destroy_cb), accounts_dialog);
	g_signal_connect(G_OBJECT(win), "configure_event",
					 G_CALLBACK(__configure_cb), accounts_dialog);

	/* Setup the vbox */
	vbox = gtk_vbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(win), vbox);
	gtk_widget_show(vbox);

	/* Setup the scrolled window that will contain the list of accounts. */
	sw = __create_accounts_list(dialog);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
	gtk_widget_show(sw);

	/* Separator... */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);
	gtk_widget_show(sep);

	/* Button box. */
	bbox = gtk_hbutton_box_new();
	gtk_box_set_spacing(GTK_BOX(bbox), 6);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_box_pack_end(GTK_BOX(vbox), bbox, FALSE, TRUE, 0);
	gtk_widget_show(bbox);

	/* Add button */
	button = gtk_button_new_from_stock(GTK_STOCK_ADD);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(__add_account_cb), dialog);

	/* Modify button */
	button = gtk_button_new_from_stock(GAIM_STOCK_MODIFY);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(__modify_account_cb), dialog);

	/* Delete button */
	button = gtk_button_new_from_stock(GTK_STOCK_DELETE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(__delete_account_cb), dialog);

	/* Close button */
	button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(__close_accounts_cb), dialog);

	/* Setup some gaim signal handlers. */
	gaim_signal_connect(dialog, event_signon,  __signed_on_off_cb, dialog);
	gaim_signal_connect(dialog, event_signoff, __signed_on_off_cb, dialog);

	gtk_widget_show(win);
}

