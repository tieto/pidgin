/**
 * @file gtkft.c GTK+ File Transfer UI
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
#include "gtkgaim.h"

#include "debug.h"
#include "notify.h"
#include "ft.h"
#include "prpl.h"
#include "util.h"

#include "gtkcellrendererprogress.h"
#include "gtkft.h"
#include "prefs.h"
#include "gtkexpander.h"
#include "gaimstock.h"
#include "gtkutils.h"

#define GAIM_GTKXFER(xfer) \
	(GaimGtkXferUiData *)(xfer)->ui_data

struct _GaimGtkXferDialog
{
	gboolean keep_open;
	gboolean auto_clear;

	gint num_transfers;

	GaimXfer *selected_xfer;

	GtkWidget *window;
	GtkWidget *tree;
	GtkListStore *model;

	GtkWidget *expander;

	GtkWidget *table;

	GtkWidget *local_user_desc_label;
	GtkWidget *local_user_label;
	GtkWidget *remote_user_desc_label;
	GtkWidget *remote_user_label;
	GtkWidget *protocol_label;
	GtkWidget *filename_label;
	GtkWidget *localfile_label;
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
	GtkWidget *close_button;
};

typedef struct
{
	GtkTreeIter iter;
	time_t last_updated_time;
	gboolean in_list;

	char *name;

} GaimGtkXferUiData;

static GaimGtkXferDialog *xfer_dialog = NULL;

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
static void
get_xfer_info_strings(GaimXfer *xfer, char **kbsec, char **time_elapsed,
					  char **time_remaining)
{
	GaimGtkXferUiData *data;
	double kb_sent, kb_rem;
	double kbps = 0.0;
	time_t elapsed, now;

	data = GAIM_GTKXFER(xfer);

	if (xfer->end_time != 0)
		now = xfer->end_time;
	else
		now = time(NULL);

	kb_sent = gaim_xfer_get_bytes_sent(xfer) / 1024.0;
	kb_rem  = gaim_xfer_get_bytes_remaining(xfer) / 1024.0;
	elapsed = (xfer->start_time > 0 ? now - xfer->start_time : 0);
	kbps    = (elapsed > 0 ? (kb_sent / elapsed) : 0);

	if (kbsec != NULL) {
		*kbsec = g_strdup_printf(_("%.2f KB/s"), kbps);
	}

	if (time_elapsed != NULL)
	{
		int h, m, s;
		int secs_elapsed;

		if (xfer->start_time > 0)
		{
			secs_elapsed = now - xfer->start_time;

			h = secs_elapsed / 3600;
			m = (secs_elapsed % 3600) / 60;
			s = secs_elapsed % 60;

			*time_elapsed = g_strdup_printf("%d:%02d:%02d", h, m, s);
		}
		else
		{
			*time_elapsed = g_strdup(_("Not started"));
		}
	}

	if (time_remaining != NULL) {
		if (gaim_xfer_get_size(xfer) == 0) {
			*time_remaining = g_strdup(_("Unknown"));
		}
		else if (gaim_xfer_is_completed(xfer)) {
			*time_remaining = g_strdup(_("Finished"));
		}
		else if (gaim_xfer_is_canceled(xfer)) {
			*time_remaining = g_strdup(_("Canceled"));
		}
		else if (kb_sent <= 0) {
			*time_remaining = g_strdup(_("Waiting for transfer to begin"));
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
update_title_progress(GaimGtkXferDialog *dialog)
{
	gboolean valid;
	GtkTreeIter iter;
	int num_active_xfers = 0;
	guint64 total_bytes_xferred = 0;
	guint64 total_file_size = 0;

	if (dialog->window == NULL)
		return;

	valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(dialog->model), &iter);

	/* Find all active transfers */
	while (valid) {
		GValue val;
		GaimXfer *xfer = NULL;

		val.g_type = 0;
		gtk_tree_model_get_value(GTK_TREE_MODEL(dialog->model),
				&iter, COLUMN_DATA, &val);

		xfer = g_value_get_pointer(&val);
		if (gaim_xfer_get_status(xfer) == GAIM_XFER_STATUS_STARTED) {
			num_active_xfers++;
			total_bytes_xferred += gaim_xfer_get_bytes_sent(xfer);
			total_file_size += gaim_xfer_get_size(xfer);
		}

		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(dialog->model), &iter);
	}

	/* Update the title */
	if (num_active_xfers > 0)
	{
		gchar *title;
		int total_pct = 0;

		if (total_file_size > 0) {
			total_pct = 100 * total_bytes_xferred / total_file_size;
		}

		title = g_strdup_printf(_("File Transfers - %d%% of %d files"),
				total_pct, num_active_xfers);
		gtk_window_set_title(GTK_WINDOW(dialog->window), title);
		g_free(title);
	} else {
		gtk_window_set_title(GTK_WINDOW(dialog->window), _("File Transfers"));
	}
}

static void
update_detailed_info(GaimGtkXferDialog *dialog, GaimXfer *xfer)
{
	GaimGtkXferUiData *data;
	char *kbsec, *time_elapsed, *time_remaining;
	char *status, *utf8;

	if (dialog == NULL || xfer == NULL)
		return;

	data = GAIM_GTKXFER(xfer);

	get_xfer_info_strings(xfer, &kbsec, &time_elapsed, &time_remaining);

	status = g_strdup_printf("%ld%% (%ld of %ld bytes)",
							 (unsigned long)(gaim_xfer_get_progress(xfer)*100),
							 (unsigned long)gaim_xfer_get_bytes_sent(xfer),
							 (unsigned long)gaim_xfer_get_size(xfer));

	if (gaim_xfer_is_completed(xfer)) {

		GdkPixbuf *pixbuf = NULL;

		pixbuf = gtk_widget_render_icon(xfer_dialog->window,
										GAIM_STOCK_FILE_DONE,
										GTK_ICON_SIZE_MENU, NULL);

		gtk_list_store_set(GTK_LIST_STORE(xfer_dialog->model), &data->iter,
						   COLUMN_STATUS, pixbuf,
						   -1);

		g_object_unref(pixbuf);
	}

	if (gaim_xfer_get_type(xfer) == GAIM_XFER_RECEIVE) {
		gtk_label_set_markup(GTK_LABEL(dialog->local_user_desc_label),
							 _("<b>Receiving As:</b>"));
		gtk_label_set_markup(GTK_LABEL(dialog->remote_user_desc_label),
							 _("<b>Receiving From:</b>"));
	}
	else {
		gtk_label_set_markup(GTK_LABEL(dialog->remote_user_desc_label),
							 _("<b>Sending To:</b>"));
		gtk_label_set_markup(GTK_LABEL(dialog->local_user_desc_label),
							 _("<b>Sending As:</b>"));
	}

	gtk_label_set_text(GTK_LABEL(dialog->local_user_label),
								 gaim_account_get_username(xfer->account));
	gtk_label_set_text(GTK_LABEL(dialog->remote_user_label), xfer->who);
	gtk_label_set_text(GTK_LABEL(dialog->protocol_label),
								 gaim_account_get_protocol_name(xfer->account));

	if (gaim_xfer_get_type(xfer) == GAIM_XFER_RECEIVE) {
		gtk_label_set_text(GTK_LABEL(dialog->filename_label),
					   gaim_xfer_get_filename(xfer));
	} else {
		char *tmp;

		tmp = g_path_get_basename(gaim_xfer_get_local_filename(xfer));
		utf8 = g_filename_to_utf8(tmp, -1, NULL, NULL, NULL);
		g_free(tmp);

		gtk_label_set_text(GTK_LABEL(dialog->filename_label), utf8);
		g_free(utf8);
	}

	utf8 = g_filename_to_utf8((gaim_xfer_get_local_filename(xfer)), -1, NULL, NULL, NULL);
	gtk_label_set_text(GTK_LABEL(dialog->localfile_label), utf8);
	g_free(utf8);

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
update_buttons(GaimGtkXferDialog *dialog, GaimXfer *xfer)
{
	if (dialog->selected_xfer == NULL) {
		gtk_widget_set_sensitive(dialog->expander, FALSE);
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

#ifdef _WIN32
		/* If using Win32... */
		if (gaim_xfer_get_type(xfer) == GAIM_XFER_RECEIVE) {
			gtk_widget_set_sensitive(dialog->open_button, TRUE);
		} else {
			gtk_widget_set_sensitive(dialog->open_button, FALSE);
		}
#else
		if (gaim_xfer_get_type(xfer) == GAIM_XFER_RECEIVE) {
			gtk_widget_set_sensitive(dialog->open_button, TRUE);
		} else {
			gtk_widget_set_sensitive (dialog->open_button, FALSE);
		}
#endif
		gtk_widget_set_sensitive(dialog->pause_button,  FALSE);
		gtk_widget_set_sensitive(dialog->resume_button, FALSE);

		gtk_widget_set_sensitive(dialog->remove_button, TRUE);
	} else if (gaim_xfer_is_canceled(xfer)) {
		gtk_widget_hide(dialog->stop_button);
		gtk_widget_show(dialog->remove_button);

		gtk_widget_set_sensitive(dialog->open_button,  FALSE);
		gtk_widget_set_sensitive(dialog->pause_button,  FALSE);
		gtk_widget_set_sensitive(dialog->resume_button, FALSE);

		gtk_widget_set_sensitive(dialog->remove_button, TRUE);
	} else {
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
ensure_row_selected(GaimGtkXferDialog *dialog)
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
	GaimGtkXferDialog *dialog;

	dialog = (GaimGtkXferDialog *)d;

	gaim_gtkxfer_dialog_hide(dialog);

	return TRUE;
}

static void
toggle_keep_open_cb(GtkWidget *w, GaimGtkXferDialog *dialog)
{
	dialog->keep_open = !dialog->keep_open;
	gaim_prefs_set_bool("/gaim/gtk/filetransfer/keep_open",
						dialog->keep_open);
}

static void
toggle_clear_finished_cb(GtkWidget *w, GaimGtkXferDialog *dialog)
{
	dialog->auto_clear = !dialog->auto_clear;
	gaim_prefs_set_bool("/gaim/gtk/filetransfer/clear_finished",
						dialog->auto_clear);
}

static void
selection_changed_cb(GtkTreeSelection *selection, GaimGtkXferDialog *dialog)
{
	GtkTreeIter iter;
	GaimXfer *xfer = NULL;

	if (gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		GValue val;

		gtk_widget_set_sensitive(dialog->expander, TRUE);

		val.g_type = 0;
		gtk_tree_model_get_value(GTK_TREE_MODEL(dialog->model),
								 &iter, COLUMN_DATA, &val);

		xfer = g_value_get_pointer(&val);

		update_detailed_info(dialog, xfer);

		dialog->selected_xfer = xfer;
	}
	else {
		gtk_expander_set_expanded(GTK_EXPANDER(dialog->expander),
									 FALSE);

		gtk_widget_set_sensitive(dialog->expander, FALSE);

		dialog->selected_xfer = NULL;
	}

	update_buttons(dialog, xfer);
}

static void
open_button_cb(GtkButton *button, GaimGtkXferDialog *dialog)
{
#ifdef _WIN32
	/* If using Win32... */
	int code;
	if (G_WIN32_HAVE_WIDECHAR_API ()) {
		wchar_t *wc_filename = g_utf8_to_utf16(
				gaim_xfer_get_local_filename(
					dialog->selected_xfer),
				-1, NULL, NULL, NULL);

		code = (int) ShellExecuteW(NULL, NULL, wc_filename, NULL, NULL,
				SW_SHOW);

		g_free(wc_filename);
	} else {
		char *l_filename = g_locale_from_utf8(
				gaim_xfer_get_local_filename(
					dialog->selected_xfer),
				-1, NULL, NULL, NULL);

		code = (int) ShellExecuteA(NULL, NULL, l_filename, NULL, NULL,
				SW_SHOW);

		g_free(l_filename);
	}

	if (code == SE_ERR_ASSOCINCOMPLETE || code == SE_ERR_NOASSOC)
	{
		gaim_notify_error(dialog, NULL,
				_("There is no application configured to open this type of file."), NULL);
	}
	else if (code < 32)
	{
		gaim_notify_error(dialog, NULL,
				_("An error occurred while opening the file."), NULL);
		gaim_debug_warning("ft", "filename: %s; code: %d\n",
				gaim_xfer_get_local_filename(dialog->selected_xfer), code);
	}
#else
	const char *filename = gaim_xfer_get_local_filename(dialog->selected_xfer);
	char *command = NULL;
	char *tmp = NULL;
	GError *error = NULL;

	if (gaim_running_gnome())
	{
		char *escaped = g_shell_quote(filename);
		command = g_strdup_printf("gnome-open %s", escaped);
		g_free(escaped);
	}
	else if (gaim_running_kde())
	{
		char *escaped = g_shell_quote(filename);

		if (gaim_str_has_suffix(filename, ".desktop"))
			command = g_strdup_printf("kfmclient openURL %s 'text/plain'", escaped);
		else
			command = g_strdup_printf("kfmclient openURL %s", escaped);
		g_free(escaped);
	}
	else
	{
		gaim_notify_uri(NULL, filename);
		return;
	}

	if (gaim_program_is_valid(command))
	{
		gint exit_status;
		if (!g_spawn_command_line_sync(command, NULL, NULL, &exit_status, &error))
		{
			tmp = g_strdup_printf(_("Error launching %s: %s"),
							gaim_xfer_get_local_filename(dialog->selected_xfer),
							error->message);
			gaim_notify_error(dialog, NULL, _("Unable to open file."), tmp);
			g_free(tmp);
			g_error_free(error);
		}
		if (exit_status != 0)
		{
			char *primary = g_strdup_printf(_("Error running %s"), command);
			char *secondary = g_strdup_printf(_("Process returned error code %d"),
									exit_status);
			gaim_notify_error(dialog, NULL, primary, secondary);
			g_free(tmp);
		}
	}
#endif
}

static void
pause_button_cb(GtkButton *button, GaimGtkXferDialog *dialog)
{
}

static void
resume_button_cb(GtkButton *button, GaimGtkXferDialog *dialog)
{
}

static void
remove_button_cb(GtkButton *button, GaimGtkXferDialog *dialog)
{
	gaim_gtkxfer_dialog_remove_xfer(dialog, dialog->selected_xfer);
}

static void
stop_button_cb(GtkButton *button, GaimGtkXferDialog *dialog)
{
	gaim_xfer_cancel_local(dialog->selected_xfer);
}

static void
close_button_cb(GtkButton *button, GaimGtkXferDialog *dialog)
{
	gaim_gtkxfer_dialog_hide(dialog);
}


/**************************************************************************
 * Dialog Building Functions
 **************************************************************************/
static GtkWidget *
setup_tree(GaimGtkXferDialog *dialog)
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
					GTK_POLICY_AUTOMATIC);
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
	renderer = gaim_gtk_cell_renderer_progress_new();
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
make_info_table(GaimGtkXferDialog *dialog)
{
	GtkWidget *table;
	GtkWidget *label;
	int i;

	struct
	{
		GtkWidget **desc_label;
		GtkWidget **val_label;
		const char *desc;

	} labels[] =
	{
		{ &dialog->local_user_desc_label, &dialog->local_user_label, NULL },
		{ &dialog->remote_user_desc_label, &dialog->remote_user_label, NULL },
		{ &label, &dialog->protocol_label,       _("Protocol:") },
		{ &label, &dialog->filename_label,       _("Filename:") },
		{ &label, &dialog->localfile_label,      _("Local File:") },
		{ &label, &dialog->status_label,         _("Status:") },
		{ &label, &dialog->speed_label,          _("Speed:") },
		{ &label, &dialog->time_elapsed_label,   _("Time Elapsed:") },
		{ &label, &dialog->time_remaining_label, _("Time Remaining:") }
	};

	/* Setup the initial table */
	dialog->table = table = gtk_table_new(9, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), GAIM_HIG_BOX_SPACE);
	gtk_table_set_col_spacings(GTK_TABLE(table), GAIM_HIG_BOX_SPACE);

	/* Setup the labels */
	for (i = 0; i < sizeof(labels) / sizeof(*labels); i++) {
		GtkWidget *label;
		char buf[256];

		g_snprintf(buf, sizeof(buf), "<b>%s</b>",
			   labels[i].desc != NULL ? labels[i].desc : "");

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
	gtk_table_attach(GTK_TABLE(table), dialog->progress, 0, 2, 8, 9,
					 GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_show(dialog->progress);

	return table;
}

GaimGtkXferDialog *
gaim_gtkxfer_dialog_new(void)
{
	GaimGtkXferDialog *dialog;
	GtkWidget *window;
	GtkWidget *vbox1, *vbox2;
	GtkWidget *bbox;
	GtkWidget *sw;
	GtkWidget *button;
	GtkWidget *expander;
	GtkWidget *table;
	GtkWidget *checkbox;

	dialog = g_new0(GaimGtkXferDialog, 1);
	dialog->keep_open =
		gaim_prefs_get_bool("/gaim/gtk/filetransfer/keep_open");
	dialog->auto_clear =
		gaim_prefs_get_bool("/gaim/gtk/filetransfer/clear_finished");

	/* Create the window. */
	dialog->window = window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_role(GTK_WINDOW(window), "file transfer");
	gtk_window_set_title(GTK_WINDOW(window), _("File Transfers"));
	gtk_container_set_border_width(GTK_CONTAINER(window), GAIM_HIG_BORDER);

	g_signal_connect(G_OBJECT(window), "delete_event",
					 G_CALLBACK(delete_win_cb), dialog);

	/* Create the parent vbox for everything. */
	vbox1 = gtk_vbox_new(FALSE, GAIM_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(window), vbox1);
	gtk_widget_show(vbox1);

	/* Create the main vbox for top half of the window. */
	vbox2 = gtk_vbox_new(FALSE, GAIM_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(vbox1), vbox2, TRUE, TRUE, 0);
	gtk_widget_show(vbox2);

	/* Setup the listbox */
	sw = setup_tree(dialog);
	gtk_box_pack_start(GTK_BOX(vbox2), sw, TRUE, TRUE, 0);
	gtk_widget_set_size_request(sw,-1, 140);

	/* "Close this window when all transfers finish" */
	checkbox = gtk_check_button_new_with_mnemonic(
			_("Close this window when all transfers _finish"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox),
								 !dialog->keep_open);
	g_signal_connect(G_OBJECT(checkbox), "toggled",
					 G_CALLBACK(toggle_keep_open_cb), dialog);
	gtk_box_pack_start(GTK_BOX(vbox2), checkbox, FALSE, FALSE, 0);
	gtk_widget_show(checkbox);

	/* "Clear finished transfers" */
	checkbox = gtk_check_button_new_with_mnemonic(
			_("C_lear finished transfers"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox),
								 dialog->auto_clear);
	g_signal_connect(G_OBJECT(checkbox), "toggled",
					 G_CALLBACK(toggle_clear_finished_cb), dialog);
	gtk_box_pack_start(GTK_BOX(vbox2), checkbox, FALSE, FALSE, 0);
	gtk_widget_show(checkbox);

	/* "Download Details" arrow */
	expander = gtk_expander_new_with_mnemonic(_("File transfer _details"));
	dialog->expander = expander;
	gtk_box_pack_start(GTK_BOX(vbox2), expander, FALSE, FALSE, 0);
	gtk_widget_show(expander);

	gtk_widget_set_sensitive(expander, FALSE);

	/* The table of information. */
	table = make_info_table(dialog);
	gtk_container_add(GTK_CONTAINER(expander), table);
	gtk_widget_show(table);

	/* Now the button box for the buttons */
	bbox = gtk_hbutton_box_new();
	gtk_box_set_spacing(GTK_BOX(bbox), GAIM_HIG_BOX_SPACE);
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

	/* Close button */
	button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);
	dialog->close_button = button;

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(close_button_cb), dialog);

	return dialog;
}

void
gaim_gtkxfer_dialog_destroy(GaimGtkXferDialog *dialog)
{
	g_return_if_fail(dialog != NULL);

	gaim_notify_close_with_handle(dialog);

	gtk_widget_destroy(dialog->window);

	g_free(dialog);
}

void
gaim_gtkxfer_dialog_show(GaimGtkXferDialog *dialog)
{
	GaimGtkXferDialog *tmp;

	if (dialog == NULL) {
		tmp = gaim_get_gtkxfer_dialog();

		if (tmp == NULL) {
			tmp = gaim_gtkxfer_dialog_new();
			gaim_set_gtkxfer_dialog(tmp);
		}

		gtk_widget_show(tmp->window);
	} else {
		gtk_widget_show(dialog->window);
	}
}

void
gaim_gtkxfer_dialog_hide(GaimGtkXferDialog *dialog)
{
	g_return_if_fail(dialog != NULL);

	gaim_notify_close_with_handle(dialog);

	gtk_widget_hide(dialog->window);
}

void
gaim_gtkxfer_dialog_add_xfer(GaimGtkXferDialog *dialog, GaimXfer *xfer)
{
	GaimGtkXferUiData *data;
	GaimXferType type;
	GdkPixbuf *pixbuf;
	char *size_str, *remaining_str;
	char *lfilename, *utf8;

	g_return_if_fail(dialog != NULL);
	g_return_if_fail(xfer != NULL);

	gaim_xfer_ref(xfer);

	data = GAIM_GTKXFER(xfer);
	data->in_list = TRUE;

	gaim_gtkxfer_dialog_show(dialog);

	data->last_updated_time = 0;

	type = gaim_xfer_get_type(xfer);

	size_str      = gaim_str_size_to_units(gaim_xfer_get_size(xfer));
	remaining_str = gaim_str_size_to_units(gaim_xfer_get_bytes_remaining(xfer));

	pixbuf = gtk_widget_render_icon(dialog->window,
									(type == GAIM_XFER_RECEIVE
									 ? GAIM_STOCK_DOWNLOAD
									 : GAIM_STOCK_UPLOAD),
									GTK_ICON_SIZE_MENU, NULL);

	gtk_list_store_append(dialog->model, &data->iter);
	lfilename = g_path_get_basename(gaim_xfer_get_local_filename(xfer));
	utf8 = g_filename_to_utf8(lfilename, -1, NULL, NULL, NULL);
	g_free(lfilename);
	lfilename = utf8;
	gtk_list_store_set(dialog->model, &data->iter,
					   COLUMN_STATUS, pixbuf,
					   COLUMN_PROGRESS, 0.0,
					   COLUMN_FILENAME, (type == GAIM_XFER_RECEIVE)
					                     ? gaim_xfer_get_filename(xfer)
							     : lfilename,
					   COLUMN_SIZE, size_str,
					   COLUMN_REMAINING, _("Waiting for transfer to begin"),
					   COLUMN_DATA, xfer,
					   -1);
	g_free(lfilename);

	gtk_tree_view_columns_autosize(GTK_TREE_VIEW(dialog->tree));

	g_object_unref(pixbuf);

	g_free(size_str);
	g_free(remaining_str);

	dialog->num_transfers++;

	ensure_row_selected(dialog);
	update_title_progress(dialog);
}

void
gaim_gtkxfer_dialog_remove_xfer(GaimGtkXferDialog *dialog,
								GaimXfer *xfer)
{
	GaimGtkXferUiData *data;

	g_return_if_fail(dialog != NULL);
	g_return_if_fail(xfer != NULL);

	data = GAIM_GTKXFER(xfer);

	if (data == NULL)
		return;

	if (!data->in_list)
		return;

	data->in_list = FALSE;

	gtk_list_store_remove(GTK_LIST_STORE(dialog->model), &data->iter);

	dialog->num_transfers--;

	if (dialog->num_transfers == 0 && !dialog->keep_open)
		gaim_gtkxfer_dialog_hide(dialog);
	else
		ensure_row_selected(dialog);

	update_title_progress(dialog);
	gaim_xfer_unref(xfer);
}

void
gaim_gtkxfer_dialog_cancel_xfer(GaimGtkXferDialog *dialog,
								GaimXfer *xfer)
{
	GaimGtkXferUiData *data;
	GdkPixbuf *pixbuf;
	const gchar *status;

	g_return_if_fail(dialog != NULL);
	g_return_if_fail(xfer != NULL);

	data = GAIM_GTKXFER(xfer);

	if (data == NULL)
		return;

	if (!data->in_list)
		return;

	if ((gaim_xfer_get_status(xfer) == GAIM_XFER_STATUS_CANCEL_LOCAL) && (dialog->auto_clear)) {
		gaim_gtkxfer_dialog_remove_xfer(dialog, xfer);
		return;
	}

	data = GAIM_GTKXFER(xfer);

	update_detailed_info(dialog, xfer);
	update_title_progress(dialog);

	pixbuf = gtk_widget_render_icon(dialog->window,
									GAIM_STOCK_FILE_CANCELED,
									GTK_ICON_SIZE_MENU, NULL);

	if (gaim_xfer_is_canceled(xfer))
		status = _("Canceled");
	else
		status = _("Failed");

	gtk_list_store_set(dialog->model, &data->iter,
	                   COLUMN_STATUS, pixbuf,
	                   COLUMN_REMAINING, status,
	                   -1);

	g_object_unref(pixbuf);

	update_buttons(dialog, xfer);
}

void
gaim_gtkxfer_dialog_update_xfer(GaimGtkXferDialog *dialog,
								GaimXfer *xfer)
{
	GaimGtkXferUiData *data;
	char *size_str, *remaining_str;
	GtkTreeSelection *selection;
	time_t current_time;

	g_return_if_fail(dialog != NULL);
	g_return_if_fail(xfer != NULL);

	if ((data = GAIM_GTKXFER(xfer)) == NULL)
		return;

	if (data->in_list == FALSE)
		return;

	current_time = time(NULL);
	if (((current_time - data->last_updated_time) == 0) &&
		(!gaim_xfer_is_completed(xfer)))
	{
		/* Don't update the window more than once per second */
		return;
	}
	data->last_updated_time = current_time;

	size_str      = gaim_str_size_to_units(gaim_xfer_get_size(xfer));
	remaining_str = gaim_str_size_to_units(gaim_xfer_get_bytes_remaining(xfer));

	gtk_list_store_set(xfer_dialog->model, &data->iter,
					   COLUMN_PROGRESS, gaim_xfer_get_progress(xfer),
					   COLUMN_SIZE, size_str,
					   COLUMN_REMAINING, remaining_str,
					   -1);

	if (gaim_xfer_is_completed(xfer))
	{
		GdkPixbuf *pixbuf;

		pixbuf = gtk_widget_render_icon(dialog->window,
										GAIM_STOCK_FILE_DONE,
										GTK_ICON_SIZE_MENU, NULL);

		gtk_list_store_set(GTK_LIST_STORE(xfer_dialog->model), &data->iter,
						   COLUMN_STATUS, pixbuf,
						   COLUMN_REMAINING, _("Finished"),
						   -1);

		g_object_unref(pixbuf);
	}

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(xfer_dialog->tree));

	update_title_progress(dialog);
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
gaim_gtkxfer_new_xfer(GaimXfer *xfer)
{
	GaimGtkXferUiData *data;

	/* This is where we're setting xfer->ui_data for the first time. */
	data = g_new0(GaimGtkXferUiData, 1);
	xfer->ui_data = data;
}

static void
gaim_gtkxfer_destroy(GaimXfer *xfer)
{
	GaimGtkXferUiData *data;

	data = GAIM_GTKXFER(xfer);
	if (data) {
		g_free(data->name);
		g_free(data);
		xfer->ui_data = NULL;
	}
}

static void
gaim_gtkxfer_add_xfer(GaimXfer *xfer)
{
	if (xfer_dialog == NULL)
		xfer_dialog = gaim_gtkxfer_dialog_new();

	gaim_gtkxfer_dialog_add_xfer(xfer_dialog, xfer);
}

static void
gaim_gtkxfer_update_progress(GaimXfer *xfer, double percent)
{
	gaim_gtkxfer_dialog_update_xfer(xfer_dialog, xfer);
}

static void
gaim_gtkxfer_cancel_local(GaimXfer *xfer)
{
	if (xfer_dialog)
		gaim_gtkxfer_dialog_cancel_xfer(xfer_dialog, xfer);
}

static void
gaim_gtkxfer_cancel_remote(GaimXfer *xfer)
{
	if (xfer_dialog)
		gaim_gtkxfer_dialog_cancel_xfer(xfer_dialog, xfer);
}

static GaimXferUiOps ops =
{
	gaim_gtkxfer_new_xfer,
	gaim_gtkxfer_destroy,
	gaim_gtkxfer_add_xfer,
	gaim_gtkxfer_update_progress,
	gaim_gtkxfer_cancel_local,
	gaim_gtkxfer_cancel_remote
};

/**************************************************************************
 * GTK+ File Transfer API
 **************************************************************************/
void
gaim_gtk_xfers_init(void)
{
	gaim_prefs_add_none("/gaim/gtk/filetransfer");
	gaim_prefs_add_bool("/gaim/gtk/filetransfer/clear_finished", TRUE);
	gaim_prefs_add_bool("/gaim/gtk/filetransfer/keep_open", FALSE);
}

void
gaim_gtk_xfers_uninit(void)
{
	if (xfer_dialog != NULL)
		gaim_gtkxfer_dialog_destroy(xfer_dialog);
}

void
gaim_set_gtkxfer_dialog(GaimGtkXferDialog *dialog)
{
	xfer_dialog = dialog;
}

GaimGtkXferDialog *
gaim_get_gtkxfer_dialog(void)
{
	return xfer_dialog;
}

GaimXferUiOps *
gaim_gtk_xfers_get_ui_ops(void)
{
	return &ops;
}
