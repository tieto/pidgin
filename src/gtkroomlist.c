/**
 * @file gtkroomlist.c Gtk Room List UI
 * @ingroup gtkui
 *
 * gaim
 *
 * Copyright (C) 2003, Timothy Ringenbach <omarvo@hotmail.com>
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
#include "gtkutils.h"
#include "stock.h"
#include "debug.h"
#include "account.h"
#include "connection.h"
#include "notify.h"

#include "gtkroomlist.h"

typedef struct _GaimGtkRoomlist {
	GaimGtkRoomlistDialog *dialog;
	GtkTreeStore *model;
	GtkWidget *tree;
	GHashTable *cats; /**< Meow. */
	gint num_rooms, total_rooms;
} GaimGtkRoomlist;

struct _GaimGtkRoomlistDialog {
	GtkWidget *window;
	GtkWidget *account_widget;
	GtkWidget *progress;
	GtkWidget *sw;

	GtkWidget *list_button;
	GtkWidget *stop_button;
	GtkWidget *close_button;

	GaimAccount *account;
	GaimRoomlist *roomlist;

};

enum {
	NAME_COLUMN = 0,
	ROOM_COLUMN,
	NUM_OF_COLUMNS,
};

static GList *roomlists = NULL;



static gint delete_win_cb(GtkWidget *w, GdkEventAny *e, gpointer d)
{
	GaimGtkRoomlistDialog *dialog;

	dialog = (GaimGtkRoomlistDialog *) d;

	/* free stuff here */
	if (dialog->roomlist)
		gaim_roomlist_unref(dialog->roomlist);
	g_free(dialog);

	return FALSE;
}

static void dialog_select_account_cb(GObject *w, GaimAccount *account,
                                     GaimGtkRoomlistDialog *dialog)
{
	dialog->account = account;
}

static void list_button_cb(GtkButton *button, GaimGtkRoomlistDialog *dialog)
{
	GaimConnection *gc;
	GaimGtkRoomlist *rl;

	gc = gaim_account_get_connection(dialog->account);
	if (!gc)
		return;

	dialog->roomlist = gaim_roomlist_get_list(gc);
	gaim_roomlist_ref(dialog->roomlist);
	rl = dialog->roomlist->ui_data;
	rl->dialog = dialog;
	if (dialog->account_widget)
		gtk_widget_set_sensitive(dialog->account_widget, FALSE);
	gtk_widget_set_sensitive(dialog->list_button, FALSE);
	gtk_container_add(GTK_CONTAINER(dialog->sw), rl->tree); /* XXX */
}

static void stop_button_cb(GtkButton *button, GaimGtkRoomlistDialog *dialog)
{
	gaim_roomlist_cancel_get_list(dialog->roomlist);
	gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
}

static void close_button_cb(GtkButton *button, GaimGtkRoomlistDialog *dialog)
{
	GtkWidget *window = dialog->window;

	delete_win_cb(NULL, NULL, dialog);
	gtk_widget_destroy(window);
}

struct _menu_cb_info {
	GaimRoomlist *list;
	GaimRoomlistRoom *room;
};

static void do_join_cb(GtkWidget *w, struct _menu_cb_info *info)
{
	GHashTable *components;
	GList *l, *j;
	GaimConnection *gc;

	gc = gaim_account_get_connection(info->list->account);
	if (!gc)
		return;

	components = g_hash_table_new(g_str_hash, g_str_equal);

	g_hash_table_replace(components, g_strdup("name"), g_strdup(info->room->name));
	for (l = info->list->fields, j = info->room->fields; l && j; l = l->next, j = j->next) {
		GaimRoomlistField *f = l->data;

		g_hash_table_replace(components, f->name, j->data);
	}

	serv_join_chat(gc, components);

	g_hash_table_destroy(components);
}

static void row_activated_cb(GtkTreeView *tv, GtkTreePath *path, GtkTreeViewColumn *arg2,
                      GaimRoomlist *list)
{
	GaimGtkRoomlist *grl = list->ui_data;
	GtkTreeIter iter;
	GaimRoomlistRoom *room;
	GValue val = { 0, };
	struct _menu_cb_info info;

	gtk_tree_model_get_iter(GTK_TREE_MODEL(grl->model), &iter, path);
	gtk_tree_model_get_value(GTK_TREE_MODEL(grl->model), &iter, ROOM_COLUMN, &val);
	room = g_value_get_pointer(&val);
	if (!room || !(room->type & GAIM_ROOMLIST_ROOMTYPE_ROOM))
		return;

	info.list = list;
	info.room = room;

	do_join_cb(GTK_WIDGET(tv), &info);
}

static gboolean room_click_cb(GtkWidget *tv, GdkEventButton *event, GaimRoomlist *list)
{
	GtkTreePath *path;
	GaimGtkRoomlist *grl = list->ui_data;
	GValue val = { 0, };
	GaimRoomlistRoom *room;
	GtkTreeIter iter;
	GtkWidget *menu;
	static struct _menu_cb_info info; /* XXX? */

	if (event->button != 3 || event->type != GDK_BUTTON_PRESS)
		return FALSE;

	/* Here we figure out which room was clicked */
	if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(tv), event->x, event->y, &path, NULL, NULL, NULL))
		return FALSE;
	gtk_tree_model_get_iter(GTK_TREE_MODEL(grl->model), &iter, path);
	gtk_tree_path_free(path);
	gtk_tree_model_get_value (GTK_TREE_MODEL(grl->model), &iter, ROOM_COLUMN, &val);
	room = g_value_get_pointer(&val);

	if (!room || !(room->type & GAIM_ROOMLIST_ROOMTYPE_ROOM))
		return FALSE;

	info.list = list;
	info.room = room;


	menu = gtk_menu_new();
	gaim_new_item_from_stock(menu, _("_Join"), GAIM_STOCK_CHAT,
		                         G_CALLBACK(do_join_cb), &info, 0, 0, NULL);


	gtk_widget_show_all(menu);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 3, event->time);

	return FALSE;
}

static void row_expanded_cb(GtkTreeView *treeview, GtkTreeIter *arg1, GtkTreePath *arg2, gpointer user_data)
{
	GaimRoomlist *list = user_data;
	GaimRoomlistRoom *catagory;
	GValue val = { 0, };

	gtk_tree_model_get_value(gtk_tree_view_get_model(treeview), arg1, ROOM_COLUMN, &val);
	catagory = g_value_get_pointer(&val);

	if (!catagory->expanded_once) {
		gaim_roomlist_expand_catagory(list, catagory);
		catagory->expanded_once = TRUE;
	}
}

static gboolean accounts_filter_func(GaimAccount *account)
{
	GaimConnection *gc;

	gc = gaim_account_get_connection(account);
	if (!gc)
		return FALSE;
	return gaim_roomlist_is_possible(gc);
}

GaimGtkRoomlistDialog *gaim_gtk_roomlist_dialog_new_with_account(GaimAccount *account)
{
	GaimGtkRoomlistDialog *dialog;
	GtkWidget *window;
	GtkWidget *vbox1, *vbox2;
	GtkWidget *account_hbox;
	GtkWidget *bbox;
	GtkWidget *label;
	GtkWidget *button;
	GaimAccount *first_account = NULL;

	if (!account) {
		GList *c;
		GaimConnection *gc;

		for (c = gaim_connections_get_all(); c != NULL; c = c->next) {
			gc = c->data;

			if (gaim_roomlist_is_possible(gc)) {
				first_account = gaim_connection_get_account(gc);
				break;
			}
		}

		if (first_account == NULL) {
			gaim_notify_error(NULL, NULL,
			                  _("You are not currently signed on with any "
		                            "protocols that have the ability to list rooms."),
			                     NULL);

			return NULL;
		}
	}

	dialog = g_new0(GaimGtkRoomlistDialog, 1);

	/* Create the window. */
	dialog->window = window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_role(GTK_WINDOW(window), "room list");
	gtk_window_set_title(GTK_WINDOW(window), _("Room List"));

	gtk_container_set_border_width(GTK_CONTAINER(window), 12);
	gtk_widget_realize(window);

	g_signal_connect(G_OBJECT(window), "delete_event",
					 G_CALLBACK(delete_win_cb), dialog);

	/* Create the parent vbox for everything. */
	vbox1 = gtk_vbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(window), vbox1);
	gtk_widget_show(vbox1);

	/* Create the main vbox for top half of the window. */
	vbox2 = gtk_vbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(vbox1), vbox2, FALSE, FALSE, 0);
	gtk_widget_show(vbox2);

	account_hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), account_hbox, TRUE, TRUE, 0);
	gtk_widget_show(account_hbox);

	/* accounts dropdown list */
	if (!account) {
		dialog->account = first_account;
		label = gtk_label_new(NULL);
		gtk_box_pack_start(GTK_BOX(account_hbox), label, TRUE, TRUE, 0);
		gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), _("_Account:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);


		dialog->account_widget = gaim_gtk_account_option_menu_new(first_account, FALSE,
				G_CALLBACK(dialog_select_account_cb), accounts_filter_func, dialog);

		gtk_box_pack_start(GTK_BOX(account_hbox), dialog->account_widget, TRUE, TRUE, 0);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), GTK_WIDGET(dialog->account_widget));
		gtk_widget_show(label);
		gtk_widget_show(dialog->account_widget);
	} else {
		dialog->account = account;
	}


	/* Now the button box for the buttons */
	bbox = gtk_hbutton_box_new();
	gtk_box_set_spacing(GTK_BOX(bbox), 6);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_EDGE);
	gtk_box_pack_start(GTK_BOX(vbox2), bbox, FALSE, TRUE, 0);
	gtk_widget_show(bbox);

	/* Get list button */
	button = gtk_button_new_with_mnemonic(_("Get _list"));
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);
	dialog->list_button = button;

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(list_button_cb), dialog);
	/* Stop button */
	button = gtk_button_new_from_stock(GTK_STOCK_STOP);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);
	gtk_widget_set_sensitive(button, FALSE);
	dialog->stop_button = button;

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(stop_button_cb), dialog);
	/* Close button */
	button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);
	dialog->close_button = button;

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(close_button_cb), dialog);

	/* The pusling dilly */
	dialog->progress = gtk_progress_bar_new();
	gtk_progress_bar_set_pulse_step(GTK_PROGRESS_BAR(dialog->progress), 0.1);
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(dialog->progress),
		                          " ");
	gtk_box_pack_start(GTK_BOX(vbox2), dialog->progress, TRUE, TRUE, 0);
	gtk_widget_show(dialog->progress);


	gtk_widget_show(dialog->window);

	dialog->sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(dialog->sw),
	                                    GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(dialog->sw),
	                               GTK_POLICY_AUTOMATIC,
	                               GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(vbox1), dialog->sw, TRUE, TRUE, 0);
	gtk_widget_show(dialog->sw);

	return dialog;
}

GaimGtkRoomlistDialog *gaim_gtk_roomlist_dialog_new(void)
{
	return gaim_gtk_roomlist_dialog_new_with_account(NULL);
}

void gaim_gtk_roomlist_dialog_show(void)
{
	gaim_gtk_roomlist_dialog_new();
}

static void gaim_gtk_roomlist_new(GaimRoomlist *list)
{
	GaimGtkRoomlist *rl;

	rl = g_new0(GaimGtkRoomlist, 1);

	list->ui_data = rl;

	rl->cats = g_hash_table_new_full(NULL, NULL, NULL, (GDestroyNotify)gtk_tree_row_reference_free);

	roomlists = g_list_append(roomlists, list);
}

static void int_cell_data_func(GtkTreeViewColumn *col, GtkCellRenderer *renderer,
                                   GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
	gchar buf[16];
	int myint;

	gtk_tree_model_get(model, iter, GPOINTER_TO_INT(user_data), &myint, -1);

	if (myint)
		g_snprintf(buf, sizeof(buf), "%d", myint);
	else
		buf[0] = '\0';

	g_object_set(renderer, "text", buf, NULL);
}

/* this sorts backwards on purpose, so that clicking name sorts a-z, while clicking users sorts
   infinity-0. you can still click again to reverse it on any of them. */
static gint int_sort_func(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data)
{
	int c, d;

	c = d = 0;

	gtk_tree_model_get(model, a, GPOINTER_TO_INT(user_data), &c, -1);
	gtk_tree_model_get(model, b, GPOINTER_TO_INT(user_data), &d, -1);

	if (c == d)
		return 0;
	else if (c > d)
		return -1;
	else
		return 1;
}

static void gaim_gtk_roomlist_set_fields(GaimRoomlist *list, GList *fields)
{
	GaimGtkRoomlist *grl = list->ui_data;
	gint columns = NUM_OF_COLUMNS;
	int j;
	GtkTreeStore *model;
	GtkWidget *tree;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GList *l;
	GType *types;

	g_return_if_fail(grl != NULL);

	columns += g_list_length(fields);
	types = g_new(GType, columns);

	types[NAME_COLUMN] = G_TYPE_STRING;
	types[ROOM_COLUMN] = G_TYPE_POINTER;

	for (j = NUM_OF_COLUMNS, l = fields; l; l = l->next, j++) {
		GaimRoomlistField *f = l->data;

		switch (f->type) {
		case GAIM_ROOMLIST_FIELD_BOOL:
			types[j] = G_TYPE_BOOLEAN;
			break;
		case GAIM_ROOMLIST_FIELD_INT:
			types[j] = G_TYPE_INT;
			break;
		case GAIM_ROOMLIST_FIELD_STRING:
			types[j] = G_TYPE_STRING;
			break;
		}
	}

	model = gtk_tree_store_newv(columns, types);
	g_free(types);

	tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(model));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree), TRUE);

	g_object_unref(model);

	grl->model = model;
	grl->tree = tree;
	gtk_widget_show(grl->tree);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Name"), renderer,
				"text", NAME_COLUMN, NULL);
	gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(column),
	                                GTK_TREE_VIEW_COLUMN_GROW_ONLY);
	gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), TRUE);
	gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(column), NAME_COLUMN);
	gtk_tree_view_column_set_reorderable(GTK_TREE_VIEW_COLUMN(column), TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	for (j = NUM_OF_COLUMNS, l = fields; l; l = l->next, j++) {
		GaimRoomlistField *f = l->data;

		if (f->hidden)
			continue;

		renderer = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes(f->label, renderer,
		                                                  "text", j, NULL);
		gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(column),
		                                GTK_TREE_VIEW_COLUMN_GROW_ONLY);
		gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), TRUE);
		gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(column), j);
		gtk_tree_view_column_set_reorderable(GTK_TREE_VIEW_COLUMN(column), TRUE);
		if (f->type == GAIM_ROOMLIST_FIELD_INT) {
			gtk_tree_view_column_set_cell_data_func(column, renderer, int_cell_data_func,
			                                        GINT_TO_POINTER(j), NULL);
			gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(model), j, int_sort_func,
			                                GINT_TO_POINTER(j), NULL);
		}
		gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
	}

	g_signal_connect(G_OBJECT(tree), "button-press-event", G_CALLBACK(room_click_cb), list);
	g_signal_connect(G_OBJECT(tree), "row-expanded", G_CALLBACK(row_expanded_cb), list);
	g_signal_connect(G_OBJECT(tree), "row-activated", G_CALLBACK(row_activated_cb), list);
	/* gtk_container_add(GTK_CONTAINER(grl->sw), tree); */
}

static void gaim_gtk_roomlist_add_room(GaimRoomlist *list, GaimRoomlistRoom *room)
{
	GaimGtkRoomlist *rl= list->ui_data;
	GtkTreeRowReference *rr, *parentrr = NULL;
	GtkTreePath *path;
	GtkTreeIter iter, parent, child;
	GList *l, *k;
	int j;
	gboolean append = TRUE;

	rl->total_rooms++;
	if (room->type == GAIM_ROOMLIST_ROOMTYPE_ROOM)
		rl->num_rooms++;

	if (rl->dialog) {
		if (rl->total_rooms > 100)
			gtk_progress_bar_set_pulse_step(GTK_PROGRESS_BAR(rl->dialog->progress),
			                                0.01);
		gtk_progress_bar_pulse(GTK_PROGRESS_BAR(rl->dialog->progress));
	}
	if (room->parent) {
		parentrr = g_hash_table_lookup(rl->cats, room->parent);
		path = gtk_tree_row_reference_get_path(parentrr);
		if (path) {
			GaimRoomlistRoom *tmproom = NULL;

			gtk_tree_model_get_iter(GTK_TREE_MODEL(rl->model), &parent, path);
			gtk_tree_path_free(path);

			if (gtk_tree_model_iter_children(GTK_TREE_MODEL(rl->model), &child, &parent)) {
				gtk_tree_model_get(GTK_TREE_MODEL(rl->model), &child, ROOM_COLUMN, &tmproom, -1);
				if (!tmproom)
					append = FALSE;
			}
		}
	}

	if (append)
		gtk_tree_store_append(rl->model, &iter, (parentrr ? &parent : NULL));
	else
		iter = child;

	if (room->type & GAIM_ROOMLIST_ROOMTYPE_CATAGORY)
		gtk_tree_store_append(rl->model, &child, &iter);

	path = gtk_tree_model_get_path(GTK_TREE_MODEL(rl->model), &iter);

	if (room->type & GAIM_ROOMLIST_ROOMTYPE_CATAGORY) {
		rr = gtk_tree_row_reference_new(GTK_TREE_MODEL(rl->model), path);
		g_hash_table_insert(rl->cats, room, rr);
	}

	gtk_tree_path_free(path);

	gtk_tree_store_set(rl->model, &iter, NAME_COLUMN, room->name, -1);
	gtk_tree_store_set(rl->model, &iter, ROOM_COLUMN, room, -1);

	for (j = NUM_OF_COLUMNS, l = room->fields, k = list->fields; l && k; j++, l = l->next, k = k->next) {
		GaimRoomlistField *f = k->data;
		if (f->hidden)
			continue;
		gtk_tree_store_set(rl->model, &iter, j, l->data, -1);
	}
}

static void gaim_gtk_roomlist_in_progress(GaimRoomlist *list, gboolean flag)
{
	GaimGtkRoomlist *rl = list->ui_data;

	if (!rl || !rl->dialog)
		return;

	gtk_widget_set_sensitive(rl->dialog->stop_button, flag);
	if (flag) {
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(rl->dialog->progress),
		                          _("Downloading List..."));
	} else {
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(rl->dialog->progress),
		                          " ");
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(rl->dialog->progress), 0.0);
	}
}

static void gaim_gtk_roomlist_destroy(GaimRoomlist *list)
{
	GaimGtkRoomlist *rl;

	roomlists = g_list_remove(roomlists, list);

	rl = list->ui_data;

	g_return_if_fail(rl != NULL);

	g_hash_table_destroy(rl->cats);
	g_free(rl);
	list->ui_data = NULL;
}

static GaimRoomlistUiOps ops = {
	gaim_gtk_roomlist_new,
	gaim_gtk_roomlist_set_fields,
	gaim_gtk_roomlist_add_room,
	gaim_gtk_roomlist_in_progress,
	gaim_gtk_roomlist_destroy
};


void gaim_gtk_roomlist_init(void)
{
	gaim_roomlist_set_ui_ops(&ops);
}
