/**
 * @file gtkdisco.c GTK+ Service Discovery UI
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
#include "debug.h"
#include "gtkutils.h"
#include "pidgin.h"
#include "request.h"
#include "pidgintooltip.h"

#include "gtkdisco.h"
#include "xmppdisco.h"

GList *dialogs = NULL;

enum {
	PIXBUF_COLUMN = 0,
	NAME_COLUMN,
	DESCRIPTION_COLUMN,
	SERVICE_COLUMN,
	NUM_OF_COLUMNS
};

static void
pidgin_disco_list_destroy(PidginDiscoList *list)
{
	g_hash_table_destroy(list->services);
	if (list->dialog && list->dialog->discolist == list)
		list->dialog->discolist = NULL;

	if (list->tree) {
		gtk_widget_destroy(list->tree);
		list->tree = NULL;
	}

	g_free((gchar*)list->server);
	g_free(list);
}

PidginDiscoList *pidgin_disco_list_ref(PidginDiscoList *list)
{
	g_return_val_if_fail(list != NULL, NULL);

	++list->ref;
	purple_debug_misc("xmppdisco", "reffing list, ref count now %d\n", list->ref);

	return list;
}

void pidgin_disco_list_unref(PidginDiscoList *list)
{
	g_return_if_fail(list != NULL);

	--list->ref;

	purple_debug_misc("xmppdisco", "unreffing list, ref count now %d\n", list->ref);
	if (list->ref == 0)
		pidgin_disco_list_destroy(list);
}

void pidgin_disco_list_set_in_progress(PidginDiscoList *list, gboolean in_progress)
{
	PidginDiscoDialog *dialog = list->dialog;

	if (!dialog)
		return;

	list->in_progress = in_progress;

	if (in_progress) {
		gtk_widget_set_sensitive(dialog->account_widget, FALSE);
		gtk_widget_set_sensitive(dialog->stop_button, TRUE);
		gtk_widget_set_sensitive(dialog->browse_button, FALSE);
	} else {
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(dialog->progress), 0.0);

		gtk_widget_set_sensitive(dialog->account_widget, TRUE);

		gtk_widget_set_sensitive(dialog->stop_button, FALSE);
		gtk_widget_set_sensitive(dialog->browse_button, TRUE);
/*
		gtk_widget_set_sensitive(dialog->register_button, FALSE);
		gtk_widget_set_sensitive(dialog->add_button, FALSE);
*/
	}
}

static GdkPixbuf *
pidgin_disco_load_icon(XmppDiscoService *service, const char *size)
{
	GdkPixbuf *pixbuf = NULL;
	char *filename = NULL;

	g_return_val_if_fail(service != NULL, NULL);
	g_return_val_if_fail(size != NULL, NULL);

	if (service->type == XMPP_DISCO_SERVICE_TYPE_GATEWAY && service->gateway_type) {
		char *tmp = g_strconcat(service->gateway_type, ".png", NULL);
		filename = g_build_filename(DATADIR, "pixmaps", "pidgin", "protocols", size, tmp, NULL);
		g_free(tmp);
#if 0
	} else if (service->type == XMPP_DISCO_SERVICE_TYPE_USER) {
		filename = g_build_filename(DATADIR, "pixmaps", "pidgin", "status", size, "person.png", NULL);
#endif
	} else if (service->type == XMPP_DISCO_SERVICE_TYPE_CHAT)
		filename = g_build_filename(DATADIR, "pixmaps", "pidgin", "status", size, "chat.png", NULL);

	if (filename) {
		pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
		g_free(filename);
	}

	return pixbuf;
}

static void pidgin_disco_create_tree(PidginDiscoList *pdl);

static void dialog_select_account_cb(GObject *w, PurpleAccount *account,
                                     PidginDiscoDialog *dialog)
{
	gboolean change = (account != dialog->account);
	dialog->account = account;
	gtk_widget_set_sensitive(dialog->browse_button, account != NULL);

	if (change && dialog->discolist) {
		if (dialog->discolist->tree) {
			gtk_widget_destroy(dialog->discolist->tree);
			dialog->discolist->tree = NULL;
		}
		pidgin_disco_list_unref(dialog->discolist);
		dialog->discolist = NULL;
	}
}

static void register_button_cb(GtkWidget *unused, PidginDiscoDialog *dialog)
{
	xmpp_disco_service_register(dialog->selected);
}

static void discolist_cancel_cb(PidginDiscoList *pdl, const char *server)
{
	pdl->dialog->prompt_handle = NULL;

	pidgin_disco_list_set_in_progress(pdl, FALSE);
	pidgin_disco_list_unref(pdl);
}

static void discolist_ok_cb(PidginDiscoList *pdl, const char *server)
{
	pdl->dialog->prompt_handle = NULL;
	gtk_widget_set_sensitive(pdl->dialog->browse_button, TRUE);

	if (!server || !*server) {
		purple_notify_error(my_plugin, _("Invalid Server"), _("Invalid Server"),
		                    NULL);

		pidgin_disco_list_set_in_progress(pdl, FALSE);
		pidgin_disco_list_unref(pdl);
		return;
	}

	pdl->server = g_strdup(server);
	pidgin_disco_list_set_in_progress(pdl, TRUE);
	xmpp_disco_start(pdl);
}

static void browse_button_cb(GtkWidget *button, PidginDiscoDialog *dialog)
{
	PurpleConnection *pc;
	PidginDiscoList *pdl;
	const char *username;
	const char *at, *slash;
	char *server = NULL;

	pc = purple_account_get_connection(dialog->account);
	if (!pc)
		return;

	gtk_widget_set_sensitive(dialog->browse_button, FALSE);
	gtk_widget_set_sensitive(dialog->add_button, FALSE);
	gtk_widget_set_sensitive(dialog->register_button, FALSE);

	if (dialog->discolist != NULL) {
		if (dialog->discolist->tree) {
			gtk_widget_destroy(dialog->discolist->tree);
			dialog->discolist->tree = NULL;
		}
		pidgin_disco_list_unref(dialog->discolist);
	}

	pdl = dialog->discolist = g_new0(PidginDiscoList, 1);
	pdl->services = g_hash_table_new_full(NULL, NULL, NULL,
			(GDestroyNotify)gtk_tree_row_reference_free);
	pdl->pc = pc;
	/* We keep a copy... */
	pidgin_disco_list_ref(pdl);

	pdl->dialog = dialog;
	pidgin_disco_create_tree(pdl);

	if (dialog->account_widget)
		gtk_widget_set_sensitive(dialog->account_widget, FALSE);

	username = purple_account_get_username(dialog->account);
	at = strchr(username, '@');
	slash = strchr(username, '/');
	if (at && !slash) {
		server = g_strdup_printf("%s", at + 1);
	} else if (at && slash && at + 1 < slash) {
		server = g_strdup_printf("%.*s", (int)(slash - (at + 1)), at + 1);
	}

	if (server == NULL)
		/* This shouldn't ever happen since the account is connected */
		server = g_strdup("jabber.org");

	/* Note to translators: The string "Enter an XMPP Server" is asking the
	   user to type the name of an XMPP server which will then be queried */
	dialog->prompt_handle = purple_request_input(my_plugin, _("Server name request"), _("Enter an XMPP Server"),
			_("Select an XMPP server to query"),
			server, FALSE, FALSE, NULL,
			_("Find Services"), PURPLE_CALLBACK(discolist_ok_cb),
			_("Cancel"), PURPLE_CALLBACK(discolist_cancel_cb),
			purple_connection_get_account(pc), NULL, NULL, pdl);

	g_free(server);
}

static void add_to_blist_cb(GtkWidget *unused, PidginDiscoDialog *dialog)
{
	XmppDiscoService *service = dialog->selected;
	PurpleAccount *account;
	const char *jid;

	g_return_if_fail(service != NULL);

	account = purple_connection_get_account(service->list->pc);
	jid = service->jid;

	if (service->type == XMPP_DISCO_SERVICE_TYPE_CHAT)
		purple_blist_request_add_chat(account, NULL, NULL, jid);
	else
		purple_blist_request_add_buddy(account, jid, NULL, NULL);
}

static gboolean
service_click_cb(GtkTreeView *tree, GdkEventButton *event, gpointer user_data)
{
	PidginDiscoList *pdl;
	XmppDiscoService *service;
	GtkWidget *menu;

	GtkTreePath *path;
	GtkTreeIter iter;
	GValue val;

	if (event->button != 3 || event->type != GDK_BUTTON_PRESS)
		return FALSE;

	pdl = user_data;

	/* Figure out what was clicked */
	if (!gtk_tree_view_get_path_at_pos(tree, event->x, event->y, &path,
		                               NULL, NULL, NULL))
		return FALSE;
	gtk_tree_model_get_iter(GTK_TREE_MODEL(pdl->model), &iter, path);
	gtk_tree_path_free(path);
	val.g_type = 0;
	gtk_tree_model_get_value(GTK_TREE_MODEL(pdl->model), &iter, SERVICE_COLUMN,
	                         &val);
	service = g_value_get_pointer(&val);

	if (!service)
		return FALSE;

	menu = gtk_menu_new();

	if (service->flags & XMPP_DISCO_ADD)
		pidgin_new_item_from_stock(menu, _("Add to Buddy List"), GTK_STOCK_ADD,
		                           G_CALLBACK(add_to_blist_cb), pdl->dialog,
		                           0, 0, NULL);

	if (service->flags & XMPP_DISCO_REGISTER) {
		GtkWidget *item = pidgin_new_item(menu, _("Register"));
		g_signal_connect(G_OBJECT(item), "activate",
		                 G_CALLBACK(register_button_cb), pdl->dialog);
	}

	gtk_widget_show_all(menu);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button,
	               event->time);
	return FALSE;
}

static void
selection_changed_cb(GtkTreeSelection *selection, PidginDiscoList *pdl)
{
	GtkTreeIter iter;
	GValue val;
	PidginDiscoDialog *dialog = pdl->dialog;

	if (gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		val.g_type = 0;
		gtk_tree_model_get_value(GTK_TREE_MODEL(pdl->model), &iter, SERVICE_COLUMN, &val);
		dialog->selected = g_value_get_pointer(&val);
		if (!dialog->selected) {
			gtk_widget_set_sensitive(dialog->add_button, FALSE);
			gtk_widget_set_sensitive(dialog->register_button, FALSE);
			return;
		}

		gtk_widget_set_sensitive(dialog->add_button, dialog->selected->flags & XMPP_DISCO_ADD);
		gtk_widget_set_sensitive(dialog->register_button, dialog->selected->flags & XMPP_DISCO_REGISTER);
	} else {
		gtk_widget_set_sensitive(dialog->add_button, FALSE);
		gtk_widget_set_sensitive(dialog->register_button, FALSE);
	}
}

static void
row_expanded_cb(GtkTreeView *tree, GtkTreeIter *arg1, GtkTreePath *rg2,
                gpointer user_data)
{
	PidginDiscoList *pdl;
	XmppDiscoService *service;
	GValue val;

	pdl = user_data;

	val.g_type = 0;
	gtk_tree_model_get_value(GTK_TREE_MODEL(pdl->model), arg1, SERVICE_COLUMN,
	                         &val);
	service = g_value_get_pointer(&val);
	xmpp_disco_service_expand(service);
}

static void
row_activated_cb(GtkTreeView       *tree_view,
                 GtkTreePath       *path,
                 GtkTreeViewColumn *column,
                 gpointer           user_data)
{
	PidginDiscoList *pdl = user_data;
	GtkTreeIter iter;
	XmppDiscoService *service;
	GValue val;

	if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(pdl->model), &iter, path))
		return;

	val.g_type = 0;
	gtk_tree_model_get_value(GTK_TREE_MODEL(pdl->model), &iter, SERVICE_COLUMN,
	                         &val);
	service = g_value_get_pointer(&val);

	if (service->flags & XMPP_DISCO_BROWSE)
		if (gtk_tree_view_row_expanded(GTK_TREE_VIEW(pdl->tree), path))
			gtk_tree_view_collapse_row(GTK_TREE_VIEW(pdl->tree), path);
		else
			gtk_tree_view_expand_row(GTK_TREE_VIEW(pdl->tree), path, FALSE);
	else if (service->flags & XMPP_DISCO_REGISTER)
		register_button_cb(NULL, pdl->dialog);
	else if (service->flags & XMPP_DISCO_ADD)
		add_to_blist_cb(NULL, pdl->dialog);
}

static void
destroy_win_cb(GtkWidget *window, gpointer d)
{
	PidginDiscoDialog *dialog = d;
	PidginDiscoList *list = dialog->discolist;

	if (dialog->prompt_handle)
		purple_request_close(PURPLE_REQUEST_INPUT, dialog->prompt_handle);

	if (list) {
		list->dialog = NULL;

		if (list->in_progress)
			list->in_progress = FALSE;

		pidgin_disco_list_unref(list);
	}

	dialogs = g_list_remove(dialogs, d);
	g_free(dialog);
}

static void stop_button_cb(GtkButton *button, PidginDiscoDialog *dialog)
{
	pidgin_disco_list_set_in_progress(dialog->discolist, FALSE);
}

static void close_button_cb(GtkButton *button, PidginDiscoDialog *dialog)
{
	GtkWidget *window = dialog->window;

	gtk_widget_destroy(window);
}

static gboolean account_filter_func(PurpleAccount *account)
{
	return purple_strequal(purple_account_get_protocol_id(account), XMPP_PLUGIN_ID);
}

static gboolean
disco_paint_tooltip(GtkWidget *tipwindow, gpointer data)
{
  cairo_t *cr = gdk_cairo_create(GDK_DRAWABLE(tipwindow));
	PangoLayout *layout = g_object_get_data(G_OBJECT(tipwindow), "tooltip-plugin");
	gtk_paint_layout(gtk_widget_get_style(tipwindow),
			cr,
			GTK_STATE_NORMAL, FALSE,
			tipwindow, "tooltip",
			6, 6, layout);
	return TRUE;
}

static gboolean
disco_create_tooltip(GtkWidget *tipwindow, GtkTreePath *path,
		gpointer data, int *w, int *h)
{
	PidginDiscoList *pdl = data;
	GtkTreeIter iter;
	PangoLayout *layout;
	int width, height;
	XmppDiscoService *service;
	GValue val;
	const char *type = NULL;
	char *markup, *jid, *name, *desc = NULL;

	if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(pdl->model), &iter, path))
		return FALSE;

	val.g_type = 0;
	gtk_tree_model_get_value(GTK_TREE_MODEL(pdl->model), &iter, SERVICE_COLUMN,
	                         &val);
	service = g_value_get_pointer(&val);

	switch (service->type) {
		case XMPP_DISCO_SERVICE_TYPE_UNSET:
			type = _("Unknown");
			break;

		case XMPP_DISCO_SERVICE_TYPE_GATEWAY:
			type = _("Gateway");
			break;

		case XMPP_DISCO_SERVICE_TYPE_DIRECTORY:
			type = _("Directory");
			break;

		case XMPP_DISCO_SERVICE_TYPE_CHAT:
			type = _("Chat");
			break;

		case XMPP_DISCO_SERVICE_TYPE_PUBSUB_COLLECTION:
			type = _("PubSub Collection");
			break;

		case XMPP_DISCO_SERVICE_TYPE_PUBSUB_LEAF:
			type = _("PubSub Leaf");
			break;

		case XMPP_DISCO_SERVICE_TYPE_OTHER:
			type = _("Other");
			break;
	}

	markup = g_strdup_printf("<span size='x-large' weight='bold'>%s</span>\n<b>%s:</b> %s%s%s",
	                         name = g_markup_escape_text(service->name, -1),
	                         type,
	                         jid = g_markup_escape_text(service->jid, -1),
	                         service->description ? _("\n<b>Description:</b> ") : "",
	                         service->description ? desc = g_markup_escape_text(service->description, -1) : "");

	layout = gtk_widget_create_pango_layout(tipwindow, NULL);
	pango_layout_set_markup(layout, markup, -1);
	pango_layout_set_wrap(layout, PANGO_WRAP_WORD);
	pango_layout_set_width(layout, 500000);
	pango_layout_get_size(layout, &width, &height);
	g_object_set_data_full(G_OBJECT(tipwindow), "tooltip-plugin", layout, g_object_unref);

	if (w)
		*w = PANGO_PIXELS(width) + 12;
	if (h)
		*h = PANGO_PIXELS(height) + 12;

	g_free(markup);
	g_free(jid);
	g_free(name);
	g_free(desc);

	return TRUE;
}

static void pidgin_disco_create_tree(PidginDiscoList *pdl)
{
	GtkCellRenderer *text_renderer, *pixbuf_renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;

	pdl->model = gtk_tree_store_new(NUM_OF_COLUMNS,
			GDK_TYPE_PIXBUF,	/* PIXBUF_COLUMN */
			G_TYPE_STRING,		/* NAME_COLUMN */
			G_TYPE_STRING,		/* DESCRIPTION_COLUMN */
			G_TYPE_POINTER		/* SERVICE_COLUMN */
	);

	pdl->tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(pdl->model));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(pdl->tree), TRUE);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(pdl->tree));
	g_signal_connect(G_OBJECT(selection), "changed",
					 G_CALLBACK(selection_changed_cb), pdl);

	g_object_unref(pdl->model);

	gtk_container_add(GTK_CONTAINER(pdl->dialog->sw), pdl->tree);
	gtk_widget_show(pdl->tree);

	text_renderer = gtk_cell_renderer_text_new();
	pixbuf_renderer = gtk_cell_renderer_pixbuf_new();

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Name"));

	gtk_tree_view_column_pack_start(column, pixbuf_renderer, FALSE);
	gtk_tree_view_column_set_attributes(column, pixbuf_renderer,
			"pixbuf", PIXBUF_COLUMN, NULL);

	gtk_tree_view_column_pack_start(column, text_renderer, TRUE);
	gtk_tree_view_column_set_attributes(column, text_renderer,
			"text", NAME_COLUMN, NULL);

	gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(column),
	                                GTK_TREE_VIEW_COLUMN_GROW_ONLY);
	gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), TRUE);
	gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(column), NAME_COLUMN);
	gtk_tree_view_column_set_reorderable(GTK_TREE_VIEW_COLUMN(column), TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(pdl->tree), column);

	column = gtk_tree_view_column_new_with_attributes(_("Description"), text_renderer,
				"text", DESCRIPTION_COLUMN, NULL);
	gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(column),
	                                GTK_TREE_VIEW_COLUMN_GROW_ONLY);
	gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), TRUE);
	gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(column), DESCRIPTION_COLUMN);
	gtk_tree_view_column_set_reorderable(GTK_TREE_VIEW_COLUMN(column), TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(pdl->tree), column);

	g_signal_connect(G_OBJECT(pdl->tree), "button-press-event", G_CALLBACK(service_click_cb), pdl);
	g_signal_connect(G_OBJECT(pdl->tree), "row-expanded", G_CALLBACK(row_expanded_cb), pdl);
	g_signal_connect(G_OBJECT(pdl->tree), "row-activated", G_CALLBACK(row_activated_cb), pdl);

	pidgin_tooltip_setup_for_treeview(pdl->tree, pdl,
	                                  disco_create_tooltip,
	                                  disco_paint_tooltip);
}

void pidgin_disco_signed_off_cb(PurpleConnection *pc)
{
	GList *node;

	for (node = dialogs; node; node = node->next) {
		PidginDiscoDialog *dialog = node->data;
		PidginDiscoList *list = dialog->discolist;

		if (list && list->pc == pc) {
			if (list->in_progress)
				pidgin_disco_list_set_in_progress(list, FALSE);

			if (list->tree) {
				gtk_widget_destroy(list->tree);
				list->tree = NULL;
			}

			pidgin_disco_list_unref(list);
			dialog->discolist = NULL;

			gtk_widget_set_sensitive(dialog->browse_button,
					pidgin_account_option_menu_get_selected(dialog->account_widget) != NULL);

			gtk_widget_set_sensitive(dialog->register_button, FALSE);
			gtk_widget_set_sensitive(dialog->add_button, FALSE);
		}
	}
}

void pidgin_disco_dialogs_destroy_all(void)
{
	while (dialogs) {
		PidginDiscoDialog *dialog = dialogs->data;

		gtk_widget_destroy(dialog->window);
		/* destroy_win_cb removes the dialog from the list */
	}
}

PidginDiscoDialog *pidgin_disco_dialog_new(void)
{
	PidginDiscoDialog *dialog;
	GtkWidget *window, *vbox, *vbox2, *bbox;

	dialog = g_new0(PidginDiscoDialog, 1);
	dialogs = g_list_prepend(dialogs, dialog);

	/* Create the window. */
	dialog->window = window = pidgin_create_dialog(_("Service Discovery"), PIDGIN_HIG_BORDER, "service discovery", TRUE);

	g_signal_connect(G_OBJECT(window), "destroy",
					 G_CALLBACK(destroy_win_cb), dialog);

	/* Create the parent vbox for everything. */
	vbox = pidgin_dialog_get_vbox_with_properties(GTK_DIALOG(window), FALSE, PIDGIN_HIG_BORDER);

	vbox2 = gtk_vbox_new(FALSE, PIDGIN_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(vbox), vbox2);
	gtk_widget_show(vbox2);

	/* accounts dropdown list */
	dialog->account_widget = pidgin_account_option_menu_new(NULL, FALSE,
	                         G_CALLBACK(dialog_select_account_cb), account_filter_func, dialog);
	dialog->account = pidgin_account_option_menu_get_selected(dialog->account_widget);
	pidgin_add_widget_to_vbox(GTK_BOX(vbox2), _("_Account:"), NULL, dialog->account_widget, TRUE, NULL);

	/* scrolled window */
	dialog->sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(dialog->sw),
	                                    GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(dialog->sw),
	                               GTK_POLICY_AUTOMATIC,
	                               GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(vbox2), dialog->sw, TRUE, TRUE, 0);
	gtk_widget_set_size_request(dialog->sw, -1, 250);
	gtk_widget_show(dialog->sw);

	/* progress bar */
	dialog->progress = gtk_progress_bar_new();
	gtk_progress_bar_set_pulse_step(GTK_PROGRESS_BAR(dialog->progress), 0.1);
	gtk_box_pack_start(GTK_BOX(vbox2), dialog->progress, FALSE, FALSE, 0);
	gtk_widget_show(dialog->progress);

	/* button box */
	bbox = pidgin_dialog_get_action_area(GTK_DIALOG(window));
	gtk_box_set_spacing(GTK_BOX(bbox), PIDGIN_HIG_BOX_SPACE);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);

	/* stop button */
	dialog->stop_button =
		pidgin_dialog_add_button(GTK_DIALOG(window), GTK_STOCK_STOP,
		                         G_CALLBACK(stop_button_cb), dialog);
	gtk_widget_set_sensitive(dialog->stop_button, FALSE);

	/* browse button */
	dialog->browse_button =
		pidgin_pixbuf_button_from_stock(_("_Browse"), GTK_STOCK_REFRESH,
		                                PIDGIN_BUTTON_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(bbox), dialog->browse_button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(dialog->browse_button), "clicked",
	                 G_CALLBACK(browse_button_cb), dialog);
	gtk_widget_set_sensitive(dialog->browse_button, dialog->account != NULL);
	gtk_widget_show(dialog->browse_button);

	/* register button */
	dialog->register_button =
		pidgin_dialog_add_button(GTK_DIALOG(dialog->window), _("Register"),
		                         G_CALLBACK(register_button_cb), dialog);
	gtk_widget_set_sensitive(dialog->register_button, FALSE);

	/* add button */
	dialog->add_button =
		pidgin_pixbuf_button_from_stock(_("_Add"), GTK_STOCK_ADD,
	                                    PIDGIN_BUTTON_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(bbox), dialog->add_button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(dialog->add_button), "clicked",
	                 G_CALLBACK(add_to_blist_cb), dialog);
	gtk_widget_set_sensitive(dialog->add_button, FALSE);
	gtk_widget_show(dialog->add_button);

	/* close button */
	dialog->close_button =
		pidgin_dialog_add_button(GTK_DIALOG(window), GTK_STOCK_CLOSE,
		                         G_CALLBACK(close_button_cb), dialog);

	/* show the dialog window and return the dialog */
	gtk_widget_show(dialog->window);

	return dialog;
}

void pidgin_disco_add_service(PidginDiscoList *pdl, XmppDiscoService *service, XmppDiscoService *parent)
{
	PidginDiscoDialog *dialog;
	GtkTreeIter iter, parent_iter, child;
	GdkPixbuf *pixbuf = NULL;
	gboolean append = TRUE;

	dialog = pdl->dialog;
	g_return_if_fail(dialog != NULL);

	if (service != NULL)
		purple_debug_info("xmppdisco", "Adding service \"%s\"\n", service->name);
	else
		purple_debug_info("xmppdisco", "Service \"%s\" has no childrens\n", parent->name);

	gtk_progress_bar_pulse(GTK_PROGRESS_BAR(dialog->progress));

	if (parent) {
		GtkTreeRowReference *rr;
		GtkTreePath *path;

		rr = g_hash_table_lookup(pdl->services, parent);
		path = gtk_tree_row_reference_get_path(rr);
		if (path) {
			gtk_tree_model_get_iter(GTK_TREE_MODEL(pdl->model), &parent_iter, path);
			gtk_tree_path_free(path);

			if (gtk_tree_model_iter_children(GTK_TREE_MODEL(pdl->model), &child,
			                                 &parent_iter)) {
				PidginDiscoList *tmp;
				gtk_tree_model_get(GTK_TREE_MODEL(pdl->model), &child,
				                   SERVICE_COLUMN, &tmp, -1);
				if (!tmp)
					append = FALSE;
			}
		}
	}

	if (service == NULL) {
		if (parent != NULL && !append)
			gtk_tree_store_remove(pdl->model, &child);
		return;
	}

	if (append)
		gtk_tree_store_append(pdl->model, &iter, (parent ? &parent_iter : NULL));
	else
		iter = child;

	if (service->flags & XMPP_DISCO_BROWSE) {
		GtkTreeRowReference *rr;
		GtkTreePath *path;

		gtk_tree_store_append(pdl->model, &child, &iter);

		path = gtk_tree_model_get_path(GTK_TREE_MODEL(pdl->model), &iter);
		rr = gtk_tree_row_reference_new(GTK_TREE_MODEL(pdl->model), path);
		g_hash_table_insert(pdl->services, service, rr);
		gtk_tree_path_free(path);
	}

	pixbuf = pidgin_disco_load_icon(service, "16");

	gtk_tree_store_set(pdl->model, &iter,
			PIXBUF_COLUMN, pixbuf,
			NAME_COLUMN, service->name,
			DESCRIPTION_COLUMN, service->description,
			SERVICE_COLUMN, service,
			-1);

	if (pixbuf)
		g_object_unref(pixbuf);
}

