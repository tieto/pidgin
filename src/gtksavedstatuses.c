/**
 * @file gtksavedstatus.c GTK+ Saved Status Editor UI
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

#include "account.h"
#include "internal.h"
#include "notify.h"
#include "request.h"
#include "savedstatuses.h"
#include "status.h"
#include "util.h"

#include "gtkblist.h"
#include "gtkgaim.h"
#include "gtkgaim-disclosure.h"
#include "gtkimhtml.h"
#include "gtkimhtmltoolbar.h"
#include "gtksavedstatuses.h"
#include "gtkstock.h"
#include "gtkutils.h"

enum
{
	STATUS_WINDOW_COLUMN_TITLE,
	STATUS_WINDOW_COLUMN_TYPE,
	STATUS_WINDOW_COLUMN_MESSAGE,
	STATUS_WINDOW_NUM_COLUMNS
};

enum
{
	STATUS_EDITOR_COLUMN_CUSTOM_STATUS,
	STATUS_EDITOR_COLUMN_ICON,
	STATUS_EDITOR_COLUMN_SCREENNAME,
	STATUS_EDITOR_NUM_COLUMNS
};

typedef struct
{
	GtkWidget *window;
	GtkListStore *model;
	GtkWidget *treeview;
	GtkWidget *modify_button;
	GtkWidget *delete_button;
} StatusWindow;

typedef struct
{
	GtkWidget *window;
	GtkListStore *model;
	GtkWidget *treeview;
	GtkWidget *save_button;

	gchar *original_title;
	GtkEntry *title;
	GtkOptionMenu *type;
	GtkIMHtml *message;
} StatusEditor;

static StatusWindow *status_window = NULL;


/**************************************************************************
* Status window
**************************************************************************/

static gboolean
status_window_find_savedstatus(GtkTreeIter *iter, const char *title)
{
	GtkTreeModel *model = GTK_TREE_MODEL(status_window->model);
	char *cur;

	if (!gtk_tree_model_get_iter_first(model, iter))
		return FALSE;

	gtk_tree_model_get(model, iter, STATUS_WINDOW_COLUMN_TITLE, &cur, -1);
	if (!strcmp(title, cur))
	{
		g_free(cur);
		return TRUE;
	}
	g_free(cur);

	while (gtk_tree_model_iter_next(model, iter))
	{
		gtk_tree_model_get(model, iter, STATUS_WINDOW_COLUMN_TITLE, &cur, -1);
		if (!strcmp(title, cur))
		{
			g_free(cur);
			return TRUE;
		}
		g_free(cur);
	}

	return FALSE;
}

static gboolean
status_window_destroy_cb(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	StatusWindow *dialog = user_data;

	dialog->window = NULL;
	gaim_gtk_status_window_hide();

	return FALSE;
}

static void
status_window_add_cb(GtkButton *button, gpointer user_data)
{
	gaim_gtk_status_editor_show(NULL);
}

static void
status_window_modify_foreach(GtkTreeModel *model, GtkTreePath *path,
							 GtkTreeIter *iter, gpointer user_data)
{
	char *title;
	GaimSavedStatus *status;

	gtk_tree_model_get(model, iter, STATUS_WINDOW_COLUMN_TITLE, &title, -1);

	status = gaim_savedstatus_find(title);
	g_free(title);
	gaim_gtk_status_editor_show(status);
}

static void
status_window_modify_cb(GtkButton *button, gpointer user_data)
{
	StatusWindow *dialog = user_data;
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->treeview));

	gtk_tree_selection_selected_foreach(selection, status_window_modify_foreach, user_data);
}

static void
status_window_delete_confirm_cb(char *title)
{
	GtkTreeIter iter;

	if (status_window_find_savedstatus(&iter, title))
		gtk_list_store_remove(status_window->model, &iter);

	gaim_savedstatus_delete(title);

	g_free(title);
}

static void
status_window_delete_foreach(GtkTreeModel *model, GtkTreePath *path,
							 GtkTreeIter *iter, gpointer user_data)
{
	char *title;
	char *title_escaped, *buf;

	gtk_tree_model_get(model, iter, STATUS_WINDOW_COLUMN_TITLE, &title, -1);

	title_escaped = g_markup_escape_text(title, -1);
	buf = g_strdup_printf(_("Are you sure you want to delete %s?"), title_escaped);
	free(title_escaped);
	gaim_request_action(NULL, NULL, buf, NULL, 0, title, 2,
						_("Delete"), status_window_delete_confirm_cb,
						_("Cancel"), g_free);
	g_free(buf);
}

static void
status_window_delete_cb(GtkButton *button, gpointer user_data)
{
	StatusWindow *dialog = user_data;
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->treeview));

	gtk_tree_selection_selected_foreach(selection, status_window_delete_foreach, user_data);
}

static void
status_window_close_cb(GtkButton *button, gpointer user_data)
{
	gaim_gtk_status_window_hide();
}

static void
status_selected_cb(GtkTreeSelection *sel, gpointer user_data)
{
	StatusWindow *dialog = user_data;
	gboolean selected = FALSE;

#if GTK_CHECK_VERSION(2,2,0)
	selected = (gtk_tree_selection_count_selected_rows(sel) > 0);
#else
	gtk_tree_selection_selected_foreach(sel, get_selected_helper, &selected);
#endif

	gtk_widget_set_sensitive(dialog->modify_button, selected);
	gtk_widget_set_sensitive(dialog->delete_button, selected);
}

static void
add_status_to_saved_status_list(GtkListStore *model, GaimSavedStatus *saved_status)
{
	GtkTreeIter iter;
	const char *title;
	const char *type;
	char *message;

	title = gaim_savedstatus_get_title(saved_status);
	type = gaim_primitive_get_name_from_type(gaim_savedstatus_get_type(saved_status));
	message = gaim_markup_strip_html(gaim_savedstatus_get_message(saved_status));

	gtk_list_store_append(model, &iter);
	gtk_list_store_set(model, &iter,
					   STATUS_WINDOW_COLUMN_TITLE, title,
					   STATUS_WINDOW_COLUMN_TYPE, type,
					   STATUS_WINDOW_COLUMN_MESSAGE, message,
					   -1);
	free(message);
}

static void
populate_saved_status_list(StatusWindow *dialog)
{
	const GList *saved_statuses;

	gtk_list_store_clear(dialog->model);

	for (saved_statuses = gaim_savedstatuses_get_all(); saved_statuses != NULL;
			saved_statuses = g_list_next(saved_statuses))
	{
		add_status_to_saved_status_list(dialog->model, saved_statuses->data);
	}
}

static gboolean
search_func(GtkTreeModel *model, gint column, const gchar *key, GtkTreeIter *iter, gpointer search_data)
{
	gboolean result;
	char *haystack;

	gtk_tree_model_get(model, iter, column, &haystack, -1);

	result = (gaim_strcasestr(haystack, key) == NULL);

	g_free(haystack);

	return result;
}

static GtkWidget *
create_saved_status_list(StatusWindow *dialog)
{
	GtkWidget *sw;
	GtkWidget *treeview;
	GtkTreeSelection *sel;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	/* Create the scrolled window */
	sw = gtk_scrolled_window_new(0, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
								   GTK_POLICY_AUTOMATIC,
								   GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
										GTK_SHADOW_IN);
	gtk_widget_show(sw);

	/* Create the list model */
	dialog->model = gtk_list_store_new(STATUS_WINDOW_NUM_COLUMNS,
									   G_TYPE_STRING,
									   G_TYPE_STRING,
									   G_TYPE_STRING);

	/* Create the treeview */
	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(dialog->model));
	dialog->treeview = treeview;
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	gtk_tree_selection_set_mode(sel, GTK_SELECTION_MULTIPLE);
	g_signal_connect(G_OBJECT(sel), "changed",
					 G_CALLBACK(status_selected_cb), dialog);

	gtk_container_add(GTK_CONTAINER(sw), treeview);
	gtk_widget_show(treeview);

	/* Add columns */
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Title"));
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column,
											STATUS_WINDOW_COLUMN_TITLE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "text",
									   STATUS_WINDOW_COLUMN_TITLE);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Type"));
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column,
											STATUS_WINDOW_COLUMN_TYPE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "text",
									   STATUS_WINDOW_COLUMN_TYPE);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Message"));
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column,
											STATUS_WINDOW_COLUMN_MESSAGE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "text",
									   STATUS_WINDOW_COLUMN_MESSAGE);
#if GTK_CHECK_VERSION(2,6,0)
	g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
#endif

	/* Enable CTRL+F searching */
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(treeview), STATUS_WINDOW_COLUMN_TITLE);
	gtk_tree_view_set_search_equal_func(GTK_TREE_VIEW(treeview), search_func, NULL, NULL);

	/* Sort the title column by default */
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(dialog->model),
										 STATUS_WINDOW_COLUMN_TITLE,
										 GTK_SORT_ASCENDING);

	/* Populate list */
	populate_saved_status_list(dialog);

	return sw;
}

static gboolean
configure_cb(GtkWidget *widget, GdkEventConfigure *event, StatusWindow *dialog)
{
	if (GTK_WIDGET_VISIBLE(widget)) {
		gaim_prefs_set_int("/gaim/gtk/status/dialog/width",  event->width);
		gaim_prefs_set_int("/gaim/gtk/status/dialog/height", event->height);
	}

	return FALSE;
}

void
gaim_gtk_status_window_show(void)
{
	StatusWindow *dialog;
	GtkWidget *bbox;
	GtkWidget *button;
	GtkWidget *list;
	GtkWidget *vbox;
	GtkWidget *win;
	int width, height;

	if (status_window != NULL) {
		gtk_window_present(GTK_WINDOW(status_window->window));
		return;
	}

	status_window = dialog = g_new0(StatusWindow, 1);

	width  = gaim_prefs_get_int("/gaim/gtk/status/dialog/width");
	height = gaim_prefs_get_int("/gaim/gtk/status/dialog/height");

	dialog->window = win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(win), width, height);
	gtk_window_set_role(GTK_WINDOW(win), "statuses");
	gtk_window_set_title(GTK_WINDOW(win), _("Saved Statuses"));
	gtk_container_set_border_width(GTK_CONTAINER(win), 12);

	g_signal_connect(G_OBJECT(win), "delete_event",
					 G_CALLBACK(status_window_destroy_cb), dialog);
	g_signal_connect(G_OBJECT(win), "configure_event",
					 G_CALLBACK(configure_cb), dialog);

	/* Setup the vbox */
	vbox = gtk_vbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(win), vbox);
	gtk_widget_show(vbox);

	/* List of saved status states */
	list = create_saved_status_list(dialog);
	gtk_box_pack_start(GTK_BOX(vbox), list, TRUE, TRUE, 0);

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
					 G_CALLBACK(status_window_add_cb), dialog);

	/* Modify button */
	button = gtk_button_new_from_stock(GAIM_STOCK_MODIFY);
	dialog->modify_button = button;
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_set_sensitive(button, FALSE);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(status_window_modify_cb), dialog);

	/* Delete button */
	button = gtk_button_new_from_stock(GTK_STOCK_DELETE);
	dialog->delete_button = button;
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_set_sensitive(button, FALSE);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(status_window_delete_cb), dialog);

	/* Close button */
	button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(status_window_close_cb), dialog);

	gtk_widget_show(win);
}

void
gaim_gtk_status_window_hide(void)
{
	if (status_window == NULL)
		return;

	if (status_window->window != NULL)
		gtk_widget_destroy(status_window->window);

	g_free(status_window);
	status_window = NULL;
}


/**************************************************************************
* Status editor
**************************************************************************/

static gboolean
status_editor_destroy_cb(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	StatusEditor *dialog = user_data;

	g_free(dialog->original_title);
	g_free(dialog);

	return FALSE;
}

static void
status_editor_cancel_cb(GtkButton *button, gpointer user_data)
{
	StatusEditor *dialog = user_data;

	gtk_widget_destroy(dialog->window);

	g_free(dialog->original_title);
	g_free(dialog);
}

static void
status_editor_save_cb(GtkButton *button, gpointer user_data)
{
	StatusEditor *dialog = user_data;
	const char *title;
	GaimStatusPrimitive type;
	char *message, *unformatted;
	GaimSavedStatus *status;

	title = gtk_entry_get_text(dialog->title);

	/*
	 * If the title is already taken then show an error dialog and
	 * don't do anything.
	 */
	if ((gaim_savedstatus_find(title) != NULL) &&
		((dialog->original_title == NULL) || (strcmp(title, dialog->original_title))))
	{
		gaim_notify_error(NULL, NULL, _("Title already in use.  You must "
						  "choose a unique title."), NULL);
		return;
	}

	type = gtk_option_menu_get_history(dialog->type);
	message = gtk_imhtml_get_markup(dialog->message);
	unformatted = gaim_markup_strip_html(message);

	/*
	 * If we're editing an existing status, remove the old one to
	 * make way for the modified one.
	 */
	if (dialog->original_title != NULL)
	{
		GtkTreeIter iter;

		gaim_savedstatus_delete(dialog->original_title);

		if (status_window_find_savedstatus(&iter, dialog->original_title))
		{
			gtk_list_store_remove(status_window->model, &iter);
		}
	}

	status = gaim_savedstatus_new(title, type);
	if (*unformatted != '\0')
		gaim_savedstatus_set_message(status, message);
	g_free(message);
	g_free(unformatted);

	gtk_widget_destroy(dialog->window);
	g_free(dialog->original_title);
	g_free(dialog);

	add_status_to_saved_status_list(status_window->model, status);
}

static void
status_editor_custom_status_cb(GtkCellRendererToggle *renderer, gchar *path_str, gpointer data)
{
	/* StatusEditor *dialog = (StatusEditor *)data; */

	/* TODO: Need to allow user to set a custom status for the highlighted account, somehow */
}

static void
editor_title_changed_cb(GtkWidget *widget, gpointer user_data)
{
	StatusEditor *dialog = user_data;
	const gchar *text;

	text = gtk_entry_get_text(dialog->title);

	gtk_widget_set_sensitive(dialog->save_button, (*text != '\0'));
}

static GtkWidget *
create_status_type_menu(GaimStatusPrimitive type)
{
	int i;
	GtkWidget *dropdown;
	GtkWidget *menu;
	GtkWidget *item;

	dropdown = gtk_option_menu_new();
	menu = gtk_menu_new();

	for (i = 0; i < GAIM_STATUS_NUM_PRIMITIVES; i++)
	{
		item = gtk_menu_item_new_with_label(gaim_primitive_get_name_from_type(i));
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_show(item);
	}

	gtk_menu_set_active(GTK_MENU(menu), type);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(dropdown), menu);
	gtk_widget_show(menu);

	return dropdown;
}

static void
status_editor_add_columns(StatusEditor *dialog)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* Custom status column */
	renderer = gtk_cell_renderer_toggle_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(dialog->treeview),
						    -1, _("Custom status"),
						    renderer,
						    "active", STATUS_EDITOR_COLUMN_CUSTOM_STATUS,
						    NULL);
	column = gtk_tree_view_get_column(GTK_TREE_VIEW(dialog->treeview), 1);
	g_signal_connect(G_OBJECT(renderer), "toggled",
			 G_CALLBACK(status_editor_custom_status_cb), dialog);

	/* Screen Name column */
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_title(column, _("Screen Name"));
	gtk_tree_view_insert_column(GTK_TREE_VIEW(dialog->treeview), column, -1);
	gtk_tree_view_column_set_resizable(column, TRUE);

	/* Icon */
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_add_attribute(column, renderer, "pixbuf",
									   STATUS_EDITOR_COLUMN_ICON);

	/* Screen Name */
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "text",
									   STATUS_EDITOR_COLUMN_SCREENNAME);
}

static void
status_editor_set_account(GtkListStore *store, GaimAccount *account, GtkTreeIter *iter)
{
	GdkPixbuf *pixbuf;
	GdkPixbuf *scale;

	scale = NULL;

	pixbuf = create_prpl_icon(account);

	if (pixbuf != NULL)
	{
		scale = gdk_pixbuf_scale_simple(pixbuf, 16, 16, GDK_INTERP_BILINEAR);

		if (!gaim_account_is_connected(account))
			gdk_pixbuf_saturate_and_pixelate(scale, scale, 0.0, FALSE);
	}

	gtk_list_store_set(store, iter,
			STATUS_EDITOR_COLUMN_ICON, scale,
			STATUS_EDITOR_COLUMN_SCREENNAME, gaim_account_get_username(account),
			-1);

	if (pixbuf != NULL) g_object_unref(G_OBJECT(pixbuf));
	if (scale  != NULL) g_object_unref(G_OBJECT(scale));
}

static void
status_editor_add_account(StatusEditor *dialog, GaimAccount *account)
{
	GtkTreeIter iter;

	gtk_list_store_append(dialog->model, &iter);

	status_editor_set_account(dialog->model, account, &iter);
}

static void
status_editor_populate_list(StatusEditor *dialog)
{
	GList *l;

	gtk_list_store_clear(dialog->model);

	for (l = gaim_accounts_get_all(); l != NULL; l = l->next)
		status_editor_add_account(dialog, (GaimAccount *)l->data);
}

void
gaim_gtk_status_editor_show(GaimSavedStatus *status)
{
	StatusEditor *dialog;
	GtkSizeGroup *sg;
	GtkWidget *bbox;
	GtkWidget *button;
	GtkWidget *dbox;
	GtkWidget *disclosure;
	GtkWidget *dropdown;
	GtkWidget *entry;
	GtkWidget *frame;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *sw;
	GtkWidget *text;
	GtkWidget *toolbar;
	GtkWidget *vbox;
	GtkWidget *win;

	dialog = g_new0(StatusEditor, 1);

	if (status != NULL)
		dialog->original_title = g_strdup(gaim_savedstatus_get_title(status));

	dialog->window = win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_role(GTK_WINDOW(win), "status");
	gtk_window_set_title(GTK_WINDOW(win), _("Status"));
	gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(win), 12);

	g_signal_connect(G_OBJECT(win), "delete_event",
					 G_CALLBACK(status_editor_destroy_cb), dialog);

	/* Setup the vbox */
	vbox = gtk_vbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(win), vbox);
	gtk_widget_show(vbox);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	/* Title */
	hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new_with_mnemonic(_("_Title:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	gtk_size_group_add_widget(sg, label);

	entry = gtk_entry_new();
	dialog->title = GTK_ENTRY(entry);
	if (status != NULL)
		gtk_entry_set_text(GTK_ENTRY(entry), dialog->original_title);
	else
		gtk_entry_set_text(GTK_ENTRY(entry), _("Out of the office"));
	gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
	gtk_widget_show(entry);
	g_signal_connect(G_OBJECT(entry), "changed",
					 G_CALLBACK(editor_title_changed_cb), dialog);

	/* Status type */
	hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new_with_mnemonic(_("_Status:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	gtk_size_group_add_widget(sg, label);

	if (status != NULL)
		dropdown = create_status_type_menu(gaim_savedstatus_get_type(status));
	else
		dropdown = create_status_type_menu(GAIM_STATUS_AWAY);
	dialog->type = GTK_OPTION_MENU(dropdown);
	gtk_box_pack_start(GTK_BOX(hbox), dropdown, TRUE, TRUE, 0);
	gtk_widget_show(dropdown);

	/* Status message */
	hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new_with_mnemonic(_("_Message:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	gtk_size_group_add_widget(sg, label);

	frame = gaim_gtk_create_imhtml(TRUE, &text, &toolbar);
	dialog->message = GTK_IMHTML(text);
	gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, TRUE, 0);
	gtk_widget_show(frame);

	if ((status != NULL) && (gaim_savedstatus_get_message(status) != NULL))
		gtk_imhtml_append_text(GTK_IMHTML(text),
							   gaim_savedstatus_get_message(status), 0);

	/* Custom status message disclosure */
	disclosure = gaim_disclosure_new(_("Use a different status for some accounts"),
									 _("Use a different status for some accounts"));
	gtk_box_pack_start(GTK_BOX(vbox), disclosure, FALSE, FALSE, 0);
	gtk_widget_show(disclosure);

	/* Setup the box that the disclosure will cover */
	dbox = gtk_vbox_new(FALSE, 18);
	gtk_box_pack_start(GTK_BOX(vbox), dbox, FALSE, FALSE, 0);
	gaim_disclosure_set_container(GAIM_DISCLOSURE(disclosure), dbox);

	/* Custom status message treeview */
	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
								   GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
										GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(dbox), sw, TRUE, TRUE, 0);
	gtk_widget_show(sw);

	/* Create the list model */
	dialog->model = gtk_list_store_new(STATUS_EDITOR_NUM_COLUMNS,
									   G_TYPE_BOOLEAN,
									   GDK_TYPE_PIXBUF, G_TYPE_STRING);

	/* Create the treeview */
	dialog->treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(dialog->model));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(dialog->treeview), TRUE);
	gtk_widget_set_size_request(dialog->treeview, 400, 250);
	gtk_container_add(GTK_CONTAINER(sw), dialog->treeview);
	gtk_widget_show(dialog->treeview);

	/* Add columns */
	status_editor_add_columns(dialog);

	/* Populate list */
	status_editor_populate_list(dialog);

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
					 G_CALLBACK(status_editor_cancel_cb), dialog);

	/* Save button */
	button = gtk_button_new_from_stock(GTK_STOCK_SAVE);
	dialog->save_button = button;
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(status_editor_save_cb), dialog);

	gtk_widget_show(win);
}


/**************************************************************************
* GTK+ saved status glue
**************************************************************************/

void *
gaim_gtk_status_get_handle()
{
	static int handle;

	return &handle;
}

void
gaim_gtk_status_init(void)
{
	gaim_prefs_add_none("/gaim/gtk/status");
	gaim_prefs_add_none("/gaim/gtk/status/dialog");
	gaim_prefs_add_int("/gaim/gtk/status/dialog/width",  550);
	gaim_prefs_add_int("/gaim/gtk/status/dialog/height", 250);
}

void
gaim_gtk_status_uninit(void)
{
	gaim_gtk_status_window_hide();
}
