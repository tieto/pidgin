/**
 * @file gtkrequest.c GTK+ Request API
 * @ingroup gtkui
 *
 * gaim
 *
 * Copyright (C) 2003 Christian Hammond <chipx86@gnupdate.org>
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
#include "gtkrequest.h"
#include "stock.h"
#include <gtk/gtk.h>
#include <string.h>

/* XXX For _(..) */
#include "gaim.h"

typedef struct
{
	GaimRequestType type;

	void *user_data;
	GtkWidget *dialog;

	size_t cb_count;
	GCallback *cbs;

	union
	{
		struct
		{
			GtkWidget *entry;

			gboolean multiline;

		} input;

	} u;

} GaimRequestData;

static void
__input_response_cb(GtkDialog *dialog, gint id, GaimRequestData *data)
{
	const char *value;

	if (data->u.input.multiline) {
		GtkTextIter start_iter, end_iter;
		GtkTextBuffer *buffer =
			gtk_text_view_get_buffer(GTK_TEXT_VIEW(data->u.input.entry));

		gtk_text_buffer_get_start_iter(buffer, &start_iter);
		gtk_text_buffer_get_end_iter(buffer, &end_iter);

		value = gtk_text_buffer_get_text(buffer, &start_iter, &end_iter,
										 FALSE);
	}
	else
		value = gtk_entry_get_text(GTK_ENTRY(data->u.input.entry));

	if (id < data->cb_count && data->cbs[id] != NULL)
		((GaimRequestInputCb)data->cbs[id])(data->user_data, value);

	gaim_request_close(GAIM_REQUEST_INPUT, data);
}

static void
__action_response_cb(GtkDialog *dialog, gint id, GaimRequestData *data)
{
	if (id < data->cb_count && data->cbs[id] != NULL)
		((GaimRequestActionCb)data->cbs[id])(data->user_data, id);

	gaim_request_close(GAIM_REQUEST_INPUT, data);
}

#define STOCK_ITEMIZE(r, l) \
	if (!strcmp((r), text)) \
		return (l);

static const char *
__text_to_stock(const char *text)
{
	STOCK_ITEMIZE(_("Yes"),    GTK_STOCK_YES);
	STOCK_ITEMIZE(_("No"),     GTK_STOCK_NO);
	STOCK_ITEMIZE(_("OK"),     GTK_STOCK_OK);
	STOCK_ITEMIZE(_("Cancel"), GTK_STOCK_CANCEL);
	STOCK_ITEMIZE(_("Apply"),  GTK_STOCK_APPLY);
	STOCK_ITEMIZE(_("Close"),  GTK_STOCK_CLOSE);
	STOCK_ITEMIZE(_("Delete"), GTK_STOCK_DELETE);
	STOCK_ITEMIZE(_("Add"),    GTK_STOCK_ADD);
	STOCK_ITEMIZE(_("Remove"), GTK_STOCK_REMOVE);

	return text;
}

void *
gaim_gtk_request_input(const char *title, const char *primary,
					   const char *secondary, const char *default_value,
					   gboolean multiline,
					   const char *ok_text, GCallback ok_cb,
					   const char *cancel_text, GCallback cancel_cb,
					   void *user_data)
{
	GaimRequestData *data;
	GtkWidget *dialog;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *img;
	char *label_text;

	data            = g_new0(GaimRequestData, 1);
	data->type      = GAIM_REQUEST_INPUT;
	data->user_data = user_data;

	data->cb_count = 2;
	data->cbs = g_new0(GCallback, 2);

	data->cbs[0] = ok_cb;
	data->cbs[1] = cancel_cb;

	/* Create the dialog. */
	dialog = gtk_dialog_new_with_buttons("", NULL, 0,
					     __text_to_stock(cancel_text), 1,
					     __text_to_stock(ok_text),     0,
					     NULL);
	data->dialog = dialog;

	g_signal_connect(G_OBJECT(dialog), "response",
					 G_CALLBACK(__input_response_cb), data);

	/* Setup the dialog */
	gtk_container_set_border_width(GTK_CONTAINER(dialog), 6);
	gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), 6);
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), 0);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog)->vbox), 12);

	/* Setup the main horizontal box */
	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), hbox);

	/* Dialog icon. */
	img = gtk_image_new_from_stock(GAIM_STOCK_DIALOG_QUESTION,
								   GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);

	/* Vertical box */
	vbox = gtk_vbox_new(FALSE, 12);

	gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

	/* Descriptive label */
	label_text = g_strdup_printf("<span weight=\"bold\" size=\"larger\">"
								 "%s</span>\n\n%s",
								 primary, (secondary ? secondary : ""));

	label = gtk_label_new(NULL);

	gtk_label_set_markup(GTK_LABEL(label), label_text);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

	g_free(label_text);

	/* Entry field. */
	data->u.input.multiline = multiline;

	if (multiline) {
		GtkWidget *sw;

		sw = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
									   GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
		gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
											GTK_SHADOW_IN);

		gtk_widget_set_size_request(sw, 300, 75);

		gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);

		entry = gtk_text_view_new();
		gtk_text_view_set_editable(GTK_TEXT_VIEW(entry), TRUE);

		gtk_container_add(GTK_CONTAINER(sw), entry);

		if (default_value != NULL) {
			GtkTextBuffer *buffer;

			buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(entry));
			gtk_text_buffer_set_text(buffer, default_value, -1);
		}
	}
	else {
		entry = gtk_entry_new();

		gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);

		gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 0);

		if (default_value != NULL)
			gtk_entry_set_text(GTK_ENTRY(entry), default_value);
	}

	data->u.input.entry = entry;

	/* Show everything. */
	gtk_widget_show_all(dialog);

	return data;
}

void *
gaim_gtk_request_choice(const char *title, const char *primary,
						const char *secondary, unsigned int default_value,
						const char *ok_text, GCallback ok_cb,
						const char *cancel_text, GCallback cancel_cb,
						void *user_data, size_t choice_count, va_list args)
{
	return NULL;
}

void *
gaim_gtk_request_action(const char *title, const char *primary,
						const char *secondary, unsigned int default_action,
						void *user_data, size_t action_count, va_list actions)
{
	GaimRequestData *data;
	GtkWidget *dialog;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *img;
	void **buttons;
	char *label_text;
	int i;

	data            = g_new0(GaimRequestData, 1);
	data->type      = GAIM_REQUEST_ACTION;
	data->user_data = user_data;

	data->cb_count = action_count;
	data->cbs = g_new0(GCallback, action_count);

	/* Reverse the buttons */
	buttons = g_new0(void *, action_count * 2);

	for (i = 0; i < action_count * 2; i += 2) {
		buttons[(action_count * 2) - i - 2] = va_arg(actions, char *);
		buttons[(action_count * 2) - i - 1] = va_arg(actions, GCallback);
	}

	/* Create the dialog. */
	data->dialog = dialog = gtk_dialog_new();

	for (i = 0; i < action_count; i++) {
		gtk_dialog_add_button(GTK_DIALOG(dialog),
							  __text_to_stock(buttons[2 * i]), i);

		data->cbs[i] = buttons[2 * i + 1];
	}

	g_free(buttons);

	g_signal_connect(G_OBJECT(dialog), "response",
					 G_CALLBACK(__action_response_cb), data);

	/* Setup the dialog */
	gtk_container_set_border_width(GTK_CONTAINER(dialog), 6);
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog)->vbox), 12);
	gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), 6);

	/* Setup the main horizontal box */
	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), hbox);

	/* Dialog icon. */
	img = gtk_image_new_from_stock(GAIM_STOCK_DIALOG_QUESTION,
								   GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);

	/* Vertical box */
	vbox = gtk_vbox_new(FALSE, 12);

	gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

	/* Descriptive label */
	label_text = g_strdup_printf("<span weight=\"bold\" size=\"larger\">"
								 "%s</span>\n\n%s",
								 primary, (secondary ? secondary : ""));

	label = gtk_label_new(NULL);

	gtk_label_set_markup(GTK_LABEL(label), label_text);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

	g_free(label_text);

	if (default_action != -1)
		gtk_dialog_set_default_response(GTK_DIALOG(dialog), default_action);

	/* Show everything. */
	gtk_widget_show_all(dialog);

	return data;
}

void
gaim_gtk_close_request(GaimRequestType type, void *ui_handle)
{
	GaimRequestData *data = (GaimRequestData *)ui_handle;

	if (data->cbs != NULL)
		g_free(data->cbs);

	gtk_widget_destroy(data->dialog);

	g_free(data);
}

static GaimRequestUiOps ops =
{
	gaim_gtk_request_input,
	gaim_gtk_request_choice,
	gaim_gtk_request_action,
	gaim_gtk_close_request
};

GaimRequestUiOps *
gaim_get_gtk_request_ui_ops(void)
{
	return &ops;
}
