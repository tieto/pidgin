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

struct gaim_gtkxfer_dialog
{
	GtkWidget *window;
	GtkWidget *tree;
	GtkListStore *model;
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
	COLUMN_USER,
	COLUMN_FILENAME,
	COLUMN_PROGRESS,
	COLUMN_SIZE,
	COLUMN_REMAINING,
	COLUMN_ESTIMATE,
	COLUMN_SPEED,
	NUM_COLUMNS
};

static gint
delete_win_cb(GtkWidget *w, GdkEventAny *e, gpointer d)
{
	gaim_gtkxfer_dialog_hide();

	return TRUE;
}

static void
close_win_cb(GtkButton *button, gpointer user_data)
{
	gaim_gtkxfer_dialog_hide();
}

static struct gaim_gtkxfer_dialog *
build_xfer_dialog(void)
{
	struct gaim_gtkxfer_dialog *dialog;
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *frame;
	GtkWidget *sw;
	GtkWidget *sep;
	GtkWidget *button;
	GtkWidget *tree;
	GtkTreeViewColumn *column;
	GtkListStore *model;
	GtkCellRenderer *renderer;
	GtkSizeGroup *sg;

	dialog = g_malloc0(sizeof(struct gaim_gtkxfer_dialog));

	/* Create the window. */
	dialog->window = window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_role(GTK_WINDOW(window), "file transfer");
	gtk_window_set_policy(GTK_WINDOW(window), TRUE, TRUE, FALSE);
	gtk_window_set_default_size(GTK_WINDOW(window), 550, 250);
	gtk_container_border_width(GTK_CONTAINER(window), 0);
	gtk_widget_realize(window);

	g_signal_connect(G_OBJECT(window), "delete_event",
					 G_CALLBACK(delete_win_cb), dialog);

	/* Create the parent vbox for everything. */
	vbox = gtk_vbox_new(FALSE, 6);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);
	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_widget_show(vbox);

	/* Create the scrolled window. */
	sw = gtk_scrolled_window_new(0, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
								   GTK_POLICY_AUTOMATIC,
								   GTK_POLICY_ALWAYS);
	gtk_widget_show(sw);

	/* Build the tree model */
	/* Transfer type, User, Filename, Progress Bar, Size, Remaining */
	dialog->model = model = gtk_list_store_new(NUM_COLUMNS,
											   GDK_TYPE_PIXBUF, G_TYPE_STRING,
											   G_TYPE_STRING, G_TYPE_DOUBLE,
											   G_TYPE_LONG, G_TYPE_LONG,
											   G_TYPE_STRING, G_TYPE_STRING);

	/* Create the treeview */
	dialog->tree = tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(model));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree), TRUE);
	gtk_tree_selection_set_mode(
			gtk_tree_view_get_selection(GTK_TREE_VIEW(tree)),
			GTK_SELECTION_MULTIPLE);
	gtk_widget_show(tree);

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

	/* User column */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("User"), renderer,
				"text", COLUMN_USER, NULL);
	gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	/* Filename column */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Filename"), renderer,
				"text", COLUMN_FILENAME, NULL);
	gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	/* Progress bar column */
	renderer = gtk_cell_renderer_progress_new();
	column = gtk_tree_view_column_new_with_attributes(_("Progress"), renderer,
				"percentage", COLUMN_PROGRESS, NULL);
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

	/* ETA column */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("ETA"),
				renderer, "text", COLUMN_ESTIMATE, NULL);
	gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	/* Speed column */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Speed"),
				renderer, "text", COLUMN_SPEED, NULL);
	gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(column), TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	gtk_tree_view_columns_autosize(GTK_TREE_VIEW(tree));

	gtk_container_add(GTK_CONTAINER(sw), tree);
	gtk_widget_show(tree);

	/* Create the outer frame for the scrolled window. */
	frame = gtk_frame_new(NULL),
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(frame), sw);
	gtk_widget_show(frame);

	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);

	/* Separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);
	gtk_widget_show(sep);

	/* Now the hbox for the buttons */
	hbox = gtk_hbox_new(FALSE, 6);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 6);
	gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_BOTH);

	/* Close button */
	button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(close_win_cb), NULL);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

#if 0
	/* Cancel button */
	button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);
#endif

	return dialog;
}

static void
gaim_gtkxfer_destroy(struct gaim_xfer *xfer)
{
	struct gaim_gtkxfer_ui_data *data;

	data = xfer->ui_data;

	if (data == NULL)
		return;
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

	data = (struct gaim_gtkxfer_ui_data *)xfer->ui_data;

	gaim_xfer_request_denied(xfer);

	gtk_widget_destroy(data->filesel);
	data->filesel = NULL;
}

static int
do_overwrite_cb(struct gaim_xfer *xfer)
{
	struct gaim_gtkxfer_ui_data *data;
	
	data = (struct gaim_gtkxfer_ui_data *)xfer->ui_data;

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
	
	data = (struct gaim_gtkxfer_ui_data *)xfer->ui_data;

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
	data = (struct gaim_gtkxfer_ui_data *)xfer->ui_data;

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
	static const char *size_str[4] = { "bytes", "KB", "MB", "GB" };
	char *buf;
	char *size_buf;
	float size_mag;
	size_t size;
	int size_index = 0;

	size = gaim_xfer_get_size(xfer);
	size_mag = (float)size;

	while ((size_index < 4) && (size_mag > 1024)) {
		size_mag /= 1024;
		size_index++;
	}

	if (size == -1)
		size_buf = g_strdup_printf(_("Unknown size"));
	else
		size_buf = g_strdup_printf("%.3g %s", size_mag, size_str[size_index]);

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
	struct gaim_gtkxfer_ui_data *data;
	GaimXferType type;
	GdkPixbuf *pixbuf;

	data = (struct gaim_gtkxfer_ui_data *)xfer->ui_data;

	gaim_gtkxfer_dialog_show();

	data->start_time = time(NULL);

	type = gaim_xfer_get_type(xfer);

	pixbuf = gtk_widget_render_icon(xfer_dialog->window,
									(type == GAIM_XFER_RECEIVE
									 ? GAIM_STOCK_DOWNLOAD
									 : GAIM_STOCK_UPLOAD),
									GTK_ICON_SIZE_MENU, NULL);

	gtk_list_store_append(xfer_dialog->model, &data->iter);
	gtk_list_store_set(xfer_dialog->model, &data->iter,
					   COLUMN_STATUS, pixbuf,
					   COLUMN_USER, xfer->who,
					   COLUMN_FILENAME, gaim_xfer_get_filename(xfer),
					   COLUMN_PROGRESS, 0.0,
					   COLUMN_SIZE, gaim_xfer_get_size(xfer),
					   COLUMN_REMAINING, gaim_xfer_get_bytes_remaining(xfer),
					   COLUMN_ESTIMATE, NULL,
					   COLUMN_SPEED, NULL,
					   -1);

	gtk_tree_view_columns_autosize(GTK_TREE_VIEW(xfer_dialog->tree));

	g_object_unref(pixbuf);
}

static void
gaim_gtkxfer_update_progress(struct gaim_xfer *xfer, double percent)
{
	struct gaim_gtkxfer_ui_data *data;
	time_t now;
	char speed_buf[256];
	char estimate_buf[256];
	double kb_sent, kb_rem;
	double kbps = 0.0;
	time_t elapsed;
	int secs_remaining;

	data = (struct gaim_gtkxfer_ui_data *)xfer->ui_data;

	now     = time(NULL);
	kb_rem  = gaim_xfer_get_bytes_remaining(xfer) / 1024.0;
	kb_sent = gaim_xfer_get_bytes_sent(xfer) / 1024.0;
	elapsed = (now - data->start_time);
	kbps    = (elapsed > 0 ? (kb_sent / elapsed) : 0);

	secs_remaining = (int)(kb_rem / kbps);

	if (secs_remaining <= 0) {
		GdkPixbuf *pixbuf = NULL;

		*speed_buf = '\0';
		strncpy(estimate_buf, _("Done."), sizeof(estimate_buf));

		pixbuf = gtk_widget_render_icon(xfer_dialog->window,
										GAIM_STOCK_FILE_DONE,
										GTK_ICON_SIZE_MENU, NULL);

		gtk_list_store_set(xfer_dialog->model, &data->iter,
						   COLUMN_STATUS, pixbuf,
						   -1);

		g_object_unref(pixbuf);
	}
	else {
		int h = secs_remaining / 3600;
		int m = (secs_remaining % 3600) / 60;
		int s = secs_remaining % 60;

		g_snprintf(estimate_buf, sizeof(estimate_buf),
				   _("%d:%02d:%02d"), h, m, s);
		g_snprintf(speed_buf, sizeof(speed_buf),
				   _("%.2f KB/s"), kbps);
	}

	gtk_list_store_set(xfer_dialog->model, &data->iter,
					   COLUMN_REMAINING, gaim_xfer_get_bytes_remaining(xfer),
					   COLUMN_PROGRESS, percent,
					   COLUMN_ESTIMATE, estimate_buf,
					   COLUMN_SPEED, speed_buf,
					   -1);
}

static void
gaim_gtkxfer_cancel(struct gaim_xfer *xfer)
{
	struct gaim_gtkxfer_ui_data *data;
	GdkPixbuf *pixbuf;

	data = (struct gaim_gtkxfer_ui_data *)xfer->ui_data;

	pixbuf = gtk_widget_render_icon(xfer_dialog->window,
									GAIM_STOCK_FILE_CANCELED,
									GTK_ICON_SIZE_MENU, NULL);

	gtk_list_store_set(xfer_dialog->model, &data->iter,
					   COLUMN_STATUS, pixbuf,
					   -1);

	g_object_unref(pixbuf);
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

void
gaim_gtkxfer_dialog_show(void)
{
	if (xfer_dialog == NULL)
		xfer_dialog = build_xfer_dialog();

	gtk_widget_show(xfer_dialog->window);
}

void
gaim_gtkxfer_dialog_hide(void)
{
	gtk_widget_hide(xfer_dialog->window);
}

struct gaim_xfer_ui_ops *
gaim_get_gtk_xfer_ui_ops(void)
{
	return &ops;
}
