/**
 * @file gntft.c GNT File Transfer UI
 * @ingroup gntui
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
#include <gnt.h>
#include <gntbox.h>
#include <gntbutton.h>
#include <gntcheckbox.h>
#include <gntlabel.h>
#include <gnttree.h>
#include "internal.h"

#include "debug.h"
#include "notify.h"
#include "ft.h"
#include "prpl.h"
#include "util.h"

#include "gntft.h"
#include "prefs.h"

#define GAIM_GNTXFER(xfer) \
	(GaimGntXferUiData *)(xfer)->ui_data

typedef struct
{
	gboolean keep_open;
	gboolean auto_clear;
	gint num_transfers;

	GntWidget *window;
	GntWidget *tree;

	GntWidget *remove_button;
	GntWidget *stop_button;
	GntWidget *close_button;
} GaimGntXferDialog;

static GaimGntXferDialog *xfer_dialog = NULL;

typedef struct
{
	time_t last_updated_time;
	gboolean in_list;

	char *name;

} GaimGntXferUiData;

enum
{
	COLUMN_PROGRESS = 0,
	COLUMN_FILENAME,
	COLUMN_SIZE,
	COLUMN_SPEED,
	COLUMN_REMAINING,
	COLUMN_STATUS,
	NUM_COLUMNS
};


/**************************************************************************
 * Utility Functions
 **************************************************************************/

static void
update_title_progress()
{
	const GList *list;
	int num_active_xfers = 0;
	guint64 total_bytes_xferred = 0;
	guint64 total_file_size = 0;

	if (xfer_dialog == NULL || xfer_dialog->window == NULL)
		return;

	/* Find all active transfers */
	for (list = gnt_tree_get_rows(GNT_TREE(xfer_dialog->tree)); list; list = list->next) {
		GaimXfer *xfer = (GaimXfer *)list->data;

		if (gaim_xfer_get_status(xfer) == GAIM_XFER_STATUS_STARTED) {
			num_active_xfers++;
			total_bytes_xferred += gaim_xfer_get_bytes_sent(xfer);
			total_file_size += gaim_xfer_get_size(xfer);
		}
	}

	/* Update the title */
	if (num_active_xfers > 0) {
		gchar *title;
		int total_pct = 0;

		if (total_file_size > 0) {
			total_pct = 100 * total_bytes_xferred / total_file_size;
		}

		title = g_strdup_printf(_("File Transfers - %d%% of %d files"),
				total_pct, num_active_xfers);
		gnt_screen_rename_widget((xfer_dialog->window), title);
		g_free(title);
	} else {
		gnt_screen_rename_widget((xfer_dialog->window), _("File Transfers"));
	}
}


/**************************************************************************
 * Callbacks
 **************************************************************************/
static void
toggle_keep_open_cb(GntWidget *w)
{
	xfer_dialog->keep_open = !xfer_dialog->keep_open;
	gaim_prefs_set_bool("/gaim/gnt/filetransfer/keep_open",
						xfer_dialog->keep_open);
}

static void
toggle_clear_finished_cb(GntWidget *w)
{
	xfer_dialog->auto_clear = !xfer_dialog->auto_clear;
	gaim_prefs_set_bool("/gaim/gnt/filetransfer/clear_finished",
						xfer_dialog->auto_clear);
}

static void
remove_button_cb(GntButton *button)
{
	GaimXfer *selected_xfer = gnt_tree_get_selection_data(GNT_TREE(xfer_dialog->tree));
	if (selected_xfer && (selected_xfer->status == GAIM_XFER_STATUS_CANCEL_LOCAL ||
			selected_xfer->status == GAIM_XFER_STATUS_CANCEL_REMOTE ||
			selected_xfer->status == GAIM_XFER_STATUS_DONE)) {
		gg_xfer_dialog_remove_xfer(selected_xfer);
	}
}

static void
stop_button_cb(GntButton *button)
{
	GaimXfer *selected_xfer = gnt_tree_get_selection_data(GNT_TREE(xfer_dialog->tree));
	if (selected_xfer && selected_xfer->status == GAIM_XFER_STATUS_STARTED)
		gaim_xfer_cancel_local(selected_xfer);
}

#if 0
static void
tree_selection_changed_cb(GntTree *tree, GntTreeRow *old, GntTreeRow *current, gpointer n)
{
	xfer_dialog->selected_xfer = (GaimXfer *)gnt_tree_get_selection_data(tree);
}
#endif

/**************************************************************************
 * Dialog Building Functions
 **************************************************************************/


void
gg_xfer_dialog_new(void)
{
	const GList *iter;
	GntWidget *window;
	GntWidget *bbox;
	GntWidget *button;
	GntWidget *checkbox;
	GntWidget *tree;

	if (!xfer_dialog)
		xfer_dialog = g_new0(GaimGntXferDialog, 1);

	xfer_dialog->keep_open =
		gaim_prefs_get_bool("/gaim/gnt/filetransfer/keep_open");
	xfer_dialog->auto_clear =
		gaim_prefs_get_bool("/gaim/gnt/filetransfer/clear_finished");

	/* Create the window. */
	xfer_dialog->window = window = gnt_vbox_new(FALSE);
	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(gg_xfer_dialog_destroy), NULL);
	gnt_box_set_toplevel(GNT_BOX(window), TRUE);
	gnt_box_set_title(GNT_BOX(window), _("File Transfers"));

	xfer_dialog->tree = tree = gnt_tree_new_with_columns(NUM_COLUMNS);
	gnt_tree_set_column_titles(GNT_TREE(tree), _("Progress"), _("Filename"), _("Size"), _("Speed"), _("Remaining"), _("Status"));
	gnt_tree_set_col_width(GNT_TREE(tree), COLUMN_PROGRESS, 8);
	gnt_tree_set_col_width(GNT_TREE(tree), COLUMN_FILENAME, 8);
	gnt_tree_set_col_width(GNT_TREE(tree), COLUMN_SIZE, 10);
	gnt_tree_set_col_width(GNT_TREE(tree), COLUMN_SPEED, 10);
	gnt_tree_set_col_width(GNT_TREE(tree), COLUMN_REMAINING, 10);
	gnt_tree_set_col_width(GNT_TREE(tree), COLUMN_STATUS, 10);
	gnt_tree_set_show_title(GNT_TREE(tree), TRUE);
	gnt_box_add_widget(GNT_BOX(window), tree);
	/*g_signal_connect(G_OBJECT(tree), "selection-changed",*/
					/*G_CALLBACK(tree_selection_changed_cb), NULL);*/
	checkbox = gnt_check_box_new( _("Close this window when all transfers finish"));
	gnt_check_box_set_checked(GNT_CHECK_BOX(checkbox),
								 !xfer_dialog->keep_open);
	g_signal_connect(G_OBJECT(checkbox), "toggled",
					 G_CALLBACK(toggle_keep_open_cb), NULL);
	gnt_box_add_widget(GNT_BOX(window), checkbox);

	checkbox = gnt_check_box_new(_("Clear finished transfers"));
	gnt_check_box_set_checked(GNT_CHECK_BOX(checkbox),
								 xfer_dialog->auto_clear);
	g_signal_connect(G_OBJECT(checkbox), "toggled",
					 G_CALLBACK(toggle_clear_finished_cb), NULL);
	gnt_box_add_widget(GNT_BOX(window), checkbox);

	bbox = gnt_hbox_new(TRUE);

	xfer_dialog->remove_button = button = gnt_button_new(_("Remove"));
	g_signal_connect(G_OBJECT(button), "activate",
					 G_CALLBACK(remove_button_cb), NULL);
	gnt_box_add_widget(GNT_BOX(bbox), button);

	xfer_dialog->stop_button = button = gnt_button_new(_("Stop"));
	g_signal_connect(G_OBJECT(button), "activate",
					 G_CALLBACK(stop_button_cb), NULL);
	gnt_box_add_widget(GNT_BOX(bbox), button);

	xfer_dialog->close_button = button = gnt_button_new(_("Close"));
	g_signal_connect(G_OBJECT(button), "activate",
					 G_CALLBACK(gg_xfer_dialog_destroy), NULL);
	gnt_box_add_widget(GNT_BOX(bbox), button);

	gnt_box_add_widget(GNT_BOX(window), bbox);

	for (iter = gaim_xfers_get_all(); iter; iter = iter->next) {
		GaimXfer *xfer = (GaimXfer *)iter->data;
		GaimGntXferUiData *data = GAIM_GNTXFER(xfer);
		if (data->in_list) {
			gg_xfer_dialog_add_xfer(xfer);
			gg_xfer_dialog_update_xfer(xfer);
			gnt_tree_set_selected(GNT_TREE(tree), xfer);
		}
	}
	gnt_widget_show(xfer_dialog->window);
}

void
gg_xfer_dialog_destroy()
{
	gnt_widget_destroy(xfer_dialog->window);
	g_free(xfer_dialog);
	xfer_dialog = NULL;
}

void
gg_xfer_dialog_show()
{
	if (xfer_dialog == NULL)
		gg_xfer_dialog_new();
}

void
gg_xfer_dialog_add_xfer(GaimXfer *xfer)
{
	GaimGntXferUiData *data;
	GaimXferType type;
	char *size_str, *remaining_str;
	char *lfilename, *utf8;

	g_return_if_fail(xfer_dialog != NULL);
	g_return_if_fail(xfer != NULL);

	gaim_xfer_ref(xfer);

	data = GAIM_GNTXFER(xfer);
	data->in_list = TRUE;

	gg_xfer_dialog_show();

	data->last_updated_time = 0;

	type = gaim_xfer_get_type(xfer);

	size_str      = gaim_str_size_to_units(gaim_xfer_get_size(xfer));
	remaining_str = gaim_str_size_to_units(gaim_xfer_get_bytes_remaining(xfer));

	lfilename = g_path_get_basename(gaim_xfer_get_local_filename(xfer));
	utf8 = g_filename_to_utf8(lfilename, -1, NULL, NULL, NULL);
	g_free(lfilename);
	lfilename = utf8;
	gnt_tree_add_row_last(GNT_TREE(xfer_dialog->tree), xfer,
		gnt_tree_create_row(GNT_TREE(xfer_dialog->tree),
			"0.0", (type == GAIM_XFER_RECEIVE) ? gaim_xfer_get_filename(xfer) : lfilename,
			size_str, "0.0", "",_("Waiting for transfer to begin")), NULL);
	g_free(lfilename);

	g_free(size_str);
	g_free(remaining_str);

	xfer_dialog->num_transfers++;

	update_title_progress();
}

void
gg_xfer_dialog_remove_xfer(GaimXfer *xfer)
{
	GaimGntXferUiData *data;

	g_return_if_fail(xfer_dialog != NULL);
	g_return_if_fail(xfer != NULL);

	data = GAIM_GNTXFER(xfer);

	if (data == NULL)
		return;

	if (!data->in_list)
		return;

	data->in_list = FALSE;

	gnt_tree_remove(GNT_TREE(xfer_dialog->tree), xfer);

	xfer_dialog->num_transfers--;

	if (xfer_dialog->num_transfers == 0 && !xfer_dialog->keep_open)
		gg_xfer_dialog_destroy();
	else 
		update_title_progress();
	gaim_xfer_unref(xfer);
}

void
gg_xfer_dialog_cancel_xfer(GaimXfer *xfer)
{
	GaimGntXferUiData *data;
	const gchar *status;

	g_return_if_fail(xfer_dialog != NULL);
	g_return_if_fail(xfer != NULL);

	data = GAIM_GNTXFER(xfer);

	if (data == NULL)
		return;

	if (!data->in_list)
		return;

	if ((gaim_xfer_get_status(xfer) == GAIM_XFER_STATUS_CANCEL_LOCAL) && (xfer_dialog->auto_clear)) {
		gg_xfer_dialog_remove_xfer(xfer);
		return;
	}

	data = GAIM_GNTXFER(xfer);

	update_title_progress();

	if (gaim_xfer_is_canceled(xfer))
		status = _("Canceled");
	else
		status = _("Failed");

	gnt_tree_change_text(GNT_TREE(xfer_dialog->tree), xfer, COLUMN_STATUS, status);
}

void
gg_xfer_dialog_update_xfer(GaimXfer *xfer)
{
	GaimGntXferUiData *data;
	char *size_str, *remaining_str;
	time_t current_time;
	char prog_str[5];
	double kb_sent, kb_rem;
	double kbps = 0.0;
	time_t elapsed, now;
	char *kbsec;

	if (xfer->end_time != 0)
		now = xfer->end_time;
	else
		now = time(NULL);

	kb_sent = gaim_xfer_get_bytes_sent(xfer) / 1024.0;
	kb_rem  = gaim_xfer_get_bytes_remaining(xfer) / 1024.0;
	elapsed = (xfer->start_time > 0 ? now - xfer->start_time : 0);
	kbps    = (elapsed > 0 ? (kb_sent / elapsed) : 0);

	kbsec = g_strdup_printf(_("%.2f KB/s"), kbps);

	g_return_if_fail(xfer_dialog != NULL);
	g_return_if_fail(xfer != NULL);

	if ((data = GAIM_GNTXFER(xfer)) == NULL)
		return;

	if (data->in_list == FALSE)
		return;

	current_time = time(NULL);
	if (((current_time - data->last_updated_time) == 0) &&
		(!gaim_xfer_is_completed(xfer))) {
		/* Don't update the window more than once per second */
		return;
	}
	data->last_updated_time = current_time;

	size_str      = gaim_str_size_to_units(gaim_xfer_get_size(xfer));
	remaining_str = gaim_str_size_to_units(gaim_xfer_get_bytes_remaining(xfer));

	gnt_tree_change_text(GNT_TREE(xfer_dialog->tree), xfer, COLUMN_PROGRESS,
			g_ascii_dtostr(prog_str, sizeof(prog_str), gaim_xfer_get_progress(xfer) * 100.));
	gnt_tree_change_text(GNT_TREE(xfer_dialog->tree), xfer, COLUMN_SIZE, size_str);
	gnt_tree_change_text(GNT_TREE(xfer_dialog->tree), xfer, COLUMN_REMAINING, remaining_str);
	gnt_tree_change_text(GNT_TREE(xfer_dialog->tree), xfer, COLUMN_SPEED, kbsec);
	g_free(size_str);
	g_free(remaining_str);
	if (gaim_xfer_is_completed(xfer)) {
		gnt_tree_change_text(GNT_TREE(xfer_dialog->tree), xfer, COLUMN_STATUS, _("Finished"));
	} else {
		gnt_tree_change_text(GNT_TREE(xfer_dialog->tree), xfer, COLUMN_STATUS, _("Transferring"));
	}

	update_title_progress();

	if (gaim_xfer_is_completed(xfer) && xfer_dialog->auto_clear)
		gg_xfer_dialog_remove_xfer(xfer);
}

/**************************************************************************
 * File Transfer UI Ops
 **************************************************************************/
static void
gg_xfer_new_xfer(GaimXfer *xfer)
{
	GaimGntXferUiData *data;

	/* This is where we're setting xfer->ui_data for the first time. */
	data = g_new0(GaimGntXferUiData, 1);
	xfer->ui_data = data;
}

static void
gg_xfer_destroy(GaimXfer *xfer)
{
	GaimGntXferUiData *data;

	data = GAIM_GNTXFER(xfer);
	if (data) {
		g_free(data->name);
		g_free(data);
		xfer->ui_data = NULL;
	}
}

static void
gg_xfer_add_xfer(GaimXfer *xfer)
{
	if (!xfer_dialog)
		gg_xfer_dialog_new();

	gg_xfer_dialog_add_xfer(xfer);
	gnt_tree_set_selected(GNT_TREE(xfer_dialog->tree), xfer);
}

static void
gg_xfer_update_progress(GaimXfer *xfer, double percent)
{
	if (xfer_dialog)
		gg_xfer_dialog_update_xfer(xfer);
}

static void
gg_xfer_cancel_local(GaimXfer *xfer)
{
	if (xfer_dialog)
		gg_xfer_dialog_cancel_xfer(xfer);
}

static void
gg_xfer_cancel_remote(GaimXfer *xfer)
{
	if (xfer_dialog)
		gg_xfer_dialog_cancel_xfer(xfer);
}

static GaimXferUiOps ops =
{
	gg_xfer_new_xfer,
	gg_xfer_destroy,
	gg_xfer_add_xfer,
	gg_xfer_update_progress,
	gg_xfer_cancel_local,
	gg_xfer_cancel_remote
};

/**************************************************************************
 * GNT File Transfer API
 **************************************************************************/
void
gg_xfers_init(void)
{
	gaim_prefs_add_none("/gaim/gnt/filetransfer");
	gaim_prefs_add_bool("/gaim/gnt/filetransfer/clear_finished", TRUE);
	gaim_prefs_add_bool("/gaim/gnt/filetransfer/keep_open", FALSE);
}

void
gg_xfers_uninit(void)
{
	if (xfer_dialog != NULL)
		gg_xfer_dialog_destroy();
}

GaimXferUiOps *
gg_xfers_get_ui_ops(void)
{
	return &ops;
}
