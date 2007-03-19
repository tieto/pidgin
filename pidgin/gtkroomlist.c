/**
 * @file gtkroomlist.c GTK+ Room List UI
 * @ingroup gtkui
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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

#include "internal.h"
#include "pidgin.h"
#include "gtkutils.h"
#include "pidginstock.h"
#include "debug.h"
#include "account.h"
#include "connection.h"
#include "notify.h"

#include "gtkroomlist.h"

typedef struct _PidginRoomlistDialog {
	GtkWidget *window;
	GtkWidget *account_widget;
	GtkWidget *progress;
	GtkWidget *sw;

	GtkWidget *stop_button;
	GtkWidget *list_button;
	GtkWidget *add_button;
	GtkWidget *join_button;
	GtkWidget *close_button;

	GaimAccount *account;
	GaimRoomlist *roomlist;

	gboolean pg_needs_pulse;
	gboolean pg_to_active;
	guint pg_update_to;
} PidginRoomlistDialog;

typedef struct _PidginRoomlist {
	PidginRoomlistDialog *dialog;
	GtkTreeStore *model;
	GtkWidget *tree;
	GHashTable *cats; /**< Meow. */
	gint num_rooms, total_rooms;
} PidginRoomlist;

enum {
	NAME_COLUMN = 0,
	ROOM_COLUMN,
	NUM_OF_COLUMNS,
};

static GList *roomlists = NULL;

static gint delete_win_cb(GtkWidget *w, GdkEventAny *e, gpointer d)
{
	PidginRoomlistDialog *dialog;

	dialog = (PidginRoomlistDialog *) d;

	if (dialog->roomlist && gaim_roomlist_get_in_progress(dialog->roomlist))
		gaim_roomlist_cancel_get_list(dialog->roomlist);

	if (dialog->roomlist) {
		if (dialog->pg_to_active) {
			gaim_timeout_remove(dialog->pg_update_to);
			dialog->pg_to_active = FALSE;
			/* yes, that's right, unref it twice. */
			gaim_roomlist_unref(dialog->roomlist);
		}
	}

	/* free stuff here */
	if (dialog->roomlist)
		gaim_roomlist_unref(dialog->roomlist);
	g_free(dialog);

	return FALSE;
}

static void dialog_select_account_cb(GObject *w, GaimAccount *account,
                                     PidginRoomlistDialog *dialog)
{
	dialog->account = account;
}

static void list_button_cb(GtkButton *button, PidginRoomlistDialog *dialog)
{
	GaimConnection *gc;
	PidginRoomlist *rl;

	gc = gaim_account_get_connection(dialog->account);
	if (!gc)
		return;

	if (dialog->roomlist != NULL) {
		rl = dialog->roomlist->ui_data;
		gtk_widget_destroy(rl->tree);
		gaim_roomlist_unref(dialog->roomlist);
	}

	dialog->roomlist = gaim_roomlist_get_list(gc);
	if (!dialog->roomlist)
		return;
	gaim_roomlist_ref(dialog->roomlist);
	rl = dialog->roomlist->ui_data;
	rl->dialog = dialog;

	if (dialog->account_widget)
		gtk_widget_set_sensitive(dialog->account_widget, FALSE);

	gtk_container_add(GTK_CONTAINER(dialog->sw), rl->tree);

	/* some protocols (not bundled with libgaim) finish getting their
	 * room list immediately */
	if(gaim_roomlist_get_in_progress(dialog->roomlist)) {
		gtk_widget_set_sensitive(dialog->stop_button, TRUE);
		gtk_widget_set_sensitive(dialog->list_button, FALSE);
	} else {
		gtk_widget_set_sensitive(dialog->stop_button, FALSE);
		gtk_widget_set_sensitive(dialog->list_button, TRUE);
	}
	gtk_widget_set_sensitive(dialog->add_button, FALSE);
	gtk_widget_set_sensitive(dialog->join_button, FALSE);
}

static void stop_button_cb(GtkButton *button, PidginRoomlistDialog *dialog)
{
	gaim_roomlist_cancel_get_list(dialog->roomlist);

	if (dialog->account_widget)
		gtk_widget_set_sensitive(dialog->account_widget, TRUE);

	gtk_widget_set_sensitive(dialog->stop_button, FALSE);
	gtk_widget_set_sensitive(dialog->list_button, TRUE);
	gtk_widget_set_sensitive(dialog->add_button, FALSE);
	gtk_widget_set_sensitive(dialog->join_button, FALSE);
}

static void close_button_cb(GtkButton *button, PidginRoomlistDialog *dialog)
{
	GtkWidget *window = dialog->window;

	delete_win_cb(NULL, NULL, dialog);
	gtk_widget_destroy(window);
}

struct _menu_cb_info {
	GaimRoomlist *list;
	GaimRoomlistRoom *room;
};

static void
selection_changed_cb(GtkTreeSelection *selection, PidginRoomlist *grl) {
	GtkTreeIter iter;
	GValue val;
	GaimRoomlistRoom *room;
	static struct _menu_cb_info *info;
	PidginRoomlistDialog *dialog;

	dialog = grl->dialog;

	if (gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		val.g_type = 0;
		gtk_tree_model_get_value(GTK_TREE_MODEL(grl->model), &iter, ROOM_COLUMN, &val);
		room = g_value_get_pointer(&val);
		if (!room || !(room->type & GAIM_ROOMLIST_ROOMTYPE_ROOM)) {
			gtk_widget_set_sensitive(dialog->join_button, FALSE);
			gtk_widget_set_sensitive(dialog->add_button, FALSE);
			return;
		}

		info = g_new0(struct _menu_cb_info, 1);
		info->list = dialog->roomlist;
		info->room = room;

		g_object_set_data_full(G_OBJECT(dialog->join_button), "room-info",
							   info, g_free);
		g_object_set_data(G_OBJECT(dialog->add_button), "room-info", info);

		gtk_widget_set_sensitive(dialog->add_button, TRUE);
		gtk_widget_set_sensitive(dialog->join_button, TRUE);
	} else {
		gtk_widget_set_sensitive(dialog->add_button, FALSE);
		gtk_widget_set_sensitive(dialog->join_button, FALSE);
	}
}

static void do_add_room_cb(GtkWidget *w, struct _menu_cb_info *info)
{
	char *name;
	GaimConnection *gc = gaim_account_get_connection(info->list->account);
	GaimPluginProtocolInfo *prpl_info = NULL;

	if(gc != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if(prpl_info != NULL && prpl_info->roomlist_room_serialize)
		name = prpl_info->roomlist_room_serialize(info->room);
	else
		name = g_strdup(info->room->name);

	gaim_blist_request_add_chat(info->list->account, NULL, NULL, name);

	g_free(name);
}

static void add_room_to_blist_cb(GtkButton *button, PidginRoomlistDialog *dialog)
{
	GaimRoomlist *rl = dialog->roomlist;
	PidginRoomlist *grl = rl->ui_data;
	struct _menu_cb_info *info;

	info = (struct _menu_cb_info*)g_object_get_data(G_OBJECT(button), "room-info");

	if(info != NULL)
		do_add_room_cb(grl->tree, info);
}

static void do_join_cb(GtkWidget *w, struct _menu_cb_info *info)
{
	gaim_roomlist_room_join(info->list, info->room);
}

static void join_button_cb(GtkButton *button, PidginRoomlistDialog *dialog)
{
	GaimRoomlist *rl = dialog->roomlist;
	PidginRoomlist *grl = rl->ui_data;
	struct _menu_cb_info *info;

	info = (struct _menu_cb_info*)g_object_get_data(G_OBJECT(button), "room-info");

	if(info != NULL)
		do_join_cb(grl->tree, info);
}

static void row_activated_cb(GtkTreeView *tv, GtkTreePath *path, GtkTreeViewColumn *arg2,
                      GaimRoomlist *list)
{
	PidginRoomlist *grl = list->ui_data;
	GtkTreeIter iter;
	GaimRoomlistRoom *room;
	GValue val;
	struct _menu_cb_info info;

	gtk_tree_model_get_iter(GTK_TREE_MODEL(grl->model), &iter, path);
	val.g_type = 0;
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
	PidginRoomlist *grl = list->ui_data;
	GValue val;
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
	val.g_type = 0;
	gtk_tree_model_get_value (GTK_TREE_MODEL(grl->model), &iter, ROOM_COLUMN, &val);
	room = g_value_get_pointer(&val);

	if (!room || !(room->type & GAIM_ROOMLIST_ROOMTYPE_ROOM))
		return FALSE;

	info.list = list;
	info.room = room;

	menu = gtk_menu_new();
	pidgin_new_item_from_stock(menu, _("_Join"), PIDGIN_STOCK_CHAT,
		                         G_CALLBACK(do_join_cb), &info, 0, 0, NULL);
	pidgin_new_item_from_stock(menu, _("_Add"), GTK_STOCK_ADD,
		                         G_CALLBACK(do_add_room_cb), &info, 0, 0, NULL);

	gtk_widget_show_all(menu);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 3, event->time);

	return FALSE;
}

static void row_expanded_cb(GtkTreeView *treeview, GtkTreeIter *arg1, GtkTreePath *arg2, gpointer user_data)
{
	GaimRoomlist *list = user_data;
	GaimRoomlistRoom *category;
	GValue val;

	val.g_type = 0;
	gtk_tree_model_get_value(gtk_tree_view_get_model(treeview), arg1, ROOM_COLUMN, &val);
	category = g_value_get_pointer(&val);

	if (!category->expanded_once) {
		gaim_roomlist_expand_category(list, category);
		category->expanded_once = TRUE;
	}
}

static gboolean account_filter_func(GaimAccount *account)
{
	GaimConnection *gc = gaim_account_get_connection(account);
	GaimPluginProtocolInfo *prpl_info = NULL;

	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	return (prpl_info->roomlist_get_list != NULL);
}

gboolean
pidgin_roomlist_is_showable()
{
	GList *c;
	GaimConnection *gc;

	for (c = gaim_connections_get_all(); c != NULL; c = c->next) {
		gc = c->data;

		if (account_filter_func(gaim_connection_get_account(gc)))
			return TRUE;
	}

	return FALSE;
}

static PidginRoomlistDialog *
pidgin_roomlist_dialog_new_with_account(GaimAccount *account)
{
	PidginRoomlistDialog *dialog;
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *vbox2;
	GtkWidget *account_hbox;
	GtkWidget *bbox;
	GtkWidget *label;

	dialog = g_new0(PidginRoomlistDialog, 1);
	dialog->account = account;

	/* Create the window. */
	dialog->window = window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_role(GTK_WINDOW(window), "room list");
	gtk_window_set_title(GTK_WINDOW(window), _("Room List"));

	gtk_container_set_border_width(GTK_CONTAINER(window), PIDGIN_HIG_BORDER);

	g_signal_connect(G_OBJECT(window), "delete_event",
					 G_CALLBACK(delete_win_cb), dialog);

	/* Create the parent vbox for everything. */
	vbox = gtk_vbox_new(FALSE, PIDGIN_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_widget_show(vbox);

	vbox2 = gtk_vbox_new(FALSE, PIDGIN_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(vbox), vbox2);
	gtk_widget_show(vbox2);

	/* accounts dropdown list */
	account_hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(vbox2), account_hbox, FALSE, FALSE, 0);
	gtk_widget_show(account_hbox);

	label = gtk_label_new(NULL);
	gtk_box_pack_start(GTK_BOX(account_hbox), label, FALSE, FALSE, 0);
	gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), _("_Account:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_widget_show(label);

	dialog->account_widget = pidgin_account_option_menu_new(dialog->account, FALSE,
	                         G_CALLBACK(dialog_select_account_cb), account_filter_func, dialog);

	if (!dialog->account) /* this is normally null, and we normally don't care what the first selected item is */
		dialog->account = pidgin_account_option_menu_get_selected(dialog->account_widget);

	gtk_box_pack_start(GTK_BOX(account_hbox), dialog->account_widget, TRUE, TRUE, 0);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), GTK_WIDGET(dialog->account_widget));
	gtk_widget_show(dialog->account_widget);

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
	bbox = gtk_hbutton_box_new();
	gtk_box_set_spacing(GTK_BOX(bbox), PIDGIN_HIG_BOX_SPACE);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, TRUE, 0);
	gtk_widget_show(bbox);

	/* stop button */
	dialog->stop_button = gtk_button_new_from_stock(GTK_STOCK_STOP);
	gtk_box_pack_start(GTK_BOX(bbox), dialog->stop_button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(dialog->stop_button), "clicked",
	                 G_CALLBACK(stop_button_cb), dialog);
	gtk_widget_set_sensitive(dialog->stop_button, FALSE);
	gtk_widget_show(dialog->stop_button);

	/* list button */
	dialog->list_button = pidgin_pixbuf_button_from_stock(_("_Get List"), GTK_STOCK_REFRESH,
	                                                    PIDGIN_BUTTON_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(bbox), dialog->list_button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(dialog->list_button), "clicked",
	                 G_CALLBACK(list_button_cb), dialog);
	gtk_widget_show(dialog->list_button);

	/* add button */
	dialog->add_button = pidgin_pixbuf_button_from_stock(_("_Add Chat"), GTK_STOCK_ADD,
	                                                    PIDGIN_BUTTON_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(bbox), dialog->add_button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(dialog->add_button), "clicked",
	                 G_CALLBACK(add_room_to_blist_cb), dialog);
	gtk_widget_set_sensitive(dialog->add_button, FALSE);
	gtk_widget_show(dialog->add_button);

	/* join button */
	dialog->join_button = pidgin_pixbuf_button_from_stock(_("_Join"), PIDGIN_STOCK_CHAT,
	                                                    PIDGIN_BUTTON_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(bbox), dialog->join_button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(dialog->join_button), "clicked",
					 G_CALLBACK(join_button_cb), dialog);
	gtk_widget_set_sensitive(dialog->join_button, FALSE);
	gtk_widget_show(dialog->join_button);

	/* close button */
	dialog->close_button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_box_pack_start(GTK_BOX(bbox), dialog->close_button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(dialog->close_button), "clicked",
					 G_CALLBACK(close_button_cb), dialog);
	gtk_widget_show(dialog->close_button);

	/* show the dialog window and return the dialog */
	gtk_widget_show(dialog->window);

	return dialog;
}

void pidgin_roomlist_dialog_show_with_account(GaimAccount *account)
{
	PidginRoomlistDialog *dialog;

	dialog = pidgin_roomlist_dialog_new_with_account(account);
	if (!dialog)
		return;

	list_button_cb(GTK_BUTTON(dialog->list_button), dialog);
}

void pidgin_roomlist_dialog_show(void)
{
	pidgin_roomlist_dialog_new_with_account(NULL);
}

static void pidgin_roomlist_new(GaimRoomlist *list)
{
	PidginRoomlist *rl;

	rl = g_new0(PidginRoomlist, 1);

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

static gboolean
_search_func(GtkTreeModel *model, gint column, const gchar *key, GtkTreeIter *iter, gpointer search_data)
{
	gboolean result;
	gchar *name, *fold, *fkey;

	gtk_tree_model_get(model, iter, column, &name, -1);
	fold = g_utf8_casefold(name, -1);
	fkey = g_utf8_casefold(key, -1);

	result = (g_strstr_len(fold, strlen(fold), fkey) == NULL);

	g_free(fold);
	g_free(fkey);
	g_free(name);

	return result;
}

static void pidgin_roomlist_set_fields(GaimRoomlist *list, GList *fields)
{
	PidginRoomlist *grl = list->ui_data;
	gint columns = NUM_OF_COLUMNS;
	int j;
	GtkTreeStore *model;
	GtkWidget *tree;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
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

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	g_signal_connect(G_OBJECT(selection), "changed",
					 G_CALLBACK(selection_changed_cb), grl);

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

	/* Enable CTRL+F searching */
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(tree), NAME_COLUMN);
	gtk_tree_view_set_search_equal_func(GTK_TREE_VIEW(tree), _search_func, NULL, NULL);

}

static gboolean pidgin_progress_bar_pulse(gpointer data)
{
	GaimRoomlist *list = data;
	PidginRoomlist *rl = list->ui_data;

	if (!rl || !rl->dialog || !rl->dialog->pg_needs_pulse) {
		if (rl && rl->dialog)
			rl->dialog->pg_to_active = FALSE;
		gaim_roomlist_unref(list);
		return FALSE;
	}

	gtk_progress_bar_pulse(GTK_PROGRESS_BAR(rl->dialog->progress));
	rl->dialog->pg_needs_pulse = FALSE;
	return TRUE;
}

static void pidgin_roomlist_add_room(GaimRoomlist *list, GaimRoomlistRoom *room)
{
	PidginRoomlist *rl = list->ui_data;
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
		if (!rl->dialog->pg_to_active) {
			rl->dialog->pg_to_active = TRUE;
			gaim_roomlist_ref(list);
			rl->dialog->pg_update_to = g_timeout_add(100, pidgin_progress_bar_pulse, list);
			gtk_progress_bar_pulse(GTK_PROGRESS_BAR(rl->dialog->progress));
		} else {
			rl->dialog->pg_needs_pulse = TRUE;
		}
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

	if (room->type & GAIM_ROOMLIST_ROOMTYPE_CATEGORY)
		gtk_tree_store_append(rl->model, &child, &iter);

	path = gtk_tree_model_get_path(GTK_TREE_MODEL(rl->model), &iter);

	if (room->type & GAIM_ROOMLIST_ROOMTYPE_CATEGORY) {
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

static void pidgin_roomlist_in_progress(GaimRoomlist *list, gboolean flag)
{
	PidginRoomlist *rl = list->ui_data;

	if (!rl || !rl->dialog)
		return;

	if (flag) {
		if (rl->dialog->account_widget)
			gtk_widget_set_sensitive(rl->dialog->account_widget, FALSE);
		gtk_widget_set_sensitive(rl->dialog->stop_button, TRUE);
		gtk_widget_set_sensitive(rl->dialog->list_button, FALSE);
	} else {
		rl->dialog->pg_needs_pulse = FALSE;
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(rl->dialog->progress), 0.0);
		if (rl->dialog->account_widget)
			gtk_widget_set_sensitive(rl->dialog->account_widget, TRUE);
		gtk_widget_set_sensitive(rl->dialog->stop_button, FALSE);
		gtk_widget_set_sensitive(rl->dialog->list_button, TRUE);
	}
}

static void pidgin_roomlist_destroy(GaimRoomlist *list)
{
	PidginRoomlist *rl;

	roomlists = g_list_remove(roomlists, list);

	rl = list->ui_data;

	g_return_if_fail(rl != NULL);

	g_hash_table_destroy(rl->cats);
	g_free(rl);
	list->ui_data = NULL;
}

static GaimRoomlistUiOps ops = {
	pidgin_roomlist_dialog_show_with_account,
	pidgin_roomlist_new,
	pidgin_roomlist_set_fields,
	pidgin_roomlist_add_room,
	pidgin_roomlist_in_progress,
	pidgin_roomlist_destroy
};


void pidgin_roomlist_init(void)
{
	gaim_roomlist_set_ui_ops(&ops);
}
