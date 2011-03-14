/**
 * @file gtkprivacy.c GTK+ Privacy UI
 * @ingroup pidgin
 */

/* pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#include "internal.h"
#include "pidgin.h"

#include "connection.h"
#include "debug.h"
#include "privacy.h"
#include "request.h"
#include "util.h"

#include "gtkblist.h"
#include "gtkprivacy.h"
#include "gtkutils.h"

typedef struct
{
	GtkWidget *win;

	GtkWidget *type_menu;

	GtkWidget *add_button;
	GtkWidget *remove_button;
	GtkWidget *removeall_button;
	GtkWidget *close_button;

	GtkWidget *button_box;
	GtkWidget *allow_widget;
	GtkWidget *block_widget;

	GtkListStore *allow_store;
	GtkListStore *block_store;

	GtkWidget *allow_list;
	GtkWidget *block_list;

	gboolean in_allow_list;

	PurpleAccount *account;

} PidginPrivacyDialog;

typedef struct
{
	PurpleAccount *account;
	char *name;
	gboolean block;

} PidginPrivacyRequestData;

static struct
{
	const char *text;
	int num;

} const menu_entries[] =
{
	{ N_("Allow all users to contact me"),         PURPLE_PRIVACY_ALLOW_ALL },
	{ N_("Allow only the users on my buddy list"), PURPLE_PRIVACY_ALLOW_BUDDYLIST },
	{ N_("Allow only the users below"),            PURPLE_PRIVACY_ALLOW_USERS },
	{ N_("Block all users"),                       PURPLE_PRIVACY_DENY_ALL },
	{ N_("Block only the users below"),            PURPLE_PRIVACY_DENY_USERS }
};

static const size_t menu_entry_count = sizeof(menu_entries) / sizeof(*menu_entries);

static PidginPrivacyDialog *privacy_dialog = NULL;

static void
rebuild_allow_list(PidginPrivacyDialog *dialog)
{
	GSList *l;
	GtkTreeIter iter;

	gtk_list_store_clear(dialog->allow_store);

	for (l = dialog->account->permit; l != NULL; l = l->next) {
		gtk_list_store_append(dialog->allow_store, &iter);
		gtk_list_store_set(dialog->allow_store, &iter, 0, l->data, -1);
	}
}

static void
rebuild_block_list(PidginPrivacyDialog *dialog)
{
	GSList *l;
	GtkTreeIter iter;

	gtk_list_store_clear(dialog->block_store);

	for (l = dialog->account->deny; l != NULL; l = l->next) {
		gtk_list_store_append(dialog->block_store, &iter);
		gtk_list_store_set(dialog->block_store, &iter, 0, l->data, -1);
	}
}

static void
user_selected_cb(GtkTreeSelection *sel, PidginPrivacyDialog *dialog)
{
	gtk_widget_set_sensitive(dialog->remove_button, TRUE);
}

static GtkWidget *
build_list(PidginPrivacyDialog *dialog, GtkListStore *model,
		   GtkWidget **ret_treeview)
{
	GtkWidget *sw;
	GtkWidget *treeview;
	GtkCellRenderer *rend;
	GtkTreeViewColumn *column;
	GtkTreeSelection *sel;

	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(model));
	*ret_treeview = treeview;

	rend = gtk_cell_renderer_text_new();

	column = gtk_tree_view_column_new_with_attributes(NULL, rend,
													  "text", 0,
													  NULL);
	gtk_tree_view_column_set_clickable(GTK_TREE_VIEW_COLUMN(column), TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);
	sw = pidgin_make_scrollable(treeview, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC, GTK_SHADOW_IN, -1, 200);

	gtk_widget_show(treeview);

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));

	g_signal_connect(G_OBJECT(sel), "changed",
					 G_CALLBACK(user_selected_cb), dialog);

	return sw;
}

static GtkWidget *
build_allow_list(PidginPrivacyDialog *dialog)
{
	GtkWidget *widget;
	GtkWidget *list;

	dialog->allow_store = gtk_list_store_new(1, G_TYPE_STRING);

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(dialog->allow_store), 0, GTK_SORT_ASCENDING);

	widget = build_list(dialog, dialog->allow_store, &list);

	dialog->allow_list = list;

	rebuild_allow_list(dialog);

	return widget;
}

static GtkWidget *
build_block_list(PidginPrivacyDialog *dialog)
{
	GtkWidget *widget;
	GtkWidget *list;

	dialog->block_store = gtk_list_store_new(1, G_TYPE_STRING);

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(dialog->block_store), 0, GTK_SORT_ASCENDING);

	widget = build_list(dialog, dialog->block_store, &list);

	dialog->block_list = list;

	rebuild_block_list(dialog);

	return widget;
}

static gint
destroy_cb(GtkWidget *w, GdkEvent *event, PidginPrivacyDialog *dialog)
{
	pidgin_privacy_dialog_hide();

	return 0;
}

static void
select_account_cb(GtkWidget *dropdown, PurpleAccount *account,
				  PidginPrivacyDialog *dialog)
{
	int i;

	dialog->account = account;

	for (i = 0; i < menu_entry_count; i++) {
		if (menu_entries[i].num == account->perm_deny) {
			gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->type_menu), i);
			break;
		}
	}

	rebuild_allow_list(dialog);
	rebuild_block_list(dialog);
}

/*
 * TODO: Setting the permit/deny setting needs to go through privacy.c
 *       Even better: the privacy API needs to not suck.
 */
static void
type_changed_cb(GtkComboBox *combo, PidginPrivacyDialog *dialog)
{
	int new_type = menu_entries[gtk_combo_box_get_active(combo)].num;

	dialog->account->perm_deny = new_type;
	serv_set_permit_deny(purple_account_get_connection(dialog->account));

	gtk_widget_hide(dialog->allow_widget);
	gtk_widget_hide(dialog->block_widget);
	gtk_widget_hide_all(dialog->button_box);

	if (new_type == PURPLE_PRIVACY_ALLOW_USERS) {
		gtk_widget_show(dialog->allow_widget);
		gtk_widget_show_all(dialog->button_box);
		dialog->in_allow_list = TRUE;
	}
	else if (new_type == PURPLE_PRIVACY_DENY_USERS) {
		gtk_widget_show(dialog->block_widget);
		gtk_widget_show_all(dialog->button_box);
		dialog->in_allow_list = FALSE;
	}

	gtk_widget_show_all(dialog->close_button);
	gtk_widget_show(dialog->button_box);

	purple_blist_schedule_save();
	pidgin_blist_refresh(purple_get_blist());
}

static void
add_cb(GtkWidget *button, PidginPrivacyDialog *dialog)
{
	if (dialog->in_allow_list)
		pidgin_request_add_permit(dialog->account, NULL);
	else
		pidgin_request_add_block(dialog->account, NULL);
}

static void
remove_cb(GtkWidget *button, PidginPrivacyDialog *dialog)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeSelection *sel;
	char *name;

	if (dialog->in_allow_list && dialog->allow_store == NULL)
		return;

	if (!dialog->in_allow_list && dialog->block_store == NULL)
		return;

	if (dialog->in_allow_list) {
		model = GTK_TREE_MODEL(dialog->allow_store);
		sel   = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->allow_list));
	}
	else {
		model = GTK_TREE_MODEL(dialog->block_store);
		sel   = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->block_list));
	}

	if (gtk_tree_selection_get_selected(sel, NULL, &iter))
		gtk_tree_model_get(model, &iter, 0, &name, -1);
	else
		return;

	if (dialog->in_allow_list)
		purple_privacy_permit_remove(dialog->account, name, FALSE);
	else
		purple_privacy_deny_remove(dialog->account, name, FALSE);

	g_free(name);
}

static void
removeall_cb(GtkWidget *button, PidginPrivacyDialog *dialog)
{
	GSList *l;
	if (dialog->in_allow_list)
		l = dialog->account->permit;
	else
		l = dialog->account->deny;
	while (l) {
		char *user;
		user = l->data;
		l = l->next;
		if (dialog->in_allow_list)
			purple_privacy_permit_remove(dialog->account, user, FALSE);
		else
			purple_privacy_deny_remove(dialog->account, user, FALSE);
	}
}

static void
close_cb(GtkWidget *button, PidginPrivacyDialog *dialog)
{
	gtk_widget_destroy(dialog->win);

	pidgin_privacy_dialog_hide();
}

static PidginPrivacyDialog *
privacy_dialog_new(void)
{
	PidginPrivacyDialog *dialog;
	GtkWidget *vbox;
	GtkWidget *button;
	GtkWidget *dropdown;
	GtkWidget *label;
	int selected = -1;
	int i;

	dialog = g_new0(PidginPrivacyDialog, 1);

	dialog->win = pidgin_create_dialog(_("Privacy"), PIDGIN_HIG_BORDER, "privacy", TRUE);

	g_signal_connect(G_OBJECT(dialog->win), "delete_event",
					 G_CALLBACK(destroy_cb), dialog);

	/* Main vbox */
	vbox = pidgin_dialog_get_vbox_with_properties(GTK_DIALOG(dialog->win), FALSE, PIDGIN_HIG_BORDER);

	/* Description label */
	label = gtk_label_new(
		_("Changes to privacy settings take effect immediately."));

	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_widget_show(label);

	/* Accounts drop-down */
	dropdown = pidgin_account_option_menu_new(NULL, FALSE,
												G_CALLBACK(select_account_cb), NULL, dialog);
	pidgin_add_widget_to_vbox(GTK_BOX(vbox), _("Set privacy for:"), NULL, dropdown, TRUE, NULL);
	dialog->account = pidgin_account_option_menu_get_selected(dropdown);

	/* Add the drop-down list with the allow/block types. */
	dialog->type_menu = gtk_combo_box_new_text();
	gtk_box_pack_start(GTK_BOX(vbox), dialog->type_menu, FALSE, FALSE, 0);
	gtk_widget_show(dialog->type_menu);

	for (i = 0; i < menu_entry_count; i++) {
		gtk_combo_box_append_text(GTK_COMBO_BOX(dialog->type_menu),
		                          _(menu_entries[i].text));

		if (menu_entries[i].num == dialog->account->perm_deny)
			selected = i;
	}

	gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->type_menu), selected);

	g_signal_connect(G_OBJECT(dialog->type_menu), "changed",
					 G_CALLBACK(type_changed_cb), dialog);

	/* Build the treeview for the allow list. */
	dialog->allow_widget = build_allow_list(dialog);
	gtk_box_pack_start(GTK_BOX(vbox), dialog->allow_widget, TRUE, TRUE, 0);

	/* Build the treeview for the block list. */
	dialog->block_widget = build_block_list(dialog);
	gtk_box_pack_start(GTK_BOX(vbox), dialog->block_widget, TRUE, TRUE, 0);

	/* Add the button box for Add, Remove, Remove All */
	dialog->button_box = pidgin_dialog_get_action_area(GTK_DIALOG(dialog->win));

	/* Add button */
	button = pidgin_dialog_add_button(GTK_DIALOG(dialog->win), GTK_STOCK_ADD, G_CALLBACK(add_cb), dialog);
	dialog->add_button = button;

	/* Remove button */
	button = pidgin_dialog_add_button(GTK_DIALOG(dialog->win), GTK_STOCK_REMOVE, G_CALLBACK(remove_cb), dialog);
	dialog->remove_button = button;
	/* TODO: This button should be sensitive/invisitive more cleverly */
	gtk_widget_set_sensitive(button, FALSE);

	/* Remove All button */
	button = pidgin_dialog_add_button(GTK_DIALOG(dialog->win), _("Remove Al_l"), G_CALLBACK(removeall_cb), dialog);
	dialog->removeall_button = button;

	/* Close button */
	button = pidgin_dialog_add_button(GTK_DIALOG(dialog->win), GTK_STOCK_CLOSE, G_CALLBACK(close_cb), dialog);
	dialog->close_button = button;

	type_changed_cb(GTK_COMBO_BOX(dialog->type_menu), dialog);
#if 0
	if (dialog->account->perm_deny == PURPLE_PRIVACY_ALLOW_USERS) {
		gtk_widget_show(dialog->allow_widget);
		gtk_widget_show(dialog->button_box);
		dialog->in_allow_list = TRUE;
	}
	else if (dialog->account->perm_deny == PURPLE_PRIVACY_DENY_USERS) {
		gtk_widget_show(dialog->block_widget);
		gtk_widget_show(dialog->button_box);
		dialog->in_allow_list = FALSE;
	}
#endif
	return dialog;
}

void
pidgin_privacy_dialog_show(void)
{
	g_return_if_fail(purple_connections_get_all() != NULL);

	if (privacy_dialog == NULL)
		privacy_dialog = privacy_dialog_new();

	gtk_widget_show(privacy_dialog->win);
	gdk_window_raise(privacy_dialog->win->window);
}

void
pidgin_privacy_dialog_hide(void)
{
	if (privacy_dialog == NULL)
		return;

	g_object_unref(G_OBJECT(privacy_dialog->allow_store));
	g_object_unref(G_OBJECT(privacy_dialog->block_store));
	g_free(privacy_dialog);
	privacy_dialog = NULL;
}

static void
destroy_request_data(PidginPrivacyRequestData *data)
{
	g_free(data->name);
	g_free(data);
}

static void
confirm_permit_block_cb(PidginPrivacyRequestData *data, int option)
{
	if (data->block)
		purple_privacy_deny(data->account, data->name, FALSE, FALSE);
	else
		purple_privacy_allow(data->account, data->name, FALSE, FALSE);

	destroy_request_data(data);
}

static void
add_permit_block_cb(PidginPrivacyRequestData *data, const char *name)
{
	data->name = g_strdup(name);

	confirm_permit_block_cb(data, 0);
}

void
pidgin_request_add_permit(PurpleAccount *account, const char *name)
{
	PidginPrivacyRequestData *data;

	g_return_if_fail(account != NULL);

	data = g_new0(PidginPrivacyRequestData, 1);
	data->account = account;
	data->name    = g_strdup(name);
	data->block   = FALSE;

	if (name == NULL) {
		purple_request_input(account, _("Permit User"),
			_("Type a user you permit to contact you."),
			_("Please enter the name of the user you wish to be "
			  "able to contact you."),
			NULL, FALSE, FALSE, NULL,
			_("_Permit"), G_CALLBACK(add_permit_block_cb),
			_("Cancel"), G_CALLBACK(destroy_request_data),
			account, name, NULL,
			data);
	}
	else {
		char *primary = g_strdup_printf(_("Allow %s to contact you?"), name);
		char *secondary =
			g_strdup_printf(_("Are you sure you wish to allow "
							  "%s to contact you?"), name);


		purple_request_action(account, _("Permit User"), primary, secondary,
							0,
							account, name, NULL,
							data, 2,
							_("_Permit"), G_CALLBACK(confirm_permit_block_cb),
							_("Cancel"), G_CALLBACK(destroy_request_data));

		g_free(primary);
		g_free(secondary);
	}
}

void
pidgin_request_add_block(PurpleAccount *account, const char *name)
{
	PidginPrivacyRequestData *data;

	g_return_if_fail(account != NULL);

	data = g_new0(PidginPrivacyRequestData, 1);
	data->account = account;
	data->name    = g_strdup(name);
	data->block   = TRUE;

	if (name == NULL) {
		purple_request_input(account, _("Block User"),
			_("Type a user to block."),
			_("Please enter the name of the user you wish to block."),
			NULL, FALSE, FALSE, NULL,
			_("_Block"), G_CALLBACK(add_permit_block_cb),
			_("Cancel"), G_CALLBACK(destroy_request_data),
			account, name, NULL,
			data);
	}
	else {
		char *primary = g_strdup_printf(_("Block %s?"), name);
		char *secondary =
			g_strdup_printf(_("Are you sure you want to block %s?"), name);

		purple_request_action(account, _("Block User"), primary, secondary,
							0,
							account, name, NULL,
							data, 2,
							_("_Block"), G_CALLBACK(confirm_permit_block_cb),
							_("Cancel"), G_CALLBACK(destroy_request_data));

		g_free(primary);
		g_free(secondary);
	}
}

static void
pidgin_permit_added_removed(PurpleAccount *account, const char *name)
{
	if (privacy_dialog != NULL)
		rebuild_allow_list(privacy_dialog);
}

static void
pidgin_deny_added_removed(PurpleAccount *account, const char *name)
{
	if (privacy_dialog != NULL)
		rebuild_block_list(privacy_dialog);
}

static PurplePrivacyUiOps privacy_ops =
{
	pidgin_permit_added_removed,
	pidgin_permit_added_removed,
	pidgin_deny_added_removed,
	pidgin_deny_added_removed,
	NULL,
	NULL,
	NULL,
	NULL
};

PurplePrivacyUiOps *
pidgin_privacy_get_ui_ops(void)
{
	return &privacy_ops;
}

void
pidgin_privacy_init(void)
{
}
