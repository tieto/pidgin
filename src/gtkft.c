/**
 * @file gtkft.c The GTK+ file transfer UI
 *
 * gaim
 *
 * Copyright (C) 2003, Christian Hammond <chipx86@gnupdate.org>
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
#include "gaim.h"
#include "prpl.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include "gtkcellrendererprogress.h"
#include "gaim-disclosure.h"

#define GAIM_GTKXFER(xfer) \
	(struct gaim_gtkxfer_ui_data *)(xfer)->ui_data

struct gaim_gtkxfer_dialog
{
	gboolean keep_open;
	gboolean auto_clear;

	gint num_transfers;

	struct gaim_xfer *selected_xfer;

	GtkWidget *window;
	GtkWidget *tree;
	GtkListStore *model;

	GtkWidget *disclosure;

	GtkWidget *table;

	GtkWidget *user_desc_label;
	GtkWidget *user_label;
	GtkWidget *filename_label;
	GtkWidget *status_label;
	GtkWidget *speed_label;
	GtkWidget *time_elapsed_label;
	GtkWidget *time_remaining_label;

	GtkWidget *progress;

	/* Buttons */
	GtkWidget *open_button;
	GtkWidget *pause_button;
	GtkWidget *resume_button;
	GtkWidget *remove_button;
	GtkWidget *stop_button;
};

struct gaim_gtkxfer_ui_data
{
	GtkWidget *filesel;
	GtkTreeIter iter;
	time_t start_time;

	char *name;
};

static struct gaim_gtkxfer_dialog *xfer_dialog = NULL;

enum
{
	COLUMN_STATUS = 0,
	COLUMN_PROGRESS,
	COLUMN_FILENAME,
	COLUMN_SIZE,
	COLUMN_REMAINING,
	COLUMN_DATA,
	NUM_COLUMNS
};

/**************************************************************************
 * Utility Functions
 **************************************************************************/
static char *
get_size_string(size_t size)
{
	static const char *size_str[4] = { "bytes", "KB", "MB", "GB" };
	float size_mag;
	int size_index = 0;

	if (size == -1) {
		return g_strdup(_("Calculating..."));
	}
	else if (size == 0) {
		return g_strdup(_("Unknown."));
	}
	else {
		size_mag = (float)size;

		while ((size_index < 4) && (size_mag > 1024)) {
			size_mag /= 1024;
			size_index++;
		}

		return g_strdup_printf("%.2f %s", size_mag, size_str[size_index]);
	}
}

static void
get_xfer_info_strings(struct gaim_xfer *xfer,
					  char **kbsec, char **time_elapsed,
					  char **time_remaining)
{
	struct gaim_gtkxfer_ui_data *data;
	double kb_sent, kb_rem;
	double kbps = 0.0;
	time_t elapsed, now;

	data = GAIM_GTKXFER(xfer);

	now = time(NULL);

	kb_sent = gaim_xfer_get_bytes_sent(xfer) / 1024.0;
	kb_rem  = gaim_xfer_get_bytes_remaining(xfer) / 1024.0;
	elapsed = (now - data->start_time);
	kbps    = (elapsed > 0 ? (kb_sent / elapsed) : 0);

	if (kbsec != NULL) {
		if (gaim_xfer_is_completed(xfer))
			*kbsec = g_strdup("");
		else
			*kbsec = g_strdup_printf(_("%.2f KB/s"), kbps);
	}

	if (time_elapsed != NULL) {
		int h, m, s;
		int secs_elapsed;

		secs_elapsed = now - data->start_time;

		h = secs_elapsed / 3600;
		m = (secs_elapsed % 3600) / 60;
		s = secs_elapsed % 60;

		*time_elapsed = g_strdup_printf("%d:%02d:%02d", h, m, s);
	}

	if (time_remaining != NULL) {
		if (gaim_xfer_get_size(xfer) == 0) {
			*time_remaining = g_strdup("Unknown");
		}
		else if (gaim_xfer_is_completed(xfer)) {
			*time_remaining = g_strdup("Finished");
		}
		else {
			int h, m, s;
			int secs_remaining;

			secs_remaining = (int)(kb_rem / kbps);

			h = secs_remaining / 3600;
			m = (secs_remaining % 3600) / 60;
			s = secs_remaining % 60;

			*time_remaining = g_strdup_printf("%d:%02d:%02d", h, m, s);
		}
	}
}

static void
update_detailed_info(struct gaim_gtkxfer_dialog *dialog,
					 struct gaim_xfer *xfer)
{
	struct gaim_gtkxfer_ui_data *data;
	char *kbsec, *time_elapsed, *time_remaining;
	char *status;

	if (dialog == NULL || xfer == NULL)
		return;

	data = GAIM_GTKXFER(xfer);

	get_xfer_info_strings(xfer, &kbsec, &time_elapsed, &time_remaining);

	status = g_strdup_printf("%ld of %ld",
							 (unsigned long)gaim_xfer_get_bytes_sent(xfer),
							 (unsigned long)gaim_xfer_get_size(xfer));

	if (gaim_xfer_get_size(xfer) >= 0 &&
		gaim_xfer_is_completed(xfer)) {

		GdkPixbuf *pixbuf = NULL;

		pixbuf = gtk_widget_render_icon(xfer_dialog->window,
										GAIM_STOCK_FILE_DONE,
										GTK_ICON_SIZE_MENU, NULL);

		gtk_list_store_set(GTK_LIST_STORE(xfer_dialog->model), &data->iter,
						   COLUMN_STATUS, pixbuf,
						   -1);

		g_object_unref(pixbuf);
	}

	if (gaim_xfer_get_type(xfer) == GAIM_XFER_RECEIVE)
		gtk_label_set_markup(GTK_LABEL(dialog->user_desc_label),
							 _("<b>Receiving From:</b>"));
	else
		gtk_label_set_markup(GTK_LABEL(dialog->user_desc_label),
							 _("<b>Sending To:</b>"));

	gtk_label_set_text(GTK_LABEL(dialog->user_label), xfer->who);

	gtk_label_set_text(GTK_LABEL(dialog->filename_label),
					   gaim_xfer_get_filename(xfer));

	gtk_label_set_text(GTK_LABEL(dialog->status_label), status);

	gtk_label_set_text(GTK_LABEL(dialog->speed_label), kbsec);
	gtk_label_set_text(GTK_LABEL(dialog->time_elapsed_label), time_elapsed);
	gtk_label_set_text(GTK_LABEL(dialog->time_remaining_label),
					   time_remaining);

	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(dialog->progress),
								  gaim_xfer_get_progress(xfer));

	g_free(kbsec);
	g_free(time_elapsed);
	g_free(time_remaining);
	g_free(status);
}

static void
update_buttons(struct gaim_gtkxfer_dialog *dialog, struct gaim_xfer *xfer)
{
	if (dialog->selected_xfer == NULL) {
		gtk_widget_set_sensitive(dialog->disclosure, FALSE);
		gtk_widget_set_sensitive(dialog->open_button, FALSE);
		gtk_widget_set_sensitive(dialog->pause_button, FALSE);
		gtk_widget_set_sensitive(dialog->resume_button, FALSE);
		gtk_widget_set_sensitive(dialog->stop_button, FALSE);

		gtk_widget_show(dialog->stop_button);
		gtk_widget_hide(dialog->remove_button);

		return;
	}

	if (dialog->selected_xfer != xfer)
		return;

	if (gaim_xfer_is_completed(xfer)) {
		gtk_widget_hide(dialog->stop_button);
		gtk_widget_show(dialog->remove_button);

		/* TODO: gtk_widget_set_sensitive(dialog->open_button, TRUE); */
		gtk_widget_set_sensitive(dialog->pause_button,  FALSE);
		gtk_widget_set_sensitive(dialog->resume_button, FALSE);

		gtk_widget_set_sensitive(dialog->remove_button, TRUE);
	}
	else {
		gtk_widget_show(dialog->stop_button);
		gtk_widget_hide(dialog->remove_button);

		gtk_widget_set_sensitive(dialog->open_button,  FALSE);

		/* TODO: If the transfer can pause, blah blah */
		gtk_widget_set_sensitive(dialog->pause_button,  FALSE);
		gtk_widget_set_sensitive(dialog->resume_button, FALSE);
		gtk_widget_set_sensitive(dialog->stop_button,   TRUE);
	}
}

static void
ensure_row_selected(struct gaim_gtkxfer_dialog *dialog)
{
	GtkTreeIter iter;
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->tree));

	if (gtk_tree_selection_get_selected(selection, NULL, &iter))
		return;

	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(dialog->model), &iter))
		gtk_tree_selection_select_iter(selection, &iter);
}

/**************************************************************************
 * Callbacks
 **************************************************************************/
static gint
delete_win_cb(GtkWidget *w, GdkEventAny *e, gpointer d)
{
	struct gaim_gtkxfer_dialog *dialog;

	dialog = (struct gaim_gtkxfer_dialog *)d;

	gaim_gtkxfer_dialog_hide(dialog);

	return TRUE;
}

static void
toggle_keep_open_cb(GtkWidget *w, struct gaim_gtkxfer_dialog *dialog)
{
	dialog->keep_open = !dialog->keep_open;
}

static void
toggle_clear_finished_cb(GtkWidget *w, struct gaim_gtkxfer_dialog *dialog)
{
	dialog->auto_clear = !dialog->auto_clear;
}

static void
selection_changed_cb(GtkTreeSelection *selection,
					 struct gaim_gtkxfer_dialog *dialog)
{
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		GValue val = {0, };
		struct gaim_xfer *xfer;

		gtk_widget_set_sensitive(dialog->disclosure, TRUE);

		gtk_tree_model_get_value(GTK_TREE_MODEL(dialog->model),
								 &iter, COLUMN_DATA, &val);

		xfer = g_value_get_pointer(&val);

		update_detailed_info(dialog, xfer);

		dialog->selected_xfer = xfer;
	}
	else {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->disclosure),
									 FALSE);

		gtk_widget_set_sensitive(dialog->disclosure, FALSE);

		dialog->selected_xfer = NULL;
	}

	update_buttons(dialog, xfer);
}

static void
open_button_cb(GtkButton *button, struct gaim_gtkxfer_dialog *dialog)
{
}

static void
pause_button_cb(GtkButton *button, struct gaim_gtkxfer_dialog *dialog)
{
}

static void
resume_button_cb(GtkButton *button, struct gaim_gtkxfer_dialog *dialog)
{
}

static void
remove_button_cb(GtkButton *button, struct gaim_gtkxfer_dialog *dialog)
{
	gaim_gtkxfer_dialog_remove_xfer(dialog, dialog->selected_xfer);
}

static void
stop_button_cb(GtkButton *button, struct gaim_gtkxfer_dialog *dialog)
{
	gaim_xfer_cancel(dialog->selected_xfer);
}

/**************************************************************************
 * Dialog Building Functions
 **************************************************************************/
static GtkWidget *
setup_tree(struct gaim_gtkxfer_dialog *dialog)
{
	GtkWidget *sw;
	GtkWidget *tree;
	GtkListStore *model;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;

	/* Create the scrolled window. */
	sw = gtk_scrolled_window_new(0, 0);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
										GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
								   GTK_POLICY_AUTOMATIC,
								   GTK_POLICY_ALWAYS);
	gtk_widget_show(sw);

	/* Build the tree model */
	/* Transfer type, Progress Bar, Filename, Size, Remaining */
	model = gtk_list_store_new(NUM_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_DOUBLE,
							   G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
							   G_TYPE_POINTER);
	dialog->model = model;

	/* Create the treeview */
	dialog->tree = tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(model));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree), TRUE);
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	/* gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE); */

	gtk_widget_show(tree);

	g_signal_connect(G_OBJECT(selection), "changed",
					 G_CALLBACK(selection_changed_cb), dialog);

	g_object_unref(G_OBJECT(model));


	/* Columns */

	/* Transfer Type column */
	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes(NULL, renderer,
				"pixbuf", COLUMN_STATUS, NULL);
	gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(column),
									GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(column), 25);
	gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	/* Progress bar column */
	renderer = gtk_cell_renderer_progress_new();
	column = gtk_tree_view_column_new_with_attributes(_("Progress"), renderer,
				"percentage", COLUMN_PROGRESS, NULL);
	gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	/* Filename column */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Filename"), renderer,
				"text", COLUMN_FILENAME, NULL);
	gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	/* File Size column */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Size"), renderer,
				"text", COLUMN_SIZE, NULL);
	gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	/* Bytes Remaining column */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Remaining"),
				renderer, "text", COLUMN_REMAINING, NULL);
	gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	gtk_tree_view_columns_autosize(GTK_TREE_VIEW(tree));

	gtk_container_add(GTK_CONTAINER(sw), tree);
	gtk_widget_show(tree);

	return sw;
}

static GtkWidget *
make_info_table(struct gaim_gtkxfer_dialog *dialog)
{
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *sep;
	int i;

	struct
	{
		GtkWidget **desc_label;
		GtkWidget **val_label;
		const char *desc;

	} labels[] =
	{
		{ &dialog->user_desc_label, &dialog->user_label, NULL },
		{ &label, &dialog->filename_label,       _("Filename:") },
		{ &label, &dialog->status_label,         _("Status:") },
		{ &label, &dialog->speed_label,          _("Speed:") },
		{ &label, &dialog->time_elapsed_label,   _("Time Elapsed:") },
		{ &label, &dialog->time_remaining_label, _("Time Remaining:") }
	};

	/* Setup the initial table */
	dialog->table = table = gtk_table_new(8, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 6);
	gtk_table_set_col_spacings(GTK_TABLE(table), 6);

	/* Setup the labels */
	for (i = 0; i < sizeof(labels) / sizeof(*labels); i++) {
		GtkWidget *label;
		char buf[256];

		printf("Adding %s\n", labels[i].desc);
		g_snprintf(buf, sizeof(buf), "<b>%s</b>", labels[i].desc);

		*labels[i].desc_label = label = gtk_label_new(NULL);
		gtk_label_set_markup(GTK_LABEL(label), buf);
		gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach(GTK_TABLE(table), label, 0, 1, i, i + 1,
						 GTK_FILL, 0, 0, 0);
		gtk_widget_show(label);

		*labels[i].val_label = label = gtk_label_new(NULL);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach(GTK_TABLE(table), label, 1, 2, i, i + 1,
						 GTK_FILL | GTK_EXPAND, 0, 0, 0);
		gtk_widget_show(label);
	}

	/* Setup the progress bar */
	dialog->progress = gtk_progress_bar_new();
	gtk_table_attach(GTK_TABLE(table), dialog->progress, 0, 2, 6, 7,
					 GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_show(dialog->progress);

	sep = gtk_hseparator_new();
	gtk_table_attach(GTK_TABLE(table), sep, 0, 2, 7, 8,
					 GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_show(sep);

	return table;
}

struct gaim_gtkxfer_dialog *
gaim_gtkxfer_dialog_new(void)
{
	struct gaim_gtkxfer_dialog *dialog;
	GtkWidget *window;
	GtkWidget *vbox1, *vbox2;
	GtkWidget *bbox;
	GtkWidget *sw;
	GtkWidget *sep;
	GtkWidget *button;
	GtkWidget *disclosure;
	GtkWidget *table;
	GtkWidget *checkbox;

	dialog = g_new0(struct gaim_gtkxfer_dialog, 1);
	dialog->keep_open  = FALSE;
	dialog->auto_clear = TRUE;

	/* Create the window. */
	dialog->window = window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_role(GTK_WINDOW(window), "file transfer");
	gtk_window_set_title(GTK_WINDOW(window), _("File Transfers"));
#if 0
	gtk_window_set_policy(GTK_WINDOW(window), TRUE, TRUE, TRUE);
	gtk_window_set_default_size(GTK_WINDOW(window), 390, 400);
#endif
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
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
	gtk_box_pack_start(GTK_BOX(vbox1), vbox2, TRUE, TRUE, 0);
	gtk_widget_show(vbox2);

	/* Setup the listbox */
	sw = setup_tree(dialog);
	gtk_box_pack_start(GTK_BOX(vbox2), sw, TRUE, TRUE, 0);

	/* "Keep the dialog open" */
	checkbox = gtk_check_button_new_with_mnemonic(
			_("_Keep the dialog open"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox),
								 dialog->keep_open);
	g_signal_connect(G_OBJECT(checkbox), "toggled",
					 G_CALLBACK(toggle_keep_open_cb), dialog);
	gtk_box_pack_start(GTK_BOX(vbox2), checkbox, FALSE, FALSE, 0);
	gtk_widget_show(checkbox);

	/* "Clear finished transfers" */
	checkbox = gtk_check_button_new_with_mnemonic(
			_("_Clear finished transfers"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox),
								 dialog->auto_clear);
	g_signal_connect(G_OBJECT(checkbox), "toggled",
					 G_CALLBACK(toggle_clear_finished_cb), dialog);
	gtk_box_pack_start(GTK_BOX(vbox2), checkbox, FALSE, FALSE, 0);
	gtk_widget_show(checkbox);

	/* "Download Details" arrow */
	disclosure = gaim_disclosure_new(_("Show download details"),
									 _("Hide download details"));
	dialog->disclosure = disclosure;
	gtk_box_pack_start(GTK_BOX(vbox2), disclosure, FALSE, FALSE, 0);
	gtk_widget_show(disclosure);

	gtk_widget_set_sensitive(disclosure, FALSE);

#if 0
	g_signal_connect(G_OBJECT(disclosure), "toggled",
					 G_CALLBACK(toggle_details_cb), dialog);
#endif

	/* Separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox2), sep, FALSE, FALSE, 0);
	gtk_widget_show(sep);

	/* The table of information. */
	table = make_info_table(dialog);
	gtk_box_pack_start(GTK_BOX(vbox2), table, TRUE, TRUE, 0);

	/* Setup the disclosure for the table. */
	gaim_disclosure_set_container(GAIM_DISCLOSURE(disclosure), table);

	/* Now the button box for the buttons */
	bbox = gtk_hbutton_box_new();
	gtk_box_set_spacing(GTK_BOX(bbox), 6);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_box_pack_end(GTK_BOX(vbox1), bbox, FALSE, TRUE, 0);
	gtk_widget_show(bbox);

	/* Open button */
	button = gtk_button_new_from_stock(GTK_STOCK_OPEN);
	gtk_widget_set_sensitive(button, FALSE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);
	dialog->open_button = button;

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(open_button_cb), dialog);

	/* Pause button */
	button = gtk_button_new_with_mnemonic(_("_Pause"));
	gtk_widget_set_sensitive(button, FALSE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);
	dialog->pause_button = button;

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(pause_button_cb), dialog);
	
	/* Resume button */
	button = gtk_button_new_with_mnemonic(_("_Resume"));
	gtk_widget_set_sensitive(button, FALSE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);
	dialog->resume_button = button;

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(resume_button_cb), dialog);

	/* Remove button */
	button = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_hide(button);
	dialog->remove_button = button;

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(remove_button_cb), dialog);

	/* Stop button */
	button = gtk_button_new_from_stock(GTK_STOCK_STOP);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);
	gtk_widget_set_sensitive(button, FALSE);
	dialog->stop_button = button;

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(stop_button_cb), dialog);

	return dialog;
}

void
gaim_gtkxfer_dialog_destroy(struct gaim_gtkxfer_dialog *dialog)
{
	if (dialog == NULL)
		return;

	gtk_widget_destroy(dialog->window);

	g_free(dialog);
}

void
gaim_gtkxfer_dialog_show(struct gaim_gtkxfer_dialog *dialog)
{
	if (dialog == NULL)
		return;

	gtk_widget_show(dialog->window);
}

void
gaim_gtkxfer_dialog_hide(struct gaim_gtkxfer_dialog *dialog)
{
	if (dialog == NULL)
		return;

	gtk_widget_hide(dialog->window);
}

void
gaim_gtkxfer_dialog_add_xfer(struct gaim_gtkxfer_dialog *dialog,
							 struct gaim_xfer *xfer)
{
	struct gaim_gtkxfer_ui_data *data;
	GaimXferType type;
	GdkPixbuf *pixbuf;
	char *size_str, *remaining_str;

	if (dialog == NULL || xfer == NULL)
		return;

	data = GAIM_GTKXFER(xfer);

	gaim_gtkxfer_dialog_show(dialog);

	data->start_time = time(NULL);

	type = gaim_xfer_get_type(xfer);

	size_str      = get_size_string(gaim_xfer_get_size(xfer));
	remaining_str = get_size_string(gaim_xfer_get_bytes_remaining(xfer));

	pixbuf = gtk_widget_render_icon(dialog->window,
									(type == GAIM_XFER_RECEIVE
									 ? GAIM_STOCK_DOWNLOAD
									 : GAIM_STOCK_UPLOAD),
									GTK_ICON_SIZE_MENU, NULL);

	gtk_list_store_append(dialog->model, &data->iter);
	gtk_list_store_set(dialog->model, &data->iter,
					   COLUMN_STATUS, pixbuf,
					   COLUMN_PROGRESS, 0.0,
					   COLUMN_FILENAME, gaim_xfer_get_filename(xfer),
					   COLUMN_SIZE, size_str,
					   COLUMN_REMAINING, remaining_str,
					   COLUMN_DATA, xfer,
					   -1);

	gtk_tree_view_columns_autosize(GTK_TREE_VIEW(dialog->tree));

	g_object_unref(pixbuf);

	g_free(size_str);
	g_free(remaining_str);

	dialog->num_transfers++;

	ensure_row_selected(dialog);
}

void
gaim_gtkxfer_dialog_remove_xfer(struct gaim_gtkxfer_dialog *dialog,
								struct gaim_xfer *xfer)
{
	struct gaim_gtkxfer_ui_data *data;

	if (dialog == NULL || xfer == NULL)
		return;

	data = GAIM_GTKXFER(xfer);

	if (data == NULL)
		return;

	gtk_list_store_remove(GTK_LIST_STORE(dialog->model), &data->iter);

	g_free(data->name);
	g_free(data);

	xfer->ui_data = NULL;

	dialog->num_transfers--;

	if (dialog->num_transfers == 0 && !dialog->keep_open)
		gaim_gtkxfer_dialog_hide(dialog);
	else
		ensure_row_selected(dialog);
}

void
gaim_gtkxfer_dialog_cancel_xfer(struct gaim_gtkxfer_dialog *dialog,
								struct gaim_xfer *xfer)
{
	struct gaim_gtkxfer_ui_data *data;
#if 0
	GdkPixbuf *pixbuf;
#endif

	if (dialog == NULL || xfer == NULL)
		return;

	data = GAIM_GTKXFER(xfer);

	if (data == NULL)
		return;

	gtk_list_store_remove(GTK_LIST_STORE(dialog->model), &data->iter);

	g_free(data->name);
	g_free(data);

	xfer->ui_data = NULL;

	dialog->num_transfers--;

	if (dialog->num_transfers == 0 && !dialog->keep_open)
		gaim_gtkxfer_dialog_hide(dialog);

#if 0
	data = GAIM_GTKXFER(xfer);

	pixbuf = gtk_widget_render_icon(dialog->window,
									GAIM_STOCK_FILE_CANCELED,
									GTK_ICON_SIZE_MENU, NULL);

	gtk_list_store_set(dialog->model, &data->iter,
					   COLUMN_STATUS, pixbuf,
					   -1);

	g_object_unref(pixbuf);
#endif
}

void
gaim_gtkxfer_dialog_update_xfer(struct gaim_gtkxfer_dialog *dialog,
								struct gaim_xfer *xfer)
{
	struct gaim_gtkxfer_ui_data *data;
	char *size_str, *remaining_str;
	GtkTreeSelection *selection;

	if (dialog == NULL || xfer == NULL)
		return;

	data = GAIM_GTKXFER(xfer);

	size_str      = get_size_string(gaim_xfer_get_size(xfer));
	remaining_str = get_size_string(gaim_xfer_get_bytes_remaining(xfer));

	gtk_list_store_set(xfer_dialog->model, &data->iter,
					   COLUMN_PROGRESS, gaim_xfer_get_progress(xfer),
					   COLUMN_SIZE, size_str,
					   COLUMN_REMAINING, remaining_str,
					   -1);

	if (gaim_xfer_is_completed(xfer)) {
		GdkPixbuf *pixbuf;

		pixbuf = gtk_widget_render_icon(dialog->window,
										GAIM_STOCK_FILE_DONE,
										GTK_ICON_SIZE_MENU, NULL);

		gtk_list_store_set(GTK_LIST_STORE(xfer_dialog->model), &data->iter,
						   COLUMN_STATUS, pixbuf,
						   COLUMN_REMAINING, "Finished",
						   -1);

		g_object_unref(pixbuf);
	}

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(xfer_dialog->tree));

	if (xfer == dialog->selected_xfer)
		update_detailed_info(xfer_dialog, xfer);

	if (gaim_xfer_is_completed(xfer) && dialog->auto_clear)
		gaim_gtkxfer_dialog_remove_xfer(dialog, xfer);
	else
		update_buttons(dialog, xfer);
}

/**************************************************************************
 * File Transfer UI Ops
 **************************************************************************/
static void
gaim_gtkxfer_destroy(struct gaim_xfer *xfer)
{
	gaim_gtkxfer_dialog_remove_xfer(xfer_dialog, xfer);
}

static gboolean
choose_file_close_cb(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	gaim_xfer_request_denied((struct gaim_xfer *)user_data);

	return FALSE;
}

static void
choose_file_cancel_cb(GtkButton *button, gpointer user_data)
{
	struct gaim_xfer *xfer = (struct gaim_xfer *)user_data;
	struct gaim_gtkxfer_ui_data *data;

	data = GAIM_GTKXFER(xfer);

	gaim_xfer_request_denied(xfer);

	gtk_widget_destroy(data->filesel);
	data->filesel = NULL;
}

static int
do_overwrite_cb(struct gaim_xfer *xfer)
{
	struct gaim_gtkxfer_ui_data *data;
	
	data = GAIM_GTKXFER(xfer);

	gaim_xfer_request_accepted(xfer, data->name);

	/*
	 * No, we don't want to free data->name. gaim_xfer_request_accepted
	 * will deal with it.
	 */
	data->name = NULL;

	return 0;
}

static int
dont_overwrite_cb(struct gaim_xfer *xfer)
{
	struct gaim_gtkxfer_ui_data *data;
	
	data = GAIM_GTKXFER(xfer);

	g_free(data->name);
	data->name = NULL;

	gaim_xfer_request_denied(xfer);

	return 0;
}

static void
choose_file_ok_cb(GtkButton *button, gpointer user_data)
{
	struct gaim_xfer *xfer;
	struct gaim_gtkxfer_ui_data *data;
	struct stat st;
	const char *name;

	xfer = (struct gaim_xfer *)user_data;
	data = GAIM_GTKXFER(xfer);

	name = gtk_file_selection_get_filename(GTK_FILE_SELECTION(data->filesel));

	if (stat(name, &st) == 0) {
		if (S_ISDIR(st.st_mode)) {
			/* XXX */
			gaim_xfer_request_denied(xfer);
		}
		else if (gaim_xfer_get_type(xfer) == GAIM_XFER_RECEIVE) {
			data->name = g_strdup(name);

			do_ask_dialog(_("That file already exists. "
							"Would you like to overwrite it?"),
						  NULL, xfer,
						  _("Yes"), do_overwrite_cb,
						  _("No"), dont_overwrite_cb,
						  NULL, FALSE);
		}
		else {
			gaim_xfer_request_accepted(xfer, g_strdup(name));
		}
	}
	else {
		/* File not found. */
		if (gaim_xfer_get_type(xfer) == GAIM_XFER_RECEIVE) {
			gaim_xfer_request_accepted(xfer, g_strdup(name));
		}
		else {
			do_error_dialog(_("That file does not exist."),
							NULL, GAIM_ERROR);

			gaim_xfer_request_denied(xfer);
		}
	}

	gtk_widget_destroy(data->filesel);
	data->filesel = NULL;
}

static int
choose_file(struct gaim_xfer *xfer)
{
	char *cur_dir, *init_str;
	struct gaim_gtkxfer_ui_data *data;

	cur_dir = g_get_current_dir();

	/* This is where we're setting xfer->ui_data for the first time. */
	data = g_malloc0(sizeof(struct gaim_gtkxfer_ui_data));
	xfer->ui_data = data;

	if (gaim_xfer_get_type(xfer) == GAIM_XFER_SEND)
		data->filesel = gtk_file_selection_new(_("Gaim - Open..."));
	else
		data->filesel = gtk_file_selection_new(_("Gaim - Save As..."));

	if (gaim_xfer_get_filename(xfer) == NULL)
		init_str = g_strdup(cur_dir);
	else
		init_str = g_build_filename(cur_dir, gaim_xfer_get_filename(xfer),
									NULL);

	g_free(cur_dir);

	gtk_file_selection_set_filename(GTK_FILE_SELECTION(data->filesel),
									init_str);

	g_free(init_str);

	g_signal_connect(G_OBJECT(data->filesel), "delete_event",
					 G_CALLBACK(choose_file_close_cb), xfer);
	g_signal_connect(G_OBJECT(GTK_FILE_SELECTION(data->filesel)->cancel_button),
					 "clicked",
					 G_CALLBACK(choose_file_cancel_cb), xfer);
	g_signal_connect(G_OBJECT(GTK_FILE_SELECTION(data->filesel)->ok_button),
					 "clicked",
					 G_CALLBACK(choose_file_ok_cb), xfer);

	gtk_widget_show(data->filesel);

	return 0;
}

static int
cancel_recv_cb(struct gaim_xfer *xfer)
{
	gaim_xfer_request_denied(xfer);

	return 0;
}

static void
gaim_gtkxfer_ask_recv(struct gaim_xfer *xfer)
{
	char *buf, *size_buf;
	size_t size;

	size = gaim_xfer_get_size(xfer);

	size_buf = get_size_string(size);

	buf = g_strdup_printf(_("%s wants to send you %s (%s)"),
						  xfer->who, gaim_xfer_get_filename(xfer), size_buf);

	g_free(size_buf);

	do_ask_dialog(buf, NULL, xfer,
				  _("Accept"), choose_file,
				  _("Cancel"), cancel_recv_cb,
				  NULL, FALSE);

	g_free(buf);
}

static void
gaim_gtkxfer_request_file(struct gaim_xfer *xfer)
{
	if (gaim_xfer_get_type(xfer) == GAIM_XFER_RECEIVE)
	{
		gaim_gtkxfer_ask_recv(xfer);
	}
	else
	{
		choose_file(xfer);
	}
}

static void
gaim_gtkxfer_ask_cancel(struct gaim_xfer *xfer)
{
}

static void
gaim_gtkxfer_add_xfer(struct gaim_xfer *xfer)
{
	if (xfer_dialog == NULL)
		xfer_dialog = gaim_gtkxfer_dialog_new();

	gaim_gtkxfer_dialog_add_xfer(xfer_dialog, xfer);
}

static void
gaim_gtkxfer_update_progress(struct gaim_xfer *xfer, double percent)
{
	gaim_gtkxfer_dialog_update_xfer(xfer_dialog, xfer);

	/* See if it's removed. */
	/* XXX - This caused some bad stuff, and I don't see a point to it */
/*	if (xfer->ui_data == NULL)
		gaim_xfer_destroy(xfer); */
}

static void
gaim_gtkxfer_cancel(struct gaim_xfer *xfer)
{
	gaim_gtkxfer_dialog_cancel_xfer(xfer_dialog, xfer);

	/* See if it's removed. */
	/* XXX - This caused some looping, and I don't see a point to it */
/*	if (xfer->ui_data == NULL)
		gaim_xfer_destroy(xfer); */
}

struct gaim_xfer_ui_ops ops =
{
	gaim_gtkxfer_destroy,
	gaim_gtkxfer_request_file,
	gaim_gtkxfer_ask_cancel,
	gaim_gtkxfer_add_xfer,
	gaim_gtkxfer_update_progress,
	gaim_gtkxfer_cancel
};

/**************************************************************************
 * GTK+ File Transfer API
 **************************************************************************/
void
gaim_set_gtkxfer_dialog(struct gaim_gtkxfer_dialog *dialog)
{
	xfer_dialog = dialog;
}

struct gaim_gtkxfer_dialog *
gaim_get_gtkxfer_dialog(void)
{
	return xfer_dialog;
}

struct gaim_xfer_ui_ops *
gaim_get_gtk_xfer_ui_ops(void)
{
	return &ops;
}
