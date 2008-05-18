/**
 * @file gtknotify.c GTK+ Notification API
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

#include <gdk/gdkkeysyms.h>

#include "connection.h"
#include "debug.h"
#include "prefs.h"
#include "pidginstock.h"
#include "util.h"

#include "gtkblist.h"
#include "gtkimhtml.h"
#include "gtknotify.h"
#include "gtkutils.h"

typedef struct
{
	GtkWidget *window;
	int count;
} PidginUserInfo;

typedef struct
{
	PurpleAccount *account;
	char *url;
	GtkWidget *label;
	GtkTreeIter iter;
	int count;
	gboolean purple_has_handle;
} PidginNotifyMailData;

typedef struct
{
	PurpleAccount *account;
	GtkListStore *model;
	GtkWidget *treeview;
	GtkWidget *window;
	gpointer user_data;
	PurpleNotifySearchResults *results;

} PidginNotifySearchResultsData;

typedef struct
{
	PurpleNotifySearchButton *button;
	PidginNotifySearchResultsData *data;

} PidginNotifySearchResultsButtonData;

enum
{
	PIDGIN_MAIL_ICON,
	PIDGIN_MAIL_TEXT,
	PIDGIN_MAIL_DATA,
	COLUMNS_PIDGIN_MAIL
};

typedef struct _PidginMailDialog PidginMailDialog;

struct _PidginMailDialog
{
	GtkWidget *dialog;
	GtkWidget *treeview;
	GtkTreeStore *treemodel;
	GtkLabel *label;
	GtkWidget *open_button;
	int total_count;
	gboolean in_use;
};

static PidginMailDialog *mail_dialog = NULL;

static void *pidgin_notify_emails(PurpleConnection *gc, size_t count, gboolean detailed,
									const char **subjects,
									const char **froms, const char **tos,
									const char **urls);

static void pidgin_close_notify(PurpleNotifyType type, void *ui_handle);

static void
message_response_cb(GtkDialog *dialog, gint id, GtkWidget *widget)
{
	purple_notify_close(PURPLE_NOTIFY_MESSAGE, widget);
}

static void
email_response_cb(GtkDialog *dlg, gint id, PidginMailDialog *dialog)
{
	PidginNotifyMailData *data = NULL;
	GtkTreeIter iter;

	if (id == GTK_RESPONSE_YES)
	{
		GtkTreeSelection *selection;

		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->treeview));

		if (gtk_tree_selection_get_selected(selection, NULL, &iter))
		{
			gtk_tree_model_get(GTK_TREE_MODEL(dialog->treemodel), &iter,
								PIDGIN_MAIL_DATA, &data, -1);
			purple_notify_uri(NULL, data->url);

			gtk_tree_store_remove(dialog->treemodel, &iter);
			purple_notify_close(PURPLE_NOTIFY_EMAILS, data);

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
								PIDGIN_MAIL_DATA, &data, -1);

			if (id == GTK_RESPONSE_ACCEPT)
				purple_notify_uri(NULL, data->url);

			gtk_tree_store_remove(dialog->treemodel, &iter);
			if (data->purple_has_handle)
				purple_notify_close(PURPLE_NOTIFY_EMAILS, data);
			else
				pidgin_close_notify(PURPLE_NOTIFY_EMAILS, data);
		}
	}
	gtk_widget_destroy(dialog->dialog);
	g_free(dialog);
	mail_dialog = NULL;
}

static void email_row_activated_cb(GtkTreeView *tv, GtkTreePath *path, GtkTreeViewColumn *col, gpointer data) {
	email_response_cb(GTK_DIALOG(mail_dialog->dialog), GTK_RESPONSE_YES, mail_dialog);
}

static void
reset_mail_dialog(GtkDialog *dialog)
{
	if (mail_dialog->in_use)
		return;
	gtk_widget_destroy(mail_dialog->dialog);
	g_free(mail_dialog);
	mail_dialog = NULL;
}

static gboolean
formatted_close_cb(GtkWidget *win, GdkEvent *event, void *user_data)
{
	purple_notify_close(PURPLE_NOTIFY_FORMATTED, win);
	return FALSE;
}

static gboolean
searchresults_close_cb(PidginNotifySearchResultsData *data, GdkEvent *event, gpointer user_data)
{
	purple_notify_close(PURPLE_NOTIFY_SEARCHRESULTS, data);
	return FALSE;
}

static void
searchresults_callback_wrapper_cb(GtkWidget *widget, PidginNotifySearchResultsButtonData *bd)
{
	PidginNotifySearchResultsData *data = bd->data;

	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	PurpleNotifySearchButton *button;
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
	button->callback(purple_account_get_connection(data->account), row, data->user_data);
	g_list_foreach(row, (GFunc)g_free, NULL);
	g_list_free(row);
}

static void *
pidgin_notify_message(PurpleNotifyMsgType type, const char *title,
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
		case PURPLE_NOTIFY_MSG_ERROR:
			icon_name = PIDGIN_STOCK_DIALOG_ERROR;
			break;

		case PURPLE_NOTIFY_MSG_WARNING:
			icon_name = PIDGIN_STOCK_DIALOG_WARNING;
			break;

		case PURPLE_NOTIFY_MSG_INFO:
			icon_name = PIDGIN_STOCK_DIALOG_INFO;
			break;

		default:
			icon_name = NULL;
			break;
	}

	if (icon_name != NULL)
	{
		img = gtk_image_new_from_stock(icon_name, gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_HUGE));
		gtk_misc_set_alignment(GTK_MISC(img), 0, 0);
	}

	dialog = gtk_dialog_new_with_buttons(title ? title : PIDGIN_ALERT_TITLE,
										 NULL, 0, GTK_STOCK_CLOSE,
										 GTK_RESPONSE_CLOSE, NULL);

	gtk_window_set_role(GTK_WINDOW(dialog), "notify_dialog");

	g_signal_connect(G_OBJECT(dialog), "response",
					 G_CALLBACK(message_response_cb), dialog);

	gtk_container_set_border_width(GTK_CONTAINER(dialog), PIDGIN_HIG_BORDER);
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog)->vbox), PIDGIN_HIG_BORDER);
	gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), PIDGIN_HIG_BOX_SPACE);

	hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), hbox);

	if (img != NULL)
		gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);

	primary_esc = g_markup_escape_text(primary, -1);
	secondary_esc = (secondary != NULL) ? g_markup_escape_text(secondary, -1) : NULL;
	g_snprintf(label_text, sizeof(label_text),
			   "<span weight=\"bold\" size=\"larger\">%s</span>%s%s",
			   primary_esc, (secondary ? "\n\n" : ""),
			   (secondary ? secondary_esc : ""));
	g_free(primary_esc);
	g_free(secondary_esc);

	label = gtk_label_new(NULL);

	gtk_label_set_markup(GTK_LABEL(label), label_text);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	pidgin_auto_parent_window(dialog);

	gtk_widget_show_all(dialog);

	return dialog;
}

static void
selection_changed_cb(GtkTreeSelection *sel, PidginMailDialog *dialog)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	PidginNotifyMailData *data;
	gboolean active = TRUE;

	if (gtk_tree_selection_get_selected(sel, &model, &iter) == FALSE)
		active = FALSE;
	else
	{
		gtk_tree_model_get(model, &iter, PIDGIN_MAIL_DATA, &data, -1);
		if (data->url == NULL)
			active = FALSE;
	}

	gtk_widget_set_sensitive(dialog->open_button, active);
}

static void *
pidgin_notify_email(PurpleConnection *gc, const char *subject, const char *from,
					  const char *to, const char *url)
{
	return pidgin_notify_emails(gc, 1, (subject != NULL),
								  (subject == NULL ? NULL : &subject),
								  (from    == NULL ? NULL : &from),
								  (to      == NULL ? NULL : &to),
								  (url     == NULL ? NULL : &url));
}

static int
mail_window_focus_cb(GtkWidget *widget, GdkEventFocus *focus, gpointer null)
{
	pidgin_set_urgent(GTK_WINDOW(widget), FALSE);
	return 0;
}

static GtkWidget *
pidgin_get_mail_dialog(void)
{
	if (mail_dialog == NULL) {
		GtkWidget *dialog = NULL;
		GtkWidget *label;
		GtkWidget *sw;
		GtkCellRenderer *rend;
		GtkTreeViewColumn *column;
		GtkWidget *button = NULL;
		GtkWidget *vbox = NULL;

		dialog = gtk_dialog_new_with_buttons(_("New Mail"), NULL, 0,
						     GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
						     NULL);
		gtk_window_set_role(GTK_WINDOW(dialog), "new_mail_detailed");
		g_signal_connect(G_OBJECT(dialog), "focus-in-event",
					G_CALLBACK(mail_window_focus_cb), NULL);

		gtk_dialog_add_button(GTK_DIALOG(dialog),
					 _("Open All Messages"), GTK_RESPONSE_ACCEPT);

		button = gtk_dialog_add_button(GTK_DIALOG(dialog),
						 PIDGIN_STOCK_OPEN_MAIL, GTK_RESPONSE_YES);

		/* Setup the dialog */
		gtk_container_set_border_width(GTK_CONTAINER(dialog), PIDGIN_HIG_BOX_SPACE);
		gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), PIDGIN_HIG_BOX_SPACE);
		gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
		gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog)->vbox), PIDGIN_HIG_BORDER);

		/* Vertical box */
		vbox = GTK_DIALOG(dialog)->vbox;

		/* Golden ratio it up! */
		gtk_widget_set_size_request(dialog, 550, 400);

		sw = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

		mail_dialog = g_new0(PidginMailDialog, 1);
		mail_dialog->dialog = dialog;
		mail_dialog->open_button = button;

		mail_dialog->treemodel = gtk_tree_store_new(COLUMNS_PIDGIN_MAIL,
						GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_POINTER);
		mail_dialog->treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(mail_dialog->treemodel));
		g_object_unref(G_OBJECT(mail_dialog->treemodel));
		gtk_tree_view_set_search_column(GTK_TREE_VIEW(mail_dialog->treeview), PIDGIN_MAIL_TEXT);
		gtk_tree_view_set_search_equal_func(GTK_TREE_VIEW(mail_dialog->treeview),
			             pidgin_tree_view_search_equal_func, NULL, NULL);

		g_signal_connect(G_OBJECT(dialog), "response",
						 G_CALLBACK(email_response_cb), mail_dialog);
		g_signal_connect(G_OBJECT(gtk_tree_view_get_selection(GTK_TREE_VIEW(mail_dialog->treeview))),
						 "changed", G_CALLBACK(selection_changed_cb), mail_dialog);
		g_signal_connect(G_OBJECT(mail_dialog->treeview), "row-activated", G_CALLBACK(email_row_activated_cb), NULL);

		gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(mail_dialog->treeview), FALSE);
		gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(mail_dialog->treeview), TRUE);
		gtk_container_add(GTK_CONTAINER(sw), mail_dialog->treeview);

		column = gtk_tree_view_column_new();
		gtk_tree_view_column_set_resizable(column, TRUE);
		rend = gtk_cell_renderer_pixbuf_new();
		gtk_tree_view_column_pack_start(column, rend, FALSE);
		gtk_tree_view_column_set_attributes(column, rend, "pixbuf", PIDGIN_MAIL_ICON, NULL);
		rend = gtk_cell_renderer_text_new();
		gtk_tree_view_column_pack_start(column, rend, TRUE);
		gtk_tree_view_column_set_attributes(column, rend, "markup", PIDGIN_MAIL_TEXT, NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(mail_dialog->treeview), column);

		label = gtk_label_new(NULL);
		gtk_label_set_markup(GTK_LABEL(label), _("<span weight=\"bold\" size=\"larger\">You have mail!</span>"));
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
	}

	return mail_dialog->dialog;
}

/* count == 0 means this is a detailed mail notification.
 * count > 0 mean non-detailed.
 */
static void *
pidgin_notify_add_mail(GtkTreeStore *treemodel, PurpleAccount *account, char *notification, const char *url, int count, gboolean clear, gboolean *new_data)
{
	PidginNotifyMailData *data = NULL;
	GtkTreeIter iter;
	GdkPixbuf *icon;
	gboolean new_n = TRUE;

	if (count > 0 || clear) {
		/* Allow only one non-detailed email notification for each account */
		if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(treemodel), &iter)) {
			gboolean advanced;
			do {
				advanced = FALSE;
				gtk_tree_model_get(GTK_TREE_MODEL(treemodel), &iter,
						PIDGIN_MAIL_DATA, &data, -1);
				if (data->account == account) {
					if (clear) {
#if GTK_CHECK_VERSION(2,2,0)
						advanced = gtk_tree_store_remove(treemodel, &iter);
#else
						gtk_tree_store_remove(treemodel, &iter);
						advanced = (iter.stamp == 0) ? FALSE : TRUE;
#endif
						purple_notify_close(PURPLE_NOTIFY_EMAILS, data);
						/* We're completely done if we've processed all entries */
						if (!advanced)
							return NULL;
					} else if (data->count > 0) {
						new_n = FALSE;
						g_free(data->url);
						data->url = NULL;
						mail_dialog->total_count -= data->count;
						break;
					}
				}
			} while (advanced || gtk_tree_model_iter_next(GTK_TREE_MODEL(treemodel), &iter));
		}
	}

	if (clear)
		return NULL;

	icon = pidgin_create_prpl_icon(account, PIDGIN_PRPL_ICON_MEDIUM);

	if (new_n) {
		data = g_new0(PidginNotifyMailData, 1);
		data->purple_has_handle = TRUE;
		gtk_tree_store_append(treemodel, &iter, NULL);
	}

	if (url != NULL)
		data->url = g_strdup(url);

	gtk_tree_store_set(treemodel, &iter,
								PIDGIN_MAIL_ICON, icon,
								PIDGIN_MAIL_TEXT, notification,
								PIDGIN_MAIL_DATA, data,
								-1);
	data->iter = iter;              /* XXX: Do we use this for something? */
	data->account = account;
	data->count = count;

	/* Why is this necessary?*/
	gtk_tree_model_get(GTK_TREE_MODEL(treemodel), &iter,
						PIDGIN_MAIL_DATA, &data, -1);
	if (icon)
		g_object_unref(icon);

	if (new_data)
		*new_data = new_n;
	return data;
}

static void *
pidgin_notify_emails(PurpleConnection *gc, size_t count, gboolean detailed,
					   const char **subjects, const char **froms,
					   const char **tos, const char **urls)
{
	GtkWidget *dialog = NULL;
	char *notification;
	PurpleAccount *account;
	PidginNotifyMailData *data = NULL, *data2;
	gboolean new_data;

	/* Don't bother updating if there aren't new emails and we don't have any displayed currently */
	if (count == 0 && mail_dialog == NULL)
		return NULL;

	account = purple_connection_get_account(gc);
	dialog = pidgin_get_mail_dialog();  /* This creates mail_dialog if necessary */

	mail_dialog->total_count += count;
	if (detailed) {
		while (count--) {
			char *to_text = NULL;
			char *from_text = NULL;
			char *subject_text = NULL;
			char *tmp;
			gboolean first = TRUE;

			if (tos != NULL) {
				tmp = g_markup_escape_text(*tos, -1);
				to_text = g_strdup_printf("<b>%s</b>: %s\n", _("Account"), tmp);
				g_free(tmp);
				first = FALSE;
				tos++;
			}
			if (froms != NULL) {
				tmp = g_markup_escape_text(*froms, -1);
				from_text = g_strdup_printf("%s<b>%s</b>: %s\n", first ? "<br>" : "", _("Sender"), tmp);
				g_free(tmp);
				first = FALSE;
				froms++;
			}
			if (subjects != NULL) {
				tmp = g_markup_escape_text(*subjects, -1);
				subject_text = g_strdup_printf("%s<b>%s</b>: %s", first ? "<br>" : "", _("Subject"), tmp);
				g_free(tmp);
				first = FALSE;
				subjects++;
			}
#define SAFE(x) ((x) ? (x) : "")
			notification = g_strdup_printf("%s%s%s", SAFE(to_text), SAFE(from_text), SAFE(subject_text));
#undef SAFE
			g_free(to_text);
			g_free(from_text);
			g_free(subject_text);

			/* If we don't keep track of this, will leak "data" for each of the notifications except the last */
			data2 = pidgin_notify_add_mail(mail_dialog->treemodel, account, notification, urls ? *urls : NULL, 0, FALSE, &new_data);
			if (new_data) {
				if (data)
					data->purple_has_handle = FALSE;
				data = data2;
			}
			g_free(notification);

			if (urls != NULL)
				urls++;
		}
	} else {
		if (count > 0) {
			notification = g_strdup_printf(ngettext("%s has %d new message.",
							   "%s has %d new messages.",
							   (int)count),
							   *tos, (int)count);
			data2 = pidgin_notify_add_mail(mail_dialog->treemodel, account, notification, urls ? *urls : NULL, count, FALSE, &new_data);
			if (new_data) {
				if (data)
					data->purple_has_handle = FALSE;
				data = data2;
			}
			g_free(notification);
		} else {
			GtkTreeIter iter;

			/* Clear out all mails for the account */
			pidgin_notify_add_mail(mail_dialog->treemodel, account, NULL, NULL, 0, TRUE, NULL);

			if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(mail_dialog->treemodel), &iter)) {
				/* There is no API to clear the headline specifically */
				/* This will trigger reset_mail_dialog() */
				pidgin_blist_set_headline(NULL, NULL, NULL, NULL, NULL);
				return NULL;
			}
		}
	}

	if (!GTK_WIDGET_VISIBLE(dialog)) {
		GdkPixbuf *pixbuf = gtk_widget_render_icon(dialog, PIDGIN_STOCK_DIALOG_MAIL,
							   gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL), NULL);
		char *label_text = g_strdup_printf(ngettext("<b>%d new e-mail.</b>",
							    "<b>%d new e-mails.</b>",
							    mail_dialog->total_count), mail_dialog->total_count);
		mail_dialog->in_use = TRUE;     /* So that _set_headline doesn't accidentally
										   remove the notifications when replacing an
										   old notification. */
		pidgin_blist_set_headline(label_text,
					    pixbuf, G_CALLBACK(gtk_widget_show_all), dialog,
					    (GDestroyNotify)reset_mail_dialog);
		mail_dialog->in_use = FALSE;
		g_free(label_text);
		if (pixbuf)
			g_object_unref(pixbuf);
	} else if (!GTK_WIDGET_HAS_FOCUS(dialog))
		pidgin_set_urgent(GTK_WINDOW(dialog), TRUE);

	return data;
}

static gboolean
formatted_input_cb(GtkWidget *win, GdkEventKey *event, gpointer data)
{
	if (event->keyval == GDK_Escape)
	{
		purple_notify_close(PURPLE_NOTIFY_FORMATTED, win);

		return TRUE;
	}

	return FALSE;
}

static GtkIMHtmlOptions
notify_imhtml_options(void)
{
	GtkIMHtmlOptions options = 0;

	if (!purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/show_incoming_formatting"))
		options |= GTK_IMHTML_NO_COLOURS | GTK_IMHTML_NO_FONTS | GTK_IMHTML_NO_SIZES;

	options |= GTK_IMHTML_NO_COMMENTS;
	options |= GTK_IMHTML_NO_TITLE;
	options |= GTK_IMHTML_NO_NEWLINE;
	options |= GTK_IMHTML_NO_SCROLL;
	return options;
}

static void *
pidgin_notify_formatted(const char *title, const char *primary,
						  const char *secondary, const char *text)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *button;
	GtkWidget *imhtml;
	GtkWidget *frame;
	char label_text[2048];
	char *linked_text, *primary_esc, *secondary_esc;

	window = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(window), title);
	gtk_container_set_border_width(GTK_CONTAINER(window), PIDGIN_HIG_BORDER);
	gtk_window_set_resizable(GTK_WINDOW(window), TRUE);

	g_signal_connect(G_OBJECT(window), "delete_event",
					 G_CALLBACK(formatted_close_cb), NULL);

	/* Setup the main vbox */
	vbox = GTK_DIALOG(window)->vbox;

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
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	/* Add the imhtml */
	frame = pidgin_create_imhtml(FALSE, &imhtml, NULL, NULL);
	gtk_widget_set_name(imhtml, "pidgin_notify_imhtml");
	gtk_imhtml_set_format_functions(GTK_IMHTML(imhtml),
			gtk_imhtml_get_format_functions(GTK_IMHTML(imhtml)) | GTK_IMHTML_IMAGE);
	gtk_widget_set_size_request(imhtml, 300, 250);
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
	gtk_widget_show(frame);

	/* Add the Close button. */
	button = gtk_dialog_add_button(GTK_DIALOG(window), GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);
	gtk_widget_grab_focus(button);

	g_signal_connect_swapped(G_OBJECT(button), "clicked",
							 G_CALLBACK(gtk_widget_destroy), window);
	g_signal_connect(G_OBJECT(window), "key_press_event",
					 G_CALLBACK(formatted_input_cb), NULL);

	/* Make sure URLs are clickable */
	linked_text = purple_markup_linkify(text);
	gtk_imhtml_append_text(GTK_IMHTML(imhtml), linked_text, notify_imhtml_options());
	g_free(linked_text);

	g_object_set_data(G_OBJECT(window), "info-widget", imhtml);

	/* Show the window */
	pidgin_auto_parent_window(window);

	gtk_widget_show(window);

	return window;
}

static void
pidgin_notify_searchresults_new_rows(PurpleConnection *gc, PurpleNotifySearchResults *results,
									   void *data_)
{
	PidginNotifySearchResultsData *data = data_;
	GtkListStore *model = data->model;
	GtkTreeIter iter;
	GdkPixbuf *pixbuf;
	guint col_num;
	GList *row, *column;
	guint n;

	gtk_list_store_clear(data->model);

	pixbuf = pidgin_create_prpl_icon(purple_connection_get_account(gc), 0.5);

	/* +1 is for the automagically created Status column. */
	col_num = g_list_length(results->columns) + 1;

	for (row = results->rows; row != NULL; row = row->next) {

		gtk_list_store_append(model, &iter);
		gtk_list_store_set(model, &iter, 0, pixbuf, -1);

		n = 1;
		for (column = row->data; column != NULL; column = column->next) {
			GValue v;

			v.g_type = 0;
			g_value_init(&v, G_TYPE_STRING);
			g_value_set_string(&v, column->data);
			gtk_list_store_set_value(model, &iter, n, &v);
			n++;
		}
	}

	if (pixbuf != NULL)
		g_object_unref(pixbuf);
}

static void *
pidgin_notify_searchresults(PurpleConnection *gc, const char *title,
							  const char *primary, const char *secondary,
							  PurpleNotifySearchResults *results, gpointer user_data)
{
	GtkWidget *window;
	GtkWidget *treeview;
	GtkWidget *close_button;
	GType *col_types;
	GtkListStore *model;
	GtkCellRenderer *renderer;
	guint col_num;
	GList *columniter;
	guint i;

	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *sw;
	PidginNotifySearchResultsData *data;
	char *label_text;
	char *primary_esc, *secondary_esc;

	g_return_val_if_fail(gc != NULL, NULL);
	g_return_val_if_fail(results != NULL, NULL);

	data = g_malloc(sizeof(PidginNotifySearchResultsData));
	data->user_data = user_data;
	data->results = results;

	/* Create the window */
	window = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(window), title ? title :_("Search Results"));
	gtk_container_set_border_width(GTK_CONTAINER(window), PIDGIN_HIG_BORDER);
	gtk_window_set_resizable(GTK_WINDOW(window), TRUE);

	g_signal_connect_swapped(G_OBJECT(window), "delete_event",
							 G_CALLBACK(searchresults_close_cb), data);

	/* Setup the main vbox */
	vbox = GTK_DIALOG(window)->vbox;

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
	col_num = g_list_length(results->columns) + 1;

	/* Setup the list model */
	col_types = g_new0(GType, col_num);

	/* There always is this first column. */
	col_types[0] = GDK_TYPE_PIXBUF;
	for (i = 1; i < col_num; i++) {
		col_types[i] = G_TYPE_STRING;
	}
	model = gtk_list_store_newv(col_num, col_types);
	g_free(col_types);

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
	g_object_unref(G_OBJECT(model));
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

	i = 1;
	for (columniter = results->columns; columniter != NULL; columniter = columniter->next) {
		PurpleNotifySearchColumn *column = columniter->data;
		renderer = gtk_cell_renderer_text_new();

		gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview), -1,
				column->title, renderer, "text", i, NULL);
		i++;
	}

	for (i = 0; i < g_list_length(results->buttons); i++) {
		PurpleNotifySearchButton *b = g_list_nth_data(results->buttons, i);
		GtkWidget *button = NULL;
		switch (b->type) {
			case PURPLE_NOTIFY_BUTTON_LABELED:
				if(b->label) {
					button = gtk_button_new_with_label(b->label);
				} else {
					purple_debug_warning("gtknotify", "Missing button label\n");
				}
				break;
			case PURPLE_NOTIFY_BUTTON_CONTINUE:
				button = gtk_dialog_add_button(GTK_DIALOG(window), GTK_STOCK_GO_FORWARD, GTK_RESPONSE_NONE);
				break;
			case PURPLE_NOTIFY_BUTTON_ADD:
				button = gtk_dialog_add_button(GTK_DIALOG(window), GTK_STOCK_ADD, GTK_RESPONSE_NONE);
				break;
			case PURPLE_NOTIFY_BUTTON_INFO:
				button = gtk_dialog_add_button(GTK_DIALOG(window), PIDGIN_STOCK_TOOLBAR_USER_INFO, GTK_RESPONSE_NONE);
				break;
			case PURPLE_NOTIFY_BUTTON_IM:
				button = gtk_dialog_add_button(GTK_DIALOG(window), PIDGIN_STOCK_TOOLBAR_MESSAGE_NEW, GTK_RESPONSE_NONE);
				break;
			case PURPLE_NOTIFY_BUTTON_JOIN:
				button = gtk_dialog_add_button(GTK_DIALOG(window), PIDGIN_STOCK_CHAT, GTK_RESPONSE_NONE);
				break;
			case PURPLE_NOTIFY_BUTTON_INVITE:
				button = gtk_dialog_add_button(GTK_DIALOG(window), PIDGIN_STOCK_INVITE, GTK_RESPONSE_NONE);
				break;
			default:
				purple_debug_warning("gtknotify", "Incorrect button type: %d\n", b->type);
		}
		if (button != NULL) {
			PidginNotifySearchResultsButtonData *bd;

			bd = g_new0(PidginNotifySearchResultsButtonData, 1);
			bd->button = b;
			bd->data = data;

			g_signal_connect(G_OBJECT(button), "clicked",
			                 G_CALLBACK(searchresults_callback_wrapper_cb), bd);
			g_signal_connect_swapped(G_OBJECT(button), "destroy", G_CALLBACK(g_free), bd);
		}
	}

	/* Add the Close button */
	close_button = gtk_dialog_add_button(GTK_DIALOG(window), GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);

	g_signal_connect_swapped(G_OBJECT(close_button), "clicked",
	                         G_CALLBACK(searchresults_close_cb), data);

	data->account = gc->account;
	data->model = model;
	data->treeview = treeview;
	data->window = window;

	/* Insert rows. */
	pidgin_notify_searchresults_new_rows(gc, results, data);

	/* Show the window */
	pidgin_auto_parent_window(window);

	gtk_widget_show(window);
	return data;
}

/** Xerox'ed from Finch! How the tables have turned!! ;) **/
/** User information. **/
static GHashTable *userinfo;

static char *
userinfo_hash(PurpleAccount *account, const char *who)
{
	char key[256];
	snprintf(key, sizeof(key), "%s - %s", purple_account_get_username(account), purple_normalize(account, who));
	return g_utf8_strup(key, -1);
}

static void
remove_userinfo(GtkWidget *widget, gpointer key)
{
	PidginUserInfo *pinfo = g_hash_table_lookup(userinfo, key);

	while (pinfo->count--)
		purple_notify_close(PURPLE_NOTIFY_USERINFO, widget);

	g_hash_table_remove(userinfo, key);
}

static void *
pidgin_notify_userinfo(PurpleConnection *gc, const char *who,
						 PurpleNotifyUserInfo *user_info)
{
	char *info;
	void *ui_handle;
	char *key = userinfo_hash(purple_connection_get_account(gc), who);
	PidginUserInfo *pinfo = NULL;

	if (!userinfo) {
		userinfo = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	}

	info = purple_notify_user_info_get_text_with_newline(user_info, "<br />");
	pinfo = g_hash_table_lookup(userinfo, key);
	if (pinfo != NULL) {
		GtkIMHtml *imhtml = g_object_get_data(G_OBJECT(pinfo->window), "info-widget");
		char *linked_text = purple_markup_linkify(info);
		gtk_imhtml_clear(imhtml);
		gtk_imhtml_append_text(imhtml, linked_text, notify_imhtml_options());
		g_free(linked_text);
		g_free(key);
		ui_handle = pinfo->window;
		pinfo->count++;
	} else {
		char *primary = g_strdup_printf(_("Info for %s"), who);
		ui_handle = pidgin_notify_formatted(_("Buddy Information"), primary, NULL, info);
		g_signal_handlers_disconnect_by_func(G_OBJECT(ui_handle), G_CALLBACK(formatted_close_cb), NULL);
		g_signal_connect(G_OBJECT(ui_handle), "destroy", G_CALLBACK(remove_userinfo), key);
		g_free(primary);
		pinfo = g_new0(PidginUserInfo, 1);
		pinfo->window = ui_handle;
		pinfo->count = 1;
		g_hash_table_insert(userinfo, key, pinfo);
	}
	g_free(info);
	return ui_handle;
}

static void
pidgin_close_notify(PurpleNotifyType type, void *ui_handle)
{
	if (type == PURPLE_NOTIFY_EMAIL || type == PURPLE_NOTIFY_EMAILS)
	{
		PidginNotifyMailData *data = (PidginNotifyMailData *)ui_handle;

		g_free(data->url);
		g_free(data);
	}
	else if (type == PURPLE_NOTIFY_SEARCHRESULTS)
	{
		PidginNotifySearchResultsData *data = (PidginNotifySearchResultsData *)ui_handle;

		gtk_widget_destroy(data->window);
		purple_notify_searchresults_free(data->results);

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

	purple_debug_misc("gtknotify", "Executing %s\n", command);

	if (!purple_program_is_valid(command))
	{
		tmp = g_strdup_printf(_("The browser command \"%s\" is invalid."),
							  command ? command : "(none)");
		purple_notify_error(NULL, NULL, _("Unable to open URL"), tmp);
		g_free(tmp);

	}
	else if (sync)
	{
		gint status;

		if (!g_spawn_command_line_sync(command, NULL, NULL, &status, &error))
		{
			tmp = g_strdup_printf(_("Error launching \"%s\": %s"),
										command, error->message);
			purple_notify_error(NULL, NULL, _("Unable to open URL"), tmp);
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
			purple_notify_error(NULL, NULL, _("Unable to open URL"), tmp);
			g_free(tmp);
			g_error_free(error);
		}
	}

	return ret;
}
#endif /* _WIN32 */

static void *
pidgin_notify_uri(const char *uri)
{
#ifndef _WIN32
	char *escaped = g_shell_quote(uri);
	char *command = NULL;
	char *remote_command = NULL;
	const char *web_browser;
	int place;

	web_browser = purple_prefs_get_string(PIDGIN_PREFS_ROOT "/browsers/browser");
	place = purple_prefs_get_int(PIDGIN_PREFS_ROOT "/browsers/place");

	/* if they are running gnome, use the gnome web browser */
	if (purple_running_gnome() == TRUE)
	{
		char *tmp = g_find_program_in_path("xdg-open");
		if (tmp == NULL)
			command = g_strdup_printf("gnome-open %s", escaped);
		else
			command = g_strdup_printf("xdg-open %s", escaped);
		g_free(tmp);
	}
	else if (purple_running_osx() == TRUE)
	{
		command = g_strdup_printf("open %s", escaped);
	}
	else if (!strcmp(web_browser, "epiphany") ||
		!strcmp(web_browser, "galeon"))
	{
		if (place == PIDGIN_BROWSER_NEW_WINDOW)
			command = g_strdup_printf("%s -w %s", web_browser, escaped);
		else if (place == PIDGIN_BROWSER_NEW_TAB)
			command = g_strdup_printf("%s -n %s", web_browser, escaped);
		else
			command = g_strdup_printf("%s %s", web_browser, escaped);
	}
	else if (!strcmp(web_browser, "xdg-open"))
	{
		command = g_strdup_printf("xdg-open %s", escaped);
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
			 !strcmp(web_browser, "firefox") ||
			 !strcmp(web_browser, "seamonkey"))
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

		if (place == PIDGIN_BROWSER_NEW_WINDOW)
			remote_command = g_strdup_printf("%s %s -remote "
											 "openURL(%s,new-window)",
											 web_browser, args, escaped);
		else if (place == PIDGIN_BROWSER_NEW_TAB)
			remote_command = g_strdup_printf("%s %s -remote "
											 "openURL(%s,new-tab)",
											 web_browser, args, escaped);
		else if (place == PIDGIN_BROWSER_CURRENT)
			remote_command = g_strdup_printf("%s %s -remote "
											 "openURL(%s)",
											 web_browser, args, escaped);
	}
	else if (!strcmp(web_browser, "netscape"))
	{
		command = g_strdup_printf("netscape %s", escaped);

		if (place == PIDGIN_BROWSER_NEW_WINDOW)
		{
			remote_command = g_strdup_printf("netscape -remote "
											 "openURL(%s,new-window)",
											 escaped);
		}
		else if (place == PIDGIN_BROWSER_CURRENT)
		{
			remote_command = g_strdup_printf("netscape -remote "
											 "openURL(%s)", escaped);
		}
	}
	else if (!strcmp(web_browser, "opera"))
	{
		if (place == PIDGIN_BROWSER_NEW_WINDOW)
			command = g_strdup_printf("opera -newwindow %s", escaped);
		else if (place == PIDGIN_BROWSER_NEW_TAB)
			command = g_strdup_printf("opera -newpage %s", escaped);
		else if (place == PIDGIN_BROWSER_CURRENT)
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

		web_command = purple_prefs_get_path(PIDGIN_PREFS_ROOT "/browsers/command");

		if (web_command == NULL || *web_command == '\0')
		{
			purple_notify_error(NULL, NULL, _("Unable to open URL"),
							  _("The 'Manual' browser command has been "
								"chosen, but no command has been set."));
			return NULL;
		}

		if (strstr(web_command, "%s"))
			command = purple_strreplace(web_command, "%s", escaped);
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
	winpidgin_notify_uri(uri);
#endif /* !_WIN32 */

	return NULL;
}

static PurpleNotifyUiOps ops =
{
	pidgin_notify_message,
	pidgin_notify_email,
	pidgin_notify_emails,
	pidgin_notify_formatted,
	pidgin_notify_searchresults,
	pidgin_notify_searchresults_new_rows,
	pidgin_notify_userinfo,
	pidgin_notify_uri,
	pidgin_close_notify,
	NULL,
	NULL,
	NULL,
	NULL
};

PurpleNotifyUiOps *
pidgin_notify_get_ui_ops(void)
{
	return &ops;
}
