/**
 * @file gtknotify.c GTK+ Notification API
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

#include <gdk/gdkkeysyms.h>

#include "connection.h"
#include "debug.h"
#include "prefs.h"
#include "gaimstock.h"
#include "util.h"

#include "gtkblist.h"
#include "gtkimhtml.h"
#include "gtknotify.h"
#include "gtkutils.h"

typedef struct
{
	GaimConnection *gc;
	char *url;
	GtkWidget *label;
	GtkTreeIter iter;
} GaimNotifyMailData;

typedef struct
{
	GaimAccount *account;
	GtkListStore *model;
	GtkWidget *treeview;
	GtkWidget *window;
	gpointer user_data;
	GaimNotifySearchResults *results;

} GaimNotifySearchResultsData;

typedef struct
{
	GaimNotifySearchButton *button;
	GaimNotifySearchResultsData *data;

} GaimNotifySearchResultsButtonData;

enum
{
	GAIM_MAIL_ICON,
	GAIM_MAIL_TO,
	GAIM_MAIL_FROM,
	GAIM_MAIL_SUBJECT,
	GAIM_MAIL_DATA,
	COLUMNS_GAIM_MAIL
};

typedef struct _GaimMailDialog GaimMailDialog;

struct _GaimMailDialog
{
	GtkWidget *dialog;
	GtkWidget *treeview;
	GtkTreeStore *treemodel;
	GtkLabel *label;
	GtkWidget *open_button;
};

static GaimMailDialog *mail_dialog = NULL;

static void *gaim_gtk_notify_emails(GaimConnection *gc, size_t count, gboolean detailed,
									const char **subjects,
									const char **froms, const char **tos,
									const char **urls);

static void
message_response_cb(GtkDialog *dialog, gint id, GtkWidget *widget)
{
	gaim_notify_close(GAIM_NOTIFY_MESSAGE, widget);
}

static void
email_nondetailed_cb(GtkDialog *dialog, gint id, GaimNotifyMailData *data)
{
	if (id == GTK_RESPONSE_OK)
		gaim_notify_uri(NULL, data->url);
	gaim_notify_close(GAIM_NOTIFY_EMAILS, data);
	gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void
email_response_cb(GtkDialog *dlg, gint id, GaimMailDialog *dialog)
{
	GaimNotifyMailData *data = NULL;
	GtkTreeIter iter;

	if (id == GTK_RESPONSE_YES)
	{
		GtkTreeSelection *selection;

		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->treeview));

		if (gtk_tree_selection_get_selected(selection, NULL, &iter))
		{
			gtk_tree_model_get(GTK_TREE_MODEL(dialog->treemodel), &iter,
								GAIM_MAIL_DATA, &data, -1);
			gaim_notify_uri(NULL, data->url);

			gtk_tree_store_remove(dialog->treemodel, &iter);
			gaim_notify_close(GAIM_NOTIFY_EMAILS, data);

			if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(mail_dialog->treemodel), &iter))
				return;
		}
		else
			return;
	}
	else
	{
		while (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(mail_dialog->treemodel), &iter))
		{
			gtk_tree_model_get(GTK_TREE_MODEL(dialog->treemodel), &iter,
								GAIM_MAIL_DATA, &data, -1);

			if (id == GTK_RESPONSE_ACCEPT)
				gaim_notify_uri(NULL, data->url);

			gtk_tree_store_remove(dialog->treemodel, &iter);
			gaim_notify_close(GAIM_NOTIFY_EMAILS, data);
		}
	}
	gtk_widget_destroy(dialog->dialog);
	g_free(dialog);
	mail_dialog = NULL;
}

static void
formatted_close_cb(GtkWidget *win, GdkEvent *event, void *user_data)
{
	gaim_notify_close(GAIM_NOTIFY_FORMATTED, win);
}

static void
searchresults_close_cb(GaimNotifySearchResultsData *data, GdkEvent *event, gpointer user_data)
{
	gaim_notify_close(GAIM_NOTIFY_SEARCHRESULTS, data);
}

static void
searchresults_callback_wrapper_cb(GtkWidget *widget, GaimNotifySearchResultsButtonData *bd)
{
	GaimNotifySearchResultsData *data = bd->data;

	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GaimNotifySearchButton *button;
	GList *row = NULL;
	gchar *str;
	int i;

	g_return_if_fail(data != NULL);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->treeview));

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		for (i = 1; i < gtk_tree_model_get_n_columns(GTK_TREE_MODEL(model)); i++) {
			gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, i, &str, -1);
			row = g_list_append(row, str);
		}
	}

	button = bd->button;
	button->callback(gaim_account_get_connection(data->account), row, data->user_data);
	g_list_foreach(row, (GFunc)g_free, NULL);
	g_list_free(row);
}

static void *
gaim_gtk_notify_message(GaimNotifyMsgType type, const char *title,
						const char *primary, const char *secondary)
{
	GtkWidget *dialog;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *img = NULL;
	char label_text[2048];
	const char *icon_name = NULL;
	char *primary_esc, *secondary_esc;

	switch (type)
	{
		case GAIM_NOTIFY_MSG_ERROR:
			icon_name = GAIM_STOCK_DIALOG_ERROR;
			break;

		case GAIM_NOTIFY_MSG_WARNING:
			icon_name = GAIM_STOCK_DIALOG_WARNING;
			break;

		case GAIM_NOTIFY_MSG_INFO:
			icon_name = GAIM_STOCK_DIALOG_INFO;
			break;

		default:
			icon_name = NULL;
			break;
	}

	if (icon_name != NULL)
	{
		img = gtk_image_new_from_stock(icon_name, GTK_ICON_SIZE_DIALOG);
		gtk_misc_set_alignment(GTK_MISC(img), 0, 0);
	}

	dialog = gtk_dialog_new_with_buttons(title ? title : GAIM_ALERT_TITLE,
										 NULL, 0, GTK_STOCK_CLOSE,
										 GTK_RESPONSE_CLOSE, NULL);

	gtk_window_set_role(GTK_WINDOW(dialog), "notify_dialog");

	g_signal_connect(G_OBJECT(dialog), "response",
					 G_CALLBACK(message_response_cb), dialog);

	gtk_container_set_border_width(GTK_CONTAINER(dialog), GAIM_HIG_BORDER);
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog)->vbox), GAIM_HIG_BORDER);
	gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), GAIM_HIG_BOX_SPACE);

	hbox = gtk_hbox_new(FALSE, GAIM_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), hbox);

	if (img != NULL)
		gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);

	primary_esc = g_markup_escape_text(primary, -1);
	secondary_esc = (secondary != NULL) ? g_markup_escape_text(secondary, -1) : NULL;
	g_snprintf(label_text, sizeof(label_text),
			   "<span weight=\"bold\" size=\"larger\">%s</span>\n\n%s",
			   primary_esc, (secondary ? secondary_esc : ""));
	g_free(primary_esc);
	g_free(secondary_esc);

	label = gtk_label_new(NULL);

	gtk_label_set_markup(GTK_LABEL(label), label_text);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	gtk_widget_show_all(dialog);

	return dialog;
}

static void
selection_changed_cb(GtkTreeSelection *sel, GaimMailDialog *dialog)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GaimNotifyMailData *data;
	gboolean active = TRUE;

	if (gtk_tree_selection_get_selected(sel, &model, &iter) == FALSE)
		active = FALSE;
	else
	{
		gtk_tree_model_get(model, &iter, GAIM_MAIL_DATA, &data, -1);
		if (data->url == NULL)
			active = FALSE;
	}

	gtk_widget_set_sensitive(dialog->open_button, active);
}

static void *
gaim_gtk_notify_email(GaimConnection *gc, const char *subject, const char *from,
					  const char *to, const char *url)
{
	return gaim_gtk_notify_emails(gc, 1, (subject != NULL),
								  (subject == NULL ? NULL : &subject),
								  (from    == NULL ? NULL : &from),
								  (to      == NULL ? NULL : &to),
								  (url     == NULL ? NULL : &url));
}

static void *
gaim_gtk_notify_emails(GaimConnection *gc, size_t count, gboolean detailed,
					   const char **subjects, const char **froms,
					   const char **tos, const char **urls)
{
	GaimNotifyMailData *data = NULL;
	GtkWidget *dialog = NULL;
	GtkWidget *vbox = NULL;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *img;
	char *detail_text;
	char *label_text;
	GtkTreeIter iter;
	GaimAccount *account;

	account = gaim_connection_get_account(gc);

	if (mail_dialog == NULL || !detailed)
	{
		GtkCellRenderer *rend;
		GtkTreeViewColumn *column;
		GtkWidget *button = NULL;

		dialog = gtk_dialog_new_with_buttons(_("New Mail"), NULL, 0,
											 GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
											 NULL);
		if (detailed)
			gtk_window_set_role(GTK_WINDOW(dialog), "new_mail_detailed");
		else
			gtk_window_set_role(GTK_WINDOW(dialog), "new_mail");

		if (!detailed && urls)
		{
			gtk_dialog_add_button(GTK_DIALOG(dialog),
					GAIM_STOCK_OPEN_MAIL, GTK_RESPONSE_OK);
		}
		else if (detailed && mail_dialog == NULL)
		{
			gtk_dialog_add_button(GTK_DIALOG(dialog),
					 _("Open All Messages"), GTK_RESPONSE_ACCEPT);

			button = gtk_dialog_add_button(GTK_DIALOG(dialog),
						 GAIM_STOCK_OPEN_MAIL, GTK_RESPONSE_YES);
		}

		/* Setup the dialog */
		gtk_container_set_border_width(GTK_CONTAINER(dialog), GAIM_HIG_BOX_SPACE);
		gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), GAIM_HIG_BOX_SPACE);
		gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
		gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog)->vbox), GAIM_HIG_BORDER);

		/* Setup the main horizontal box */
		hbox = gtk_hbox_new(FALSE, GAIM_HIG_BORDER);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), hbox);

		/* Dialog icon */
		img = gtk_image_new_from_stock(GAIM_STOCK_DIALOG_INFO,
									   GTK_ICON_SIZE_DIALOG);
		gtk_misc_set_alignment(GTK_MISC(img), 0, 0);
		gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);

		/* Vertical box */
		vbox = gtk_vbox_new(FALSE, GAIM_HIG_BORDER);

		gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

		if (mail_dialog == NULL && detailed)
		{
			GtkWidget *sw;

			/* Golden ratio it up! */
			gtk_widget_set_size_request(dialog, 475, 200);

			sw = gtk_scrolled_window_new(NULL, NULL);
			gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);
			gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

			mail_dialog = g_new0(GaimMailDialog, 1);
			mail_dialog->dialog = dialog;
			mail_dialog->open_button = button;

			mail_dialog->treemodel = gtk_tree_store_new(COLUMNS_GAIM_MAIL,
							GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
			mail_dialog->treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(mail_dialog->treemodel));

			g_signal_connect(G_OBJECT(dialog), "response",
							 G_CALLBACK(email_response_cb), mail_dialog);
			g_signal_connect(G_OBJECT(gtk_tree_view_get_selection(GTK_TREE_VIEW(mail_dialog->treeview))),
							 "changed", G_CALLBACK(selection_changed_cb), mail_dialog);

			/* Account column */
			column = gtk_tree_view_column_new();
			gtk_tree_view_column_set_resizable(column, TRUE);
			gtk_tree_view_column_set_title(column, _("Account"));
			rend = gtk_cell_renderer_pixbuf_new();
			gtk_tree_view_column_pack_start(column, rend, FALSE);
			gtk_tree_view_column_set_attributes(column, rend, "pixbuf", GAIM_MAIL_ICON, NULL);
			rend = gtk_cell_renderer_text_new();
			gtk_tree_view_column_pack_start(column, rend, TRUE);
			gtk_tree_view_column_set_attributes(column, rend, "markup", GAIM_MAIL_TO, NULL);
			gtk_tree_view_append_column(GTK_TREE_VIEW(mail_dialog->treeview), column);

			/* From column */
			column = gtk_tree_view_column_new();
			gtk_tree_view_column_set_resizable(column, TRUE);
			gtk_tree_view_column_set_title(column, _("From"));
			rend = gtk_cell_renderer_text_new();
			gtk_tree_view_column_pack_start(column, rend, TRUE);
			gtk_tree_view_column_set_attributes(column, rend, "markup", GAIM_MAIL_FROM, NULL);
			gtk_tree_view_append_column(GTK_TREE_VIEW(mail_dialog->treeview), column);

			/* Subject column */
			column = gtk_tree_view_column_new();
			gtk_tree_view_column_set_resizable(column, TRUE);
			gtk_tree_view_column_set_title(column, _("Subject"));
			rend = gtk_cell_renderer_text_new();
			gtk_tree_view_column_pack_start(column, rend, TRUE);
			gtk_tree_view_column_set_attributes(column, rend, "markup", GAIM_MAIL_SUBJECT, NULL);
			gtk_tree_view_append_column(GTK_TREE_VIEW(mail_dialog->treeview), column);

			gtk_container_add(GTK_CONTAINER(sw), mail_dialog->treeview);

			label = gtk_label_new(NULL);
			gtk_label_set_markup(GTK_LABEL(label), _("<span weight=\"bold\" size=\"larger\">You have mail!</span>"));
			gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
			gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
			gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
			gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
		}
	}

	if (detailed)
	{
		dialog = mail_dialog->dialog;
		while (count--)
		{
			char *to_text = NULL;
			char *from_text = NULL;
			char *subject_text = NULL;
			GdkPixbuf *pixbuf;

			if (tos != NULL)
				to_text = g_markup_escape_text(*tos, -1);
			if (froms != NULL)
				from_text = g_markup_escape_text(*froms, -1);
			if (subjects != NULL)
				subject_text = g_markup_escape_text(*subjects, -1);

			data = g_new0(GaimNotifyMailData, 1);
			if (urls != NULL)
				data->url = g_strdup(*urls);

			pixbuf = gaim_gtk_create_prpl_icon(account, 0.5);

			gtk_tree_store_append(mail_dialog->treemodel, &iter, NULL);
			gtk_tree_store_set(mail_dialog->treemodel, &iter,
									GAIM_MAIL_ICON, pixbuf,
									GAIM_MAIL_TO, to_text,
									GAIM_MAIL_FROM, from_text,
									GAIM_MAIL_SUBJECT, subject_text,
									GAIM_MAIL_DATA, data,
									-1);
			if (pixbuf != NULL)
				g_object_unref(pixbuf);
			g_free(to_text);
			g_free(from_text);
			g_free(subject_text);
			data->iter = iter;

			if (urls != NULL)
				urls++;
			if (froms != NULL)
				froms++;
			if (subjects != NULL)
				subjects++;
			if (tos != NULL)
				tos++;
		}
	}
	else
	{
		data = g_new0(GaimNotifyMailData, 1);

		if (urls != NULL)
			data->url = g_strdup(*urls);

		g_signal_connect(G_OBJECT(dialog), "response",
						 G_CALLBACK(email_nondetailed_cb), data);

		detail_text = g_strdup_printf(ngettext("%s has %d new message.",
											   "%s has %d new messages.",
											   (int)count),
									  *tos, (int)count);
		label_text = g_strdup_printf(
			_("<span weight=\"bold\" size=\"larger\">You have mail!</span>"
			"\n\n%s"), detail_text);

		label = gtk_label_new(NULL);

		gtk_label_set_markup(GTK_LABEL(label), label_text);
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

		g_free(label_text);
		g_free(detail_text);
		gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	}
	gtk_widget_show_all(dialog);

	return data;
}

static gboolean
formatted_input_cb(GtkWidget *win, GdkEventKey *event, gpointer data)
{
	if (event->keyval == GDK_Escape)
	{
		gaim_notify_close(GAIM_NOTIFY_FORMATTED, win);

		return TRUE;
	}

	return FALSE;
}

static void *
gaim_gtk_notify_formatted(const char *title, const char *primary,
						  const char *secondary, const char *text)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *button;
	GtkWidget *imhtml;
	GtkWidget *frame;
	int options = 0;
	char label_text[2048];
	char *linked_text, *primary_esc, *secondary_esc;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), title);
	gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_container_set_border_width(GTK_CONTAINER(window), GAIM_HIG_BORDER);

	g_signal_connect(G_OBJECT(window), "delete_event",
					 G_CALLBACK(formatted_close_cb), NULL);

	/* Setup the main vbox */
	vbox = gtk_vbox_new(FALSE, GAIM_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_widget_show(vbox);

	/* Setup the descriptive label */
	primary_esc = g_markup_escape_text(primary, -1);
	secondary_esc = (secondary != NULL) ? g_markup_escape_text(secondary, -1) : NULL;
	g_snprintf(label_text, sizeof(label_text),
			   "<span weight=\"bold\" size=\"larger\">%s</span>%s%s",
			   primary_esc,
			   (secondary ? "\n" : ""),
			   (secondary ? secondary_esc : ""));
	g_free(primary_esc);
	g_free(secondary_esc);

	label = gtk_label_new(NULL);

	gtk_label_set_markup(GTK_LABEL(label), label_text);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	/* Add the imhtml */
	frame = gaim_gtk_create_imhtml(FALSE, &imhtml, NULL, NULL);
	gtk_widget_set_name(imhtml, "gaim_gtknotify_imhtml");
	gtk_imhtml_set_format_functions(GTK_IMHTML(imhtml),
			gtk_imhtml_get_format_functions(GTK_IMHTML(imhtml)) | GTK_IMHTML_IMAGE);
	gtk_widget_set_size_request(imhtml, 300, 250);
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
	gtk_widget_show(frame);

	/* Add the Close button. */
	button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	g_signal_connect_swapped(G_OBJECT(button), "clicked",
							 G_CALLBACK(formatted_close_cb), window);
	g_signal_connect(G_OBJECT(window), "key_press_event",
					 G_CALLBACK(formatted_input_cb), NULL);

	/* Add the text to the gtkimhtml */
	if (!gaim_prefs_get_bool("/gaim/gtk/conversations/show_incoming_formatting"))
		options |= GTK_IMHTML_NO_COLOURS | GTK_IMHTML_NO_FONTS | GTK_IMHTML_NO_SIZES;

	options |= GTK_IMHTML_NO_COMMENTS;
	options |= GTK_IMHTML_NO_TITLE;
	options |= GTK_IMHTML_NO_NEWLINE;
	options |= GTK_IMHTML_NO_SCROLL;

	/* Make sure URLs are clickable */
	linked_text = gaim_markup_linkify(text);
	gtk_imhtml_append_text(GTK_IMHTML(imhtml), linked_text, options);
	g_free(linked_text);

	/* Show the window */
	gtk_widget_show(window);

	return window;
}

static void
gaim_gtk_notify_searchresults_new_rows(GaimConnection *gc, GaimNotifySearchResults *results,
									   void *data_)
{
	GaimNotifySearchResultsData *data = data_;
	GtkListStore *model = data->model;
	GtkTreeIter iter;
	GdkPixbuf *pixbuf;
	guint col_num;
	guint i;
	guint j;

	gtk_list_store_clear(data->model);

	pixbuf = gaim_gtk_create_prpl_icon(gaim_connection_get_account(gc), 0.5);

	/* +1 is for the automagically created Status column. */
	col_num = gaim_notify_searchresults_get_columns_count(results) + 1;

	for (i = 0; i < gaim_notify_searchresults_get_rows_count(results); i++) {
		GList *row = gaim_notify_searchresults_row_get(results, i);

		gtk_list_store_append(model, &iter);
		gtk_list_store_set(model, &iter, 0, pixbuf, -1);

		for (j = 1; j < col_num; j++) {
			GValue v;
			char *escaped = g_markup_escape_text(g_list_nth_data(row, j - 1), -1);

			v.g_type = 0;
			g_value_init(&v, G_TYPE_STRING);
			g_value_set_string(&v, escaped);
			gtk_list_store_set_value(model, &iter, j, &v);
			g_free(escaped);
		}
	}

	if (pixbuf != NULL)
		g_object_unref(pixbuf);
}

static void *
gaim_gtk_notify_searchresults(GaimConnection *gc, const char *title,
							  const char *primary, const char *secondary,
							  GaimNotifySearchResults *results, gpointer user_data)
{
	GtkWidget *window;
	GtkWidget *treeview;
	GtkWidget *close_button;
	GType *col_types;
	GtkListStore *model;
	GtkCellRenderer *renderer;
	guint col_num;
	guint i;

	GtkWidget *vbox;
	GtkWidget *button_area;
	GtkWidget *label;
	GtkWidget *sw;
	GaimNotifySearchResultsData *data;
	char *label_text;
	char *primary_esc, *secondary_esc;

	g_return_val_if_fail(gc != NULL, NULL);
	g_return_val_if_fail(results != NULL, NULL);

	data = g_malloc(sizeof(GaimNotifySearchResultsData));
	data->user_data = user_data;
	data->results = results;

	/* Create the window */
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), (title ? title :_("Search Results")));
	gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_container_set_border_width(GTK_CONTAINER(window), GAIM_HIG_BORDER);

	g_signal_connect_swapped(G_OBJECT(window), "delete_event",
							 G_CALLBACK(searchresults_close_cb), data);

	/* Setup the main vbox */
	vbox = gtk_vbox_new(FALSE, GAIM_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_widget_show(vbox);

	/* Setup the descriptive label */
	primary_esc = (primary != NULL) ? g_markup_escape_text(primary, -1) : NULL;
	secondary_esc = (secondary != NULL) ? g_markup_escape_text(secondary, -1) : NULL;
	label_text = g_strdup_printf(
			"<span weight=\"bold\" size=\"larger\">%s</span>%s%s",
			(primary ? primary_esc : ""),
			(primary && secondary ? "\n" : ""),
			(secondary ? secondary_esc : ""));
	g_free(primary_esc);
	g_free(secondary_esc);
	label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(label), label_text);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	g_free(label_text);

	/* +1 is for the automagically created Status column. */
	col_num = gaim_notify_searchresults_get_columns_count(results) + 1;

	/* Setup the list model */
	col_types = g_new0(GType, col_num);

	/* There always is this first column. */
	col_types[0] = GDK_TYPE_PIXBUF;
	for (i = 1; i < col_num; i++) {
		col_types[i] = G_TYPE_STRING;
	}
	model = gtk_list_store_newv(col_num, col_types);

	/* Setup the scrolled window containing the treeview */
	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
								   GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
										GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
	gtk_widget_show(sw);

	/* Setup the treeview */
	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(model));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);
	gtk_widget_set_size_request(treeview, 500, 400);
	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview)),
								GTK_SELECTION_SINGLE);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), TRUE);
	gtk_container_add(GTK_CONTAINER(sw), treeview);
	gtk_widget_show(treeview);

	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview),
					-1, "", renderer, "pixbuf", 0, NULL);

	for (i = 1; i < col_num; i++) {
		renderer = gtk_cell_renderer_text_new();

		gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview), -1,
				gaim_notify_searchresults_column_get_title(results, i-1),
				renderer, "text", i, NULL);
	}

	/* Setup the button area */
	button_area = gtk_hbutton_box_new();
	gtk_box_pack_start(GTK_BOX(vbox), button_area, FALSE, FALSE, 0);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(button_area), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(button_area), GAIM_HIG_BORDER);
	gtk_widget_show(button_area);

	for (i = 0; i < g_list_length(results->buttons); i++) {
		GaimNotifySearchButton *b = g_list_nth_data(results->buttons, i);
		GtkWidget *button = NULL;
		switch (b->type) {
			case GAIM_NOTIFY_BUTTON_LABELED:
				if(b->label) {
					button = gtk_button_new_with_label(b->label);
				} else {
					gaim_debug_warning("gtknotify", "Missing button label");
				}
				break;
			case GAIM_NOTIFY_BUTTON_CONTINUE:
				button = gtk_button_new_from_stock(GTK_STOCK_GO_FORWARD);
				break;
			case GAIM_NOTIFY_BUTTON_ADD:
				button = gtk_button_new_from_stock(GTK_STOCK_ADD);
				break;
			case GAIM_NOTIFY_BUTTON_INFO:
				button = gtk_button_new_from_stock(GAIM_STOCK_INFO);
				break;
			case GAIM_NOTIFY_BUTTON_IM:
				button = gtk_button_new_from_stock(GAIM_STOCK_IM);
				break;
			case GAIM_NOTIFY_BUTTON_JOIN:
				button = gtk_button_new_from_stock(GAIM_STOCK_CHAT);
				break;
			case GAIM_NOTIFY_BUTTON_INVITE:
				button = gtk_button_new_from_stock(GAIM_STOCK_INVITE);
				break;
			default:
				gaim_debug_warning("gtknotify", "Incorrect button type: %d\n", b->type);
		}
		if (button != NULL) {
			GaimNotifySearchResultsButtonData *bd;

			gtk_box_pack_start(GTK_BOX(button_area), button, FALSE, FALSE, 0);
			gtk_widget_show(button);

			bd = g_new0(GaimNotifySearchResultsButtonData, 1);
			bd->button = b;
			bd->data = data;

			g_signal_connect(G_OBJECT(button), "clicked",
			                 G_CALLBACK(searchresults_callback_wrapper_cb), bd);
			g_signal_connect_swapped(G_OBJECT(button), "destroy", G_CALLBACK(g_free), bd);
		}
	}

	/* Add the Close button */
	close_button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_box_pack_start(GTK_BOX(button_area), close_button, FALSE, FALSE, 0);
	gtk_widget_show(close_button);

	g_signal_connect_swapped(G_OBJECT(close_button), "clicked",
	                         G_CALLBACK(searchresults_close_cb), data);

	data->account = gc->account;
	data->model = model;
	data->treeview = treeview;
	data->window = window;

	/* Insert rows. */
	gaim_gtk_notify_searchresults_new_rows(gc, results, data);

	/* Show the window */
	gtk_widget_show(window);
	return data;
}

static void *
gaim_gtk_notify_userinfo(GaimConnection *gc, const char *who,
						 const char *text)
{
	char *primary;
	void *ui_handle;

	primary = g_strdup_printf(_("Info for %s"), who);
	ui_handle = gaim_gtk_notify_formatted(_("Buddy Information"), primary, NULL, text);
	g_free(primary);
	return ui_handle;
}

static void
gaim_gtk_close_notify(GaimNotifyType type, void *ui_handle)
{
	if (type == GAIM_NOTIFY_EMAIL || type == GAIM_NOTIFY_EMAILS)
	{
		GaimNotifyMailData *data = (GaimNotifyMailData *)ui_handle;

		g_free(data->url);
		g_free(data);
	}
	else if (type == GAIM_NOTIFY_SEARCHRESULTS)
	{
		GaimNotifySearchResultsData *data = (GaimNotifySearchResultsData *)ui_handle;

		gtk_widget_destroy(data->window);
		gaim_notify_searchresults_free(data->results);

		g_free(data);
	}
	else if (ui_handle != NULL)
		gtk_widget_destroy(GTK_WIDGET(ui_handle));
}

#ifndef _WIN32
static gint
uri_command(const char *command, gboolean sync)
{
	gchar *tmp;
	GError *error = NULL;
	gint ret = 0;

	gaim_debug_misc("gtknotify", "Executing %s\n", command);

	if (!gaim_program_is_valid(command))
	{
		tmp = g_strdup_printf(_("The browser command \"%s\" is invalid."),
							  command ? command : "(none)");
		gaim_notify_error(NULL, NULL, _("Unable to open URL"), tmp);
		g_free(tmp);

	}
	else if (sync)
	{
		gint status;

		if (!g_spawn_command_line_sync(command, NULL, NULL, &status, &error))
		{
			tmp = g_strdup_printf(_("Error launching \"%s\": %s"),
										command, error->message);
			gaim_notify_error(NULL, NULL, _("Unable to open URL"), tmp);
			g_free(tmp);
			g_error_free(error);
		}
		else
			ret = status;
	}
	else
	{
		if (!g_spawn_command_line_async(command, &error))
		{
			tmp = g_strdup_printf(_("Error launching \"%s\": %s"),
										command, error->message);
			gaim_notify_error(NULL, NULL, _("Unable to open URL"), tmp);
			g_free(tmp);
			g_error_free(error);
		}
	}

	return ret;
}
#endif /* _WIN32 */

static void *
gaim_gtk_notify_uri(const char *uri)
{
#ifndef _WIN32
	char *escaped = g_shell_quote(uri);
	char *command = NULL;
	char *remote_command = NULL;
	const char *web_browser;
	int place;

	web_browser = gaim_prefs_get_string("/gaim/gtk/browsers/browser");
	place = gaim_prefs_get_int("/gaim/gtk/browsers/place");

	/* if they are running gnome, use the gnome web browser */
	if (gaim_running_gnome() == TRUE)
	{
		command = g_strdup_printf("gnome-open %s", escaped);
	}
	else if (!strcmp(web_browser, "epiphany") ||
		!strcmp(web_browser, "galeon"))
	{
		if (place == GAIM_BROWSER_NEW_WINDOW)
			command = g_strdup_printf("%s -w %s", web_browser, escaped);
		else if (place == GAIM_BROWSER_NEW_TAB)
			command = g_strdup_printf("%s -n %s", web_browser, escaped);
		else
			command = g_strdup_printf("%s %s", web_browser, escaped);
	}
	else if (!strcmp(web_browser, "gnome-open"))
	{
		command = g_strdup_printf("gnome-open %s", escaped);
	}
	else if (!strcmp(web_browser, "kfmclient"))
	{
		command = g_strdup_printf("kfmclient openURL %s", escaped);
		/*
		 * Does Konqueror have options to open in new tab
		 * and/or current window?
		 */
	}
	else if (!strcmp(web_browser, "mozilla") ||
			 !strcmp(web_browser, "mozilla-firebird") ||
			 !strcmp(web_browser, "firefox"))
	{
		char *args = "";

		command = g_strdup_printf("%s %s", web_browser, escaped);

		/*
		 * Firefox 0.9 and higher require a "-a firefox" option when
		 * using -remote commands.  This breaks older versions of
		 * mozilla.  So we include this other handly little string
		 * when calling firefox.  If the API for remote calls changes
		 * any more in firefox then firefox should probably be split
		 * apart from mozilla-firebird and mozilla... but this is good
		 * for now.
		 */
		if (!strcmp(web_browser, "firefox"))
			args = "-a firefox";

		if (place == GAIM_BROWSER_NEW_WINDOW)
			remote_command = g_strdup_printf("%s %s -remote "
											 "openURL(%s,new-window)",
											 web_browser, args, escaped);
		else if (place == GAIM_BROWSER_NEW_TAB)
			remote_command = g_strdup_printf("%s %s -remote "
											 "openURL(%s,new-tab)",
											 web_browser, args, escaped);
		else if (place == GAIM_BROWSER_CURRENT)
			remote_command = g_strdup_printf("%s %s -remote "
											 "openURL(%s)",
											 web_browser, args, escaped);
	}
	else if (!strcmp(web_browser, "netscape"))
	{
		command = g_strdup_printf("netscape %s", escaped);

		if (place == GAIM_BROWSER_NEW_WINDOW)
		{
			remote_command = g_strdup_printf("netscape -remote "
											 "openURL(%s,new-window)",
											 escaped);
		}
		else if (place == GAIM_BROWSER_CURRENT)
		{
			remote_command = g_strdup_printf("netscape -remote "
											 "openURL(%s)", escaped);
		}
	}
	else if (!strcmp(web_browser, "opera"))
	{
		if (place == GAIM_BROWSER_NEW_WINDOW)
			command = g_strdup_printf("opera -newwindow %s", escaped);
		else if (place == GAIM_BROWSER_NEW_TAB)
			command = g_strdup_printf("opera -newpage %s", escaped);
		else if (place == GAIM_BROWSER_CURRENT)
		{
			remote_command = g_strdup_printf("opera -remote "
											 "openURL(%s)", escaped);
			command = g_strdup_printf("opera %s", escaped);
		}
		else
			command = g_strdup_printf("opera %s", escaped);

	}
	else if (!strcmp(web_browser, "custom"))
	{
		const char *web_command;

		web_command = gaim_prefs_get_string("/gaim/gtk/browsers/command");

		if (web_command == NULL || *web_command == '\0')
		{
			gaim_notify_error(NULL, NULL, _("Unable to open URL"),
							  _("The 'Manual' browser command has been "
								"chosen, but no command has been set."));
			return NULL;
		}

		if (strstr(web_command, "%s"))
			command = gaim_strreplace(web_command, "%s", escaped);
		else
		{
			/*
			 * There is no "%s" in the browser command.  Assume the user
			 * wanted the URL tacked on to the end of the command.
			 */
			command = g_strdup_printf("%s %s", web_command, escaped);
		}
	}

	g_free(escaped);

	if (remote_command != NULL)
	{
		/* try the remote command first */
		if (uri_command(remote_command, TRUE) != 0)
			uri_command(command, FALSE);

		g_free(remote_command);

	}
	else
		uri_command(command, FALSE);

	g_free(command);

#else /* !_WIN32 */
	gtkwgaim_notify_uri(uri);
#endif /* !_WIN32 */

	return NULL;
}

static GaimNotifyUiOps ops =
{
	gaim_gtk_notify_message,
	gaim_gtk_notify_email,
	gaim_gtk_notify_emails,
	gaim_gtk_notify_formatted,
	gaim_gtk_notify_searchresults,
	gaim_gtk_notify_searchresults_new_rows,
	gaim_gtk_notify_userinfo,
	gaim_gtk_notify_uri,
	gaim_gtk_close_notify
};

GaimNotifyUiOps *
gaim_gtk_notify_get_ui_ops(void)
{
	return &ops;
}
