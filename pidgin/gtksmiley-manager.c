/*
 * pidgin
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#include "internal.h"

#include "debug.h"
#include "http.h"
#include "notify.h"
#include "smiley.h"
#include "smiley-custom.h"
#include "smiley-list.h"

#include "gtksmiley-manager.h"
#include "gtkutils.h"
#include "pidginstock.h"

#include "gtk3compat.h"

typedef struct
{
	PurpleSmiley *smiley;

	gchar *filename;
	PurpleStoredImage *new_image;

	GtkDialog *window;
	GtkImage *thumbnail;
	GtkEntry *shortcut;
} SmileyEditDialog;

typedef struct
{
	GtkDialog *window;
	GtkListStore *model;
	GtkTreeView *tree;

	PurpleHttpConnection *running_request;
} SmileyManager;

enum
{
	SMILEY_LIST_MODEL_ICON,
	SMILEY_LIST_MODEL_SHORTCUT,
	SMILEY_LIST_MODEL_PURPLESMILEY,
	SMILEY_LIST_MODEL_N_COL
};

enum
{
	PIDGIN_RESPONSE_MODIFY
};

static SmileyManager *smiley_manager = NULL;

static void
edit_dialog_update_buttons(SmileyEditDialog *edit_dialog);

static void
manager_list_fill(SmileyManager *manager);


/*******************************************************************************
 * Custom smiley edit dialog image.
 ******************************************************************************/

static void
edit_dialog_image_update_thumb(SmileyEditDialog *edit_dialog)
{
	GdkPixbuf *pixbuf = NULL;

	if (edit_dialog->new_image) {
		pixbuf = pidgin_pixbuf_from_imgstore(edit_dialog->new_image);
	} else if (edit_dialog->filename) {
		pixbuf = pidgin_pixbuf_new_from_file(edit_dialog->filename);
		if (!pixbuf) {
			g_free(edit_dialog->filename);
			edit_dialog->filename = NULL;
		}
	}

	if (pixbuf) {
		pixbuf = pidgin_pixbuf_scale_down(pixbuf, 64, 64,
			GDK_INTERP_HYPER, TRUE);
	}

	if (!pixbuf) {
		GtkIconSize icon_size =
			gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_SMALL);
		pixbuf = gtk_widget_render_icon(GTK_WIDGET(edit_dialog->window),
			PIDGIN_STOCK_TOOLBAR_SELECT_AVATAR, icon_size,
			"PidginSmileyManager");
	}
	g_return_if_fail(pixbuf != NULL);

	gtk_image_set_from_pixbuf(GTK_IMAGE(edit_dialog->thumbnail), pixbuf);

	g_object_unref(G_OBJECT(pixbuf));
}

static gboolean
edit_dialog_set_image(SmileyEditDialog *edit_dialog,
	PurpleStoredImage *image)
{
	GdkPixbuf *tmp = NULL;

	if (edit_dialog->new_image)
		purple_imgstore_unref(edit_dialog->new_image);

	if (edit_dialog->smiley) {
		g_object_set_data(G_OBJECT(edit_dialog->smiley),
			"pidgin-smiley-manager-list-thumb", NULL);
	}

	/* check, if image is valid */
	if (image)
		tmp = pidgin_pixbuf_from_imgstore(image);
	if (tmp)
		g_object_unref(tmp);
	else {
		purple_imgstore_unref(image);
		image = NULL;
	}

	edit_dialog->new_image = image;

	edit_dialog_image_update_thumb(edit_dialog);
	edit_dialog_update_buttons(edit_dialog);

	return (image != NULL);
}

static void
edit_dialog_image_choosen(const char *filename, gpointer _edit_dialog)
{
	PurpleStoredImage *image;
	SmileyEditDialog *edit_dialog = _edit_dialog;

	if (!filename)
		return;

	image = purple_imgstore_new_from_file(filename);
	if (!image)
		return;

	g_free(edit_dialog->filename);
	edit_dialog->filename = NULL;

	if (!edit_dialog_set_image(edit_dialog, image))
		return;
	edit_dialog->filename = g_strdup(filename);

	gtk_widget_grab_focus(GTK_WIDGET(edit_dialog->shortcut));
}

static void
edit_dialog_image_choose(GtkWidget *widget, gpointer _edit_dialog)
{
	GtkWidget *file_chooser;
	file_chooser = pidgin_buddy_icon_chooser_new(
		GTK_WINDOW(gtk_widget_get_toplevel(widget)),
		edit_dialog_image_choosen, _edit_dialog);
	gtk_window_set_title(GTK_WINDOW(file_chooser), _("Custom Smiley"));
	gtk_window_set_role(GTK_WINDOW(file_chooser),
		"file-selector-custom-smiley");
	gtk_widget_show_all(file_chooser);
}


/*******************************************************************************
 * Custom smiley edit dialog.
 ******************************************************************************/

static void
edit_dialog_destroy(GtkWidget *window, gpointer _edit_dialog)
{
	SmileyEditDialog *edit_dialog = _edit_dialog;

	if (edit_dialog->smiley) {
		g_object_set_data(G_OBJECT(edit_dialog->smiley),
			"pidgin-smiley-manager-edit-dialog", NULL);
		g_object_unref(edit_dialog->smiley);
	}

	purple_imgstore_unref(edit_dialog->new_image);

	g_free(edit_dialog->filename);
	g_free(edit_dialog);
}

static void
edit_dialog_save(SmileyEditDialog *edit_dialog)
{
	const gchar *shortcut;
	PurpleSmiley *existing_smiley;
	gboolean shortcut_changed, image_changed;

	shortcut = gtk_entry_get_text(edit_dialog->shortcut);

	existing_smiley = purple_smiley_list_get_by_shortcut(
		purple_smiley_custom_get_list(), shortcut);

	if (existing_smiley && existing_smiley != edit_dialog->smiley) {
		gchar *msg = g_strdup_printf(
			_("A custom smiley for '%s' already exists.  "
			"Please use a different shortcut."), shortcut);
		purple_notify_error(edit_dialog, _("Custom Smiley"),
			_("Duplicate Shortcut"), msg, NULL);
		g_free(msg);
		return;
	}

	if (edit_dialog->smiley == NULL)
		shortcut_changed = image_changed = TRUE;
	else {
		shortcut_changed = (g_strcmp0(purple_smiley_get_shortcut(
			edit_dialog->smiley), shortcut) != 0);
		image_changed = (edit_dialog->new_image != NULL);
	}

	if (!shortcut_changed && !image_changed) {
		gtk_widget_destroy(GTK_WIDGET(edit_dialog->window));
		return;
	}

	if (edit_dialog->new_image == NULL) {
		/* We're reading the file and then writing it back - it's not
		 * efficient, but it's also not really important here. */
		edit_dialog->new_image = purple_imgstore_new_from_file(
			purple_smiley_get_path(edit_dialog->smiley));
		g_return_if_fail(edit_dialog->new_image);
	}

	if (edit_dialog->smiley)
		purple_smiley_custom_remove(edit_dialog->smiley);
	purple_smiley_custom_add(edit_dialog->new_image, shortcut);

	if (smiley_manager)
		manager_list_fill(smiley_manager);

	gtk_widget_destroy(GTK_WIDGET(edit_dialog->window));
}

static void
edit_dialog_update_buttons(SmileyEditDialog *edit_dialog)
{
	gboolean shortcut_ok, image_ok;

	shortcut_ok = (gtk_entry_get_text_length(edit_dialog->shortcut) > 0);
	image_ok = (edit_dialog->filename || edit_dialog->new_image);

	gtk_dialog_set_response_sensitive(edit_dialog->window,
		GTK_RESPONSE_ACCEPT, shortcut_ok && image_ok);
}

static void
edit_dialog_shortcut_changed(GtkEditable *shortcut, gpointer _edit_dialog)
{
	SmileyEditDialog *edit_dialog = _edit_dialog;

	edit_dialog_update_buttons(edit_dialog);
}

static void
edit_dialog_response(GtkDialog *window, gint response_id,
	gpointer _edit_dialog)
{
	SmileyEditDialog *edit_dialog = _edit_dialog;

	switch (response_id) {
		case GTK_RESPONSE_ACCEPT:
			edit_dialog_save(edit_dialog);
			break;
		case GTK_RESPONSE_DELETE_EVENT:
		case GTK_RESPONSE_CANCEL:
			gtk_widget_destroy(GTK_WIDGET(edit_dialog->window));
			break;
		default:
			g_warn_if_reached();
	}
}

static SmileyEditDialog *
edit_dialog_show(SmileyManager *manager, PurpleSmiley *smiley)
{
	SmileyEditDialog *edit_dialog;
	GtkWidget *vbox, *hbox;
	GtkLabel *label;
	GtkButton *filech;

	if (smiley) {
		edit_dialog = g_object_get_data(G_OBJECT(smiley),
			"pidgin-smiley-manager-edit-dialog");
		if (edit_dialog) {
			gtk_window_present(GTK_WINDOW(edit_dialog->window));
			return edit_dialog;
		}
	}

	edit_dialog = g_new0(SmileyEditDialog, 1);

	edit_dialog->window = GTK_DIALOG(gtk_dialog_new_with_buttons(
		smiley ? _("Edit Smiley") : _("Add Smiley"),
		GTK_WINDOW(manager->window), GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		smiley ? GTK_STOCK_SAVE : GTK_STOCK_ADD, GTK_RESPONSE_ACCEPT,
		NULL));
	gtk_dialog_set_default_response(
		edit_dialog->window, GTK_RESPONSE_ACCEPT);

	if (smiley) {
		edit_dialog->smiley = smiley;
		g_object_set_data(G_OBJECT(smiley),
			"pidgin-smiley-manager-edit-dialog", edit_dialog);
		g_object_ref(smiley);
	}

#if !GTK_CHECK_VERSION(3,0,0)
	gtk_container_set_border_width(
		GTK_CONTAINER(edit_dialog->window), PIDGIN_HIG_BORDER);
#endif

	/* The vbox */
#if GTK_CHECK_VERSION(3,0,0)
	vbox = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(vbox), PIDGIN_HIG_BORDER);
#else
	vbox = gtk_vbox_new(FALSE, PIDGIN_HIG_BORDER);
#endif
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(
		edit_dialog->window)), vbox);
	gtk_widget_show(vbox);

	/* The hbox */
#if GTK_CHECK_VERSION(3,0,0)
	hbox = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(hbox), PIDGIN_HIG_BORDER);
	gtk_grid_attach(GTK_GRID(vbox), hbox, 0, 0, 1, 1);
#else
	hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(GTK_VBOX(vbox)), hbox);
#endif

	label = GTK_LABEL(gtk_label_new_with_mnemonic(_("_Image:")));
#if GTK_CHECK_VERSION(3,0,0)
	gtk_grid_attach(GTK_GRID(hbox), GTK_WIDGET(label), 0, 0, 1, 1);
#else
	gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(label), FALSE, FALSE, 0);
#endif
	gtk_widget_show(GTK_WIDGET(label));

	filech = GTK_BUTTON(gtk_button_new());
#if GTK_CHECK_VERSION(3,0,0)
	gtk_grid_attach_next_to(GTK_GRID(hbox), GTK_WIDGET(filech), NULL,
		GTK_POS_RIGHT, 1, 1);
#else
	gtk_box_pack_end(GTK_BOX(hbox), GTK_WIDGET(filech), FALSE, FALSE, 0);
#endif
	pidgin_set_accessible_label(GTK_WIDGET(filech), label);

	edit_dialog->thumbnail = GTK_IMAGE(gtk_image_new());
	gtk_container_add(GTK_CONTAINER(filech),
		GTK_WIDGET(edit_dialog->thumbnail));

	gtk_widget_show_all(hbox);

	/* info */
#if GTK_CHECK_VERSION(3,0,0)
	hbox = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(hbox), PIDGIN_HIG_BORDER);

	gtk_grid_attach_next_to(GTK_GRID(vbox), hbox, NULL,
		GTK_POS_BOTTOM, 1, 1);
#else
	hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(GTK_VBOX(vbox)), hbox);
#endif

	/* Shortcut text */
	label = GTK_LABEL(gtk_label_new_with_mnemonic(_("S_hortcut text:")));
#if GTK_CHECK_VERSION(3,0,0)
	gtk_grid_attach(GTK_GRID(hbox), GTK_WIDGET(label), 0, 0, 1, 1);
#else
	gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(label), FALSE, FALSE, 0);
#endif
	gtk_widget_show(GTK_WIDGET(label));

	edit_dialog->shortcut = GTK_ENTRY(gtk_entry_new());
	gtk_entry_set_activates_default(edit_dialog->shortcut, TRUE);
	pidgin_set_accessible_label(GTK_WIDGET(edit_dialog->shortcut), label);

#if GTK_CHECK_VERSION(3,0,0)
	gtk_grid_attach_next_to(GTK_GRID(hbox),
		GTK_WIDGET(edit_dialog->shortcut), NULL, GTK_POS_RIGHT, 1, 1);
#else
	gtk_box_pack_end(GTK_BOX(hbox), GTK_WIDGET(edit_dialog->shortcut),
		FALSE, FALSE, 0);
#endif

	gtk_widget_show(GTK_WIDGET(edit_dialog->shortcut));
	gtk_widget_show(hbox);
	gtk_widget_show(GTK_WIDGET(edit_dialog->window));

	if (smiley) {
		edit_dialog->filename =
			g_strdup(purple_smiley_get_path(smiley));
		gtk_entry_set_text(edit_dialog->shortcut,
			purple_smiley_get_shortcut(smiley));
	}

	edit_dialog_image_update_thumb(edit_dialog);
	edit_dialog_update_buttons(edit_dialog);

	g_signal_connect(edit_dialog->window, "response",
		G_CALLBACK(edit_dialog_response), edit_dialog);
	g_signal_connect(filech, "clicked",
		G_CALLBACK(edit_dialog_image_choose), edit_dialog);
	g_signal_connect(edit_dialog->shortcut, "changed",
		G_CALLBACK(edit_dialog_shortcut_changed), edit_dialog);

	g_signal_connect(edit_dialog->window, "destroy",
		G_CALLBACK(edit_dialog_destroy), edit_dialog);
	g_signal_connect(edit_dialog->window, "destroy",
		G_CALLBACK(purple_notify_close_with_handle), edit_dialog);

	return edit_dialog;
}

/*******************************************************************************
 * Custom smiley list Drag-and-drop support.
 ******************************************************************************/

static void
smiley_list_dnd_url_got(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, gpointer _manager)
{
	SmileyManager *manager = _manager;
	SmileyEditDialog *edit_dialog;
	PurpleStoredImage *image;
	const gchar *image_data;
	size_t image_size;

	g_return_if_fail(manager == smiley_manager);
	g_return_if_fail(manager->running_request == http_conn);
	manager->running_request = NULL;

	if (!purple_http_response_is_successful(response))
		return;

	image_data = purple_http_response_get_data(response, &image_size);
	image = purple_imgstore_new(g_memdup(image_data, image_size),
		image_size, NULL);
	if (!image)
		return;

	edit_dialog = edit_dialog_show(manager, NULL);
	if (!edit_dialog_set_image(edit_dialog, image))
		gtk_widget_destroy(GTK_WIDGET(edit_dialog->window));
}

static void
smiley_list_dnd_recv(GtkWidget *widget, GdkDragContext *dc, gint x, gint y,
	GtkSelectionData *sd, guint info, guint time, gpointer _manager)
{
	SmileyManager *manager = _manager;
	gchar content[1024];

	/* We don't need anything, that is not 8-bit per element (char). */
	if (gtk_selection_data_get_format(sd) != 8) {
		gtk_drag_finish(dc, FALSE, FALSE, time);
		return;
	}

	if (gtk_selection_data_get_length(sd) <= 0) {
		gtk_drag_finish(dc, FALSE, FALSE, time);
		return;
	}

	memset(&content, 0, sizeof(content));
	memcpy(&content, gtk_selection_data_get_data(sd),
		MIN((guint)gtk_selection_data_get_length(sd), sizeof(content)));
	g_strstrip(content);
	if (content[0] == '\0') {
		gtk_drag_finish(dc, FALSE, FALSE, time);
		return;
	}

	/* Well, it looks like the drag event was cool.
	 * Let's do something with it */

	if (purple_str_has_caseprefix(content, "file://")) {
		SmileyEditDialog *edit_dialog;
		PurpleStoredImage *image;
		gchar *filename;

		filename = g_filename_from_uri(content, NULL, NULL);
		if (!filename || !g_file_test(filename, G_FILE_TEST_EXISTS)) {
			purple_debug_warning("gtksmiley-manager",
				"dropped file does not exists");
			gtk_drag_finish(dc, FALSE, FALSE, time);
			return;
		}

		image = purple_imgstore_new_from_file(filename);
		if (!image) {
			purple_debug_warning("gtksmiley-manager",
				"dropped file is not a valid image");
			gtk_drag_finish(dc, FALSE, FALSE, time);
			return;
		}
		edit_dialog = edit_dialog_show(manager, NULL);
		if (!edit_dialog_set_image(edit_dialog, image)) {
			gtk_widget_destroy(GTK_WIDGET(edit_dialog->window));
			gtk_drag_finish(dc, FALSE, FALSE, time);
			return;
		}

		gtk_drag_finish(dc, TRUE, FALSE, time);
		return;
	}

	if (purple_str_has_caseprefix(content, "http://") ||
		purple_str_has_caseprefix(content, "https://"))
	{
		purple_http_conn_cancel(smiley_manager->
			running_request);
		smiley_manager->running_request = purple_http_get(NULL,
			smiley_list_dnd_url_got, manager, content);

		gtk_drag_finish(dc, TRUE, FALSE, time);
		return;
	}

	gtk_drag_finish(dc, FALSE, FALSE, time);
}

/*******************************************************************************
 * Custom smiley list.
 ******************************************************************************/

static void
smiley_list_selected(GtkTreeSelection *sel, gpointer _manager)
{
	SmileyManager *manager = _manager;
	gboolean sens;

	sens = (gtk_tree_selection_count_selected_rows(sel) > 0);

	gtk_dialog_set_response_sensitive(manager->window,
		GTK_RESPONSE_NO, sens);
	gtk_dialog_set_response_sensitive(manager->window,
		PIDGIN_RESPONSE_MODIFY, sens);
}

static void
smiley_list_activated(GtkTreeView *tree, GtkTreePath *path,
	GtkTreeViewColumn *col, gpointer _manager)
{
	SmileyManager *manager = _manager;
	GtkTreeIter iter;
	PurpleSmiley *smiley = NULL;

	if (!gtk_tree_model_get_iter(
		GTK_TREE_MODEL(manager->model), &iter, path))
	{
		return;
	}

	gtk_tree_model_get(GTK_TREE_MODEL(manager->model), &iter,
		SMILEY_LIST_MODEL_PURPLESMILEY, &smiley, -1);
	g_return_if_fail(PURPLE_IS_SMILEY(smiley));

	edit_dialog_show(manager, smiley);
}

static void
manager_list_add(SmileyManager *manager, PurpleSmiley *smiley)
{
	GdkPixbuf *smiley_image;
	GtkTreeIter iter;

	smiley_image = g_object_get_data(G_OBJECT(smiley),
		"pidgin-smiley-manager-list-thumb");
	if (smiley_image == NULL) {
		smiley_image = pidgin_pixbuf_new_from_file(
			purple_smiley_get_path(smiley));
		smiley_image = pidgin_pixbuf_scale_down(smiley_image,
			22, 22, GDK_INTERP_BILINEAR, TRUE);
		g_object_set_data_full(G_OBJECT(smiley),
			"pidgin-smiley-manager-list-thumb",
			smiley_image, g_object_unref);
	}

	gtk_list_store_append(manager->model, &iter);
	gtk_list_store_set(manager->model, &iter,
		SMILEY_LIST_MODEL_ICON, smiley_image,
		SMILEY_LIST_MODEL_SHORTCUT, purple_smiley_get_shortcut(smiley),
		SMILEY_LIST_MODEL_PURPLESMILEY, smiley,
		-1);
}

static void
manager_list_fill(SmileyManager *manager)
{
	GList *custom_smileys, *it;
	gtk_list_store_clear(manager->model);

	custom_smileys = purple_smiley_list_get_all(
		purple_smiley_custom_get_list());

	for (it = custom_smileys; it; it = g_list_next(it)) {
		PurpleSmiley *smiley = it->data;

		manager_list_add(manager, smiley);
	}
	g_list_free(custom_smileys);
}

static GtkWidget *
manager_list_create(SmileyManager *manager)
{
	GtkTreeView *tree;
	GtkTreeSelection *sel;
	GtkCellRenderer *cellrend;
	GtkTreeViewColumn *column;
	GtkTargetEntry targets[3] = {
		{"text/plain", 0, 0},
		{"text/uri-list", 0, 1},
		{"STRING", 0, 2}
	};

	manager->model = gtk_list_store_new(SMILEY_LIST_MODEL_N_COL,
		GDK_TYPE_PIXBUF, /* icon */
		G_TYPE_STRING, /* shortcut */
		G_TYPE_OBJECT /* PurpleSmiley */
		);

	manager->tree = tree = GTK_TREE_VIEW(gtk_tree_view_new_with_model(
		GTK_TREE_MODEL(manager->model)));

	gtk_tree_view_set_rules_hint(tree, TRUE);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(manager->model),
		SMILEY_LIST_MODEL_SHORTCUT, GTK_SORT_ASCENDING);

	g_object_unref(manager->model);

	sel = gtk_tree_view_get_selection(tree);
	gtk_tree_selection_set_mode(sel, GTK_SELECTION_MULTIPLE);

	g_signal_connect(sel, "changed",
		G_CALLBACK(smiley_list_selected), manager);
	g_signal_connect(tree, "row-activated",
		G_CALLBACK(smiley_list_activated), manager);

	gtk_drag_dest_set(GTK_WIDGET(tree), GTK_DEST_DEFAULT_MOTION |
		GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP,
		targets, G_N_ELEMENTS(targets),
		GDK_ACTION_COPY | GDK_ACTION_MOVE);
	g_signal_connect(tree, "drag-data-received",
		G_CALLBACK(smiley_list_dnd_recv), manager);

	gtk_widget_show(GTK_WIDGET(tree));

	/* setting up columns */

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Smiley"));
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(tree, column);
	cellrend = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, cellrend, FALSE);
	gtk_tree_view_column_add_attribute(column, cellrend,
		"pixbuf", SMILEY_LIST_MODEL_ICON);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Shortcut Text"));
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(tree, column);
	cellrend = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, cellrend, TRUE);
	gtk_tree_view_column_add_attribute(column, cellrend,
		"text", SMILEY_LIST_MODEL_SHORTCUT);

	manager_list_fill(manager);

	return pidgin_make_scrollable(GTK_WIDGET(tree), GTK_POLICY_AUTOMATIC,
		GTK_POLICY_AUTOMATIC, GTK_SHADOW_IN, -1, -1);
}

/*******************************************************************************
 * Custom smiley manager window.
 ******************************************************************************/


static void
manager_select_cb(GtkWidget *widget, gint resp, SmileyManager *manager)
{
	GtkTreeSelection *selection = NULL;
	GList *selected_rows, *selected_smileys = NULL, *it;
	GtkTreeModel *model = GTK_TREE_MODEL(manager->model);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(manager->tree));
	selected_rows = gtk_tree_selection_get_selected_rows(selection, NULL);
	for (it = selected_rows; it; it = g_list_next(it)) {
		GtkTreePath *path = it->data;
		GtkTreeIter iter;
		PurpleSmiley *smiley = NULL;

		if (!gtk_tree_model_get_iter(model, &iter, path))
			continue;

		gtk_tree_model_get(model, &iter,
			SMILEY_LIST_MODEL_PURPLESMILEY, &smiley, -1);
		if (!smiley)
			continue;

		selected_smileys = g_list_prepend(selected_smileys, smiley);
	}
	g_list_free_full(selected_rows, (GDestroyNotify)gtk_tree_path_free);

	switch (resp) {
		case GTK_RESPONSE_YES:
			edit_dialog_show(manager, NULL);
			break;
		case GTK_RESPONSE_NO:
			for (it = selected_smileys; it; it = g_list_next(it))
				purple_smiley_custom_remove(it->data);
			manager_list_fill(manager);
			break;
		case GTK_RESPONSE_DELETE_EVENT:
		case GTK_RESPONSE_CLOSE:
			gtk_widget_destroy(GTK_WIDGET(manager->window));
			purple_http_conn_cancel(manager->running_request);
			g_free(manager);
			smiley_manager = NULL;
			break;
		case PIDGIN_RESPONSE_MODIFY:
			for (it = selected_smileys; it; it = g_list_next(it))
				edit_dialog_show(manager, it->data);
			break;
		default:
			g_warn_if_reached();
	}

	g_list_free(selected_smileys);
}

void
pidgin_smiley_manager_show(void)
{
	SmileyManager *manager;
	GtkDialog *win;
	GtkWidget *sw, *vbox;

	if (smiley_manager) {
		gtk_window_present(GTK_WINDOW(smiley_manager->window));
		return;
	}

	manager = g_new0(SmileyManager, 1);
	smiley_manager = manager;

	manager->window = win = GTK_DIALOG(gtk_dialog_new_with_buttons(
		_("Custom Smiley Manager"), NULL,
		GTK_DIALOG_DESTROY_WITH_PARENT,
		PIDGIN_STOCK_ADD, GTK_RESPONSE_YES,
		PIDGIN_STOCK_MODIFY, PIDGIN_RESPONSE_MODIFY,
		GTK_STOCK_DELETE, GTK_RESPONSE_NO,
		GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
		NULL));

	gtk_window_set_default_size(GTK_WINDOW(win), 50, 400);
	gtk_window_set_role(GTK_WINDOW(win), "custom_smiley_manager");
#if !GTK_CHECK_VERSION(3,0,0)
	gtk_container_set_border_width(GTK_CONTAINER(win), PIDGIN_HIG_BORDER);
#endif
	gtk_dialog_set_response_sensitive(win, GTK_RESPONSE_NO, FALSE);
	gtk_dialog_set_response_sensitive(win, PIDGIN_RESPONSE_MODIFY, FALSE);

	g_signal_connect(win, "response",
		G_CALLBACK(manager_select_cb), manager);

	/* The vbox */
	vbox = gtk_dialog_get_content_area(win);

	/* get the scrolled window with all stuff */
	sw = manager_list_create(manager);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
	gtk_widget_show(sw);

	gtk_widget_show(GTK_WIDGET(win));
}
