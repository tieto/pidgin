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
#include "pidgin.h"
#include "gtkutils.h"
#include "debug.h"
#include "disco.h"

#include "gtkdisco.h"

typedef struct _PidginDiscoDialog {
	GtkWidget *window;
	GtkWidget *account_widget;
	
	GtkWidget *sw;
	GtkWidget *progress;

	GtkWidget *stop_button;
	GtkWidget *list_button;
	GtkWidget *register_button;
	GtkWidget *add_button;
	GtkWidget *close_button;

	PurpleAccount *account;
	PurpleDiscoList *discolist;
} PidginDiscoDialog;

typedef struct _PidginDiscoList {
	PidginDiscoDialog *dialog;
	GtkTreeStore *model;
	GtkWidget *tree;
	GHashTable *cats; /** Meow. */
} PidginDiscoList;

struct _menu_cb_info {
	PurpleDiscoList *list;
	PurpleDiscoService *service;
};

enum {
	PIXBUF_COLUMN = 0,
	NAME_COLUMN,
	DESCRIPTION_COLUMN,
	SERVICE_COLUMN,
	NUM_OF_COLUMNS
};

static void pidgin_disco_create_tree(PidginDiscoList *pdl);

static void dialog_select_account_cb(GObject *w, PurpleAccount *account,
				     PidginDiscoDialog *dialog)
{
	dialog->account = account;
}

static void register_button_cb(GtkButton *button, PidginDiscoDialog *dialog)
{
	struct _menu_cb_info *info = g_object_get_data(G_OBJECT(button), "disco-info");
	PurpleAccount *account;
	PurpleConnection *gc;

	account = purple_disco_list_get_account(info->list);
	gc = purple_account_get_connection(account);

	purple_disco_service_register(info->service);
}

static void list_button_cb(GtkButton *button, PidginDiscoDialog *dialog)
{
	PurpleConnection *gc;
	PidginDiscoList *pdl;

	gc = purple_account_get_connection(dialog->account);
	if (!gc)
		return;

	if (dialog->discolist != NULL) {
		pdl = purple_disco_list_get_ui_data(dialog->discolist);
		gtk_widget_destroy(pdl->tree);
		purple_disco_list_unref(dialog->discolist);
	}

	dialog->discolist = purple_disco_get_list(gc);
	if (!dialog->discolist)
		return;
	/* We keep a copy... */
	purple_disco_list_ref(dialog->discolist);

	pdl = purple_disco_list_get_ui_data(dialog->discolist);
	pdl->dialog = dialog;

	pidgin_disco_create_tree(pdl);

	if (dialog->account_widget)
		gtk_widget_set_sensitive(dialog->account_widget, FALSE);
}

static void add_room_to_blist_cb(GtkButton *button, PidginDiscoDialog *dialog)
{
	struct _menu_cb_info *info = g_object_get_data(G_OBJECT(button), "disco-info");
	PurpleAccount *account;
	const char *name;

	g_return_if_fail(info != NULL);

	account = purple_disco_list_get_account(info->list);
	name = purple_disco_service_get_name(info->service);

	if (purple_disco_service_get_type(info->service) == PURPLE_DISCO_SERVICE_TYPE_CHAT)
		purple_blist_request_add_chat(account, NULL, NULL, name);
	else
		purple_blist_request_add_buddy(account, name, NULL, NULL);
}

static void
selection_changed_cb(GtkTreeSelection *selection, PidginDiscoList *pdl)
{
	PurpleDiscoService *service;
	PurpleDiscoServiceFlags flags;
	GtkTreeIter iter;
	GValue val;
	static struct _menu_cb_info *info;
	PidginDiscoDialog *dialog = pdl->dialog;

	if (gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		val.g_type = 0;
		gtk_tree_model_get_value(GTK_TREE_MODEL(pdl->model), &iter, SERVICE_COLUMN, &val);
		service = g_value_get_pointer(&val);
		if (!service) {
			gtk_widget_set_sensitive(dialog->add_button, FALSE);
			gtk_widget_set_sensitive(dialog->register_button, FALSE);
			return;
		}

		info = g_new0(struct _menu_cb_info, 1);
		info->list = dialog->discolist;
		info->service = service;

		g_object_set_data(G_OBJECT(dialog->add_button), "disco-info", info);
		g_object_set_data(G_OBJECT(dialog->register_button), "disco-info", info);

		flags = purple_disco_service_get_flags(service);

		gtk_widget_set_sensitive(dialog->add_button, flags & PURPLE_DISCO_ADD);
		gtk_widget_set_sensitive(dialog->register_button, flags & PURPLE_DISCO_REGISTER);
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
	PurpleDiscoService *service;
	GValue val;

	pdl = user_data;

	val.g_type = 0;
	gtk_tree_model_get_value(GTK_TREE_MODEL(pdl->model), arg1, SERVICE_COLUMN,
	                         &val);
	service = g_value_get_pointer(&val);
	purple_disco_service_expand(service);
}

static gint
delete_win_cb(GtkWidget *w, GdkEventAny *e, gpointer d)
{
	PidginDiscoDialog *dialog = d;

	if (dialog->discolist && purple_disco_list_get_in_progress(dialog->discolist))
		purple_disco_cancel_get_list(dialog->discolist);

	if (dialog->discolist) {
		PidginDiscoList *pdl = purple_disco_list_get_ui_data(dialog->discolist);

		if (pdl)
			pdl->dialog = NULL;
		purple_disco_list_unref(dialog->discolist);
	}

	g_free(dialog);

	return FALSE;
}

static void stop_button_cb(GtkButton *button, PidginDiscoDialog *dialog)
{
	purple_disco_cancel_get_list(dialog->discolist);

	if (dialog->account_widget)
		gtk_widget_set_sensitive(dialog->account_widget, TRUE);

	gtk_widget_set_sensitive(dialog->stop_button, FALSE);
	gtk_widget_set_sensitive(dialog->list_button, TRUE);
	gtk_widget_set_sensitive(dialog->add_button, FALSE);
}

static void close_button_cb(GtkButton *button, PidginDiscoDialog *dialog)
{
	GtkWidget *window = dialog->window;

	delete_win_cb(NULL, NULL, dialog);
	gtk_widget_destroy(window);
}

static gboolean account_filter_func(PurpleAccount *account)
{
	PurpleConnection *conn = purple_account_get_connection(account);
	PurplePluginProtocolInfo *prpl_info = NULL;

	if (conn)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(conn->prpl);

	return (prpl_info && PURPLE_PROTOCOL_PLUGIN_HAS_FUNC(prpl_info, disco_get_list));
}

gboolean
pidgin_disco_is_showable()
{
	GList *c;
	PurpleConnection *gc;

	for (c = purple_connections_get_all(); c != NULL; c = c->next) {
		gc = c->data;

		if (account_filter_func(purple_connection_get_account(gc)))
			return TRUE;
	}

	return FALSE;
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

	gtk_tree_view_column_pack_start(column,  pixbuf_renderer, FALSE);
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

	g_signal_connect(G_OBJECT(pdl->tree), "row-expanded", G_CALLBACK(row_expanded_cb), pdl);
}

static PidginDiscoDialog*
pidgin_disco_dialog_new_with_account(PurpleAccount *account)
{
	PidginDiscoDialog *dialog;
	GtkWidget *window, *vbox, *vbox2, *bbox;

	dialog = g_new0(PidginDiscoDialog, 1);
	dialog->account = account;

	/* Create the window. */
	dialog->window = window = pidgin_create_dialog(_("Service Discovery"), PIDGIN_HIG_BORDER, "service discovery", TRUE);

	g_signal_connect(G_OBJECT(window), "delete_event",
					 G_CALLBACK(delete_win_cb), dialog);

	/* Create the parent vbox for everything. */
	vbox = pidgin_dialog_get_vbox_with_properties(GTK_DIALOG(window), FALSE, PIDGIN_HIG_BORDER);

	vbox2 = gtk_vbox_new(FALSE, PIDGIN_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(vbox), vbox2);
	gtk_widget_show(vbox2);

	/* accounts dropdown list */
	dialog->account_widget = pidgin_account_option_menu_new(dialog->account, FALSE,
	                         G_CALLBACK(dialog_select_account_cb), account_filter_func, dialog);
	if (!dialog->account) /* this is normally null, and we normally don't care what the first selected item is */
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
	dialog->stop_button = pidgin_dialog_add_button(GTK_DIALOG(window), GTK_STOCK_STOP,
	                 G_CALLBACK(stop_button_cb), dialog);
	gtk_widget_set_sensitive(dialog->stop_button, FALSE);

	/* list button */
	dialog->list_button = pidgin_pixbuf_button_from_stock(_("_Get List"), GTK_STOCK_REFRESH,
	                                                    PIDGIN_BUTTON_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(bbox), dialog->list_button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(dialog->list_button), "clicked",
	                 G_CALLBACK(list_button_cb), dialog);
	gtk_widget_show(dialog->list_button);

	/* register button */
	dialog->register_button = pidgin_dialog_add_button(GTK_DIALOG(dialog->window), _("Register"),
	                 G_CALLBACK(register_button_cb), dialog);
	gtk_widget_set_sensitive(dialog->register_button, FALSE);

	/* add button */
	dialog->add_button = pidgin_pixbuf_button_from_stock(_("_Add"), GTK_STOCK_ADD,
	                                                    PIDGIN_BUTTON_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(bbox), dialog->add_button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(dialog->add_button), "clicked",
	                 G_CALLBACK(add_room_to_blist_cb), dialog);
	gtk_widget_set_sensitive(dialog->add_button, FALSE);
	gtk_widget_show(dialog->add_button);

	/* close button */
	dialog->close_button = pidgin_dialog_add_button(GTK_DIALOG(window), GTK_STOCK_CLOSE,
					 G_CALLBACK(close_button_cb), dialog);

	/* show the dialog window and return the dialog */
	gtk_widget_show(dialog->window);

	return dialog;
}

void
pidgin_disco_dialog_show(void)
{
	pidgin_disco_dialog_new_with_account(NULL);
}

void
pidgin_disco_dialog_show_with_account(PurpleAccount* account)
{
	PidginDiscoDialog *dialog = pidgin_disco_dialog_new_with_account(account);

	if (!dialog)
		return;

	list_button_cb(GTK_BUTTON(dialog->list_button), dialog);
}

static void
pidgin_disco_create(PurpleDiscoList *list)
{
	PidginDiscoList *pdl = g_new0(PidginDiscoList, 1);

	purple_disco_list_set_ui_data(list, pdl);

	pdl->cats = g_hash_table_new_full(NULL, NULL, NULL, (GDestroyNotify)gtk_tree_row_reference_free);
}


static void
pidgin_disco_destroy(PurpleDiscoList *list)
{
	PidginDiscoList *pdl;

	pdl = purple_disco_list_get_ui_data(list);

	g_hash_table_destroy(pdl->cats);
	g_free(pdl);

	purple_disco_list_set_ui_data(list, NULL);
}

static void pidgin_disco_in_progress(PurpleDiscoList *list, gboolean in_progress)
{
	PidginDiscoList *pdl;
	PidginDiscoDialog *dialog;
	
	pdl = purple_disco_list_get_ui_data(list);
	if (!pdl)
		return;

	dialog = pdl->dialog;
	if (!dialog)
		return;

	if (in_progress) {
		if (dialog->account_widget)
			gtk_widget_set_sensitive(dialog->account_widget, FALSE);
		gtk_widget_set_sensitive(dialog->stop_button, TRUE);
		gtk_widget_set_sensitive(dialog->list_button, FALSE);
	} else {
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(dialog->progress), 0.0);
		if (dialog->account_widget)
			gtk_widget_set_sensitive(dialog->account_widget, TRUE);
		gtk_widget_set_sensitive(dialog->stop_button, FALSE);
		gtk_widget_set_sensitive(dialog->list_button, TRUE);
	}
}

static void pidgin_disco_add_service(PurpleDiscoList *list, PurpleDiscoService *service, PurpleDiscoService *parent)
{
	PidginDiscoList *pdl;
	PidginDiscoDialog *dialog;
	PurpleDiscoServiceType type;
	const char *gateway_type;
	GtkTreeIter iter, parent_iter, child;
	char *filename = NULL;
	GdkPixbuf *pixbuf = NULL;
	gboolean append = TRUE;

	pdl = purple_disco_list_get_ui_data(list);
	dialog = pdl->dialog;

	purple_debug_info("disco", "Add_service \"%s\"\n", purple_disco_service_get_name(service));

	gtk_progress_bar_pulse(GTK_PROGRESS_BAR(dialog->progress));

	if (parent) {
		GtkTreeRowReference *rr;
		GtkTreePath *path;

		rr = g_hash_table_lookup(pdl->cats, parent);
		path = gtk_tree_row_reference_get_path(rr);
		if (path) {
			gtk_tree_model_get_iter(GTK_TREE_MODEL(pdl->model), &parent_iter, path);
			gtk_tree_path_free(path);

			if (gtk_tree_model_iter_children(GTK_TREE_MODEL(pdl->model), &child,
			                                 &parent_iter)) {
				PurpleDiscoList *tmp;
				gtk_tree_model_get(GTK_TREE_MODEL(pdl->model), &child,
				                   SERVICE_COLUMN, &tmp, -1);
				if (!tmp)
					append = FALSE;
			}
		}
	}

	if (append)
		gtk_tree_store_append(pdl->model, &iter, (parent ? &parent_iter : NULL));
	else
		iter = child;

	if (purple_disco_service_get_flags(service) & PURPLE_DISCO_BROWSE) {
		GtkTreeRowReference *rr;
		GtkTreePath *path;

		gtk_tree_store_append(pdl->model, &child, &iter);

		path = gtk_tree_model_get_path(GTK_TREE_MODEL(pdl->model), &iter);
		rr = gtk_tree_row_reference_new(GTK_TREE_MODEL(pdl->model), path);
		g_hash_table_insert(pdl->cats, service, rr);
		gtk_tree_path_free(path);
	}

	type = purple_disco_service_get_type(service);
	gateway_type = purple_disco_service_get_gateway_type(service);

	if (type == PURPLE_DISCO_SERVICE_TYPE_GATEWAY && gateway_type) {
		char *tmp = g_strconcat(gateway_type, ".png", NULL);
		filename = g_build_filename(DATADIR, "pixmaps", "pidgin", "protocols", "22", tmp, NULL);
		g_free(tmp);
#if 0
	} else if (type == PURPLE_DISCO_SERVICE_TYPE_USER) {
		filename = g_build_filename(DATADIR, "pixmaps", "pidgin", "status", "22", "person.png", NULL);
#endif
	} else if (type == PURPLE_DISCO_SERVICE_TYPE_CHAT)
		filename = g_build_filename(DATADIR, "pixmaps", "pidgin", "status", "22", "chat.png", NULL);

	if (filename) {
		pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
		g_free(filename);
	}

	gtk_tree_store_set(pdl->model, &iter,
			PIXBUF_COLUMN, pixbuf,
			NAME_COLUMN, purple_disco_service_get_name(service),
			DESCRIPTION_COLUMN, purple_disco_service_get_description(service),
			SERVICE_COLUMN, service,
			-1);

	if (pixbuf)
		g_object_unref(pixbuf);
}

static PurpleDiscoUiOps ops = {
	pidgin_disco_dialog_show_with_account,
	pidgin_disco_create,
	pidgin_disco_destroy,
	pidgin_disco_add_service,
	pidgin_disco_in_progress,
	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

void pidgin_disco_init() {
	purple_disco_set_ui_ops(&ops);
}
